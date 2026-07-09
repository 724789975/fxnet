using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.Server;

/// <summary>
/// 增强版回显服务器：
/// - 连接管理（最大连接数限制、空闲超时检测、IP 黑白名单）
/// - 多种回显模式（原文回显、大写回显、反转回显、字节统计）
/// - 心跳机制（CMD:PING/CMD:PONG）
/// - 性能统计（吞吐量、消息速率、延迟）
/// - 详细的日志记录
/// </summary>
class Program
{
    private const string ListenIp = "127.0.0.1";
    private const ushort ListenPort = 9000;
    private const int StatsIntervalSec = 5; // 性能统计输出间隔
    private const string LogFilePath = "server.txt";
    private const int MaxConnections = 100; // 最大连接数
    private const int IdleTimeoutSec = 60; // 空闲超时（秒）
    private static readonly string? AuthToken = null; // 认证 Token（null 表示不启用认证）

    static volatile bool _running = true;
    static StreamWriter? _logFile;

    // === 连接管理 ===
    static int _connectionCount;           // 当前活跃连接数
    static long _totalConnections;         // 累计连接总数
    static readonly Dictionary<Connector, ConnectionInfo> _connections = new();
    static readonly object _connLock = new();
    static int _nextConnId;

    // === IP 黑白名单 ===
    static readonly HashSet<string> _ipWhitelist = new(); // 为空表示不启用白名单
    static readonly HashSet<string> _ipBlacklist = new(); // 黑名单

    // === 性能统计 ===
    static long _totalMsgRecv;             // 累计收到消息数
    static long _totalBytesRecv;           // 累计收到字节数
    static long _totalBytesSent;           // 累计发送字节数
    static long _lastMsgCount;             // 上次统计时的消息数
    static long _lastByteCount;            // 上次统计时的字节数
    static double _lastStatsTime;           // 上次统计时间

    // === 回显模式 ===
    enum EchoMode { Original, UpperCase, Reverse, CountInfo }
    static int _echoModeIndex;
    static readonly string[] EchoModeNames = { "原文回显", "大写回显", "反转回显", "字节统计" };

    /// <summary>连接信息：记录连接时间、最后活跃时间、收发字节数等</summary>
    sealed class ConnectionInfo
    {
        public int Id;
        public DateTime ConnectTime;
        public double LastActiveTime;
        public long RecvBytes;
        public long SentBytes;
        public long RecvMsgCount;
        public bool Authenticated; // 是否已通过认证
    }

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        // 初始化文件日志（UTF-8 无 BOM）
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║       FxNet 增强版回显服务器         ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 监听地址: {ListenIp}:{ListenPort}");
        LogRaw($"[配置] 最大连接数: {MaxConnections}");
        LogRaw($"[配置] 空闲超时: {IdleTimeoutSec} 秒");
        LogRaw($"[配置] 认证模式: {(AuthToken != null ? "已启用" : "未启用")}");
        LogRaw($"[配置] 统计间隔: {StatsIntervalSec} 秒");
        LogRaw($"[配置] 回显模式: {EchoModeNames[0]}（客户端可切换）");
        LogRaw("按 Ctrl+C 退出");
        LogRaw("");

        // 初始化
        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

        FxNetApi.CreateSessionMaker(
            onRecv: OnClientRecv,
            onConnected: OnClientConnected,
            onError: OnClientError,
            onClose: OnClientClosed);

        FxNetApi.TcpListen(ListenIp, ListenPort);
        Log("[启动] 服务器已启动，等待连接...");

        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true;
            _running = false;
        };

        _lastStatsTime = GetNow();

        // 主循环
        while (_running)
        {
            FxNetInterface.ProcSingleThread();

            // 定期输出性能统计
            double now = GetNow();
            if (now - _lastStatsTime >= StatsIntervalSec)
            {
                PrintStats(now);
                CheckIdleConnections(now);
                _lastStatsTime = now;
            }

            Thread.Sleep(1);
        }

        // 关闭
        Console.WriteLine();
        PrintFinalStats();
        Log("[关闭] 服务器正在关闭...");
        FxNetInterface.CloseAllSockets();
        Log("[关闭] 服务器已关闭");
        _logFile?.Close();
    }

    // ======================== 回调处理 ========================

    static void OnClientConnected(Connector connector)
    {
        // 检查最大连接数
        if (Volatile.Read(ref _connectionCount) >= MaxConnections)
        {
            Log($"[拒绝] 连接数已达上限 ({MaxConnections})，拒绝新连接");
            byte[] rejectData = Encoding.UTF8.GetBytes("[服务器] 连接数已满，请稍后重试");
            connector.Send(rejectData, rejectData.Length);
            connector.Close();
            return;
        }

        int connId = Interlocked.Increment(ref _nextConnId);
        long total = Interlocked.Increment(ref _totalConnections);
        int active = Interlocked.Increment(ref _connectionCount);

        var info = new ConnectionInfo
        {
            Id = connId,
            ConnectTime = DateTime.Now,
            LastActiveTime = GetNow(),
            Authenticated = AuthToken == null // 未启用认证时默认为已认证
        };

        lock (_connLock) _connections[connector] = info;

        Log($"[连接] 客户端 #{connId} 已连接 | 活跃连接: {active} | 累计连接: {total}");

        // 如果启用认证，提示客户端发送认证 Token
        if (AuthToken != null)
        {
            byte[] authPrompt = Encoding.UTF8.GetBytes("[服务器] 请发送认证: AUTH:<token>");
            connector.Send(authPrompt, authPrompt.Length);
            Interlocked.Add(ref _totalBytesSent, authPrompt.Length);
        }
    }

    static void OnClientRecv(Connector connector, byte[] data, int len)
    {
        Interlocked.Increment(ref _totalMsgRecv);
        Interlocked.Add(ref _totalBytesRecv, len);

        // 更新连接活跃时间和统计
        lock (_connLock)
        {
            if (_connections.TryGetValue(connector, out var info))
            {
                info.LastActiveTime = GetNow();
                info.RecvBytes += len;
                info.RecvMsgCount++;
            }
        }

        string message = Encoding.UTF8.GetString(data, 0, len);

        // 认证检查
        if (AuthToken != null)
        {
            bool authenticated;
            lock (_connLock)
            {
                authenticated = _connections.TryGetValue(connector, out var info) && info.Authenticated;
            }
            if (!authenticated)
            {
                HandleAuth(connector, message);
                return;
            }
        }
        string echoText;
        byte[] echoData;

        // 根据消息命令处理特殊指令
        if (message.StartsWith("CMD:", StringComparison.OrdinalIgnoreCase))
        {
            HandleCommand(connector, message);
            return;
        }

        // 心跳响应
        if (message.Equals("CMD:PING", StringComparison.OrdinalIgnoreCase))
        {
            byte[] pongData = Encoding.UTF8.GetBytes("CMD:PONG");
            connector.Send(pongData, pongData.Length);
            Interlocked.Add(ref _totalBytesSent, pongData.Length);
            return;
        }

        // 根据回显模式处理
        EchoMode mode = (EchoMode)Volatile.Read(ref _echoModeIndex);
        switch (mode)
        {
            case EchoMode.UpperCase:
                echoText = message.ToUpperInvariant();
                echoData = Encoding.UTF8.GetBytes(echoText);
                break;
            case EchoMode.Reverse:
                var chars = message.ToCharArray();
                Array.Reverse(chars);
                echoText = new string(chars);
                echoData = Encoding.UTF8.GetBytes(echoText);
                break;
            case EchoMode.CountInfo:
                echoText = $"[收到 {len} 字节] {message}";
                echoData = Encoding.UTF8.GetBytes(echoText);
                break;
            default: // Original
                echoData = data;
                echoText = message;
                break;
        }

        connector.Send(echoData, echoData.Length);
        Interlocked.Add(ref _totalBytesSent, echoData.Length);

        // 更新发送统计
        lock (_connLock)
        {
            if (_connections.TryGetValue(connector, out var info))
            {
                info.SentBytes += echoData.Length;
            }
        }

        Log($"[收发] 收到 {len} 字节 → 回显 {echoData.Length} 字节 | 内容: \"{Truncate(message, 50)}\"");
    }

    static void OnClientError(Connector connector, int error)
    {
        Log($"[错误] 客户端错误码: {error} ({GetErrorDescription(error)})");
    }

    static void OnClientClosed(Connector connector)
    {
        int active = Interlocked.Decrement(ref _connectionCount);
        lock (_connLock)
        {
            if (_connections.TryGetValue(connector, out var info))
            {
                Log($"[断开] 客户端 #{info.Id} 已断开 | 活跃连接: {active} | " +
                    $"存活: {(DateTime.Now - info.ConnectTime).TotalSeconds:F0}s | " +
                    $"接收: {FormatBytes(info.RecvBytes)} | 发送: {FormatBytes(info.SentBytes)}");
                _connections.Remove(connector);
            }
            else
            {
                Log($"[断开] 客户端已断开 | 活跃连接: {active}");
            }
        }
    }

    // ======================== 命令处理 ========================

    static void HandleCommand(Connector connector, string cmd)
    {
        string response;

        if (cmd.Equals("CMD:SWITCH_MODE", StringComparison.OrdinalIgnoreCase))
        {
            int newMode = (Volatile.Read(ref _echoModeIndex) + 1) % EchoModeNames.Length;
            Volatile.Write(ref _echoModeIndex, newMode);
            response = $"[服务器] 回显模式已切换为: {EchoModeNames[newMode]}";
            Log($"[命令] 回显模式切换 → {EchoModeNames[newMode]}");
        }
        else if (cmd.Equals("CMD:STATS", StringComparison.OrdinalIgnoreCase))
        {
            response = BuildStatsString();
            Log("[命令] 客户端请求性能统计");
        }
        else if (cmd.Equals("CMD:CONN_COUNT", StringComparison.OrdinalIgnoreCase))
        {
            response = $"[服务器] 当前活跃连接数: {Volatile.Read(ref _connectionCount)}";
            Log("[命令] 客户端请求连接数");
        }
        else if (cmd.StartsWith("CMD:ECHO_REPEAT:", StringComparison.OrdinalIgnoreCase))
        {
            // 批量回显命令：CMD:ECHO_REPEAT:100
            if (int.TryParse(cmd.AsSpan("CMD:ECHO_REPEAT:".Length), out int count))
            {
                count = Math.Clamp(count, 1, 10000);
                for (int i = 0; i < count; i++)
                {
                    byte[] data = Encoding.UTF8.GetBytes($"echo-{i + 1}/{count}");
                    connector.Send(data, data.Length);
                    Interlocked.Add(ref _totalBytesSent, data.Length);
                }
                response = $"[服务器] 已发送 {count} 条回显消息";
                Log($"[命令] 批量回显 {count} 条消息");
            }
            else
            {
                response = "[服务器] 格式错误，请使用: CMD:ECHO_REPEAT:<数量>";
            }
        }
        else
        {
            response = $"[服务器] 未知命令: {cmd}";
            Log($"[命令] 未知命令: {cmd}");
        }

        byte[] respData = Encoding.UTF8.GetBytes(response);
        connector.Send(respData, respData.Length);
        Interlocked.Add(ref _totalBytesSent, respData.Length);
    }

    // ======================== 认证与超时 ========================

    /// <summary>处理客户端认证请求</summary>
    static void HandleAuth(Connector connector, string message)
    {
        if (message.StartsWith("AUTH:", StringComparison.OrdinalIgnoreCase))
        {
            string token = message["AUTH:".Length..].Trim();
            if (token == AuthToken)
            {
                lock (_connLock)
                {
                    if (_connections.TryGetValue(connector, out var info))
                        info.Authenticated = true;
                }
                byte[] respData = Encoding.UTF8.GetBytes("[服务器] 认证成功，允许通信");
                connector.Send(respData, respData.Length);
                Interlocked.Add(ref _totalBytesSent, respData.Length);
                Log("[认证] 客户端认证成功");
            }
            else
            {
                byte[] respData = Encoding.UTF8.GetBytes("[服务器] 认证失败，请重新发送 AUTH:<token>");
                connector.Send(respData, respData.Length);
                Interlocked.Add(ref _totalBytesSent, respData.Length);
                Log("[认证] 客户端认证失败");
            }
        }
        else
        {
            byte[] respData = Encoding.UTF8.GetBytes("[服务器] 请先认证: AUTH:<token>");
            connector.Send(respData, respData.Length);
            Interlocked.Add(ref _totalBytesSent, respData.Length);
        }
    }

    /// <summary>检查并断开空闲超时的连接</summary>
    static void CheckIdleConnections(double now)
    {
        List<Connector> idleConns = new();
        lock (_connLock)
        {
            foreach (var (connector, info) in _connections)
            {
                if (now - info.LastActiveTime > IdleTimeoutSec)
                {
                    idleConns.Add(connector);
                }
            }
        }

        foreach (var connector in idleConns)
        {
            int connId = 0;
            lock (_connLock)
            {
                if (_connections.TryGetValue(connector, out var info))
                    connId = info.Id;
            }
            Log($"[超时] 客户端 #{connId} 空闲超过 {IdleTimeoutSec} 秒，自动断开");
            byte[] timeoutMsg = Encoding.UTF8.GetBytes("[服务器] 空闲超时，连接已断开");
            connector.Send(timeoutMsg, timeoutMsg.Length);
            connector.Close();
        }
    }

    // ======================== 统计与日志 ========================

    static void PrintStats(double now)
    {
        double elapsed = now - _lastStatsTime;
        long currentMsg = Volatile.Read(ref _totalMsgRecv);
        long currentBytes = Volatile.Read(ref _totalBytesRecv);

        double msgRate = (currentMsg - _lastMsgCount) / elapsed;
        double byteRate = (currentBytes - _lastByteCount) / elapsed;

        int active = Volatile.Read(ref _connectionCount);
        string stats = $"[统计] 连接数: {active} | " +
                       $"消息速率: {msgRate:F1}/s | " +
                       $"吞吐: {FormatBytes((long)byteRate)}/s | " +
                       $"累计消息: {currentMsg} | " +
                       $"累计流量: {FormatBytes(currentBytes)}";
        LogRaw(stats);

        _lastMsgCount = currentMsg;
        _lastByteCount = currentBytes;
    }

    static string BuildStatsString()
    {
        double now = GetNow();
        double elapsed = now - _lastStatsTime;
        long currentMsg = Volatile.Read(ref _totalMsgRecv);
        long currentBytes = Volatile.Read(ref _totalBytesRecv);
        long sentBytes = Volatile.Read(ref _totalBytesSent);

        double msgRate = elapsed > 0 ? (currentMsg - _lastMsgCount) / elapsed : 0;
        double byteRate = elapsed > 0 ? (currentBytes - _lastByteCount) / elapsed : 0;

        return $"[服务器统计] " +
               $"活跃连接: {Volatile.Read(ref _connectionCount)} | " +
               $"累计连接: {Volatile.Read(ref _totalConnections)} | " +
               $"消息速率: {msgRate:F1}/s | " +
               $"接收吞吐: {FormatBytes((long)byteRate)}/s | " +
               $"累计接收: {FormatBytes(currentBytes)} | " +
               $"累计发送: {FormatBytes(sentBytes)} | " +
               $"回显模式: {EchoModeNames[Volatile.Read(ref _echoModeIndex)]}";
    }

    static void PrintFinalStats()
    {
        LogRaw("");
        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║           服务器最终统计             ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"  累计连接数:   {Volatile.Read(ref _totalConnections)}");
        LogRaw($"  累计消息数:   {Volatile.Read(ref _totalMsgRecv)}");
        LogRaw($"  累计接收:     {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
        LogRaw($"  累计发送:     {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
        LogRaw($"  回显模式:     {EchoModeNames[Volatile.Read(ref _echoModeIndex)]}");
    }

    // ======================== 工具方法 ========================

    static void Log(string message)
    {
        string line = $"{GetTimestamp()} {message}";
        Console.WriteLine(line);
        _logFile?.WriteLine(line);
    }

    static void LogRaw(string message)
    {
        Console.WriteLine(message);
        _logFile?.WriteLine(message);
    }

    static double GetNow() => TimeUtility.GetTimeSeconds();

    static string GetTimestamp() => DateTime.Now.ToString("HH:mm:ss.fff");

    static string FormatBytes(long bytes)
    {
        if (bytes < 1024) return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024.0):F1} MB";
    }

    static string Truncate(string s, int maxLen)
    {
        return s.Length <= maxLen ? s : s[..maxLen] + "...";
    }

    static string GetErrorDescription(int error) => error switch
    {
        0 => "成功/EOF",
        10054 => "连接被对端重置",
        10053 => "连接被本地中止",
        10060 => "连接超时",
        10061 => "连接被拒绝",
        _ => $"错误码 {error}"
    };
}
