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
        private SocketAsyncEventArgs? _sendArgs; // 复用的发送 SAEA
        private bool _sendSubscribed;            // 发送 SAEA 的 Completed 是否已订阅（仅订阅一次）
        private bool _sending;                   // 发送在途标志，串行化发送避免并发复用 _sendArgs
        private readonly object _sendLock = new object(); // 保护发送路径与发送缓冲区的并发访问

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

        /// <summary>发送消息：复用同一个发送 SAEA，串行化发送，避免每次创建 SAEA。
        /// 若已有发送在途，新追加到发送缓冲区的数据会在完成回调的循环中被继续发送。</summary>
        public override void SendMessage(ErrorCode error, TextWriter? output)
        {
            if (NativeSocketHandle == null || Session == null) return;
            lock (_sendLock)
            {
                if (_sending) return; // 已有发送在途，数据会在 OnSendCompleted 中被继续发送
                BeginSend(error);
            }
        }

        // 必须在持有 _sendLock 时调用：从发送缓冲区取出数据并发起一次异步发送（同步完成时循环发送，避免递归）。
        private void BeginSend(ErrorCode error)
        {
            while (true)
            {
                if (NativeSocketHandle == null || Session == null) { _sending = false; return; }

                var sendBuff = Session.GetSendBuff();
                int sendSize = sendBuff.GetSize();
                if (sendSize == 0) { _sending = false; return; }

                var data = ArrayPool<byte>.Shared.Rent(sendSize);
                Array.Copy(sendBuff.GetData(), data, sendSize);
                sendBuff.PopData(sendSize);

                _sendArgs ??= new SocketAsyncEventArgs { UserToken = this };
                if (!_sendSubscribed) { _sendArgs.Completed += OnSendCompleted; _sendSubscribed = true; }
                _sendArgs.SetBuffer(data, 0, sendSize);
                _sending = true;

                bool pending;
                try
                {
                    pending = NativeSocketHandle.SendAsync(_sendArgs);
                }
                catch (Exception ex)
                {
                    _sendArgs.SetBuffer(null, 0, 0);
                    ArrayPool<byte>.Shared.Return(data);
                    _sending = false;
                    error.Set(ex.HResult, $"TcpConnector:SendMessage {ex.Message}");
                    return;
                }

                if (pending) return; // 异步完成，OnSendCompleted 会继续发送剩余数据

                // 同步完成：直接处理后循环发送剩余数据（避免递归）
                if (!HandleSendCompletion(_sendArgs)) return; // 出错/断线，停止发送
            }
        }

        // 处理一次发送完成：归还缓冲区、投递 OnSend 事件。返回 false 表示应停止发送。
        // 调用方需持有 _sendLock。
        private bool HandleSendCompletion(SocketAsyncEventArgs e)
        {
            var buffer = e.Buffer;
            e.SetBuffer(null, 0, 0); // 清除对缓冲区的引用，便于复用与回收
            if (buffer != null) ArrayPool<byte>.Shared.Return(buffer);

            if (e.SocketError != SocketError.Success)
            {
                _sending = false;
                HandleDisconnect(new ErrorCode((int)e.SocketError, "TcpConnector:OnSendCompleted"));
                return false;
            }

            var session = Session;
            if (session != null)
            {
                var module = IoModule.GetInstance(IOModuleIndex);
                module?.PushMessageEvent(session.NewOnSendEvent(e.BytesTransferred));
            }
            return true;
        }

        // 异步发送完成回调（线程池线程）：处理完成并继续发送剩余数据
        private void OnSendCompleted(object? sender, SocketAsyncEventArgs e)
        {
            var connector = (TcpConnector)e.UserToken!;
            lock (connector._sendLock)
            {
                if (!connector.HandleSendCompletion(e)) return;
                var error = new ErrorCode();
                connector.BeginSend(error); // 继续发送剩余数据
            }
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

        /// <summary>释放接收/发送 SAEA 和 Socket 资源</summary>
        public override void Dispose()
        {
            _recvArgs?.Dispose();
            _recvArgs = null;
            _sendArgs?.Dispose();
            _sendArgs = null;
            base.Dispose();
        }
    }
}
