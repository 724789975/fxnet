#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_

#include "util.h"


class Connector {
public:
	virtual ~Connector() {}
	virtual void UdpConnect(const char* szIp, unsigned short wPort) = 0;
	virtual void TcpConnect(const char* szIp, unsigned short wPort) = 0;
	virtual void Send(const char* szData, unsigned int dwLen) = 0;
	virtual void Close() = 0;
private:
};

extern "C" {
/**
 * @brief �������ݻص�
 * 
 * @param pConnector ������
 * @param pData ����
 * @param nLen ����
 */
typedef FX_API void OnRecvCallback(Connector* pConnector, const char* pData, int nLen);

/**
 * @brief ���ӳɹ��ص�
 * 
 * @param pConnector ������
 */
typedef FX_API void OnConnectedCallback(Connector* pConnector);

/**
 * @brief ����ص�
 * 
 * @param pConnector ������
 * @param refError ������
 */
typedef FX_API void OnErrorCallback(Connector* pConnector, const int& refError);

/**
 * @brief �رջص�
 * 
 * @param pConnector ������
 */
typedef FX_API void OnCloseCallback(Connector* pConnector);

/**
 * @brief ����������
 * 
 * @param onRecv �������ݻص�
 * @param onConnected ���ӳɹ��ص�
 * @param onError ����ص�
 * @param onClose �رջص�
 * 
 * @return ������
 * 
 * @note �����������������ɵ����߹���
 */
FX_API Connector* CreateConnector(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose);

/**
 * @brief ����������
 * 
 * @param pConnector ������
 * 
 * @note �����������������ɵ����߹���
 */
FX_API void DestroyConnector(Connector* pConnector);

/**
 * @brief udp����
 * 
 * @param pConnector ������
 * @param szIp ip��ַ
 * @param wPort �˿�
 */
FX_API void UdpConnect(Connector* pConnector, const char* szIp, unsigned short wPort);

/**
 * @brief tcp����
 * 
 * @param pConnector ������
 * @param szIp ip��ַ
 * @param wPort �˿�
 */
FX_API void TcpConnect(Connector* pConnector, const char* szIp, unsigned short wPort);

/**
 * @brief ��������
 * 
 * @param pConnector ������
 * @param szData ����
 * @param dwLen ����
 */
FX_API void Send(Connector* pConnector, const char* szData, unsigned int dwLen);

/**
 * @brief �ر�����
 * 
 * @param pConnector ������
 * @note �ر����Ӻ󣬲����ٷ�������
 */
FX_API void Close(Connector* pConnector);

/**
 * @brief ����IOģ��
 * @note ����IOģ�����ܽ�������
 */
FX_API void StartIOModule();

/**
 * @brief ����IOģ��
 * @note ����IOģ�����ܽ�������
 * @note ��Ҫ��ÿ֡����һ��
 */
FX_API void ProcessIOModule();

/**
 * @brief ��־�ص�
 * 
 * @param pLog ��־����
 * @param nLen ��־����
 * @note ��־�ص��������ڽ���ioģ�����־��Ϣ
 */
typedef FX_API void OnLogCallback(const char* pLog, int nLen);

/**
 * @brief ������־�ص�
 * 
 * @param onLog ��־�ص�����
 * @note ������־�ص���ioģ�����־��Ϣ��ͨ���ص���������
 */
FX_API void SetLogCallback(OnLogCallback* onLog);

}
#endif // _CONNECTOR_H_