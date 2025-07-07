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
   	virtual int UdpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort)
	{
		ErrorCode code;
		FXNET::UdpConnect(dwIOModuleIndex, szIp, wPort, &m_oSession, code, nullptr);
		return code;
	}
	virtual int TcpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort)
	{
		ErrorCode code;
		FXNET::UdpConnect(dwIOModuleIndex, szIp, wPort, &m_oSession, code, nullptr);
		return code;
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