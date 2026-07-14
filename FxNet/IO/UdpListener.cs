using FxNet.Core;
using FxNet.Util;
using System.Buffers;
using System.Net;
using System.Net.Sockets;

namespace FxNet.IO
{
    /// <summary>
    /// UDP 监听器，基于非阻塞 Socket 的同步 ReceiveFrom 轮询接收。
    /// 按远端 IPEndPoint 管理客户端字典，首次收到数据时自动创建 UdpConnector。
    /// 所有接收和处理均在主线程 Update 中完成，避免多线程并发问题。
    /// </summary>
    public class UdpListener : ListenSocket
    {
        private readonly ISessionMaker _sessionMaker;
        private readonly Dictionary<IPEndPoint, UdpConnector> _clients = new();
        private readonly byte[] _recvBuffer = new byte[SlidingWindow.BuffSize];
        // 复用的发送方端点（ReceiveFrom 每次会重新赋值为新对象，此字段仅作为模板，避免每包 new）
        private EndPoint _recvRemoteEp = new IPEndPoint(IPAddress.Any, 0);

        public UdpListener(ISessionMaker sessionMaker)
        {
            _sessionMaker = sessionMaker;
        }

        public override string Name => "UdpListener";

        public override void Update(double time, ErrorCode error, TextWriter? output)
        {
            // 1. 同步轮询接收所有待处理的 UDP 数据
            if (NativeSocketHandle != null)
            {
                while (true)
                {
                    try
                    {
                        int received = NativeSocketHandle.ReceiveFrom(_recvBuffer, 0, _recvBuffer.Length,
                            SocketFlags.None, ref _recvRemoteEp);
                        if (received <= 0) break;

                        var data = ArrayPool<byte>.Shared.Rent(received);
                        Array.Copy(_recvBuffer, data, received);
                        var clientEp = (IPEndPoint)_recvRemoteEp;

                        // 查找或创建客户端连接器
                        if (!_clients.TryGetValue(clientEp, out var client))
                        {
                            var session = _sessionMaker.Create();
                            client = new UdpConnector(session);
                            client.SetIOModuleIndex(IOModuleIndex);
                            session.SetSocket(client);
                            client.SetRemoteEndPoint(clientEp);
                            client.SetListenerSocket(NativeSocketHandle);
                            client.Init(null, (int)ConnectionStatus.Established);

                            // 注意：不要 RegisterSocket(NativeSocketHandle, client)，
                            // 因为 listener socket 已经注册为 UdpListener，覆盖会导致
                            // DealFunction 不再调用 UdpListener.Update()。
                            // 客户端的 Update 由 UdpListener.Update() 内部遍历 _clients 调用。
                            _clients[clientEp] = client;

                            var module = IoModule.GetInstance(IOModuleIndex);
                            module?.PushMessageEvent(session.NewConnectedEvent());
                        }

                        // 将数据加入客户端 UdpConnector 的待处理队列
                        client.EnqueuePendingRecv(data, received);
                    }
                    catch (SocketException ex) when (ex.SocketErrorCode == SocketError.WouldBlock)
                    {
                        break; // 无更多数据
                    }
                    catch (SocketException ex) when (ex.SocketErrorCode == SocketError.MessageSize)
                    {
                        // UDP 数据报大于缓冲区，丢弃（正常情况）
                        break;
                    }
                    catch (ObjectDisposedException)
                    {
                        break; // Socket 已关闭
                    }
                    catch (Exception ex)
                    {
                        LogUtility.Log(output, LogLevel.Error, $"UdpListener:ReceiveFrom error: {ex.Message}");
                        break;
                    }
                }
            }

            // 2. Update all client connections (处理接收数据 + 发送 keepalive/重传等)
            foreach (var client in _clients.Values)
            {
                client.Update(time, error, output);
                if (error)
                {
                    error.Set(0, "");
                }
            }
        }

        /// <summary>绑定并监听指定 UDP 地址</summary>
        public UdpListener Listen(string ip, ushort port, ErrorCode error, TextWriter? output)
        {
            try
            {
                NativeSocketHandle = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                NativeSocketHandle.SetSocketOption(SocketOptionLevel.Socket, SocketOptionName.ReuseAddress, true);

                var localEp = new IPEndPoint(
                    string.IsNullOrEmpty(ip) ? IPAddress.Any : IPAddress.Parse(ip),
                    port);

                NativeSocketHandle.Bind(localEp);
                NativeSocketHandle.Blocking = false;

                LocalEndPoint = localEp;

                var module = IoModule.GetInstance(IOModuleIndex);
                module?.RegisterSocket(NativeSocketHandle, this);

                LogUtility.Log(output, LogLevel.Info, $"UdpListener listening on {localEp}");
            }
            catch (Exception ex)
            {
                error.Set(ex.HResult, $"UdpListener:Listen {ex.Message}");
                LogUtility.Log(output, LogLevel.Error, $"UdpListener:Listen failed: {ex.Message}");
            }

            return this;
        }

        public override void Close(TextWriter? output)
        {
            foreach (var client in _clients.Values)
            {
                client.Dispose();
            }
            _clients.Clear();

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
            LogUtility.Log(output, LogLevel.Error, $"UdpListener error: {error.GetWhat()}");
        }

        public override void OnClose(TextWriter? output)
        {
            LogUtility.Log(output, LogLevel.Info, "UdpListener closed");
        }

        public override void Dispose()
        {
            foreach (var client in _clients.Values)
            {
                client.Dispose();
            }
            _clients.Clear();
            base.Dispose();
        }
    }
}
