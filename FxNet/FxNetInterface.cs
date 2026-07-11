using FxNet.Core;
using FxNet.IO;
using FxNet.Util;
using System.Net;

namespace FxNet
{
    /// <summary>
    /// FxNet 网络库主入口接口。
    /// 提供 IO 模块启动、TCP/UDP 监听与连接、事件投递、日志管理等全局 API。
    /// </summary>
    public static class FxNetInterface
    {
        private static readonly MessageEventQueue _eventQueue = new MessageEventQueue(); // 全局消息事件队列
        private static uint _ioModuleIndex; // IO 模块索引分配器
        private static double _timeOffset;  // 时间偏移量（可通过 SetTimeOffset 调整）

        /// <summary>设置时间偏移量</summary>
        public static void SetTimeOffset(double offset) => _timeOffset = offset;

        private static Func<double>? _timeFunc; // 自定义时间函数

        /// <summary>获取当前时间（秒），支持自定义时间源</summary>
        public static double GetNow()
        {
            if (_timeFunc != null) return _timeFunc();
            return TimeUtility.GetTimeSeconds() + _timeOffset;
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

        /// <summary>初始化日志模块（创建单例，不启动后台线程）</summary>
        public static void InitLogModule()
        {
            LogModule.CreateInstance();
        }

        /// <summary>启动日志模块（创建单例并启动后台日志输出线程）</summary>
        public static void StartLogModule()
        {
            LogModule.CreateInstance();
            LogModule.Instance?.Init();
        }

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
            var events = new List<MessageEventBase>();
            _eventQueue.SwapEvents(events);
            foreach (var evt in events)
            {
                evt.Execute(output);
            }
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

        /// <summary>将日志内容推送到日志模块缓冲区</summary>
        public static void PushLog(System.Text.StringBuilder stream)
        {
            LogModule.Instance?.PushLog(stream);
        }

        /// <summary>获取日志模块中累积的日志字符串</summary>
        public static string GetLogStr()
        {
            return LogModule.Instance?.GetLogStr() ?? "";
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
