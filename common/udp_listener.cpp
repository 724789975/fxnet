#include "udp_listener.h"
#include "iothread.h"

#ifdef _WIN32
#include <WinSock2.h>
#define macro_closesocket closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#define macro_closesocket close
#endif // _WIN32

namespace FXNET
{
	int CUdpListener::Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		int dwError = 0;
		memset(&m_oAddr, 0, sizeof(m_oAddr));
		m_oAddr.sin_family = AF_INET;

		if (szIp) { m_oAddr.sin_addr.s_addr = inet_addr(szIp); }

		m_oAddr.sin_port = htons(wPort);

		// 创建socket
#ifdef _WIN32
		NativeSocket() = WSASocket( AF_INET
			,SOCK_DGRAM ,0 ,NULL ,0 ,WSA_FLAG_OVERLAPPED);
#else
		NativeSocket() = socket(AF_INET, SOCK_DGRAM, 0);
#endif // _WIN32


		if (NativeSocket() == -1)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "socket create failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(NativeSocket(), F_SETFL, fcntl(NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif // _WIN32
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
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
			dwError = errno;

			if (*pOStream)
			{
				(*pOStream) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}
#endif // _WIN32

		// 地址重用
		int nReuse = 1;
		if (setsockopt(NativeSocket(), SOL_SOCKET, SO_REUSEADDR, (char*) & nReuse, sizeof(nReuse)))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			if (pOStream)
			{
				(*pOStream) << "socket set SO_REUSEADDR failed(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

		// bind
		if (bind(NativeSocket(), (sockaddr*)&m_oAddr, sizeof(m_oAddr)))
		{
			macro_closesocket(NativeSocket());
			NativeSocket() = (NativeSocketType)InvalidNativeHandle();
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			if (pOStream)
			{
				(*pOStream) << "bind failed on (" << inet_ntoa(m_oAddr.sin_addr)
					<< ", " << (int)ntohs(m_oAddr.sin_port) << ")(" << dwError << ") ["
					<< __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
			}
			return dwError;
		}

#ifdef _WIN32
		FxIoModule::Instance()->RegisterIO(NativeSocket(), this, pOStream);
#else
		m_oAcceptPool.Init();
		memset(m_arroAcceptQueue, 0, sizeof(m_arroAcceptQueue));

		FxIoModule::Instance()->RegisterIO(NativeSocket(), EPOLLIN, this, pOStream);
#endif // _WIN32

#ifdef _WIN32
		dwError = PostAccept(pOStream);
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

	void CUdpListener::OnRead(std::ostream* pOStream)
	{
#ifdef _WIN32
#else
		for (;;)
		{
			UDPPacketHeader header;
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
					if (pOStream)
					{
						*pOStream << NativeSocket() << " can't allocate more udp accept request.\n";
					}
					break;
				}

				req->m_pNext = NULL;
				req->addr = address;

				req->m_oAcceptSocket = socket(AF_INET, SOCK_DGRAM, 0);
				if (req->m_oAcceptSocket == -1)
				{
					if (pOStream)
					{
						*pOStream << NativeSocket() << " create socket failed.\n";
					}
					m_oAcceptPool.Free(req);
					continue;
				}

				// cloexec
				fcntl(req->m_oAcceptSocket, F_SETFD, FD_CLOEXEC);

				// set reuseaddr
				int yes = 1;
				if (setsockopt(req->m_oAcceptSocket, SOL_SOCKET, SO_REUSEADDR, (char*)& yes, sizeof(yes)))
				{
					if (pOStream)
					{
						*pOStream << NativeSocket() << ", errno(" << errno << ")"
							<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";;
					}
					macro_closesocket(req->m_oAcceptSocket);
					m_oAcceptPool.Free(req);
					continue;
				}

				// bind
				if (bind(req->m_oAcceptSocket, (sockaddr*)&m_oAddr, sizeof(m_oAddr)))
				{
					if (pOStream)
					{
						*pOStream << NativeSocket() << ", errno(" << errno << ") " << "bind failed on "
							<< inet_ntoa(m_oAddr.sin_addr) << ":" << (int)ntohs(m_oAddr.sin_port)
							<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
					}
					macro_closesocket(req->m_oAcceptSocket);
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
					if (pOStream)
					{
						*pOStream << NativeSocket() << "error accept(" << errno << ")."
							<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
					}
				}

				break;
			}
		}

		// temp remove accept req at here
		AcceptReq *pReq;
		for (int i = 0; i < UDP_ACCEPT_HASH_SIZE; i++)
		{
			while ((pReq = m_arroAcceptQueue[i]) != NULL)
			{
				m_arroAcceptQueue[i] = pReq->m_pNext;

				m_oAcceptPool.Free(pReq);
			}
		}
	
#endif // _WIN32

	}

#ifdef _WIN32
	CUdpListener& CUdpListener::OnClientConnected(NativeSocketType hSock, sockaddr_in address)
	{
		//Client* client = server.client_pool.Allocate();
		//if (client == NULL)
		//{
		//	close(connected_socket);
		//	return;
		//}

		//client->client_address = addr;
		//client->tcp_connection.stream = NULL;
		//client->udp_connection.stream = client;
		//client->connection = &client->udp_connection;
		//client->udp_connection.Connect(connected_socket, addr);
		return *this;
	}

	int CUdpListener::PostAccept(std::ostream* pOStream)
	{
		IOReadOperation& refPeration = NewReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		int dwSockAddr = sizeof(refPeration.m_stRemoteAddr);

		if (SOCKET_ERROR == WSARecvFrom(NativeSocket(), &refPeration.m_stWsaBuff, 1, &dwReadLen
			, &dwFlags, (sockaddr*)(&refPeration.m_stRemoteAddr), &dwSockAddr, &refPeration, NULL))
		{
			int dwError = WSAGetLastError();
			if (dwError != WSA_IO_PENDING)
			{
				if (pOStream)
				{
					(*pOStream) << "WSARecvFrom errno : " << dwError << ", handle : " << NativeSocket()
						<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				}
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

	void CUdpListener::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		if (0 == dwLen)
		{
			//TODO
		}
		//refSocketBase.OnRead(pOStream);

		CUdpListener& refSock = (CUdpListener&) refSocketBase;

#ifndef _WIN32
		for (;;)
		{
			UDPPacketHeader oUDPPacketHeader;

			sockaddr_in stRemoteAddr = { 0 };
			unsigned int nRemoteAddrLen = sizeof(stRemoteAddr);

			int dwLen = recvfrom(refSocketBase.NativeSocket(), (char*)(&oUDPPacketHeader), sizeof(oUDPPacketHeader), 0, (sockaddr*)&stRemoteAddr, &nRemoteAddrLen);
			if (0 > dwLen)
			{
				if (errno == EINTR) { continue; }

				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					*pOStream << "error accept : " << refSocketBase.NativeSocket()
						<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				}

				break;
			}

			if (dwLen != sizeof(oUDPPacketHeader))
			{
				*pOStream << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				continue;
			}

			if (oUDPPacketHeader.m_btAck != 0)
			{
				*pOStream << "ack error want : 0, recv : " << oUDPPacketHeader.m_btAck <<  ", " << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				continue;
			}
			if (oUDPPacketHeader.m_btSyn != 1)
			{
				*pOStream << "syn error want : 0, recv : " << oUDPPacketHeader.m_btSyn <<  ", " << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ", " << __FILE__ << ", " << __FUNCTION__ << "]\n";
				continue;
			}

			AcceptReq* req = refSock.GetAcceptReq(stRemoteAddr);

			if (req != NULL)
				continue;

			req = refSock.m_oAcceptPool.Allocate();

			if (req == NULL)
			{
				if (pOStream)
				{
					*pOStream << refSock.NativeSocket() << " can't allocate more udp accept request.\n";
				}
				break;
			}

			req->m_pNext = NULL;
			req->addr = stRemoteAddr;

			req->m_oAcceptSocket = socket(AF_INET, SOCK_DGRAM, 0);
			if (req->m_oAcceptSocket == -1)
			{
				if (pOStream)
				{
					*pOStream << refSock.NativeSocket() << " create socket failed.\n";
				}
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			// cloexec
			fcntl(req->m_oAcceptSocket, F_SETFD, FD_CLOEXEC);

			// set reuseaddr
			int yes = 1;
			if (setsockopt(req->m_oAcceptSocket, SOL_SOCKET, SO_REUSEADDR, (char*)& yes, sizeof(yes)))
			{
				if (pOStream)
				{
					*pOStream << refSock.NativeSocket() << ", errno(" << errno << ")"
						<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";;
				}
				macro_closesocket(req->m_oAcceptSocket);
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			// bind
			if (bind(req->m_oAcceptSocket, (sockaddr*)&refSock.m_oAddr, sizeof(m_oAddr)))
			{
				if (pOStream)
				{
					*pOStream << refSock.NativeSocket() << ", errno(" << errno << ") " << "bind failed on "
						<< inet_ntoa(refSock.m_oAddr.sin_addr) << ":" << (int)ntohs(refSock.m_oAddr.sin_port)
						<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ << "]\n";
				}
				macro_closesocket(req->m_oAcceptSocket);
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			// add accept req
			refSock.AddAcceptReq(req);

			// send back
			//TODO
			//OnClientConnected(req->accept_socket, address);
		}
	
#endif
		delete this;
		return;
	}

	void CUdpListener::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		delete this;
		return;
	}

};


