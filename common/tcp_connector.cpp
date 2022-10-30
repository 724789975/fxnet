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
		refConnector.PostRecv(pOStream);

		m_dwLen = dwLen;
		refConnector.m_funRecvOperator.SetIOReadOperation(this);
		bool bReadable = true;
#else
		refConnector.m_bReadable = true;
		bool& bReadable = refConnector.m_bReadable;
#endif // _WIN32

		if (int dwError = refConnector.m_oBuffContral.ReceiveMessages(FxIoModule::Instance()->FxGetCurrentTime(), bReadable, pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOReadOperation failed " << dwError
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return dwError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket() << ", " << bReadable
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
#else
		refConnector.m_bWritable = true;
#endif // _WIN32

		if (int dwError = refConnector.m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " IOWriteOperation failed " << dwError
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return dwError;
		}
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

		//处理错误
		FxIoModule::Instance()->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_dwError));
		FxIoModule::Instance()->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewCloseEvent());

		((CTcpConnector&)refSocketBase).SetSession(NULL);

		return 0;
	}

	CTcpConnector::UDPOnRecvOperator::UDPOnRecvOperator(CTcpConnector& refTcpConnector)
		: m_refTcpConnector(refTcpConnector)
	{
	}

	int CTcpConnector::UDPOnRecvOperator::operator()(char* szBuff, unsigned short wSize, std::ostream* pOStream)
	{
		//收到的内容
		if (0 == wSize)
		{
			return 0;
		}

		m_refTcpConnector.GetSession()->GetRecvBuff().PushData(szBuff, wSize);
		if (!m_refTcpConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			return 0;
		}

		std::string szData;
		m_refTcpConnector.GetSession()->GetRecvBuff().PopData(szData);

		MessageEventBase* pOperator = m_refTcpConnector.GetSession()->NewRecvMessageEvent(szData);

		if (NULL == pOperator)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << m_refTcpConnector.NativeSocket() << " UDPOnRecvOperator failed " << CODE_ERROR_NET_PARSE_MESSAGE
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_PARSE_MESSAGE;
		}

		FxIoModule::Instance()->PushMessageEvent(pOperator);
		return 0;
	}

	CTcpConnector::UDPOnConnectedOperator::UDPOnConnectedOperator(CTcpConnector& refTcpConnector)
		: m_refTcpConnector(refTcpConnector)
	{
	}

	int CTcpConnector::UDPOnConnectedOperator::operator()(std::ostream* pOStream)
	{
		m_refTcpConnector.OnConnected(pOStream);
		return 0;
	}

	CTcpConnector::UDPRecvOperator::UDPRecvOperator(CTcpConnector& refTcpConnector)
		: m_refTcpConnector(refTcpConnector)
#ifdef _WIN32
		, m_pReadOperation(NULL)
#endif // _WIN32
	{
	}

	int CTcpConnector::UDPRecvOperator::operator()(char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream)
	{
#ifdef _WIN32
		//没有可读事件
		if (!m_pReadOperation)
		{
			return CODE_SUCCESS_NO_BUFF_READ;
		}

		wRecvSize = (unsigned short)m_pReadOperation->m_dwLen;
		if (0 == m_pReadOperation)
		{
			return CODE_SUCCESS_NET_EOF;
		}

		assert(wRecvSize <= UDP_WINDOW_BUFF_SIZE);
		assert(m_pReadOperation->m_dwLen <= UDP_WINDOW_BUFF_SIZE);
		memcpy(pBuff, m_pReadOperation->m_stWsaBuff.buf, wRecvSize);
		SetIOReadOperation(NULL);
#else
		wRecvSize = recv(m_refTcpConnector.NativeSocket(), pBuff, wBuffSize, 0);
		if (0 == wRecvSize)
		{
			return CODE_SUCCESS_NET_EOF;
		}

		if (0 > wRecvSize)
		{
			m_refTcpConnector.m_bReadable = false;
			return errno;
		}
#endif // _WIN32

		return 0;
	}

#ifdef _WIN32
	CTcpConnector::UDPRecvOperator& CTcpConnector::UDPRecvOperator::SetIOReadOperation(IOReadOperation* pReadOperation)
	{
		m_pReadOperation = pReadOperation;
		return *this;
	}
#endif // _WIN32

	CTcpConnector::UDPSendOperator::UDPSendOperator(CTcpConnector& refTcpConnector)
		: m_refTcpConnector(refTcpConnector)
	{
	}

	int CTcpConnector::UDPSendOperator::operator()(char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << m_refTcpConnector.NativeSocket()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
#ifdef _WIN32
		dwSendLen = wBufferSize;
		return m_refTcpConnector.PostSend(szBuff, wBufferSize, pOStream);
#else
		dwSendLen = send(m_refTcpConnector.NativeSocket(), szBuff, wBufferSize, 0);
		if (0 > dwSendLen)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "UDPSendOperator failed " << errno
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return errno;
		}
#endif // _WIN32
		return 0;
	}

	CTcpConnector::UDPReadStreamOperator::UDPReadStreamOperator(CTcpConnector& refTcpConnector)
		: m_refTcpConnector(refTcpConnector)
	{
	}

	int CTcpConnector::UDPReadStreamOperator::operator()(std::ostream* pOStream)
	{
		if (0 == m_refTcpConnector.GetSession()->GetSendBuff().GetSize())
		{
			return 0;
		}

		unsigned short wLen = m_refTcpConnector.m_oBuffContral.Send(
			(char*)m_refTcpConnector.GetSession()->GetSendBuff().GetData()
			, m_refTcpConnector.GetSession()->GetSendBuff().GetSize());

		//unsigned short wLen = m_refTcpConnector.m_oBuffContral.Send(sz.c_str(), sz.size());

		m_refTcpConnector.GetSession()->GetSendBuff().PopData(wLen);
		return wLen;

	}

	CTcpConnector::CTcpConnector(ISession* pSession)
		: CConnectorSocket(pSession)
		, m_funOnRecvOperator(*this)
		, m_funOnConnectedOperator(*this)
		, m_funRecvOperator(*this)
		, m_funSendOperator(*this)
		, m_funReadStreamOperator(*this)
	{
		memset(&m_stRemoteAddr, 0, sizeof(m_stRemoteAddr));
		m_oBuffContral.SetOnRecvOperator(&m_funOnRecvOperator)
			.SetOnConnectedOperator(&m_funOnConnectedOperator)
			.SetRecvOperator(&m_funRecvOperator)
			.SetSendOperator(&m_funSendOperator)
			.SetReadStreamOperator(&m_funReadStreamOperator)
			;
	}

	CTcpConnector::~CTcpConnector()
	{
	}

	int CTcpConnector::Init(std::ostream* pOStream, int dwState)
	{
		m_oBuffContral.SetAckOutTime(5.);
		if (int dwError = m_oBuffContral.Init(dwState))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "init failed:" << dwError
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		return 0;
	}

	int CTcpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << ", error: " << m_dwError
#ifndef _WIN32
			<< ", " << m_bReadable << ", " << m_bWritable
#endif // !_WIN32
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		if (GetError())
		{
			return 0;
		}

#ifdef _WIN32
#else
		if (int dwError = m_oBuffContral.ReceiveMessages(dTimedouble, m_bReadable,  pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << " SendMessages failed (" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			return dwError;
		}
#endif // _WIN32

		
		if (int dwError = m_oBuffContral.SendMessages(dTimedouble, pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << "SendMessages failed ("<< dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return dwError;
		}

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

			//post 到iomodule 移除
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
		pOperation->m_stWsaBuff.buf = pOperation->m_szRecvBuff;
		pOperation->m_stWsaBuff.len = sizeof(pOperation->m_szRecvBuff);
		memset(pOperation->m_stWsaBuff.buf, 0, pOperation->m_stWsaBuff.len);
		//m_setIOOperations.insert(pOperation);
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
		m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream);
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
		int buf_size = 256 * 1024;
		setsockopt(NativeSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size));
		setsockopt(NativeSocket(), SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size));

		// connect
		if (connect(NativeSocket(), (sockaddr*)&address, sizeof(address)))
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "connect failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
			return dwError;
		}

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

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << ", " << m_oBuffContral.GetState()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		return 0;
	}



};
