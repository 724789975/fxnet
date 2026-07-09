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
        private int _dealCount;           // DealFunction 调用计数（调试用）

        // Socket 注册表（Socket 句柄 → SocketBase 对象）
        private readonly Dictionary<Socket, SocketBase> _sockets = new();
        // 本地 PostEvent 队列（从外部线程投递到 IO 线程的事件）
        private readonly List<MessageEventBase> _pendingEvents = new();
        private readonly object _eventLock = new object(); // 事件队列锁

        // 单例数组（当前仅支持 1 个 IO 模块）
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

            _thread = new FxThread(this);
            if (!_thread.Start())
            {
                LogUtility.Log(output, LogLevel.Error, "IoModule start thread failed");
                return false;
            }
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
        /// 后台 IO 线程主循环。
        /// 注意：Socket 周期性更新（UDP 重传等）由 DealFunction 在主线程处理，
        /// 后台线程仅负责处理 PostEvent 投递的 IO 事件，避免多线程并发访问 BufferContral 状态。
        /// </summary>
        public void ThreadFunc()
        {
            while (!_stop)
            {
                _currentTime = TimeUtility.GetTimeSeconds();

                // 处理 PostEvent 投递的 IO 事件
                ProcessPendingEvents(null);

                Thread.Sleep(1);
            }
        }

        /// <summary>处理从外部线程 PostEvent 投递的事件</summary>
        private void ProcessPendingEvents(TextWriter? output)
        {
            List<MessageEventBase> events;
            lock (_eventLock)
            {
                if (_pendingEvents.Count == 0) return;
                events = new List<MessageEventBase>(_pendingEvents);
                _pendingEvents.Clear();
            }

            foreach (var evt in events)
            {
                evt.Execute(output);
            }
        }

        /// <summary>
        /// 核心处理函数，周期性调用：
        /// 1. 更新当前时间
        /// 2. 周期性更新所有 Socket（UDP 可靠传输定时重传等）
        /// 3. 批量处理全局消息队列中的事件
        /// </summary>
        public void DealFunction(TextWriter? output)
        {
            _currentTime = TimeUtility.GetTimeSeconds();

            // Update sockets periodically
            if (_currentTime - _lastUpdateTime >= SlidingWindowDef.UdpSendFrequency)
            {
                var error = new ErrorCode();
                _lastUpdateTime = _currentTime;

                List<SocketBase> socketsCopy;
                lock (_sockets)
                {
                    socketsCopy = new List<SocketBase>(_sockets.Values);
                }

                if (_dealCount++ % 200 == 0)
                    Console.WriteLine($"[DBG-DEAL] Updating {socketsCopy.Count} sockets");

                foreach (var sock in socketsCopy)
                {
                    sock.Update(_currentTime, error, output);
                    if (error)
                    {
                        var sockHandle2 = sock.GetSocket();
                        if (sockHandle2 != null)
                            DeregisterSocket(sockHandle2);
                        sock.OnError(error, output);
                        error.Set(0, "");
                    }
                }
            }

            // Process message events from the queue
            if (_eventQueue != null)
            {
                var events = new List<MessageEventBase>();
                _eventQueue.SwapEvents(events);
                foreach (var evt in events)
                {
                    evt.Execute(output);
                }
            }
        }

        public void Stop()
        {
            _stop = true;
            _thread?.Stop();
            _thread = null;
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
