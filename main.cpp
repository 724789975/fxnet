#include "common/buff_contral.h"
#include "common/cas_lock.h"
#include "common/iothread.h"

#include "include/fxnet_interface.h"

#ifndef _WIN32
#include <unistd.h>
#endif

int main()
{
	FXNET::StartIOModule();

	while (true)
	{
		std::deque<MessageEventBase*> dequeMessage;
		FXNET::SwapEvent(dequeMessage);

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}
}