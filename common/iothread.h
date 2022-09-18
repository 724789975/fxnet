#ifndef __IOThread_H__
#define __IOThread_H__

#include "thread.h"
#include "singleton.h"
#include "socket_base.h"

#include <vector>
#include <set>
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
	class FxIoThread : public IFxThread, public TSingleton<FxIoThread>
	{
	public:
		FxIoThread();
		virtual ~FxIoThread();

		virtual void			ThrdFunc();
		virtual void			Stop();

		void					SetStoped() { m_bStop = true; }

		bool					Start();

		bool					Init(std::ostream* pOStream);
		void					Uninit();

		unsigned int			GetThreadId();

#ifdef _WIN32
		bool					AddEvent(ISocketBase::NativeSocketType hSock, ISocketBase* poSock, std::ostream* pOStream);
#else
		bool					AddEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
		bool					ChangeEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream);
#endif // _WIN32
		bool					DelEvent(ISocketBase::NativeSocketType hSock, std::ostream* pOStream);

		// win下为完成端口 linux下为epoll
#ifdef _WIN32
		HANDLE					GetHandle();
#else
		int						GetHandle();
		int						WaitEvents(int nMilliSecond);
		epoll_event*			GetEvent(int nIndex);
#endif // _WIN32

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
		epoll_event* m_pEvents;
		//TEventQueue<ISocketBase*>	m_oDelayCloseSockQueue;
#endif // _WIN32

		//存放连接指针 每0.05秒更新一次
		std::set<ISocketBase*>	m_setConnectSockets;
		//最后一次更新的时间
		double					m_dLoatUpdateTime;
	};

};


#endif // __IOThread_H__
