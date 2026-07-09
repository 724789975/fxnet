using FxNet.Core;
using FxNet.Util;
using System.Buffers.Binary;

namespace FxNet.Dll
{
    /// <summary>
    /// 文本协议会话实现，采用 4 字节大端长度头 + 数据体的消息格式。
    /// 实现 ISession 接口，负责数据的序列化/反序列化和事件生成。
    /// </summary>
    public class TextSession : ISession
    {
        private ConnectorSocket? _socket; // 底层连接器
        private readonly TextWorkStream _sendBuff = new TextWorkStream(); // 发送缓冲区
        private readonly TextWorkStream _recvBuff = new TextWorkStream(); // 接收缓冲区

        private readonly Connector _connector;
        private readonly OnRecvCallback _onRecv;
        private readonly OnConnectedCallback _onConnected;
        private readonly OnErrorCallback _onError;
        private readonly OnCloseCallback _onClose;

        public TextSession(Connector connector,
            OnRecvCallback onRecv, OnConnectedCallback onConnected,
            OnErrorCallback onError, OnCloseCallback onClose)
        {
            _connector = connector;
            _onRecv = onRecv;
            _onConnected = onConnected;
            _onError = onError;
            _onClose = onClose;
        }

        public void SetSocket(ConnectorSocket? socket) => _socket = socket;
        public ConnectorSocket? GetSocket() => _socket;

        /// <summary>发送数据：写入 4 字节长度头 + 数据体，然后触发底层发送</summary>
        public ISession Send(byte[] data, int len, TextWriter? output)
        {
            // Write length header + data
            var header = new byte[4];
            BinaryPrimitives.WriteInt32BigEndian(header, len);
            _sendBuff.PushData(header, 4);
            _sendBuff.PushData(data, len);

            _socket?.SendMessage(new ErrorCode(), output);
            return this;
        }

        public ISession OnRecv(NetStreamPackage package, TextWriter? output)
        {
            _onRecv(_connector, package.GetData(), package.DataLength);
            return this;
        }

        public void OnConnected(TextWriter? output)
        {
            _onConnected(_connector);
        }

        public void OnError(ErrorCode error, TextWriter? output)
        {
            _onError(_connector, error.Code);
        }

        public void OnClose(TextWriter? output)
        {
            _onClose(_connector);
        }

        public NetWorkStream GetSendBuff() => _sendBuff;
        public NetWorkStream GetRecvBuff() => _recvBuff;

        /// <summary>创建各类跨线程消息事件，由 IO 线程投递并执行</summary>

        public MessageRecvEventBase NewRecvMessageEvent()
        {
            return new TextMessageEvent(this);
        }

        public MessageEventBase NewConnectedEvent()
        {
            return new ConnectedEvent(this);
        }

        public MessageEventBase NewErrorEvent(ErrorCode error)
        {
            return new SessionErrorEvent(this, error);
        }

        public MessageEventBase NewCloseEvent()
        {
            return new CloseSessionEvent(this);
        }

        public MessageEventBase NewOnSendEvent(int len)
        {
            return new SessionOnSendEvent(this, len);
        }

        // === 内部事件类，用于跨线程投递 ===

        /// <summary>数据接收事件，在 IO 线程中调用 Session.OnRecv</summary>
        private class TextMessageEvent : MessageRecvEventBase
        {
            private readonly ISession _session;

            public TextMessageEvent(ISession session)
            {
                _session = session;
                Session = session;
            }

            public override void Execute(TextWriter? output)
            {
                _session.OnRecv(Package, output);
            }
        }

        /// <summary>连接建立事件</summary>
        private class ConnectedEvent : MessageEventBase
        {
            private readonly ISession _session;
            public ConnectedEvent(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnConnected(output);
        }

        /// <summary>会话错误事件</summary>
        private class SessionErrorEvent : MessageEventBase
        {
            private readonly ISession _session;
            private readonly ErrorCode _error;
            public SessionErrorEvent(ISession session, ErrorCode error)
            {
                _session = session;
                _error = error;
            }
            public override void Execute(TextWriter? output) => _session.OnError(_error, output);
        }

        /// <summary>会话关闭事件</summary>
        private class CloseSessionEvent : MessageEventBase
        {
            private readonly ISession _session;
            public CloseSessionEvent(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnClose(output);
        }

        /// <summary>发送完成通知事件</summary>
        private class SessionOnSendEvent : MessageEventBase
        {
            private readonly ISession _session;
            private readonly int _len;
            public SessionOnSendEvent(ISession session, int len)
            {
                _session = session;
                _len = len;
            }
            public override void Execute(TextWriter? output)
            {
                // OnSend notification - could be extended
            }
        }
    }
}
