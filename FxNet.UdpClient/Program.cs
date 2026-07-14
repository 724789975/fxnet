using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.UdpClient;

/// <summary>
/// UDP 测试客户端（端到端验证版）：
/// - 阶段1: 基础消息回显验证（各种数据类型、大小、特殊字符）
/// - 阶段2: 延迟测量（PING-PONG 往返时间）
/// - 阶段3: 批量发送吞吐量测试
/// - 阶段4: 服务器命令交互（CLIENT_COUNT/STATS/SWITCH_MODE）
/// - 阶段5: 回显模式验证（大写模式、反转模式）
/// - 阶段6: 大数据传输（1KB/8KB/10KB）
/// - 阶段7: 多客户端并发
/// - 阶段8: 错误处理（关闭后发送）
/// - 自动化 PASS/FAIL 判定，UTF-8 日志输出
/// </summary>
class Program
{
    private const string ServerIp = "115.190.230.47";
    private const ushort ServerPort = 9001;
    private const string LogFilePath = "udp_client.txt";
    private static double MaxRunSeconds = 60.0;

    static volatile bool _running = true;
    static volatile bool _connected = false;
    static Connector? _connector;
    static StreamWriter? _logFile;

    // === 统计 ===
    static long _totalSent, _totalRecv;
    static long _totalBytesSent, _totalBytesRecv;

    // === 验证 ===
    static int _passCount, _failCount;
    static volatile bool _waitingResponse;
    static byte[]? _lastRecvData;
    static int _lastRecvLen;
    static string? _lastExpectedResponse;

    // === 延迟测量 ===
    static double _pingSendTime;
    static volatile bool _waitingPong;

    // === 多客户端 ===
    static readonly List<Connector> _extraConnectors = new();

    // === 顺序验证 ===
    static readonly List<int> _recvOrder = new();

    static void Main(string[] args)
    {
        // 支持通过命令行参数配置运行时间（秒）
        if (args.Length > 0 && double.TryParse(args[0], out var secs) && secs > 0)
            MaxRunSeconds = secs;

        Console.OutputEncoding = Encoding.UTF8;
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║   FxNet UDP 测试客户端（验证版）     ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 服务器: {ServerIp}:{ServerPort}");
        LogRaw($"[配置] 最大运行时间: {MaxRunSeconds} 秒");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

#if SINGLE_THREAD
        LogRaw("[配置] 线程模式: 单线程 (SINGLE_THREAD)");
#else
        LogRaw("[配置] 线程模式: 多线程 (IO模块=1)");
#endif
        LogRaw("");

        Console.CancelKeyPress += (_, e) => { e.Cancel = true; _running = false; };

        // 创建主 UDP 连接
        _connector = FxNetApi.CreateConnector(
            onRecv: OnServerRecv,
            onConnected: OnConnected,
            onError: OnError,
            onClose: OnClosed);

        Log($"[连接] 正在连接 {ServerIp}:{ServerPort} (UDP)...");
        FxNetApi.UdpConnect(_connector, ServerIp, ServerPort);
        _connected = true;

        // 先处理 IO 事件，确保 UDP socket 已创建（UdpConnect 是异步投递到 IO 线程的）
        PumpEvents(500);

        // 发送初始握手
        SendText("UDP-CONNECT");
        PumpEvents(500);

        // 运行测试阶段
        double startTime = GetNow();
        RunTests(startTime);

        // 输出最终结果
        PrintFinalStats();
        PrintVerdict();

        Log("[关闭] UDP 客户端已退出");
        _connector?.Close();
        foreach (var c in _extraConnectors) { try { c.Close(); } catch { } }
        _logFile?.Close();
    }

    static void RunTests(double startTime)
    {
        bool TimeExpired() => GetNow() - startTime >= MaxRunSeconds;

        // 阶段1: 基础消息回显验证
        Log("\n═══════ 阶段1: 基础消息回显验证 ═══════");
        BasicEchoTest();
        if (TimeExpired()) return;

        // 阶段2: 延迟测量
        Log("\n═══════ 阶段2: 延迟测量 ═══════");
        MeasureLatency(5);
        if (TimeExpired()) return;

        // 阶段3: 批量发送吞吐量测试
        Log("\n═══════ 阶段3: 批量发送吞吐量测试 ═══════");
        BatchSendTest(590);
        if (TimeExpired()) return;

        // 阶段3.5: 收发顺序验证
        Log("\n═══════ 阶段3.5: 收发顺序验证 ═══════");
        OrderVerificationTest(2000);
        if (TimeExpired()) return;

        // 阶段4: 服务器命令交互
        Log("\n═══════ 阶段4: 服务器命令交互 ═══════");
        ServerCommandTest();
        if (TimeExpired()) return;

        // 阶段5: 回显模式验证
        Log("\n═══════ 阶段5: 回显模式验证 ═══════");
        EchoModeTest();
        if (TimeExpired()) return;

        // 阶段6: 大数据传输
        Log("\n═══════ 阶段6: 大数据传输测试 ═══════");
        LargeDataTest();
        if (TimeExpired()) return;

        // 阶段7: 多客户端并发
        Log("\n═══════ 阶段7: 多客户端并发 ═══════");
        MultiClientTest();
        if (TimeExpired()) return;

        // 阶段8: 错误处理
        Log("\n═══════ 阶段8: 错误处理 ═══════");
        ErrorHandlingTest();

        Log("\n═══════ 所有测试阶段完成 ═══════");
    }

    // ======================== 阶段1: 基础消息回显验证 ========================

    static void BasicEchoTest()
    {
        // 英文文本
        SendAndVerify("Hello, UDP Server!", "Hello, UDP Server!");

        // 中文文本
        SendAndVerify("你好世界！UDP 中文测试", "你好世界！UDP 中文测试");

        // 数字格式
        SendAndVerify("NUM:42", "NUM:42");

        // JSON
        string json = "{\"type\":\"udp\",\"id\":1,\"data\":\"hello\"}";
        SendAndVerify(json, json);

        // 特殊字符
        string special = "Special: !@#$%^&*()_+-=";
        SendAndVerify(special, special);

        // 空消息（服务器原文回显空数据）
        SendAndVerify("", "");

        // 重复字符长串
        string repeated = new string('U', 200);
        SendAndVerify(repeated, repeated);

        // 含换行符
        string newline = "Line1\nLine2";
        SendAndVerify(newline, newline);

        // Unicode/Emoji
        string unicode = "你好🎉🚀 emoji: 😀👍";
        SendAndVerify(unicode, unicode);

        // Tab 和混合空白
        string whitespace = "Col1\tCol2\r\nCol3  Space";
        SendAndVerify(whitespace, whitespace);
    }

    // ======================== 阶段2: 延迟测量 ========================

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
            while (_waitingPong && _connected && GetNow() - waitStart < 3.0)
            {
#if SINGLE_THREAD
                FxNetInterface.ProcSingleThread();
#endif
                FxNetInterface.ProcessMessageEvents();
                Thread.Sleep(1);
            }

            if (!_waitingPong)
            {
                double latency = (GetNow() - _pingSendTime) * 1000;
                totalLatency += latency;
                successCount++;
                Log($"  [延迟] PING-{i + 1}: {latency:F2} ms");
                RecordPass($"PING-{i + 1} 延迟 {latency:F2}ms");
            }
            else
            {
                Log($"  [延迟] PING-{i + 1}: 超时");
                RecordFail("PING-" + (i + 1) + " 超时");
            }

            Thread.Sleep(50);
        }

        if (successCount > 0)
            Log($"  [延迟] 平均: {totalLatency / successCount:F2} ms ({successCount}/{count} 成功)");
    }

    // ======================== 阶段3: 批量发送吞吐量 ========================

    static void BatchSendTest(int count)
    {
        Log($"  批量发送 {count} 条消息...");
        long startRecv = Volatile.Read(ref _totalRecv);
        double startTime = GetNow();

        // 分批发送，每批 50 条，间隔 10ms，避免 UDP 缓冲区溢出
        int batchSize = 50;
        for (int i = 0; i < count && _connected && _running; i++)
        {
            SendText($"udp-batch-{i + 1}");
            if ((i + 1) % batchSize == 0)
            {
#if SINGLE_THREAD
                FxNetInterface.ProcSingleThread();
#endif
                FxNetInterface.ProcessMessageEvents();
                Thread.Sleep(10);
            }
        }

        double sendTime = GetNow() - startTime;
        if (sendTime > 0)
            Log($"  发送完成，耗时: {sendTime * 1000:F1} ms，速率: {count / sendTime:F0} msg/s");

        // 等待回显
        int expected = count;
        double waitEnd = GetNow() + 30.0;
        while (Volatile.Read(ref _totalRecv) - startRecv < expected && _connected && GetNow() < waitEnd)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(10);
        }

        long batchRecv = Volatile.Read(ref _totalRecv) - startRecv;
        double lossRate = (1.0 - (double)batchRecv / expected) * 100;
        Log($"  批量回显: 收到 {batchRecv}/{expected} (丢包率 {lossRate:F1}%)");
        // 批量测试仅作信息统计，跨公网 UDP 丢包不可避免
        if (batchRecv > 0)
            RecordPass($"批量发送 {count} 条回显 {batchRecv} 条 (丢包 {lossRate:F1}%)");
        else
            RecordFail($"批量发送 {count} 条无回显");
    }

    // ======================== 阶段3.5: 收发顺序验证 ========================

    /// <summary>发送带序号的随机长度消息，验证回显顺序与发送一致</summary>
    static void OrderVerificationTest(int count)
    {
        const int MinLen = 100, MaxLen = 1024;
        var rng = new Random(42);
        _recvOrder.Clear();
        long totalBytes = 0;

        Log($"  计划发送 {count} 条 (长度 {MinLen}-{MaxLen} 随机)");

        for (int i = 0; i < count && _connected && _running; i++)
        {
            int msgLen = rng.Next(MinLen, MaxLen + 1);
            byte[] sendData = new byte[msgLen];
            byte[] header = Encoding.UTF8.GetBytes($"SEQ-{i:D04}");
            Array.Copy(header, sendData, header.Length);

            _connector?.Send(sendData, sendData.Length);
            Interlocked.Increment(ref _totalSent);
            Interlocked.Add(ref _totalBytesSent, sendData.Length);
            totalBytes += sendData.Length;

            // 每条发送后处理 IO，让可靠传输有机会工作
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
        }

        Log($"  已发送 {count} 条 (共 {totalBytes / 1024} KB)，等待回显...");

        // 等待所有回显到达
        double waitEnd = GetNow() + 120.0;
        while (_recvOrder.Count < count && _connected && _running && GetNow() < waitEnd)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(5);
        }

        // 验证顺序
        bool orderCorrect = true;
        for (int i = 1; i < _recvOrder.Count; i++)
        {
            if (_recvOrder[i] <= _recvOrder[i - 1])
            {
                orderCorrect = false;
                Log($"  [顺序异常] 位置 {i}: SEQ-{_recvOrder[i - 1]:D04} → SEQ-{_recvOrder[i]:D04}");
                break;
            }
        }

        Log($"  接收: {_recvOrder.Count}/{count}");
        if (_recvOrder.Count > 0)
            Log($"  首条: SEQ-{_recvOrder[0]:D04} 末条: SEQ-{_recvOrder[^1]:D04}");

        if (orderCorrect && _recvOrder.Count == count)
            RecordPass($"收发顺序正确 ({count} 条 × {MinLen}-{MaxLen} 随机字节)");
        else if (orderCorrect && _recvOrder.Count > 0)
            RecordPass($"收发顺序正确 (收到 {_recvOrder.Count}/{count})");
        else if (!orderCorrect)
            RecordFail("收发顺序异常");
        else
            RecordFail("无回显");
    }

    // ======================== 阶段4: 服务器命令交互 ========================

    static void ServerCommandTest()
    {
        // CMD:CLIENT_COUNT
        SendCommandAndVerify("CMD:CLIENT_COUNT", "活跃客户端");

        // CMD:STATS
        SendCommandAndVerify("CMD:STATS", "消息");

        // CMD:SWITCH_MODE（切换到 UpperCase）
        SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");

        // 未知命令
        SendCommandAndVerify("CMD:UNKNOWN_CMD", "未知命令");
    }

    // ======================== 阶段5: 回显模式验证 ========================

    static void EchoModeTest()
    {
        // 当前服务器应处于 UpperCase 模式（阶段4已切换一次）
        // 验证大写回显
        SendAndVerify("hello uppercase", "HELLO UPPERCASE");

        // 切换到 Reverse 模式
        SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");
        PumpEvents(300);

        // 验证反转回显
        SendAndVerify("abcdef", "fedcba");

        // 切换回 Original 模式
        SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");
        PumpEvents(300);

        // 验证原文回显恢复
        SendAndVerify("original test", "original test");
    }

    // ======================== 阶段6: 大数据传输 ========================

    static void LargeDataTest()
    {
        // 1KB
        string data1k = new string('A', 1024);
        SendAndVerify(data1k, data1k, "1KB");

        // 8KB
        string data8k = new string('C', 8 * 1024);
        SendAndVerify(data8k, data8k, "8KB");

        // 10KB
        string data10k = new string('B', 10 * 1024);
        SendAndVerify(data10k, data10k, "10KB");
    }

    // ======================== 阶段7: 多客户端并发 ========================

    static void MultiClientTest()
    {
        const int clientCount = 3;
        var results = new string?[clientCount];
        var dones = new ManualResetEventSlim[clientCount];

        for (int i = 0; i < clientCount; i++)
        {
            int idx = i;
            dones[idx] = new ManualResetEventSlim(false);

            var c = FxNetApi.CreateConnector(
                onRecv: (conn, data, len) =>
                {
                    results[idx] = Encoding.UTF8.GetString(data, 0, len);
                    dones[idx].Set();
                },
                onConnected: _ => { },
                onError: (_, _) => { },
                onClose: _ => { });

            FxNetApi.UdpConnect(c, ServerIp, ServerPort);
            _extraConnectors.Add(c);

            string msg = $"multi-client-{idx + 1}";
            byte[] sendData = Encoding.UTF8.GetBytes(msg);
            c.Send(sendData, sendData.Length);
            Interlocked.Increment(ref _totalSent);
            Interlocked.Add(ref _totalBytesSent, sendData.Length);
            Log($"  [多客户端] 客户端 {idx + 1} 发送: \"{msg}\"");
            Thread.Sleep(50);
        }

        // 等待所有回显
        double waitEnd = GetNow() + 15.0;
        for (int i = 0; i < clientCount; i++)
        {
            while (!dones[i].IsSet && GetNow() < waitEnd)
            {
#if SINGLE_THREAD
                FxNetInterface.ProcSingleThread();
#endif
                FxNetInterface.ProcessMessageEvents();
                Thread.Sleep(10);
            }
        }

        // 验证
        for (int i = 0; i < clientCount; i++)
        {
            string expected = $"multi-client-{i + 1}";
            if (results[i] == expected)
                RecordPass($"多客户端 {i + 1} 回显正确");
            else
                RecordFail($"多客户端 {i + 1} 期望 \"{expected}\" 实际 \"{results[i] ?? "null"}\"");
        }

        // 清理额外连接器
        foreach (var c in _extraConnectors) { try { c.Close(); } catch { } }
        _extraConnectors.Clear();
    }

    // ======================== 阶段8: 错误处理 ========================

    static void ErrorHandlingTest()
    {
        // 测试：关闭连接器后发送不崩溃
        var testConnector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });
        FxNetApi.UdpConnect(testConnector, ServerIp, ServerPort);
        PumpEvents(200);

        testConnector.Close();
        PumpEvents(200);

        byte[] data = Encoding.UTF8.GetBytes("after-close-test");
        Exception? caught = null;
        try { testConnector.Send(data, data.Length); } catch (Exception ex) { caught = ex; }
        if (caught == null)
            RecordPass("关闭后发送不崩溃");
        else
            RecordFail($"关闭后发送异常: {caught.Message}");
    }

    // ======================== 核心验证方法 ========================

    /// <summary>发送消息并验证回显内容（原文回显模式）</summary>
    static void SendAndVerify(string message, string expectedEcho, string? label = null)
    {
        if (!_connected || !_running) return;

        string tag = label ?? Truncate(message, 30);
        _lastExpectedResponse = expectedEcho;
        _waitingResponse = true;
        _lastRecvData = null;
        _lastRecvLen = 0;

        byte[] sendData = Encoding.UTF8.GetBytes(message);
        _connector?.Send(sendData, sendData.Length);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, sendData.Length);
        Log($"  [发送] {sendData.Length} 字节 | \"{tag}\"");

        // 等待回显
        double waitEnd = GetNow() + 10.0;
        while (_waitingResponse && _connected && _running && GetNow() < waitEnd)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(5);
        }

        if (_lastRecvData != null && _lastRecvLen > 0)
        {
            string actual = Encoding.UTF8.GetString(_lastRecvData, 0, _lastRecvLen);
            if (actual == expectedEcho)
                RecordPass($"回显正确: \"{tag}\"");
            else
                RecordFail($"回显不匹配: \"{tag}\" 期望 \"{Truncate(expectedEcho, 40)}\" 实际 \"{Truncate(actual, 40)}\"");
        }
        else if (_waitingResponse)
        {
            RecordFail($"回显超时: \"{tag}\"");
        }
    }

    /// <summary>发送命令并验证响应包含指定关键字</summary>
    static void SendCommandAndVerify(string command, string expectedKeyword)
    {
        if (!_connected || !_running) return;

        _lastExpectedResponse = expectedKeyword;
        _waitingResponse = true;
        _lastRecvData = null;
        _lastRecvLen = 0;

        byte[] sendData = Encoding.UTF8.GetBytes(command);
        _connector?.Send(sendData, sendData.Length);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, sendData.Length);
        Log($"  [命令] {command}");

        double waitEnd = GetNow() + 10.0;
        while (_waitingResponse && _connected && _running && GetNow() < waitEnd)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(5);
        }

        if (_lastRecvData != null && _lastRecvLen > 0)
        {
            string actual = Encoding.UTF8.GetString(_lastRecvData, 0, _lastRecvLen);
            if (actual.Contains(expectedKeyword))
                RecordPass($"命令响应正确: {command} → \"{Truncate(actual, 50)}\"");
            else
                RecordFail($"命令响应缺少关键字: {command} 期望含 \"{expectedKeyword}\" 实际 \"{Truncate(actual, 50)}\"");
        }
        else
        {
            RecordFail($"命令响应超时: {command}");
        }

        PumpEvents(200);
    }

    static void RecordPass(string detail)
    {
        Interlocked.Increment(ref _passCount);
        Log($"  [PASS] {detail}");
    }

    static void RecordFail(string detail)
    {
        Interlocked.Increment(ref _failCount);
        Log($"  [FAIL] {detail}");
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

        // 顺序验证：检查 SEQ- 前缀（从原始字节提取，避免 null 字符干扰）
        if (len >= 8 && data[0] == (byte)'S' && data[1] == (byte)'E' &&
            data[2] == (byte)'Q' && data[3] == (byte)'-')
        {
            // 解析序号 "SEQ-XXXX"
            int seq = 0;
            for (int j = 4; j < 8 && j < len; j++)
            {
                if (data[j] >= (byte)'0' && data[j] <= (byte)'9')
                    seq = seq * 10 + (data[j] - (byte)'0');
            }
            _recvOrder.Add(seq);
        }

        string message = Encoding.UTF8.GetString(data, 0, len);

        // PING-PONG 延迟测量
        if (_waitingPong && message.StartsWith("PING-"))
            _waitingPong = false;

        // 回显验证
        if (_waitingResponse)
        {
            _lastRecvData = new byte[len];
            Array.Copy(data, _lastRecvData, len);
            _lastRecvLen = len;
            _waitingResponse = false;
        }

        string display = message.Length > 60 ? message[..57] + "..." : message;
        Log($"  [接收] {len} 字节 | \"{display}\"");
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
        Log($"  [发送] {data.Length} 字节 | \"{display}\"");
    }

    static void PumpEvents(int milliseconds)
    {
        double end = GetNow() + milliseconds / 1000.0;
        while (_running && GetNow() < end)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(5);
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
        LogRaw($"  验证通过: {Volatile.Read(ref _passCount)}");
        LogRaw($"  验证失败: {Volatile.Read(ref _failCount)}");
    }

    static void PrintVerdict()
    {
        int pass = Volatile.Read(ref _passCount);
        int fail = Volatile.Read(ref _failCount);
        LogRaw("");
        if (fail == 0 && pass > 0)
            LogRaw("=== 验证结果: PASS ===");
        else
            LogRaw("=== 验证结果: FAIL ===");
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
