#include "../include/connector.h"
#include "text_session.h"
#include "../../include/fxnet_interface.h"
#include "../../include/error_code.h"
#include "connector.h"

OnLogCallback *g_pLogCallback = nullptr;
	std::stringstream g_LogStream;

class ConnectorImpl : public Connector {
public:
    ConnectorImpl(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose)
		: m_oSession(this, onRecv, onConnected, onError, onClose)
    {
    }
    virtual ~ConnectorImpl()
    {
    }
   	virtual void UdpConnect(const char* szIp, unsigned short wPort)
	{
		int dwIndex1 = FXNET::GetFxIoModuleIndex();
		FXNET::PostEvent(dwIndex1, new FXNET::UDPConnect(szIp, wPort, dwIndex1, &m_oSession));
	}
	virtual void TcpConnect(const char* szIp, unsigned short wPort)
	{
		int dwIndex1 = FXNET::GetFxIoModuleIndex();
		FXNET::PostEvent(dwIndex1, new FXNET::TCPConnect(szIp, wPort, dwIndex1, &m_oSession));
	}
    virtual void Send(const char* szData, unsigned int dwLen)
    {
		m_oSession.Send(szData, dwLen, &g_LogStream);
    }
    virtual void Close()
    {
		m_oSession.Close(&g_LogStream);
    }
private:
	CTextSession m_oSession;
};

Connector* CreateConnector(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose)
{
    return new ConnectorImpl(onRecv, onConnected, onError, onClose);
}
void DestroyConnector(Connector *pConnector)
{
	delete pConnector;
}

FX_API void UdpConnect(Connector *pConnector, const char *szIp, unsigned short wPort)
{
	pConnector->UdpConnect(szIp, wPort);
}

FX_API void TcpConnect(Connector *pConnector, const char *szIp, unsigned short wPort)
{
	pConnector->TcpConnect(szIp, wPort);
}

FX_API void Send(Connector *pConnector, const char *szData, unsigned int dwLen)
{
	pConnector->Send(szData, dwLen);
}

FX_API void Close(Connector *pConnector)
{
	pConnector->Close();
}

FXNET::MessageEventQueue oQueue;
void StartIOModule()
{
	FXNET::InitLogModule();
	FXNET::StartIOModule(&oQueue);
}

FX_API void ProcessIOModule()
{
#ifdef __SINGLE_THREAD__
	FXNET::ProcSignelThread(&g_LogStream);
#endif	//!__SINGLE_THREAD__
	std::deque<MessageEventBase*> dequeMessage;
	oQueue.SwapEvent(dequeMessage);

	for (std::deque<MessageEventBase*>::iterator it = dequeMessage.begin()
		; it != dequeMessage.end(); ++it)
	{
		(**it)(&g_LogStream);
	}
	FXNET::PushLog(&g_LogStream);
	g_LogStream.str("");
	const char* szLog = FXNET::GetLogStr();
	if (g_pLogCallback)
	{
		if (size_t nLen = strlen(szLog))
		{
			g_pLogCallback(szLog, strlen(szLog));
		}
	}
}

FX_API void SetLogCallback(OnLogCallback *onLog)
{
	g_pLogCallback = onLog;
}
