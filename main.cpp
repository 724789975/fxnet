#include "include/fxnet_interface.h"
#include "include/message_queue.h"
#include "text_session.h"
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
#include <vector>

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

	FXNET::MessageEventQueue oQueue;
	FXNET::StartIOModule(&oQueue);
	for (int i = 0; i < 100; ++i)
	{
#ifdef _WIN32
		Sleep(1);
#else
		usleep(1000);
#endif // _WIN32
	}

	// const char* szTargetIp = "192.168.85.128";
	const char* szTargetIp = "127.0.0.1";
	//const char* szTargetIp = "114.132.124.13";
	// const char* szTargetIp = "81.70.54.105";
	//const char* szTargetIp = "192.168.122.128";

	std::vector<CTextSession*> vecSession;

#define SERVER_TEST 1

#if SERVER_TEST
#define TCP_CONNECT(ip, tcp_port) {}
#define UDP_CONNECT(ip, udp_port) {}
#else
#define TCP_CONNECT(ip, tcp_port) \
{\
	vecSession.push_back(new CWSSession());\
	int dwIndex1 = FXNET::GetFxIoModuleIndex();\
	FXNET::PostEvent(dwIndex1, new FXNET::TCPConnect(ip, tcp_port, dwIndex1, vecSession.back()));\
}\

#define UDP_CONNECT(ip, udp_port) \
{\
	vecSession.push_back(new CWSSession());\
	int dwIndex1 = FXNET::GetFxIoModuleIndex();\
	FXNET::PostEvent(dwIndex1, new FXNET::UDPConnect(ip, udp_port, dwIndex1, vecSession.back()));\
}\

#endif

#if SERVER_TEST
#define TCP_LISTEN(tcp_port) \
{\
	int dwIndex1 = FXNET::GetFxIoModuleIndex();\
	FXNET::PostEvent(dwIndex1, new FXNET::TCPListen("0.0.0.0", tcp_port, dwIndex1, new TextSessionMaker));\
}\

#define UDP_LISTEN(udp_port) \
{\
	int dwIndex1 = FXNET::GetFxIoModuleIndex();\
	FXNET::PostEvent(dwIndex1, new FXNET::UDPListen("0.0.0.0", udp_port, dwIndex1, new TextSessionMaker));\
}\

#else
#define TCP_LISTEN(tcp_port){}
#define UDP_LISTEN(udp_port){}
#endif

	TCP_LISTEN(10085);
	UDP_LISTEN(10086);

	std::stringstream* pStrstream = FXNET::GetStream();
	pStrstream->flags(std::cout.fixed);
	for (int i = 0; ; ++i)
	{
		if (i >= 10 && i < 11)
		{
			TCP_CONNECT(szTargetIp, 10085);
			// UDP_CONNECT(szTargetIp, 10086);
		}
#ifdef __SINGLE_THREAD__
		FXNET::ProcSignelThread(pStrstream);
#endif	//!__SINGLE_THREAD__
		std::deque<MessageEventBase*> dequeMessage;
		oQueue.SwapEvent(dequeMessage);

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
		pStrstream = FXNET::GetStream();
		pStrstream->flags(std::cout.fixed);
	}
}
