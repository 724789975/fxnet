using System.Text;
using FxNet;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.UdpMultiClient;

class Program
{
    private const string ServerIp = "115.190.230.47";
    private const ushort ServerPort = 9001;
    private const string LogFilePath = "udp_multi_client.txt";
    private static double MaxRunSeconds = 300.0;

    static volatile bool _running = true;
    static readonly Dictionary<int, Connector> _connectors = new();
    static StreamWriter? _logFile;

    static long _totalSent, _totalRecv;
    static long _totalBytesSent, _totalBytesRecv;
    static int _passCount, _failCount;

    static readonly object _statsLock = new();
    static readonly Dictionary<int, ClientStats> _clientStats = new();

    sealed class ClientStats
    {
        public int Id;
        public volatile bool Connected;
        public long Sent;
        public long Recv;
        public long Errors;
        public volatile bool Running;
        public string? LastResponse;
        public DateTime LastActive;
        public DateTime ConnectTime;
        public Connector? CurrentConnector;
    }

    static void Main(string[] args)
    {
        if (args.Length > 0 && double.TryParse(args[0], out var secs) && secs > 0)
            MaxRunSeconds = secs;

        Console.OutputEncoding = Encoding.UTF8;
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║    FxNet UDP 多客户端并发测试        ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 服务器: {ServerIp}:{ServerPort}");
        LogRaw($"[配置] 最大运行时间: {MaxRunSeconds} 秒");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

#if SINGLE_THREAD
        LogRaw("[配置] 线程模式: 单线程");
#else
        LogRaw("[配置] 线程模式: 多线程");
#endif
        LogRaw("");

        Console.CancelKeyPress += (_, e) => { e.Cancel = true; _running = false; };

        double startTime = GetNow();

        RunMultiClientTest(startTime);

        PrintFinalStats();
        PrintVerdict();

        Log("[关闭] 多客户端测试已退出");
        foreach (var kvp in _connectors) { try { kvp.Value.Close(); } catch { } }
        _logFile?.Close();
    }

    static void RunMultiClientTest(double startTime)
    {
        const int TotalClients = 15;
        const int BatchSize = 5;

        Log("\n═══════ 阶段1: 分批创建客户端连接 ═══════");
        for (int i = 0; i < TotalClients; i += BatchSize)
        {
            int count = Math.Min(BatchSize, TotalClients - i);
            Log($"  创建客户端 {i + 1} - {i + count}...");

            for (int j = 0; j < count; j++)
            {
                CreateClient(i + j + 1);
                Thread.Sleep(50);
            }

            PumpEvents(500);
            Log($"  当前活跃客户端: {CountConnectedClients()}");

            if (GetNow() - startTime >= MaxRunSeconds) return;
        }

        Log("\n═══════ 阶段2: 所有客户端并发发送消息 ═══════");
        for (int i = 1; i <= TotalClients; i++)
        {
            SendClientMessage(i, $"concurrent-test-{i}-msg-1");
            Thread.Sleep(10);
        }
        PumpEvents(3000);

        for (int i = 1; i <= TotalClients; i++)
        {
            SendClientMessage(i, $"concurrent-test-{i}-msg-2");
            Thread.Sleep(10);
        }
        PumpEvents(3000);

        Log("\n═══════ 阶段3: 客户端混合消息大小测试 ═══════");
        var rng = new Random(42);
        for (int i = 1; i <= TotalClients; i++)
        {
            int size = rng.Next(100, 500);
            SendClientMessage(i, $"mixed-{i}-{new string('X', size)}");
            Thread.Sleep(20);
        }
        PumpEvents(5000);

        Log("\n═══════ 阶段4: 客户端消息验证 ═══════");
        for (int i = 1; i <= TotalClients; i++)
        {
            SendAndVerify(i, $"verify-{i}", $"verify-{i}", startTime);
            Thread.Sleep(50);
        }

        Log("\n═══════ 阶段5: 长时间稳定运行测试(30秒) ═══════");
        long startRecv = Volatile.Read(ref _totalRecv);
        double phaseStart = GetNow();
        
        for (int round = 1; round <= 3; round++)
        {
            for (int i = 1; i <= TotalClients && GetNow() - startTime < MaxRunSeconds; i++)
            {
                SendClientMessage(i, $"long-run-{round}-{i}");
                Thread.Sleep(20);
            }
            PumpEvents(2000);
        }
        
        double phaseEnd = GetNow();
        long phaseRecv = Volatile.Read(ref _totalRecv) - startRecv;
        Log($"  30秒内收到 {phaseRecv} 条消息，速率: {phaseRecv / (phaseEnd - phaseStart):F1} msg/s");

        Log("\n═══════ 所有测试阶段完成 ═══════");
    }

    static void CreateClient(int id)
    {
        var stats = new ClientStats { Id = id, Running = true, ConnectTime = DateTime.Now };

        lock (_statsLock)
        {
            _clientStats[id] = stats;
        }

        var connector = FxNetApi.CreateConnector(
            onRecv: (conn, data, len) =>
            {
                Interlocked.Increment(ref _totalRecv);
                Interlocked.Add(ref _totalBytesRecv, len);
                
                lock (_statsLock)
                {
                    if (_clientStats.TryGetValue(id, out var s))
                    {
                        s.Recv++;
                        s.LastActive = DateTime.Now;
                        s.LastResponse = Encoding.UTF8.GetString(data, 0, len);
                    }
                }
            },
            onConnected: _ =>
            {
                lock (_statsLock)
                {
                    if (_clientStats.TryGetValue(id, out var s))
                    {
                        s.Connected = true;
                        s.LastActive = DateTime.Now;
                    }
                }
                Log($"  [连接] 客户端 {id} 已连接");
            },
            onError: (_, _) =>
            {
                lock (_statsLock)
                {
                    if (_clientStats.TryGetValue(id, out var s))
                    {
                        s.Errors++;
                    }
                }
                Log($"  [错误] 客户端 {id} 错误");
            },
            onClose: _ =>
            {
                lock (_statsLock)
                {
                    if (_clientStats.TryGetValue(id, out var s))
                    {
                        s.Connected = false;
                    }
                }
                Log($"  [关闭] 客户端 {id} 已断开");
            });

        lock (_statsLock)
        {
            _connectors[id] = connector;
            stats.CurrentConnector = connector;
        }

        Log($"  [创建] 客户端 {id}");
        FxNetApi.UdpConnect(connector, ServerIp, ServerPort);

        PumpEvents(300);
    }

    static void SendClientMessage(int id, string message)
    {
        Connector? connector = null;
        lock (_statsLock)
        {
            if (_connectors.TryGetValue(id, out var c))
                connector = c;
        }

        if (connector == null)
        {
            Log($"  [警告] 客户端 {id} 无有效连接");
            return;
        }

        byte[] data = Encoding.UTF8.GetBytes(message);
        connector.Send(data, data.Length);
        Interlocked.Increment(ref _totalSent);
        Interlocked.Add(ref _totalBytesSent, data.Length);

        lock (_statsLock)
        {
            if (_clientStats.TryGetValue(id, out var s))
            {
                s.Sent++;
                s.LastActive = DateTime.Now;
            }
        }
    }

    static void SendAndVerify(int clientId, string message, string expected, double startTime)
    {
        double waitEnd = GetNow() + 10.0;

        while (!_clientStats.ContainsKey(clientId) && GetNow() < waitEnd)
            Thread.Sleep(10);

        ClientStats? stats = null;
        lock (_statsLock)
        {
            if (_clientStats.TryGetValue(clientId, out var s))
                stats = s;
        }

        if (stats == null || !stats.Connected)
        {
            RecordFail($"客户端 {clientId} 未连接");
            return;
        }

        string lastResponse = stats.LastResponse ?? "";
        string oldResponse = lastResponse;

        SendClientMessage(clientId, message);

        waitEnd = GetNow() + 5.0;
        while (GetNow() < waitEnd)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(10);

            lock (_statsLock)
            {
                if (_clientStats.TryGetValue(clientId, out var s))
                {
                    if (s.LastResponse != oldResponse)
                    {
                        if (s.LastResponse == expected)
                            RecordPass($"客户端 {clientId} 回显正确: \"{message}\"");
                        else
                            RecordFail($"客户端 {clientId} 回显不匹配: 期望 \"{expected}\" 实际 \"{s.LastResponse ?? "null"}\"");
                        return;
                    }
                }
            }
        }

        RecordFail($"客户端 {clientId} 回显超时: \"{message}\"");
    }

    static int CountConnectedClients()
    {
        lock (_statsLock)
        {
            return _clientStats.Count(kvp => kvp.Value.Connected);
        }
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
        LogRaw("═══ 多客户端测试最终统计 ═══");
        LogRaw($"  发送消息数: {Volatile.Read(ref _totalSent)}");
        LogRaw($"  接收消息数: {Volatile.Read(ref _totalRecv)}");
        LogRaw($"  发送字节数: {FormatBytes(Volatile.Read(ref _totalBytesSent))}");
        LogRaw($"  接收字节数: {FormatBytes(Volatile.Read(ref _totalBytesRecv))}");
        LogRaw($"  验证通过: {Volatile.Read(ref _passCount)}");
        LogRaw($"  验证失败: {Volatile.Read(ref _failCount)}");

        LogRaw("");
        LogRaw("═══ 各客户端统计 ═══");
        lock (_statsLock)
        {
            foreach (var stat in _clientStats.Values)
            {
                string status = stat.Connected ? "✅" : "❌";
                LogRaw($"  客户端 {stat.Id}: {status} 发送={stat.Sent} 接收={stat.Recv} 错误={stat.Errors}");
            }
        }
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
}
