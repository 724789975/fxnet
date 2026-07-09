using FxNet.Util;

namespace FxNet.Tests.Util;

/// <summary>Pool 对象池测试</summary>
public class PoolTests
{
    private class TestItem { public int Value; }

    [Fact]
    public void Allocate_ReturnsNonNull()
    {
        var pool = new Pool<TestItem>();
        var item = pool.Allocate();
        Assert.NotNull(item);
    }

    [Fact]
    public void AllocateFree_Allocate_ReturnsSameInstance()
    {
        var pool = new Pool<TestItem>();
        var item = pool.Allocate()!;
        item.Value = 42;
        pool.Free(item);

        var item2 = pool.Allocate()!;
        Assert.Same(item, item2);
        Assert.Equal(42, item2.Value);
    }

    [Fact]
    public void ExhaustPool_ReturnsNull()
    {
        var pool = new Pool<TestItem>();
        var items = new List<TestItem>();

        // 持续分配直到返回 null
        while (true)
        {
            var item = pool.Allocate();
            if (item == null) break;
            items.Add(item);
        }

        // 池应已耗尽，且分配了不超过 Size 个对象
        Assert.NotNull(items);
        Assert.True(items.Count > 0);
        Assert.True(items.Count <= Pool<TestItem>.Size);

        // 归还一个后可以再分配
        pool.Free(items[0]);
        Assert.NotNull(pool.Allocate());
    }

    [Fact]
    public void Free_UnknownItem_DoesNotThrow()
    {
        var pool = new Pool<TestItem>();
        var external = new TestItem();
        // Free 一个不在池中的对象，不应抛异常
        pool.Free(external);
    }
}

/// <summary>ErrorCode 测试</summary>
public class ErrorCodeTests
{
    [Fact]
    public void Default_IsNotError()
    {
        var error = new FxNet.Core.ErrorCode();
        Assert.False((bool)error);
        Assert.Equal(0, (int)error);
    }

    [Fact]
    public void SetCode_BecomesError()
    {
        var error = new FxNet.Core.ErrorCode(1, "test error");
        Assert.True((bool)error);
        Assert.Equal(1, (int)error);
        Assert.Contains("test error", error.GetWhat());
    }

    [Fact]
    public void Set_ResetsError()
    {
        var error = new FxNet.Core.ErrorCode(1, "error");
        error.Set(0, "");
        Assert.False((bool)error);
    }
}
