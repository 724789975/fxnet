#include "include/fxnet_interface.h"
#include "iothread.h"
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

	void UdpListen(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		CUdpListener* pListener = new CUdpListener;
		pListener->Listen(szIp, wPort, pOStream);
	}

	void UdpConnect(const char* szIp, unsigned short wPort, std::ostream* pOStream)
	{
		CUdpConnector* pConnector = new CUdpConnector;

		sockaddr_in stRemoteAddr;
		memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
		stRemoteAddr.sin_family = AF_INET;

		if (szIp) { stRemoteAddr.sin_addr.s_addr = inet_addr(szIp); }

		stRemoteAddr.sin_port = htons(wPort);
		static BinaryMessageParse s_oBinaryMessageParse;
		pConnector->m_pMessageParse = &s_oBinaryMessageParse;
		if (int dwError = pConnector->SetRemoteAddr(stRemoteAddr).Connect(stRemoteAddr, pOStream))
		{
			pConnector->NewErrorOperation(dwError)(*pConnector, 0, pOStream);
		}
	}

};

