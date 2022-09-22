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
	CUdpConnector::CUdpConnector()
	{
		class UDPOnRecvOperator : public OnRecvOperator
		{
		public:
			virtual int operator() (char* szBuff, unsigned short wSize)
			{}
		};

		class UDPOnConnectedOperator : public OnConnectedOperator
		{
		public:
			virtual int operator() ()
			{}
		};

		class UDPRecvOperator : public RecvOperator
		{
		public:
			virtual int operator() (char* pBuff, unsigned short wBuffSize, int wRecvSize)
			{}
		};

		class UDPSendOperator : public SendOperator
		{
		public:
			virtual int operator() (char* szBuff, unsigned short wBufferSize, unsigned short& wSendLen)
			{}
		};
	}

	int CUdpConnector::Init(std::ostream* pOStream, int dwState)
	{
		if (m_oBuffContral.Init(dwState))
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

		return 0;
	}

	ISocketBase& CUdpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		// TODO: 在此处插入 return 语句

		//TODO 需要将数据放入窗口 暂时未添加
		//m_oBuffContral.Send(NULL, 0, dTimedouble);

		m_oBuffContral.SendMessages(dTimedouble);
		m_oBuffContral.ReceiveMessages(dTimedouble);

		return *this;
	}

	int CUdpConnector::Connect(sockaddr_in address, std::ostream* pOStream)
	{
#ifdef _WIN32
		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_DGRAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocketType hSock = socket(AF_INET, SOCK_DGRAM, 0);
#endif // _WIN32

		if (hSock == -1)
		{
			if (pOStream)
			{
				*pOStream << "create socket failed."
					<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";;
			}
			return -1;
		}

		CUdpConnector* pUdpSock = new CUdpConnector;
		if (int dwError = pUdpSock->SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			if (pOStream)
			{
				(*pOStream) << "client connect failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
		}

		if (int dwError = pUdpSock->Init(pOStream, ST_SYN_SEND))
		{
			if (pOStream)
			{
				(*pOStream) << "client connect failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}

			//post 到iomodule 移除
			return 1;
		}

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
		if (fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
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
	

		return 0;
	}

	void CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{}

	void CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		delete this;
		return;
	}
};
