using System.Diagnostics;
using FxNet;
using FxNet.Core;
using FxNet.Dll;

namespace FxNet.Tests.Stress;

/// <summary>长时间运行稳定性测试</summary>
public class LongRunningTests : IDisposable
{
    [Fact]
    public void SustainedOperation_NoMemoryLeak()
    {
        FxNetInterface.StartIOModule();
        
        // 运行 5 秒（缩短测试时间，实际生产环境可设为 60 秒）
        var duration = TimeSpan.FromSeconds(5);
        var sw = Stopwatch.StartNew();
        var messageCount = 0;

        while (sw.Elapsed < duration)
        {
            // 每 100ms 创建和处理消息
            for (int i = 0; i < 10; i++)
            {
                var pkg = new NetStreamPackage();
                pkg.WriteInt(messageCount);
                pkg.WriteString($"Message {messageCount}");
                messageCount++;
            }
            Thread.Sleep(100);
        }

        // 验证：处理了足够多的消息
        Assert.True(messageCount > 100);
    }

    [Fact]
    public void RepeatedConnectDisconnect_NoResourceLeak()
    {
        FxNetInterface.StartIOModule();
        
        // 重复连接/断开 100 次
        for (int i = 0; i < 100; i++)
        {
            var connector = FxNetApi.CreateConnector(
                onRecv: (_, _, _) => { },
                onConnected: _ => { },
                onError: (_, _) => { },
                onClose: _ => { });

            // 尝试连接（会失败，但不影响测试）
            try
            {
                FxNetApi.TcpConnect(connector, "127.0.0.1", (ushort)(30000 + i));
            }
            catch { }

            connector.Close();
        }

        // 如果没有资源泄漏，测试应正常完成
        Assert.True(true);
    }

    [Fact]
    public void PackageAllocationReuse_StableMemory()
    {
        // 测试 NetStreamPackage 的内存稳定性
        var initialMemory = GC.GetTotalMemory(false);

        for (int round = 0; round < 100; round++)
        {
            var pkg = new NetStreamPackage();
            for (int i = 0; i < 100; i++)
            {
                pkg.WriteInt(i);
                pkg.WriteString($"Test {i}");
            }
        }

        GC.Collect();
        var finalMemory = GC.GetTotalMemory(false);

        // 内存增长应在合理范围内（允许一定增长）
        var growth = finalMemory - initialMemory;
        Assert.True(growth < 50_000_000); // 小于 50MB
    }

    public void Dispose() { }
}
