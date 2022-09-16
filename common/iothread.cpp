#include "iothread.h"
#include "socket_base.h"
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include <iostream>

namespace FXNET
{

	FxIoThread::FxIoThread()
	{
		m_poThrdHandler = NULL;
		m_pFile = NULL;
#ifdef _WIN32
#else
		m_pEvents = NULL;
		m_hEpoll = INVALID_SOCKET;
#endif // _WIN32
		m_dwMaxSock = 0;
		m_bStop = false;

		m_dLoatUpdateTime = 0;
	}

	FxIoThread::~FxIoThread()
	{
#ifdef _WIN32
#else
		if (m_hEpoll != (int)INVALID_SOCKET)
		{
			close(m_hEpoll);
			m_hEpoll = INVALID_SOCKET;
		}

		if (m_pEvents != NULL)
		{
			delete m_pEvents;
			m_pEvents = NULL;
		}
#endif // _WIN32
	}

	bool FxIoThread::Init(unsigned int dwMaxSock)
	{
#ifdef _WIN32
		// 初始化的时候 先获取下 创建完成端口 //
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		if (m_hCompletionPort == NULL)
		{
			return false;
		}
#else
		m_dwMaxSock = dwMaxSock;
		m_pEvents = new epoll_event[dwMaxSock];
		if (NULL == m_pEvents)
		{
			return false;
		}

		m_hEpoll = epoll_create(dwMaxSock);
		if (m_hEpoll < 0)
		{
			return false;
		}
		m_oDelayCloseSockQueue.Init(2 * dwMaxSock);
#endif // _WIN32
		if (!Start())
		{
			return false;
		}

		return true;
	}

	void FxIoThread::Uninit()
	{
		if (!m_bStop)
		{
			Stop();
		}
#ifdef _WIN32
#else
		if (m_hEpoll != (int)INVALID_SOCKET)
		{
			close(m_hEpoll);
			m_hEpoll = INVALID_SOCKET;
		}

		if (m_pEvents != NULL)
		{
			delete[] m_pEvents;
			m_pEvents = NULL;
		}
#endif // _WIN32
	}



	unsigned int FxIoThread::GetThreadId()
	{
		if (m_poThrdHandler)
		{
			return m_poThrdHandler->GetThreadId();
		}
		return 0;
	}


	FILE*& FxIoThread::GetFile()
	{
		unsigned int dwTime = GetTimeHandler()->GetSecond() - GetTimeHandler()->GetSecond() % 3600;
		sprintf(m_szLogPath, "./%s_%d_%d_%p_log.txt", GetExeName(), dwTime, GetPid(), this);
		if (Access(m_szLogPath, 0) == -1)
		{
			if (m_pFile)
			{
				fclose(m_pFile);
				m_pFile = NULL;
			}
			m_pFile = fopen(m_szLogPath, "a+");
			setvbuf(m_pFile, (char*)NULL, _IOLBF, 1024);
		}
		if (m_pFile == NULL)
		{
			m_pFile = fopen(m_szLogPath, "a+");
			setvbuf(m_pFile, (char*)NULL, _IOLBF, 1024);
		}
		return m_pFile;
	}

	void FxIoThread::AddConnectSocket(IFxConnectSocket* pSock)
	{
		m_setConnectSockets.insert(pSock);
	}

	void FxIoThread::DelConnectSocket(IFxConnectSocket* pSock)
	{
		m_setConnectSockets.erase(pSock);
	}

	void FxIoThread::__DealSock()
	{
	}

#ifdef _WIN32
	bool FxIoThread::AddEvent(int hSock, IFxSocket* poSock)
	{
		if (hSock < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "hSock : %d, socket id : %d", hSock, poSock->GetSockId());
			return false;
		}

		if (NULL == GetHandle())
		{
			ThreadLog(LogLv_Error, GetFile(), "%s", "GetHandle failed");
			return false;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)hSock, GetHandle(), (ULONG_PTR)poSock, 0))
		{
			int dwErr = WSAGetLastError();
			ThreadLog(LogLv_Error, GetFile(), "CreateIoCompletionPort errno %d", dwErr);
			return false;
		}

		return true;
	}
#else
	bool FxIoThread::AddEvent(int hSock, unsigned int dwEvents, IFxSocket* poSock)
	{
		if (hSock < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "hSock : %d, socket id : %d", hSock, poSock->GetSockId());
			return false;
		}

		if (m_hEpoll < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "%s", "m_hEpoll < 0");
			return false;
		}
		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(m_hEpoll, EPOLL_CTL_ADD, hSock, &e) < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "epoll_ctl errno %d", errno);
			return false;
		}

		return true;
	}

	bool FxIoThread::ChangeEvent(int hSock, unsigned int dwEvents, IFxSocket* poSock)
	{
		if (m_hEpoll < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "%s", "m_hEpoll < 0");
			return false;
		}

		if (hSock < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "socket : %d", hSock);
			return false;
		}

		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(m_hEpoll, EPOLL_CTL_MOD, hSock, &e) < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "epoll_ctl errno : %d", errno);
			return false;
		}

		return true;
	}

	bool FxIoThread::DelEvent(int hSock)
	{
		if (m_hEpoll < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "%s", "m_hEpoll < 0");
			return false;
		}

		if (hSock < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "socket : %d", hSock);
			return false;
		}

		epoll_event e;
		if (epoll_ctl(m_hEpoll, EPOLL_CTL_DEL, hSock, &e) < 0)
		{
			ThreadLog(LogLv_Error, GetFile(), "epoll_ctl errno : %d", errno);
			return false;
		}
		return true;
	}

#endif // _WIN32

	void FxIoThread::ThrdFunc()
	{
		std::ostream& refOStream = std::cout;
		refOStream << "thread id " << m_poThrdHandler->GetThreadId() << " start, "
			<< "[" << __FILE__ << ", " << __LINE__ << ", " << __FUNCTION__ <<"]";

		double dTime = 0;
		
		while (!m_bStop)
		{
			if (!__DealData(refOStream))
			{
				break;
			}

			__DealSock();

			if (GetTimeHandler()->GetMilliSecond() - m_dLoatUpdateTime >= 0.01)
			{
				m_dLoatUpdateTime = GetTimeHandler()->GetMilliSecond();
				for (std::set<CSocketBase*>::iterator it = m_setConnectSockets.begin();
					it != m_setConnectSockets.end();)
				{
					if ((*it)->IsConnected())
					{
						(*it)->Update(dTime, refOStream);
						++it;
					}
					else
					{
						m_setConnectSockets.erase(it++);
					}
				}
			}

			FxSleep(1);
		}
		ThreadLog(LogLv_Info, GetFile(), "thread id %d end", m_poThrdHandler->GetThreadId());
	}

	bool FxIoThread::__DealData(std::ostream& refOStream)
{
		const int dwTimeOut = 1;
#ifdef _WIN32
		void* pstPerIoData = NULL;
		CSocketBase* poSock = NULL;
		BOOL bRet = false;
		DWORD dwByteTransferred = 0;

		//while (true)
		{
			poSock = NULL;
			pstPerIoData = NULL;
			dwByteTransferred = 0;

			bRet = GetQueuedCompletionStatus(
				GetHandle(),
				&dwByteTransferred,
				(PULONG_PTR)&poSock,
				(LPOVERLAPPED*)&pstPerIoData,
				dwTimeOut);


			(*(IOOperationBase*)(pstPerIoData))(*poSock, refOStream);
		}
#else

		int nCount = WaitEvents(dwTimeOut);
		if (nCount < 0)
		{
			return false;
		}

		for (int i = 0; i < nCount; i++)
		{
			epoll_event* pEvent = GetEvent(i);
			if (NULL == pEvent)
			{
				return false;
			}

			CSocketBase* poSock = (CSocketBase*)pEvent->data.ptr;
			if (NULL == poSock)
			{
				return false;
			}
			if ()
			{
			}

		}
#endif // _WIN32

		return true;
	}

	void FxIoThread::Stop()
	{
		m_bStop = true;
		if (m_poThrdHandler != NULL)
		{
			m_poThrdHandler->WaitFor(3000);
			m_poThrdHandler->Release();
			m_poThrdHandler = NULL;
		}
	}

	bool FxIoThread::Start()
	{
		FxCreateThreadHandler(this, true, m_poThrdHandler);
		if (NULL == m_poThrdHandler)
		{
			return false;
		}
		return true;
	}

#ifdef _WIN32
	HANDLE FxIoThread::GetHandle()
	{
		// 创建完成端口
		return m_hCompletionPort;
	}
#else
	int FxIoThread::GetHandle()
	{
		return m_hEpoll;
	}

	int FxIoThread::WaitEvents(int nMilliSecond)
	{
		if ((int)INVALID_SOCKET == m_hEpoll)
		{
			return 0;
		}

		int nCount = epoll_wait(m_hEpoll, m_pEvents, m_dwMaxSock, nMilliSecond);
		if (nCount < 0)
		{
			if (errno != EINTR)
			{
				return nCount;
			}
			else
			{
				return 0;
			}
		}
		return nCount;
	}

	epoll_event* FxIoThread::GetEvent(int nIndex)
	{
		if (0 > nIndex || (int)m_dwMaxSock <= nIndex)
		{
			return NULL;
		}

		return &m_pEvents[nIndex];
	}


	void FxIoThread::PushDelayCloseSock(IFxSocket* poSock)
	{
		while (!m_oDelayCloseSockQueue.PushBack(poSock))
		{
			FxSleep(10);
		}
	}

#endif // _WIN32
};


