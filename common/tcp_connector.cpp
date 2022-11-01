#include "tcp_connector.h"
#include "include/iothread.h"
#include "include/net_work_stream.h"
#include "include/error_code.h"
#include "include/log_utility.h"

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

#include <assert.h>

#include <iostream>

namespace FXNET
{
	int CTcpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CTcpConnector::IOReadOperation, this);

		CTcpConnector& refConnector = (CTcpConnector&)refSocketBase;
#ifdef _WIN32
		refConnector.GetSession()->GetRecvBuff().PushData(dwLen);
		if (!refConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			refConnector.PostRecv(pOStream);
			return 0;
		}

		std::string szData;
		refConnector.GetSession()->GetRecvBuff().PopData(szData);

		MessageEventBase* pOperator = refConnector.GetSession()->NewRecvMessageEvent(szData);

		if (NULL == pOperator)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " failed " << CODE_ERROR_NET_PARSE_MESSAGE
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_PARSE_MESSAGE;
		}

		FxIoModule::Instance()->PushMessageEvent(pOperator);
		refConnector.PostRecv(pOStream);
#else
		refConnector.m_bReadable = true;
		while (refConnector.m_bReadable)
		{
			int dwLen = recv(refConnector.NativeSocket()
				, refConnector.GetSession()->GetRecvBuff().GetData() + refConnector.GetSession()->GetRecvBuff().GetSize()
				, refConnector.GetSession()->GetRecvBuff().GetFreeSize(), 0);

			if (0 > dwLen)
			{
				refConnector.m_bReadable = false;
				int dwError = errno;

				if (dwError != EAGAIN && dwError != EINTR) { return dwError; }
			}
			else if (0 == dwLen)
			{
				return CODE_SUCCESS_NET_EOF;
			}
			else
			{
				refConnector.GetSession()->GetRecvBuff().PushData(dwLen);
				if (!refConnector.GetSession()->GetRecvBuff().CheckPackage())
				{
					continue;
				}

				std::string szData;
				refConnector.GetSession()->GetRecvBuff().PopData(szData);

				MessageEventBase* pOperator = refConnector.GetSession()->NewRecvMessageEvent(szData);

				if (NULL == pOperator)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " failed " << CODE_ERROR_NET_PARSE_MESSAGE
						<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
					return CODE_ERROR_NET_PARSE_MESSAGE;
				}

				FxIoModule::Instance()->PushMessageEvent(pOperator);
				continue;
			}
		}
#endif // _WIN32

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
			<< " ip:" << inet_ntoa(refConnector.GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(refConnector.GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(refConnector.GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(refConnector.GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		return 0;
	}

	int CTcpConnector::IOWriteOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CTcpConnector::IOWriteOperation, this);
		CTcpConnector& refConnector = (CTcpConnector&)refSocketBase;

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
#ifdef _WIN32
		refConnector.PostSend(pOStream);
#else
		refConnector.m_bWritable = true;

		// send as much data as we can.
		while (refConnector.writable)
		{
			int dwLen = send(refConnector.NativeSocket()
				, refConnector.GetSession()->GetSendBuff().GetData()
				, refConnector.GetSession()->GetSendBuff.GetSize(), 0);

			if (0 > dwLen)
			{
				int dwError = errno;
				refConnector.writable = false;

				if (dwError == EAGAIN)
					break;
				else
				{
					LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket() << "(" << dwError << ")"
						<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
					return dwError;
				}
			}

			if (0 == dwLen) break;
			refConnector.GetSession()->GetSendBuff().PopData(dwLen);
			if (0 == refConnector.GetSession()->GetSendBuff().GetSize()) break;
		}

#endif // _WIN32

		return 0;
	}

	int CTcpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CTcpConnector::IOErrorOperation, this);

		LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << "(" << m_dwError << ")"
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		if (NULL == ((CTcpConnector&)refSocketBase).GetSession())
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " already wait delete (" << m_dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return 0;
		}

		macro_closesocket(refSocketBase.NativeSocket());

		//´¦Àí´íÎó
		FxIoModule::Instance()->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_dwError));
		FxIoModule::Instance()->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewCloseEvent());

		((CTcpConnector&)refSocketBase).SetSession(NULL);

		return 0;
	}

	CTcpConnector::CTcpConnector(ISession* pSession)
		: CConnectorSocket(pSession)
	{
		memset(&m_stRemoteAddr, 0, sizeof(m_stRemoteAddr));
	}

	CTcpConnector::~CTcpConnector()
	{
	}

	int CTcpConnector::Init(std::ostream* pOStream, int dwState)
	{
	
		return 0;
	}

	int CTcpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << ", error: " << m_dwError
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		return 0;
	}

	int CTcpConnector::Connect(sockaddr_in address, std::ostream* pOStream)
	{
#ifdef _WIN32
		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocketType hSock = socket(AF_INET, SOCK_DGRAM, 0);
#endif // _WIN32

		if (hSock == -1)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "create socket failed."
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
			return -1;
		}

		if (int dwError = Init(pOStream, ST_SYN_SEND))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << dwError << ")"
				<<"[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			//post µ½iomodule ÒÆ³ý
			return dwError;
		}

		if (int dwError = SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << " ip:" << inet_ntoa(GetLocalAddr().sin_addr) << ", port:" << (int)ntohs(GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr) << ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		for (int i = 0; i < 16; ++i)
		{
			PostRecv(pOStream);
		}
#endif // _WIN32

		return 0;
	}

	void CTcpConnector::Close(std::ostream* pOStream)
	{
		NewErrorOperation(CODE_SUCCESS_NET_EOF)(*this, 0, pOStream);
	}

	CTcpConnector::IOReadOperation& CTcpConnector::NewReadOperation()
	{
		CTcpConnector::IOReadOperation* pOperation = new CTcpConnector::IOReadOperation();
#ifdef _WIN32
		pOperation->m_stWsaBuff.buf = (char*)GetSession()->GetRecvBuff().GetData();
		pOperation->m_stWsaBuff.len = GetSession()->GetRecvBuff().GetFreeSize();
#endif // _WIN32

		return *pOperation;
	}

	CTcpConnector::IOWriteOperation& CTcpConnector::NewWriteOperation()
	{
		CTcpConnector::IOWriteOperation* pOperation = new CTcpConnector::IOWriteOperation();
#ifdef _WIN32
		//m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	CTcpConnector::IOErrorOperation& CTcpConnector::NewErrorOperation(int dwError)
	{
		CTcpConnector::IOErrorOperation* pOperation = new CTcpConnector::IOErrorOperation();
		pOperation->m_dwError = dwError;
		m_dwError = dwError;
		return *pOperation;
	}

	CTcpConnector& CTcpConnector::SendMessage(std::ostream* pOStream)
	{
		PostSend(pOStream);
		return *this;
	}

#ifdef _WIN32
	CTcpConnector& CTcpConnector::PostRecv(std::ostream* pOStream)
	{
		IOReadOperation& refIOReadOperation = NewReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSARecv(NativeSocket(), &refIOReadOperation.m_stWsaBuff
			, 1, &dwReadLen, &dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOReadOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << ", " << "PostRecv failed."<< dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
			}
		}

		return *this;
	}

	int CTcpConnector::PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream)
	{
		IOWriteOperation& refIOWriteOperation = NewWriteOperation();
		refIOWriteOperation.m_stWsaBuff.buf = pBuff;
		refIOWriteOperation.m_stWsaBuff.len = wLen;

		DWORD dwWriteLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSASend(NativeSocket(), &refIOWriteOperation.m_stWsaBuff
			, 1, &dwWriteLen, dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOWriteOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << ", " << "PostSend failed." << dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				delete &refIOWriteOperation;
			}

			return dwError;
		}

		return 0;
	}

	int CTcpConnector::PostSend(std::ostream* pOStream)
	{
		if (0 == GetSession()->GetSendBuff().GetSize())
		{
			return 0;
		}
		IOWriteOperation& refIOWriteOperation = NewWriteOperation();
		GetSession()->GetSendBuff().PopData(refIOWriteOperation.m_strData);
		refIOWriteOperation.m_stWsaBuff.buf = &(*refIOWriteOperation.m_strData.begin());
		refIOWriteOperation.m_stWsaBuff.len = refIOWriteOperation.m_strData.size();

		DWORD dwWriteLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSASend(NativeSocket(), &refIOWriteOperation.m_stWsaBuff
			, 1, &dwWriteLen, dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOWriteOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << ", " << "PostSend failed." << dwError
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";;
				delete& refIOWriteOperation;
			}

			return dwError;
		}

		return 0;
	}

#endif // _WIN32

	void CTcpConnector::OnRead(std::ostream* refOStream)
	{
	}

	void CTcpConnector::OnWrite(std::ostream* pOStream)
	{
	}

	void CTcpConnector::OnError(int dwError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << ", " << dwError
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
	}

	void CTcpConnector::OnClose(std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_INFO) << NativeSocket()
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
		delete this;
	}

	void CTcpConnector::OnConnected(std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_INFO) << NativeSocket() << " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		FxIoModule::Instance()->PushMessageEvent(GetSession()->NewConnectedEvent());
	}

	int CTcpConnector::Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		NativeSocket() = hSock;

		LOG(pOStream, ELOG_LEVEL_INFO)
			<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(NativeSocket(), F_SETFL, fcntl(NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif //_WIN32
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			int dwError = errno;
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif

		// set ttl
		int ttl = 128;
		if (setsockopt(NativeSocket(), IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)))
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "setsockopt failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		// set recv buffer size and send buffer size
		int buf_size = 64 * 1024;
		setsockopt(NativeSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size));
		setsockopt(NativeSocket(), SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size));

		socklen_t dwAddrLen = sizeof(GetLocalAddr());
		getsockname(hSock, (sockaddr*)&GetLocalAddr(), &dwAddrLen);

		if (int dwError =
#ifdef _WIN32
			FxIoModule::Instance()->RegisterIO(NativeSocket(), this, pOStream)
#else
			FxIoModule::Instance()->RegisterIO(NativeSocket(), EPOLLET | EPOLLIN | EPOLLOUT, this, pOStream)
#endif // _WIN32
			)
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "register io failed"
				<< " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			return dwError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		return 0;
	}



};
