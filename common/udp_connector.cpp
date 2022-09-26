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

#include <assert.h>

#include <iostream>

namespace FXNET
{
	int CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOReadOperation, this);

		CUdpConnector& refConnector = (CUdpConnector&)refSocketBase;
#ifdef _WIN32
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
					<< " [" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}

			return dwError;
		}

		return 0;
	}

	int CUdpConnector::IOWriteOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOWriteOperation, this);
		//TODO
		return 0;
	}

	int CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpConnector::IOErrorOperation, this);
		return 0;
	}

	CUdpConnector::UDPOnRecvOperator::UDPOnRecvOperator(CUdpConnector& refUdpConnector)
		: m_refUdpConnector(refUdpConnector)
	{
	}

	int CUdpConnector::UDPOnRecvOperator::operator()(char* szBuff, unsigned short wSize, std::ostream* refOStream)
	{
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

	int CUdpConnector::UDPRecvOperator::operator()(char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* refOStream)
	{
#ifdef _WIN32
		//没有可读事件
		if (!m_pReadOperation)
		{
			return CODE_SUCCESS_NO_BUFF_READ;
		}

		wBuffSize = (unsigned short)m_pReadOperation->m_dwLen;
		if (0 == m_pReadOperation)
		{
			return CODE_SUCCESS_NET_EOF;
		}

		assert(wBuffSize <= UDP_WINDOW_BUFF_SIZE);
		assert(m_pReadOperation->m_dwLen <= UDP_WINDOW_BUFF_SIZE);
		memcpy(pBuff, m_pReadOperation->m_stWsaBuff.buf, m_pReadOperation->m_dwLen);
		SetIOReadOperation(NULL);
#else
		wRecvSize = recv(m_refUdpConnector.NativeSocket(), pBuff, wBuffSize, 0);
		if (0 == wRecvSize)
		{
			return CODE_SUCCESS_NET_EOF;
		}

		if (0 > wRecvSize)
		{
			m_bReadable = false;
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
#ifdef _WIN32
		return m_refUdpConnector.PostSend(szBuff, wBufferSize, pOStream);
#else
		dwSendLen = send(m_refUdpConnector.NativeSocket(), szBuff, wBufferSize, 0);
		if (0 < dwSendLen)
		{
			if (pOStream)
			{
				(*pOStream) << "UDPSendOperator failed " << errno 
					<< " [" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return errno;
		}
#endif // _WIN32
		return 0;
	}

	CUdpConnector::CUdpConnector()
		: m_stRemoteAddr({0})
		, m_funOnRecvOperator(*this)
		, m_funOnConnectedOperator(*this)
		, m_funRecvOperator(*this)
		, m_funSendOperator(*this)
	{
		m_oBuffContral.SetOnRecvOperator(&m_funOnRecvOperator)
			.SetOnConnectedOperator(&m_funOnConnectedOperator)
			.SetRecvOperator(&m_funRecvOperator)
			.SetSendOperator(&m_funSendOperator);
	}

	CUdpConnector::~CUdpConnector()
	{
		for (std::set<IOOperationBase*>::iterator it = m_setIOOperations.begin();
			it != m_setIOOperations.end(); ++it)
		{
			delete* it;
		}
		m_setIOOperations.clear();
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

		m_oBuffContral.SendMessages(dTimedouble, pOStream);
	
#ifdef _WIN32
		bool bReadable = true;
#else
		bool& bReadable = refConnector.m_bReadable;
#endif // _WIN32
		m_oBuffContral.ReceiveMessages(dTimedouble, bReadable, pOStream);

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
#ifdef _WIN32
		m_setIOOperations.insert(&t);
#endif // _WIN32

		return t;
	}

	CUdpConnector::IOWriteOperation& CUdpConnector::NewWriteOperation()
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOWriteOperation t;
#ifdef _WIN32
		m_setIOOperations.insert(&t);
#endif // _WIN32
		return t;
	}

	CUdpConnector::IOErrorOperation& CUdpConnector::NewErrorOperation(int dwError)
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOErrorOperation t;
		return t;
	}

#ifdef _WIN32
	CUdpConnector& CUdpConnector::PostRecv(std::ostream* pOStream)
	{
		IOReadOperation& refIOReadOperation = NewReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		if (SOCKET_ERROR == WSARecv(NativeSocket(), &refIOReadOperation.m_stWsaBuff
			, 1, &dwReadLen, &dwFlags, &refIOReadOperation, NULL))
		{
			if (pOStream)
			{
				*pOStream << "PostRecv failed." << NativeSocket() << ", " << WSAGetLastError()
					<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";;
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
			, 1, &dwWriteLen, dwFlags, &refIOWriteOperation, NULL))
		{
			if (WSA_IO_PENDING != WSAGetLastError())
			{
				if (pOStream)
				{
					*pOStream << "PostRecv failed." << NativeSocket() << ", " << WSAGetLastError()
						<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";;
				}
				delete &refIOWriteOperation;
			}

			return WSAGetLastError();
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

	void CUdpConnector::OnError(std::ostream* pOStream)
	{
	}

	void CUdpConnector::OnConnected(std::ostream* pOStream)
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

};
