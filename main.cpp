#include "common/buff_contral.h"
#include "include/cas_lock.h"
#include "include/iothread.h"

#include "include/fxnet_interface.h"
#include "text_session.h"

#ifndef _WIN32
#include <unistd.h>
#endif

int main()
{
	FXNET::StartIOModule();
	for (int i = 0; i < 100; ++i)
	{
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}

	std::vector<CTextSession> vecSession;
	FXNET::PostEvent(new FXNET::UDPListen("192.168.10.104", 10085, new TextSessionMaker));
	//vecSession.push_back(CTextSession());
	CTextSession t1;
	FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.104", 10085, &t1));
	//FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.104", 10085, &vecSession.back()));
	CTextSession t2;
	FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.104", 10085, &t2));

	for (int i = 0; ; ++i)
	{
		//if (i >= 10 && i < 30)
		//{
		//	vecSession.push_back(CTextSession());
		//	FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.104", 10085, &vecSession.back()));
		//}
		std::deque<MessageEventBase*> dequeMessage;
		FXNET::SwapEvent(dequeMessage);

		for (auto& p : dequeMessage)
		{
			(*p)(&std::cout);
		}

#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}
}