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

		m_oAcceptPool.Init();

		memset(m_arroAcceptQueue, 0, sizeof(m_arroAcceptQueue));

		return dwError;
	}

	void CUdpListener::OnRead()
	{
		//TODO
	}

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
};


