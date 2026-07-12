using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.MultiClient;

/// <summary>
/// 多客户端并发测试程序：
/// - 多个客户端同时连接到服务器
/// - 每个客户端发送带唯一标识的消息
/// - 验证回显内容正确、消息不混淆
/// - 测试并发连接管理和资源释放
/// </summary>
class Program
{
    private const string ServerIp = "127.0.0.1";
    private const ushort ServerPort = 9000;
    private const int ClientCount = 5;       // 并发客户端数量
    private const int MsgsPerClient = 20;    // 每个客户端发送的消息数
    private const string LogFilePath = "multiclient.txt";

    static volatile bool _running = true;
    static StreamWriter? _logFile;

    /// <summary>单个客户端的状态</summary>
    sealed class ClientState
    {
        public int Index;
        public Connector? Connector;
        public bool Connected;
        public bool Closed;
        public int ErrorCount;
        public int RecvCount;
        public long RecvBytes;
        public readonly List<string> SentMessages = new();
        public readonly List<string> RecvMessages = new();
        public readonly object Lock = new();
    }

    static void Main(string[] args)
    {
        Console.OutputEncoding = Encoding.UTF8;
        _logFile = new StreamWriter(LogFilePath, false, new UTF8Encoding(false)) { AutoFlush = true };

        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║     FxNet 多客户端并发测试           ║");
        LogRaw("╚══════════════════════════════════════╝");
        LogRaw($"[配置] 服务器: {ServerIp}:{ServerPort}");
        LogRaw($"[配置] 客户端数: {ClientCount}");
        LogRaw($"[配置] 每客户端消息数: {MsgsPerClient}");
        LogRaw("");

        FxNetInterface.StartLogModule();
        FxNetInterface.StartIOModule();

        Console.CancelKeyPress += (_, e) =>
        {
            e.Cancel = true;
            _running = false;
        };

        // === 测试1: 多客户端并发连接 ===
        Log("\n═══════ 测试1: 多客户端并发连接 ═══════");
        var clients = CreateAndConnectAll();
        int connectedCount = clients.Count(c => c.Connected);
        Log($"  连接结果: {connectedCount}/{ClientCount} 成功");

        if (connectedCount == 0)
        {
            Log("[错误] 无客户端成功连接，退出");
            Cleanup(clients);
            _logFile?.Close();
            return;
        }

        // === 测试2: 各客户端独立收发 ===
        Log("\n═══════ 测试2: 各客户端独立收发 ═══════");
        SendAndVerify(clients);

        // === 测试3: 并发同时发送 ===
        Log("\n═══════ 测试3: 并发同时发送 ═══════");
        ConcurrentSendTest(clients);

        // === 测试4: 客户端逐个断开 ===
        Log("\n═══════ 测试4: 客户端逐个断开 ═══════");
        SequentialDisconnectTest(clients);

        // === 测试5: 重新连接 ===
        Log("\n═══════ 测试5: 断开后重新连接 ═══════");
        ReconnectTest();

        // === 总结 ===
        Log("\n═══════ 测试完成 ═══════");
        PrintSummary(clients);

        Cleanup(clients);
        _logFile?.Close();
    }

    // ======================== 测试实现 ========================

    /// <summary>创建多个客户端并并发连接</summary>
    static List<ClientState> CreateAndConnectAll()
    {
        var clients = new List<ClientState>();
        for (int i = 0; i < ClientCount; i++)
        {
            var state = new ClientState { Index = i };
            state.Connector = FxNetApi.CreateConnector(
                onRecv: (conn, data, len) => OnClientRecv(state, data, len),
                onConnected: _ => { state.Connected = true; },
                onError: (_, _) => Interlocked.Increment(ref state.ErrorCount),
                onClose: _ => { state.Closed = true; });
            clients.Add(state);
        }

        // 并发发起连接
        foreach (var c in clients)
        {
            FxNetApi.TcpConnect(c.Connector!, ServerIp, ServerPort);
        }

        // 等待所有连接建立
        double timeout = GetNow() + 10.0;
        while (GetNow() < timeout && _running)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            int allConnected = clients.Count(c => c.Connected);
            if (allConnected >= ClientCount) break;
            Thread.Sleep(10);
        }

        return clients;
    }

    /// <summary>每个客户端独立发送带标识的消息并验证回显</summary>
    static void SendAndVerify(List<ClientState> clients)
    {
        // 每个客户端依次发送
        foreach (var client in clients.Where(c => c.Connected))
        {
            for (int i = 0; i < MsgsPerClient && _running; i++)
            {
                string msg = $"C{client.Index}-M{i:D04}";
                lock (client.Lock) client.SentMessages.Add(msg);
                SendToClient(client, msg);
                Thread.Sleep(3);
            }
        }

        // 等待回显
        double timeout = GetNow() + 20.0;
        while (GetNow() < timeout && _running)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();

            bool allDone = clients.Where(c => c.Connected)
                .All(c => { lock (c.Lock) return c.RecvMessages.Count >= MsgsPerClient; });
            if (allDone) break;
            Thread.Sleep(10);
        }

        // 验证每个客户端的回显
        int totalPass = 0, totalFail = 0;
        foreach (var client in clients.Where(c => c.Connected))
        {
            int pass = 0, fail = 0;
            lock (client.Lock)
            {
                var recvSet = new HashSet<string>(client.RecvMessages);
                foreach (var sent in client.SentMessages)
                {
                    if (recvSet.Contains(sent))
                        pass++;
                    else
                        fail++;
                }
            }
            totalPass += pass;
            totalFail += fail;
            Log($"  客户端 #{client.Index}: 发送 {client.SentMessages.Count}, " +
                $"收到 {client.RecvMessages.Count}, 匹配 {pass}, 不匹配 {fail}");
        }
        Log($"  汇总: 匹配 {totalPass}, 不匹配 {totalFail}");
    }

    /// <summary>所有客户端同时发送，测试并发</summary>
    static void ConcurrentSendTest(List<ClientState> clients)
    {
        var activeClients = clients.Where(c => c.Connected).ToList();
        if (activeClients.Count == 0) return;

        // 清空接收缓冲
        foreach (var c in activeClients)
            lock (c.Lock) c.RecvMessages.Clear();

        int msgsPerRound = 10;
        var sw = System.Diagnostics.Stopwatch.StartNew();

        // 交替发送：每轮每个客户端发一条
        for (int round = 0; round < msgsPerRound && _running; round++)
        {
            foreach (var client in activeClients)
            {
                string msg = $"CONC-C{client.Index}-R{round:D04}";
                lock (client.Lock) client.SentMessages.Add(msg);
                SendToClient(client, msg);
            }
            Thread.Sleep(5);
        }

        // 等待回显
        double timeout = GetNow() + 15.0;
        while (GetNow() < timeout && _running)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();

            bool allDone = activeClients.All(c =>
            {
                lock (c.Lock) return c.RecvMessages.Count >= msgsPerRound;
            });
            if (allDone) break;
            Thread.Sleep(10);
        }
        sw.Stop();

        int totalRecv = 0;
        foreach (var c in activeClients)
        {
            int recvCount;
            lock (c.Lock) recvCount = c.RecvMessages.Count;
            totalRecv += recvCount;
        }
        int expected = activeClients.Count * msgsPerRound;
        double throughput = expected / (sw.ElapsedMilliseconds / 1000.0);

        Log($"  并发发送: {activeClients.Count} 客户端 × {msgsPerRound} 条 = {expected} 条");
        Log($"  收到回显: {totalRecv} 条, 耗时 {sw.ElapsedMilliseconds}ms");
        Log($"  吞吐量: {throughput:F0} msg/s");
    }

    /// <summary>逐个断开客户端，验证断开流程正常</summary>
    static void SequentialDisconnectTest(List<ClientState> clients)
    {
        foreach (var client in clients.Where(c => c.Connected))
        {
            Log($"  断开客户端 #{client.Index}...");
            client.Connector!.Close();

            // 等待关闭回调
            for (int i = 0; i < 50 && !client.Closed; i++)
            {
#if SINGLE_THREAD
                FxNetInterface.ProcSingleThread();
#endif
                FxNetInterface.ProcessMessageEvents();
                Thread.Sleep(10);
            }

            Log($"  客户端 #{client.Index}: 已关闭={client.Closed}");
        }
    }

    /// <summary>断开后重新连接测试</summary>
    static void ReconnectTest()
    {
        var newClients = CreateAndConnectAll();
        int reconnected = newClients.Count(c => c.Connected);
        Log($"  重连结果: {reconnected}/{ClientCount} 成功");

        // 快速发送验证
        foreach (var client in newClients.Where(c => c.Connected))
        {
            SendToClient(client, $"RECONNECT-C{client.Index}");
        }

        // 等待回显
        double timeout = GetNow() + 5.0;
        while (GetNow() < timeout && _running)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            bool allRecv = newClients.Where(c => c.Connected)
                .All(c => { lock (c.Lock) return c.RecvMessages.Count > 0; });
            if (allRecv) break;
            Thread.Sleep(10);
        }

        foreach (var client in newClients.Where(c => c.Connected))
        {
            int recvCount;
            lock (client.Lock) recvCount = client.RecvMessages.Count;
            Log($"  重连客户端 #{client.Index}: 收到 {recvCount} 条回显");
        }

        Cleanup(newClients);
    }

    // ======================== 回调与工具 ========================

    static void OnClientRecv(ClientState state, byte[] data, int len)
    {
        Interlocked.Increment(ref state.RecvCount);
        Interlocked.Add(ref state.RecvBytes, len);
        string message = Encoding.UTF8.GetString(data, 0, len);
        lock (state.Lock) state.RecvMessages.Add(message);
    }

    static void SendToClient(ClientState client, string text)
    {
        if (client.Connector == null || !client.Connected) return;
        byte[] data = Encoding.UTF8.GetBytes(text);
        client.Connector.Send(data, data.Length);
    }

    static void Cleanup(List<ClientState> clients)
    {
        foreach (var c in clients)
        {
            if (!c.Closed)
                c.Connector?.Close();
        }
        // 等待清理完成
        for (int i = 0; i < 30; i++)
        {
#if SINGLE_THREAD
            FxNetInterface.ProcSingleThread();
#endif
            FxNetInterface.ProcessMessageEvents();
            Thread.Sleep(10);
        }
    }

    static void PrintSummary(List<ClientState> clients)
    {
        LogRaw("");
        LogRaw("╔══════════════════════════════════════╗");
        LogRaw("║         多客户端测试总结             ║");
        LogRaw("╚══════════════════════════════════════╝");
        foreach (var c in clients)
        {
            LogRaw($"  客户端 #{c.Index}: 连接={c.Connected} 关闭={c.Closed} " +
                   $"错误={c.ErrorCount} 收到={c.RecvCount} {FormatBytes(c.RecvBytes)}");
        }
    }

    static double GetNow() => TimeUtility.GetTimeSeconds();

    static string FormatBytes(long bytes)
    {
        if (bytes < 1024) return $"{bytes} B";
        if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
        return $"{bytes / (1024.0 * 1024.0):F1} MB";
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
}
