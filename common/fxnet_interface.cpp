#include "../include/fxnet_interface.h"
#include "iothread.h"
#include "log_module.h"
#include "udp_listener.h"
#include "udp_connector.h"
#include "tcp_listener.h"
#include "tcp_connector.h"

#include <iostream>

namespace FXNET
{

	void StartIOModule(MessageEventQueue* pQueue)
	{
		for (unsigned int i = 0; i < GetFxIoModuleNum(); ++i)
		{
			GetFxIoModule(i)->Init(i, pQueue, &std::cout);
		}
	}

	void StartLogModule()
	{
		LogModule::CreateInstance();
		LogModule::Instance()->Init();	
	}

	unsigned int GetFxIoModuleIndex()
	{
		static unsigned int dwIndex = 0;
		return dwIndex++;
	}

	void PostEvent(unsigned int dwIoModuleIndex, IOEventBase* pEvent)
	{
		GetFxIoModule(dwIoModuleIndex)->PostEvent(pEvent);
	}

	void UdpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream)
	{
		CUdpListener* pListener = new CUdpListener(pSessionMaker);
		pListener->SetIOModuleIndex(dwIOModuleIndex);
		pListener->Listen(szIp, wPort, pOStream);
	}

	void UdpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream)
	{
		CUdpConnector* pConnector = new CUdpConnector(pSession);
		pConnector->SetIOModuleIndex(dwIOModuleIndex);

		pSession->SetSock(pConnector);

		sockaddr_in stRemoteAddr;
		memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
		stRemoteAddr.sin_family = AF_INET;

		if (szIp) { stRemoteAddr.sin_addr.s_addr = inet_addr(szIp); }

		stRemoteAddr.sin_port = htons(wPort);
		if (int dwError = pConnector->SetRemoteAddr(stRemoteAddr).Connect(stRemoteAddr, pOStream))
		{
			pConnector->NewErrorOperation(dwError)(*pConnector, 0, pOStream);
		}
	}

	void TcpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream)
	{
		CTcpListener* pListener = new CTcpListener(pSessionMaker);
		pListener->SetIOModuleIndex(dwIOModuleIndex);
		pListener->Listen(szIp, wPort, pOStream);
	}

	void TcpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream)
	{
		CTcpConnector* pConnector = new CTcpConnector(pSession);
		pConnector->SetIOModuleIndex(dwIOModuleIndex);

		pSession->SetSock(pConnector);

		sockaddr_in stRemoteAddr;
		memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
		stRemoteAddr.sin_family = AF_INET;

		if (szIp) { stRemoteAddr.sin_addr.s_addr = inet_addr(szIp); }

		stRemoteAddr.sin_port = htons(wPort);
		if (int dwError = pConnector->SetRemoteAddr(stRemoteAddr).Connect(stRemoteAddr, pOStream))
		{
			pConnector->NewErrorOperation(dwError)(*pConnector, 0, pOStream);
		}
	}

	std::stringstream* GetStream()
	{
		return LogModule::Instance()->GetStream();
	}

	void PushLog(std::stringstream*& pStrstream)
	{
		LogModule::Instance()->PushLog(pStrstream);
	}

};

