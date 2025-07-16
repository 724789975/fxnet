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

	void InitLogModule()
	{
		LogModule::CreateInstance();
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

	void ProcSignelThread(std::stringstream*& pStrstream)
	{
		FXNET::GetFxIoModule(0)->DealFunction(pStrstream);
	}

	void PostEvent(unsigned int dwIoModuleIndex, IOEventBase *pEvent)
	{
		GetFxIoModule(dwIoModuleIndex)->PostEvent(pEvent);
	}

	void UdpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream)
	{
		CUdpListener* pListener = new CUdpListener(pSessionMaker);
		pListener->SetIOModuleIndex(dwIOModuleIndex);
		ErrorCode oError;
		pListener->Listen(szIp, wPort, oError, pOStream);
	}

	void UdpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, ErrorCode& refError, std::ostream* pOStream)
	{
		CUdpConnector* pConnector = new CUdpConnector(pSession);
		pConnector->SetIOModuleIndex(dwIOModuleIndex);

		pSession->SetSock(pConnector);

		sockaddr_in stRemoteAddr;
		memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
		stRemoteAddr.sin_family = AF_INET;

		if (szIp) { stRemoteAddr.sin_addr.s_addr = inet_addr(szIp); }

		stRemoteAddr.sin_port = htons(wPort);
		pConnector->SetRemoteAddr(stRemoteAddr).Connect(stRemoteAddr, refError, pOStream);
		if (refError)
		{
			pConnector->NewErrorOperation(refError)(*pConnector, 0, refError, pOStream);
		}
	}

	void TcpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream)
	{
		CTcpListener* pListener = new CTcpListener(pSessionMaker);
		pListener->SetIOModuleIndex(dwIOModuleIndex);
		ErrorCode oError;
		pListener->Listen(szIp, wPort, oError, pOStream);
	}

	void TcpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, ErrorCode& refError, std::ostream* pOStream)
	{
		CTcpConnector* pConnector = new CTcpConnector(pSession);
		pConnector->SetIOModuleIndex(dwIOModuleIndex);

		pSession->SetSock(pConnector);

		sockaddr_in stRemoteAddr;
		memset(&stRemoteAddr, 0, sizeof(stRemoteAddr));
		stRemoteAddr.sin_family = AF_INET;

		if (szIp) { stRemoteAddr.sin_addr.s_addr = inet_addr(szIp); }

		stRemoteAddr.sin_port = htons(wPort);
		pConnector->SetRemoteAddr(stRemoteAddr).Connect(stRemoteAddr, refError, pOStream);
		if (refError)
		{
			pConnector->NewErrorOperation(refError)(*pConnector, 0, refError, pOStream);
		}
	}

	void PushLog(std::stringstream* pStrstream)
	{
		LogModule::Instance()->PushLog(pStrstream);
	}

	/**
	 * @brief 获取日志字符串，并清空日志缓存
	 * 
	 * @return const char* 
	 * @note 如果不使用日志线程，那么每帧都要调用一下这个函数
	 */
	const char* GetLogStr()
	{
		return LogModule::Instance()->GetLogStr();
	}
};

