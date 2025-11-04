using System;
using System.Runtime.InteropServices;

namespace fxnetlib.dllimport
{
    public class DLLImport
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnRecvCallback(IntPtr pConnector, [MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 2)] byte[] pData, uint nLen);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnConnectedCallback(IntPtr pConnector);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnErrorCallback(IntPtr pConnector, IntPtr nLen);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnCloseCallback(IntPtr pConnector);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnLogCallback([MarshalAs(UnmanagedType.LPArray, SizeParamIndex = 1)] byte[] pLog, int nLen);

        [DllImport("fxnet", EntryPoint = "CreateConnector", CharSet = CharSet.Ansi)]
        public static extern IntPtr CreateConnector(OnRecvCallback onRecv, OnConnectedCallback onConnected, OnErrorCallback onError, OnCloseCallback onClose);
        [DllImport("fxnet", EntryPoint = "DestroyConnector", CharSet = CharSet.Ansi)]
        public static extern void DestroyConnector(IntPtr pConnector);
        [DllImport("fxnet", EntryPoint = "CreateSessionMake", CharSet = CharSet.Ansi)]
        public static extern void CreateSessionMake(OnRecvCallback onRecv, OnConnectedCallback onConnected, OnErrorCallback onError, OnCloseCallback onClose);
        [DllImport("fxnet", EntryPoint = "TcpListen", CharSet = CharSet.Ansi)]
        public static extern void TcpListen(string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "UdpListen", CharSet = CharSet.Ansi)]
        public static extern void UdpListen(string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "UdpConnect", CharSet = CharSet.Ansi)]
        public static extern void UdpConnect(IntPtr pConnector, string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "TcpConnect", CharSet = CharSet.Ansi)]
        public static extern void TcpConnect(IntPtr pConnector, string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "Send", CharSet = CharSet.Ansi)]
        public static extern void Send(IntPtr pConnector, byte[] szData, UInt32 dwLen);
        [DllImport("fxnet", EntryPoint = "Close", CharSet = CharSet.Ansi)]
        public static extern void Close(IntPtr pConnector);
        [DllImport("fxnet", EntryPoint = "StartIOModule", CharSet = CharSet.Ansi)]
        public static extern void StartIOModule();
        [DllImport("fxnet", EntryPoint = "ProcessIOModule", CharSet = CharSet.Ansi)]
        public static extern void ProcessIOModule();
        [DllImport("fxnet", EntryPoint = "SetLogCallback", CharSet = CharSet.Ansi)]
        public static extern void SetLogCallback(OnLogCallback onLogCallback);
#if UNITY_EDITOR
        [DllImport("fxnet", EntryPoint = "StopAllSockets", CharSet = CharSet.Ansi)]
        public static extern void StopAllSockets();
#endif
    }
}
