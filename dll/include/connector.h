#ifndef _CONNECTOR_H_
#define _CONNECTOR_H_

#include "util.h"

namespace FXNET {
	class ISession;
};
class Connector {
public:
	virtual ~Connector() {}
	virtual void UdpConnect(const char* szIp, unsigned short wPort) = 0;
	virtual void TcpConnect(const char* szIp, unsigned short wPort) = 0;
	virtual void Send(const char* szData, unsigned int dwLen) = 0;
	virtual void Close() = 0;
	virtual FXNET::ISession* GetSession() = 0;
private:
};

extern "C" {
	/**
	 * @brief 接收数据回调
	 * 
	 * @param pConnector 连接器
	 * @param pData 数据
	 * @param nLen 长度
	 */
	typedef FX_API void OnRecvCallback(Connector* pConnector, const char* pData, unsigned int nLen);

	/**
	 * @brief 连接成功回调
	 * 
	 * @param pConnector 连接器
	 */
	typedef FX_API void OnConnectedCallback(Connector* pConnector);

	/**
	 * @brief 错误回调
	 * 
	 * @param pConnector 连接器
	 * @param refError 错误码
	 */
	typedef FX_API void OnErrorCallback(Connector* pConnector, const int& refError);

	/**
	 * @brief 关闭回调
	 * 
	 * @param pConnector 连接器
	 */
	typedef FX_API void OnCloseCallback(Connector* pConnector);

	/**
	 * @brief 创建连接器
	 * 
	 * @param onRecv 接收数据回调
	 * @param onConnected 连接成功回调
	 * @param onError 错误回调
	 * @param onClose 关闭回调
	 * 
	 * @return 连接器
	 * 
	 * @note 连接器的生命周期由调用者管理
	 */
	FX_API Connector* CreateConnector(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose);

	/**
	 * @brief 创建session生成器
	 * 
	 * @param onRecv 接收数据回调
	 * @param onConnected 连接成功回调
	 * @param onError 错误回调
	 * @param onClose 关闭回调
	 * 
	 * @note 连接器的生命周期由调用者管理
	 * @note 一定要在listen之前调用
	 */
	FX_API void CreateSessionMake(OnRecvCallback* onRecv, OnConnectedCallback* onConnected, OnErrorCallback* onError, OnCloseCallback* onClose);

	/**
	 * @brief 销毁连接器
	 * 
	 * @param pConnector 连接器
	 * 
	 * @note 连接器的生命周期由调用者管理
	 */
	FX_API void DestroyConnector(Connector* pConnector);

	/**
	 * @brief udp连接
	 * 
	 * @param pConnector 连接器
	 * @param szIp ip地址
	 * @param wPort 端口
	 */
	FX_API void UdpConnect(Connector* pConnector, const char* szIp, unsigned short wPort);

	/**
	 * @brief tcp连接
	 * 
	 * @param pConnector 连接器
	 * @param szIp ip地址
	 * @param wPort 端口
	 */
	FX_API void TcpConnect(Connector* pConnector, const char* szIp, unsigned short wPort);

	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * tcp listen
	 * @param szIp 监听的ip
	 * @param wPort 监听的端口
	 */
	FX_API void TcpListen(const char* szIp, unsigned short wPort);

	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * udp listen
	 * udp会监听 0.0.0.0 wPort 所以不支持多网卡 不同ip 相同port的监听
	 * @param szIp 监听的ip
	 * @param wPort 监听的端口
	 */
	FX_API void UdpListen(const char* szIp, unsigned short wPort);

	/**
	 * @brief 发送数据
	 * 
	 * @param pConnector 连接器
	 * @param szData 数据
	 * @param dwLen 长度
	 */
	FX_API void Send(Connector* pConnector, const char* szData, unsigned int dwLen);

	/**
	 * @brief 关闭连接
	 * 
	 * @param pConnector 连接器
	 * @note 关闭连接后，不能再发送数据
	 */
	FX_API void Close(Connector* pConnector);

	/**
	 * @brief 启动IO模块
	 * @note 启动IO模块后才能进行连接
	 */
	FX_API void StartIOModule();

	/**
	 * @brief 处理IO模块
	 * @note 处理IO模块后才能接收数据
	 * @note 需要在每帧调用一次
	 */
	FX_API void ProcessIOModule();

	/**
	 * @brief 日志回调
	 * 
	 * @param pLog 日志内容
	 * @param nLen 日志长度
	 * @note 日志回调函数用于接收io模块的日志信息
	 */
	typedef FX_API void OnLogCallback(const char* pLog, int nLen);

	/**
	 * @brief 设置日志回调
	 * 
	 * @param onLog 日志回调函数
	 * @note 设置日志回调后，io模块的日志信息会通过回调函数发送
	 */
	FX_API void SetLogCallback(OnLogCallback* onLog);

}
#endif // _CONNECTOR_H_