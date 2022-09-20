#ifndef __SOCKET_BASE_H__
#define __SOCKET_BASE_H__

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
#else
#include<netinet/in.h>
#include<arpa/inet.h>
#endif // _WIN32

#include <ostream>

#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")
#endif //_WIN32

namespace FXNET
{

	class IOOperationBase;

	class ISocketBase
	{
	public:
#ifdef _WIN32
		typedef HANDLE NativeHandleType;
		typedef SOCKET NativeSocketType;
#else //_WIN32
		typedef int NativeHandleType;
		typedef int NativeSocketType;
#endif //_WIN32

		virtual const char* Name()const { return "CSocketBase"; }
		virtual ISocketBase& Update(double dTime, std::ostream* POStream) = 0;

		static NativeHandleType InvalidNativeHandle() { return (NativeHandleType)-1; };
		NativeHandleType& NativeHandle() { return m_hNativeHandle; }
		NativeSocketType& NativeSocket() { return (NativeSocketType&)m_hNativeHandle; }

		sockaddr_in& GetLocalAddr() { return m_stLocalAddr; }

		virtual IOOperationBase& NewReadOperation() = 0;
		virtual IOOperationBase& NewWriteOperation() = 0;
		virtual IOOperationBase& NewErrorOperation(int dwError) = 0;

		virtual void OnRead(std::ostream* refOStream) = 0;
		virtual void OnWrite(std::ostream* pOStream) = 0;
		virtual void OnError(std::ostream* pOStream) = 0;
	protected:
		NativeHandleType m_hNativeHandle;
		sockaddr_in m_stLocalAddr;

		//linux下et模式 需要表示是否刻度可写
#ifndef _WIN32
		bool bReadable;
		bool bWritable;
#endif
	private:
	};

	class IOOperationBase
#ifdef _WIN32
		: public OVERLAPPED
#endif // _WIN32
	{
	public:
		IOOperationBase() : m_dwError(0) {}
		virtual ~IOOperationBase() {}
		virtual void operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream) = 0;

		int m_dwError;
	protected:
	private:
	};
};

#endif // !__SOCKET_BASE_H__


