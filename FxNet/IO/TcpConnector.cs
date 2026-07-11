using FxNet.Core;
using FxNet.Util;
using System.Buffers;
using System.Net;
using System.Net.Sockets;

namespace FxNet.IO
{
    /// <summary>
    /// TCP 连接器，基于 SocketAsyncEventArgs 实现异步 TCP 连接/收发。
    /// 负责：异步连接、接收循环、粘包处理、发送调度、断线处理。
    /// </summary>
    public class TcpConnector : ConnectorSocket
    {
        private IPEndPoint _remoteEndPoint = new IPEndPoint(IPAddress.Any, 0); // 远端地址
        private readonly byte[] _recvBuffer = new byte[8192]; // 接收缓冲区
        private SocketAsyncEventArgs? _recvArgs; // 复用的接收 SAEA

        public TcpConnector(ISession? session) : base(session)
        {
        }

        public override string Name => "TcpConnector";

        public IPEndPoint GetRemoteEndPoint() => _remoteEndPoint;
        public TcpConnector SetRemoteEndPoint(IPEndPoint ep) { _remoteEndPoint = ep; return this; }

        public void Init(TextWriter? output, int state) { }

        public override void Update(double time, ErrorCode error, TextWriter? output) { }

        /// <summary>发起异步 TCP 连接</summary>
        public TcpConnector Connect(IPEndPoint address, ErrorCode error, TextWriter? output)
        {
            ArgumentNullException.ThrowIfNull(address, nameof(address));
            if (NativeSocketHandle != null)
            {
                error.Set((int)UserError.CodeErrorNetSessionAlreadyConnected, "TcpConnector:Connect already connected");
                return this;
            }
            Socket? socket = null;
            try
            {
                socket = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                socket.SetSocketOption(SocketOptionLevel.Tcp, SocketOptionName.NoDelay, true);
                socket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.Linger, new LingerOption(false, 0));
                socket.Blocking = false;

                _remoteEndPoint = address;

                var module = IoModule.GetInstance(IOModuleIndex);
                if (module == null)
                {
                    error.Set((int)UserError.CodeErrorNetErrorSocket, "TcpConnector:Connect no module");
                    try { socket.Close(); } catch { }
                    return this;
                }

                NativeSocketHandle = socket;
                module.RegisterSocket(NativeSocketHandle, this);

                // Start async connect（完成后在回调中释放 SAEA）
                var args = new SocketAsyncEventArgs();
                args.RemoteEndPoint = address;
                args.Completed += OnConnectCompleted;
                args.UserToken = this;

                if (!NativeSocketHandle.ConnectAsync(args))
                {
                    OnConnectCompleted(null, args);
                }
            }
            catch (Exception ex)
            {
                // 失败时关闭已创建的 Socket，避免句柄泄漏
                if (socket != null)
                {
                    try { socket.Close(); } catch { }
                }
                NativeSocketHandle = null;
                error.Set(ex.HResult, $"TcpConnector:Connect {ex.Message}");
            }
            return this;
        }

        /// <summary>使用已接受的 Socket 初始化连接（服务端接受新连接时调用）</summary>
        public TcpConnector ConnectWithSocket(Socket socket, IPEndPoint remoteEp, ErrorCode error, TextWriter? output)
        {
            NativeSocketHandle = socket;
            _remoteEndPoint = remoteEp;

            try
            {
                NativeSocketHandle.SetSocketOption(SocketOptionLevel.Tcp, SocketOptionName.NoDelay, true);
                NativeSocketHandle.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.Linger, new LingerOption(false, 0));
                NativeSocketHandle.Blocking = false;
            }
            catch (Exception ex) { LogUtility.Log((TextWriter?)null, LogLevel.Warn, $"TcpConnector:ConnectWithSocket {ex.Message}"); }

            var module = IoModule.GetInstance(IOModuleIndex);
            module?.RegisterSocket(NativeSocketHandle, this);

            OnConnected(output);
            return this;
        }

        // 异步连接完成回调
        private void OnConnectCompleted(object? sender, SocketAsyncEventArgs e)
        {
            var connector = (TcpConnector)e.UserToken!;
            var output = (TextWriter?)null;

            // 连接 SAEA 仅使用一次，完成后释放
            e.Completed -= OnConnectCompleted;
            e.Dispose();

            if (e.SocketError != SocketError.Success)
            {
                var error = new ErrorCode((int)e.SocketError, "TcpConnector:ConnectAsync");
                connector.Error = error;
                var module = IoModule.GetInstance(connector.IOModuleIndex);
                if (connector.Session != null)
                {
                    module?.PushMessageEvent(connector.Session.NewErrorEvent(error));
                    module?.PushMessageEvent(connector.Session.NewCloseEvent());
                    connector.Session = null;
                }
                return;
            }

            connector.OnConnected(output);
        }

        // 连接建立后初始化：获取本地端点、通知 Session、启动接收
        private void OnConnected(TextWriter? output)
        {
            try
            {
                LocalEndPoint = (IPEndPoint)NativeSocketHandle!.LocalEndPoint!;
            }
            catch (Exception ex) { LogUtility.Log((TextWriter?)null, LogLevel.Warn, $"TcpConnector:OnConnected {ex.Message}"); }

            var module = IoModule.GetInstance(IOModuleIndex);
            module?.PushMessageEvent(Session!.NewConnectedEvent());

            StartReceive();
        }

        /// <summary>启动异步接收循环（复用同一个 SAEA 实例）</summary>
        private void StartReceive()
        {
            if (NativeSocketHandle == null || Session == null) return;

            // 首次调用时创建接收 SAEA，后续复用
            _recvArgs ??= new SocketAsyncEventArgs();
            if (_recvArgs.Buffer != _recvBuffer)
            {
                _recvArgs.SetBuffer(_recvBuffer, 0, _recvBuffer.Length);
                _recvArgs.Completed += OnReceiveCompleted;
                _recvArgs.UserToken = this;
            }

            try
            {
                if (!NativeSocketHandle.ReceiveAsync(_recvArgs))
                {
                    OnReceiveCompleted(null, _recvArgs);
                }
            }
            catch (Exception)
            {
                HandleDisconnect(new ErrorCode((int)UserError.CodeSuccessNetEOF, "TcpConnector:StartReceive"));
            }
        }

        // 接收完成回调：将数据写入 Session 接收缓冲区，进行粘包处理
        private void OnReceiveCompleted(object? sender, SocketAsyncEventArgs e)
        {
            var connector = (TcpConnector)e.UserToken!;

            if (e.SocketError != SocketError.Success || e.BytesTransferred == 0)
            {
                connector.HandleDisconnect(new ErrorCode(
                    e.SocketError == SocketError.Success ? (int)UserError.CodeSuccessNetEOF : (int)e.SocketError,
                    "TcpConnector:OnReceive"));
                return;
            }

            var session = connector.Session;
            if (session == null) return;

            var recvBuff = session.GetRecvBuff();
            recvBuff.PushData(e.Buffer!, e.BytesTransferred);

            while (recvBuff.CheckPackage())
            {
                var msgEvent = session.NewRecvMessageEvent();
                recvBuff.PopData(msgEvent.Package);
                msgEvent.Session = session;

                var module = IoModule.GetInstance(connector.IOModuleIndex);
                module?.PushMessageEvent(msgEvent);
            }

            // Continue receiving
            connector.StartReceive();
        }

        /// <summary>发送消息：将 Session 发送缓冲区的数据通过 Socket 异步发送（使用 ArrayPool 减少分配）</summary>
        public override void SendMessage(ErrorCode error, TextWriter? output)
        {
            if (NativeSocketHandle == null || Session == null) return;

            var sendBuff = Session.GetSendBuff();
            if (sendBuff.GetSize() == 0) return;

            int sendSize = sendBuff.GetSize();
            var data = ArrayPool<byte>.Shared.Rent(sendSize);
            Array.Copy(sendBuff.GetData(), data, sendSize);
            sendBuff.PopData(sendSize);

            var args = new SocketAsyncEventArgs();
            args.SetBuffer(data, 0, sendSize);
            args.Completed += OnSendCompleted;
            args.UserToken = this;

            try
            {
                if (!NativeSocketHandle.SendAsync(args))
                {
                    OnSendCompleted(null, args);
                }
            }
            catch (Exception ex)
            {
                ArrayPool<byte>.Shared.Return(data);
                args.Dispose();
                error.Set(ex.HResult, $"TcpConnector:SendMessage {ex.Message}");
            }
        }

        // 发送完成回调：继续发送剩余数据，完成后释放 SAEA 和 ArrayPool 缓冲区
        private void OnSendCompleted(object? sender, SocketAsyncEventArgs e)
        {
            var connector = (TcpConnector)e.UserToken!;

            // 归还 ArrayPool 缓冲区
            if (e.Buffer != null)
            {
                ArrayPool<byte>.Shared.Return(e.Buffer);
            }

            // 发送 SAEA 释放
            e.Completed -= OnSendCompleted;
            e.Dispose();

            if (e.SocketError != SocketError.Success)
            {
                connector.HandleDisconnect(new ErrorCode((int)e.SocketError, "TcpConnector:OnSendCompleted"));
                return;
            }

            var module = IoModule.GetInstance(connector.IOModuleIndex);
            module?.PushMessageEvent(connector.Session!.NewOnSendEvent(e.BytesTransferred));

            // Send remaining data
            var error = new ErrorCode();
            connector.SendMessage(error, null);
        }

        /// <summary>处理断线：注销 Socket、通知 Session 错误和关闭</summary>
        private void HandleDisconnect(ErrorCode error)
        {
            var module = IoModule.GetInstance(IOModuleIndex);
            if (NativeSocketHandle != null)
            {
                module?.DeregisterSocket(NativeSocketHandle);
                try { NativeSocketHandle.Close(); } catch (Exception) { /* Socket.Close 异常可忽略 */ }
                NativeSocketHandle = null;
            }

            if (Session != null)
            {
                module?.PushMessageEvent(Session.NewErrorEvent(error));
                module?.PushMessageEvent(Session.NewCloseEvent());
                Session = null;
            }
        }

        public override void Close(TextWriter? output)
        {
            var module = IoModule.GetInstance(IOModuleIndex);
            if (NativeSocketHandle != null)
            {
                module?.DeregisterSocket(NativeSocketHandle);
                try { NativeSocketHandle.Close(); } catch (Exception) { /* Socket.Close 异常可忽略 */ }
                NativeSocketHandle = null;
            }
        }

        public override void OnError(ErrorCode error, TextWriter? output)
        {
            LogUtility.Log(output, LogLevel.Error, $"TcpConnector {NativeSocketHandle?.Handle} error: {error.GetWhat()}");
        }

        public override void OnClose(TextWriter? output)
        {
            LogUtility.Log(output, LogLevel.Info, $"TcpConnector {NativeSocketHandle?.Handle} closed");
        }

        /// <summary>释放接收 SAEA 和 Socket 资源</summary>
        public override void Dispose()
        {
            _recvArgs?.Dispose();
            _recvArgs = null;
            base.Dispose();
        }
    }
}
