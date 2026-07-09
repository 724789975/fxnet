using FxNet.Core;
using FxNet.IO;
using FxNet.Util;

namespace FxNet.Tests.Core;

/// <summary>BufferContral 可靠 UDP 传输单元测试</summary>
public class BufferContralTests : IDisposable
{
    private bool _ioInitialized;

    private void EnsureIoModule()
    {
        if (_ioInitialized) return;
        _ioInitialized = true;
        FxNetInterface.StartIOModule();
    }

    /// <summary>验证 SlidingWindow 初始化后 Begin/End/Count 值正确</summary>
    [Fact]
    public void SlidingWindow_Init_SetsCorrectDefaults()
    {
        var window = new SlidingWindow();
        Assert.Equal(0, window.Begin);
        Assert.Equal(0, window.End);
        Assert.Equal(0, window.Count());
        Assert.Equal(SlidingWindow.WindowSize, window.Buffers.Length);
    }

    /// <summary>验证空闲缓冲区链表初始化后能正确遍历</summary>
    [Fact]
    public void SlidingWindow_ClearBuffer_FreeBufferIdChainCorrect()
    {
        var window = new SlidingWindow();
        window.ClearBuffer();

        byte count = 0;
        byte current = window.FreeBufferId;
        var visited = new HashSet<byte>();

        while (current < SlidingWindow.WindowSize && !visited.Contains(current))
        {
            visited.Add(current);
            current = window.Buffers[current][0];
            count++;
        }

        Assert.Equal(SlidingWindow.WindowSize, count);
    }

    /// <summary>验证 IsValidIndex 对边界序号的判断</summary>
    [Fact]
    public void SlidingWindow_IsValidIndex_BoundaryCheck()
    {
        var window = new SlidingWindow();
        window.Begin = 5;
        window.End = 10;

        Assert.False(window.IsValidIndex(3));
        Assert.True(window.IsValidIndex(5));
        Assert.True(window.IsValidIndex(7));
        Assert.True(window.IsValidIndex(9));
        Assert.False(window.IsValidIndex(10));
    }

    /// <summary>验证 SendWindow.Add2SendWindow 后 End 指针前移、SeqBufferId 映射正确</summary>
    [Fact]
    public void SendWindow_Add2SendWindow_AdvancesEnd()
    {
        var window = new SendWindow();
        window.ClearBuffer();
        window.Begin = 1;
        window.End = 1;

        byte id = (byte)(window.End % SlidingWindow.WindowSize);
        byte bufferId = window.FreeBufferId;
        window.FreeBufferId = window.Buffers[bufferId][0];

        window.Add2SendWindow(id, bufferId, 100, 1.0, 0.01);

        Assert.Equal(2, window.End);
        Assert.Equal(bufferId, window.SeqBufferId[id]);
        Assert.Equal(100, window.SeqSize[id]);
        Assert.Equal(1, window.Count());
    }

    /// <summary>验证 Init(Established) 后状态正确</summary>
    [Fact]
    public void BufferContral_Init_EstablishedState_SetsConnected()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        int result = bc.Init((int)ConnectionStatus.Established, FxNetInterface.GetNow());

        Assert.Equal(0, result);
        Assert.Equal((int)ConnectionStatus.Established, bc.GetState());
    }

    /// <summary>验证 Init(Idle) 后状态为 Idle</summary>
    [Fact]
    public void BufferContral_Init_IdleState_NotConnected()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        int result = bc.Init((int)ConnectionStatus.Idle, FxNetInterface.GetNow());

        Assert.Equal(0, result);
        Assert.Equal((int)ConnectionStatus.Idle, bc.GetState());
    }

    /// <summary>验证 3 字节包头的序列化/反序列化</summary>
    [Fact]
    public void UDPPacketHeader_WriteRead_RoundTrips()
    {
        byte[] buffer = new byte[10];
        var original = new UDPPacketHeader
        {
            Status = (byte)ConnectionStatus.Established,
            Syn = 42,
            Ack = 17
        };

        original.WriteTo(buffer, 0);

        var readBack = new UDPPacketHeader();
        readBack.ReadFrom(buffer, 0);

        Assert.Equal(original.Status, readBack.Status);
        Assert.Equal(original.Syn, readBack.Syn);
        Assert.Equal(original.Ack, readBack.Ack);
    }

    /// <summary>验证 UDPPacketHeader 在不同偏移量下读写正确</summary>
    [Fact]
    public void UDPPacketHeader_WriteRead_WithOffset()
    {
        byte[] buffer = new byte[20];
        int offset = 5;
        var original = new UDPPacketHeader { Status = 4, Syn = 200, Ack = 100 };

        original.WriteTo(buffer, offset);

        var readBack = new UDPPacketHeader();
        readBack.ReadFrom(buffer, offset);

        Assert.Equal(original.Status, readBack.Status);
        Assert.Equal(original.Syn, readBack.Syn);
        Assert.Equal(original.Ack, readBack.Ack);
    }

    /// <summary>验证 Send() 将数据写入窗口缓冲区</summary>
    [Fact]
    public void BufferContral_Send_FillsWindow()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        bc.Init((int)ConnectionStatus.Established, FxNetInterface.GetNow());

        byte[] testData = new byte[100];
        for (int i = 0; i < testData.Length; i++)
            testData[i] = (byte)(i % 256);

        uint written = bc.Send(testData, (uint)testData.Length);

        Assert.True(written > 0);
        Assert.True(written <= testData.Length);
    }

    /// <summary>验证窗口满时 Send 返回已写入字节数小于请求字节数</summary>
    [Fact]
    public void BufferContral_Send_WindowFull_StopsAccepting()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        bc.Init((int)ConnectionStatus.Established, FxNetInterface.GetNow());

        byte[] largeData = new byte[33 * 1024];

        uint written = bc.Send(largeData, (uint)largeData.Length);

        Assert.True(written < largeData.Length,
            $"窗口满时应停止接受：written={written}, requested={largeData.Length}");
    }

    /// <summary>验证 Init 后发送窗口初始状态</summary>
    [Fact]
    public void BufferContral_Init_SendWindowInitialState()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        bc.Init((int)ConnectionStatus.Established, FxNetInterface.GetNow());

        byte[] data = new byte[50];
        uint written = bc.Send(data, (uint)data.Length);
        Assert.Equal(50u, written);
    }

    /// <summary>验证多次 Send 累积填充窗口</summary>
    [Fact]
    public void BufferContral_MultipleSend_AccumulatesInWindow()
    {
        EnsureIoModule();
        var bc = new BufferContral();
        bc.Init((int)ConnectionStatus.Established, FxNetInterface.GetNow());

        byte[] data1 = new byte[500];
        byte[] data2 = new byte[500];

        uint written1 = bc.Send(data1, (uint)data1.Length);
        uint written2 = bc.Send(data2, (uint)data2.Length);

        Assert.Equal(500u, written1);
        Assert.Equal(500u, written2);
    }

    public void Dispose() { }
}