#ifndef __SOCKET_BASE_H__
#define __SOCKET_BASE_H__

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
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

		virtual IOOperationBase& NewReadOperation() = 0;
		virtual IOOperationBase& NewWriteOperation() = 0;
		virtual IOOperationBase& NewErrorOperation(int dwError) = 0;

		virtual void OnRead(std::ostream* refOStream) = 0;
		virtual void OnWrite(std::ostream* pOStream) = 0;
		virtual void OnError(std::ostream* pOStream) = 0;
	protected:
		NativeHandleType m_hNativeHandle;
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


