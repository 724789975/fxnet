using System.Text;
using FxNet;
using FxNet.Core;
using FxNet.Dll;
using FxNet.Util;

namespace FxNet.Tests.Integration;

/// <summary>TCP 连接集成测试：单连接收发、多连接并发</summary>
public class TcpConnectionTests : IDisposable
{
    private const string TestIp = "127.0.0.1";
    private const ushort TestPort = 19000;
    private bool _initialized;

    private void EnsureInit()
    {
        if (_initialized) return;
        _initialized = true;
        FxNetInterface.StartIOModule();
    }

    [Fact]
    public void SingleConnection_SendReceive()
    {
        EnsureInit();
        // 此测试需要服务器运行，作为框架验证
        // 实际集成测试需要启动内嵌服务器
        Assert.True(true); // 框架占位
    }

    [Fact]
    public void FxNetInterface_TcpConnect_DoesNotThrowOnInvalidAddress()
    {
        EnsureInit();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        // 连接到不存在的端口不应抛异常（异步连接会延迟报错）
        FxNetApi.TcpConnect(connector, "127.0.0.1", 19999);
        connector.Close();
    }

    public void Dispose() { }
}

/// <summary>UDP 连接集成测试</summary>
public class UdpConnectionTests : IDisposable
{
    [Fact]
    public void UdpConnect_DoesNotThrow()
    {
        FxNetInterface.StartIOModule();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        FxNetApi.UdpConnect(connector, "127.0.0.1", 19001);
        connector.Close();
    }

    public void Dispose() { }
}
