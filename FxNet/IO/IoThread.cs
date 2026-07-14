using FxNet.Core;
using FxNet.Util;
using System.Net.Sockets;

namespace FxNet.IO
{
    /// <summary>
    /// IO 模块核心类，负责：
    /// 1. 管理所有注册的 Socket（注册/注销/遍历更新）
    /// 2. 周期性调用 DealFunction 更新 Socket 状态（UDP 可靠传输定时重传等）
    /// 3. 处理消息事件队列（从全局队列和本地 PostEvent 队列中取出事件并执行）
    /// 4. 在独立后台线程中运行 IO 循环
    /// </summary>
    public class IoModule : IFxThread, IDisposable
    {
        private const int MaxEventNum = 256;

        private FxThread? _thread;        // 后台 IO 线程
        private volatile bool _stop;      // 停止标志
        private double _currentTime;      // 当前时间戳
        private double _lastUpdateTime;   // 上次更新时间戳（用于周期性 UDP 更新）
        private MessageEventQueue? _eventQueue; // 全局消息事件队列
        private uint _ioModuleIndex;      // 本模块索引
        // Socket 注册表（Socket 句柄 → SocketBase 对象）
        private readonly Dictionary<Socket, SocketBase> _sockets = new();
        // 预分配缓冲区：避免 DealFunction 每次 new List（线程本地，避免并发访问）
        private readonly List<SocketBase> _socketsBuffer = new(256);
        // 本地 PostEvent 队列（从外部线程投递到 IO 线程的事件）—— 双缓冲
        private List<MessageEventBase> _pendingEvents = new();
        private List<MessageEventBase> _processingEvents = new();
        private readonly object _eventLock = new object(); // 事件队列锁

        // 单例数组：始终 1 个 IO 模块（与 C++ FXNET_AOI_THREAD_NUM=1 对齐）
        // SINGLE_THREAD 只影响是否创建后台线程，不影响模块数量
        private static readonly IoModule[] _instances = new IoModule[1];

        public IoModule()
        {
            _currentTime = 0;
            _lastUpdateTime = 0;
        }

        /// <summary>获取指定索引的 IO 模块实例</summary>
        public static IoModule? GetInstance(uint index)
        {
            if (index >= _instances.Length) return _instances[0];
            return _instances[index];
        }

        public static uint GetModuleCount() => (uint)_instances.Length;

        /// <summary>初始化 IO 模块，启动后台 IO 线程</summary>
        public bool Init(uint index, MessageEventQueue queue, TextWriter? output)
        {
            _ioModuleIndex = index;
            _eventQueue = queue;

            if (_instances[index] == null)
                _instances[index] = this;

            // 创建后台 IO 线程（对齐 C++ FxIoModule::Start）
            // 单线程模式不创建后台线程，由主线程通过 ProcSingleThread 调用 DealFunction
            _stop = false;
#if !SINGLE_THREAD
            _thread = new FxThread(this);
            if (!_thread.Start())
            {
                output?.WriteLine($"[IoModule] 后台线程启动失败!");
                return false;
            }
#endif

            return true;
        }

        public void Uninit()
        {
            if (!_stop) Stop();
        }

        public double GetCurrentTime() => _currentTime;

        /// <summary>向全局消息队列投递事件</summary>
        public void PushMessageEvent(MessageEventBase evt)
        {
            _eventQueue?.PushMessageEvent(evt);
        }

        /// <summary>从外部线程向 IO 线程投递事件（线程安全）</summary>
        public void PostEvent(IOEventBase evt)
        {
            lock (_eventLock)
            {
                _pendingEvents.Add(evt);
            }
        }

        /// <summary>注册 Socket 到本 IO 模块的管理表</summary>
        public void RegisterSocket(Socket socket, SocketBase socketBase)
        {
            lock (_sockets)
            {
                _sockets[socket] = socketBase;
            }
        }

        /// <summary>注销 Socket</summary>
        public void DeregisterSocket(Socket socket)
        {
            lock (_sockets)
            {
                _sockets.Remove(socket);
            }
        }

        /// <summary>关闭所有注册的 Socket，先通知 Session 再注销</summary>
        public void CloseAllSockets(TextWriter? output)
        {
            List<KeyValuePair<Socket, SocketBase>> sockets;
            lock (_sockets)
            {
                sockets = new List<KeyValuePair<Socket, SocketBase>>(_sockets);
            }

            var error = new ErrorCode((int)UserError.CodeSuccessNetEOF, $"{nameof(IoModule)}:CloseAllSockets");

            foreach (var kvp in sockets)
            {
                // 先通知 Session 错误/关闭，再注销 Socket
                kvp.Value.OnError(error, output);
                DeregisterSocket(kvp.Key);
                try { kvp.Key.Close(); } catch (Exception) { /* Socket.Close 异常可忽略 */ }
            }
        }

        /// <summary>
        /// 后台 IO 线程主循环（多线程模式下由后台线程调用）。
        /// 每轮调用 DealFunction：更新 Socket 状态 + 处理消息事件队列。
        /// </summary>
        public void ThreadFunc()
        {
            try
            {
                while (!_stop)
                {
                    DealFunction(null);
                    Thread.Sleep(1);
                }
            }
            catch (Exception ex)
            {
                // 后台线程不应崩溃，记录异常后继续
                Console.Error.WriteLine($"[IoModule:{_ioModuleIndex}] ThreadFunc 异常: {ex}");
            }
        }

        /// <summary>处理从外部线程 PostEvent 投递的事件（双缓冲：交换列表避免每次分配）</summary>
        private void ProcessPendingEvents(TextWriter? output)
        {
            lock (_eventLock)
            {
                if (_pendingEvents.Count == 0) return;
                (_pendingEvents, _processingEvents) = (_processingEvents, _pendingEvents);
            }

            foreach (var evt in _processingEvents)
            {
                evt.Execute(output);
            }
            _processingEvents.Clear();
        }

        /// <summary>
        /// 核心处理函数，周期性调用：
        /// 1. 更新当前时间
        /// 2. 周期性更新所有 Socket（UDP 可靠传输定时重传等）
        /// 3. 处理 PostEvent 投递的本地 IO 事件
        /// 注意：全局消息队列处理不在此方法内，由调用方（主循环）单独调用 ProcessMessageEvents
        /// </summary>
        public void DealFunction(TextWriter? output)
        {
            _currentTime = TimeUtility.GetTimeSeconds();

            // 1. 周期性更新所有 Socket（UDP 可靠传输定时重传等）
            if (_currentTime - _lastUpdateTime >= SlidingWindowDef.UdpSendFrequency)
            {
                var error = new ErrorCode();
                _lastUpdateTime = _currentTime;

                List<SocketBase> snapshot;
                lock (_sockets)
                {
                    _socketsBuffer.Clear();
                    _socketsBuffer.EnsureCapacity(_sockets.Count);
                    foreach (var kvp in _sockets) _socketsBuffer.Add(kvp.Value);
                    snapshot = new List<SocketBase>(_socketsBuffer);
                }

                foreach (var sock in snapshot)
                {
                    sock.Update(_currentTime, error, output);
                    if (error)
                    {
                        var sockHandle2 = sock.GetSocket();
                        if (sockHandle2 != null)
                        {
                            DeregisterSocket(sockHandle2);
                            try { sockHandle2.Close(); } catch (Exception) { }
                        }
                        sock.OnError(error, output);
                        error.Set(0, "");
                    }
                }
            }

            // 2. 处理 PostEvent 投递的本地 IO 事件
            ProcessPendingEvents(output);
        }

        public void Stop()
        {
            _stop = true;
#if !SINGLE_THREAD
            _thread?.Stop();
            _thread = null;
#endif
        }

        public void SetStoped() => _stop = true;
        public uint GetThreadId() => _thread?.ThreadId ?? 0;

        /// <summary>释放资源：停止 IO 线程并关闭所有 Socket</summary>
        public void Dispose()
        {
            Stop();
            CloseAllSockets(null);
        }
    }
}
