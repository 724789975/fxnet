namespace FxNet.Core
{
    /// <summary>
    /// 消息事件抽象基类。
    /// 所有跨线程投递的事件均继承此类，由 IO 线程在 DealFunction 中调用 Execute 执行。
    /// </summary>
    public abstract class MessageEventBase
    {
        /// <summary>事件执行入口，由 IO 线程调用</summary>
        public abstract void Execute(TextWriter? output);
    }

    /// <summary>
    /// 数据接收事件基类，携带 NetStreamPackage 数据包
    /// </summary>
    public class MessageRecvEventBase : MessageEventBase
    {
        public NetStreamPackage Package { get; set; } = new NetStreamPackage();
        public ISession? Session { get; set; }

        public override void Execute(TextWriter? output)
        {
            Session?.OnRecv(Package, output);
        }
    }

    /// <summary>
    /// IO 事件基类，用于从外部线程向 IO 线程投递事件（如新连接建立）
    /// </summary>
    public abstract class IOEventBase : MessageEventBase
    {
    }
}
