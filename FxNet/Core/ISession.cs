namespace FxNet.Core
{
    /// <summary>
    /// 会话接口，表示一个网络连接的对端实体。
    /// 每个连接对应一个 ISession，负责数据的收发、事件生成和生命周期管理。
    /// </summary>
    public interface ISession
    {
        /// <summary>绑定底层 ConnectorSocket</summary>
        void SetSocket(ConnectorSocket? socket);
        ConnectorSocket? GetSocket();

        /// <summary>发送数据（由上层调用）</summary>
        ISession Send(byte[] data, int len, TextWriter? output);
        /// <summary>接收数据回调（由底层 IO 调用）</summary>
        ISession OnRecv(NetStreamPackage package, TextWriter? output);

        /// <summary>连接建立/错误/关闭事件回调</summary>
        void OnConnected(TextWriter? output);
        void OnError(ErrorCode error, TextWriter? output);
        void OnClose(TextWriter? output);

        /// <summary>获取发送/接收缓冲区</summary>
        NetWorkStream GetSendBuff();
        NetWorkStream GetRecvBuff();

        /// <summary>创建各类消息事件（用于跨线程投递）</summary>
        MessageRecvEventBase NewRecvMessageEvent();
        MessageEventBase NewConnectedEvent();
        MessageEventBase NewErrorEvent(ErrorCode error);
        MessageEventBase NewCloseEvent();
        MessageEventBase NewOnSendEvent(int len);
    }

    /// <summary>
    /// 会话工厂接口，用于在监听器接受新连接时创建 Session
    /// </summary>
    public interface ISessionMaker
    {
        ISession Create();
    }
}
