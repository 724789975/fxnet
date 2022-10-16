#include "../include/iothread.h"
#include "include/error_code.h"
#include <errno.h>
#include <fcntl.h>

#ifdef _WIN32
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __FUNCSIG__
#endif
#else
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __PRETTY_FUNCTION__
#endif
#endif //!_WIN32

#ifdef _WIN32
#include <process.h>
//#include <ntifs.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#endif

#include <iostream>

namespace FXNET
{
#define MAX_EVENT_NUM 256

	FxIoModule::FxIoModule()
		: m_poThrdHandler(NULL)
		, m_bStop(false)
#ifdef _WIN32
		, m_hCompletionPort(INVALID_HANDLE_VALUE)
#else
		, m_hEpoll(-1)
		, m_pEvents(NULL)
#endif // _WIN32
		, m_dLoatUpdateTime(0.)
	{
#ifdef _WIN32
		SYSTEMTIME st;
		GetSystemTime(&st);
		m_dCurrentTime = double(time(NULL)) + double(st.wMilliseconds) / 1000.0f;
#else
		static struct timeval tv;
		gettimeofday(&tv, NULL);
		m_dCurrentTime = tv.tv_sec / 1.0 + tv.tv_usec / 1000000.0;
#endif
	}

	FxIoModule::~FxIoModule()
	{
#ifdef _WIN32
#else
		if (m_hEpoll != (int)ISocketBase::InvalidNativeHandle())
		{
			close(m_hEpoll);
			m_hEpoll = (int)ISocketBase::InvalidNativeHandle();
		}

		if (m_pEvents != NULL)
		{
			delete m_pEvents;
			m_pEvents = NULL;
		}
#endif // _WIN32
	}

	void FxIoModule::ThrdFunc()
	{
		std::ostream& refOStream = std::cout;
		refOStream << "thread id " << m_poThrdHandler->GetThreadId() << " start, "
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

		while (!m_bStop)
		{
#ifdef _WIN32
			SYSTEMTIME st;
			GetSystemTime(&st);
			m_dCurrentTime = double(time(NULL)) + double(st.wMilliseconds) / 1000.0f;
#else
			static struct timeval tv;
			gettimeofday(&tv, NULL);
			m_dCurrentTime = tv.tv_sec / 1.0 + tv.tv_usec / 1000000.0;
#endif

			std::ostream& refOStream = std::cout;
			if (!__DealData(&refOStream))
			{
				break;
			}

			__DealSock();

			if (m_dCurrentTime - m_dLoatUpdateTime >= 0.05)
			{
				m_dLoatUpdateTime = m_dCurrentTime;
				for (std::map<ISocketBase::NativeSocketType, ISocketBase*>::iterator it = m_mapSockets.begin();
					it != m_mapSockets.end();)
				{
					std::map<ISocketBase::NativeSocketType, ISocketBase*>::iterator it_tmp = it++;
					ISocketBase* pISock = it_tmp->second;
					if (int dwError = pISock->Update(m_dCurrentTime, &refOStream))
					{
						DeregisterIO(pISock->NativeSocket(), &refOStream);
						pISock->NewErrorOperation(dwError)(*pISock, 0, &refOStream);
					}
				}
			}

#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif // _WIN32
		}
		refOStream << "thread id " << m_poThrdHandler->GetThreadId() << " end\n";
	}

	void FxIoModule::Stop()
	{
		m_bStop = true;
		if (m_poThrdHandler != NULL)
		{
			m_poThrdHandler->WaitFor(3000);
			m_poThrdHandler->Release();
			m_poThrdHandler = NULL;
		}
	}

	bool FxIoModule::Start()
	{
		FxCreateThreadHandler(this, true, m_poThrdHandler);
		if (NULL == m_poThrdHandler)
		{
			return false;
		}
		return true;
	}

	unsigned int FxIoModule::GetThreadId()
	{
		if (m_poThrdHandler)
		{
			return m_poThrdHandler->GetThreadId();
		}
		return 0;
	}

	bool FxIoModule::Init(std::ostream* pOStream)
	{
#ifdef _WIN32
		// 初始化的时候 先获取下 创建完成端口 //
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		if (m_hCompletionPort == NULL)
		{
			if (pOStream)
			{
				*pOStream << "CreateIoCompletionPort error " << WSAGetLastError()
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return false;
		}
#else
		m_pEvents = new epoll_event[MAX_EVENT_NUM];
		if (NULL == m_pEvents)
		{
			*pOStream << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		m_hEpoll = epoll_create(MAX_EVENT_NUM);
		if (m_hEpoll < 0)
		{
			*pOStream << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		//m_hEvent = eventfd(0, EFD_NONBLOCK);
		//m_hEvent = eventfd(0, EFD_CLOEXEC);
		m_hEvent = eventfd(0, EFD_CLOEXEC);
		if (m_hEvent < 0)
		{
			*pOStream << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		epoll_event e;
		e.events = EPOLLIN;
		e.data.ptr = 0;

		if (epoll_ctl(m_hEpoll, EPOLL_CTL_ADD, m_hEvent, &e) < 0)
		{
			if (pOStream)
			{
				*pOStream << "epoll_ctl errno " << errno
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return false;
		}

#endif // _WIN32
		if (!Start())
		{
			*pOStream << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		return true;
	}

	void FxIoModule::Uninit()
	{
		if (!m_bStop)
		{
			Stop();
		}
#ifdef _WIN32
#else
		if (m_hEpoll != (int)ISocketBase::InvalidNativeHandle())
		{
			close(m_hEpoll);
			m_hEpoll = (int)ISocketBase::InvalidNativeHandle();
		}

		if (m_pEvents != NULL)
		{
			delete[] m_pEvents;
			m_pEvents = NULL;
		}
#endif // _WIN32
	}

	ISocketBase* FxIoModule::GetSocket(ISocketBase::NativeSocketType hSock)
	{
		std::map<ISocketBase::NativeSocketType, ISocketBase*>::iterator it = m_mapSockets.find(hSock);
		if (m_mapSockets.end() == it)
		{
			return nullptr;
		}
		return it->second;
	}

	double FxIoModule::FxGetCurrentTime()
	{
		return m_dCurrentTime;
	}

	void FxIoModule::PushMessageEvent(MessageEventBase* pMessageEvent)
	{
		CLockImp oImp(m_lockEventLock);
		m_dequeEvent.push_back(pMessageEvent);
	}

	void FxIoModule::SwapEvent(std::deque<MessageEventBase*>& refDeque)
	{
		CLockImp oImp(m_lockEventLock);
		refDeque.swap(m_dequeEvent);
	}

#ifdef _WIN32
	HANDLE FxIoModule::GetHandle()
	{
		// 创建完成端口
		return m_hCompletionPort;
	}

	int FxIoModule::RegisterIO(ISocketBase::NativeSocketType hSock, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (hSock < 0)
		{
			if (pOStream)
			{
				*pOStream << "hSock : " << hSock
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		if (NULL == GetHandle())
		{
			if (pOStream)
			{
				*pOStream << "get handle error"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_COMPLETION_PORT;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)hSock, GetHandle(), (ULONG_PTR)poSock, 0))
		{
			int dwErr = WSAGetLastError();
			if (pOStream)
			{
				*pOStream << "CreateIoCompletionPort errno " << dwErr
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return dwErr;
		}

		m_mapSockets[hSock] = poSock;
		return 0;
	}
#else
	int FxIoModule::RegisterIO(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (hSock < 0)
		{
			if (pOStream)
			{
				*pOStream << "hSock : " << hSock
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		if (m_hEpoll < 0)
		{
			if (pOStream)
			{
				*pOStream << "m_hEpoll < 0"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}
		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(m_hEpoll, EPOLL_CTL_ADD, hSock, &e) < 0)
		{
			if (pOStream)
			{
				*pOStream << "epoll_ctl errno " << errno
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return errno;
		}

		if (pOStream)
		{
			*pOStream << "hSock : " << hSock << ", event: " << dwEvents
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
		m_mapSockets[hSock] = poSock;
		return 0;
	}

	int FxIoModule::ChangeEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (m_hEpoll < 0)
		{
			if (pOStream)
			{
				*pOStream << hSock << " m_hEpoll < 0"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}

		if (hSock < 0)
		{
			if (pOStream)
			{
				*pOStream << hSock
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(m_hEpoll, EPOLL_CTL_MOD, hSock, &e) < 0)
		{
			if (pOStream)
			{
				*pOStream << "epoll_ctl errno : " << errno
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return errno;
		}

		return 0;
	}

#endif // _WIN32
	int FxIoModule::DeregisterIO(ISocketBase::NativeSocketType hSock, std::ostream* pOStream)
	{
#ifdef _WIN32
		CancelIo((ISocketBase::NativeHandleType)hSock);
#else
		if (m_hEpoll < 0)
		{
			if (pOStream)
			{
				*pOStream << "m_hEpoll < 0"
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}

		if (hSock < 0)
		{
			if (pOStream)
			{
				*pOStream << hSock
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		epoll_event e;
		if (epoll_ctl(m_hEpoll, EPOLL_CTL_DEL, hSock, &e) < 0)
		{
			if (pOStream)
			{
				*pOStream << "epoll_ctl errno : " << errno
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			}
			return errno;
		}
#endif // _WIN32
		m_mapSockets.erase(hSock);
		return 0;

	}

	FxIoModule& FxIoModule::PostEvent(IOEventBase* pEvent)
	{
#ifdef _WIN32
		PostQueuedCompletionStatus(GetHandle(), 0, 0, (OVERLAPPED*)pEvent);
#else
		//unsigned long long lPoint = (unsigned long long)pEvent;
		//std::cout << lPoint << "\n";
		unsigned long long lPoint = 1LL;

		CLockImp oImp(m_lockEventLock);
		m_vecIOEvent.push_back(pEvent);
		write(m_hEvent, &lPoint, sizeof(lPoint));
#endif //_WIN32
		return *this;
	}

#ifndef _WIN32
	int FxIoModule::GetHandle()
	{
		return m_hEpoll;
	}

	int FxIoModule::WaitEvents(int nMilliSecond)
	{
		if ((int)ISocketBase::InvalidNativeHandle() == m_hEpoll)
		{
			return 0;
		}

		int nCount = epoll_wait(m_hEpoll, m_pEvents, MAX_EVENT_NUM, nMilliSecond);
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

	epoll_event* FxIoModule::GetEvent(int nIndex)
	{
		if (0 > nIndex || (int)MAX_EVENT_NUM <= nIndex)
		{
			return NULL;
		}

		return &m_pEvents[nIndex];
	}
#endif // _WIN32


	void FxIoModule::__DealSock()
	{
	}

	bool FxIoModule::__DealData(std::ostream* pOStream)
	{
		const int dwTimeOut = 1;
#ifdef _WIN32
		void* pstPerIoData = NULL;
		ISocketBase* poSock = NULL;
		DWORD dwByteTransferred = 0;
		BOOL bRet = FALSE;

		while (bRet = GetQueuedCompletionStatus(GetHandle(), &dwByteTransferred
			, (PULONG_PTR)&poSock, (LPOVERLAPPED*)&pstPerIoData, dwTimeOut))
		{
			if (poSock)
			{
				if (int dwError = (*(IOOperationBase*)(OVERLAPPED*)(pstPerIoData))(*poSock, dwByteTransferred, pOStream))
				{
					DeregisterIO(poSock->NativeSocket(), pOStream);
					poSock->NewErrorOperation(dwError)(*poSock, dwByteTransferred, pOStream);
				}
			}
			else
			{
				(* (IOEventBase*)(OVERLAPPED*)pstPerIoData)(pOStream);
			}

			pstPerIoData = NULL;
			poSock = NULL;
			dwByteTransferred = 0;
		}

		if (bRet == FALSE)
		{
			int dwError = WSAGetLastError();
			if (WAIT_TIMEOUT == dwError)
			{
				return true;
			}
			if (NULL == pstPerIoData)
			{
				if (pOStream)
				{
					*pOStream << "GetQueuedCompletionStatus FALSE " << dwError
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				}
				return true;
			}

			if (!poSock)
			{
				if (pOStream)
				{
					*pOStream << "GetQueuedCompletionStatus FALSE " << dwError
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				}
				return true;
			}

			//TODO 删除这个op 还是执行？ 先删除好了
			//(*(IOOperationBase*)(pstPerIoData))(*poSock, dwByteTransferred, pOStream);
			//TODO 会在socketbase销毁的时候 删除 所以下面不用执行
			//delete (IOOperationBase*)(OVERLAPPED*)(pstPerIoData);
			DeregisterIO(poSock->NativeSocket(), pOStream);
			poSock->NewErrorOperation(dwError)(*poSock, dwByteTransferred, pOStream);
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
				if (pOStream)
				{
					*pOStream << "get event error "
						<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				}
				return false;
			}

			if (pOStream)
			{
				*pOStream << "event:" << pEvent->events
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}

			ISocketBase* poSock = (ISocketBase*)pEvent->data.ptr;
			if (NULL == poSock)
			{
				if (pOStream)
				{
					*pOStream << "event:" << pEvent->events
						<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
				}
				if (pEvent->events & EPOLLIN)
				{
					std::vector<IOEventBase*> vecTmp;
					unsigned long long lPoint;
					int n = read(m_hEvent, &lPoint, sizeof(lPoint));
					if (sizeof(lPoint) == n)
					{
						CLockImp oImp(m_lockEventLock);
						vecTmp.swap(m_vecIOEvent);
						//(*(IOEventBase*)((void*)lPoint))(pOStream);
					}
					for (std::vector<IOEventBase*>::iterator it = vecTmp.begin(); it != vecTmp.end(); ++it)
					{
						(* (*it))(pOStream);
					}
				}
				else
				{
					if (pOStream)
					{
						*pOStream << pEvent->events << ", " << m_hEvent << ", [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
					}
				}
				return true;
			}

			if (pOStream)
			{
				*pOStream << poSock->NativeSocket() << ", " << pEvent->events << ", " << poSock->Name()
					<< ", [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			}
			if (pEvent->events & (EPOLLERR | EPOLLHUP))
			{
				int dwError = errno;
				DeregisterIO(poSock->NativeSocket(), pOStream);
				poSock->NewErrorOperation(dwError)(*poSock, 0, pOStream);
			}
			else
			{
				if (pEvent->events & EPOLLOUT)
				{
					if (int dwError = poSock->NewWriteOperation()(*poSock, 0, pOStream))
					{
						DeregisterIO(poSock->NativeSocket(), pOStream);
						poSock->NewErrorOperation(dwError)(*poSock, 0, pOStream);
					}
				}
				if (pEvent->events & EPOLLIN)
				{
					if (int dwError = poSock->NewReadOperation()(*poSock, 0, pOStream))
					{
						DeregisterIO(poSock->NativeSocket(), pOStream);
						poSock->NewErrorOperation(dwError)(*poSock, 0, pOStream);
					}
				}
			}
		}
#endif // _WIN32

		return true;
	}

};


