#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_

#include "util.h"

class FX_API Connector {
public:
	virtual ~Connector() {}
	virtual void UdpConnect(const char* szIp, unsigned short wPort) = 0;
	virtual void TcpConnect(const char* szIp, unsigned short wPort) = 0;

	virtual void Send(const char* szData, unsigned int dwLen) = 0;

	virtual void Close() = 0;
private:
};

typedef FX_API void OnRecvCallback(Connector* pConnector, const char* pData, int nLen);
typedef FX_API void OnConnectedCallback(Connector* pConnector);
typedef FX_API void OnErrorCallback(Connector* pConnector, const int& refError);
typedef FX_API void OnCloseCallback(Connector* pConnector);

FX_API Connector* CreateConnector(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose);
FX_API void DestroyConnector(Connector* pConnector);

#endif // _CONNECTOR_H_