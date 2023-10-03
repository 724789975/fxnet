#include "tcp_listener.h"
#include "../include/log_utility.h"
#include "../include/fxnet_interface.h"
#include "tcp_connector.h"
#include "iothread.h"
#include <cstdlib>

#ifdef _WIN32
#include <WinSock2.h>
#ifndef macro_closesocket
#define macro_closesocket closesocket
#endif //macro_closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netinet/tcp.h>
#ifndef macro_closesocket
#define macro_closesocket close
#endif //macro_closesocket
#endif // _WIN32

#ifdef _WIN32
struct tcp_keepalive
{
	u_long  onoff;
	u_long  keepalivetime;
	u_long  keepaliveinterval;
};
#define SIO_KEEPALIVE_VALS _WSAIOW(IOC_VENDOR,4)
#endif

namespace FXNET
{
	class TCPListenIOAcceptOperation : public IOOperationBase
	{
	public:
		friend class CTcpListener;
		virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPListenIOAcceptOperation, this);

			CTcpListener& refListenerSocket = (CTcpListener&)refSocketBase;

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refListenerSocket.NativeSocket() << ", " << refListenerSocket.Name()
				<< "\n";

#ifdef _WIN32
			//SOCKET hSock = m_hSocket;
			::setsockopt(this->m_hSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT
				, (char*)&(refListenerSocket.NativeSocket()), sizeof(refListenerSocket.NativeSocket()));

			sockaddr_in* pstRemoteAddr = NULL;
			sockaddr_in* pstLocalAddr = NULL;
			int nRemoteAddrLen = sizeof(sockaddr_in);
			int nLocalAddrLen = sizeof(sockaddr_in);
			int nAddrLen = sizeof(sockaddr_in) + 16;

			refListenerSocket.m_lpfnGetAcceptExSockaddrs(
				this->m_stWsaBuff.buf,
				0,
				nAddrLen,
				nAddrLen,
				(SOCKADDR**)&pstLocalAddr,
				&nLocalAddrLen,
				(SOCKADDR**)&pstRemoteAddr,
				&nRemoteAddrLen);

			// keep alive
			struct tcp_keepalive keepAliveIn;
			struct tcp_keepalive keepAliveOut;

			unsigned long ulBytesReturn = 0;

			keepAliveIn.keepaliveinterval = 10000;//
			keepAliveIn.keepalivetime = 1000 * 30;//
			keepAliveIn.onoff = 1;

			if (SOCKET_ERROR == WSAIoctl(m_hSocket
				, SIO_KEEPALIVE_VALS
				, &keepAliveIn
				, sizeof(keepAliveIn)
				, &keepAliveOut
				, sizeof(keepAliveOut)
				, &ulBytesReturn
				, NULL
				, NULL))
			{
				int dwError = WSAGetLastError();
				LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSocket << ", Set keep alive error"
					<< "\n";

				macro_closesocket(this->m_hSocket);
				refListenerSocket.PostAccept(pOStream);
				return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			}

			// send back
			refListenerSocket.OnClientConnected(this->m_hSocket, *pstRemoteAddr, pOStream);

			refListenerSocket.PostAccept(pOStream);
#else
			sockaddr_in stRemoteAddr;
			unsigned int dwAddrLen = sizeof(stRemoteAddr);
			ISocketBase::NativeSocketType hAcceptSock = accept(refListenerSocket.NativeSocket(), (sockaddr*)&stRemoteAddr, &dwAddrLen);
			if (ISocketBase::InvalidNativeHandle() == hAcceptSock)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "accept error(" << errno << ")"
					<< "\n";
				return ErrorCode(errno, __FILE__ ":" __LINE2STR__(__LINE__));
			}

			// keep alive
			int keepAlive = 1;
			setsockopt(hAcceptSock, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepAlive, sizeof(keepAlive));

			int keepIdle = 30;
			int keepInterval = 5;
			int keepCount = 6;
			setsockopt(hAcceptSock, SOL_TCP, TCP_KEEPIDLE, (void*)&keepIdle, sizeof(keepIdle));
			setsockopt(hAcceptSock, SOL_TCP, TCP_KEEPINTVL, (void*)&keepInterval, sizeof(keepInterval));
			setsockopt(hAcceptSock, SOL_TCP, TCP_KEEPCNT, (void*)&keepCount, sizeof(keepCount));

			// send back
			refListenerSocket.OnClientConnected(hAcceptSock, stRemoteAddr, pOStream);
#endif
			return ErrorCode();
		}
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		ISocketBase::NativeSocketType m_hSocket;
#endif // _WIN32
		char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
	};

	TCPListenIOAcceptOperation& NewTcpListenReadOperationHelp()
	{
		TCPListenIOAcceptOperation& refPeration = *(new TCPListenIOAcceptOperation());
#ifdef _WIN32

		refPeration.m_stWsaBuff.buf = refPeration.m_szRecvBuff;
		refPeration.m_stWsaBuff.len = sizeof(refPeration.m_szRecvBuff);
		memset(refPeration.m_stWsaBuff.buf, 0, refPeration.m_stWsaBuff.len);
#endif // _WIN32
		return refPeration;
	}

	class TCPListenIOErrorOperation : public IOOperationBase
	{
	public:
		friend class CTcpListener;
		virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPListenIOErrorOperation, this);
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOErrorOperation failed(" << m_oError.What() << ")"
				<< "\n";
			return ErrorCode();
		}
	};

	CTcpListener::CTcpListener(SessionMaker* pMaker)
		: m_pSessionMaker(pMaker)
#ifdef _WIN32
		, m_lpfnAcceptEx(0)
		, m_lpfnGetAcceptExSockaddrs(0)
#endif // _WIN32
	{

	}

	ErrorCode CTcpListener::Update(double dTimedouble, std::ostream* pOStream)
	{
		//TODO
		return ErrorCode();
	}

	ErrorCode CTcpListener::Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		sockaddr_in& refLocalAddr = this->GetLocalAddr();
		memset(&refLocalAddr, 0, sizeof(refLocalAddr));
		refLocalAddr.sin_family = AF_INET;

		if (szIp) { refLocalAddr.sin_addr.s_addr = inet_addr(szIp); }

		refLocalAddr.sin_port = htons(wPort);

		int dwError = 0;

		// 创建socket
#ifdef _WIN32
		this->NativeSocket() = WSASocket(AF_INET
			, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		this->NativeSocket() = socket(AF_INET, SOCK_STREAM, 0);
#endif // _WIN32

		if (this->NativeSocket() == -1)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket create failed(" << dwError << ")"
				<< "\n";
			return dwError;
		}

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(this->NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(this->NativeSocket(), F_SETFL, fcntl(this->NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif // _WIN32
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "\n";
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
				<< __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif // _WIN32

		// 地址重用
		int nReuse = 1;
		if (setsockopt(this->NativeSocket(), SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(nReuse)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " socket set SO_REUSEADDR failed(" << dwError << ")"
				<< "\n";
			return dwError;
		}

#ifndef _WIN32
		if (setsockopt(this->NativeSocket(), SOL_SOCKET, SO_REUSEPORT, (char*)&nReuse, sizeof(nReuse)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " socket set SO_REUSEADDR failed(" << dwError << ")"
				<< "\n";
			return dwError;
		}
#endif

		// bind
		if (bind(this->NativeSocket(), (sockaddr*)&refLocalAddr, sizeof(refLocalAddr)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
					<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
					<< "\n";
			return dwError;
		}

		if (listen(this->NativeSocket(), 128) < 0)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " listen failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "\n";
			return dwError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:" << inet_ntoa(refLocalAddr.sin_addr)
			<< ", port:" << (int)ntohs(refLocalAddr.sin_port)
			<< "\n";

		sockaddr_in addr;
		socklen_t sock_len = sizeof(addr);
		getsockname(this->NativeSocket(), (sockaddr*)&addr, &sock_len);

#ifdef _WIN32
		if (dwError = GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), this, pOStream))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << "bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "\n";
			return dwError;
		}
#else

		if (0 != (dwError = GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), EPOLLIN, this, pOStream)))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "\n";
			return dwError;
		}
#endif // _WIN32

#ifdef _WIN32
		this->InitAcceptEx(pOStream);

		dwError = this->PostAccept(pOStream);
#endif // _WIN32

		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket() << " ip:" << inet_ntoa(addr.sin_addr)
			<< ", port:" << (int)ntohs(addr.sin_port)
			<< "\n";

		return dwError;
	}

	void CTcpListener::Close(std::ostream* pOStream)
	{
		//TODO
	}

	IOOperationBase& CTcpListener::NewReadOperation()
	{
		return NewTcpListenReadOperationHelp();
	}

	IOOperationBase& CTcpListener::NewWriteOperation()
	{
		static TCPListenIOAcceptOperation oPeration;
		abort();
		return oPeration;
	}

	IOOperationBase& CTcpListener::NewErrorOperation(const ErrorCode& refError)
	{
		TCPListenIOErrorOperation& refPeration = *(new TCPListenIOErrorOperation());
		return refPeration;
	}

	void CTcpListener::OnError(const ErrorCode& refError, std::ostream* pOStream)
	{

	}

	void CTcpListener::OnClose(std::ostream* pOStream)
	{

	}

	CTcpListener& CTcpListener::OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		CTcpConnector* pTcpSock = new CTcpConnector((*m_pSessionMaker)());
		pTcpSock->SetIOModuleIndex(GetFxIoModuleIndex());

		pTcpSock->GetSession()->SetSock(pTcpSock);
		pTcpSock->SetRemoteAddr(address);

		class TcpConnect : public IOEventBase
		{
		public:
			TcpConnect(CTcpConnector* pTcpSock, NativeSocketType hSock)
				: m_pTcpSock(pTcpSock)
				, m_hSock(hSock)
			{}
			virtual void operator ()(std::ostream* pOStream)
			{
				DELETE_WHEN_DESTRUCT(TcpConnect, this);

				if (int dwError = m_pTcpSock->Connect(m_hSock, m_pTcpSock->GetRemoteAddr(), pOStream))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSock << " client connect failed(" << dwError << ")"
						<< "\n";

					m_pTcpSock->NewErrorOperation(dwError)(*m_pTcpSock, 0, pOStream);
					return;
				}

				if (int dwError = m_pTcpSock->Init(pOStream, ST_SYN_RECV))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSock << " client connect failed(" << dwError << ")"
						<< "\n";

					m_pTcpSock->NewErrorOperation(dwError)(*m_pTcpSock, 0, pOStream);
					return;
				}

				if (int dwError =
#ifdef _WIN32
					GetFxIoModule(m_pTcpSock->GetIOModuleIndex())->RegisterIO(m_hSock, m_pTcpSock, pOStream)
#else
					GetFxIoModule(m_pTcpSock->GetIOModuleIndex())->RegisterIO(m_hSock, EPOLLET | EPOLLIN | EPOLLOUT, m_pTcpSock, pOStream)
#endif // _WIN32
					)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << "register io failed(" << dwError << ")"
						<< " ip:" << inet_ntoa(m_pTcpSock->GetLocalAddr().sin_addr)
						<< ", port:" << (int)ntohs(m_pTcpSock->GetLocalAddr().sin_port)
						<< " remote_ip:" << inet_ntoa(m_pTcpSock->GetRemoteAddr().sin_addr)
						<< ", remote_port:" << (int)ntohs(m_pTcpSock->GetRemoteAddr().sin_port)
						<< "\n";

					m_pTcpSock->NewErrorOperation(dwError)(*m_pTcpSock, 0, pOStream);

					return;
				}

#ifdef _WIN32
#else
				m_pTcpSock->m_bConnecting = true;
#endif // _WIN32

				m_pTcpSock->OnConnected(pOStream);

			}
		protected:
		private:
			CTcpConnector* m_pTcpSock;
			NativeSocketType m_hSock;
		};

		PostEvent(pTcpSock->GetIOModuleIndex(), new TcpConnect(pTcpSock, hSock));
		return *this;
	}

#ifdef _WIN32
	int CTcpListener::PostAccept(std::ostream* pOStream)
	{
		SOCKET hNewSock = WSASocket(
			AF_INET,
			SOCK_STREAM,
			0,
			NULL,
			0,
			WSA_FLAG_OVERLAPPED);

		if (INVALID_SOCKET == hNewSock)
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << hNewSock << " WSASocket failed, errno(" << dwError << ")"
				<< "\n";

			return dwError;
		}

		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(hNewSock, FIONBIO, (unsigned long*)&ul))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << hNewSock << " ioctlsocket, errno(" << dwError << ")"
				<< "\n";

			macro_closesocket(hNewSock);
			return dwError;
		}

		int nSendBuffSize = 256 * 1024;
		int nRecvBuffSize = 8 * nSendBuffSize;
		if ((0 != setsockopt(hNewSock, SOL_SOCKET, SO_RCVBUF, (char*)&nRecvBuffSize, sizeof(int))) ||
			(0 != setsockopt(hNewSock, SOL_SOCKET, SO_SNDBUF, (char*)&nSendBuffSize, sizeof(int))))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << hNewSock << " setsockopt, errno(" << dwError << ")"
				<< "\n";

			macro_closesocket(hNewSock);
			return dwError;
		}


		TCPListenIOAcceptOperation& refOperation = NewTcpListenReadOperationHelp();
		refOperation.m_hSocket = hNewSock;

		DWORD wLength = 0;
		if (!m_lpfnAcceptEx( this->NativeSocket()
			, hNewSock
			, refOperation.m_stWsaBuff.buf
			, 0
			, sizeof(SOCKADDR_IN) + 16
			, sizeof(SOCKADDR_IN) + 16
			, &wLength
			, (OVERLAPPED*)(IOOperationBase*)(&refOperation)))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << hNewSock << " setsockopt, errno(" << dwError << ")"
					<< "\n";
				macro_closesocket(hNewSock);
				return dwError;
			}
		}
		return 0;
	}

	int CTcpListener::InitAcceptEx(std::ostream* pOStream)
	{
		DWORD dwbytes = 0;

		GUID guidGuidAcceptEx = WSAID_ACCEPTEX;

		if (SOCKET_ERROR == ::WSAIoctl(this->NativeSocket()
			, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &guidGuidAcceptEx
			, sizeof(guidGuidAcceptEx)
			, &this->m_lpfnAcceptEx
			, sizeof(LPFN_ACCEPTEX)
			, &dwbytes
			, NULL
			, NULL))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " WSAIoctl, errno(" << dwError << ")"
				<< "\n";
			macro_closesocket(this->NativeSocket());
			return dwError;
		}

		GUID guidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		dwbytes = 0;

		if (SOCKET_ERROR == ::WSAIoctl(NativeSocket()
			, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &guidGetAcceptExSockaddrs
			, sizeof(guidGetAcceptExSockaddrs)
			, &this->m_lpfnGetAcceptExSockaddrs
			, sizeof(LPFN_GETACCEPTEXSOCKADDRS)
			, &dwbytes
			, NULL
			, NULL))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " WSAIoctl, errno(" << dwError << ")"
				<< "\n";
			macro_closesocket(this->NativeSocket());
			return dwError;
		}

		return 0;
	}

#else
#endif // _WIN32


};

