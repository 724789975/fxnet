#include "tcp_listener.h"
#include "include/iothread.h"
#include "tcp_connector.h"
#include "include/log_utility.h"
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
	int CTcpListener::IOAcceptOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CTcpListener::IOAcceptOperation, this);

		CTcpListener& refListenerSocket = (CTcpListener&) refSocketBase;

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refListenerSocket.NativeSocket() << ", " << refListenerSocket.Name()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		//SOCKET hSock = m_hSocket;
		::setsockopt(m_hSocket, SOL_SOCKET, SO_UPDATE_ACCEPT_CONTEXT
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

		if(SOCKET_ERROR == WSAIoctl(m_hSocket
			, SIO_KEEPALIVE_VALS
			, &keepAliveIn
			, sizeof(keepAliveIn)
			, &keepAliveOut
			, sizeof(keepAliveOut)
			, &ulBytesReturn
			, NULL
			, NULL ))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSocket << ", Set keep alive error" 
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			macro_closesocket(m_hSocket);
			refListenerSocket.PostAccept(pOStream);
			return dwError;
		}

		// send back
		refListenerSocket.OnClientConnected(m_hSocket, *pstRemoteAddr, pOStream);

		refListenerSocket.PostAccept(pOStream);
#else
		sockaddr_in stRemoteAddr;
		unsigned int dwAddrLen = sizeof(stRemoteAddr);
		NativeSocketType hAcceptSock = accept(refListenerSocket.NativeSocket(), (sockaddr*)&stRemoteAddr, &dwAddrLen);
		if (InvalidNativeHandle() == hAcceptSock)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "accept error(" << errno << ")"
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return errno;
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
		return 0;
	}

	int CTcpListener::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CTcpListener::IOErrorOperation, this);
		LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOErrorOperation failed(" << m_dwError << ")"
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
		return 0;
	}

	CTcpListener::CTcpListener(SessionMaker* pMaker)
		: m_pSessionMaker(pMaker)
	{

	}

	int CTcpListener::Update(double dTimedouble, std::ostream* pOStream)
	{
		//TODO
		return 0;
	}

	int CTcpListener::Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		sockaddr_in& refLocalAddr = GetLocalAddr();
		memset(&refLocalAddr, 0, sizeof(refLocalAddr));
		refLocalAddr.sin_family = AF_INET;

		if (szIp) { refLocalAddr.sin_addr.s_addr = inet_addr(szIp); }

		refLocalAddr.sin_port = htons(wPort);

		int dwError = 0;

		// 创建socket
#ifdef _WIN32
		NativeSocket() = WSASocket(AF_INET
			, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocket() = socket(AF_INET, SOCK_STREAM, 0);
#endif // _WIN32

		if (NativeSocket() == -1)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket create failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(NativeSocket(), F_SETFL, fcntl(NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif // _WIN32
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			dwError = errno;
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
				<< __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif // _WIN32

		// 地址重用
		int nReuse = 1;
		if (setsockopt(NativeSocket(), SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(nReuse)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " socket set SO_REUSEADDR failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		// bind
		if (bind(NativeSocket(), (sockaddr*)&refLocalAddr, sizeof(refLocalAddr)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
				LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
					<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		if (listen(NativeSocket(), 128) < 0)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else
			dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " listen failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << " ip:" << inet_ntoa(refLocalAddr.sin_addr)
			<< ", port:" << (int)ntohs(refLocalAddr.sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		sockaddr_in addr;
		socklen_t sock_len = sizeof(addr);
		getsockname(NativeSocket(), (sockaddr*)&addr, &sock_len);

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << " ip:" << inet_ntoa(addr.sin_addr)
			<< ", port:" << (int)ntohs(addr.sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		if (dwError = FxIoModule::Instance()->RegisterIO(NativeSocket(), this, pOStream))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << "bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#else

		if (0 != (dwError = FxIoModule::Instance()->RegisterIO(NativeSocket(), EPOLLIN, this, pOStream)))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "bind failed on (" << inet_ntoa(refLocalAddr.sin_addr)
				<< ", " << (int)ntohs(refLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif // _WIN32

#ifdef _WIN32
		InitAcceptEx(pOStream);

		dwError = PostAccept(pOStream);
#endif // _WIN32

		return dwError;
	}

	void CTcpListener::Close(std::ostream* pOStream)
	{
		//TODO
	}

	CTcpListener::IOAcceptOperation& CTcpListener::NewReadOperation()
	{
		IOAcceptOperation& refPeration = *(new IOAcceptOperation());
#ifdef _WIN32

		refPeration.m_stWsaBuff.buf = refPeration.m_szRecvBuff;
		refPeration.m_stWsaBuff.len = sizeof(refPeration.m_szRecvBuff);
		memset(refPeration.m_stWsaBuff.buf, 0, refPeration.m_stWsaBuff.len);
#endif // _WIN32
		return refPeration;
	}

	IOOperationBase& CTcpListener::NewWriteOperation()
	{
		static IOAcceptOperation oPeration;
		abort();
		return oPeration;
	}

	CTcpListener::IOErrorOperation& CTcpListener::NewErrorOperation(int dwError)
	{
		IOErrorOperation& refPeration = *(new IOErrorOperation());
		return refPeration;
	}

	void CTcpListener::OnError(int dwError, std::ostream* pOStream)
	{

	}

	void CTcpListener::OnClose(std::ostream* pOStream)
	{

	}

	CTcpListener& CTcpListener::OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		CTcpConnector* pTcpSock = new CTcpConnector((*m_pSessionMaker)());

		pTcpSock->GetSession()->SetSock(pTcpSock);
		if (int dwError = pTcpSock->SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock << " client connect failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			//post 到iomodule 移除
			return *this;
		}

		if (int dwError = pTcpSock->Init(pOStream, ST_SYN_RECV))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock << " client connect failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";

			//post 到iomodule 移除
			return *this;
		}

		if (int dwError =
#ifdef _WIN32
			FxIoModule::Instance()->RegisterIO(hSock, pTcpSock, pOStream)
#else
			FxIoModule::Instance()->RegisterIO(hSock, EPOLLET | EPOLLIN | EPOLLOUT, pTcpSock, pOStream)
#endif // _WIN32
			)
		{
			macro_closesocket(hSock);
			pTcpSock->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "register io failed(" << dwError << ")"
				<< " ip:" << inet_ntoa(pTcpSock->GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(pTcpSock->GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(pTcpSock->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(pTcpSock->GetRemoteAddr().sin_port)
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			return *this;
		}

#ifdef _WIN32
#else
		pTcpSock->m_bConnecting = true;
#endif // _WIN32

		pTcpSock->OnConnected(pOStream);

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
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

			return dwError;
		}

		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(hNewSock, FIONBIO, (unsigned long*)&ul))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << hNewSock << " ioctlsocket, errno(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

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
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

			macro_closesocket(hNewSock);
			return dwError;
		}


		IOAcceptOperation& refOperation = NewReadOperation();
		refOperation.m_hSocket = hNewSock;

		DWORD wLength = 0;
		if (!m_lpfnAcceptEx( NativeSocket()
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
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
				macro_closesocket(hNewSock);
				return dwError;
			}
		}
		return 0;
	}

	int CTcpListener::InitAcceptEx(std::ostream* pOStream)
	{
		DWORD dwbytes = 0;

		GUID m_GuidAcceptEx = WSAID_ACCEPTEX;

		if (SOCKET_ERROR == ::WSAIoctl(NativeSocket()
			, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &m_GuidAcceptEx
			, sizeof(m_GuidAcceptEx)
			, &m_lpfnAcceptEx
			, sizeof(LPFN_ACCEPTEX)
			, &dwbytes
			, NULL
			, NULL))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " WSAIoctl, errno(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
			macro_closesocket(NativeSocket());
			return dwError;
		}

		GUID m_GuidGetAcceptExSockaddrs = WSAID_GETACCEPTEXSOCKADDRS;

		dwbytes = 0;

		if (SOCKET_ERROR == ::WSAIoctl(NativeSocket()
			, SIO_GET_EXTENSION_FUNCTION_POINTER
			, &m_GuidGetAcceptExSockaddrs
			, sizeof(m_GuidGetAcceptExSockaddrs)
			, &m_lpfnGetAcceptExSockaddrs
			, sizeof(LPFN_GETACCEPTEXSOCKADDRS)
			, &dwbytes
			, NULL
			, NULL))
		{
			int dwError = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " WSAIoctl, errno(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
			macro_closesocket(NativeSocket());
			return dwError;
		}

		return 0;
	}

#else
#endif // _WIN32

	int CTcpListener::OnAccept(std::ostream* pOStream)
	{
		return 0;
	}


};

