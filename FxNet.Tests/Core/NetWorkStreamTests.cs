using FxNet.Core;

namespace FxNet.Tests.Core;

/// <summary>TextWorkStream 粘包处理测试</summary>
public class NetWorkStreamTests
{
    [Fact]
    public void PushPop_BasicFlow()
    {
        var stream = new TextWorkStream();
        byte[] data = { 1, 2, 3, 4 };
        stream.PushData(data, data.Length);
        Assert.Equal(4, stream.GetSize());

        stream.PopData(2);
        Assert.Equal(2, stream.GetSize());
    }

    [Fact]
    public void PushData_TriggersExpansion()
    {
        var stream = new TextWorkStream(); // 默认 4096 容量
        byte[] data = new byte[100];
        stream.PushData(data, data.Length);
        Assert.Equal(100, stream.GetSize());
        Assert.True(stream.GetFreeSize() > 0);
    }

    [Fact]
    public void CheckPackage_IncompleteHeader_ReturnsFalse()
    {
        var stream = new TextWorkStream();
        // 只写入 3 字节（不足 4 字节头）
        byte[] partial = { 0, 0, 0 };
        stream.PushData(partial, 3);
        Assert.False(stream.CheckPackage());
    }

    [Fact]
    public void CheckPackage_CompletePackage_ReturnsTrue()
    {
        var stream = new TextWorkStream();
        // 写入长度头 = 5 + 5 字节数据
        var pkg = new NetStreamPackage();
        pkg.WriteInt(5); // 长度头
        pkg.WriteData(new byte[] { 1, 2, 3, 4, 5 }, 5);

        stream.PushData(pkg.GetData(), pkg.DataLength);
        Assert.True(stream.CheckPackage());
    }

    [Fact]
    public void PopData_EmptyStream_NoError()
    {
        var stream = new TextWorkStream();
        stream.PopData(0); // 不应抛异常
        Assert.Equal(0, stream.GetSize());
    }
}
