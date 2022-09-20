#include "udp_connector.h"
#include "iothread.h"

#ifdef _WIN32
#ifndef macro_closesocket
#define macro_closesocket closesocket
#endif //macro_closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef macro_closesocket
#define macro_closesocket close
#endif //macro_closesocket
#endif // _WIN32

namespace FXNET
{
	ISocketBase& CUdpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		// TODO: 在此处插入 return 语句
		return *this;
	}

	int CUdpConnector::Connect(NativeSocketType hSock, sockaddr_in address, std::ostream* pOStream)
	{
		NativeSocket() = hSock;

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(NativeSocket(), F_SETFL, fcntl(NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif //_WIN32
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "socket set nonblock failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

#ifndef _WIN32
		if(fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			if (pOStream)
			{
				(*pOStream) << "socket set nonblock failed(" << errno << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return errno;
		}
#endif

		// set ttl
		int ttl = 128;
		if (setsockopt(NativeSocket(), IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "socket set nonblock failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

		// set recv buffer size and send buffer size
		int buf_size = 256 * 1024;
		setsockopt(NativeSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size));
		setsockopt(NativeSocket(), SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size));

		// connect
		if (connect(NativeSocket(), (sockaddr*)&address, sizeof(address)))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "socket set nonblock failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

		if (!
#ifdef _WIN32
			FxIoModule::Instance()->RegisterIO(NativeSocket(), this, pOStream)
#else
			FxIoModule::Instance()->RegisterIO(NativeSocket(), EPOLLIN | EPOLLOUT, this, pOStream)
#endif // _WIN32
		)
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "register io failed" << " ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return 1;
		}
		if (m_oBuffContral.Init())
		{
		}

		// status = ST_SYN_RECV;
		// delay_time = 0;
		// delay_average = 3 * send_frequency;
		// retry_time = delay_time + 2 * delay_average;
		// send_time = 0;
		// ack_recv_time = Event::GetTime();
		// ack_timeout_retry = 1;
		// ack_same_count = 0;
		// quick_retry = false;
		// send_data_time = 0;

		// ack_last = 0;
		// syn_last = 0;
		// send_ack = false;
		// send_window_control = 1;
		// send_window_threshhold = send_window.window_size;

		// readable = false;
		// writable = false;

		// // clear sliding window buffer
		// recv_window.ClearBuffer();
		// send_window.ClearBuffer();

		// // initialize send window
		// send_window.begin = 1;
		// send_window.end = send_window.begin;

		// // initialize recv window
		// recv_window.begin = 1;
		// recv_window.end = recv_window.begin + recv_window.window_size;








		return 0;
	}

	CUdpConnector::IOReadOperation& CUdpConnector::NewReadOperation()
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	IOOperationBase& CUdpConnector::NewWriteOperation()
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	IOOperationBase& CUdpConnector::NewErrorOperation(int dwError)
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	void CUdpConnector::OnRead(std::ostream* refOStream)
	{
	}

	void CUdpConnector::OnWrite(std::ostream* pOStream)
	{
	}

	void CUdpConnector::OnError(std::ostream* pOStream)
	{
	}

	void CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{}

	void CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		delete this;
		return;
	}
};
