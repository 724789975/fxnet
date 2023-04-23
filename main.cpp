#include "include/fxnet_interface.h"
#include "text_session.h"

#ifdef GPERF
#include "gperftools/tcmalloc.h"
#include "gperftools/profiler.h"
#include "gperftools/heap-profiler.h"
#include "gperftools/malloc_extension.h"
#endif	//!GPERF

#ifndef _WIN32
#include <unistd.h>
#endif
#include <signal.h>

bool g_bProFileState = false;
void OnSig45(int sig)
{
	g_bProFileState = !g_bProFileState;
#ifdef GPERF
	if (g_bProFileState)
	{
		ProfilerStart("prefix");
	}
	else
	{
		ProfilerStop();
	}
#endif	//!GPERF
	
	std::cout <<__FUNCTION__ << "\n";
}

bool g_bHeapProFileState = false;
void OnSig46(int n)
{
	g_bHeapProFileState = !g_bHeapProFileState;
#ifdef GPERF
	if (g_bHeapProFileState)
	{
		HeapProfilerStart("heap_prefix");
	}
	else
	{
		HeapProfilerStop();
	}
#endif	//!GPERF
	
	std::cout <<__FUNCTION__ << "\n";
}

void OnSig47(int sig)
{
#ifdef GPERF
	MallocExtension::instance()->ReleaseFreeMemory();
#endif	//!GPERF
}

int main()
{
	signal(45, OnSig45);
	signal(46, OnSig46);
	signal(47, OnSig47);
	FXNET::StartLogModule();
#ifdef _WIN32
	Sleep(1);
#else
	usleep(1000);
#endif // _WIN32

	FXNET::StartIOModule();
	for (int i = 0; i < 100; ++i)
	{
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}

	std::vector<CTextSession*> vecSession;
	//FXNET::PostEvent(new FXNET::UDPListen("192.168.10.104", 10085, new TextSessionMaker));
	vecSession.push_back(new CTextSession);
	//CTextSession t1;
	FXNET::PostEvent(new FXNET::UDPConnect("81.70.54.105", 10086, vecSession.back()));
	//FXNET::PostEvent(new FXNET::UDPConnect("81.70.54.105", 10085, vecSession.back()));
	//FXNET::PostEvent(new FXNET::UDPConnect("192.168.10.104", 10085, &vecSession.back()));
	//CTextSession t2;
	// vecSession.push_back(new CTextSession);
	// FXNET::PostEvent(new FXNET::UDPConnect("192.168.30.1", 10085, vecSession.back()));
	//FXNET::PostEvent(new FXNET::UDPConnect("81.70.54.105", 10085, vecSession.back()));

	std::stringstream* pStrstream = FXNET::GetStream();
	pStrstream->flags(std::cout.fixed);
	for (int i = 0; ; ++i)
	{
		if (i >= 10 && i < 100)
		{
			//vecSession.push_back(new CTextSession);
			//FXNET::PostEvent(new FXNET::UDPConnect("192.168.30.1", 10085, vecSession.back()));
			//FXNET::PostEvent(new FXNET::UDPConnect("81.70.54.105", 10085, vecSession.back()));
		}
#ifdef __SINGLE_THREAD__
		FXNET::FxIoModule::Instance()->DealFunction(pStrstream);
#endif	//!__SINGLE_THREAD__
		std::deque<MessageEventBase*> dequeMessage;
		FXNET::SwapEvent(dequeMessage);

		if(0 == dequeMessage.size())
		{
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif // _WIN32
			continue;
		}
		for (std::deque<MessageEventBase*>::iterator it = dequeMessage.begin()
			; it != dequeMessage.end(); ++it)
		{
			(**it)(pStrstream);
		}
		FXNET::PushLog(pStrstream);
		pStrstream->flags(std::cout.fixed);
	}
}
