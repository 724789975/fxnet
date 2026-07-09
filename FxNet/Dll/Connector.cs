using FxNet.Core;
using FxNet.IO;
using FxNet.Util;
using System.Net;

namespace FxNet.Dll
{
    /// <summary>数据接收回调委托</summary>
    public delegate void OnRecvCallback(Connector connector, byte[] data, int len);
    /// <summary>连接建立回调委托</summary>
    public delegate void OnConnectedCallback(Connector connector);
    /// <summary>错误回调委托</summary>
    public delegate void OnErrorCallback(Connector connector, int error);
    /// <summary>连接关闭回调委托</summary>
    public delegate void OnCloseCallback(Connector connector);
    /// <summary>日志回调委托</summary>
    public delegate void OnLogCallback(string log, int len);

    /// <summary>
    /// 连接器封装类，对外提供简洁的连接/发送/关闭 API。
    /// 内部持有 ISession，通过回调委托将事件通知上层。
    /// </summary>
    public class Connector
    {
        public ISession? Session { get; private set; }

        private readonly OnRecvCallback _onRecv;
        private readonly OnConnectedCallback _onConnected;
        private readonly OnErrorCallback _onError;
        private readonly OnCloseCallback _onClose;

        public Connector(OnRecvCallback onRecv, OnConnectedCallback onConnected,
            OnErrorCallback onError, OnCloseCallback onClose)
        {
            _onRecv = onRecv;
            _onConnected = onConnected;
            _onError = onError;
            _onClose = onClose;
        }

        public void UdpConnect(string ip, ushort port)
        {
            FxNetInterface.UdpConnect(0, ip, port, Session!, null);
        }

        public void TcpConnect(string ip, ushort port)
        {
            FxNetInterface.TcpConnect(0, ip, port, Session!, null);
        }

        public void Send(byte[] data, int len)
        {
            Session?.Send(data, len, null);
        }

        public void Close()
        {
            Session?.GetSocket()?.Close(null);
        }

        public ISession? GetSession() => Session;

        internal void SetSession(ISession session) => Session = session;

        internal void InvokeOnRecv(byte[] data, int len) => _onRecv(this, data, len);
        internal void InvokeOnConnected() => _onConnected(this);
        internal void InvokeOnError(int error) => _onError(this, error);
        internal void InvokeOnClose() => _onClose(this);
    }

    /// <summary>
    /// FxNet 对外 API 静态类，提供简化的网络操作接口。
    /// 封装了 Connector/SessionMaker 的创建和管理。
    /// </summary>
    public static class FxNetApi
    {
        private static ISessionMaker? _defaultSessionMaker;
        private static OnLogCallback? _logCallback;

        public static Connector CreateConnector(
            OnRecvCallback onRecv,
            OnConnectedCallback onConnected,
            OnErrorCallback onError,
            OnCloseCallback onClose)
        {
            var connector = new Connector(onRecv, onConnected, onError, onClose);
            var session = new TextSession(connector, onRecv, onConnected, onError, onClose);
            connector.SetSession(session);
            return connector;
        }

        public static void CreateSessionMaker(
            OnRecvCallback onRecv,
            OnConnectedCallback onConnected,
            OnErrorCallback onError,
            OnCloseCallback onClose)
        {
            _defaultSessionMaker = new TextSessionMaker(onRecv, onConnected, onError, onClose);
        }

        public static void DestroyConnector(Connector connector)
        {
            connector.Close();
        }

        public static void UdpConnect(Connector connector, string ip, ushort port)
        {
            connector.UdpConnect(ip, port);
        }

        public static void TcpConnect(Connector connector, string ip, ushort port)
        {
            connector.TcpConnect(ip, port);
        }

        public static void TcpListen(string ip, ushort port)
        {
            if (_defaultSessionMaker == null) return;
            FxNetInterface.TcpListen(0, ip, port, _defaultSessionMaker, null);
        }

        public static void UdpListen(string ip, ushort port)
        {
            if (_defaultSessionMaker == null) return;
            FxNetInterface.UdpListen(0, ip, port, _defaultSessionMaker, null);
        }

        public static void Send(Connector connector, byte[] data, int len)
        {
            connector.Send(data, len);
        }

        public static void Close(Connector connector)
        {
            connector.Close();
        }

        public static void StartIOModule()
        {
            FxNetInterface.StartIOModule();
        }

        public static void ProcessIOModule()
        {
            FxNetInterface.ProcSingleThread();
        }

        public static void SetLogCallback(OnLogCallback onLog)
        {
            _logCallback = onLog;
        }
    }

    /// <summary>
    /// 文本会话工厂，用于监听器接受新连接时创建 TextSession。
    /// </summary>
    internal class TextSessionMaker : ISessionMaker
    {
        private readonly OnRecvCallback _onRecv;
        private readonly OnConnectedCallback _onConnected;
        private readonly OnErrorCallback _onError;
        private readonly OnCloseCallback _onClose;

        public TextSessionMaker(OnRecvCallback onRecv, OnConnectedCallback onConnected,
            OnErrorCallback onError, OnCloseCallback onClose)
        {
            _onRecv = onRecv;
            _onConnected = onConnected;
            _onError = onError;
            _onClose = onClose;
        }

        public ISession Create()
        {
            var connector = new Connector(_onRecv, _onConnected, _onError, _onClose);
            var session = new TextSession(connector, _onRecv, _onConnected, _onError, _onClose);
            connector.SetSession(session);
            return session;
        }
    }
}
