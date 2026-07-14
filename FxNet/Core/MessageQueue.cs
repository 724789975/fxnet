using System.Collections.Concurrent;

namespace FxNet.Core
{
    /// <summary>
    /// 线程安全的消息事件队列。
    /// 使用 ConcurrentQueue 实现无锁跨线程事件投递，
    /// SwapEvents 可原子性地取出所有事件并批量处理。
    /// </summary>
    public class MessageEventQueue
    {
        private readonly ConcurrentQueue<MessageEventBase> _events = new();

        public void PushMessageEvent(MessageEventBase evt)
        {
            _events.Enqueue(evt);
        }

        /// <summary>原子性地取出所有事件（用于 IO 线程批量处理）</summary>
        public void SwapEvents(List<MessageEventBase> target)
        {
            while (_events.TryDequeue(out var evt))
            {
                target.Add(evt);
            }
        }

        public bool TryDequeue(out MessageEventBase? evt)
        {
            if (_events.TryDequeue(out var e))
            {
                evt = e;
                return true;
            }
            evt = null;
            return false;
        }

        public bool IsEmpty => _events.IsEmpty;
    }
}
