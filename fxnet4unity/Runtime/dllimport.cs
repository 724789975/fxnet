using System;
using System.Runtime.InteropServices;

namespace fxnetlib.dllimport
{
    public class DLLImport
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnRecvCallback(IntPtr pConnector, string pData, int nLen);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnConnectedCallback(IntPtr pConnector);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnErrorCallback(IntPtr pConnector, IntPtr nLen);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnCloseCallback(IntPtr pConnector);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void OnLogCallback(string pLog, int nLen);

        [DllImport("fxnet", EntryPoint = "CreateConnector", CharSet = CharSet.Ansi)]
        public static extern IntPtr CreateConnector(OnRecvCallback onRecv, OnConnectedCallback onConnected, OnErrorCallback onError, OnCloseCallback onClose);
        [DllImport("fxnet", EntryPoint = "DestroyConnector", CharSet = CharSet.Ansi)]
        public static extern void DestroyConnector(IntPtr pConnector);
        [DllImport("fxnet", EntryPoint = "UdpConnect", CharSet = CharSet.Ansi)]
        public static extern void UdpConnect(IntPtr pConnector, string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "TcpConnect", CharSet = CharSet.Ansi)]
        public static extern void TcpConnect(IntPtr pConnector, string szIp, UInt16 wPort);
        [DllImport("fxnet", EntryPoint = "Send", CharSet = CharSet.Ansi)]
        public static extern void Send(IntPtr pConnector, string szData, UInt32 dwLen);
        [DllImport("fxnet", EntryPoint = "Close", CharSet = CharSet.Ansi)]
        public static extern void Close(IntPtr pConnector);
        [DllImport("fxnet", EntryPoint = "StartIOModule", CharSet = CharSet.Ansi)]
        public static extern void StartIOModule();
        [DllImport("fxnet", EntryPoint = "ProcessIOModule", CharSet = CharSet.Ansi)]
        public static extern void ProcessIOModule();
        [DllImport("fxnet", EntryPoint = "SetLogCallback", CharSet = CharSet.Ansi)]
        public static extern void SetLogCallback(OnLogCallback onLogCallback);

    }
}
