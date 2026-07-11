using System.Text;
using System.Buffers.Binary;
using FxNet;
using FxNet.Core;
using FxNet.IO;
using FxNet.Util;

namespace FxNet.UdpServer;

/// <summary>
/// UDP 回显服务器：
/// - UDP 监听指定端口，收到客户端数据后原样回显
/// - 支持多种回显模式（原文/大写/反转）
/// - 客户端地址管理（记录活跃客户端集合）
/// - 性能统计（消息速率、吞吐量、累计流量）
/// - 命令协议支持（CMD:SWITCH_MODE, CMD:STATS, CMD:CLIENT_COUNT）
/// - 文件日志输出（UTF-8 编码写入 udp_server.txt）
/// - 空闲客户端超时清理
/// </summary>
class Program
{
    private const string ListenIp = "0.0.0.0";
    private static ushort ListenPort = 9001;
    private const int StatsIntervalSec = 5;
    private const int IdleTimeoutSec = 30;
    private const string LogFilePath = "udp_server.txt";

    static volatile bool _running = true;
    static StreamWriter? _logFile;

    // === 回显模式 ===
    enum EchoMode { Original, UpperCase, Reverse }
    static int _echoModeIndex;
    static readonly string[] EchoModeNames = { "原文回显", "大写回显", "反转回显" };

    // === 客户端管理 ===
    static readonly Dictionary<string, ClientInfo> _clients = new();
    static readonly object _clientLock = new();

    sealed class ClientInfo
    {
        public string Address = "";
        public DateTime LastActiveTime = DateTime.Now;
        public long RecvCount;
        public long SentCount;
    }

    // === 性能统计 ===
    static long _totalMsgRecv;
    static long _totalBytesRecv;
    static long _totalBytesSent;
    static long _lastMsgCount;
    static long _lastByteCount;
    static double _lastStatsTime;

    static void Main(string[] args)
    {
        // 支持通过命令行参数配置监听端口
        if (args.Length > 0 && ushort.TryParse(args[0], out var port) && port > 0)
            ListenPort = port;

        Console.OutputEncoding = Encoding.UTF8;
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║       FxNet UDP 回显服务器           ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 监听地址: {ListenIp}:{ListenPort}");
        LogRaw($"[配置] 回显模式: {EchoModeNames[0]}");
        LogRaw($"[配置] 空闲超时: {IdleTimeoutSec} 秒");
        LogRaw("按 Ctrl+C 退出");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

        FxNetInterface.UdpListen(0, ListenIp, ListenPort, new UdpSessionMaker(), Console.Out);
        Log($"[启动] UDP 服务器已启动 (端口 {ListenPort})");

        Console.CancelKeyPress += (_, e) => { e.Cancel = true; _running = false; };
        _lastStatsTime = GetNow();

        while (_running)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();

            double now = GetNow();
            if (now - _lastStatsTime >= StatsIntervalSec)
            {
                PrintStats(now);
                CleanupIdleClients();
                _lastStatsTime = now;
            }

            Thread.Sleep(1);
        }

        PrintFinalStats();
        Log("[关闭] UDP 服务器已关闭");
        _logFile?.Close();
    }

    // ======================== Session 实现 ========================

    sealed class UdpSessionMaker : ISessionMaker
    {
        public ISession Create() => new UdpSession();
    }

    /// <summary>UDP 会话：收到数据后根据回显模式处理并回显</summary>
    sealed class UdpSession : ISession
    {
        private ConnectorSocket? _socket;
        private readonly TextWorkStream _sendBuff = new();
        private readonly TextWorkStream _recvBuff = new();

        public void SetSocket(ConnectorSocket? socket) => _socket = socket;
        public ConnectorSocket? GetSocket() => _socket;
        public NetWorkStream GetSendBuff() => _sendBuff;
        public NetWorkStream GetRecvBuff() => _recvBuff;

        public ISession Send(byte[] data, int len, TextWriter? output)
        {
            // 写入 4 字节大端长度头 + 数据体（与 TextSession 协议一致）
            var header = new byte[4];
            BinaryPrimitives.WriteInt32BigEndian(header, len);
            _sendBuff.PushData(header, 4);
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
            Interlocked.Increment(ref _totalMsgRecv);
            Interlocked.Add(ref _totalBytesRecv, len);

            // 更新客户端信息
            // 使用远端地址作为客户端唯一标识
            string clientKey = _socket is UdpConnector uc ? uc.GetRemoteEndPoint().ToString() : "unknown";
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

            // 命令处理
            if (message.StartsWith("CMD:", StringComparison.OrdinalIgnoreCase))
            {
                HandleCommand(message, output);
                return this;
            }

            // 回显
            EchoMode mode = (EchoMode)Volatile.Read(ref _echoModeIndex);
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
            Interlocked.Add(ref _totalBytesSent, echoData.Length);

            lock (_clientLock)
            {
                if (_clients.TryGetValue(clientKey, out var info))
                    info.SentCount++;
            }

            Log($"[验证] 回显 {len} 字节 → {echoData.Length} 字节 | 模式={EchoModeNames[(int)mode]} | \"{Truncate(message, 40)}\"");
            return this;
        }

        private void HandleCommand(string cmd, TextWriter? output)
        {
            string response;
            if (cmd.Equals("CMD:SWITCH_MODE", StringComparison.OrdinalIgnoreCase))
            {
                int newMode = (Volatile.Read(ref _echoModeIndex) + 1) % EchoModeNames.Length;
                Volatile.Write(ref _echoModeIndex, newMode);
                response = $"[UDP服务器] 回显模式: {EchoModeNames[newMode]}";
                Log($"[验证] 命令处理: {cmd} → \"{Truncate(response, 50)}\"");
            }
            else if (cmd.Equals("CMD:STATS", StringComparison.OrdinalIgnoreCase))
            {
                response = $"[UDP服务器] 消息: {Volatile.Read(ref _totalMsgRecv)} | " +
                           $"接收: {FormatBytes(Volatile.Read(ref _totalBytesRecv))} | " +
                           $"发送: {FormatBytes(Volatile.Read(ref _totalBytesSent))}";
            }
            else if (cmd.Equals("CMD:CLIENT_COUNT", StringComparison.OrdinalIgnoreCase))
            {
                int count;
                lock (_clientLock) count = _clients.Count;
                response = $"[UDP服务器] 活跃客户端: {count}";
            }
            else
            {
                response = $"[UDP服务器] 未知命令: {cmd}";
            }

            byte[] respData = Encoding.UTF8.GetBytes(response);
            Send(respData, respData.Length, output);
            Interlocked.Add(ref _totalBytesSent, respData.Length);
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
        public MessageEventBase NewOnSendEvent(int len) => new OnSendEvt(this, len);

        // 内部事件类
        private class UdpRecvEvent : MessageRecvEventBase
        {
            private readonly ISession _session;
            public UdpRecvEvent(ISession session) { _session = session; Session = session; }
            public override void Execute(TextWriter? output) => _session.OnRecv(Package, output);
        }
        private class ConnectedEvt : MessageEventBase
        {
            private readonly ISession _session;
            public ConnectedEvt(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnConnected(output);
        }
        private class ErrorEvt : MessageEventBase
        {
            private readonly ISession _session;
            private readonly ErrorCode _error;
            public ErrorEvt(ISession session, ErrorCode error) { _session = session; _error = error; }
            public override void Execute(TextWriter? output) => _session.OnError(_error, output);
        }
        private class CloseEvt : MessageEventBase
        {
            private readonly ISession _session;
            public CloseEvt(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnClose(output);
        }
        private class OnSendEvt : MessageEventBase
        {
            public OnSendEvt(ISession session, int len) { }
            public override void Execute(TextWriter? output) { }
        }

        /// <summary>关闭操作事件，在 IO 线程中执行底层 Socket 关闭（对齐 C++ CloseOperator）</summary>
        private class CloseOperator : IOEventBase
        {
            private readonly ConnectorSocket _socket;
            public CloseOperator(ConnectorSocket socket) => _socket = socket;
            public override void Execute(TextWriter? output) => _socket.Close(output);
        }
    }

    // ======================== 工具方法 ========================

    static void CleanupIdleClients()
    {
        lock (_clientLock)
        {
            var now = DateTime.Now;
            var idleKeys = _clients.Where(kvp => (now - kvp.Value.LastActiveTime).TotalSeconds > IdleTimeoutSec)
                                   .Select(kvp => kvp.Key).ToList();
            foreach (var key in idleKeys)
            {
                _clients.Remove(key);
                Log($"[清理] 移除空闲客户端: {key}");
            }
        }
    }

    static void PrintStats(double now)
    {
        double elapsed = now - _lastStatsTime;
        long currentMsg = Volatile.Read(ref _totalMsgRecv);
        long currentBytes = Volatile.Read(ref _totalBytesRecv);
        double msgRate = (currentMsg - _lastMsgCount) / elapsed;
        double byteRate = (currentBytes - _lastByteCount) / elapsed;

        int clientCount;
        lock (_clientLock) clientCount = _clients.Count;

        string stats = $"[UDP统计] 客户端: {clientCount} | " +
                       $"消息速率: {msgRate:F1}/s | " +
                       $"吞吐: {FormatBytes((long)byteRate)}/s | " +
                       $"累计: {FormatBytes(currentBytes)}";
        LogRaw(stats);

        _lastMsgCount = currentMsg;
        _lastByteCount = currentBytes;
    }

    static void PrintFinalStats()
    {
        LogRaw("");
        LogRaw("═══ UDP 服务器最终统计 ═══");
        LogRaw($"  累计消息: {Volatile.Read(ref _totalMsgRecv)}");
        LogRaw($"  累计接收: {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
        LogRaw($"  累计发送: {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
    }

    static void Log(string message)
    {
        string line = $"{DateTime.Now:HH:mm:ss.fff} {message}";
        Console.WriteLine(line);
        _logFile?.WriteLine(line);
    }

    static void LogRaw(string message)
    {
        Console.WriteLine(message);
        _logFile?.WriteLine(message);
    }

    static double GetNow() => TimeUtility.GetTimeSeconds();

    static string FormatBytes(long bytes)
    {
        if (bytes < 1024) return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024.0):F1} MB";
    }

    static string Truncate(string s, int maxLen) => s.Length <= maxLen ? s : s[..maxLen] + "...";
}
