#nullable enable
using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using UnityEngine;
using FxNet.Dll;

namespace FxNet.UdpTest
{
    /// <summary>
    /// UDP 测试客户端（Unity）。对齐 FxNet.UdpClient 控制台项目的端到端验证流程，
    /// 但以 Unity 协程驱动（不阻塞主线程），网络回调在主线程 Update 中经由
    /// ProcessMessageEvents 执行。
    ///
    /// 两种运行形态：
    /// - 自动化（headless / -auto，WSL dedicated server 形式）：运行全部测试阶段，
    ///   写 udp_client.txt（日志标记与控制台版一致），并以退出码 PASS(0)/FAIL(1) 结束进程；
    /// - 交互式（本地 Windows 客户端）：提供 GUI 手动发送与查看日志，也可点击按钮运行自动化测试。
    /// </summary>
    public sealed class UdpTestClient : MonoBehaviour
    {
        private const string LogFilePath = "udp_client.txt";

        private UdpTestConfig _cfg = new UdpTestConfig();
        private TestLog? _log;

        private bool _running = true;
        private bool _connected;
        private Connector? _connector;
        private bool _testRunning;
        private bool _testFinished;

        // === 统计 ===
        private long _totalSent, _totalRecv;
        private long _totalBytesSent, _totalBytesRecv;

        // === 验证 ===
        private int _passCount, _failCount;
        private bool _waitingResponse;
        private byte[]? _lastRecvData;
        private int _lastRecvLen;

        // === 延迟测量 ===
        private double _pingSendTime;
        private bool _waitingPong;

        // === 多客户端 & 顺序验证 ===
        private readonly List<Connector> _extraConnectors = new();
        private readonly List<int> _recvOrder = new();

        // === GUI ===
        private string _guiInput = "Hello, UDP Server!";
        private Vector2 _scroll;

        public void Configure(UdpTestConfig cfg) => _cfg = cfg;

        private void Start()
        {
            Application.targetFrameRate = 120;
            QualitySettings.vSyncCount = 0;

            _log = new TestLog(LogFilePath);
            _log.Raw("╔══════════════════════════════════════╗");
            _log.Raw("║     FxNet UDP 测试客户端 (Unity)     ║");
            _log.Raw("╚══════════════════════════════════════╝");
            _log.Raw($"[配置] 服务器: {_cfg.ServerIp}:{_cfg.Port}");
            _log.Raw($"[配置] 模式: {(_cfg.AutoTest ? "自动化测试" : "交互式")}");

            FxNetInterface.StartLogModule();
            FxNetInterface.StartIOModule();

            _connector = FxNetApi.CreateConnector(OnServerRecv, OnConnected, OnError, OnClosed);
            _log.Log($"[连接] 正在连接 {_cfg.ServerIp}:{_cfg.Port} (UDP)...");
            FxNetApi.UdpConnect(_connector, _cfg.ServerIp, _cfg.Port);
            _connected = true;

            if (_cfg.AutoTest)
                StartCoroutine(RunTests());
        }

        private void Update()
        {
            // 抽取 IO 线程投递的消息事件（OnRecv/OnConnected 等回调在此主线程执行）
            FxNetInterface.ProcessMessageEvents();
        }

        private void OnDestroy()
        {
            _running = false;
            try { _connector?.Close(); } catch { }
            foreach (var c in _extraConnectors) { try { c.Close(); } catch { } }
            _log?.Dispose();
        }

        // ======================== 测试主流程 ========================

        private IEnumerator RunTests()
        {
            _testRunning = true;
            double startTime = Now();
            bool TimeExpired() => Now() - startTime >= _cfg.DurationSeconds;

            // 确保 UDP socket 已建立并完成握手
            yield return Pump(0.5);
            SendText("UDP-CONNECT");
            yield return Pump(0.5);

            _log?.Raw("\n═══════ 阶段1: 基础消息回显验证 ═══════");
            yield return BasicEchoTest();
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段2: 延迟测量 ═══════");
            yield return MeasureLatency(5);
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段3: 批量发送吞吐量测试 ═══════");
            yield return BatchSendTest(590);
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段3.5: 收发顺序验证 ═══════");
            yield return OrderVerificationTest(2000);
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段4: 服务器命令交互 ═══════");
            yield return ServerCommandTest();
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段5: 回显模式验证 ═══════");
            yield return EchoModeTest();
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段6: 大数据传输测试 ═══════");
            yield return LargeDataTest();
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段7: 多客户端并发 ═══════");
            yield return MultiClientTest();
            if (TimeExpired()) { Finish(); yield break; }

            _log?.Raw("\n═══════ 阶段8: 错误处理 ═══════");
            yield return ErrorHandlingTest();

            _log?.Raw("\n═══════ 所有测试阶段完成 ═══════");
            Finish();
        }

        private void Finish()
        {
            _testRunning = false;
            _testFinished = true;
            PrintFinalStats();
            int pass = _passCount, fail = _failCount;
            _log?.Raw("");
            _log?.Raw((fail == 0 && pass > 0) ? "=== 验证结果: PASS ===" : "=== 验证结果: FAIL ===");
            _log?.Log("[关闭] UDP 客户端已退出");

            _connector?.Close();
            foreach (var c in _extraConnectors) { try { c.Close(); } catch { } }

            // headless 自动化：以退出码结束进程供部署脚本判定
            if (_cfg.AutoTest && Application.isBatchMode)
                Application.Quit((fail == 0 && pass > 0) ? 0 : 1);
        }

        // ======================== 阶段1: 基础消息回显 ========================

        private IEnumerator BasicEchoTest()
        {
            yield return SendAndVerify("Hello, UDP Server!", "Hello, UDP Server!");
            yield return SendAndVerify("你好世界！UDP 中文测试", "你好世界！UDP 中文测试");
            yield return SendAndVerify("NUM:42", "NUM:42");
            string json = "{\"type\":\"udp\",\"id\":1,\"data\":\"hello\"}";
            yield return SendAndVerify(json, json);
            string special = "Special: !@#$%^&*()_+-=";
            yield return SendAndVerify(special, special);
            yield return SendAndVerify("", "");
            string repeated = new string('U', 200);
            yield return SendAndVerify(repeated, repeated);
            yield return SendAndVerify("Line1\nLine2", "Line1\nLine2");
            yield return SendAndVerify("你好🎉🚀 emoji: 😀👍", "你好🎉🚀 emoji: 😀👍");
            yield return SendAndVerify("Col1\tCol2\r\nCol3  Space", "Col1\tCol2\r\nCol3  Space");
        }

        // ======================== 阶段2: 延迟测量 ========================

        private IEnumerator MeasureLatency(int count)
        {
            double totalLatency = 0;
            int successCount = 0;

            for (int i = 0; i < count && _connected && _running; i++)
            {
                _pingSendTime = Now();
                _waitingPong = true;
                SendText($"PING-{i + 1}");

                double waitStart = Now();
                while (_waitingPong && _connected && Now() - waitStart < 3.0)
                    yield return null;

                if (!_waitingPong)
                {
                    double latency = (Now() - _pingSendTime) * 1000;
                    totalLatency += latency;
                    successCount++;
                    RecordPass($"PING-{i + 1} 延迟 {latency:F2}ms");
                }
                else
                {
                    RecordFail($"PING-{i + 1} 超时");
                }
                yield return Pump(0.05);
            }

            if (successCount > 0)
                _log?.Log($"  [延迟] 平均: {totalLatency / successCount:F2} ms ({successCount}/{count} 成功)");
        }

        // ======================== 阶段3: 批量发送吞吐量 ========================

        private IEnumerator BatchSendTest(int count)
        {
            _log?.Log($"  批量发送 {count} 条消息...");
            long startRecv = _totalRecv;
            double startTime = Now();

            int batchSize = 50;
            for (int i = 0; i < count && _connected && _running; i++)
            {
                SendText($"udp-batch-{i + 1}");
                if ((i + 1) % batchSize == 0)
                    yield return Pump(0.01);
            }

            double sendTime = Now() - startTime;
            if (sendTime > 0)
                _log?.Log($"  发送完成，耗时: {sendTime * 1000:F1} ms，速率: {count / sendTime:F0} msg/s");

            int expected = count;
            double waitEnd = Now() + 30.0;
            while (_totalRecv - startRecv < expected && _connected && Now() < waitEnd)
                yield return null;

            long batchRecv = _totalRecv - startRecv;
            double lossRate = (1.0 - (double)batchRecv / expected) * 100;
            _log?.Log($"  批量回显: 收到 {batchRecv}/{expected} (丢包率 {lossRate:F1}%)");
            if (batchRecv > 0)
                RecordPass($"批量发送 {count} 条回显 {batchRecv} 条 (丢包 {lossRate:F1}%)");
            else
                RecordFail($"批量发送 {count} 条无回显");
        }

        // ======================== 阶段3.5: 收发顺序验证 ========================

        private IEnumerator OrderVerificationTest(int count)
        {
            const int MinLen = 100, MaxLen = 1024;
            var rng = new System.Random(42);
            _recvOrder.Clear();
            long totalBytes = 0;

            _log?.Log($"  计划发送 {count} 条 (长度 {MinLen}-{MaxLen} 随机)");

            for (int i = 0; i < count && _connected && _running; i++)
            {
                int msgLen = rng.Next(MinLen, MaxLen + 1);
                byte[] sendData = new byte[msgLen];
                byte[] header = Encoding.UTF8.GetBytes($"SEQ-{i:D04}");
                Array.Copy(header, sendData, header.Length);

                _connector?.Send(sendData, sendData.Length);
                _totalSent++;
                _totalBytesSent += sendData.Length;
                totalBytes += sendData.Length;

                if ((i + 1) % 100 == 0) yield return null;
            }

            _log?.Log($"  已发送 {count} 条 (共 {totalBytes / 1024} KB)，等待回显...");

            double waitEnd = Now() + 120.0;
            while (_recvOrder.Count < count && _connected && _running && Now() < waitEnd)
                yield return null;

            bool orderCorrect = true;
            for (int i = 1; i < _recvOrder.Count; i++)
            {
                if (_recvOrder[i] <= _recvOrder[i - 1])
                {
                    orderCorrect = false;
                    _log?.Log($"  [顺序异常] 位置 {i}: SEQ-{_recvOrder[i - 1]:D04} → SEQ-{_recvOrder[i]:D04}");
                    break;
                }
            }

            _log?.Log($"  接收: {_recvOrder.Count}/{count}");
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

        private IEnumerator ServerCommandTest()
        {
            yield return SendCommandAndVerify("CMD:CLIENT_COUNT", "活跃客户端");
            yield return SendCommandAndVerify("CMD:STATS", "消息");
            yield return SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");
            yield return SendCommandAndVerify("CMD:UNKNOWN_CMD", "未知命令");
        }

        // ======================== 阶段5: 回显模式验证 ========================

        private IEnumerator EchoModeTest()
        {
            // 阶段4已切换一次 → 当前应为大写模式
            yield return SendAndVerify("hello uppercase", "HELLO UPPERCASE");

            yield return SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");
            yield return Pump(0.3);
            yield return SendAndVerify("abcdef", "fedcba");

            yield return SendCommandAndVerify("CMD:SWITCH_MODE", "回显模式");
            yield return Pump(0.3);
            yield return SendAndVerify("original test", "original test");
        }

        // ======================== 阶段6: 大数据传输 ========================

        private IEnumerator LargeDataTest()
        {
            yield return SendAndVerify(new string('A', 1024), new string('A', 1024), "1KB");
            yield return SendAndVerify(new string('C', 8 * 1024), new string('C', 8 * 1024), "8KB");
            yield return SendAndVerify(new string('B', 10 * 1024), new string('B', 10 * 1024), "10KB");
        }

        // ======================== 阶段7: 多客户端并发 ========================

        private IEnumerator MultiClientTest()
        {
            const int clientCount = 3;
            var results = new string?[clientCount];

            for (int i = 0; i < clientCount; i++)
            {
                int idx = i;
                var c = FxNetApi.CreateConnector(
                    onRecv: (conn, data, len) => { results[idx] = Encoding.UTF8.GetString(data, 0, len); },
                    onConnected: _ => { },
                    onError: (_, _) => { },
                    onClose: _ => { });

                FxNetApi.UdpConnect(c, _cfg.ServerIp, _cfg.Port);
                _extraConnectors.Add(c);

                string msg = $"multi-client-{idx + 1}";
                byte[] sendData = Encoding.UTF8.GetBytes(msg);
                c.Send(sendData, sendData.Length);
                _totalSent++;
                _totalBytesSent += sendData.Length;
                _log?.Log($"  [多客户端] 客户端 {idx + 1} 发送: \"{msg}\"");
                yield return Pump(0.05);
            }

            double waitEnd = Now() + 15.0;
            while (Now() < waitEnd)
            {
                bool allDone = true;
                for (int i = 0; i < clientCount; i++)
                    if (results[i] == null) allDone = false;
                if (allDone) break;
                yield return null;
            }

            for (int i = 0; i < clientCount; i++)
            {
                string expected = $"multi-client-{i + 1}";
                if (results[i] == expected)
                    RecordPass($"多客户端 {i + 1} 回显正确");
                else
                    RecordFail($"多客户端 {i + 1} 期望 \"{expected}\" 实际 \"{results[i] ?? "null"}\"");
            }

            foreach (var c in _extraConnectors) { try { c.Close(); } catch { } }
            _extraConnectors.Clear();
        }

        // ======================== 阶段8: 错误处理 ========================

        private IEnumerator ErrorHandlingTest()
        {
            var testConnector = FxNetApi.CreateConnector(
                onRecv: (_, _, _) => { }, onConnected: _ => { }, onError: (_, _) => { }, onClose: _ => { });
            FxNetApi.UdpConnect(testConnector, _cfg.ServerIp, _cfg.Port);
            yield return Pump(0.2);

            testConnector.Close();
            yield return Pump(0.2);

            byte[] data = Encoding.UTF8.GetBytes("after-close-test");
            Exception? caught = null;
            try { testConnector.Send(data, data.Length); } catch (Exception ex) { caught = ex; }
            if (caught == null) RecordPass("关闭后发送不崩溃");
            else RecordFail($"关闭后发送异常: {caught.Message}");
        }

        // ======================== 核心验证方法 ========================

        private IEnumerator SendAndVerify(string message, string expectedEcho, string? label = null)
        {
            if (!_connected || !_running) yield break;

            string tag = label ?? Truncate(message, 30);
            _waitingResponse = true;
            _lastRecvData = null;
            _lastRecvLen = 0;

            byte[] sendData = Encoding.UTF8.GetBytes(message);
            _connector?.Send(sendData, sendData.Length);
            _totalSent++;
            _totalBytesSent += sendData.Length;
            _log?.Log($"  [发送] {sendData.Length} 字节 | \"{tag}\"");

            double waitEnd = Now() + 10.0;
            while (_waitingResponse && _connected && _running && Now() < waitEnd)
                yield return null;

            if (_lastRecvData != null && _lastRecvLen > 0)
            {
                string actual = Encoding.UTF8.GetString(_lastRecvData, 0, _lastRecvLen);
                if (actual == expectedEcho) RecordPass($"回显正确: \"{tag}\"");
                else RecordFail($"回显不匹配: \"{tag}\" 期望 \"{Truncate(expectedEcho, 40)}\" 实际 \"{Truncate(actual, 40)}\"");
            }
            else if (_waitingResponse)
            {
                RecordFail($"回显超时: \"{tag}\"");
            }
        }

        private IEnumerator SendCommandAndVerify(string command, string expectedKeyword)
        {
            if (!_connected || !_running) yield break;

            _waitingResponse = true;
            _lastRecvData = null;
            _lastRecvLen = 0;

            byte[] sendData = Encoding.UTF8.GetBytes(command);
            _connector?.Send(sendData, sendData.Length);
            _totalSent++;
            _totalBytesSent += sendData.Length;
            _log?.Log($"  [命令] {command}");

            double waitEnd = Now() + 10.0;
            while (_waitingResponse && _connected && _running && Now() < waitEnd)
                yield return null;

            if (_lastRecvData != null && _lastRecvLen > 0)
            {
                string actual = Encoding.UTF8.GetString(_lastRecvData, 0, _lastRecvLen);
                if (actual.Contains(expectedKeyword)) RecordPass($"命令响应正确: {command} → \"{Truncate(actual, 50)}\"");
                else RecordFail($"命令响应缺少关键字: {command} 期望含 \"{expectedKeyword}\" 实际 \"{Truncate(actual, 50)}\"");
            }
            else
            {
                RecordFail($"命令响应超时: {command}");
            }
            yield return Pump(0.2);
        }

        private void RecordPass(string detail) { _passCount++; _log?.Log($"  [PASS] {detail}"); }
        private void RecordFail(string detail) { _failCount++; _log?.Log($"  [FAIL] {detail}"); }

        // ======================== 回调 ========================

        private void OnConnected(Connector connector)
        {
            _connected = true;
            _log?.Log("[连接] 已连接到 UDP 服务器");
        }

        private void OnServerRecv(Connector connector, byte[] data, int len)
        {
            _totalRecv++;
            _totalBytesRecv += len;

            // 顺序验证：解析 SEQ- 前缀
            if (len >= 8 && data[0] == (byte)'S' && data[1] == (byte)'E' &&
                data[2] == (byte)'Q' && data[3] == (byte)'-')
            {
                int seq = 0;
                for (int j = 4; j < 8 && j < len; j++)
                    if (data[j] >= (byte)'0' && data[j] <= (byte)'9')
                        seq = seq * 10 + (data[j] - (byte)'0');
                _recvOrder.Add(seq);
            }

            string message = Encoding.UTF8.GetString(data, 0, len);

            if (_waitingPong && message.StartsWith("PING-")) _waitingPong = false;

            if (_waitingResponse)
            {
                _lastRecvData = new byte[len];
                Array.Copy(data, _lastRecvData, len);
                _lastRecvLen = len;
                _waitingResponse = false;
            }

            string display = message.Length > 60 ? message[..57] + "..." : message;
            _log?.Log($"  [接收] {len} 字节 | \"{display}\"");
        }

        private void OnError(Connector connector, int error)
        {
            _log?.Log($"[错误] 错误码: {error}");
            _connected = false;
        }

        private void OnClosed(Connector connector)
        {
            _log?.Log("[连接] 连接已关闭");
            _connected = false;
        }

        // ======================== 工具 ========================

        private void SendText(string text)
        {
            if (_connector == null || !_connected) return;
            byte[] data = Encoding.UTF8.GetBytes(text);
            _connector.Send(data, data.Length);
            _totalSent++;
            _totalBytesSent += data.Length;
            string display = text.Length > 50 ? text[..47] + "..." : text;
            _log?.Log($"  [发送] {data.Length} 字节 | \"{display}\"");
        }

        /// <summary>等待若干秒（期间由 Update 持续抽取消息事件）</summary>
        private IEnumerator Pump(double seconds)
        {
            double end = Now() + seconds;
            while (_running && Now() < end) yield return null;
        }

        private void PrintFinalStats()
        {
            _log?.Raw("");
            _log?.Raw("═══ UDP 客户端最终统计 ═══");
            _log?.Raw($"  发送消息数: {_totalSent}");
            _log?.Raw($"  接收消息数: {_totalRecv}");
            _log?.Raw($"  发送字节数: {FormatBytes(_totalBytesSent)}");
            _log?.Raw($"  接收字节数: {FormatBytes(_totalBytesRecv)}");
            _log?.Raw($"  验证通过: {_passCount}");
            _log?.Raw($"  验证失败: {_failCount}");
        }

        private static double Now() => FxNetInterface.GetNow();

        private static string FormatBytes(long bytes)
        {
            if (bytes < 1024) return $"{bytes} B";
            if (bytes < 1024 * 1024) return $"{bytes / 1024.0:F1} KB";
            return $"{bytes / (1024.0 * 1024.0):F1} MB";
        }

        private static string Truncate(string s, int maxLen) => s.Length <= maxLen ? s : s[..maxLen] + "...";

        // ======================== 交互式 GUI（本地 Windows 客户端） ========================

        private void OnGUI()
        {
            if (_cfg.AutoTest) return; // headless 不绘制

            const int pad = 10;
            GUILayout.BeginArea(new Rect(pad, pad, Screen.width - pad * 2, Screen.height - pad * 2));

            GUILayout.Label($"<b>FxNet UDP 测试客户端</b>  服务器: {_cfg.ServerIp}:{_cfg.Port}  " +
                            $"状态: {(_connected ? "已连接" : "未连接")}");
            GUILayout.Label($"发送: {_totalSent}  接收: {_totalRecv}  PASS: {_passCount}  FAIL: {_failCount}");

            GUILayout.BeginHorizontal();
            _guiInput = GUILayout.TextField(_guiInput, GUILayout.Width(Screen.width - 260));
            if (GUILayout.Button("发送", GUILayout.Width(80)))
                SendText(_guiInput);
            if (GUILayout.Button(_testRunning ? "测试中..." : "运行自动化测试", GUILayout.Width(140)) && !_testRunning && !_testFinished)
                StartCoroutine(RunTests());
            GUILayout.EndHorizontal();

            GUILayout.Label("日志:");
            _scroll = GUILayout.BeginScrollView(_scroll, GUI.skin.box);
            string[] lines = _log?.Snapshot() ?? Array.Empty<string>();
            for (int i = 0; i < lines.Length; i++)
                GUILayout.Label(lines[i]);
            GUILayout.EndScrollView();

            GUILayout.EndArea();
        }
    }
}
