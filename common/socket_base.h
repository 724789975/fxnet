#ifndef __SOCKET_BASE_H__

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
#endif // _WIN32

#include <ostream>

namespace FXNET
{

	class IOOperationBase;

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

		virtual IOOperationBase& NewReadOperation() = 0;
		virtual IOOperationBase& NewWriteOperation() = 0;

		virtual void OnRead(std::ostream& refOStream) = 0;
		virtual void OnWrite(std::ostream& refOStream) = 0;
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
		virtual ~IOOperationBase() {}
		virtual IOOperationBase& operator()(CSocketBase& refSocketBase, std::ostream& refOStream) = 0;

	protected:
	private:
	};
};

#endif // !__SOCKET_BASE_H__


