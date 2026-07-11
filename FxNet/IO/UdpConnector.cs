using FxNet.Core;
using FxNet.Util;
using System.Buffers;
using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;

namespace FxNet.IO
{
    /// <summary>
    /// UDP 连接器，集成 BufferContral 滑动窗口可靠传输。
    /// 客户端模式：通过非阻塞 Socket 的同步 Receive 轮询接收数据。
    /// 服务端模式：通过 UdpListener 的 EnqueuePendingRecv 接收数据。
    /// 所有 BufferContral 处理均在主线程 Update 中完成。
    /// </summary>
    public class UdpConnector : ConnectorSocket
    {
        private IPEndPoint _remoteEndPoint = new IPEndPoint(IPAddress.Any, 0);
        private readonly BufferContral _bufferContral;
        private readonly byte[] _recvBuffer = new byte[SlidingWindow.BuffSize];

        // 服务端模式：通过监听器的 Socket 发送数据
        private Socket? _listenerSocket;

        // 待处理接收数据队列（服务端模式由 UdpListener 入队，客户端模式由 Update 轮询入队）
        private readonly ConcurrentQueue<PendingRecvData> _pendingRecvQueue = new();

        private sealed class PendingRecvData
        {
            public byte[] Data = null!;
            public int Length;
        }

        // === BufferContral 回调实现 ===

        /// <summary>底层接收操作：从待处理队列中取出数据提供给 BufferContral</summary>
        private sealed class RecvOperator : IRecvOperator
        {
            private readonly UdpConnector _connector;
            public RecvOperator(UdpConnector connector) => _connector = connector;

            public void Execute(byte[] buffer, ushort buffSize, out int recvSize, ErrorCode error, TextWriter? output)
            {
                // 直接从队列中取下一个数据包
                if (_connector._pendingRecvQueue.TryDequeue(out var pending))
                {
                    int copyLen = Math.Min(pending.Length, buffSize);
                    Array.Copy(pending.Data, 0, buffer, 0, copyLen);
                    recvSize = copyLen;
                }
                else
                {
                    recvSize = 0; // 无数据
                }
            }
        }

        /// <summary>底层发送操作：通过 UDP Socket 发送原始数据</summary>
        private sealed class SendOperator : ISendOperator
        {
            private readonly UdpConnector _connector;
            public SendOperator(UdpConnector connector) => _connector = connector;

            public void Execute(byte[] buffer, ushort bufferSize, out int sendLen, ErrorCode error, TextWriter? output)
            {
                sendLen = 0;
                try
                {
                    if (_connector.NativeSocketHandle != null)
                    {
                        sendLen = _connector.NativeSocketHandle.Send(buffer, 0, bufferSize, SocketFlags.None);
                    }
                    else if (_connector._listenerSocket != null)
                    {
                        sendLen = _connector._listenerSocket.SendTo(buffer, 0, bufferSize, SocketFlags.None,
                            _connector._remoteEndPoint);
                    }
                }
                catch (Exception ex)
                {
                    error.Set(ex.HResult, $"UdpConnector:SendOperator {ex.Message}");
                }
            }
        }

        /// <summary>数据接收回调：将 BufferContral 处理后的有效数据投递到 Session</summary>
        private sealed class OnRecvOperator : IOnRecvOperator
        {
            private readonly UdpConnector _connector;
            public OnRecvOperator(UdpConnector connector) => _connector = connector;

            public void Execute(byte[] buffer, ushort size, ErrorCode error, TextWriter? output)
            {
                var session = _connector.Session;
                if (session == null || size == 0)
                {
                    return;
                }

                var recvBuff = session.GetRecvBuff();
                recvBuff.PushData(buffer, size);

                while (recvBuff.CheckPackage())
                {
                    var msgEvent = session.NewRecvMessageEvent();
                    recvBuff.PopData(msgEvent.Package);
                    msgEvent.Session = session;

                    var module = IoModule.GetInstance(_connector.IOModuleIndex);
                    module?.PushMessageEvent(msgEvent);
                }
            }
        }

        /// <summary>读流操作：每帧由 BufferContral.SendMessages 调用，将 sendBuff 数据搬入发送窗口</summary>
        private sealed class ReadStreamOperator : IReadStreamOperator
        {
            private readonly UdpConnector _connector;
            public ReadStreamOperator(UdpConnector connector) => _connector = connector;

            public uint Execute(TextWriter? output) => _connector.TryMoveSendBuffToWindow();
        }

        public UdpConnector(ISession? session) : base(session)
        {
            _bufferContral = new BufferContral();
            _bufferContral.SetRecvOperator(new RecvOperator(this));
            _bufferContral.SetSendOperator(new SendOperator(this));
            _bufferContral.SetOnRecvOperator(new OnRecvOperator(this));
            _bufferContral.SetReadStreamOperator(new ReadStreamOperator(this));
        }

        public override string Name => "UdpConnector";

        public IPEndPoint GetRemoteEndPoint() => _remoteEndPoint;
        public UdpConnector SetRemoteEndPoint(IPEndPoint ep) { _remoteEndPoint = ep; return this; }

        /// <summary>设置监听器 Socket（服务端模式，用于发送数据）</summary>
        public void SetListenerSocket(Socket socket) => _listenerSocket = socket;

        /// <summary>将接收数据加入待处理队列（供 UdpListener 调用）</summary>
        public void EnqueuePendingRecv(byte[] data, int len)
        {
            _pendingRecvQueue.Enqueue(new PendingRecvData { Data = data, Length = len });
        }

        public int GetPendingQueueCount() => _pendingRecvQueue.Count;

        /// <summary>初始化可靠传输控制状态</summary>
        public ErrorCode Init(TextWriter? output, int state)
        {
            _bufferContral.Init(state, FxNetInterface.GetNow());
            return new ErrorCode();
        }

        public override void Update(double time, ErrorCode error, TextWriter? output)
        {
            // 1. 客户端模式：从非阻塞 Socket 同步轮询接收数据
            if (NativeSocketHandle != null)
            {
                while (true)
                {
                    try
                    {
                        int received = NativeSocketHandle.Receive(_recvBuffer, 0, _recvBuffer.Length,
                            SocketFlags.None);
                        if (received <= 0) break;

                        var data = new byte[received];
                        Array.Copy(_recvBuffer, data, received);
                        _pendingRecvQueue.Enqueue(new PendingRecvData { Data = data, Length = received });
                    }
                    catch (SocketException ex) when (ex.SocketErrorCode == SocketError.WouldBlock)
                    {
                        break; // 无更多数据
                    }
                    catch (ObjectDisposedException)
                    {
                        break;
                    }
                    catch (Exception)
                    {
                        break;
                    }
                }
            }

            // 2. 处理所有待处理的接收数据
            // ReceiveMessages 内部 while(readable) 循环会通过 RecvOperator 从队列中取数据
            if (_pendingRecvQueue.Count > 0)
            {
                bool readable = true;
                _bufferContral.ReceiveMessages(time, ref readable, error, output);
                if (error)
                {
                    error.Set(0, "");
                }
            }

            // 3. 发送数据（keepalive、重传、新数据等）
            _bufferContral.SendMessages(time, error, output);
        }

        /// <summary>发起 UDP 连接：创建 Socket</summary>
        public UdpConnector Connect(IPEndPoint address, ErrorCode error, TextWriter? output)
        {
            try
            {
                NativeSocketHandle = new Socket(AddressFamily.InterNetwork, SocketType.Dgram, ProtocolType.Udp);
                NativeSocketHandle.Blocking = false;

                // 与 C++ 对齐：socket 缓冲区 256KB + TTL 128，避免默认 8KB 缓冲区在跨公网高并发下内核丢包
                NativeSocketHandle.ReceiveBufferSize = 256 * 1024;
                NativeSocketHandle.SendBufferSize = 256 * 1024;
                NativeSocketHandle.Ttl = 128;

                _remoteEndPoint = address;

                // 初始化 BufferContral 为 Established 状态
                Init(output, (int)ConnectionStatus.Established);

                var module = IoModule.GetInstance(IOModuleIndex);
                module?.RegisterSocket(NativeSocketHandle, this);

                // Connect UDP socket
                NativeSocketHandle.Connect(address);

                // UDP 无连接特性，连接设置完成后立即触发 OnConnected 事件
                var connectedEvent = Session?.NewConnectedEvent();
                if (connectedEvent != null)
                {
                    module?.PushMessageEvent(connectedEvent);
                }
            }
            catch (Exception ex)
            {
                error.Set(ex.HResult, $"UdpConnector:Connect {ex.Message}");
            }
            return this;
        }

        /// <summary>发送消息：将 sendBuff 数据搬入 BufferContral 滑动窗口（只弹出已入窗口部分，背压）</summary>
        public override void SendMessage(ErrorCode error, TextWriter? output)
        {
            if ((NativeSocketHandle == null && _listenerSocket == null) || Session == null) return;
            TryMoveSendBuffToWindow();
        }

        /// <summary>
        /// 将 sendBuff 数据搬入发送窗口。Send 只能入窗口容量内的数据，
        /// 未入窗口的数据保留在 sendBuff，仅弹出已入窗口字节数（对齐 C++ ReadStreamOperator 背压语义）。
        /// </summary>
        private uint TryMoveSendBuffToWindow()
        {
            var sendBuff = Session?.GetSendBuff();
            if (sendBuff == null || sendBuff.GetSize() == 0) return 0;

            int sendSize = sendBuff.GetSize();
            var data = ArrayPool<byte>.Shared.Rent(sendSize);
            try
            {
                Array.Copy(sendBuff.GetData(), data, sendSize);
                uint sent = _bufferContral.Send(data, (uint)sendSize);
                if (sent > 0) sendBuff.PopData((int)sent);
                return sent;
            }
            finally
            {
                ArrayPool<byte>.Shared.Return(data);
            }
        }

        public override void Close(TextWriter? output)
        {
            var module = IoModule.GetInstance(IOModuleIndex);
            if (NativeSocketHandle != null)
            {
                module?.DeregisterSocket(NativeSocketHandle);
                try { NativeSocketHandle.Close(); } catch (Exception) { }
                NativeSocketHandle = null;
            }
        }

        public override void OnError(ErrorCode error, TextWriter? output)
        {
            LogUtility.Log(output, LogLevel.Error, $"UdpConnector error: {error.GetWhat()}");
        }

        public override void OnClose(TextWriter? output)
        {
            LogUtility.Log(output, LogLevel.Info, "UdpConnector closed");
        }

        public override void Dispose()
        {
            base.Dispose();
        }
    }
}
