using System.Threading;

namespace FxNet.Util
{
    /// <summary>
    /// CAS 自旋锁，基于 Interlocked.CompareExchange 实现无锁同步。
    /// 适用于短时间持锁、低竞争场景。
    /// </summary>
    public class CasLock
    {
        private volatile int _lock; // 0 = 未锁定, 1 = 已锁定

        public CasLock()
        {
            _lock = 0;
        }

        /// <summary>自旋等待获取锁</summary>
        public CasLock Lock()
        {
            while (Interlocked.CompareExchange(ref _lock, 1, 0) != 0)
            {
                Thread.SpinWait(1);
            }
            return this;
        }

        /// <summary>释放锁</summary>
        public CasLock Unlock()
        {
            Interlocked.Exchange(ref _lock, 0);
            return this;
        }
    }

    /// <summary>
    /// 锁作用域封装（IDisposable 模式），替代 C++ RAII 析构自动解锁。
    /// 用法：using (new LockScope(casLock)) { ... }
    /// </summary>
    public sealed class LockScope : IDisposable
    {
        private readonly CasLock _lock;

        public LockScope(CasLock casLock)
        {
            _lock = casLock;
            _lock.Lock();
        }

        public void Dispose()
        {
            _lock.Unlock();
        }
    }
}
