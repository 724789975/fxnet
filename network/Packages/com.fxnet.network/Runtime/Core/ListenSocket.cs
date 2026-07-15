using System.IO;

namespace FxNet.Core
{
    /// <summary>
    /// 监听 Socket 抽象基类，继承 SocketBase。
    /// TCP/UDP Listener 均继承此类。
    /// </summary>
    public abstract class ListenSocket : SocketBase
    {
        public override string Name => "ListenSocket";
    }
}
