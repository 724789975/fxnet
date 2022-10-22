#ifndef __IOThread_H__
#define __IOThread_H__

#include "thread.h"
#include "singleton.h"
#include "socket_base.h"
#include "cas_lock.h"
#include "message_event.h"

#include <vector>
#include <set>
#include <map>
#include <deque>
#include <mutex>
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

		virtual void			ThrdFunc();
		virtual void			Stop();
		void					SetStoped() { m_bStop = true; }
		bool					Start();
		unsigned int			GetThreadId();

		bool					Init(std::ostream* pOStream);
		void					Uninit();

		ISocketBase*			GetSocket(ISocketBase::NativeSocketType hSock);

		double					FxGetCurrentTime();
		void					PushMessageEvent(MessageEventBase* pMessageEvent);
		void					SwapEvent(std::deque<MessageEventBase*>& refDeque);

#ifdef _WIN32
		// win下为完成端口 linux下为epoll
		HANDLE					GetHandle();
		int						RegisterIO(ISocketBase::NativeSocketType hSock, ISocketBase* poSock, std::ostream* pOStream);
#else
		int						RegisterIO(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
		int						ChangeEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
		int						GetHandle();
		int						WaitEvents(int nMilliSecond);
		epoll_event*			GetEvent(int nIndex);
#endif // _WIN32
		int 					DeregisterIO(ISocketBase::NativeSocketType hSock, std::ostream* pOStream);

		FxIoModule&				PostEvent(IOEventBase* pEvent);
	private:
		void					__DealSock();
		bool					__DealData(std::ostream* pOStream);

	protected:
		IFxThreadHandler*		m_poThrdHandler;
		bool					m_bStop;

#ifdef _WIN32
		HANDLE					m_hCompletionPort;
#else
		int						m_hEpoll;
		int						m_hEvent;
		epoll_event* m_pEvents;
#endif // _WIN32

		//存放连接指针 每0.05秒更新一次
		std::set<ISocketBase*>	m_setConnectSockets;
		//最后一次更新的时间
		double					m_dCurrentTime;
		double					m_dLoatUpdateTime;

		std::map<ISocketBase::NativeSocketType, ISocketBase*> m_mapSockets;

		std::deque<MessageEventBase*> m_dequeEvent;
#ifndef _WIN32
		std::vector<IOEventBase*> m_vecIOEvent;
#endif // _WIN32
		//CCasLock				m_lockEventLock;
		std::mutex m_lockEventLock;
	};

};


#endif // __IOThread_H__
