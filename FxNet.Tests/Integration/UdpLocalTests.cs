using System.Text;
using System.Buffers.Binary;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.IO;
using FxNet.Util;

namespace FxNet.Tests.Integration;

/// <summary>UDP 本地回环集成测试：在同一进程内启动 UDP 监听 + UDP 连接，验证消息收发</summary>
public class UdpLocalTests : IDisposable
{
    private const string TestIp = "127.0.0.1";
    private const ushort TestPort = 19200;
    private static readonly object _initLock = new();
    private static bool _ioInitialized;

    private readonly List<Connector> _clientConnectors = new();

    private void EnsureInit()
    {
        if (_ioInitialized) return;
        lock (_initLock)
        {
            if (_ioInitialized) return;
            _ioInitialized = true;
            FxNetInterface.StartIOModule();
        }
    }

    /// <summary>启动 UDP 回显服务器（类似 UdpServer 的会话逻辑）</summary>
    private void StartEchoServer()
    {
        FxNetInterface.UdpListen(0, TestIp, TestPort, new UdpEchoSessionMaker());
    }

    /// <summary>创建客户端连接器</summary>
    private Connector CreateClientConnector(
        Action<byte[], int>? onRecv = null,
        ManualResetEventSlim? connectedEvent = null)
    {
        var connector = FxNetApi.CreateConnector(
            onRecv: (c, data, len) =>
            {
                onRecv?.Invoke(data, len);
            },
            onConnected: c => connectedEvent?.Set(),
            onError: (_, _) => { },
            onClose: _ => { });

        _clientConnectors.Add(connector);
        return connector;
    }

    /// <summary>等待条件满足或超时</summary>
    private static bool WaitFor(Func<bool> condition, int timeoutMs, int pumpIntervalMs = 5)
    {
        double end = FxNetInterface.GetNow() + timeoutMs / 1000.0;
        while (FxNetInterface.GetNow() < end)
        {
            if (condition()) return true;
            FxNetInterface.ProcSingleThread();
            Thread.Sleep(pumpIntervalMs);
        }
        return condition();
    }

    /// <summary>UDP 连接后发送数据，验证对端收到回显</summary>
    [Fact]
    public void UdpLoopback_ConnectAndSend()
    {
        EnsureInit();
        StartEchoServer();

        var received = new ManualResetEventSlim(false);
        byte[]? receivedData = null;
        int receivedLen = 0;

        var connector = CreateClientConnector(
            onRecv: (data, len) =>
            {
                receivedData = new byte[len];
                Array.Copy(data, receivedData, len);
                receivedLen = len;
                received.Set();
            });

        FxNetApi.UdpConnect(connector, TestIp, TestPort);

        // UDP 连接后立即发送（UDP 是无连接的，BC 已初始化为 Established）
        string message = "Hello, UDP Echo!";
        byte[] sendData = Encoding.UTF8.GetBytes(message);
        connector.Send(sendData, sendData.Length);

        // 驱动事件循环，等待回显（UDP 可靠传输需要多次 ProcSingleThread）
        Assert.True(WaitFor(() => received.IsSet, 10000), "等待 UDP 回显超时");

        Assert.True(receivedLen > 0, "应收到回显数据");
        string receivedMessage = Encoding.UTF8.GetString(receivedData!, 0, receivedLen);
        Assert.Equal(message, receivedMessage);
    }

    /// <summary>发送多个包，验证按序交付</summary>
    [Fact]
    public void UdpLoopback_MultiplePackets_InOrder()
    {
        EnsureInit();
        StartEchoServer();

        var receivedMessages = new List<string>();
        var allReceived = new ManualResetEventSlim(false);
        const int packetCount = 5;

        var connector = CreateClientConnector(
            onRecv: (data, len) =>
            {
                string msg = Encoding.UTF8.GetString(data, 0, len);
                lock (receivedMessages)
                {
                    receivedMessages.Add(msg);
                    if (receivedMessages.Count >= packetCount)
                        allReceived.Set();
                }
            });

        FxNetApi.UdpConnect(connector, TestIp, TestPort);

        // 发送多个包
        for (int i = 0; i < packetCount; i++)
        {
            string msg = $"udp-pkt-{i + 1}";
            byte[] data = Encoding.UTF8.GetBytes(msg);
            connector.Send(data, data.Length);
            Thread.Sleep(20); // 间隔发送，确保每个包独立处理
        }

        // 等待所有回显
        Assert.True(WaitFor(() => allReceived.IsSet, 15000),
            $"等待 UDP 回显超时，已收到 {receivedMessages.Count}/{packetCount}");

        Assert.Equal(packetCount, receivedMessages.Count);
        for (int i = 0; i < packetCount; i++)
        {
            Assert.Equal($"udp-pkt-{i + 1}", receivedMessages[i]);
        }
    }

    /// <summary>发送空消息，验证不崩溃</summary>
    [Fact]
    public void UdpLoopback_EmptyMessage_Handled()
    {
        EnsureInit();
        StartEchoServer();

        var connector = CreateClientConnector();
        FxNetApi.UdpConnect(connector, TestIp, TestPort);

        // 发送空消息不应崩溃
        byte[] emptyData = Array.Empty<byte>();
        var ex = Record.Exception(() =>
        {
            connector.Send(emptyData, 0);
        });
        Assert.Null(ex);

        // 驱动事件循环，验证连接仍然存活
        PumpEvents(2000);
    }

    /// <summary>驱动事件循环指定毫秒数</summary>
    private static void PumpEvents(int milliseconds)
    {
        double end = FxNetInterface.GetNow() + milliseconds / 1000.0;
        while (FxNetInterface.GetNow() < end)
        {
            FxNetInterface.ProcSingleThread();
            Thread.Sleep(1);
        }
    }

    public void Dispose()
    {
        foreach (var c in _clientConnectors)
        {
            try { c.Close(); } catch { }
        }
        _clientConnectors.Clear();
    }

    // ======================== UDP 回显服务器实现 ========================

    /// <summary>UDP 回显会话工厂</summary>
    private class UdpEchoSessionMaker : ISessionMaker
    {
        public ISession Create() => new UdpEchoSession();
    }

    /// <summary>UDP 回显会话：收到数据后原样回显</summary>
    private class UdpEchoSession : ISession
    {
        private ConnectorSocket? _socket;
        private readonly TextWorkStream _sendBuff = new();
        private readonly TextWorkStream _recvBuff = new();

        public void SetSocket(ConnectorSocket? socket) => _socket = socket;
        public ConnectorSocket? GetSocket() => _socket;
        public NetWorkStream GetSendBuff() => _sendBuff;
        public NetWorkStream GetRecvBuff() => _recvBuff;

        public ISession Send(byte[] data, int len, TextWriter? output)
        {
            // 写入 4 字节大端长度头 + 数据体
            var header = new byte[4];
            BinaryPrimitives.WriteInt32BigEndian(header, len);
            _sendBuff.PushData(header, 4);
            _sendBuff.PushData(data, len);
            _socket?.SendMessage(new ErrorCode(), output);
            return this;
        }

        public ISession OnRecv(NetStreamPackage package, TextWriter? output)
        {
            byte[] data = package.GetData();
            int offset = package.GetOffset();
            int len = package.DataLength;

            // 回显：原样发回
            byte[] echoData = data.AsSpan(offset, len).ToArray();
            Send(echoData, echoData.Length, output);
            return this;
        }

        public void OnConnected(TextWriter? output) { }
        public void OnError(ErrorCode error, TextWriter? output) { }
        public void OnClose(TextWriter? output) { }

        public MessageRecvEventBase NewRecvMessageEvent() => new RecvEvent(this);
        public MessageEventBase NewConnectedEvent() => new ConnectedEvt(this);
        public MessageEventBase NewErrorEvent(ErrorCode error) => new ErrorEvt(this, error);
        public MessageEventBase NewCloseEvent() => new CloseEvt(this);
        public MessageEventBase NewOnSendEvent(int len) => new OnSendEvt();

        private class RecvEvent : MessageRecvEventBase
        {
            private readonly ISession _session;
            public RecvEvent(ISession session) { _session = session; Session = session; }
            public override void Execute(TextWriter? output) => _session.OnRecv(Package, output);
        }
        private class ConnectedEvt : MessageEventBase
        {
            private readonly ISession _session;
            public ConnectedEvt(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnConnected(output);
        }
        private class ErrorEvt : MessageEventBase
        {
            private readonly ISession _session;
            private readonly ErrorCode _error;
            public ErrorEvt(ISession session, ErrorCode error) { _session = session; _error = error; }
            public override void Execute(TextWriter? output) => _session.OnError(_error, output);
        }
        private class CloseEvt : MessageEventBase
        {
            private readonly ISession _session;
            public CloseEvt(ISession session) => _session = session;
            public override void Execute(TextWriter? output) => _session.OnClose(output);
        }
        private class OnSendEvt : MessageEventBase
        {
            public override void Execute(TextWriter? output) { }
        }
    }
}
