#ifndef __IOThread_H__
#define __IOThread_H__

#include "thread.h"
#include "singleton.h"
#include "../include/socket_base.h"
#include "../include/message_event.h"
#include "cas_lock.h"

#include <vector>
#include <set>
#include <map>
#include <deque>
#ifdef _WIN32
#else
#include <sys/epoll.h>
#endif // _WIN32

namespace FXNET
{
	/**
	 * @brief 
	 * io线程 只用一个就够了 10000个连接问题不大
	 */
	class FxIoModule : public IFxThread, public TSingleton<FxIoModule>
	{
	public:
		FxIoModule();
		virtual ~FxIoModule();

		/**
		 * @brief 
		 * 
		 * 线程函数
		 */
		virtual void			ThrdFunc();
		/**
		 * @brief 
		 * 
		 * 处理函数 单线程 多线程都可调用
		 */
		void					DealFunction(std::ostream* pOStream);
		/**
		 * @brief 
		 * 
		 * 停止执行
		 */
		virtual void			Stop();
		void					SetStoped() { m_bStop = true; }
		bool					Start();
		unsigned int			GetThreadId();

		bool					Init(std::ostream* pOStream);
		void					Uninit();

		double					FxGetCurrentTime();
		/**
		 * @brief 
		 * 
		 * 投递消息事件 在主线程执行 放入队列 在io线程消费
		 * @param pMessageEvent 
		 */
		void					PushMessageEvent(MessageEventBase* pMessageEvent);
		/**
		 * @brief 
		 * 
		 * 交换事件 在主线程执行 事件在io线程创建 放入队列 在主线程消费
		 * @param refDeque 
		 */
		void					SwapEvent(std::deque<MessageEventBase*>& refDeque);

#ifdef _WIN32
		/**
		 * @brief Get the Handle object
		 * 
		 * win下为完成端口 linux下为epoll
		 * @return HANDLE 
		 */
		HANDLE					GetHandle();

		/**
		 * @brief 
		 * 
		 * 注册到io事件
		 * @param hSock 
		 * @param poSock 
		 * @param pOStream 
		 * @return int 
		 */
		int						RegisterIO(ISocketBase::NativeSocketType hSock, ISocketBase* poSock, std::ostream* pOStream);
#else
		/**
		 * @brief 
		 * 
		 * 注册到io事件
		 * @param hSock 
		 * @param dwEvents 注册到io的事件
		 * @param poSock 
		 * @param pOStream 
		 * @return int 
		 */
		int						RegisterIO(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
		int						ChangeEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
		int						GetHandle();
		int						WaitEvents(int nMilliSecond);
		epoll_event*			GetEvent(int nIndex);
#endif // _WIN32

		/**
		 * @brief 
		 * 
		 * 解除注册
		 * @param hSock 
		 * @param pOStream 
		 * @return int 
		 */
		int 					DeregisterIO(ISocketBase::NativeSocketType hSock, std::ostream* pOStream);

		/**
		 * @brief 
		 * 
		 * 投递io事件 在主线程调用 放入队列 在io线程消费
		 * @param pEvent 
		 * @return FxIoModule& 
		 */
		FxIoModule&				PostEvent(IOEventBase* pEvent);
	private:
		bool					DealData(std::ostream* pOStream);

	protected:
		/**
		 * @brief 
		 * 
		 * thread的句柄
		 */
		IFxThreadHandler*		m_poThrdHandler;
		bool					m_bStop;

#ifdef _WIN32
		HANDLE					m_hCompletionPort;
#else
		int						m_hEpoll;
		int						m_hEvent;
		epoll_event* m_pEvents;
#endif // _WIN32

		/**
		 * @brief 
		 * 
		 * 当前事件
		 */
		double					m_dCurrentTime;
		/**
		 * @brief 
		 * 
		 * 最后一次更新事件
		 */
		double					m_dLoatUpdateTime;

		/**
		 * @brief 
		 * 
		 * 存放连接指针 建议每0.05秒更新一次(UDP使用 其他看情况)
		 */
		std::map<ISocketBase::NativeSocketType, ISocketBase*> m_mapSockets;

		/**
		 * @brief 
		 * 
		 * 待消费消息事件
		 */
		std::deque<MessageEventBase*> m_dequeEvent;
#ifndef _WIN32
		std::vector<IOEventBase*> m_vecIOEvent;
#endif // _WIN32
		CCasLock				m_lockEventLock;
	};

};


#endif // __IOThread_H__
