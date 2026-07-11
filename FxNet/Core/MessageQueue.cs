using System.Collections.Generic;

namespace FxNet.Core
{
    /// <summary>
    /// 线程安全的消息事件队列。
    /// 使用 Queue + lock 实现跨线程事件投递，
    /// SwapEvents 可原子性地取出所有事件并批量处理。
    /// </summary>
    public class MessageEventQueue
    {
        private readonly Queue<MessageEventBase> _events = new();
        private readonly object _lock = new();

        public void PushMessageEvent(MessageEventBase evt)
        {
            lock (_lock)
            {
                _events.Enqueue(evt);
            }
        }

        /// <summary>原子性地取出所有事件（用于 IO 线程批量处理）</summary>
        public void SwapEvents(List<MessageEventBase> target)
        {
            lock (_lock)
            {
                while (_events.Count > 0)
                {
                    target.Add(_events.Dequeue());
                }
            }
        }

        public bool TryDequeue(out MessageEventBase? evt)
        {
            lock (_lock)
            {
                if (_events.Count > 0)
                {
                    evt = _events.Dequeue();
                    return true;
                }
                evt = null;
                return false;
            }
        }

        public bool IsEmpty
        {
            get
            {
                lock (_lock)
                {
                    return _events.Count == 0;
                }
            }
        }
    }
}
