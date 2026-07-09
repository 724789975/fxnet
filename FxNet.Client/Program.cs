using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.Client;

/// <summary>
/// 增强版测试客户端：
/// - 多种消息类型（文本、数字、JSON 对象、二进制数据）
/// - 批量发送与吞吐量测试
/// - 往返延迟测量
/// - 心跳机制（CMD:PING/CMD:PONG）
/// - 指数退避断线重连
/// - 连接状态机
/// - 服务器命令交互
/// </summary>
class Program
{
    private const string ServerIp = "127.0.0.1";
    private const ushort ServerPort = 9000;
    private const int MaxReconnectAttempts = 5;
    private const int HeartbeatIntervalSec = 10; // 心跳间隔
    private const int HeartbeatTimeoutSec = 5;  // 心跳超时
    private const int MaxHeartbeatFailures = 3;   // 最大心跳失败次数
    private const string LogFilePath = "client.txt";

    static volatile bool _running = true;
    static StreamWriter? _logFile;

    // === 连接状态机 ===
    enum ConnectionState { Disconnected, Connecting, Connected, Reconnecting }
    static volatile ConnectionState _state = ConnectionState.Disconnected;
    static Connector? _connector;

    // === 统计 ===
    static int _reconnectAttempts;
    static long _totalSent, _totalRecv;
    static long _totalBytesSent, _totalBytesRecv;

    // === 心跳 ===
    static double _lastHeartbeatTime;
    static double _heartbeatSendTime;
    static volatile bool _waitingHeartbeatPong;
    static int _heartbeatFailures;

    // === 延迟测量 ===
    static double _pingSendTime;
    static volatile bool _waitingPong;

    // === 消息保留与重发 ===
    static byte[]? _lastSentMessage;
    static bool _resendOnReconnect = true; // 可配置：重连时是否重发最后消息

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;

        // 初始化文件日志（UTF-8 无 BOM）
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║       FxNet 增强版测试客户端         ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 服务器: {ServerIp}:{ServerPort}");
        LogRaw($"[配置] 最大重连次数: {MaxReconnectAttempts}");
        LogRaw($"[配置] 心跳间隔: {HeartbeatIntervalSec} 秒");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true;
            _running = false;
            SetState(ConnectionState.Disconnected);
        };

        // 带指数退避重连的连接循环
        while (_running && _reconnectAttempts <= MaxReconnectAttempts)
        {
            SetState(_reconnectAttempts == 0 ? ConnectionState.Connecting : ConnectionState.Reconnecting);
            Connect();
            if (!_running) break;

            // 连接后的交互循环
            RunSession();
            if (!_running) break;

            // 连接断开，指数退避重连
            if (_reconnectAttempts < MaxReconnectAttempts)
            {
                _reconnectAttempts++;
                int delayMs = CalculateBackoffDelay(_reconnectAttempts);
                Log($"[重连] 第 {_reconnectAttempts}/{MaxReconnectAttempts} 次重连，{delayMs}ms 后重试...");
                SetState(ConnectionState.Reconnecting);
                Thread.Sleep(delayMs);
            }
        }

        PrintFinalStats();
        Log("[关闭] 客户端已退出");
        _logFile?.Close();
    }

    // ======================== 连接管理 ========================

    static void Connect()
    {
        _connector = FxNetApi.CreateConnector(
            onRecv: OnServerRecv,
            onConnected: OnConnected,
            onError: OnError,
            onClose: OnClosed);

        Log($"[连接] 正在连接 {ServerIp}:{ServerPort}...");
        FxNetApi.TcpConnect(_connector, ServerIp, ServerPort);

        // 等待连接建立或失败
        double timeout = GetNow() + 5.0;
        while (_state != ConnectionState.Connected && _running && GetNow() < timeout)
        {
            FxNetInterface.ProcSingleThread();
            Thread.Sleep(10);
        }
    }

    static void RunSession()
    {
        if (_state != ConnectionState.Connected) return;

        _lastHeartbeatTime = GetNow();
        _heartbeatFailures = 0;

        // 重连后重发最后消息（如果启用）
        if (_reconnectAttempts > 0 && _resendOnReconnect && _lastSentMessage != null)
        {
            Log($"[重连] 重发最后消息 ({_lastSentMessage.Length} 字节)...");
            _connector!.Send(_lastSentMessage, _lastSentMessage.Length);
        }

        // 阶段1: 基础消息测试
        Log("\n═══════ 阶段1: 基础消息测试 ═══════");
        SendTestMessages();
        WaitForResponses(8);

        // 阶段2: 延迟测量
        Log("\n═══════ 阶段2: 延迟测量 ═══════");
        MeasureLatency(5);
        WaitForResponses(5);

        // 阶段3: 批量发送测试
        Log("\n═══════ 阶段3: 批量发送测试 ═══════");
        BatchSendTest(50);
        WaitForResponses(52);

        // 阶段4: 服务器命令交互
        Log("\n═══════ 阶段4: 服务器命令交互 ═══════");
        ServerCommandTest();
        WaitForResponses(10);

        // 阶段5: 大数据传输测试
        Log("\n═══════ 阶段5: 大数据传输测试 ═══════");
        LargeDataTest();
        WaitForResponses(3);

        Log("\n═══════ 所有测试完成 ═══════");

        // 保持连接，处理心跳和等待断开
        while (_state == ConnectionState.Connected && _running)
        {
            FxNetInterface.ProcSingleThread();

            // 心跳检测
            double now = GetNow();
            if (now - _lastHeartbeatTime >= HeartbeatIntervalSec)
            {
                SendHeartbeat();
                _lastHeartbeatTime = now;
            }

            // 心跳超时检测
            if (_waitingHeartbeatPong && now - _heartbeatSendTime > HeartbeatTimeoutSec)
            {
                _waitingHeartbeatPong = false;
                _heartbeatFailures++;
                Log($"[心跳] 超时 ({_heartbeatFailures}/{MaxHeartbeatFailures})");

                if (_heartbeatFailures >= MaxHeartbeatFailures)
                {
                    Log("[心跳] 连续超时次数达上限，主动断开连接");
                    _connector?.Close();
                    break;
                }
            }

            Thread.Sleep(10);
        }
    }

    // ======================== 测试阶段 ========================

    /// <summary>阶段1: 发送多种类型的消息</summary>
    static void SendTestMessages()
    {
        // 1. 简单文本
        SendText("Hello, FxNet Server!");

        // 2. 中文字符
        SendText("你好世界！这是一条中文测试消息。");

        // 3. 数字数据
        SendText("NUM:42");
        SendText("NUM:3.14159");

        // 4. 类 JSON 格式
        SendText("{\"type\":\"test\",\"id\":1,\"value\":\"hello\",\"nested\":{\"a\":1,\"b\":2}}");

        // 5. 特殊字符
        SendText("Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?");

        // 6. 空消息
        SendText("");

        // 7. 长消息
        SendText(new string('A', 500));

        // 8. 多行文本
        SendText("Line1\nLine2\nLine3");
    }

    /// <summary>阶段2: 测量往返延迟</summary>
    static void MeasureLatency(int count)
    {
        double totalLatency = 0;
        int successCount = 0;

        for (int i = 0; i < count && _state == ConnectionState.Connected && _running; i++)
        {
            _pingSendTime = GetNow();
            _waitingPong = true;
            SendText($"PING-{i + 1}");

            // 等待响应
            double waitStart = GetNow();
            while (_waitingPong && _state == ConnectionState.Connected && GetNow() - waitStart < 2.0)
            {
                FxNetInterface.ProcSingleThread();
                Thread.Sleep(1);
            }

            if (!_waitingPong)
            {
                double latency = (GetNow() - _pingSendTime) * 1000; // ms
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
        {
            Log($"  [延迟] 平均: {totalLatency / successCount:F2} ms ({successCount}/{count} 成功)");
        }
    }

    /// <summary>阶段3: 批量发送测试</summary>
    static void BatchSendTest(int count)
    {
        Log($"  批量发送 {count} 条消息...");
        double startTime = GetNow();

        for (int i = 0; i < count && _state == ConnectionState.Connected && _running; i++)
        {
            SendText($"batch-msg-{i + 1}");
        }

        double sendTime = GetNow() - startTime;
        Log($"  发送完成，耗时: {sendTime * 1000:F1} ms，速率: {count / sendTime:F0} msg/s");
    }

    /// <summary>阶段4: 服务器命令交互</summary>
    static void ServerCommandTest()
    {
        // 查询连接数
        SendText("CMD:CONN_COUNT");
        Thread.Sleep(200);
        PumpEvents();

        // 查询服务器统计
        SendText("CMD:STATS");
        Thread.Sleep(200);
        PumpEvents();

        // 切换回显模式
        SendText("CMD:SWITCH_MODE");
        Thread.Sleep(200);
        PumpEvents();

        // 在新模式下发送消息
        SendText("Hello after mode switch!");
        Thread.Sleep(200);
        PumpEvents();

        // 再切换一次
        SendText("CMD:SWITCH_MODE");
        Thread.Sleep(200);
        PumpEvents();

        SendText("Hello in new mode!");
        Thread.Sleep(200);
        PumpEvents();

        // 批量回显命令
        SendText("CMD:ECHO_REPEAT:5");
    }

    /// <summary>阶段5: 大数据传输测试</summary>
    static void LargeDataTest()
    {
        // 1KB
        string data1K = new string('X', 1024);
        SendText(data1K);

        // 10KB
        string data10K = new string('Y', 10 * 1024);
        SendText(data10K);

        // 100KB
        string data100K = new string('Z', 100 * 1024);
        SendText(data100K);
    }

    // ======================== 回调处理 ========================

    static void OnConnected(Connector connector)
    {
        SetState(ConnectionState.Connected);
        _reconnectAttempts = 0;
        _heartbeatFailures = 0;
        Log("[连接] 已连接到服务器");
    }

    static void OnServerRecv(Connector connector, byte[] data, int len)
    {
        Interlocked.Increment(ref _totalRecv);
        Interlocked.Add(ref _totalBytesRecv, len);

        string message = Encoding.UTF8.GetString(data, 0, len);

        // 心跳 PONG 响应
        if (message == "CMD:PONG")
        {
            if (_waitingHeartbeatPong)
            {
                _waitingHeartbeatPong = false;
                double rtt = (GetNow() - _heartbeatSendTime) * 1000;
                Log($"[心跳] PONG 响应，RTT: {rtt:F2} ms");
                _heartbeatFailures = 0; // 重置失败计数
            }
            return;
        }

        // 延迟测量响应
        if (_waitingPong && message.StartsWith("PING-"))
        {
            _waitingPong = false;
        }

        // 截断显示
        string display = message.Length > 80 ? message[..77] + "..." : message;
        Log($"[接收] {len} 字节 | \"{display}\"");
    }

    static void OnError(Connector connector, int error)
    {
        Log($"[错误] 错误码: {error} ({GetErrorDescription(error)})");
        SetState(ConnectionState.Disconnected);
    }

    static void OnClosed(Connector connector)
    {
        Log("[连接] 连接已关闭");
        SetState(ConnectionState.Disconnected);
    }

    // ======================== 状态机与心跳 ========================

    /// <summary>设置连接状态并输出日志</summary>
    static void SetState(ConnectionState newState)
    {
        var oldState = _state;
        _state = newState;
        if (oldState != newState)
        {
            Log($"[状态] {oldState} → {newState}");
        }
    }

    /// <summary>发送心跳 PING</summary>
    static void SendHeartbeat()
    {
        if (_connector == null || _state != ConnectionState.Connected) return;
        _waitingHeartbeatPong = true;
        _heartbeatSendTime = GetNow();
        SendText("CMD:PING");
    }

    /// <summary>指数退避延迟计算: 1s → 2s → 4s → 8s → ... 上限 30s</summary>
    static int CalculateBackoffDelay(int attempt)
    {
        int delayMs = 1000 * (1 << (attempt - 1)); // 1s, 2s, 4s, 8s, 16s
        return Math.Min(delayMs, 30000); // 上限 30s
    }

    // ======================== 工具方法 ========================

    static void SendText(string text)
    {
        if (_connector == null || _state != ConnectionState.Connected) return;

        byte[] data = Encoding.UTF8.GetBytes(text);
        _connector.Send(data, data.Length);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, data.Length);

        // 保留最后发送的消息（用于重连时重发）
        _lastSentMessage = data;

        string display = text.Length > 60 ? text[..57] + "..." : text;
        Log($"[发送] {data.Length} 字节 | \"{display}\"");
    }

    static void WaitForResponses(int expectedCount)
    {
        int received = 0;
        long startRecv = Volatile.Read(ref _totalRecv);
        double timeout = GetNow() + 10.0;

        while (received < expectedCount && _state == ConnectionState.Connected && _running && GetNow() < timeout)
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
        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║           客户端最终统计             ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"  发送消息数:   {Volatile.Read(ref _totalSent)}");
        LogRaw($"  接收消息数:   {Volatile.Read(ref _totalRecv)}");
        LogRaw($"  发送字节数:   {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
        LogRaw($"  接收字节数:   {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
        LogRaw($"  重连次数:     {_reconnectAttempts}");
    }

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
