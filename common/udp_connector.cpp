#include "udp_connector.h"
#include "../include/net_work_stream.h"
#include "../include/error_code.h"
#include "../include/log_utility.h"
#include "iothread.h"

#ifdef _WIN32
#ifndef macro_closesocket
#define macro_closesocket closesocket
#endif  //macro_closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#ifndef macro_closesocket
#define macro_closesocket close
#endif  //macro_closesocket
#endif  // _WIN32

#include <assert.h>

#include <iostream>

namespace FXNET
{
	class UDPConnectorIOReadOperation: public IOOperationBase
	{
	public: 
		friend class CUdpConnector;
		virtual UDPConnectorIOReadOperation& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnectorIOReadOperation, this);

			CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;

#ifdef _WIN32
			refConnector.m_setOperation.erase(this);
#endif
			if (refSocketBase.GetError())
			{
				refError(refSocketBase.GetError());
				return *this;
			}

#ifdef _WIN32
			refConnector.m_setOperation.erase(this);
			refConnector.PostRecv(pOStream);

			m_dwLen = dwLen;
			refConnector.m_funRecvOperator.SetIOReadOperation(this);
			bool bReadable = true;
#else
			refConnector.m_bReadable = true;
			bool& bReadable          = refConnector.m_bReadable;
#endif  // _WIN32

			refConnector.m_oBuffContral.ReceiveMessages(GetFxIoModule(refConnector.GetIOModuleIndex())->FxGetCurrentTime(), bReadable, refError, pOStream);
			if (refError)
			{
				//此处有报错
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOReadOperation failed " << refError.What()
					<< "\n";

				return *this;
			}

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket() << ", " << bReadable
				<< " ip:" << inet_ntoa(refConnector.GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(refConnector.GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(refConnector.GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(refConnector.GetRemoteAddr().sin_port)
				<< "\n";

			return *this;
		}
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		sockaddr_in m_stRemoteAddr;
		unsigned int m_dwLen;
#endif  // _WIN32
		char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
	};

	UDPConnectorIOReadOperation& NewUDPConnectorIOReadOperation()
	{
		UDPConnectorIOReadOperation* pOperation = new UDPConnectorIOReadOperation();
#ifdef _WIN32
		pOperation->m_stWsaBuff.buf = pOperation->m_szRecvBuff;
		pOperation->m_stWsaBuff.len = sizeof(pOperation->m_szRecvBuff);
		memset(pOperation->m_stWsaBuff.buf, 0, pOperation->m_stWsaBuff.len);
		//m_setIOOperations.insert(pOperation);
#endif  // _WIN32

		return *pOperation;
	}

	class UDPConnectorIOWriteOperation: public IOOperationBase
	{
	public: 
		friend class CUdpConnector;
		virtual UDPConnectorIOWriteOperation& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnectorIOWriteOperation, this);
			CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
				<< "\n";
#ifdef _WIN32
			refConnector.m_setOperation.erase(this);
#else
			refConnector.m_bWritable = true;
#endif  // _WIN32

			if (refSocketBase.GetError())
			{
				refError(refSocketBase.GetError());
				return *this;
			}

			refConnector.m_oBuffContral.SendMessages(GetFxIoModule(refConnector.GetIOModuleIndex())->FxGetCurrentTime(), refError, pOStream);
			if (refError)
			{
				//此处有报错
				LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " IOWriteOperation failed " << refError.What()
					<< "\n";

				return *this;
			}
			return *this;
		}
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		sockaddr_in m_stRemoteAddr;
#endif  // _WIN32
	};

	UDPConnectorIOWriteOperation& NewUDPConnectorWriteOperation()
	{
		UDPConnectorIOWriteOperation* pOperation = new UDPConnectorIOWriteOperation();
		return *pOperation;
	}

	class UDPConnectorIOErrorOperation: public IOOperationBase
	{
	public: 
		friend class CUdpConnector;
		virtual UDPConnectorIOErrorOperation& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnectorIOErrorOperation, this);

			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << "(" << m_oError.What() << ")"
				<< "\n";
			if (NULL == ((CUdpConnector&)refSocketBase).GetSession())
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " already wait delete (" << m_oError.What() << ")"
					<< "\n";
				return *this;
			}

			macro_closesocket(refSocketBase.NativeSocket());

			//处理错误
			GetFxIoModule(refSocketBase.GetIOModuleIndex())->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_oError));
			GetFxIoModule(refSocketBase.GetIOModuleIndex())->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewCloseEvent());

			((CUdpConnector&)refSocketBase).SetSession(NULL);

			return *this;
		}
	};

	CUdpConnector::UDPOnRecvOperator::UDPOnRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	CUdpConnector::UDPOnRecvOperator& CUdpConnector::UDPOnRecvOperator::operator()
		(char* szBuff, unsigned short wSize, ErrorCode& refError, std::ostream* pOStream)
	{
		//收到的内容
		if (0 == wSize)
		{
			return *this;
		}

		this->m_refUdpConnector.GetSession()->GetRecvBuff().PushData(szBuff, wSize);
		while (this->m_refUdpConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			MessageRecvEventBase* pOperator = this->m_refUdpConnector.GetSession()->NewRecvMessageEvent();
			this->m_refUdpConnector.GetSession()->GetRecvBuff().PopData(pOperator->m_oPackage);

			if (NULL == pOperator)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->m_refUdpConnector.NativeSocket() << " UDPOnRecvOperator failed " << CODE_ERROR_NET_PARSE_MESSAGE
					<< "\n";
				refError(CODE_ERROR_NET_PARSE_MESSAGE, __FILE__ ":" __LINE2STR__(__LINE__));
				return *this;
			}

			GetFxIoModule(this->m_refUdpConnector.GetIOModuleIndex())->PushMessageEvent(pOperator);
		}

		return *this;
	}

	CUdpConnector::UDPOnConnectedOperator::UDPOnConnectedOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	CUdpConnector::UDPOnConnectedOperator& CUdpConnector::UDPOnConnectedOperator::operator()(ErrorCode& refError, std::ostream* pOStream)
	{
		this->m_refUdpConnector.OnConnected(pOStream);
		return *this;
	}

	CUdpConnector::UDPRecvOperator::UDPRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
#ifdef _WIN32
		, m_pReadOperation(NULL)
#endif  // _WIN32
	{
	}

	CUdpConnector::UDPRecvOperator& CUdpConnector::UDPRecvOperator::operator()
		(char* pBuff, unsigned short wBuffSize, int& wRecvSize, ErrorCode& refError, std::ostream* pOStream)
	{
#ifdef _WIN32
		//没有可读事件
		if (!this->m_pReadOperation)
		{
			refError(CODE_SUCCESS_NO_BUFF_READ, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		wRecvSize = (unsigned short)this->m_pReadOperation->m_dwLen;
		if (0 == this->m_pReadOperation)
		{
			refError(CODE_SUCCESS_NET_EOF, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		assert(wRecvSize <= UDP_WINDOW_BUFF_SIZE);
		assert(this->m_pReadOperation->m_dwLen <= UDP_WINDOW_BUFF_SIZE);
		memcpy(pBuff, this->m_pReadOperation->m_stWsaBuff.buf, wRecvSize);
		SetIOReadOperation(NULL);
#else
		wRecvSize = recv(this->m_refUdpConnector.NativeSocket(), pBuff, wBuffSize, 0);
		if (0 == wRecvSize)
		{
			refError(CODE_SUCCESS_NET_EOF, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		if (0 > wRecvSize)
		{
			this->m_refUdpConnector.m_bReadable = false;
			refError(errno, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#endif  // _WIN32

		LOG(pOStream, ELOG_LEVEL_DEBUG4) << pBuff + 7
			<< "\n";
		return *this;
	}

#ifdef _WIN32
	CUdpConnector::UDPRecvOperator& CUdpConnector::UDPRecvOperator::SetIOReadOperation(UDPConnectorIOReadOperation* pReadOperation)
	{
		this->m_pReadOperation = pReadOperation;
		return *this;
	}
#endif  // _WIN32

	CUdpConnector::UDPSendOperator::UDPSendOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	CUdpConnector::UDPSendOperator& CUdpConnector::UDPSendOperator::operator()
		(char* szBuff, unsigned short wBufferSize, int& dwSendLen, ErrorCode& refError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->m_refUdpConnector.NativeSocket()
			<< "\n";
#ifdef _WIN32
		dwSendLen = wBufferSize;
		this->m_refUdpConnector.PostSend(szBuff, wBufferSize, refError, pOStream);
		return *this;
#else
		dwSendLen = send(this->m_refUdpConnector.NativeSocket(), szBuff, wBufferSize, 0);
		if (0 > dwSendLen)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "UDPSendOperator failed " << errno
				<< ", BufferSize : " << wBufferSize << ", " << this->m_refUdpConnector.NativeSocket()
				<< "\n";
			refError(errno, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#endif  // _WIN32
		LOG(pOStream, ELOG_LEVEL_DEBUG4) << szBuff + 7
			<< "\n";
		return *this;
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

			GetFxIoModule(this->m_refUdpConnector.GetIOModuleIndex())->PushMessageEvent(this->m_refUdpConnector.GetSession()->NewOnSendEvent(dwLen));

			LOG(pOStream, ELOG_LEVEL_DEBUG4) << (char*)this->m_refUdpConnector.GetSession()->GetSendBuff().GetData() + 4
				<< "\n";
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
#ifdef _WIN32
		for (std::set<IOOperationBase*>::iterator it = m_setOperation.begin();
			it != m_setOperation.end(); ++it)
		{
			delete* it;
		}
#endif  // _WIN32
	}

	ErrorCode CUdpConnector::Init(std::ostream* pOStream, int dwState)
	{
		if (int dwError = this->m_oBuffContral.SetAckOutTime(5.).Init(dwState, GetFxIoModule(this->GetIOModuleIndex())->FxGetCurrentTime()))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "init failed:" << dwError
				<< "\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		return 0;
	}

	CUdpConnector& CUdpConnector::Update(double dTimedouble, ErrorCode& refError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->NativeSocket() << ", error: " << this->m_oError
#ifndef _WIN32
			<< ", " << this->m_bReadable << ", " << this->m_bWritable
#endif  // !_WIN32
			<< "\n";

		if (this->GetError())
		{
			return *this;
		}

		m_oBuffContral.SendMessages(dTimedouble, refError, pOStream);
		if (refError)
		{
			//此处有报错
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << "SendMessages failed ("<< refError.What() << ")"
				<< "\n";

			return *this;
		}

		return *this;
	}

	CUdpConnector& CUdpConnector::Connect(sockaddr_in address, ErrorCode& refError, std::ostream* pOStream)
	{
#ifdef _WIN32
		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocketType hSock = socket(AF_INET, SOCK_DGRAM, 0);
#endif  // _WIN32

		if (hSock == -1)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "create socket failed."
				<< "\n";;
			refError(-1, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		if (refError(this->Init(pOStream, ST_SYN_SEND)))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << refError.What() << ")"
				<<"[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			return *this;
		}

		this->SetRemoteAddr(address).Connect(hSock, address, refError, pOStream);
		if (refError)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << refError.What() << ")"
				<< "\n";
			return *this;
		}

		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket() << " ip:"
			<< inet_ntoa(this->GetLocalAddr().sin_addr) << ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "\n";

#ifdef _WIN32
		for (int i = 0; i < 16; ++i)
		{
			this->PostRecv(pOStream);
		}
#endif  // _WIN32

		return *this;
	}

	void CUdpConnector::Close(std::ostream* pOStream)
	{
		GetFxIoModule(this->GetIOModuleIndex())->DeregisterIO(this->NativeSocket(), pOStream);
		ErrorCode oError;
		this->NewErrorOperation(CODE_SUCCESS_NET_EOF)(*this, 0, oError, pOStream);
	}

	IOOperationBase& CUdpConnector::NewReadOperation()
	{
		return NewUDPConnectorIOReadOperation();
	}

	IOOperationBase& CUdpConnector::NewWriteOperation()
	{
		return NewUDPConnectorWriteOperation();
	}

	IOOperationBase& CUdpConnector::NewErrorOperation(const ErrorCode& refError)
	{
		UDPConnectorIOErrorOperation* pOperation = new UDPConnectorIOErrorOperation();
		pOperation->m_oError                     = refError;
		m_oError                                 = refError;
		return *pOperation;
	}

	CUdpConnector& CUdpConnector::SendMessage(ErrorCode& refEror, std::ostream* pOStream)
	{
		m_oBuffContral.SendMessages(GetFxIoModule(this->GetIOModuleIndex())->FxGetCurrentTime(), refEror, pOStream);
		return *this;
	}

#ifdef _WIN32
	CUdpConnector& CUdpConnector::PostRecv(std::ostream* pOStream)
	{
		UDPConnectorIOReadOperation& refIOReadOperation = NewUDPConnectorIOReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags   = 0;

		m_setOperation.insert(&refIOReadOperation);
		if (SOCKET_ERROR == WSARecv(NativeSocket(), &refIOReadOperation.m_stWsaBuff
			, 1, &dwReadLen, &dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOReadOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_DEBUG2) << NativeSocket() << ", " << "PostRecv failed."<< dwError
					<< "\n";;
			}
		}

		return *this;
	}

	CUdpConnector& CUdpConnector::PostSend(char* pBuff, unsigned short wLen, ErrorCode& refError, std::ostream* pOStream)
	{
		UDPConnectorIOWriteOperation& refIOWriteOperation = NewUDPConnectorWriteOperation();
		refIOWriteOperation.m_stWsaBuff.buf               = pBuff;
		refIOWriteOperation.m_stWsaBuff.len               = wLen;

		DWORD dwWriteLen = 0;
		DWORD dwFlags    = 0;

		m_setOperation.insert(&refIOWriteOperation);
		if (SOCKET_ERROR == WSASend(this->NativeSocket(), &refIOWriteOperation.m_stWsaBuff
			, 1, &dwWriteLen, dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOWriteOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << ", " << "PostSend failed." << dwError
					<< "\n";;
				delete &refIOWriteOperation;
			}

			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		return *this;
	}

#endif  // _WIN32

	void CUdpConnector::OnError(const ErrorCode& refError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << ", " << refError.What()
			<< "\n";
	}

	void CUdpConnector::OnClose(std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket()
			<< "\n";
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
			<< "\n";

		GetFxIoModule(this->GetIOModuleIndex())->PushMessageEvent(this->GetSession()->NewConnectedEvent());
	}

	CUdpConnector& CUdpConnector::Connect(NativeSocketType hSock, const sockaddr_in& address, ErrorCode& refError, std::ostream* pOStream)
	{
		this->NativeSocket() = hSock;

		LOG(pOStream, ELOG_LEVEL_INFO)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "\n";

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(this->NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(this->NativeSocket(), F_SETFL, fcntl(this->NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif  //_WIN32
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else  // _WIN32
			int dwError = errno;
#endif  // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			int dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ")"
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#endif

		// set ttl
		int ttl = 128;
		if (setsockopt(this->NativeSocket(), IPPROTO_IP, IP_TTL, (char*)&ttl, sizeof(ttl)))
		{
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else  // _WIN32
			int dwError = errno;
#endif  // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "setsockopt failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
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
#else  // _WIN32
			int dwError = errno;
#endif  // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "connect failed(" << dwError << ")"
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		if (int dwError =
#ifdef _WIN32
			GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), this, pOStream)
#else
			GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), EPOLLET | EPOLLIN | EPOLLOUT, this, pOStream)
#endif  // _WIN32
			)
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "register io failed"
				<< " ip:" << inet_ntoa(this->GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
				<< "\n";

			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << ", " << m_oBuffContral.GetState()
			<< "\n";
		return *this;
	}



};
