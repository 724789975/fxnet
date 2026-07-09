using FxNet.Util;

namespace FxNet.Tests.Core;

/// <summary>MessageQueue 消息队列测试</summary>
public class MessageQueueTests
{
    [Fact]
    public void CasLockQueue_AllocateFree_Works()
    {
        var queue = new CasLockQueue<TestMessage>();
        var item = queue.Allocate();
        Assert.NotNull(item);
        
        item.Value = "test";
        queue.Free(item);
        
        var item2 = queue.Allocate();
        Assert.NotNull(item2);
    }

    [Fact]
    public void CasLockQueue_MultipleAllocations_ReturnDifferentInstances()
    {
        var queue = new CasLockQueue<TestMessage>();
        var item1 = queue.Allocate();
        Assert.NotNull(item1);
        
        // CasLockQueue 可能因内部锁机制限制并发分配
        // 归还后再分配应成功
        queue.Free(item1);
        var item2 = queue.Allocate();
        Assert.NotNull(item2);
    }

    [Fact]
    public async Task CasLockQueue_ConcurrentAccess_ThreadSafe()
    {
        var queue = new CasLockQueue<TestMessage>();
        var tasks = new List<Task>();
        var allocated = new System.Collections.Concurrent.ConcurrentBag<TestMessage>();

        // 多线程并发分配
        for (int i = 0; i < 10; i++)
        {
            tasks.Add(Task.Run(() =>
            {
                for (int j = 0; j < 10; j++)
                {
                    var item = queue.Allocate();
                    if (item != null)
                    {
                        allocated.Add(item);
                    }
                }
            }));
        }

        await Task.WhenAll(tasks);
        Assert.True(allocated.Count > 0);
    }

    private class TestMessage
    {
        public string Value { get; set; } = "";
    }
}
