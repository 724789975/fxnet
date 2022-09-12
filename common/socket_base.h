#ifndef __SOCKET_BASE_H__

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
#endif // _WIN32

namespace FXNET
{
	class CSocketBase
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
		virtual CSocketBase& Update(double dTime) = 0;

		static NativeHandleType InvalidNativeHandle() { return (NativeHandleType)-1; };
		NativeHandleType& NativeHandle() { return m_hNativeHandle; }
		NativeSocketType& NativeSocket() { return (NativeSocketType&)m_hNativeHandle; }

	protected:
		NativeHandleType m_hNativeHandle;
	private:
	};

};

#endif // !__SOCKET_BASE_H__


