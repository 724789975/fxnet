#nullable enable
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Text;
using UnityEngine;

namespace FxNet.UdpTest
{
    /// <summary>运行角色</summary>
    public enum UdpRole
    {
        /// <summary>客户端（本地 Unity 客户端 / WSL headless 客户端）</summary>
        Client,
        /// <summary>服务器（远程 Linux dedicated server）</summary>
        Server,
    }

    /// <summary>
    /// UDP 测试运行配置。通过命令行参数解析，支持本地交互客户端、
    /// headless 自动化客户端、以及 Linux dedicated server 三种运行形态。
    ///
    /// 支持的命令行参数（对齐 deploy_test.sh 的使用习惯）：
    ///   -role   server|client   显式指定角色
    ///   -ip / -serverip  &lt;ip&gt; 服务器地址（客户端使用）
    ///   -port   &lt;port&gt;         端口（默认 9001）
    ///   -duration &lt;seconds&gt;    客户端自动测试最大运行秒数（默认 90）
    ///   -auto                    强制自动化测试模式（headless 客户端）
    ///   -interactive             强制交互模式（带 GUI 的窗口客户端）
    /// </summary>
    public sealed class UdpTestConfig
    {
        public UdpRole Role = UdpRole.Client;
        public string ServerIp = "115.190.230.47"; // 与 deploy_test.sh 默认服务器一致
        public string ListenIp = "0.0.0.0";
        public ushort Port = 9001;
        public double DurationSeconds = 90.0;
        public bool AutoTest;       // true=自动化测试并退出；false=交互式
        public bool ExplicitRole;   // 是否命令行显式指定了 role

        /// <summary>从进程命令行解析配置</summary>
        public static UdpTestConfig Parse()
        {
            var cfg = new UdpTestConfig();
            string[] args = Environment.GetCommandLineArgs();

            for (int i = 0; i < args.Length; i++)
            {
                string a = args[i].ToLowerInvariant();
                string Next() => (i + 1 < args.Length) ? args[++i] : "";

                switch (a)
                {
                    case "-role":
                        {
                            string v = Next().ToLowerInvariant();
                            cfg.Role = v == "server" ? UdpRole.Server : UdpRole.Client;
                            cfg.ExplicitRole = true;
                            break;
                        }
                    case "-ip":
                    case "-serverip":
                        cfg.ServerIp = Next();
                        break;
                    case "-listenip":
                        cfg.ListenIp = Next();
                        break;
                    case "-port":
                        if (ushort.TryParse(Next(), out var p) && p > 0) cfg.Port = p;
                        break;
                    case "-duration":
                        if (double.TryParse(Next(), NumberStyles.Float, CultureInfo.InvariantCulture, out var d) && d > 0)
                            cfg.DurationSeconds = d;
                        break;
                    case "-auto":
                        cfg.AutoTest = true;
                        break;
                    case "-interactive":
                        cfg.AutoTest = false;
                        break;
                }
            }

            // 未显式指定角色时的推断：
            // - Dedicated Server 构建（UNITY_SERVER）或 Linux 平台默认作服务器
            // - 其他（Windows 客户端）默认作客户端
            if (!cfg.ExplicitRole)
            {
#if UNITY_SERVER
                cfg.Role = UdpRole.Server;
#else
                cfg.Role = (Application.platform == RuntimePlatform.LinuxPlayer)
                    ? UdpRole.Server : UdpRole.Client;
#endif
            }

            // 客户端在 batchmode/无图形环境下强制走自动化测试（WSL headless）
            if (cfg.Role == UdpRole.Client && Application.isBatchMode)
                cfg.AutoTest = true;

            return cfg;
        }

        public override string ToString()
        {
            return Role == UdpRole.Server
                ? $"Role=Server Listen={ListenIp}:{Port}"
                : $"Role=Client Server={ServerIp}:{Port} Auto={AutoTest} Duration={DurationSeconds}s";
        }
    }

    /// <summary>
    /// 测试日志工具：同时写入 Unity 控制台（Debug.Log → Player.log / stdout）
    /// 和 UTF-8 文本日志文件，日志标记与 C# 控制台版本一致（[发送]/[接收]/[PASS]/[FAIL] 等），
    /// 以便 deploy_unity_test.sh 复用 deploy_test.sh 的日志校验逻辑。
    /// 另维护一个内存环形缓冲，供交互式 GUI 显示最近若干行。
    /// </summary>
    public sealed class TestLog : IDisposable
    {
        private readonly StreamWriter? _file;
        private readonly object _lock = new object();
        private readonly Queue<string> _ring = new Queue<string>();
        private readonly int _ringCapacity;

        public TestLog(string filePath, int ringCapacity = 200)
        {
            _ringCapacity = ringCapacity;
            try
            {
                _file = new StreamWriter(filePath, false, new UTF8Encoding(false)) { AutoFlush = true };
            }
            catch (Exception ex)
            {
                Debug.LogWarning($"[TestLog] 无法创建日志文件 {filePath}: {ex.Message}");
                _file = null;
            }
        }

        /// <summary>带时间戳的普通日志</summary>
        public void Log(string message)
        {
            string line = $"{DateTime.Now:HH:mm:ss.fff} {message}";
            Write(line);
        }

        /// <summary>不带时间戳的原始日志（用于表格/分隔线/最终判定）</summary>
        public void Raw(string message) => Write(message);

        private void Write(string line)
        {
            lock (_lock)
            {
                Debug.Log(line);
                _file?.WriteLine(line);
                _ring.Enqueue(line);
                while (_ring.Count > _ringCapacity) _ring.Dequeue();
            }
        }

        /// <summary>获取内存环形缓冲快照（供 GUI 显示）</summary>
        public string[] Snapshot()
        {
            lock (_lock) { return _ring.ToArray(); }
        }

        public void Dispose()
        {
            lock (_lock)
            {
                _file?.Flush();
                _file?.Dispose();
            }
        }
    }
}
