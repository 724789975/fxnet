using System.IO;

namespace FxNet.Core
{
    /// <summary>
    /// 连接器 Socket 抽象基类，继承 SocketBase，增加 Session 绑定和消息发送能力。
    /// TCP/UDP Connector 均继承此类。
    /// </summary>
    public abstract class ConnectorSocket : SocketBase
    {
        protected ISession? Session; // 关联的会话

        public ConnectorSocket(ISession? session)
        {
            Session = session;
        }

        public ISession? GetSession() => Session;
        public ConnectorSocket SetSession(ISession? session)
        {
            Session = session;
            return this;
        }

        public override string Name => "ConnectorSocket";

        /// <summary>将待发送数据从 Session 缓冲区取出并写入底层 Socket</summary>
        public abstract void SendMessage(ErrorCode error, TextWriter? output);
    }
}
