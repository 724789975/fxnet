#include "udp_listener.h"
#include "../include/log_utility.h"
#include "../include/fxnet_interface.h"
#include "iothread.h"
#include "udp_connector.h"
#include <cstdlib>

#ifdef _WIN32
#include <WinSock2.h>
#include <Iphlpapi.h>
#include <iostream>
#pragma comment(lib,"Iphlpapi.lib") //需要添加Iphlpapi.lib库
#ifndef macro_closesocket
#define macro_closesocket closesocket
#endif //macro_closesocket
#else
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
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
			//记录网卡数量
			int netCardNum = 0;
			//记录每张网卡上的IP地址数量
			int IPnumPerNetCard = 0;
			//PIP_ADAPTER_INFO结构体指针存储本机网卡信息
			PIP_ADAPTER_INFO pIpAdapterInfo = new IP_ADAPTER_INFO();
			//得到结构体大小,用于GetAdaptersInfo参数
			unsigned long stSize = sizeof(IP_ADAPTER_INFO);
			//调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量;其中stSize参数既是一个输入量也是一个输出量
			int nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
			if (ERROR_BUFFER_OVERFLOW == nRel) {
				//如果函数返回的是ERROR_BUFFER_OVERFLOW
				//则说明GetAdaptersInfo参数传递的内存空间不够,同时其传出stSize,表示需要的空间大小
				//这也是说明为什么stSize既是一个输入量也是一个输出量
				//释放原来的内存空间
				delete pIpAdapterInfo;
				//重新申请内存空间用来存储所有网卡信息
				pIpAdapterInfo = (PIP_ADAPTER_INFO)new BYTE[stSize];
				//再次调用GetAdaptersInfo函数,填充pIpAdapterInfo指针变量
				nRel = GetAdaptersInfo(pIpAdapterInfo, &stSize);
			}
			PIP_ADAPTER_INFO p = pIpAdapterInfo;
			if (ERROR_SUCCESS == nRel) {
				//输出网卡信息
				//可能有多网卡,因此通过循环去判断
				while (pIpAdapterInfo)
				{
					//std::cout << "网卡数量：" << ++netCardNum << std::endl;
					//std::cout << "网卡名称：" << pIpAdapterInfo->AdapterName << std::endl;
					//std::cout << "网卡描述：" << pIpAdapterInfo->Description << std::endl;
					switch (pIpAdapterInfo->Type) {
					case MIB_IF_TYPE_OTHER:
						//std::cout << "网卡类型：" << "OTHER" << std::endl;
						break;
					case MIB_IF_TYPE_ETHERNET:
						//std::cout << "网卡类型：" << "ETHERNET" << std::endl;
						break;
					case MIB_IF_TYPE_TOKENRING:
						//std::cout << "网卡类型：" << "TOKENRING" << std::endl;
						break;
					case MIB_IF_TYPE_FDDI:
						//std::cout << "网卡类型：" << "FDDI" << std::endl;
						break;
					case MIB_IF_TYPE_PPP:
						//printf("PP\n");
						//std::cout << "网卡类型：" << "PPP" << std::endl;
						break;
					case MIB_IF_TYPE_LOOPBACK:
						//std::cout << "网卡类型：" << "LOOPBACK" << std::endl;
						break;
					case MIB_IF_TYPE_SLIP:
						//std::cout << "网卡类型：" << "SLIP" << std::endl;
						break;
					default:
						break;
					}
					//std::cout << "网卡MAC地址：";
					for (DWORD i = 0; i < pIpAdapterInfo->AddressLength; i++)
						if (i < pIpAdapterInfo->AddressLength - 1) {
							//printf("%02X-", pIpAdapterInfo->Address[i]);
						}
						else {
							//printf("%02X\n", pIpAdapterInfo->Address[i]);
						}
					//std::cout << "网卡IP地址如下：" << std::endl;
					//可能网卡有多IP,因此通过循环去判断
					IP_ADDR_STRING* pIpAddrString = &(pIpAdapterInfo->IpAddressList);
					do {
						if (strcmp(pIpAddrString->IpAddress.String, "127.0.0.1")
							&& strcmp(pIpAddrString->IpAddress.String, ""))
						{
							szRetIp = pIpAddrString->IpAddress.String;
							break;
						}
						//std::cout << "该网卡上的IP数量：" << ++IPnumPerNetCard << std::endl;
						//std::cout << "IP 地址：" << pIpAddrString->IpAddress.String << std::endl;
						//std::cout << "子网地址：" << pIpAddrString->IpMask.String << std::endl;
						//std::cout << "网关地址：" << pIpAdapterInfo->GatewayList.IpAddress.String << std::endl;
						pIpAddrString = pIpAddrString->Next;
					} while (pIpAddrString);

					pIpAdapterInfo = pIpAdapterInfo->Next;
					//std::cout << "--------------------------------------------------------------------" << std::endl;
				}
			}
			//释放内存空间
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
					if (AF_INET == (*ifc).ifa_addr->sa_family && IFF_UP & (*ifc).ifa_flags)
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

            return szRetIp.c_str();

#endif //!_WIN32
		}
		return szIp;
	}

	class UDPListenIOReadOperation : public IOOperationBase
	{
	public:
		friend class CUdpListener;
		virtual UDPListenIOReadOperation& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPListenIOReadOperation, this);

			CUdpListener& refSock = (CUdpListener&)refSocketBase;

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << refSock.NativeSocket() << ", " << refSock.Name()
				<< "\n";

#ifdef _WIN32
			UDPPacketHeader& oUDPPacketHeader = *(UDPPacketHeader*)m_stWsaBuff.buf;

			sockaddr_in& stRemoteAddr = this->m_stRemoteAddr;

			if (dwLen != sizeof(oUDPPacketHeader))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
					<< "\n";
				return *this;
			}

			if (oUDPPacketHeader.m_btStatus != ST_SYN_SEND)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
					<< "\n";
				return *this;
			}

			if (oUDPPacketHeader.m_btAck != 0)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "ack error want : 0, recv : " << (int)oUDPPacketHeader.m_btAck << ", " << refSocketBase.NativeSocket()
					<< "\n";
				return *this;
			}
			if (oUDPPacketHeader.m_btSyn != 1)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "syn error want : 1, recv : " << (int)oUDPPacketHeader.m_btSyn << ", " << refSocketBase.NativeSocket()
					<< "\n";
				return *this;
			}

			LOG(pOStream, ELOG_LEVEL_INFO) << refSock.NativeSocket() << " recvfrom "
				<< inet_ntoa(stRemoteAddr.sin_addr) << ":" << (int)ntohs(stRemoteAddr.sin_port)
				<< "\n";

			ISocketBase::NativeSocketType hSock = WSASocket(AF_INET
				, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, WSA_FLAG_OVERLAPPED);
			if (hSock == -1)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << " create socket failed."
					<< "\n";;
				return *this;
			}

			// set reuseaddr
			int yes = 1;
			if (setsockopt(hSock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << ", errno(" << WSAGetLastError() << ")"
					<< "\n";;
				macro_closesocket(hSock);
				return *this;
			}

			// bind
			if (bind(hSock, (sockaddr*)&refSock.GetLocalAddr(), sizeof(refSock.GetLocalAddr())))
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << ", errno(" << WSAGetLastError() << ") " << "bind failed on "
					<< inet_ntoa(refSock.GetLocalAddr().sin_addr) << ":" << (int)ntohs(refSock.GetLocalAddr().sin_port)
					<< "\n";
				macro_closesocket(hSock);
				return *this;
			}

			// send back
			refSock.OnClientConnected(hSock, m_stRemoteAddr, pOStream);

			refSock.PostAccept(refError, pOStream);
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
							<< "\n";
					}

					break;
				}

				if (dwLen != sizeof(oUDPPacketHeader))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
						<< "\n";
					continue;
				}

				if (oUDPPacketHeader.m_btStatus != ST_SYN_SEND)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket()
						<< "\n";
					continue;
				}

				if (oUDPPacketHeader.m_btAck != 0)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << "ack error want : 0, recv : " << oUDPPacketHeader.m_btAck << ", " << refSocketBase.NativeSocket()
						<< "\n";
					continue;
				}
				if (oUDPPacketHeader.m_btSyn != 1)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << "syn error want : 0, recv : " << oUDPPacketHeader.m_btSyn << ", " << refSocketBase.NativeSocket()
						<< "\n";
					continue;
				}

				CUdpListener::AcceptReq* req = refSock.GetAcceptReq(stRemoteAddr);

				if (req != NULL)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << ", req not null"
						<< "\n";
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
					<< "\n";

				LOG(pOStream, ELOG_LEVEL_DEBUG2) << refSock.NativeSocket() << " ip:" << inet_ntoa(refSock.GetLocalAddr().sin_addr)
					<< ", port:" << (int)ntohs(refSock.GetLocalAddr().sin_port)
					<< "\n";

				LOG(pOStream, ELOG_LEVEL_INFO) << refSock.NativeSocket() << " recvfrom "
					<< inet_ntoa(stRemoteAddr.sin_addr) << ":" << (int)ntohs(stRemoteAddr.sin_port)
					<< "\n";

				req->m_oAcceptSocket = socket(AF_INET, SOCK_DGRAM, 0);
				//req->m_oAcceptSocket = refSock.NativeSocket();
				if (req->m_oAcceptSocket == -1)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << refSock.NativeSocket() << " create socket failed."
						<< "\n";;
					refSock.m_oAcceptPool.Free(req);
					continue;
				}

				// cloexec
				fcntl(req->m_oAcceptSocket, F_SETFD, FD_CLOEXEC);

				// set reuseaddr
				int yes = 1;
				if (setsockopt(req->m_oAcceptSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << req->m_oAcceptSocket << ", errno(" << errno << ")"
						<< "\n";;
					macro_closesocket(req->m_oAcceptSocket);
					refSock.m_oAcceptPool.Free(req);
					continue;
				}

				// bind
				if (bind(req->m_oAcceptSocket, (sockaddr*)&refSock.GetLocalAddr(), sizeof(refSock.GetLocalAddr())))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << req->m_oAcceptSocket << ", errno(" << errno << ") " << "bind failed on "
						<< inet_ntoa(refSock.GetLocalAddr().sin_addr) << ":" << (int)ntohs(refSock.GetLocalAddr().sin_port)
						<< "\n";
					macro_closesocket(req->m_oAcceptSocket);
					refSock.m_oAcceptPool.Free(req);
					continue;
				}

				refSock.AddAcceptReq(req);

				//GetFxIoModule(0)->DeregisterIO(refSock.NativeSocket(), pOStream);
				//macro_closesocket(refSock.NativeSocket());

				// send back
				refSock.OnClientConnected(req->m_oAcceptSocket, req->addr, pOStream);

				//refSock.Listen(pOStream);
				//TODO 是否需要break
				break;
			}

			// 移除缓存的req
			CUdpListener::AcceptReq* pReq;
			for (unsigned int i = 0; i < CUdpListener::UDP_ACCEPT_HASH_SIZE; i++)
			{
				while ((pReq = refSock.m_arroAcceptQueue[i]) != NULL)
				{
					refSock.m_arroAcceptQueue[i] = pReq->m_pNext;

					refSock.m_oAcceptPool.Free(pReq);
				}
			}

#endif
			return *this;
		}
#ifdef _WIN32
		WSABUF m_stWsaBuff;
		sockaddr_in m_stRemoteAddr;
#endif // _WIN32
		char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
	};

	UDPListenIOReadOperation& NewUDPListenIOReadOperation()
	{
		UDPListenIOReadOperation& refPeration = *(new UDPListenIOReadOperation());
#ifdef _WIN32

		refPeration.m_stWsaBuff.buf = refPeration.m_szRecvBuff;
		refPeration.m_stWsaBuff.len = sizeof(refPeration.m_szRecvBuff);
		memset(refPeration.m_stWsaBuff.buf, 0, refPeration.m_stWsaBuff.len);
#endif // _WIN32
		return refPeration;
	}

	class UDPIOErrorOperation : public IOOperationBase
	{
	public:
		friend class CUdpListener;
		virtual UDPIOErrorOperation& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPIOErrorOperation, this);
			LOG(pOStream, ELOG_LEVEL_ERROR) << refSocketBase.NativeSocket() << " IOErrorOperation failed(" << m_oError.What() << ")"
				<< "\n";
			return *this;
		}
	};

	CUdpListener::CUdpListener(SessionMaker* pMaker)
		: m_pSessionMaker(pMaker)
	{

	}

	CUdpListener& CUdpListener::Update(double dTimedouble, ErrorCode& refError, std::ostream* pOStream)
	{
		//TODO
		return *this;
	}

	CUdpListener& CUdpListener::Listen(const char* szIp, unsigned short wPort, ErrorCode& refError, std::ostream* pOStream)
	{
		sockaddr_in& refLocalAddr = this->GetLocalAddr();
		memset(&refLocalAddr, 0, sizeof(refLocalAddr));
		refLocalAddr.sin_family = AF_INET;

        szIp = GetIp(szIp);
        LOG(pOStream, ELOG_LEVEL_INFO) << "Listen at " << szIp << "\n";
		refLocalAddr.sin_addr.s_addr = inet_addr(szIp);

		refLocalAddr.sin_port = htons(wPort);

		int dwError = 0;
		sockaddr_in oLocalAddr = this->GetLocalAddr();
		oLocalAddr.sin_addr.s_addr = 0;

		// 创建socket
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
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
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
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

#ifndef _WIN32
		if (fcntl(this->NativeSocket(), F_SETFD, FD_CLOEXEC))
		{
			dwError = errno;
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();

			LOG(pOStream, ELOG_LEVEL_ERROR) << "socket set FD_CLOEXEC failed(" << dwError << ") ["
				<< __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#endif // _WIN32

		// 地址重用
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
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
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
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG2) << this->NativeSocket() << " ip:" << inet_ntoa(oLocalAddr.sin_addr)
			<< ", port:" << (int)ntohs(oLocalAddr.sin_port)
			<< "\n";

		sockaddr_in addr;
		socklen_t sock_len = sizeof(addr);
		getsockname(this->NativeSocket(), (sockaddr*)&addr, &sock_len);

		LOG(pOStream, ELOG_LEVEL_INFO) << this->NativeSocket() << " ip:" << inet_ntoa(addr.sin_addr)
			<< ", port:" << (int)ntohs(addr.sin_port)
			<< "\n";

#ifdef _WIN32
		if (dwError = GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), this, pOStream))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << this->NativeSocket() << "bind failed on (" << inet_ntoa(oLocalAddr.sin_addr)
				<< ", " << (int)ntohs(oLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#else
		this->m_oAcceptPool.Init();
		memset(this->m_arroAcceptQueue, 0, sizeof(this->m_arroAcceptQueue));

		if (0 != (dwError = GetFxIoModule(this->GetIOModuleIndex())->RegisterIO(this->NativeSocket(), EPOLLIN, this, pOStream)))
		{
			macro_closesocket(this->NativeSocket());
			this->NativeSocket() = (NativeSocketType)InvalidNativeHandle();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "bind failed on (" << inet_ntoa(oLocalAddr.sin_addr)
				<< ", " << (int)ntohs(oLocalAddr.sin_port) << ")(" << dwError << ")"
				<< "\n";
			refError(dwError, __FILE__ ":" __LINE2STR__(__LINE__));
			return *this;
		}
#endif // _WIN32

#ifdef _WIN32
		for (int i = 0; i < 16; i++)
		{
			this->PostAccept(refError, pOStream);
			if (refError)
			{
				break;
			}
		}
#endif // _WIN32

		return *this;
	}

	void CUdpListener::Close(std::ostream* pOStream)
	{
		//TODO
	}

	IOOperationBase& CUdpListener::NewReadOperation()
	{
		return NewUDPListenIOReadOperation();
	}

	IOOperationBase& CUdpListener::NewWriteOperation()
	{
		static UDPListenIOReadOperation oPeration;
		abort();
		return oPeration;
	}

	IOOperationBase& CUdpListener::NewErrorOperation(const ErrorCode& refError)
	{
		UDPIOErrorOperation& refPeration = *(new UDPIOErrorOperation());
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
		pUdpSock->SetIOModuleIndex(GetFxIoModuleIndex());

		pUdpSock->GetSession()->SetSock(pUdpSock);
		pUdpSock->SetRemoteAddr(address);

		class UdpConnect : public IOEventBase
		{
		public:
			UdpConnect(CUdpConnector* pTcpSock, NativeSocketType hSock)
				: m_pUdpSock(pTcpSock)
				, m_hSock(hSock)
			{}
			virtual void operator ()(std::ostream* pOStream)
			{
				DELETE_WHEN_DESTRUCT(UdpConnect, this);

				ErrorCode oError;
				m_pUdpSock->Connect(m_hSock, m_pUdpSock->GetRemoteAddr(), oError, pOStream);
				if (oError)
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSock << " client connect failed(" << oError.What() << ")"
						<< "\n";

					m_pUdpSock->NewErrorOperation(oError)(*m_pUdpSock, 0, oError, pOStream);
					return;
				}

				if (int dwError = m_pUdpSock->Init(pOStream, ST_SYN_RECV))
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << m_hSock << " client connect failed(" << dwError << ")"
						<< "\n";

					m_pUdpSock->NewErrorOperation(dwError)(*m_pUdpSock, 0, oError, pOStream);
					return;
				}

#ifdef _WIN32
				for (int i = 0; i < 16; ++i)
				{
					m_pUdpSock->PostRecv(pOStream);
				}
#endif // _WIN32

			}
		protected:
		private:
			CUdpConnector* m_pUdpSock;
			NativeSocketType m_hSock;
		};

		PostEvent(pUdpSock->GetIOModuleIndex(), new UdpConnect(pUdpSock, hSock));

		return *this;
	}

#ifdef _WIN32
	CUdpListener& CUdpListener::PostAccept(ErrorCode& refError, std::ostream* pOStream)
	{
		UDPListenIOReadOperation& refOperation = NewUDPListenIOReadOperation();

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
					<< "\n";
				refError(dwError, __FILE__ __LINE2STR__(__LINE__));
			}
		}
		return *this;
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

