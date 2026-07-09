using System.Runtime.CompilerServices;
using System.Text;

namespace FxNet.Util
{
    /// <summary>
    /// 日志级别枚举，支持位运算组合（多级日志同时输出）
    /// </summary>
    public enum LogLevel
    {
        Error   = 1,
        Warn    = 1 << 1,
        Info    = 1 << 2,
        Debug   = 1 << 3,
        Debug1  = 1 << 4,
        Debug2  = 1 << 5,
        Debug3  = 1 << 6,
        Debug4  = 1 << 7,
    }

    /// <summary>
    /// 日志工具类，提供分级日志功能。
    /// 支持输出到 StringBuilder 或 TextWriter，附带文件/行号/方法名等调用信息。
    /// </summary>
    public static class LogUtility
    {
        private static LogLevel _logLevel = LogLevel.Info; // 当前日志级别

        public static LogLevel GetLogLevel() => _logLevel;
        public static void SetLogLevel(LogLevel level) => _logLevel = level;

        public static void Log(
            StringBuilder? stream,
            LogLevel level,
            string message,
            [CallerFilePath] string file = "",
            [CallerLineNumber] int line = 0,
            [CallerMemberName] string member = "")
        {
            if (stream == null) return;
            if ((_logLevel & level) == 0) return;

            stream.AppendLine($"[{level}]\t[{FxNetInterface.GetNow():F6}]\t [{file}:{line}, {member}]\t{message}");
        }

        public static void Log(
            TextWriter? stream,
            LogLevel level,
            string message,
            [CallerFilePath] string file = "",
            [CallerLineNumber] int line = 0,
            [CallerMemberName] string member = "")
        {
            if (stream == null) return;
            if ((_logLevel & level) == 0) return;

            stream.WriteLine($"[{level}]\t[{FxNetInterface.GetNow():F6}]\t [{file}:{line}, {member}]\t{message}");
        }
    }

    /// <summary>
    /// 日志模块单例，后台线程定时输出累积日志。
    /// 避免日志 IO 阻塞业务线程。
    /// </summary>
    public class LogModule : Singleton<LogModule>
    {
        private Thread? _thread;
        private volatile bool _stop;
        private readonly CasLock _lock = new CasLock();
        private readonly StringBuilder _stream = new StringBuilder();
        private readonly StringBuilder _logBuffer = new StringBuilder();

        public void Init()
        {
            _stop = false;
            _thread = new Thread(ThreadFunc) { IsBackground = true, Name = "LogModule" };
            _thread.Start();
        }

        public void Uninit()
        {
            _stop = true;
            _thread?.Join(3000);
            _thread = null;
        }

        public void PushLog(StringBuilder stream)
        {
            if (stream.Length == 0) return;
            using (new LockScope(_lock))
            {
                _stream.Append(stream);
            }
            stream.Clear();
        }

        public string GetLogStr()
        {
            using (new LockScope(_lock))
            {
                var result = _stream.ToString();
                _stream.Clear();
                return result;
            }
        }

        private void ThreadFunc()
        {
            while (!_stop)
            {
                string log = GetLogStr();
                if (!string.IsNullOrEmpty(log))
                {
                    Console.Write(log);
                }
                Thread.Sleep(100);
            }
        }
    }
}
