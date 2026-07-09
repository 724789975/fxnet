using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.UdpClient;

/// <summary>
/// UDP 测试客户端：
/// - 阶段1: 基础 UDP 收发（发送多种消息类型，验证回显正确性）
/// - 阶段2: 延迟测量（PING-PONG 往返时间）
/// - 阶段3: 批量发送吞吐量测试
/// - 阶段4: 服务器命令交互（模式切换、统计查询）
/// - 阶段5: 大数据传输（1KB/10KB/64KB）
/// - 连接状态监控与日志输出（UTF-8 编码写入 udp_client.txt）
/// </summary>
class Program
{
    private const string ServerIp = "115.190.230.47";
    private const ushort ServerPort = 9001;
    private const string LogFilePath = "udp_client.txt";
    private const double MaxRunSeconds = 30.0;

    static volatile bool _running = true;
    static volatile bool _connected = false;
    static Connector? _connector;
    static StreamWriter? _logFile;

    // === 统计 ===
    static long _totalSent, _totalRecv;
    static long _totalBytesSent, _totalBytesRecv;

    // === 延迟测量 ===
    static double _pingSendTime;
    static volatile bool _waitingPong;

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║       FxNet UDP 测试客户端           ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 服务器: {ServerIp}:{ServerPort}");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

        Console.CancelKeyPress += (_, e) => { e.Cancel = true; _running = false; };

        // 创建 UDP 连接
        _connector = FxNetApi.CreateConnector(
            onRecv: OnServerRecv,
            onConnected: OnConnected,
            onError: OnError,
            onClose: OnClosed);

        Log($"[连接] 正在连接 {ServerIp}:{ServerPort} (UDP)...");
        FxNetApi.UdpConnect(_connector, ServerIp, ServerPort);

        // UDP 是无连接的，BC 已初始化为 Established 状态，直接标记已连接
        _connected = true;

        // 发送初始握手消息触发服务器响应
        SendText("UDP-CONNECT");

        // 运行测试阶段（限时30秒）
        double startTime = GetNow();
        RunTests(startTime);

        // 如果还有剩余时间，继续处理事件直到30秒
        double remaining = MaxRunSeconds - (GetNow() - startTime);
        if (remaining > 0)
        {
            Log($"\n[等待] 测试已完成，继续运行 {remaining:F1} 秒...");
            double idleEnd = GetNow() + remaining;
            while (_connected && _running && GetNow() < idleEnd)
            {
                FxNetInterface.ProcSingleThread();
                Thread.Sleep(50);
            }
        }

        PrintFinalStats();
        Log("[关闭] UDP 客户端已退出");
        _connector?.Close();
        _logFile?.Close();
    }

    static void RunTests(double startTime)
    {
        // 阶段1: 基础消息测试
        Log("\n═══════ 阶段1: 基础 UDP 消息测试 ═══════");
        SendTestMessages();
        WaitForResponses(8);
        if (GetNow() - startTime >= MaxRunSeconds) return;

        // 阶段2: 延迟测量
        Log("\n═══════ 阶段2: 延迟测量 ═══════");
        MeasureLatency(5);
        WaitForResponses(5);
        if (GetNow() - startTime >= MaxRunSeconds) return;

        // 阶段3: 批量发送测试
        Log("\n═══════ 阶段3: 批量发送测试 ═══════");
        BatchSendTest(50);
        WaitForResponses(50);
        if (GetNow() - startTime >= MaxRunSeconds) return;

        // 阶段4: 服务器命令交互
        Log("\n═══════ 阶段4: 服务器命令交互 ═══════");
        ServerCommandTest();
        WaitForResponses(5);
        if (GetNow() - startTime >= MaxRunSeconds) return;

        // 阶段5: 大数据传输测试
        Log("\n═══════ 阶段5: 大数据传输测试 ═══════");
        LargeDataTest();
        WaitForResponses(3);

        Log("\n═══════ 所有测试完成 ═══════");
    }

    // ======================== 测试阶段 ========================

    static void SendTestMessages()
    {
        SendText("Hello, UDP Server!");
        SendText("你好世界！UDP 中文测试");
        SendText("NUM:42");
        SendText("{\"type\":\"udp\",\"id\":1}");
        SendText("Special: !@#$%^&*()");
        SendText("");
        SendText(new string('U', 200));
        SendText("Line1\nLine2");
    }

    static void MeasureLatency(int count)
    {
        double totalLatency = 0;
        int successCount = 0;

        for (int i = 0; i < count && _connected && _running; i++)
        {
            _pingSendTime = GetNow();
            _waitingPong = true;
            SendText($"PING-{i + 1}");

            double waitStart = GetNow();
            while (_waitingPong && _connected && GetNow() - waitStart < 2.0)
            {
                FxNetInterface.ProcSingleThread();
                Thread.Sleep(1);
            }

            if (!_waitingPong)
            {
                double latency = (GetNow() - _pingSendTime) * 1000;
                totalLatency += latency;
                successCount++;
                Log($"  [延迟] PING-{i + 1}: {latency:F2} ms");
            }
            else
            {
                Log($"  [延迟] PING-{i + 1}: 超时");
            }

            Thread.Sleep(50);
        }

        if (successCount > 0)
            Log($"  [延迟] 平均: {totalLatency / successCount:F2} ms ({successCount}/{count} 成功)");
    }

    static void BatchSendTest(int count)
    {
        Log($"  批量发送 {count} 条消息...");
        double startTime = GetNow();

        for (int i = 0; i < count && _connected && _running; i++)
        {
            SendText($"udp-batch-{i + 1}");
        }

        double sendTime = GetNow() - startTime;
        if (sendTime > 0)
            Log($"  发送完成，耗时: {sendTime * 1000:F1} ms，速率: {count / sendTime:F0} msg/s");
    }

    static void ServerCommandTest()
    {
        SendText("CMD:CLIENT_COUNT");
        Thread.Sleep(200);
        PumpEvents();

        SendText("CMD:STATS");
        Thread.Sleep(200);
        PumpEvents();

        SendText("CMD:SWITCH_MODE");
        Thread.Sleep(200);
        PumpEvents();

        SendText("Hello after mode switch!");
        Thread.Sleep(200);
        PumpEvents();
    }

    static void LargeDataTest()
    {
        // 1KB
        SendText(new string('A', 1024));
        // 10KB
        SendText(new string('B', 10 * 1024));
        // 8KB (UDP 安全大小，避免分片丢失；BufferContral 单包有 1024 字节载荷限制)
        SendText(new string('C', 8 * 1024));
    }

    // ======================== 回调处理 ========================

    static void OnConnected(Connector connector)
    {
        _connected = true;
        Log("[连接] 已连接到 UDP 服务器");
    }

    static void OnServerRecv(Connector connector, byte[] data, int len)
    {
        Interlocked.Increment(ref _totalRecv);
        Interlocked.Add(ref _totalBytesRecv, len);

        string message = Encoding.UTF8.GetString(data, 0, len);

        if (_waitingPong && message.StartsWith("PING-"))
            _waitingPong = false;

        string display = message.Length > 60 ? message[..57] + "..." : message;
        Log($"[接收] {len} 字节 | \"{display}\"");
    }

    static void OnError(Connector connector, int error)
    {
        Log($"[错误] 错误码: {error}");
        _connected = false;
    }

    static void OnClosed(Connector connector)
    {
        Log("[连接] 连接已关闭");
        _connected = false;
    }

    // ======================== 工具方法 ========================

    static void SendText(string text)
    {
        if (_connector == null || !_connected) return;
        byte[] data = Encoding.UTF8.GetBytes(text);
        _connector.Send(data, data.Length);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, data.Length);

        string display = text.Length > 50 ? text[..47] + "..." : text;
        Log($"[发送] {data.Length} 字节 | \"{display}\"");
    }

    /// <summary>绕过 _connected 检查直接发送（用于初始握手）</summary>
    static void SendRaw(string text)
    {
        if (_connector?.Session == null) return;
        byte[] data = Encoding.UTF8.GetBytes(text);
        _connector.Session.Send(data, data.Length, null);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, data.Length);
        Log($"[发送-原始] {data.Length} 字节 | \"{text}\"");
    }

    static void WaitForResponses(int expectedCount)
    {
        int received = 0;
        long startRecv = Volatile.Read(ref _totalRecv);
        double timeout = GetNow() + 10.0;

        while (received < expectedCount && _connected && _running && GetNow() < timeout)
        {
            FxNetInterface.ProcSingleThread();
            Thread.Sleep(10);
            received = (int)(Volatile.Read(ref _totalRecv) - startRecv);
        }

        Log($"  等待响应: 收到 {received}/{expectedCount}");
    }

    static void PumpEvents()
    {
        for (int i = 0; i < 10 && _running; i++)
        {
            FxNetInterface.ProcSingleThread();
            Thread.Sleep(10);
        }
    }

    static void PrintFinalStats()
    {
        LogRaw("");
        LogRaw("═══ UDP 客户端最终统计 ═══");
        LogRaw($"  发送消息数: {Volatile.Read(ref _totalSent)}");
        LogRaw($"  接收消息数: {Volatile.Read(ref _totalRecv)}");
        LogRaw($"  发送字节数: {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
        LogRaw($"  接收字节数: {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
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
}
