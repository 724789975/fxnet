#include "udp_connector.h"
#include "include/iothread.h"
#include "include/net_work_stream.h"
#include "include/error_code.h"

#ifdef _WIN32
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __FUNCSIG__
#endif
#else
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __PRETTY_FUNCTION__
#endif
#endif //!_WIN32

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
	int CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOReadOperation, this);

		CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;
#ifdef _WIN32
		refConnector.PostRecv(pOStream);

		refConnector.m_setIOOperations.erase(this);
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
			if (pOStream)
			{
				(*pOStream) << "IOReadOperation failed " << dwError
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}

			return dwError;
		}

		if (pOStream)
		{
			*pOStream << refConnector.NativeSocket() << ", " << bReadable << " ip:" << refConnector.GetLocalAddr().sin_addr.s_addr << ", port:" << refConnector.GetLocalAddr().sin_port
				<< " remote_ip:" << refConnector.GetRemoteAddr().sin_addr.s_addr << ", remote_port:" << refConnector.GetRemoteAddr().sin_port
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}

		return 0;
	}

	int CUdpConnector::IOWriteOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOWriteOperation, this);
		CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;

		if (pOStream)
		{
			(*pOStream) << refConnector.NativeSocket()
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
#ifdef _WIN32
		refConnector.m_setIOOperations.erase(this);
#else
		refConnector.m_bWritable = true;
#endif // _WIN32

		if (int dwError = refConnector.m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream))
		{
			//此处有报错
			if (pOStream)
			{
				(*pOStream) << "IOWriteOperation failed " << dwError
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}

			return dwError;
		}
		return 0;
	}

	int CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOErrorOperation, this);

		if (pOStream)
		{
			(*pOStream) << refSocketBase.NativeSocket() << "(" << m_dwError << ")"
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
		if (NULL == ((CUdpConnector&)refSocketBase).GetSession())
		{
			if (pOStream)
			{
				(*pOStream) << refSocketBase.NativeSocket() << " already wait delete (" << m_dwError << ")"
					<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}
		}

		macro_closesocket(refSocketBase.NativeSocket());

		//处理错误
		FxIoModule::Instance()->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewErrorEvent(m_dwError));
		FxIoModule::Instance()->PushMessageEvent(((CUdpConnector&)refSocketBase).GetSession()->NewCloseEvent());

		((CUdpConnector&)refSocketBase).SetSession(NULL);

		return 0;
	}

	CUdpConnector::UDPOnRecvOperator::UDPOnRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	int CUdpConnector::UDPOnRecvOperator::operator()(char* szBuff, unsigned short wSize, std::ostream* pOStream)
	{
		//收到的内容
		if (0 == wSize)
		{
			return 0;
		}

		m_refUdpConnector.GetSession()->GetRecvBuff().PushData(szBuff, wSize);
		if (!m_refUdpConnector.GetSession()->GetRecvBuff().CheckPackage())
		{
			return 0;
		}

		std::string szData;
		m_refUdpConnector.GetSession()->GetRecvBuff().PopData(szData);

		//if (pOStream)
		//{
		//	*pOStream << szData << "\n";
		//}
		MessageEventBase* pOperator = m_refUdpConnector.GetSession()->NewRecvMessageEvent(szData);

		if (NULL == pOperator)
		{
			if (pOStream)
			{
				(*pOStream) << "UDPOnRecvOperator failed " << CODE_ERROR_NET_PARSE_MESSAGE
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_PARSE_MESSAGE;
		}

		FxIoModule::Instance()->PushMessageEvent(pOperator);
		return 0;
	}

	CUdpConnector::UDPOnConnectedOperator::UDPOnConnectedOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	int CUdpConnector::UDPOnConnectedOperator::operator()(std::ostream* pOStream)
	{
		m_refUdpConnector.OnConnected(pOStream);
		return 0;
	}

	CUdpConnector::UDPRecvOperator::UDPRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
#ifdef _WIN32
		, m_pReadOperation(NULL)
#endif // _WIN32
	{
	}

	int CUdpConnector::UDPRecvOperator::operator()(char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream)
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
		wRecvSize = recv(m_refUdpConnector.NativeSocket(), pBuff, wBuffSize, 0);
		if (0 == wRecvSize)
		{
			return CODE_SUCCESS_NET_EOF;
		}

		if (0 > wRecvSize)
		{
			m_refUdpConnector.m_bReadable = false;
			return errno;
		}
#endif // _WIN32

		return 0;
	}

#ifdef _WIN32
	CUdpConnector::UDPRecvOperator& CUdpConnector::UDPRecvOperator::SetIOReadOperation(IOReadOperation* pReadOperation)
	{
		m_pReadOperation = pReadOperation;
		return *this;
	}
#endif // _WIN32

	CUdpConnector::UDPSendOperator::UDPSendOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	int CUdpConnector::UDPSendOperator::operator()(char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream)
	{
		if (pOStream)
		{
			(*pOStream) << m_refUdpConnector.NativeSocket()
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
#ifdef _WIN32
		dwSendLen = wBufferSize;
		return m_refUdpConnector.PostSend(szBuff, wBufferSize, pOStream);
#else
		dwSendLen = send(m_refUdpConnector.NativeSocket(), szBuff, wBufferSize, 0);
		if (0 > dwSendLen)
		{
			if (pOStream)
			{
				(*pOStream) << "UDPSendOperator failed " << errno 
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return errno;
		}
#endif // _WIN32
		return 0;
	}

	CUdpConnector::UDPReadStreamOperator::UDPReadStreamOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	int CUdpConnector::UDPReadStreamOperator::operator()(std::ostream* pOStream)
	{
		if (0 == m_refUdpConnector.GetSession()->GetSendBuff().GetSize())
		{
			return 0;
		}

		//std::string sz((char*)m_refUdpConnector.GetSession()->GetSendBuff().GetData()
		//	, m_refUdpConnector.GetSession()->GetSendBuff().GetSize());

		//if (pOStream)
		//{
		//	*pOStream << sz << ", " << m_refUdpConnector.GetSession()->GetSendBuff().GetSize() << "\n";
		//}

		unsigned short wLen = m_refUdpConnector.m_oBuffContral.Send(
			(char*)m_refUdpConnector.GetSession()->GetSendBuff().GetData()
			, m_refUdpConnector.GetSession()->GetSendBuff().GetSize());

		//unsigned short wLen = m_refUdpConnector.m_oBuffContral.Send(sz.c_str(), sz.size());

		m_refUdpConnector.GetSession()->GetSendBuff().PopData(wLen);
		return wLen;

	}

	CUdpConnector::CUdpConnector(ISession* pSession)
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

	CUdpConnector::~CUdpConnector()
	{
#ifdef _WIN32
		for (std::set<IOOperationBase*>::iterator it = m_setIOOperations.begin();
			it != m_setIOOperations.end(); ++it)
		{
			delete* it;
		}
		m_setIOOperations.clear();
#endif // _WIN32
	}

	int CUdpConnector::Init(std::ostream* pOStream, int dwState)
	{
		m_oBuffContral.SetAckOutTime(5.);
		if (int dwError = m_oBuffContral.Init(dwState))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			if (pOStream)
			{
				(*pOStream) << "init failed:" << dwError
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return dwError;
		}

		return 0;
	}

	int CUdpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		if (pOStream)
		{
			(*pOStream) << NativeSocket() << ", error: " << m_dwError
#ifndef _WIN32
				<< ", " << m_bReadable << ", " << m_bWritable
#endif // !_WIN32

				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}

		if (GetError())
		{
			return 0;
		}

#ifdef _WIN32
#else
		if (int dwError = m_oBuffContral.ReceiveMessages(dTimedouble, m_bReadable,  pOStream))
		{
			//此处有报错
			if (pOStream)
			{
				(*pOStream) << "SendMessages failed (" << dwError << "), " << NativeSocket()
					<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}

			return dwError;
		}
#endif // _WIN32

		
		if (int dwError = m_oBuffContral.SendMessages(dTimedouble, pOStream))
		{
			//此处有报错
			if (pOStream)
			{
				(*pOStream) << "SendMessages failed ("<< dwError << "), " << NativeSocket()
					<< " [" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}

			return dwError;
		}

		return 0;
	}

	int CUdpConnector::Connect(sockaddr_in address, std::ostream* pOStream)
	{
#ifdef _WIN32
		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		NativeSocketType hSock = socket(AF_INET, SOCK_DGRAM, 0);
#endif // _WIN32

		if (hSock == -1)
		{
			if (pOStream)
			{
				*pOStream << "create socket failed."
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
			}
			return -1;
		}

		if (int dwError = Init(pOStream, ST_SYN_SEND))
		{
			if (pOStream)
			{
				(*pOStream) << "client connect failed(" << dwError << ") ["
					<< __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}

			//post 到iomodule 移除
			return dwError;
		}

		if (int dwError = SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			if (pOStream)
			{
				(*pOStream) << "client connect failed(" << dwError << ") ["
					<< __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
			}
		}

		if (pOStream)
		{
			*pOStream << NativeSocket() << " ip:" << inet_ntoa(GetLocalAddr().sin_addr) << ", port:" << (int)ntohs(GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr) << ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}

#ifdef _WIN32
		for (int i = 0; i < 16; ++i)
		{
			PostRecv(pOStream);
		}
#endif // _WIN32

		return 0;
	}

	void CUdpConnector::Close(std::ostream* pOStream)
	{
		NewErrorOperation(CODE_SUCCESS_NET_EOF)(*this, 0, pOStream);
	}

	CUdpConnector::IOReadOperation& CUdpConnector::NewReadOperation()
	{
		CUdpConnector::IOReadOperation* pOperation = new CUdpConnector::IOReadOperation();
#ifdef _WIN32
		pOperation->m_stWsaBuff.buf = pOperation->m_szRecvBuff;
		pOperation->m_stWsaBuff.len = sizeof(pOperation->m_szRecvBuff);
		memset(pOperation->m_stWsaBuff.buf, 0, pOperation->m_stWsaBuff.len);
		m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	CUdpConnector::IOWriteOperation& CUdpConnector::NewWriteOperation()
	{
		CUdpConnector::IOWriteOperation* pOperation = new CUdpConnector::IOWriteOperation();
#ifdef _WIN32
		m_setIOOperations.insert(pOperation);
#endif // _WIN32

		return *pOperation;
	}

	CUdpConnector::IOErrorOperation& CUdpConnector::NewErrorOperation(int dwError)
	{
		CUdpConnector::IOErrorOperation* pOperation = new CUdpConnector::IOErrorOperation();
		pOperation->m_dwError = dwError;
		m_dwError = dwError;
		return *pOperation;
	}

	CUdpConnector& CUdpConnector::SendMessage(std::ostream* pOStream)
	{
		m_oBuffContral.SendMessages(FxIoModule::Instance()->FxGetCurrentTime(), pOStream);
		return *this;
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
				if (pOStream)
				{
					*pOStream << "PostRecv failed." << NativeSocket() << ", " << dwError
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				}
			}
		}

		return *this;
	}

	int CUdpConnector::PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream)
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
				if (pOStream)
				{
					*pOStream << "PostSend failed." << NativeSocket() << ", " << dwError
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				}
				delete &refIOWriteOperation;
			}

			return dwError;
		}

		return 0;
	}

#endif // _WIN32

	void CUdpConnector::OnRead(std::ostream* refOStream)
	{
	}

	void CUdpConnector::OnWrite(std::ostream* pOStream)
	{
	}

	void CUdpConnector::OnError(int dwError, std::ostream* pOStream)
	{
		//TODO
		if (pOStream)
		{
			*pOStream << NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
		}
	}

	void CUdpConnector::OnClose(std::ostream* pOStream)
	{
		if (pOStream)
		{
			*pOStream << NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
		}
		delete this;
	}

	void CUdpConnector::OnConnected(std::ostream* pOStream)
	{
		if (pOStream)
		{
			*pOStream << NativeSocket() << " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
				<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
				<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}

		FxIoModule::Instance()->PushMessageEvent(GetSession()->NewConnectedEvent());
	}

	int CUdpConnector::Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		NativeSocket() = hSock;

		socklen_t dwAddrLen = sizeof(GetLocalAddr());
		getsockname(hSock, (sockaddr*)&GetLocalAddr(), &dwAddrLen);

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

			if (pOStream)
			{
				(*pOStream) << "socket set nonblock failed(" << dwError << ") ["
					<< __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			int dwError = errno;
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			if (pOStream)
			{
				(*pOStream) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
					<< __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
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

			if (pOStream)
			{
				(*pOStream) << "setsockopt failed(" << dwError << ")"
					<< " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
					<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
					<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
					<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
					<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
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
#ifdef _WIN32
			int dwError = WSAGetLastError();
#else // _WIN32
			int dwError = errno;
#endif // _WIN32
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			if (pOStream)
			{
				(*pOStream) << "connect failed(" << dwError << ")"
					<< " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
					<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
					<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
					<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

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

			if (pOStream)
			{
				(*pOStream) << "register io failed"
					<< " ip:" << inet_ntoa(GetLocalAddr().sin_addr)
					<< ", port:" << (int)ntohs(GetLocalAddr().sin_port)
					<< " remote_ip:" << inet_ntoa(GetRemoteAddr().sin_addr)
					<< ", remote_port:" << (int)ntohs(GetRemoteAddr().sin_port)
					<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}

			return dwError;
		}

		if (pOStream)
		{
			(*pOStream) << NativeSocket() << ", " << m_oBuffContral.GetState()
				<< " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
		return 0;
	}



};
