#include "include/fxnet_interface.h"
#include "include/iothread.h"
#include "udp_listener.h"
#include "udp_connector.h"

#include <iostream>

namespace FXNET
{

	void StartIOModule()
	{
		FxIoModule::CreateInstance();
		FxIoModule::Instance()->Init(&std::cout);
	}

	void PostEvent(IOEventBase* pEvent)
	{
		FxIoModule::Instance()->PostEvent(pEvent);
	}

	void SwapEvent(std::deque<MessageEventBase*>& refDeque)
	{
		FxIoModule::Instance()->SwapEvent(refDeque);
	}

	void UdpListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream)
	{
		CUdpListener* pListener = new CUdpListener(pSessionMaker);
		pListener->Listen(szIp, wPort, pOStream);
	}

	void UdpConnect(const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream)
	{
		CUdpConnector* pConnector = new CUdpConnector(pSession);

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

};

