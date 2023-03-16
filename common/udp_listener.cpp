#include "udp_listener.h"
#include "../include/iothread.h"
#include "udp_connector.h"
#include "../include/log_utility.h"
#include <cstdlib>

#ifdef _WIN32
#include <WinSock2.h>
#include <Iphlpapi.h>
#include <iostream>
#pragma comment(lib,"Iphlpapi.lib") //��Ҫ���Iphlpapi.lib��
#ifndef macro_closesocket
#define macro_closesocket closesocket
#endif //macro_closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ifaddrs.h>
#ifndef macro_closesocket
#define macro_closesocket close
#endif //macro_closesocket
#endif // _WIN32

namespace FXNET
{
	const char* GetIp(const char* szIp)
	{
		static std::string szRetIp = szIp;
		if (0 == strcmp(szIp, "0.0.0.0"))
		{
#ifdef _WIN32
			//��¼��������
			int netCardNum = 0;
			//��¼ÿ�������ϵ�IP��ַ����
			int IPnumPerNetCard = 0;
			//PIP_ADAPTER_INFO�ṹ��ָ��洢����������Ϣ
			PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
			//�õ��ṹ���С,����GetAdaptersInfo����
			unsigned long stSize = sizeof(IP_ADAPTER_INFO);
			//����GetAdaptersInfo����,���pIpAdapterInfoָ�����;����stSize��������һ��������Ҳ��һ�������
			int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
			if (ERROR_BUFFER_OVERFLOW == nRel) {
				//����������ص���ERROR_BUFFER_OVERFLOW
				//��˵��GetAdaptersInfo�������ݵ��ڴ�ռ䲻��,ͬʱ�䴫��stSize,��ʾ��Ҫ�Ŀռ��С
				//��Ҳ��˵��ΪʲôstSize����һ��������Ҳ��һ�������
				//�ͷ�ԭ�����ڴ�ռ�
				delete pIpAdapterInfo;
				//���������ڴ�ռ������洢����������Ϣ
				pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
				//�ٴε���GetAdaptersInfo����,���pIpAdapterInfoָ�����
				nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
			}
			PIP_ADAPTER_INFO p = pIpAdapterInfo;
			if (ERROR_SUCCESS == nRel) {
				//���������Ϣ
				//�����ж�����,���ͨ��ѭ��ȥ�ж�
				while (pIpAdapterInfo)
				{
					//std::cout << "����������" << ++netCardNum << std::endl;
					//std::cout << "�������ƣ�" << pIpAdapterInfo->AdapterName << std::endl;
					//std::cout << "����������" << pIpAdapterInfo->Description << std::endl;
					switch (pIpAdapterInfo->Type) {
					case MIB_IF_TYPE_OTHER:
						//std::cout << "�������ͣ�" << "OTHER" << std::endl;
						break;
					case MIB_IF_TYPE_ETHERNET:
						//std::cout << "�������ͣ�" << "ETHERNET" << std::endl;
						break;
					case MIB_IF_TYPE_TOKENRING:
						//std::cout << "�������ͣ�" << "TOKENRING" << std::endl;
						break;
					case MIB_IF_TYPE_FDDI:
						//std::cout << "�������ͣ�" << "FDDI" << std::endl;
						break;
					case MIB_IF_TYPE_PPP:
						//printf("PP\n");
						//std::cout << "�������ͣ�" << "PPP" << std::endl;
						break;
					case MIB_IF_TYPE_LOOPBACK:
						//std::cout << "�������ͣ�" << "LOOPBACK" << std::endl;
						break;
					case MIB_IF_TYPE_SLIP:
						//std::cout << "�������ͣ�" << "SLIP" << std::endl;
						break;
					default:
						break;
					}
					//std::cout << "����MAC��ַ��";
					for (DWORD i = 0; i < pIpAdapterInfo->AddressLength; i++)
						if (i < pIpAdapterInfo->AddressLength - 1) {
							//printf("%02X-", pIpAdapterInfo->Address[i]);
						}
						else {
							//printf("%02X\n", pIpAdapterInfo->Address[i]);
						}
					//std::cout << "����IP��ַ���£�" << std::endl;
					//���������ж�IP,���ͨ��ѭ��ȥ�ж�
					IP_ADDR_STRING* pIpAddrString = &(pIpAdapterInfo->IpAddressList);
					do {
						if (strcmp(pIpAddrString->IpAddress.String, "127.0.0.1")
							&& strcmp(pIpAddrString->IpAddress.String, ""))
						{
							szRetIp = pIpAddrString->IpAddress.String;
							break;
						}
						//std::cout << "�������ϵ�IP������" << ++IPnumPerNetCard << std::endl;
						//std::cout << "IP ��ַ��" << pIpAddrString->IpAddress.String << std::endl;
						//std::cout << "������ַ��" << pIpAddrString->IpMask.String << std::endl;
						//std::cout << "���ص�ַ��" << pIpAdapterInfo->GatewayList.IpAddress.String << std::endl;
						pIpAddrString = pIpAddrString->Next;
					} while (pIpAddrString);

					pIpAdapterInfo = pIpAdapterInfo->Next;
					//std::cout << "--------------------------------------------------------------------" << std::endl;
				}
			}
			//�ͷ��ڴ�ռ�
			if (p) {
				delete p;
			}

			return szRetIp.c_str();
#else
			struct ifaddrs *ifc, *ifc1;
			char ip[64] = {};
		
			if(0 == getifaddrs(&ifc))
			{
				ifc1 = ifc;
				for (; NULL != ifc; ifc = (*ifc).ifa_next)
				{
					if ((*ifc).ifa_addr)
					{
						inet_ntop(AF_INET, &(((struct sockaddr_in *)((*ifc).ifa_addr))->sin_addr), ip, 64);
						if (strcmp(ip, "127.0.0.1") && strcmp(ip, ""))
						{
							szRetIp = ip;
							break;
						}
					}
					else
					{
					}
					if ((*ifc).ifa_netmask)
					{
					}
					else
					{
					}
				}

				freeifaddrs(ifc1);
			}

#endif //!_WIN32
		}
		return szIp;
	}

	ErrorCode CUdpListener::IOReadOperation::operator()(ISocketBase &refSocketBase, unsigned int dwLen, std::ostream *pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpListener::IOReadOperation, this);

		CUdpListener &refSock = (CUdpListener &)refSocketBase;

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << refSock.NativeSocket() << ", " << refSock.Name()
										 << " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		UDPPacketHeader& oUDPPacketHeader = *(UDPPacketHeader*)m_stWsaBuff.buf;

		sockaddr_in& stRemoteAddr = this->m_stRemoteAddr;

		if (dwLen != sizeof(oUDPPacketHeader))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode();
		}

		if (oUDPPacketHeader.m_btStatus != ST_SYN_SEND)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode();
		}

		if (oUDPPacketHeader.m_btAck != 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "ack error want : 0, recv : " << (int)oUDPPacketHeader.m_btAck << ", " << refSocketBase.NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode();
		}
		if (oUDPPacketHeader.m_btSyn != 1)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "syn error want : 1, recv : " << (int)oUDPPacketHeader.m_btSyn << ", " << refSocketBase.NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode();
		}

		LOG(pOStream, ELOG_LEVEL_INFO) << refSock.NativeSocket() << " recvfrom "
			<< inet_ntoa(stRemoteAddr.sin_addr) << ":" << (int)ntohs(stRemoteAddr.sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

		NativeSocketType hSock = WSASocket(AF_INET
			, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
		if (hSock == -1)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << " create socket failed."
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
			return ErrorCode();
		}

		// set reuseaddr
		int yes = 1;
		if (setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << ", errno(" << WSAGetLastError() << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
			macro_closesocket(hSock);
			return ErrorCode();
		}

		// bind
		if (bind(hSock, (sockaddr*)&refSock.GetLocalAddr(), sizeof(refSock.GetLocalAddr())))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << ", errno(" << WSAGetLastError() << ") " << "bind failed on "
				<< inet_ntoa(refSock.GetLocalAddr().sin_addr) << ":" << (int)ntohs(refSock.GetLocalAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			macro_closesocket(hSock);
			return ErrorCode();
		}

		// send back
		refSock.OnClientConnected(hSock, m_stRemoteAddr, pOStream);

		refSock.PostAccept(pOStream);
#else
		for (;;)
		{
			UDPPacketHeader oUDPPacketHeader;

			sockaddr_in stRemoteAddr = { 0 };
			memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
			unsigned int nRemoteAddrLen = sizeof(stRemoteAddr);

			int dwLen = recvfrom(refSocketBase.NativeSocket(), (char*)(&oUDPPacketHeader), sizeof(oUDPPacketHeader), 0, (sockaddr*)&stRemoteAddr, &nRemoteAddrLen);
			if (0 > dwLen)
			{
				if (errno == EINTR) { continue; }

				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " error accept : " << errno
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				}

				break;
			}

			if (dwLen != sizeof(oUDPPacketHeader))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				continue;
			}

			if (oUDPPacketHeader.m_btStatus != ST_SYN_SEND)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				continue;
			}

			if (oUDPPacketHeader.m_btAck != 0)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "ack error want : 0, recv : " << oUDPPacketHeader.m_btAck <<  ", " << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				continue;
			}
			if (oUDPPacketHeader.m_btSyn != 1)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "syn error want : 0, recv : " << oUDPPacketHeader.m_btSyn <<  ", " << refSocketBase.NativeSocket()
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				continue;
			}

			AcceptReq* req = refSock.GetAcceptReq(stRemoteAddr);

			if (req != NULL)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) <<  refSocketBase.NativeSocket() << ", req not null"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				continue;
			}

			req = refSock.m_oAcceptPool.Allocate();

			if (req == NULL)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << " can't allocate more udp accept request.\n";
				break;
			}

			req->m_pNext = NULL;
			req->addr = stRemoteAddr;

			sockaddr_in addr;
			socklen_t sock_len = sizeof(addr);
			getsockname(refSock.NativeSocket(), (sockaddr*)&addr, &sock_len);

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refSock.NativeSocket() << " ip:" << inet_ntoa(addr.sin_addr) << ", port:" << (int)ntohs(addr.sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refSock.NativeSocket() << " ip:" << inet_ntoa(refSock.GetLocalAddr().sin_addr)
				<< ", port:" << (int)ntohs(refSock.GetLocalAddr().sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			LOG(pOStream, ELOG_LEVEL_INFO) << refSock.NativeSocket() << " recvfrom "
				<< inet_ntoa(stRemoteAddr.sin_addr) << ":" << (int)ntohs(stRemoteAddr.sin_port)
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			req->m_oAcceptSocket = socket(AF_INET, SOCK_DGRAM, 0);
			//req->m_oAcceptSocket = refSock.NativeSocket();
			if (req->m_oAcceptSocket == -1)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << " create socket failed."
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			// cloexec
			fcntl(req->m_oAcceptSocket, F_SETFD, FD_CLOEXEC);

			// set reuseaddr
			int yes = 1;
			if (setsockopt(req->m_oAcceptSocket, SOL_SOCKET, SO_REUSEADDR, (char*)& yes, sizeof(yes)))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << req->m_oAcceptSocket << ", errno(" << errno << ")"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";;
				macro_closesocket(req->m_oAcceptSocket);
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			// bind
			if (bind(req->m_oAcceptSocket, (sockaddr*)&refSock.GetLocalAddr(), sizeof(refSock.GetLocalAddr())))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << req->m_oAcceptSocket << ", errno(" << errno << ") " << "bind failed on "
					<< inet_ntoa(refSock.GetLocalAddr().sin_addr) << ":" << (int)ntohs(refSock.GetLocalAddr().sin_port)
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				macro_closesocket(req->m_oAcceptSocket);
				refSock.m_oAcceptPool.Free(req);
				continue;
			}

			refSock.AddAcceptReq(req);

			//FxIoModule::Instance()->DeregisterIO(refSock.NativeSocket(), pOStream);
			//macro_closesocket(refSock.NativeSocket());

			// send back
			refSock.OnClientConnected(req->m_oAcceptSocket, req->addr, pOStream);

			//refSock.Listen(pOStream);
			//TODO �Ƿ���Ҫbreak
			break;
		}

		// �Ƴ������req
		AcceptReq* pReq;
		for (unsigned int i = 0; i < UDP_ACCEPT_HASH_SIZE; i++)
		{
			while ((pReq = refSock.m_arroAcceptQueue[i]) != NULL)
			{
				refSock.m_arroAcceptQueue[i] = pReq->m_pNext;

				refSock.m_oAcceptPool.Free(pReq);
			}
		}
	
#endif
		return ErrorCode();
	}

	ErrorCode CUdpListener::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{
		DELETE_WHEN_DESTRUCT(CUdpListener::IOErrorOperation, this);
		LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOErrorOperation failed(" << m_oError.What() << ")"
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
		return ErrorCode();
	}

	CUdpListener::CUdpListener(SessionMaker* pMaker)
		: m_pSessionMaker(pMaker)
	{

	}

	ErrorCode CUdpListener::Update(double dTimedouble, std::ostream* pOStream)
	{
		//TODO
		return ErrorCode();
	}

	ErrorCode CUdpListener::Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		sockaddr_in& refLocalAddr = this->GetLocalAddr();
		memset(&refLocalAddr, 0, sizeof(refLocalAddr));
		refLocalAddr.sin_family = AF_INET;

		if (szIp) { refLocalAddr.sin_addr.s_addr = inet_addr(GetIp(szIp)); }

		refLocalAddr.sin_port = htons(wPort);

		int dwError = 0;
		sockaddr_in oLocalAddr = this->GetLocalAddr();
		oLocalAddr.sin_addr.s_addr = 0;

		// ����socket
#ifdef _WIN32
		this->NativeSocket() = WSASocket(AF_INET
			, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
#else
		this->NativeSocket() = socket(AF_INET, SOCK_DGRAM, 0);
#endif // _WIN32

		if (this->NativeSocket() == -1)
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket create failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
		}

#ifdef _WIN32
		unsigned long ul = 1;
		if (SOCKET_ERROR == ioctlsocket(this->NativeSocket(), FIONBIO, (unsigned long*)&ul))
#else
		if (fcntl(this->NativeSocket(), F_SETFL, fcntl(this->NativeSocket(), F_GETFL) | O_NONBLOCK))
#endif // _WIN32
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set nonblock failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
				<< __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif // _WIN32

		// ��ַ����
		int nReuse = 1;
		if (setsockopt(this->NativeSocket(), SOL_SOCKET, SO_REUSEADDR, (char*)&nReuse, sizeof(nReuse)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " socket set SO_REUSEADDR failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		// bind
		if (bind(this->NativeSocket(), (sockaddr*)&oLocalAddr, sizeof(oLocalAddr)))
		{
#ifdef _WIN32
			dwError = WSAGetLastError();
#else // _WIN32
			dwError = errno;
#endif // _WIN32
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket()
					<< " bind failed on (" << inet_ntoa(oLocalAddr.sin_addr)
					<< ", " << (int)ntohs(oLocalAddr.sin_port) << ")(" << dwError << ")"
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:" << inet_ntoa(oLocalAddr.sin_addr)
			<< ", port:" << (int)ntohs(oLocalAddr.sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

		sockaddr_in addr;
		socklen_t sock_len = sizeof(addr);
		getsockname(this->NativeSocket(), (sockaddr*)&addr, &sock_len);

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:" << inet_ntoa(addr.sin_addr)
			<< ", port:" << (int)ntohs(addr.sin_port)
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

#ifdef _WIN32
		if (dwError = FxIoModule::Instance()->RegisterIO(this->NativeSocket(), this, pOStream))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << "bind failed on (" << inet_ntoa(oLocalAddr.sin_addr)
				<< ", " << (int)ntohs(oLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#else
		this->m_oAcceptPool.Init();
		memset(this->m_arroAcceptQueue, 0, sizeof(this->m_arroAcceptQueue));

		if (0 != (dwError = FxIoModule::Instance()->RegisterIO(this->NativeSocket(), EPOLLIN, this, pOStream)))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "bind failed on (" << inet_ntoa(oLocalAddr.sin_addr)
				<< ", " << (int)ntohs(oLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return dwError;
		}
#endif // _WIN32

#ifdef _WIN32
		for (int i = 0; i < 16; i++)
		{
			dwError = this->PostAccept(pOStream);
		}
#endif // _WIN32

		return dwError;
	}

	void CUdpListener::Close(std::ostream* pOStream)
	{
		//TODO
	}

	CUdpListener::IOReadOperation& CUdpListener::NewReadOperation()
	{
		IOReadOperation& refPeration = *(new IOReadOperation());
#ifdef _WIN32

		refPeration.m_stWsaBuff.buf = refPeration.m_szRecvBuff;
		refPeration.m_stWsaBuff.len = sizeof(refPeration.m_szRecvBuff);
		memset(refPeration.m_stWsaBuff.buf, 0, refPeration.m_stWsaBuff.len);
#endif // _WIN32
		return refPeration;
	}

	IOOperationBase& CUdpListener::NewWriteOperation()
	{
		static IOReadOperation oPeration;
		abort();
		return oPeration;
	}

	CUdpListener::IOErrorOperation& CUdpListener::NewErrorOperation(const ErrorCode& refError)
	{
		IOErrorOperation& refPeration = *(new IOErrorOperation());
		return refPeration;
	}

	void CUdpListener::OnError(const ErrorCode& refError, std::ostream* pOStream)
	{

	}

	void CUdpListener::OnClose(std::ostream* pOStream)
	{

	}

	CUdpListener& CUdpListener::OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream)
	{
		CUdpConnector* pUdpSock = new CUdpConnector((*this->m_pSessionMaker)());

		pUdpSock->GetSession()->SetSock(pUdpSock);
		if (int dwError = pUdpSock->SetRemoteAddr(address).Connect(hSock, address, pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock << " client connect failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

			//post ��iomodule �Ƴ�
			return *this;
		}

		if (int dwError = pUdpSock->Init(pOStream, ST_SYN_RECV))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock << " client connect failed(" << dwError << ")"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION__ << "]\n";

			//post ��iomodule �Ƴ�
			return *this;
		}

#ifdef _WIN32
		for (int i = 0; i < 16; ++i)
		{
			pUdpSock->PostRecv(pOStream);
		}
#endif // _WIN32

		return *this;
	}

#ifdef _WIN32
	ErrorCode CUdpListener::PostAccept(std::ostream* pOStream)
	{
		IOReadOperation& refOperation = this->NewReadOperation();

		DWORD dwReadLen = 0;
		DWORD dwFlags = 0;

		int dwSockAddr = sizeof(refOperation.m_stRemoteAddr);

		if (SOCKET_ERROR == WSARecvFrom(this->NativeSocket(), &refOperation.m_stWsaBuff, 1, &dwReadLen
			, &dwFlags, (sockaddr*)(&refOperation.m_stRemoteAddr), &dwSockAddr, (OVERLAPPED*)(IOOperationBase*)(&refOperation), NULL))
		{
			int dwError = WSAGetLastError();
			if (dwError != WSA_IO_PENDING)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << " WSARecvFrom errno : " << dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				return ErrorCode(dwError, __FILE__ __LINE2STR__(__LINE__));
			}
		}
		return ErrorCode();
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
		for (AcceptReq* pReq = this->m_arroAcceptQueue[this->GenerateAcceptHash(addr)]; pReq != NULL; pReq = pReq->m_pNext)
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
			unsigned int h = this->GenerateAcceptHash(pReq->addr);
			pReq->m_pNext = this->m_arroAcceptQueue[h];
			this->m_arroAcceptQueue[h] = pReq;
		}
	}

	void CUdpListener::RemoveAcceptReq(const sockaddr_in& addr)
	{
		AcceptReq* pReq = this->m_arroAcceptQueue[this->GenerateAcceptHash(addr)];
		AcceptReq* pPrev = NULL;

		while (pReq != NULL)
		{
			if (pReq->addr.sin_addr.s_addr == addr.sin_addr.s_addr &&
				pReq->addr.sin_port == addr.sin_port)
			{
				if (pPrev) pPrev->m_pNext = pReq->m_pNext;
				this->m_oAcceptPool.Free(pReq);
				break;
			}

			pPrev = pReq;
			pReq = pReq->m_pNext;
		}
	}
#endif // _WIN32

};

