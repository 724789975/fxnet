using System.Collections.Concurrent;

namespace FxNet.Core
{
    /// <summary>
    /// 线程安全的消息事件队列。
    /// 使用 ConcurrentQueue 实现跨线程事件投递，
    /// IO 线程通过 SwapEvents 批量取出事件并处理。
    /// </summary>
    public class MessageEventQueue
    {
        private readonly ConcurrentQueue<MessageEventBase> _events = new();

        public void PushMessageEvent(MessageEventBase evt)
        {
            _events.Enqueue(evt);
        }

        public void SwapEvents(List<MessageEventBase> target)
        {
            while (_events.TryDequeue(out var evt))
            {
                target.Add(evt);
            }
        }

        public bool TryDequeue(out MessageEventBase? evt)
        {
            return _events.TryDequeue(out evt);
        }

        public bool IsEmpty => _events.IsEmpty;
    }
}
