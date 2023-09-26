#include "tcp_connector.h"
#include "../include/net_work_stream.h"
#include "../include/error_code.h"
#include "../include/log_utility.h"
#include "iothread.h"

#ifdef _WIN32
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

#include <assert.h>

#include <iostream>

namespace FXNET
{
	class TCPConnectorIOReadOperation : public IOOperationBase
	{
	public:
		friend class CTcpConnector;
		virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		sockaddr_in m_stRemoteAddr;
		unsigned int m_dwLen;
#endif // _WIN32
	};

	ErrorCode TCPConnectorIOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(TCPConnectorIOReadOperation, this);

		CTcpConnector& refConnector = (CTcpConnector&)refSocketBase;
#ifdef _WIN32
		refConnector.GetSession()->GetRecvBuff().PushData(dwLen);
		while (refConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			MessageRecvEventBase* pOperator = refConnector.GetSession()->NewRecvMessageEvent();
			refConnector.GetSession()->GetRecvBuff().PopData(pOperator->m_oPackage);

			if (NULL == pOperator)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " failed " << CODE_ERROR_NET_PARSE_MESSAGE
					<< "\n";
				return ErrorCode(CODE_ERROR_NET_PARSE_MESSAGE, __FILE__ ":" __LINE2STR__(__LINE__));
			}

			GetFxIoModule(refConnector.GetIOModuleIndex())->PushMessageEvent(pOperator);
		}
		
		refConnector.PostRecv(pOStream);
#else
		refConnector.m_bReadable = true;
		while (refConnector.m_bReadable && refConnector.GetSession()->GetRecvBuff().GetFreeSize())
		{
			int dwLen = recv(refConnector.NativeSocket()
				, refConnector.GetSession()->GetRecvBuff().GetData() + refConnector.GetSession()->GetRecvBuff().GetSize()
				, refConnector.GetSession()->GetRecvBuff().GetFreeSize(), 0);

			if (0 > dwLen)
			{
				refConnector.m_bReadable = false;
				int dwError = errno;

				if (dwError != EAGAIN && dwError != EINTR) { return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__)); }
			}
			else if (0 == dwLen)
			{
				return ErrorCode(CODE_SUCCESS_NET_EOF, __FILE__ ":" __LINE2STR__(__LINE__));
			}
			else
			{
				refConnector.GetSession()->GetRecvBuff().PushData(dwLen);
				while (refConnector.GetSession()->GetRecvBuff().CheckPackage())
				{
					MessageRecvEventBase* pOperator = refConnector.GetSession()->NewRecvMessageEvent();
					refConnector.GetSession()->GetRecvBuff().PopData(pOperator->m_oPackage);

					if (NULL == pOperator)
					{
						LOG(pOStream, ELOG_LEVEL_ERROR) << refConnector.NativeSocket() << " failed " << CODE_ERROR_NET_PARSE_MESSAGE
							<< "\n";
						return ErrorCode(CODE_ERROR_NET_PARSE_MESSAGE, __FILE__ ":" __LINE2STR__(__LINE__));
					}

					GetFxIoModule(refConnector.GetIOModuleIndex())->PushMessageEvent(pOperator);
				}

				continue;
			}
		}
#endif // _WIN32

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
			<< " ip:" << inet_ntoa(refConnector.GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(refConnector.GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(refConnector.GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(refConnector.GetRemoteAddr().sin_port)
			<< "\n";

		return ErrorCode();
	}

	TCPConnectorIOReadOperation& NewTCPConnectorIOReadOperation(CTcpConnector* pConnector)
	{
		TCPConnectorIOReadOperation* pOperation = new TCPConnectorIOReadOperation();
#ifdef _WIN32
		pOperation->m_stWsaBuff.buf = (char*)pConnector->GetSession()->GetRecvBuff().GetData()
			+ pConnector->GetSession()->GetRecvBuff().GetSize();
		pOperation->m_stWsaBuff.len = pConnector->GetSession()->GetRecvBuff().GetFreeSize();
#endif // _WIN32

		return *pOperation;
	}
	class TCPConnectorIOWriteOperation : public IOOperationBase
	{
	public:
		friend class CTcpConnector;
		virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPConnectorIOWriteOperation, this);
			CTcpConnector& refConnector = (CTcpConnector&)refSocketBase;

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refConnector.NativeSocket()
				<< "\n";

#ifdef _WIN32
			GetFxIoModule(refConnector.GetIOModuleIndex())->PushMessageEvent(refConnector.GetSession()->NewOnSendEvent(dwLen));
#else
			if (!refConnector.m_bConnecting)
			{
				int dwConnectError = 0;
				socklen_t dwLen = sizeof(dwConnectError);
				if (0 > getsockopt(refConnector.NativeSocket(), SOL_SOCKET, SO_ERROR, (void*)(&dwConnectError), &dwLen))
				{
					int dwError = errno;
					return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
				}
				if (dwConnectError) return dwConnectError;
				refConnector.m_bConnecting = true;
				refConnector.OnConnected(pOStream);
			}
			refConnector.m_bWritable = true;
#endif // _WIN32

			return refConnector.SendMessage(pOStream);
		}
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		std::string m_strData;
#endif // _WIN32
	};

	TCPConnectorIOWriteOperation& NewTCPConnectorIOWriteOperation()
	{
		TCPConnectorIOWriteOperation* pOperation = new TCPConnectorIOWriteOperation();
#ifdef _WIN32
		//m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	class TCPConnectorIOErrorOperation : public IOOperationBase
	{
	public:
		friend class CTcpConnector;
		virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPConnectorIOErrorOperation, this);

			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << "(" << m_oError.What() << ")"
				<< "\n";
			if (NULL == ((CTcpConnector&)refSocketBase).GetSession())
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " already wait delete (" << m_oError.What() << ")"
					<< "\n";
				return ErrorCode();
			}

			macro_closesocket(refSocketBase.NativeSocket());

			//�������
			GetFxIoModule(refSocketBase.GetIOModuleIndex())->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_oError));
			GetFxIoModule(refSocketBase.GetIOModuleIndex())->PushMessageEvent(((CTcpConnector&)refSocketBase).GetSession()->NewCloseEvent());

			((CTcpConnector&)refSocketBase).SetSession(NULL);

			return ErrorCode();
		}
	};

	CTcpConnector::CTcpConnector(ISession* pSession)
		: CConnectorSocket(pSession)
#ifndef _WIN32
		, m_bConnecting(false)
#endif // _WIN32
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

	ErrorCode CTcpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG4) << NativeSocket() << ", error: " << m_oError.What()
			<< "\n";

		return ErrorCode();
	}

	ErrorCode CTcpConnector::Connect(sockaddr_in address, std::ostream* pOStream)
	{
#ifdef _WIN32
		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocketType hSock = socket(AF_INET, SOCK_STREAM, 0);
#endif // _WIN32

		if (hSock == -1)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "create socket failed."
				<< "\n";;
			return ErrorCode(CODE_ERROR_NET_ERROR_SOCKET, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		if (int dwError = this->Init(pOStream, ST_SYN_SEND))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << dwError << ")"
				<<"[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			//post ��iomodule �Ƴ�
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		if (ErrorCode oError = this->SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "client connect failed(" << oError.What() << ")"
				<< "\n";
			return oError;
		}

		//��������ʱ Windows��linux���������//
#ifdef _WIN32
		sockaddr_in stLocalAddr;
		ZeroMemory(&stLocalAddr, sizeof(sockaddr_in));
		stLocalAddr.sin_family = AF_INET;

		if (int dwError = ::bind(NativeSocket(), (sockaddr*)(&stLocalAddr), sizeof(sockaddr_in)))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "bind failed(" << dwError << ")"
				<< "\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}
#else
		if (-1 == connect(this->NativeSocket(), (sockaddr*)&this->GetRemoteAddr(), sizeof(this->GetRemoteAddr())))
		{
			int dwError = errno;
			if (dwError != EINPROGRESS && dwError != EINTR && dwError != EAGAIN)
			{ 
				macro_closesocket(this->NativeSocket());
				LOG(pOStream, ELOG_LEVEL_ERROR) << "failed(" << dwError << ")"
					<< "\n";
				return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			}
		}
#endif

		if (int dwError =
#ifdef _WIN32
			GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), this, pOStream)
#else
			GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), EPOLLET | EPOLLIN | EPOLLOUT, this, pOStream)
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
				<< "\n";

			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

#ifdef _WIN32
		LPFN_CONNECTEX lpfnConnectEx = NULL;
		DWORD dwBytes = 0;
		GUID GuidConnectEx = WSAID_CONNECTEX;

		if (SOCKET_ERROR == WSAIoctl(this->NativeSocket(), SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidConnectEx, sizeof(GuidConnectEx),
			&lpfnConnectEx, sizeof(lpfnConnectEx), &dwBytes, 0, 0))
		{
			int dwError = WSAGetLastError();
			macro_closesocket(this->NativeSocket());
			LOG(pOStream, ELOG_LEVEL_ERROR) << "failed(" << dwError << ")"
				<< "\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

		class IOConnectOperation : public IOOperationBase
		{
		public:
			virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
			{
				DELETE_WHEN_DESTRUCT(IOConnectOperation, this);

				CTcpConnector& refConnector = (CTcpConnector&)refSocketBase;
				refConnector.OnConnected(pOStream);

				return ErrorCode();
			}
		};

		IOConnectOperation* pOperation = new IOConnectOperation;

		memset((OVERLAPPED*)pOperation, 0, sizeof(OVERLAPPED));
		// ���ʱ�� ��û������

		if (0 == lpfnConnectEx(this->NativeSocket()
			, (sockaddr*)&address			// [in] �Է���ַ
			, sizeof(address)				// [in] �Է���ַ����
			, NULL									// [in] ���Ӻ�Ҫ���͵����ݣ����ﲻ��
			, 0										// [in] �������ݵ��ֽ��� �����ﲻ��
			, NULL									// [out] �����˶��ٸ��ֽڣ����ﲻ��
			, (OVERLAPPED*)pOperation))							// [in]
		{
			int dwError = (unsigned int)WSAGetLastError();
			if (dwError != ERROR_IO_PENDING && dwError != ERROR_IO_INCOMPLETE)
			{
				macro_closesocket(this->NativeSocket());
				LOG(pOStream, ELOG_LEVEL_ERROR) << "failed(" << dwError << ")"
					<< "\n";
				return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			}
		}

#endif // _WIN32

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:" << inet_ntoa(this->GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "\n";

		return ErrorCode();
	}

	void CTcpConnector::Close(std::ostream* pOStream)
	{
		GetFxIoModule(this->GetIOModuleIndex())->DeregisterIO(this->NativeSocket(), pOStream);
		this->NewErrorOperation(CODE_SUCCESS_NET_EOF)(*this, 0, pOStream);
	}

	IOOperationBase& CTcpConnector::NewReadOperation()
	{
		return NewTCPConnectorIOReadOperation(this);
	}

	IOOperationBase& CTcpConnector::NewWriteOperation()
	{
		return NewTCPConnectorIOWriteOperation();
	}

	IOOperationBase& CTcpConnector::NewErrorOperation(const ErrorCode& refError)
	{
		TCPConnectorIOErrorOperation* pOperation = new TCPConnectorIOErrorOperation();
		pOperation->m_oError = refError;
		m_oError = refError;
		return *pOperation;
	}

	ErrorCode CTcpConnector::SendMessage(std::ostream* pOStream)
	{
#ifdef _WIN32
		return this->PostSend(pOStream);
#else
		// send as much data as we can.
		while (this->m_bWritable && this->GetSession()->GetSendBuff().GetSize())
		{
			int dwLen = send(this->NativeSocket()
				, this->GetSession()->GetSendBuff().GetData()
				, this->GetSession()->GetSendBuff().GetSize(), 0);

			if (0 > dwLen)
			{
				int dwError = errno;
				this->m_bWritable = false;

				if (dwError == EAGAIN)
					break;
				else
				{
					LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << "(" << dwError << ")"
						<< "\n";
					return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
				}
			}

			if (0 == dwLen) break;
			this->GetSession()->GetSendBuff().PopData(dwLen);

			GetFxIoModule(this->GetIOModuleIndex())->PushMessageEvent(GetSession()->NewOnSendEvent(dwLen));
			if (0 == this->GetSession()->GetSendBuff().GetSize()) break;
		}
#endif // _WIN32

		return ErrorCode();
	}

#ifdef _WIN32
	CTcpConnector& CTcpConnector::PostRecv(std::ostream* pOStream)
	{
		TCPConnectorIOReadOperation& refIOReadOperation = NewTCPConnectorIOReadOperation(this);

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSARecv(NativeSocket(), &refIOReadOperation.m_stWsaBuff
			, 1, &dwReadLen, &dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOReadOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << ", " << "PostRecv failed."<< dwError
					<< "\n";;
			}
		}

		return *this;
	}

	ErrorCode CTcpConnector::PostSend(std::ostream* pOStream)
	{
		if (0 == this->GetSession()->GetSendBuff().GetSize())
		{
			return ErrorCode();
		}
		TCPConnectorIOWriteOperation& refIOWriteOperation = NewTCPConnectorIOWriteOperation();
		refIOWriteOperation.m_strData.resize(this->GetSession()->GetSendBuff().GetSize());
		memcpy(&( * refIOWriteOperation.m_strData.begin())
			, this->GetSession()->GetSendBuff().GetData()
			, this->GetSession()->GetSendBuff().GetSize());
		this->GetSession()->GetSendBuff().PopData(GetSession()->GetSendBuff().GetSize());
		//GetSession()->GetSendBuff().PopData(refIOWriteOperation.m_strData);
		refIOWriteOperation.m_stWsaBuff.buf = &(*refIOWriteOperation.m_strData.begin());
		refIOWriteOperation.m_stWsaBuff.len = (unsigned long)refIOWriteOperation.m_strData.size();

		DWORD dwWriteLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSASend(NativeSocket(), &refIOWriteOperation.m_stWsaBuff
			, 1, &dwWriteLen, dwFlags, (OVERLAPPED*)(IOOperationBase*)&refIOWriteOperation, NULL))
		{
			int dwError = WSAGetLastError();
			if (WSA_IO_PENDING != dwError)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << NativeSocket() << ", " << "PostSend failed." << dwError
					<< "\n";;
				// delete& refIOWriteOperation;
				return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			}

		}

		return ErrorCode();
	}

#endif // _WIN32

	void CTcpConnector::OnError(const ErrorCode& refError, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << ", " << refError.What()
			<< "\n";
	}

	void CTcpConnector::OnClose(std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket()
			<< "\n";
		delete this;
	}

	void CTcpConnector::OnConnected(std::ostream* pOStream)
	{
		socklen_t dwAddrLen = sizeof(this->GetLocalAddr());
		getsockname(this->NativeSocket(), (sockaddr*)&this->GetLocalAddr(), &dwAddrLen);

		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket() << " ip:" << inet_ntoa(this->GetLocalAddr().sin_addr)
			<< ", port:" << (int)ntohs(this->GetLocalAddr().sin_port)
			<< " remote_ip:" << inet_ntoa(this->GetRemoteAddr().sin_addr)
			<< ", remote_port:" << (int)ntohs(this->GetRemoteAddr().sin_port)
			<< "\n";

		GetFxIoModule(this->GetIOModuleIndex())->PushMessageEvent(this->GetSession()->NewConnectedEvent());

#ifdef _WIN32
		this->PostRecv(pOStream);
#endif // _WIN32

	}

	ErrorCode CTcpConnector::Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
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
				<< "\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			int dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ")"
				<< "\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}
#endif

		// tcp nodelay
		int nodelay = 1;
		setsockopt(this->NativeSocket(), IPPROTO_TCP, TCP_NODELAY, (char*)&nodelay, sizeof(nodelay));

		// linger
		linger l;
		l.l_onoff = 0;
		l.l_linger = 0;
		setsockopt(this->NativeSocket(), SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof(l));

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket()
			<< "\n";
		return ErrorCode();
	}



};
