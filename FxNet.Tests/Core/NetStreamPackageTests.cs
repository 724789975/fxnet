using FxNet.Core;

namespace FxNet.Tests.Core;

/// <summary>NetStreamPackage 单元测试：读写各种数据类型、扩容、边界条件</summary>
public class NetStreamPackageTests
{
    [Fact]
    public void WriteReadByte_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteByte(42);
        pkg.WriteByte(255);

        Assert.True(pkg.ReadByte(out byte v1));
        Assert.Equal(42, v1);
        Assert.True(pkg.ReadByte(out byte v2));
        Assert.Equal(255, v2);
    }

    [Fact]
    public void WriteReadShort_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteShort((short)-1234);
        pkg.WriteShort(ushort.MaxValue);

        Assert.True(pkg.ReadShort(out short v1));
        Assert.Equal(-1234, v1);
        Assert.True(pkg.ReadShort(out ushort v2));
        Assert.Equal(ushort.MaxValue, v2);
    }

    [Fact]
    public void WriteReadInt_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteInt(int.MinValue);
        pkg.WriteInt(uint.MaxValue);

        Assert.True(pkg.ReadInt(out int v1));
        Assert.Equal(int.MinValue, v1);
        Assert.True(pkg.ReadInt(out uint v2));
        Assert.Equal(uint.MaxValue, v2);
    }

    [Fact]
    public void WriteReadInt64_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteInt64(long.MinValue);
        pkg.WriteInt64(ulong.MaxValue);

        Assert.True(pkg.ReadInt64(out long v1));
        Assert.Equal(long.MinValue, v1);
        Assert.True(pkg.ReadInt64(out ulong v2));
        Assert.Equal(ulong.MaxValue, v2);
    }

    [Fact]
    public void WriteReadString_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteString("Hello, 世界!");
        pkg.WriteString("");
        pkg.WriteString(new string('A', 1000));

        Assert.True(pkg.ReadString(out string s1));
        Assert.Equal("Hello, 世界!", s1);
        Assert.True(pkg.ReadString(out string s2));
        Assert.Equal("", s2);
        Assert.True(pkg.ReadString(out string s3));
        Assert.Equal(1000, s3.Length);
    }

    [Fact]
    public void WriteReadFloat_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        pkg.WriteFloat(3.14f);
        pkg.WriteFloat(-1.0f);

        Assert.True(pkg.ReadFloat(out float v1));
        Assert.Equal(3.14f, v1, 0.01f); // 定点数精度有限
        Assert.True(pkg.ReadFloat(out float v2));
        Assert.Equal(-1.0f, v2, 0.01f);
    }

    [Fact]
    public void WriteReadData_RoundTrips()
    {
        var pkg = new NetStreamPackage();
        byte[] original = { 1, 2, 3, 4, 5 };
        pkg.WriteData(original, original.Length);

        byte[] readBack = new byte[5];
        Assert.True(pkg.ReadData(readBack, 5));
        Assert.Equal(original, readBack);
    }

    [Fact]
    public void ReadFromEmpty_ReturnsFalse()
    {
        var pkg = new NetStreamPackage();
        Assert.False(pkg.ReadByte(out byte _));
        Assert.False(pkg.ReadShort(out short _));
        Assert.False(pkg.ReadInt(out int _));
        Assert.False(pkg.ReadInt64(out long _));
        Assert.False(pkg.ReadString(out string _));
    }

    [Fact]
    public void LargeData_TriggersExpansion()
    {
        var pkg = new NetStreamPackage();
        byte[] largeData = new byte[8192]; // 超过初始 2KB
        Random.Shared.NextBytes(largeData);
        pkg.WriteData(largeData, largeData.Length);

        byte[] readBack = new byte[8192];
        Assert.True(pkg.ReadData(readBack, 8192));
        Assert.Equal(largeData, readBack);
    }

    [Fact]
    public void ConstructFromExistingData()
    {
        byte[] data = { 0, 0, 0, 5, 72, 101, 108, 108, 111 }; // length=5 + "Hello"
        var pkg = new NetStreamPackage(data, data.Length);

        Assert.Equal(data.Length, pkg.DataLength);
    }
}
