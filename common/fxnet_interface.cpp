#include "include/fxnet_interface.h"
#include "iothread.h"

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
};

