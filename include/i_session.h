#ifndef __ISESSION_H__
#define __ISESSION_H__

#include "error_code.h"
#include "socket_base.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

#include <string>

class INetWorkStream;
class MessageEventBase;
class MessageRecvEventBase;

namespace FXNET
{
	class CNetStreamPackage;
	class CConnectorSocket;
	/**
	 * @brief 
	 * 
	 * session
	 * 创建早于关联套接字
	 * 销毁时与套接字解除绑定 并把socket销毁事件投递到io执行
	 */
	class ISession
	{
	public:
#ifdef _WIN32
		typedef SOCKET NativeSocketType;
#else //_WIN32
		typedef int NativeSocketType;
#endif //_WIN32
		
		ISession() : m_opSock(0) {}
		virtual ~ISession() {}
		void SetSock(CConnectorSocket* opSock) { m_opSock = opSock; }
		CConnectorSocket* GetSocket() { return m_opSock; }

		/**
		 * @brief 
		 * 
		 * 将要发送的数据投递到io线程 然后放入缓存中
		 * @param szData 
		 * @param dwLen 
		 * @return ISession& 返回自身
		 */
		virtual ISession& Send(const char* szData, unsigned int dwLen, std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * 当接收到数据时的处理
		 * @param szData 
		 * @param dwLen 
		 * @return ISession& 
		 */
		virtual ISession& OnRecv(CNetStreamPackage& refPackage, std::ostream* pOStream) = 0;

		/**
		 * @brief 
		 * 
		 * 当连接建立时
		 * @param pOStream 
		 */
		virtual void OnConnected(std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * 当错误发生时
		 * @param dwError 错误码
		 * @param pOStream 
		 */
		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * 当连接关闭时
		 * @param pOStream 
		 */
		virtual void OnClose(std::ostream* pOStream) = 0;

		/**
		 * @brief Get the Send Buff object
		 * 
		 * 获取发送缓冲
		 * @return INetWorkStream& 
		 */
		virtual INetWorkStream& GetSendBuff() = 0;
		/**
		 * @brief Get the Recv Buff object
		 * 
		 * 获取接收缓冲
		 * @return INetWorkStream& 
		 */
		virtual INetWorkStream& GetRecvBuff() = 0;

		/**
		 * @brief 
		 * 
		 * 创建接收事件 在io线程创建 在主线程执行
		 * @param refData 
		 * @return MessageEventBase* 
		 */
		virtual MessageRecvEventBase* NewRecvMessageEvent() = 0;
		/**
		 * @brief 
		 * 
		 * 创建连接完成事件 在io线程创建 在主线程执行
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewConnectedEvent() = 0;
		/**
		 * @brief 
		 * 
		 * 创建错误事件 在io线程创建 在主线程执行
		 * @param dwError 错误码
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewErrorEvent(const ErrorCode& refError) = 0;
		/**
		 * @brief 
		 * 
		 * 创建连接关闭事件 在io线程创建 在主线程执行
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewCloseEvent() = 0;

		virtual MessageEventBase* NewOnSendEvent(int dwLen) = 0;
	protected:
		CConnectorSocket* m_opSock;
	private:
	};

	class SessionMaker
	{
	public:
		virtual ~SessionMaker() {}

		virtual ISession* operator()() = 0;
	};


}

#endif //!__ISESSION_H__
