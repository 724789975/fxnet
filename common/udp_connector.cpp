#include "udp_connector.h"
#include "../include/iothread.h"
#include "../include/net_work_stream.h"
#include "../include/error_code.h"
#include "../include/log_utility.h"

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
	ErrorCode CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOReadOperation, this);

		CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;
#ifdef _WIN32
		refConnector.PostRecv(pOStream);

		m_dwLen = dwLen;
		refConnector.m_funRecvOperator.SetIOReadOperation(this);
		bool bReadable = true;
#else
		refConnector.m_bReadable = true;
		bool& bReadable = refConnector.m_bReadable;
#endif // _WIN32

		if (ErrorCode oError = refConnector.m_oBuffContral.ReceiveMessages(FxIoModule::Instance()->FxGetCurrentTime(), bReadable, pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOReadOperation failed " << oError.What()
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return oError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket() << ", " << bReadable
			<< " ip:" << inet_ntoa(refConnector.GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(refConnector.GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(refConnector.GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(refConnector.GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		return ErrorCode();
	}

	ErrorCode CUdpConnector::IOWriteOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOWriteOperation, this);
		CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
#ifdef _WIN32
#else
		refConnector.m_bWritable = true;
#endif // _WIN32

		if (ErrorCode oError = refConnector.m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " IOWriteOperation failed " << oError.What()
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return oError;
		}
		return 0;
	}

	ErrorCode CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOErrorOperation, this);

		LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << "(" << m_oError.What() << ")"
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		if (NULL == ((CUdpConnector&)refSocketBase).GetSession())
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " already wait delete (" << m_oError.What() << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode();
		}

		macro_closesocket(refSocketBase.NativeSocket());

		//处理错误
		FxIoModule::Instance()->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_oError));
		FxIoModule::Instance()->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewCloseEvent());

		((CUdpConnector&)refSocketBase).SetSession(NULL);

		return ErrorCode();
	}

	CUdpConnector::UDPOnRecvOperator::UDPOnRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	ErrorCode CUdpConnector::UDPOnRecvOperator::operator()(char* szBuff, unsigned short wSize, std::ostream* pOStream)
	{
		//收到的内容
		if (0 == wSize)
		{
			return ErrorCode();
		}

		this->m_refUdpConnector.GetSession()->GetRecvBuff().PushData(szBuff, wSize);
		while (this->m_refUdpConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			MessageRecvEventBase* pOperator = this->m_refUdpConnector.GetSession()->NewRecvMessageEvent();
			this->m_refUdpConnector.GetSession()->GetRecvBuff().PopData(pOperator->m_oPackage);

			if (NULL == pOperator)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->m_refUdpConnector.NativeSocket() << " UDPOnRecvOperator failed " << CODE_ERROR_NET_PARSE_MESSAGE
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				return ErrorCode(CODE_ERROR_NET_PARSE_MESSAGE, __FILE__ ":" __LINE2STR__(__LINE__));
			}

			FxIoModule::Instance()->PushMessageEvent(pOperator);
		}

		return ErrorCode();
	}

	CUdpConnector::UDPOnConnectedOperator::UDPOnConnectedOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	ErrorCode CUdpConnector::UDPOnConnectedOperator::operator()(std::ostream* pOStream)
	{
		this->m_refUdpConnector.OnConnected(pOStream);
		return ErrorCode();
	}

	CUdpConnector::UDPRecvOperator::UDPRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
#ifdef _WIN32
		, m_pReadOperation(NULL)
#endif // _WIN32
	{
	}

	ErrorCode CUdpConnector::UDPRecvOperator::operator()(char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream)
	{
#ifdef _WIN32
		//没有可读事件
		if (!this->m_pReadOperation)
		{
			return ErrorCode(CODE_SUCCESS_NO_BUFF_READ, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		wRecvSize = (unsigned short)this->m_pReadOperation->m_dwLen;
		if (0 == this->m_pReadOperation)
		{
			return ErrorCode(CODE_SUCCESS_NET_EOF, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		assert(wRecvSize <= UDP_WINDOW_BUFF_SIZE);
		assert(this->m_pReadOperation->m_dwLen <= UDP_WINDOW_BUFF_SIZE);
		memcpy(pBuff, this->m_pReadOperation->m_stWsaBuff.buf, wRecvSize);
		SetIOReadOperation(NULL);
#else
		wRecvSize = recv(this->m_refUdpConnector.NativeSocket(), pBuff, wBuffSize, 0);
		if (0 == wRecvSize)
		{
			return ErrorCode(CODE_SUCCESS_NET_EOF, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		if (0 > wRecvSize)
		{
			this->m_refUdpConnector.m_bReadable = false;
			return ErrorCode(errno, __FILE__ ":" __LINE2STR__(__LINE__));
		}
#endif // _WIN32

		LOG(pOStream, ELOG_LEVEL_DEBUG4) << pBuff + 7
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
		return ErrorCode();
	}

#ifdef _WIN32
	CUdpConnector::UDPRecvOperator& CUdpConnector::UDPRecvOperator::SetIOReadOperation(IOReadOperation* pReadOperation)
	{
		this->m_pReadOperation = pReadOperation;
		return *this;
	}
#endif // _WIN32

	CUdpConnector::UDPSendOperator::UDPSendOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	ErrorCode CUdpConnector::UDPSendOperator::operator()(char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->m_refUdpConnector.NativeSocket()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
#ifdef _WIN32
		dwSendLen = wBufferSize;
		return this->m_refUdpConnector.PostSend(szBuff, wBufferSize, pOStream);
#else
		dwSendLen = send(this->m_refUdpConnector.NativeSocket(), szBuff, wBufferSize, 0);
		if (0 > dwSendLen)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "UDPSendOperator failed " << errno
				<< ", BufferSize : " << wBufferSize << ", " << this->m_refUdpConnector.NativeSocket()
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(errno, __FILE__ ":" __LINE2STR__(__LINE__));
		}
#endif // _WIN32
		LOG(pOStream, ELOG_LEVEL_DEBUG4) << szBuff + 7
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
		return ErrorCode();
	}

	CUdpConnector::UDPReadStreamOperator::UDPReadStreamOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	unsigned int CUdpConnector::UDPReadStreamOperator::operator()(std::ostream* pOStream)
	{
		if (0 == this->m_refUdpConnector.GetSession()->GetSendBuff().GetSize())
		{
			return 0;
		}

		unsigned int dwLen = this->m_refUdpConnector.m_oBuffContral.Send(
			(char*)this->m_refUdpConnector.GetSession()->GetSendBuff().GetData()
			, this->m_refUdpConnector.GetSession()->GetSendBuff().GetSize());

		if (dwLen)
		{
			this->m_refUdpConnector.GetSession()->GetSendBuff().PopData(dwLen);

			FxIoModule::Instance()->PushMessageEvent(this->m_refUdpConnector.GetSession()->NewOnSendEvent(dwLen));

			LOG(pOStream, ELOG_LEVEL_DEBUG4) << (char*)this->m_refUdpConnector.GetSession()->GetSendBuff().GetData() + 4
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
		}

		return dwLen;

	}

	CUdpConnector::CUdpConnector(ISession* pSession)
		: CConnectorSocket(pSession)
		, m_funOnRecvOperator(*this)
		, m_funOnConnectedOperator(*this)
		, m_funRecvOperator(*this)
		, m_funSendOperator(*this)
		, m_funReadStreamOperator(*this)
	{
		memset(&this->m_stRemoteAddr, 0, sizeof(this->m_stRemoteAddr));
		this->m_oBuffContral.SetOnRecvOperator(&this->m_funOnRecvOperator)
			.SetOnConnectedOperator(&this->m_funOnConnectedOperator)
			.SetRecvOperator(&this->m_funRecvOperator)
			.SetSendOperator(&this->m_funSendOperator)
			.SetReadStreamOperator(&this->m_funReadStreamOperator)
			;
	}

	CUdpConnector::~CUdpConnector()
	{
	}

	ErrorCode CUdpConnector::Init(std::ostream* pOStream, int dwState)
	{
		if (int dwError = this->m_oBuffContral.SetAckOutTime(5.).Init(dwState))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "init failed:" << dwError
				<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		return 0;
	}

	ErrorCode CUdpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->NativeSocket() << ", error: " << this->m_oError
#ifndef _WIN32
			<< ", " << this->m_bReadable << ", " << this->m_bWritable
#endif // !_WIN32
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		if (this->GetError())
		{
			return ErrorCode();
		}

		if (ErrorCode oError = m_oBuffContral.SendMessages(dTimedouble, pOStream))
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << "SendMessages failed ("<< oError.What() << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return oError;
		}

		return ErrorCode();
	}

	ErrorCode CUdpConnector::Connect(sockaddr_in address, std::ostream* pOStream)
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
			return ErrorCode(-1, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		if (ErrorCode oError = this->Init(pOStream, ST_SYN_SEND))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << oError.What() << ")"
				<<"[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			//post 到iomodule 移除
			return oError;
		}

		if (ErrorCode oError = this->SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << oError.What() << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:"
			<< inet_ntoa(this->GetLocalAddr().sin_addr) << ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		for (int i = 0; i < 16; ++i)
		{
			this->PostRecv(pOStream);
		}
#endif // _WIN32

		return ErrorCode();
	}

	void CUdpConnector::Close(std::ostream* pOStream)
	{
		this->NewErrorOperation(CODE_SUCCESS_NET_EOF)(*this, 0, pOStream);
	}

	CUdpConnector::IOReadOperation& CUdpConnector::NewReadOperation()
	{
		CUdpConnector::IOReadOperation* pOperation = new CUdpConnector::IOReadOperation();
#ifdef _WIN32
		pOperation->m_stWsaBuff.buf = pOperation->m_szRecvBuff;
		pOperation->m_stWsaBuff.len = sizeof(pOperation->m_szRecvBuff);
		memset(pOperation->m_stWsaBuff.buf, 0, pOperation->m_stWsaBuff.len);
		//m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	CUdpConnector::IOWriteOperation& CUdpConnector::NewWriteOperation()
	{
		CUdpConnector::IOWriteOperation* pOperation = new CUdpConnector::IOWriteOperation();
#ifdef _WIN32
		//m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	CUdpConnector::IOErrorOperation& CUdpConnector::NewErrorOperation(const ErrorCode& refError)
	{
		CUdpConnector::IOErrorOperation* pOperation = new CUdpConnector::IOErrorOperation();
		pOperation->m_oError = refError;
		m_oError = refError;
		return *pOperation;
	}

	ErrorCode CUdpConnector::SendMessage(std::ostream* pOStream)
	{
		return m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream);
	}

#ifdef _WIN32
	CUdpConnector& CUdpConnector::PostRecv(std::ostream* pOStream)
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

	ErrorCode CUdpConnector::PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream)
	{
		IOWriteOperation& refIOWriteOperation = this->NewWriteOperation();
		refIOWriteOperation.m_stWsaBuff.buf = pBuff;
		refIOWriteOperation.m_stWsaBuff.len = wLen;

		DWORD dwWriteLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSASend(this->NativeSocket(), &refIOWriteOperation.m_stWsaBuff
			, 1, &dwWriteLen, dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOWriteOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << ", " << "PostSend failed." << dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				delete &refIOWriteOperation;
			}

			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		return 0;
	}

#endif // _WIN32

	void CUdpConnector::OnError(const ErrorCode& refError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << ", " << refError.What()
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
	}

	void CUdpConnector::OnClose(std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket()
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
		delete this;
	}

	void CUdpConnector::OnConnected(std::ostream* pOStream)
	{
		socklen_t dwAddrLen = sizeof(this->GetLocalAddr());
		getsockname(this->NativeSocket(), (sockaddr*)&this->GetLocalAddr(), &dwAddrLen);

		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket()
			<< " ip:" << inet_ntoa(this->GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		FxIoModule::Instance()->PushMessageEvent(this->GetSession()->NewConnectedEvent());
	}

	ErrorCode CUdpConnector::Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		this->NativeSocket() = hSock;

		LOG(pOStream, ELOG_LEVEL_INFO)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(this->NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(this->NativeSocket(), F_SETFL, fcntl(this->NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif //_WIN32
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			int dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}
#endif

		// set ttl
		int ttl = 128;
		if (setsockopt(this->NativeSocket(), IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)))
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "setsockopt failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		// set recv buffer size and send buffer size
		int buf_size = 256 * 1024;
		setsockopt(this->NativeSocket(), SOL_SOCKET, SO_RCVBUF, (char*)&buf_size, sizeof(buf_size));
		setsockopt(this->NativeSocket(), SOL_SOCKET, SO_SNDBUF, (char*)&buf_size, sizeof(buf_size));

		// connect
		if (connect(this->NativeSocket(), (sockaddr*)&address, sizeof(address)))
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "connect failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		if (int dwError =
#ifdef _WIN32
			FxIoModule::Instance()->RegisterIO(this->NativeSocket(), this, pOStream)
#else
			FxIoModule::Instance()->RegisterIO(this->NativeSocket(), EPOLLET | EPOLLIN | EPOLLOUT, this, pOStream)
#endif // _WIN32
			)
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "register io failed"
				<< " ip:" << inet_ntoa(this->GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << ", " << m_oBuffContral.GetState()
			<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		return 0;
	}



};
