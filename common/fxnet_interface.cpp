#include "include/fxnet_interface.h"
#include "iothread.h"
#include "udp_listener.h"

#include <iostream>

namespace FXNET
{

	void StartIOModule()
	{
		FxIoModule::CreateInstance();
		FxIoModule::Instance()->Init(&std::cout);
		FxIoModule::Instance()->Start();
	}

	void SwapEvent(std::deque<MessageEventBase*>& refDeque)
	{
		FxIoModule::Instance()->SwapEvent(refDeque);
	}

	void UdpListen(const char* szIp, unsigned short wPort)
	{
		CUdpListener* pListener = new CUdpListener;
	}
};

