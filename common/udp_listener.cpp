#include "udp_listener.h"

#ifdef _WIN32
#include <WinSock2.h>
#else
#endif // _WIN32

namespace FXNET
{
	int CUdpListener::Listen(const char* szIp, unsigned short wPort, std::ostream& refOStream)
	{
		int dwError = 0;
		memset(&m_oAddr, 0, sizeof(m_oAddr));
		m_oAddr.sin_family = AF_INET;

		if (szIp) { m_oAddr.sin_addr.s_addr = inet_addr(szIp); }

		m_oAddr.sin_port = htons(wPort);

		// 创建socket
		NativeSocket() = socket(AF_INET, SOCK_DGRAM, 0);
		if (NativeSocket() == -1)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			refOStream << "socket create failed(" << dwError << ") ["
				<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			return dwError;
		}

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(NativeSocket(), F_SETFL, fcntl(NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif // _WIN32
		{
			closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			refOStream << "socket set nonblock failed(" << dwError << ") ["
				<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			dwError = errno;

			refOStream << "socket set FD_CLOEXEC failed(" << dwError << ") ["
				<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			return dwError;
		}
#endif // _WIN32

		// 地址重用
		int nReuse = 1;
		if (setsockopt(NativeSocket(), SOL_SOCKET, SO_REUSEADDR, (char*) & nReuse, sizeof(nReuse)))
		{
			closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			refOStream << "socket set SO_REUSEADDR failed(" << dwError << ") ["
				<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			return dwError;
		}

		// bind
		if (bind(NativeSocket(), (sockaddr*)&m_oAddr, sizeof(m_oAddr)))
		{
			closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			refOStream << "bind failed on (" << inet_ntoa(m_oAddr.sin_addr)
				<< ", " << (int)ntohs(m_oAddr.sin_port) << ")(" << dwError << ") ["
				<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			return dwError;
		}

#ifndef _WIN32
		m_oAcceptPool.Init();

		memset(m_arroAcceptQueue, 0, sizeof(m_arroAcceptQueue));
#endif // _WIN32

		//todo注册事件


#ifdef _WIN32
		dwError = PostAccept(refOStream);
#endif // _WIN32

		return dwError;
	}

	CUdpListener::IOReadOperation& CUdpListener::NewReadOperation()
	{
		IOReadOperation& refPeration = *(new IOReadOperation());
#ifdef _WIN32

		memset(&refPeration, 0, sizeof(OVERLAPPED));

		refPeration.m_stWsaBuff.buf = refPeration.m_szRecvBuff;
		refPeration.m_stWsaBuff.len = sizeof(refPeration.m_szRecvBuff);
		memset(refPeration.m_stWsaBuff.buf, 0, refPeration.m_stWsaBuff.len);
#endif // _WIN32
		return refPeration;
	}

	void CUdpListener::OnRead(std::ostream& refOStream)
	{

#ifdef _WIN32
#else
		for (;;)
		{
			PacketHeader header;
			sockaddr_in address;
			socklen_t addr_len = sizeof(sockaddr);

			// receive message
			int size = recvfrom(NativeSocket(), (char*)& header, sizeof(header), 0, (sockaddr*)&address, &addr_len);

			if (size == sizeof(header))
			{
				AcceptReq* req = GetAcceptReq(address);

				if (req != NULL)
					continue;

				if (header.m_btSyn != 1 || header.m_btAck != 0)
					continue;

				req = m_oAcceptPool.Allocate();

				if (req == NULL)
				{
					refOStream << NativeSocket() << " can't allocate more udp accept request.\n";
					break;
				}

				req->m_pNext = NULL;
				req->addr = address;

				req->m_oAcceptSocket = socket(AF_INET, SOCK_DGRAM, 0);
				if (req->m_oAcceptSocket == -1)
				{
					refOStream << NativeSocket() << " create socket failed.\n";
					m_oAcceptPool.Free(req);
					continue;
				}

				// cloexec
				fcntl(req->m_oAcceptSocket, F_SETFD, FD_CLOEXEC);

				// set reuseaddr
				int yes = 1;
				if (setsockopt(req->m_oAcceptSocket, SOL_SOCKET, SO_REUSEADDR, (char*)& yes, sizeof(yes)))
				{
					refOStream << NativeSocket() << ", errno(" << errno << ")\n";
					close(req->m_oAcceptSocket);
					m_oAcceptPool.Free(req);
					continue;
				}

				// bind
				if (bind(req->m_oAcceptSocket, (sockaddr*)&m_oAddr, sizeof(m_oAddr)))
				{
					refOStream << NativeSocket() << ", errno(" << errno << ") " << "bind failed on "
						<< inet_ntoa(m_oAddr.sin_addr) << ":" << (int)ntohs(addr.sin_port);
					close(req->m_oAcceptSocket);
					m_oAcceptPool.Free(req);
					continue;
				}

				// add accept req
				AddAcceptReq(req);

				// send back
				//TODO
				//OnClientConnected(req->accept_socket, address);
			}
			else if (size < 0)
			{
				if (errno == EINTR) { continue; }

				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					refOStream << NativeSocket() << "error accept(" << errno << ").";
				}

				break;
			}
		}
	
#endif // _WIN32

	}

#ifdef _WIN32
	int CUdpListener::PostAccept(std::ostream& refOStream)
	{
		IOReadOperation& refPeration = NewReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		int dwSockAddr = sizeof(refPeration.m_stRemoteAddr);

		if (WSARecvFrom(NativeSocket(), &refPeration.m_stWsaBuff, 1, &dwReadLen, &dwFlags,
			(sockaddr*)(&refPeration.m_stRemoteAddr), &dwSockAddr, &refPeration, NULL) == SOCKET_ERROR)
		{
			int dwError = WSAGetLastError();
			if (dwError != WSA_IO_PENDING)
			{
				refOStream << "WSARecvFrom errno : " << dwError << ", handle : " << NativeSocket()
					<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				return dwError;
			}
		}
		return 0;
	}
#else
	unsigned int CUdpListener::GenerateAcceptHash(const sockaddr_in& addr)
	{
		unsigned int h = addr.sin_addr.s_addr ^ addr.sin_port;
		h ^= h >> 16;
		h ^= h >> 8;
		return h & (UDP_ACCEPT_HASH_SIZE - 1);
	}

	CUdpListener::AcceptReq* CUdpListener::GetAcceptReq(const sockaddr_in& addr)
	{
		for (AcceptReq* pReq = m_arroAcceptQueue[GenerateAcceptHash(addr)]; pReq != NULL; pReq = pReq->m_pNext)
		{
			if (pReq->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
				pReq->addr.sin_port == addr.sin_port)
				return pReq;
		}

		return NULL;
	}
	void CUdpListener::AddAcceptReq(AcceptReq* pReq)
	{
		if (pReq && pReq->m_pNext == NULL)
		{
			unsigned int h = GenerateAcceptHash(pReq->addr);
			pReq->m_pNext = m_arroAcceptQueue[h];
			m_arroAcceptQueue[h] = pReq;
		}
	}

	void CUdpListener::RemoveAcceptReq(const sockaddr_in& addr)
	{
		AcceptReq* pReq = m_arroAcceptQueue[GenerateAcceptHash(addr)];
		AcceptReq* pPrev = NULL;

		while (pReq != NULL)
		{
			if (pReq->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
				pReq->addr.sin_port == addr.sin_port)
			{
				if (pPrev) pPrev->m_pNext = pReq->m_pNext;
				m_oAcceptPool.Free(pReq);
				break;
			}

			pPrev = pReq;
			pReq = pReq->m_pNext;
		}
	}
#endif // _WIN32

	IOOperationBase& CUdpListener::IOReadOperation::operator()(CSocketBase& refSocketBase, std::ostream& refOStream)
	{
		refSocketBase.OnRead(refOStream);
		return *this;
	}

};


