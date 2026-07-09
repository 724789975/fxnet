using System.Net;
using System.Net.Sockets;

namespace FxNet.Core
{
    /// <summary>
    /// Socket 抽象基类，封装 .NET Socket 句柄和 IO 模块绑定。
    /// 所有 TCP/UDP 的 Connector 和 Listener 均继承此类。
    /// 实现 IDisposable 以确保 Socket 和 SAEA 资源正确释放。
    /// </summary>
    public abstract class SocketBase : IDisposable
    {
        protected Socket? NativeSocketHandle;  // .NET Socket 句柄
        protected IPEndPoint LocalEndPoint = new IPEndPoint(IPAddress.Any, 0); // 本地端点
        protected ErrorCode Error = new ErrorCode(); // 错误状态
        protected uint IOModuleIndex; // 所属 IO 模块索引

        public Socket? GetSocket() => NativeSocketHandle;
        public void SetIOModuleIndex(uint index) => IOModuleIndex = index;
        public uint GetIOModuleIndex() => IOModuleIndex;

        public IPEndPoint GetLocalEndPoint() => LocalEndPoint;
        public int GetError() => Error.Code;

        public virtual string Name => "SocketBase";

        public abstract void Update(double time, ErrorCode error, TextWriter? output); // 周期性更新
        public abstract void Close(TextWriter? output); // 关闭 Socket
        public abstract void OnError(ErrorCode error, TextWriter? output); // 错误处理
        public abstract void OnClose(TextWriter? output); // 关闭回调

        /// <summary>释放资源，子类可重写以释放 SAEA 等额外资源</summary>
        public virtual void Dispose()
        {
            if (NativeSocketHandle != null)
            {
                try { NativeSocketHandle.Close(); } catch (Exception) { /* 忽略关闭异常 */ }
                NativeSocketHandle = null;
            }
        }
    }
}
