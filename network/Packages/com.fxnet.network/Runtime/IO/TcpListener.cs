using System;
using System.IO;
using System.Net;
using System.Net.Sockets;
using UnityEngine;
using FxNet.Core;

namespace FxNet.IO
{
    /// <summary>
    /// TCP 监听器，基于 SocketAsyncEventArgs 实现异步 Accept。
    /// 新连接通过 TcpClientConnectEvent 投递到 IO 线程完成后续初始化。
    /// </summary>
    public class TcpListener : ListenSocket
    {
        private readonly ISessionMaker _sessionMaker;
        private SocketAsyncEventArgs? _acceptArgs;

        public TcpListener(ISessionMaker sessionMaker)
        {
            _sessionMaker = sessionMaker;
        }

        public override string Name => "TcpListener";

        public override void Update(double time, ErrorCode error, TextWriter? output) { }

        /// <summary>绑定并监听指定地址，启动异步 Accept</summary>
        public TcpListener Listen(string ip, ushort port, ErrorCode error, TextWriter? output)
        {
            try
            {
                NativeSocketHandle = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
                NativeSocketHandle.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);
                NativeSocketHandle.Blocking = false;

                var localEp = new IPEndPoint(
                    string.IsNullOrEmpty(ip) ? IPAddress.Any : IPAddress.Parse(ip),
                    port);

                NativeSocketHandle.Bind(localEp);
                NativeSocketHandle.Listen(128);

                LocalEndPoint = localEp;

                var module = IoModule.GetInstance(IOModuleIndex);
                module?.RegisterSocket(NativeSocketHandle, this);

                StartAccept(output);

                Debug.Log($"TcpListener listening on {localEp}");
            }
            catch (Exception ex)
            {
                error.Set(ex.HResult, $"TcpListener:Listen {ex.Message}");
                Debug.LogError($"TcpListener:Listen failed: {ex.Message}");
            }

            return this;
        }

        /// <summary>启动异步 Accept 循环（复用同一个 SAEA 实例）</summary>
        private void StartAccept(TextWriter? output)
        {
            if (NativeSocketHandle == null) return;

            _acceptArgs ??= new SocketAsyncEventArgs();
            if (_acceptArgs.AcceptSocket != null) _acceptArgs.AcceptSocket = null;
            if (_acceptArgs.Buffer == null)
            {
                _acceptArgs.Completed += OnAcceptCompleted;
                _acceptArgs.UserToken = this;
                _acceptArgs.RemoteEndPoint = new IPEndPoint(IPAddress.Any, 0);
            }

            try
            {
                if (!NativeSocketHandle.AcceptAsync(_acceptArgs))
                {
                    OnAcceptCompleted(null, _acceptArgs);
                }
            }
            catch (Exception ex)
            {
                Debug.LogError($"TcpListener:StartAccept {ex.Message}");
            }
        }

        // Accept 完成回调
        private void OnAcceptCompleted(object? sender, SocketAsyncEventArgs e)
        {
            var listener = (TcpListener)e.UserToken!;

            if (e.SocketError != SocketError.Success)
            {
                Debug.LogError($"TcpListener:OnAccept error: {e.SocketError}");
                listener.StartAccept(null);
                return;
            }

            var clientSocket = e.AcceptSocket;
            var remoteEp = (IPEndPoint?)e.RemoteEndPoint;

            if (clientSocket != null && remoteEp != null)
            {
                listener.OnClientConnected(clientSocket, remoteEp, null);
            }

            listener.StartAccept(null);
        }

        // 新客户端连接处理：创建 Session 和 Connector，投递到 IO 线程
        private void OnClientConnected(Socket clientSocket, IPEndPoint remoteEp, TextWriter? output)
        {
            try
            {
                clientSocket.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.KeepAlive, true);
                clientSocket.SetSocketOption(SocketOptionLevel.Tcp, SocketOptionName.NoDelay, true);
                clientSocket.Blocking = false;
            }
            catch (Exception ex) { Debug.LogWarning($"TcpListener:OnClientConnected {ex.Message}"); }

            var session = _sessionMaker.Create();
            var connector = new TcpConnector(session);
            connector.SetIOModuleIndex(IOModuleIndex);
            session.SetSocket(connector);
            connector.SetRemoteEndPoint(remoteEp);

            var module = IoModule.GetInstance(IOModuleIndex);
            module?.PostEvent(new TcpClientConnectEvent(connector, clientSocket, remoteEp));
        }

        /// <summary>
        /// TCP 客户端连接事件，投递到 IO 线程执行。
        /// 在 IO 线程中完成 Socket 绑定和连接通知。
        /// </summary>
        private class TcpClientConnectEvent : IOEventBase
        {
            private readonly TcpConnector _connector;
            private readonly Socket _socket;
            private readonly IPEndPoint _remoteEp;

            public TcpClientConnectEvent(TcpConnector connector, Socket socket, IPEndPoint remoteEp)
            {
                _connector = connector;
                _socket = socket;
                _remoteEp = remoteEp;
            }

            public override void Execute(TextWriter? output)
            {
                var error = new ErrorCode();
                _connector.ConnectWithSocket(_socket, _remoteEp, error, output);
                if (error)
                {
                    Debug.LogError($"TcpClientConnect failed: {error.GetWhat()}");
                    if (_connector.GetSession() != null)
                    {
                        var module = IoModule.GetInstance(_connector.GetIOModuleIndex());
                        module?.PushMessageEvent(_connector.GetSession()!.NewErrorEvent(error));
                        module?.PushMessageEvent(_connector.GetSession()!.NewCloseEvent());
                    }
                }
            }
        }

        public override void Close(TextWriter? output)
        {
            if (NativeSocketHandle != null)
            {
                var module = IoModule.GetInstance(IOModuleIndex);
                module?.DeregisterSocket(NativeSocketHandle);
                try { NativeSocketHandle.Close(); } catch (Exception) { /* Socket.Close 异常可忽略 */ }
                NativeSocketHandle = null;
            }
        }

        public override void OnError(ErrorCode error, TextWriter? output)
        {
            Debug.LogError($"TcpListener error: {error.GetWhat()}");
        }

        public override void OnClose(TextWriter? output)
        {
            Debug.Log("TcpListener closed");
        }
    }
}
