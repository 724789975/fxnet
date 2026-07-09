namespace FxNet.Util
{
    /// <summary>
    /// 线程接口，实现类需提供线程主循环和停止方法
    /// </summary>
    public interface IFxThread
    {
        void ThreadFunc();
        void Stop();
    }

    /// <summary>
    /// 线程封装类，包装 .NET Thread，提供启动/停止/状态查询。
    /// 后台线程模式（IsBackground = true），不阻止进程退出。
    /// </summary>
    public class FxThread
    {
        private Thread? _thread;
        private volatile bool _stop;
        private readonly IFxThread _owner;

        public FxThread(IFxThread owner)
        {
            _owner = owner;
        }

        public bool Start()
        {
            _stop = false;
            _thread = new Thread(() =>
            {
                _owner.ThreadFunc();
            })
            {
                IsBackground = true
            };
            _thread.Start();
            return true;
        }

        public void Stop()
        {
            _stop = true;
            _thread?.Join(3000);
            _thread = null;
        }

        public bool IsStop => _stop;
        public uint ThreadId => _thread != null ? (uint)_thread.ManagedThreadId : 0;
    }
}
