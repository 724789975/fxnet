using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using FxNet.Core;
using FxNet.IO;

namespace FxNet
{
    /// <summary>
    /// FxNet 网络库主入口接口。
    /// 提供 IO 模块启动、TCP/UDP 监听与连接、事件投递、日志管理等全局 API。
    /// </summary>
    public static class FxNetInterface
    {
        private static readonly MessageEventQueue _eventQueue = new MessageEventQueue(); // 全局消息事件队列
        private static readonly List<MessageEventBase> _eventBuffer = new List<MessageEventBase>(256); // 复用的事件处理缓冲（仅主线程访问）
        private static uint _ioModuleIndex; // IO 模块索引分配器
        private static double _timeOffset;  // 时间偏移量（可通过 SetTimeOffset 调整）

        // 单调递增的高精度时钟（线程安全，可从任意线程读取）。
        // 取代 UnityEngine.Time.timeAsDouble：后者仅允许主线程访问，而多线程模式下 IO
        // 后台线程也需读取时间；且 Stopwatch 不受 timeScale/暂停影响，更适合网络计时。
        private static readonly Stopwatch _clock = Stopwatch.StartNew();

        /// <summary>设置时间偏移量</summary>
        public static void SetTimeOffset(double offset) => _timeOffset = offset;

        private static Func<double>? _timeFunc; // 自定义时间函数

        /// <summary>获取当前时间（秒），线程安全，支持自定义时间源</summary>
        public static double GetNow()
        {
            if (_timeFunc != null) return _timeFunc();
            return _clock.Elapsed.TotalSeconds + _timeOffset;
        }

        /// <summary>设置自定义时间获取函数</summary>
        public static void SetTimeFunc(Func<double> func)
        {
            _timeFunc = func;
        }

        /// <summary>启动所有 IO 模块（创建后台 IO 线程）</summary>
        public static void StartIOModule(TextWriter? output = null)
        {
            for (uint i = 0; i < IoModule.GetModuleCount(); i++)
            {
                var module = new IoModule();
                module.Init(i, _eventQueue, output);
            }
        }

        /// <summary>初始化日志模块（Unity 环境使用 Debug.Log，无需额外初始化）</summary>
        public static void InitLogModule() { }

        /// <summary>启动日志模块（Unity 环境使用 Debug.Log，无需额外启动）</summary>
        public static void StartLogModule() { }

        /// <summary>获取下一个 IO 模块索引</summary>
        public static uint GetFxIoModuleIndex()
        {
            return _ioModuleIndex++;
        }

        /// <summary>处理一次 IO 事件（主线程调用，遍历所有 IO 模块执行 DealFunction）</summary>
        public static void ProcSingleThread(TextWriter? output = null)
        {
            for (uint i = 0; i < IoModule.GetModuleCount(); i++)
            {
                IoModule.GetInstance(i)?.DealFunction(output);
            }
        }

        /// <summary>处理全局消息队列中的事件（主线程调用，对齐 C++ main.cpp 中 oQueue.SwapEvent）</summary>
        public static void ProcessMessageEvents(TextWriter? output = null)
        {
            _eventBuffer.Clear();
            _eventQueue.SwapEvents(_eventBuffer);
            foreach (var evt in _eventBuffer)
            {
                evt.Execute(output);
            }
            _eventBuffer.Clear(); // 释放对事件对象的引用，避免延长其生命周期
        }

        /// <summary>向指定 IO 模块投递事件</summary>
        public static void PostEvent(uint ioModuleIndex, IOEventBase evt)
        {
            IoModule.GetInstance(ioModuleIndex)?.PostEvent(evt);
        }

        /// <summary>启动 UDP 监听</summary>
        public static void UdpListen(uint ioModuleIndex, string ip, ushort port, ISessionMaker sessionMaker, TextWriter? output = null)
        {
            var listener = new UdpListener(sessionMaker);
            listener.SetIOModuleIndex(ioModuleIndex);
            var error = new ErrorCode();
            listener.Listen(ip, port, error, output);
        }

        /// <summary>发起 UDP 连接（客户端模式）</summary>
        public static void UdpConnect(uint ioModuleIndex, string ip, ushort port, ISession session, TextWriter? output = null)
        {
            if (session.GetSocket() != null)
            {
                var error = new ErrorCode((int)UserError.CodeErrorNetSessionAlreadyConnected, "FxNetInterface:UdpConnect");
                session.GetSocket()!.OnError(error, output);
                return;
            }

            var connector = new UdpConnector(session);
            connector.SetIOModuleIndex(ioModuleIndex);
            session.SetSocket(connector);

            var remoteEp = new IPEndPoint(
                string.IsNullOrEmpty(ip) ? IPAddress.Loopback : IPAddress.Parse(ip),
                port);

            var connectError = new ErrorCode();
            connector.Connect(remoteEp, connectError, output);
            if (connectError)
            {
                connector.OnError(connectError, output);
            }
        }

        /// <summary>启动 TCP 监听</summary>
        public static void TcpListen(uint ioModuleIndex, string ip, ushort port, ISessionMaker sessionMaker, TextWriter? output = null)
        {
            var listener = new TcpListener(sessionMaker);
            listener.SetIOModuleIndex(ioModuleIndex);
            var error = new ErrorCode();
            listener.Listen(ip, port, error, output);
        }

        /// <summary>发起 TCP 连接</summary>
        public static void TcpConnect(uint ioModuleIndex, string ip, ushort port, ISession session, TextWriter? output = null)
        {
            if (session.GetSocket() != null)
            {
                var error = new ErrorCode((int)UserError.CodeErrorNetSessionAlreadyConnected, "FxNetInterface:TcpConnect");
                session.GetSocket()!.OnError(error, output);
                return;
            }

            var connector = new TcpConnector(session);
            connector.SetIOModuleIndex(ioModuleIndex);
            session.SetSocket(connector);

            var remoteEp = new IPEndPoint(
                string.IsNullOrEmpty(ip) ? IPAddress.Loopback : IPAddress.Parse(ip),
                port);

            var connectError = new ErrorCode();
            connector.Connect(remoteEp, connectError, output);
            if (connectError)
            {
                connector.OnError(connectError, output);
            }
        }

        /// <summary>关闭所有 Socket 连接</summary>
        public static void CloseAllSockets(TextWriter? output = null)
        {
            for (uint i = 0; i < IoModule.GetModuleCount(); i++)
            {
                IoModule.GetInstance(i)?.CloseAllSockets(output);
            }
        }

        /// <summary>将日志内容推送到日志模块缓冲区（Unity 环境已直接使用 Debug.Log，此方法为空）</summary>
        public static void PushLog(System.Text.StringBuilder stream) { }

        /// <summary>获取日志模块中累积的日志字符串（Unity 环境已直接使用 Debug.Log，此方法返回空）</summary>
        public static string GetLogStr()
        {
            return "";
        }

        #region IO 事件类（对齐 C++ fxnet_interface.h 中的 UDPConnect/TCPConnect/UDPListen/TCPListen）

        /// <summary>UDP Connect 事件，投递到 IO 线程执行连接操作（对齐 C++ UDPConnect）</summary>
        public class UDPConnect : IOEventBase
        {
            private readonly string _ip;
            private readonly ushort _port;
            private readonly uint _ioModuleIndex;
            private readonly ISession _session;

            public UDPConnect(string ip, ushort port, uint ioModuleIndex, ISession session)
            {
                _ip = ip;
                _port = port;
                _ioModuleIndex = ioModuleIndex;
                _session = session;
            }

            public override void Execute(TextWriter? output)
            {
                FxNetInterface.UdpConnect(_ioModuleIndex, _ip, _port, _session, output);
            }
        }

        /// <summary>TCP Connect 事件，投递到 IO 线程执行连接操作（对齐 C++ TCPConnect）</summary>
        public class TCPConnect : IOEventBase
        {
            private readonly string _ip;
            private readonly ushort _port;
            private readonly uint _ioModuleIndex;
            private readonly ISession _session;

            public TCPConnect(string ip, ushort port, uint ioModuleIndex, ISession session)
            {
                _ip = ip;
                _port = port;
                _ioModuleIndex = ioModuleIndex;
                _session = session;
            }

            public override void Execute(TextWriter? output)
            {
                FxNetInterface.TcpConnect(_ioModuleIndex, _ip, _port, _session, output);
            }
        }

        /// <summary>UDP Listen 事件，投递到 IO 线程执行监听操作（对齐 C++ UDPListen）</summary>
        public class UDPListen : IOEventBase
        {
            private readonly string _ip;
            private readonly ushort _port;
            private readonly uint _ioModuleIndex;
            private readonly ISessionMaker _sessionMaker;

            public UDPListen(string ip, ushort port, uint ioModuleIndex, ISessionMaker sessionMaker)
            {
                _ip = ip;
                _port = port;
                _ioModuleIndex = ioModuleIndex;
                _sessionMaker = sessionMaker;
            }

            public override void Execute(TextWriter? output)
            {
                FxNetInterface.UdpListen(_ioModuleIndex, _ip, _port, _sessionMaker, output);
            }
        }

        /// <summary>TCP Listen 事件，投递到 IO 线程执行监听操作（对齐 C++ TCPListen）</summary>
        public class TCPListen : IOEventBase
        {
            private readonly string _ip;
            private readonly ushort _port;
            private readonly uint _ioModuleIndex;
            private readonly ISessionMaker _sessionMaker;

            public TCPListen(string ip, ushort port, uint ioModuleIndex, ISessionMaker sessionMaker)
            {
                _ip = ip;
                _port = port;
                _ioModuleIndex = ioModuleIndex;
                _sessionMaker = sessionMaker;
            }

            public override void Execute(TextWriter? output)
            {
                FxNetInterface.TcpListen(_ioModuleIndex, _ip, _port, _sessionMaker, output);
            }
        }

        #endregion
    }
}
