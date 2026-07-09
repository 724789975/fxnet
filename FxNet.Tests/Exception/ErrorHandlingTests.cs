using FxNet;
using FxNet.Core;
using FxNet.Dll;

namespace FxNet.Tests.Exception;

/// <summary>异常和错误处理测试</summary>
public class ErrorHandlingTests : IDisposable
{
    [Fact]
    public void ConnectToInvalidPort_DoesNotThrowImmediately()
    {
        FxNetInterface.StartIOModule();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        // 连接到不存在的端口不应立即抛异常（异步连接）
        var ex = Record.Exception(() =>
        {
            FxNetApi.TcpConnect(connector, "127.0.0.1", 59999);
        });
        Assert.Null(ex);
        connector.Close();
    }

    [Fact]
    public void ConnectWithNullAddress_HandledGracefully()
    {
        FxNetInterface.StartIOModule();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        // 空地址可能被异步处理，不会立即抛异常
        // 验证不会崩溃
        var ex = Record.Exception(() =>
        {
            try
            {
                FxNetApi.TcpConnect(connector, null!, 8080);
            }
            catch (ArgumentNullException) { }
            catch (ArgumentException) { }
        });
        Assert.Null(ex);
        connector.Close();
    }

    [Fact]
    public void CloseConnector_MultipleTimes_DoesNotThrow()
    {
        FxNetInterface.StartIOModule();
        var connector = FxNetApi.CreateConnector(
            onRecv: (_, _, _) => { },
            onConnected: _ => { },
            onError: (_, _) => { },
            onClose: _ => { });

        // 多次关闭不应抛异常
        var ex = Record.Exception(() =>
        {
            connector.Close();
            connector.Close();
            connector.Close();
        });
        Assert.Null(ex);
    }

    [Fact]
    public void NetStreamPackage_ReadBeyondData_ReturnsFalse()
    {
        var pkg = new NetStreamPackage();
        // 尝试从空包读取
        Assert.False(pkg.ReadByte(out byte _));
        Assert.False(pkg.ReadInt(out int _));
        Assert.False(pkg.ReadString(out string _));
    }

    [Fact]
    public void FxNetException_CanBeCreated()
    {
        var error = new FxNet.Core.ErrorCode(1, "Test error");
        var ex = new FxNetException(error);
        
        Assert.NotNull(ex);
        // FxNetException.Message 格式包含错误码信息
        Assert.Contains("Test error", ex.Message);
    }

    public void Dispose() { }
}
