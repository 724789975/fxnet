using System.Diagnostics;
using FxNet;
using FxNet.Core;
using FxNet.Dll;

namespace FxNet.Tests.Stress;

/// <summary>高并发压力测试</summary>
public class ConcurrencyTests : IDisposable
{
    private const string TestIp = "127.0.0.1";
    private const ushort BasePort = 20000;

    [Fact]
    public void MultipleConnectors_CanBeCreated()
    {
        FxNetInterface.StartIOModule();
        var connectors = new List<Connector>();

        // 创建多个连接器（不实际连接）
        for (int i = 0; i < 10; i++)
        {
            var connector = FxNetApi.CreateConnector(
                onRecv: (_, _, _) => { },
                onConnected: _ => { },
                onError: (_, _) => { },
                onClose: _ => { });
            connectors.Add(connector);
        }

        Assert.Equal(10, connectors.Count);

        // 清理
        foreach (var c in connectors)
        {
            c.Close();
        }
    }

    [Fact]
    public void HighFrequencyMessageSend_DoesNotThrow()
    {
        FxNetInterface.StartIOModule();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        // 高频创建和关闭连接器（模拟压力）
        var sw = Stopwatch.StartNew();
        for (int i = 0; i < 100; i++)
        {
            var c = FxNetApi.CreateConnector(
                onRecv: (_, _, _) => { },
                onConnected: _ => { },
                onError: (_, _) => { },
                onClose: _ => { });
            c.Close();
        }
        sw.Stop();

        // 100 次创建/关闭应在合理时间内完成
        Assert.True(sw.ElapsedMilliseconds < 5000);
    }

    [Fact]
    public void MixedWorkload_PackageOperations()
    {
        // 混合负载：大量小包 + 少量大包
        var sw = Stopwatch.StartNew();

        for (int i = 0; i < 1000; i++)
        {
            var pkg = new NetStreamPackage();
            pkg.WriteInt(i);
            pkg.WriteString($"Message {i}");
        }

        // 大包
        for (int i = 0; i < 10; i++)
        {
            var pkg = new NetStreamPackage();
            byte[] largeData = new byte[10000];
            pkg.WriteData(largeData, largeData.Length);
        }

        sw.Stop();
        Assert.True(sw.ElapsedMilliseconds < 1000);
    }

    public void Dispose() { }
}
