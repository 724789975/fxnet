#nullable enable
using System;
using System.Buffers.Binary;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading;
using UnityEngine;
using FxNet.Core;
using FxNet.IO;

namespace FxNet.UdpTest
{
    /// <summary>
    /// UDP 回显服务器（Unity headless / Linux dedicated server）。
    /// 对齐 FxNet.UdpServer 控制台项目：
    /// - UDP 监听端口，收到数据后按回显模式（原文/大写/反转）回显
    /// - 命令协议 CMD:SWITCH_MODE / CMD:STATS / CMD:CLIENT_COUNT
    /// - 客户端地址管理与空闲超时清理
    /// - 周期性性能统计
    /// 网络事件驱动：多线程模式下 IO 在后台线程，主线程 Update 抽取消息事件队列。
    /// </summary>
    public sealed class UdpEchoServer : MonoBehaviour
    {
        private const int StatsIntervalSec = 5;
        private const int IdleTimeoutSec = 30;
        private const string LogFilePath = "udp_server.txt";

        private UdpTestConfig _cfg = new UdpTestConfig();
        private TestLog? _log;

        // === 回显模式 ===
        private int _echoModeIndex;
        internal static readonly string[] EchoModeNames = { "原文回显", "大写回显", "反转回显" };
        internal enum EchoMode { Original, UpperCase, Reverse }

        // === 客户端管理 ===
        private readonly Dictionary<string, ClientInfo> _clients = new();
        private readonly object _clientLock = new object();

        internal sealed class ClientInfo
        {
            public string Address = "";
            public DateTime LastActiveTime = DateTime.Now;
            public long RecvCount;
            public long SentCount;
        }

        // === 性能统计 ===
        private long _totalMsgRecv;
        private long _totalBytesRecv;
        private long _totalBytesSent;
        private long _lastMsgCount;
        private double _lastStatsTime;

        public void Configure(UdpTestConfig cfg) => _cfg = cfg;

        private void Start()
        {
            // dedicated server 无渲染，尽量提高 tick 频率保证网络响应
            Application.targetFrameRate = 120;
            QualitySettings.vSyncCount = 0;

            _log = new TestLog(LogFilePath);
            _log.Raw("╔══════════════════════════════════════╗");
            _log.Raw("║     FxNet UDP 回显服务器 (Unity)     ║");
            _log.Raw("╚══════════════════════════════════════╝");
            _log.Raw($"[配置] 监听地址: {_cfg.ListenIp}:{_cfg.Port}");
            _log.Raw($"[配置] 回显模式: {EchoModeNames[0]}");
            _log.Raw($"[配置] 空闲超时: {IdleTimeoutSec} 秒");

            FxNetInterface.StartLogModule();
            FxNetInterface.StartIOModule();
            FxNetInterface.UdpListen(0, _cfg.ListenIp, _cfg.Port, new UdpSessionMaker(this), Console.Out);
            _log.Log($"[启动] UDP 服务器已启动 (端口 {_cfg.Port})");

            _lastStatsTime = FxNetInterface.GetNow();
        }

        private void Update()
        {
            // 抽取 IO 线程投递的消息事件（回调在主线程执行）
            FxNetInterface.ProcessMessageEvents();

            double now = FxNetInterface.GetNow();
            if (now - _lastStatsTime >= StatsIntervalSec)
            {
                PrintStats(now);
                CleanupIdleClients();
                _lastStatsTime = now;
            }
        }

        private void OnDestroy()
        {
            FxNetInterface.CloseAllSockets(Console.Out);
            PrintFinalStats();
            _log?.Log("[关闭] UDP 服务器已关闭");
            _log?.Dispose();
        }

        // ======================== 供 Session 回调的服务器状态接口 ========================

        internal EchoMode CurrentMode => (EchoMode)Volatile.Read(ref _echoModeIndex);

        internal string SwitchMode()
        {
            int newMode = (Volatile.Read(ref _echoModeIndex) + 1) % EchoModeNames.Length;
            Volatile.Write(ref _echoModeIndex, newMode);
            return EchoModeNames[newMode];
        }

        internal string StatsString()
        {
            return $"[UDP服务器] 消息: {Volatile.Read(ref _totalMsgRecv)} | " +
                   $"接收: {FormatBytes(Volatile.Read(ref _totalBytesRecv))} | " +
                   $"发送: {FormatBytes(Volatile.Read(ref _totalBytesSent))}";
        }

        internal int ClientCount()
        {
            lock (_clientLock) return _clients.Count;
        }

        internal void OnRecvStat(string clientKey, int len)
        {
            Interlocked.Increment(ref _totalMsgRecv);
            Interlocked.Add(ref _totalBytesRecv, len);
            lock (_clientLock)
            {
                if (!_clients.TryGetValue(clientKey, out var info))
                {
                    info = new ClientInfo { Address = clientKey };
                    _clients[clientKey] = info;
                }
                info.LastActiveTime = DateTime.Now;
                info.RecvCount++;
            }
        }

        internal void OnSentStat(string clientKey, int len)
        {
            Interlocked.Add(ref _totalBytesSent, len);
            lock (_clientLock)
            {
                if (_clients.TryGetValue(clientKey, out var info))
                    info.SentCount++;
            }
        }

        internal void LogLine(string message) => _log?.Log(message);

        // ======================== 工具方法 ========================

        private void CleanupIdleClients()
        {
            lock (_clientLock)
            {
                var now = DateTime.Now;
                var idleKeys = new List<string>();
                foreach (var kvp in _clients)
                {
                    if ((now - kvp.Value.LastActiveTime).TotalSeconds > IdleTimeoutSec)
                        idleKeys.Add(kvp.Key);
                }
                foreach (var key in idleKeys)
                {
                    _clients.Remove(key);
                    _log?.Log($"[清理] 移除空闲客户端: {key}");
                }
            }
        }

        private void PrintStats(double now)
        {
            double elapsed = now - _lastStatsTime;
            if (elapsed <= 0) elapsed = 1;
            long currentMsg = Volatile.Read(ref _totalMsgRecv);
            long currentBytes = Volatile.Read(ref _totalBytesRecv);
            double msgRate = (currentMsg - _lastMsgCount) / elapsed;

            int clientCount;
            lock (_clientLock) clientCount = _clients.Count;

            _log?.Raw($"[UDP统计] 客户端: {clientCount} | 消息速率: {msgRate:F1}/s | 累计: {FormatBytes(currentBytes)}");
            _lastMsgCount = currentMsg;
        }

        private void PrintFinalStats()
        {
            _log?.Raw("");
            _log?.Raw("═══ UDP 服务器最终统计 ═══");
            _log?.Raw($"  累计消息: {Volatile.Read(ref _totalMsgRecv)}");
            _log?.Raw($"  累计接收: {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
            _log?.Raw($"  累计发送: {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
        }

        internal static string FormatBytes(long bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            return $"{bytes / (1024.0 * 1024.0):F1} MB";
        }

        internal static string Truncate(string s, int maxLen) => s.Length <= maxLen ? s : s[..maxLen] + "...";

        // ======================== Session 实现 ========================

        private sealed class UdpSessionMaker : ISessionMaker
        {
            private readonly UdpEchoServer _server;
            public UdpSessionMaker(UdpEchoServer server) => _server = server;
            public ISession Create() => new UdpSession(_server);
        }

        /// <summary>UDP 会话：收到数据后根据回显模式处理并回显（对齐控制台 UdpSession）</summary>
        private sealed class UdpSession : ISession
        {
            private readonly UdpEchoServer _server;
            private ConnectorSocket? _socket;
            private readonly TextWorkStream _sendBuff = new TextWorkStream();
            private readonly TextWorkStream _recvBuff = new TextWorkStream();

            public UdpSession(UdpEchoServer server) => _server = server;

            public void SetSocket(ConnectorSocket? socket) => _socket = socket;
            public ConnectorSocket? GetSocket() => _socket;
            public NetWorkStream GetSendBuff() => _sendBuff;
            public NetWorkStream GetRecvBuff() => _recvBuff;

            public ISession Send(byte[] data, int len, TextWriter? output)
            {
                // 写入 4 字节大端长度头 + 数据体（与 TextSession 协议一致）
                Span<byte> header = stackalloc byte[4];
                BinaryPrimitives.WriteInt32BigEndian(header, len);
                _sendBuff.PushData(header);
                _sendBuff.PushData(data, len);
                _socket?.SendMessage(new ErrorCode(), output);
                return this;
            }

            public ISession OnRecv(NetStreamPackage package, TextWriter? output)
            {
                byte[] data = package.GetData();
                int offset = package.GetOffset();
                int len = package.DataLength;

                string message = Encoding.UTF8.GetString(data, offset, len);
                string clientKey = _socket is UdpConnector uc ? uc.GetRemoteEndPoint().ToString() : "unknown";
                _server.OnRecvStat(clientKey, len);

                // 命令处理
                if (message.StartsWith("CMD:", StringComparison.OrdinalIgnoreCase))
                {
                    HandleCommand(message, clientKey, output);
                    return this;
                }

                // 回显
                EchoMode mode = _server.CurrentMode;
                byte[] echoData;
                switch (mode)
                {
                    case EchoMode.UpperCase:
                        echoData = Encoding.UTF8.GetBytes(message.ToUpperInvariant());
                        break;
                    case EchoMode.Reverse:
                        var chars = message.ToCharArray();
                        Array.Reverse(chars);
                        echoData = Encoding.UTF8.GetBytes(new string(chars));
                        break;
                    default:
                        echoData = data.AsSpan(offset, len).ToArray();
                        break;
                }

                Send(echoData, echoData.Length, output);
                _server.OnSentStat(clientKey, echoData.Length);
                _server.LogLine($"[验证] 回显 {len} 字节 → {echoData.Length} 字节 | 模式={EchoModeNames[(int)mode]} | \"{Truncate(message, 40)}\"");
                return this;
            }

            private void HandleCommand(string cmd, string clientKey, TextWriter? output)
            {
                string response;
                if (cmd.Equals("CMD:SWITCH_MODE", StringComparison.OrdinalIgnoreCase))
                {
                    string newModeName = _server.SwitchMode();
                    response = $"[UDP服务器] 回显模式: {newModeName}";
                    _server.LogLine($"[验证] 命令处理: {cmd} → \"{Truncate(response, 50)}\"");
                }
                else if (cmd.Equals("CMD:STATS", StringComparison.OrdinalIgnoreCase))
                {
                    response = _server.StatsString();
                }
                else if (cmd.Equals("CMD:CLIENT_COUNT", StringComparison.OrdinalIgnoreCase))
                {
                    response = $"[UDP服务器] 活跃客户端: {_server.ClientCount()}";
                }
                else
                {
                    response = $"[UDP服务器] 未知命令: {cmd}";
                }

                byte[] respData = Encoding.UTF8.GetBytes(response);
                Send(respData, respData.Length, output);
                _server.OnSentStat(clientKey, respData.Length);
            }

            public void OnConnected(TextWriter? output) { }
            public void OnError(ErrorCode error, TextWriter? output) { }
            public void OnClose(TextWriter? output) { }

            public void Close(TextWriter? output)
            {
                if (_socket == null) return;
                var op = new CloseOperator(_socket);
                FxNetInterface.PostEvent(_socket.GetIOModuleIndex(), op);
            }

            // === 事件创建 ===
            public MessageRecvEventBase NewRecvMessageEvent() => new UdpRecvEvent(this);
            public MessageEventBase NewConnectedEvent() => new ConnectedEvt(this);
            public MessageEventBase NewErrorEvent(ErrorCode error) => new ErrorEvt(this, error);
            public MessageEventBase NewCloseEvent() => new CloseEvt(this);
            public MessageEventBase NewOnSendEvent(int len) => new OnSendEvt();

            private sealed class UdpRecvEvent : MessageRecvEventBase
            {
                private readonly ISession _session;
                public UdpRecvEvent(ISession session) { _session = session; Session = session; }
                public override void Execute(TextWriter? output) => _session.OnRecv(Package, output);
            }
            private sealed class ConnectedEvt : MessageEventBase
            {
                private readonly ISession _session;
                public ConnectedEvt(ISession session) => _session = session;
                public override void Execute(TextWriter? output) => _session.OnConnected(output);
            }
            private sealed class ErrorEvt : MessageEventBase
            {
                private readonly ISession _session;
                private readonly ErrorCode _error;
                public ErrorEvt(ISession session, ErrorCode error) { _session = session; _error = error; }
                public override void Execute(TextWriter? output) => _session.OnError(_error, output);
            }
            private sealed class CloseEvt : MessageEventBase
            {
                private readonly ISession _session;
                public CloseEvt(ISession session) => _session = session;
                public override void Execute(TextWriter? output) => _session.OnClose(output);
            }
            private sealed class OnSendEvt : MessageEventBase
            {
                public override void Execute(TextWriter? output) { }
            }

            /// <summary>关闭操作事件，在 IO 线程中执行底层 Socket 关闭</summary>
            private sealed class CloseOperator : IOEventBase
            {
                private readonly ConnectorSocket _socket;
                public CloseOperator(ConnectorSocket socket) => _socket = socket;
                public override void Execute(TextWriter? output) => _socket.Close(output);
            }
        }
    }
}
