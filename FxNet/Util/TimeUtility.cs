using System.Diagnostics;

namespace FxNet.Util
{
    /// <summary>
    /// 时间工具类，提供基于 UTC 时间的高精度时间戳。
    /// 替代 C++ 原生的 Windows FILETIME 实现。
    /// </summary>
    public static class TimeUtility
    {
        // Unix 纪元时间的 Tick 数（1970-01-01 00:00:00 UTC）
        private static readonly long _epochTicks = new DateTime(1970, 1, 1, 0, 0, 0, DateTimeKind.Utc).Ticks;

        /// <summary>获取当前 UTC 时间戳（微秒）</summary>
        public static ulong GetTimeUS()
        {
            return (ulong)((DateTime.UtcNow.Ticks - _epochTicks) / 10);
        }

        /// <summary>获取当前 UTC 时间戳（毫秒）</summary>
        public static ulong GetTimeMS()
        {
            return GetTimeUS() / 1000;
        }

        /// <summary>获取当前 UTC 时间戳（秒）</summary>
        public static ulong GetTime()
        {
            return GetTimeMS() / 1000;
        }

        /// <summary>获取当前 UTC 时间戳（秒，浮点精度）</summary>
        public static double GetTimeSeconds()
        {
            return GetTimeUS() / 1_000_000.0;
        }
    }
}
