#include "../include/connector.h"
#include "text_session.h"
#include "../../include/fxnet_interface.h"
#include "../../include/error_code.h"

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
		m_oSession.Send(szData, dwLen, nullptr);
    }
    virtual void Close()
    {
		m_oSession.Close(nullptr);
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