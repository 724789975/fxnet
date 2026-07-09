using FxNet.Core;

namespace FxNet.Tests.Core;

/// <summary>SlidingWindow 滑动窗口单元测试</summary>
public class SlidingWindowTests
{
    /// <summary>空窗口 Count 应返回 0</summary>
    [Fact]
    public void Count_EmptyWindow_ReturnsZero()
    {
        var window = new SlidingWindow();
        window.Begin = 0;
        window.End = 0;
        Assert.Equal(0, window.Count());
    }

    /// <summary>添加数据后 Count 应返回正确数量</summary>
    [Fact]
    public void Count_AfterAddition_ReturnsCorrectCount()
    {
        var window = new SlidingWindow();
        window.Begin = 5;
        window.End = 10;
        Assert.Equal(5, window.Count());
    }

    /// <summary>Begin 之前的序号应返回 false</summary>
    [Fact]
    public void IsValidIndex_BeforeBegin_ReturnsFalse()
    {
        var window = new SlidingWindow();
        window.Begin = 10;
        window.End = 20;

        Assert.False(window.IsValidIndex(5));
        Assert.False(window.IsValidIndex(9));
    }

    /// <summary>窗口范围内的序号应返回 true</summary>
    [Fact]
    public void IsValidIndex_WithinRange_ReturnsTrue()
    {
        var window = new SlidingWindow();
        window.Begin = 10;
        window.End = 20;

        Assert.True(window.IsValidIndex(10));
        Assert.True(window.IsValidIndex(15));
        Assert.True(window.IsValidIndex(19));
    }

    /// <summary>End 位置的序号应返回 false（半开区间）</summary>
    [Fact]
    public void IsValidIndex_AtEnd_ReturnsFalse()
    {
        var window = new SlidingWindow();
        window.Begin = 10;
        window.End = 20;

        Assert.False(window.IsValidIndex(20));
        Assert.False(window.IsValidIndex(25));
    }

    /// <summary>验证窗口常量定义正确</summary>
    [Fact]
    public void SlidingWindowDef_Constants_AreCorrect()
    {
        Assert.Equal(32, SlidingWindowDef.UdpWindowSize);
        Assert.Equal(1024, SlidingWindowDef.UdpWindowBuffSize);
        Assert.True(SlidingWindowDef.UdpSendFrequency > 0);
    }

    /// <summary>验证 SlidingWindow 缓冲区大小与常量一致</summary>
    [Fact]
    public void SlidingWindow_Buffers_MatchConstants()
    {
        var window = new SlidingWindow();
        Assert.Equal(SlidingWindow.WindowSize, window.Buffers.Length);

        for (int i = 0; i < window.Buffers.Length; i++)
        {
            Assert.Equal(SlidingWindow.BuffSize, window.Buffers[i].Length);
        }
    }

    /// <summary>验证序号数组长度与窗口大小一致</summary>
    [Fact]
    public void SlidingWindow_SeqArrays_MatchWindowSize()
    {
        var window = new SlidingWindow();
        Assert.Equal(SlidingWindow.WindowSize, window.SeqBufferId.Length);
        Assert.Equal(SlidingWindow.WindowSize, window.SeqSize.Length);
        Assert.Equal(SlidingWindow.WindowSize, window.SeqTime.Length);
        Assert.Equal(SlidingWindow.WindowSize, window.SeqRetry.Length);
        Assert.Equal(SlidingWindow.WindowSize, window.SeqRetryTime.Length);
        Assert.Equal(SlidingWindow.WindowSize, window.SeqRetryCount.Length);
    }

    /// <summary>验证 SendWindow 继承自 SlidingWindow</summary>
    [Fact]
    public void SendWindow_InheritsFromSlidingWindow()
    {
        var sendWindow = new SendWindow();
        Assert.IsAssignableFrom<SlidingWindow>(sendWindow);
    }

    /// <summary>验证 SendWindow.Add2SendWindow 返回 this 支持链式调用</summary>
    [Fact]
    public void SendWindow_Add2SendWindow_ReturnsThis()
    {
        var sendWindow = new SendWindow();
        sendWindow.ClearBuffer();
        sendWindow.Begin = 1;
        sendWindow.End = 1;

        byte bufferId = sendWindow.FreeBufferId;
        sendWindow.FreeBufferId = sendWindow.Buffers[bufferId][0];

        var result = sendWindow.Add2SendWindow(0, bufferId, 100, 1.0, 0.01);
        Assert.Same(sendWindow, result);
    }

    /// <summary>验证窗口序号回绕时的 IsValidIndex 行为（byte 溢出场景）</summary>
    [Fact]
    public void IsValidIndex_WrapAround_WorksCorrectly()
    {
        var window = new SlidingWindow();
        // 模拟序号接近 byte.MaxValue 后回绕
        window.Begin = 250;
        window.End = 5; // 回绕后 End < Begin（byte 运算）

        // Begin 到 End 之间的序号应有效（通过 byte 减法）
        Assert.True(window.IsValidIndex(250));
        Assert.True(window.IsValidIndex(255));
        Assert.True(window.IsValidIndex(0));
        Assert.True(window.IsValidIndex(4));

        // 范围外的序号应无效
        Assert.False(window.IsValidIndex(249));
        Assert.False(window.IsValidIndex(10));
    }

    /// <summary>验证 ClearBuffer 后所有缓冲区首字节形成正确链表</summary>
    [Fact]
    public void ClearBuffer_ChainLinks_AllBuffers()
    {
        var window = new SlidingWindow();
        window.ClearBuffer();

        // 从 FreeBufferId 开始遍历链表，应访问所有缓冲区
        var visited = new bool[SlidingWindow.WindowSize];
        byte current = window.FreeBufferId;
        int count = 0;

        while (count < SlidingWindow.WindowSize)
        {
            Assert.True(current < SlidingWindow.WindowSize, $"缓冲区索引 {current} 超出范围");
            Assert.False(visited[current], $"缓冲区 {current} 被重复访问");
            visited[current] = true;
            current = window.Buffers[current][0];
            count++;
        }

        // 所有缓冲区都应被访问
        Assert.Equal(SlidingWindow.WindowSize, count);
        Assert.All(visited, v => Assert.True(v));
    }
}
