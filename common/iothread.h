#ifndef __IOThread_H__
#define __IOThread_H__

#include "thread.h"

#include "socket_base.h"

#include <vector>
#include <set>
#ifdef _WIN32
#else
#include <sys/epoll.h>
#endif // _WIN32

namespace FXNET
{
	class FxIoThread : public IFxThread
	{
	public:
		FxIoThread();
		virtual ~FxIoThread();

		virtual void			ThrdFunc();
		virtual void			Stop();

		void					SetStoped() { m_bStop = true; }

		bool					Start();

		bool					Init(unsigned int dwMaxSock);
		void					Uninit();

		unsigned int			GetThreadId();
		FILE*& GetFile();

		void					AddConnectSocket(IFxConnectSocket* pSock);
		void					DelConnectSocket(IFxConnectSocket* pSock);

#ifdef _WIN32
		bool					AddEvent(int hSock, IFxSocket* poSock);
#else
		bool					AddEvent(int hSock, unsigned int dwEvents, IFxSocket* poSock);
		bool					ChangeEvent(int hSock, unsigned int dwEvents, IFxSocket* poSock);
		bool					DelEvent(int hSock);
#endif // _WIN32

		// win下为完成端口 linux下为epoll
#ifdef _WIN32
		HANDLE					GetHandle();
#else
		int						GetHandle();
		int						WaitEvents(int nMilliSecond);
		epoll_event* GetEvent(int nIndex);
		void					PushDelayCloseSock(IFxSocket* poSock);
#endif // _WIN32

	private:
		void					 __DealSock();
		bool __DealData(std::ostream& refOStream);

	protected:
		IFxThreadHandler* m_poThrdHandler;

		unsigned int					m_dwMaxSock;
		bool					m_bStop;

#ifdef _WIN32
		HANDLE					m_hCompletionPort;
#else
		int						m_hEpoll;
		epoll_event* m_pEvents;
		TEventQueue<IFxSocket*>	m_oDelayCloseSockQueue;
#endif // _WIN32

		FILE* m_pFile;
		char					m_szLogPath[64];

		//存放udp的连接指针 目前只在udp中用到 每0.01秒发送一次
		std::set<CSocketBase*>	m_setConnectSockets;
		//最后一次更新的时间
		double					m_dLoatUpdateTime;
	};

};


#endif // __IOThread_H__
