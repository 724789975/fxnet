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
	for (int i = 0; i < 1000; ++i)
	{
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}

	FXNET::PostEvent(new FXNET::UDPListen("0.0.0.0", 10085));
	FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.103", 10085));

	for (int i = 0; ; ++i)
	{
		if (i == 10000)
		{
			//FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.103", 10085));
		}
		std::deque<MessageEventBase*> dequeMessage;
		FXNET::SwapEvent(dequeMessage);

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}
}