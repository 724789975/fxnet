#include "../include/iothread.h"
#include "../include/error_code.h"
#include "../include/log_utility.h"
#include "../utility/time_utility.h"
#include <errno.h>
#include <fcntl.h>


#ifdef _WIN32
#include <process.h>
#include <time.h>
//#include <ntifs.h>
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/eventfd.h>
#endif

#include <iostream>
#include <sstream>

namespace FXNET
{
#define MAX_EVENT_NUM 256

	double _FxGetCurrentTime()
	{
		return FxIoModule::Instance()->FxGetCurrentTime();
	}

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
// #ifdef _WIN32
// 		SYSTEMTIME st;
// 		GetSystemTime(&st);
// 		this->m_dCurrentTime = double(time(NULL)) + double(st.wMilliseconds) / 1000.0f;
// #else
// 		static struct timeval tv;
// 		gettimeofday(&tv, NULL);
// 		this->m_dCurrentTime = tv.tv_sec / 1.0 + tv.tv_usec / 1000000.0;
// #endif
	}

	FxIoModule::~FxIoModule()
	{
#ifdef _WIN32
#else
		if (this->m_hEpoll != (int)ISocketBase::InvalidNativeHandle())
		{
			close(m_hEpoll);
			this->m_hEpoll = (int)ISocketBase::InvalidNativeHandle();
		}

		if (this->m_pEvents != NULL)
		{
			delete this->m_pEvents;
			this->m_pEvents = NULL;
		}
#endif // _WIN32
	}

	void FxIoModule::ThrdFunc()
	{
		std::stringstream strstream;
		strstream.flags(std::cout.fixed);
		strstream << "thread id " << this->m_poThrdHandler->GetThreadId() << " start, "
			<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";

		while (!m_bStop)
		{
			this->DealFunction(&strstream);

			LogModule::Instance()->PushLog(strstream);
		}
		strstream << "thread id " << this->m_poThrdHandler->GetThreadId() << " end\n";

		LogModule::Instance()->PushLog(strstream);
	}

	void FxIoModule::DealFunction(std::ostream* pOStream)
	{
// #ifdef _WIN32
// 		SYSTEMTIME st;
// 		GetSystemTime(&st);
// 		this->m_dCurrentTime = double(time(NULL)) + double(st.wMilliseconds) / 1000.0f;
// #else
// 		static struct timeval tv;
// 		gettimeofday(&tv, NULL);
// 		this->m_dCurrentTime = tv.tv_sec / 1.0 + tv.tv_usec / 1000000.0;
// #endif

		this->m_dCurrentTime = UTILITY::TimeUtility::GetTimeUS() / 1000000.;

		//pOStream = NULL;

		//LOG(pOStream, ELOG_LEVEL_INFO) << m_dCurrentTime << "\n";

		if (this->m_dCurrentTime - this->m_dLoatUpdateTime >= 0.05)
		{
			this->m_dLoatUpdateTime = this->m_dCurrentTime;
			for (std::map<ISocketBase::NativeSocketType, ISocketBase*>::iterator it = this->m_mapSockets.begin();
				it != this->m_mapSockets.end();)
			{
				std::map<ISocketBase::NativeSocketType, ISocketBase*>::iterator it_tmp = it++;
				ISocketBase* pISock = it_tmp->second;
				if (ErrorCode oError = pISock->Update(this->m_dCurrentTime, pOStream))
				{
					this->DeregisterIO(pISock->NativeSocket(), pOStream);
					pISock->NewErrorOperation(oError)(*pISock, 0, pOStream);
				}
			}
		}
		if (!this->DealData(pOStream))
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "deal data error";
		}
	}

	void FxIoModule::Stop()
	{
		this->m_bStop = true;
		if (this->m_poThrdHandler != NULL)
		{
			this->m_poThrdHandler->WaitFor(3000);
			this->m_poThrdHandler->Release();
			this->m_poThrdHandler = NULL;
		}
	}

	bool FxIoModule::Start()
	{
#ifdef __SINGLE_THREAD__
#else
		FxCreateThreadHandler(this, true, this->m_poThrdHandler);
		if (NULL == this->m_poThrdHandler)
		{
			return false;
		}
#endif	// !__SINGLE_THREAD__
		return true;
	}

	unsigned int FxIoModule::GetThreadId()
	{
		if (this->m_poThrdHandler)
		{
			return this->m_poThrdHandler->GetThreadId();
		}
		return 0;
	}

	bool FxIoModule::Init(std::ostream* pOStream)
	{
		SetTimeFunc(_FxGetCurrentTime);

#ifdef _WIN32
		// 初始化的时候 先获取下 创建完成端口 //
		WSADATA wsaData;
		WSAStartup(MAKEWORD(2, 2), &wsaData);
		this->m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);

		if (this->m_hCompletionPort == NULL)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "CreateIoCompletionPort error " << WSAGetLastError()
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}
#else
		this->m_pEvents = new epoll_event[MAX_EVENT_NUM];
		if (NULL == this->m_pEvents)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		this->m_hEpoll = epoll_create(MAX_EVENT_NUM);
		if (this->m_hEpoll < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		//m_hEvent = eventfd(0, EFD_NONBLOCK);
		//m_hEvent = eventfd(0, EFD_CLOEXEC);
		this->m_hEvent = eventfd(0, EFD_CLOEXEC);
		if (this->m_hEvent < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		epoll_event e;
		e.events = EPOLLIN;
		e.data.ptr = 0;

		if (epoll_ctl(this->m_hEpoll, EPOLL_CTL_ADD, this->m_hEvent, &e) < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "epoll_ctl errno " << errno
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

#endif // _WIN32
		if (!this->Start())
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "start error "
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return false;
		}

		return true;
	}

	void FxIoModule::Uninit()
	{
		if (!this->m_bStop)
		{
			this->Stop();
		}
#ifdef _WIN32
#else
		if (this->m_hEpoll != (int)ISocketBase::InvalidNativeHandle())
		{
			close(this->m_hEpoll);
			this->m_hEpoll = (int)ISocketBase::InvalidNativeHandle();
		}

		if (this->m_pEvents != NULL)
		{
			delete[] this->m_pEvents;
			this->m_pEvents = NULL;
		}
#endif // _WIN32
	}

	double FxIoModule::FxGetCurrentTime()
	{
		return this->m_dCurrentTime;
	}

	void FxIoModule::PushMessageEvent(MessageEventBase* pMessageEvent)
	{
		CLockImp oImp(this->m_lockEventLock);
		this->m_dequeEvent.push_back(pMessageEvent);
	}

	void FxIoModule::SwapEvent(std::deque<MessageEventBase*>& refDeque)
	{
		CLockImp oImp(this->m_lockEventLock);
		refDeque.swap(this->m_dequeEvent);
	}

#ifdef _WIN32
	HANDLE FxIoModule::GetHandle()
	{
		// 创建完成端口
		return this->m_hCompletionPort;
	}

	int FxIoModule::RegisterIO(ISocketBase::NativeSocketType hSock, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (hSock < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "hSock : " << hSock
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		if (NULL == GetHandle())
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "get handle error"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_COMPLETION_PORT;
		}

		if (NULL == CreateIoCompletionPort((HANDLE)hSock, GetHandle(), (ULONG_PTR)poSock, 0))
		{
			int dwErr = WSAGetLastError();
			LOG(pOStream, ELOG_LEVEL_ERROR) << "CreateIoCompletionPort errno " << dwErr
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return dwErr;
		}

		this->m_mapSockets[hSock] = poSock;
		return 0;
	}
#else
	int FxIoModule::RegisterIO(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (hSock < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "hSock : " << hSock
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		if (this->m_hEpoll < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "m_hEpoll < 0"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}
		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(this->m_hEpoll, EPOLL_CTL_ADD, hSock, &e) < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "epoll_ctl errno " << errno
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return errno;
		}

		LOG(pOStream, ELOG_LEVEL_DEBUG) << "hSock : " << hSock << ", event: " << dwEvents
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		this->m_mapSockets[hSock] = poSock;
		return 0;
	}

	int FxIoModule::ChangeEvent(ISocketBase::NativeSocketType hSock, unsigned int dwEvents, ISocketBase* poSock, std::ostream* pOStream)
	{
		if (this->m_hEpoll < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock << " m_hEpoll < 0"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}

		if (hSock < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		epoll_event e;
		e.events = dwEvents;
		e.data.ptr = poSock;

		if (epoll_ctl(this->m_hEpoll, EPOLL_CTL_MOD, hSock, &e) < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "epoll_ctl errno : " << errno
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return errno;
		}

		return 0;
	}

#endif // _WIN32
	int FxIoModule::DeregisterIO(ISocketBase::NativeSocketType hSock, std::ostream* pOStream)
	{
		LOG(pOStream, ELOG_LEVEL_DEBUG2) << "DeregisterIO : " << hSock
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
#ifdef _WIN32
		CancelIoEx((ISocketBase::NativeHandleType)hSock, NULL);
		//CancelIo((ISocketBase::NativeHandleType)hSock);
#else
		if (this->m_hEpoll < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "m_hEpoll < 0"
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_EPOLL_HANDLE;
		}

		if (hSock < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << hSock
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return CODE_ERROR_NET_ERROR_SOCKET;
		}

		epoll_event e;
		if (epoll_ctl(this->m_hEpoll, EPOLL_CTL_DEL, hSock, &e) < 0)
		{
			LOG(pOStream, ELOG_LEVEL_ERROR) << "epoll_ctl errno : " << errno
				<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
			return errno;
		}
#endif // _WIN32
		this->m_mapSockets.erase(hSock);
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

		CLockImp oImp(this->m_lockEventLock);
		this->m_vecIOEvent.push_back(pEvent);
		write(this->m_hEvent, &lPoint, sizeof(lPoint));
#endif //_WIN32
		return *this;
	}

#ifndef _WIN32
	int FxIoModule::GetHandle()
	{
		return this->m_hEpoll;
	}

	int FxIoModule::WaitEvents(int nMilliSecond)
	{
		if ((int)ISocketBase::InvalidNativeHandle() == m_hEpoll)
		{
			return 0;
		}

		int nCount = epoll_wait(this->m_hEpoll, this->m_pEvents, MAX_EVENT_NUM, nMilliSecond);
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

		return &this->m_pEvents[nIndex];
	}
#endif // _WIN32

	bool FxIoModule::DealData(std::ostream* pOStream)
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
				IOOperationBase* pOp = (IOOperationBase*)(OVERLAPPED*)(pstPerIoData);
				if (ErrorCode oError = (*pOp)(*poSock, dwByteTransferred, pOStream))
				{
					this->DeregisterIO(poSock->NativeSocket(), pOStream);
					poSock->NewErrorOperation(oError)(*poSock, dwByteTransferred, pOStream);
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
			if (ERROR_OPERATION_ABORTED == dwError)
			{
				return true;
			}
			if (NULL == pstPerIoData)
			{
				LOG(pOStream, ELOG_LEVEL_INFO) << "GetQueuedCompletionStatus FALSE " << dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				return true;
			}
			
			if (!poSock)
			{
				LOG(pOStream, ELOG_LEVEL_INFO) << "GetQueuedCompletionStatus FALSE " << dwError
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				return true;
			}

			LOG(pOStream, ELOG_LEVEL_INFO) << poSock->NativeSocket() << " GetQueuedCompletionStatus FALSE " << dwError
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			this->DeregisterIO(poSock->NativeSocket(), pOStream);
			poSock->NewErrorOperation(ErrorCode(dwError, __FILE__ ":" __LINE2STR__(__LINE__)))(*poSock, dwByteTransferred, pOStream);
		}
#else

		int nCount = this->WaitEvents(dwTimeOut);
		if (nCount < 0)
		{
			return false;
		}

		for (int i = 0; i < nCount; i++)
		{
			epoll_event* pEvent = this->GetEvent(i);
			if (NULL == pEvent)
			{
				LOG(pOStream, ELOG_LEVEL_ERROR) << "get event error "
					<< "[" << __FILE__ << ":" << __LINE__ <<", " << __FUNCTION_DETAIL__ << "]\n";
				return false;
			}

			LOG(pOStream, ELOG_LEVEL_DEBUG2) << "event:" << pEvent->events
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

			ISocketBase* poSock = (ISocketBase*)pEvent->data.ptr;
			if (NULL == poSock)
			{
				LOG(pOStream, ELOG_LEVEL_DEBUG2) << "event:" << pEvent->events
					<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
				if (pEvent->events & EPOLLIN)
				{
					std::vector<IOEventBase*> vecTmp;
					unsigned long long lPoint;
					int n = read(this->m_hEvent, &lPoint, sizeof(lPoint));
					if (sizeof(lPoint) == n)
					{
						CLockImp oImp(this->m_lockEventLock);
						vecTmp.swap(this->m_vecIOEvent);
						//(*(IOEventBase*)((void*)lPoint))(pOStream);
					}
					for (std::vector<IOEventBase*>::iterator it = vecTmp.begin(); it != vecTmp.end(); ++it)
					{
						(* (*it))(pOStream);
					}
				}
				else
				{
					LOG(pOStream, ELOG_LEVEL_ERROR) << pEvent->events << ", " << m_hEvent
						<<"[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
				}
				continue;
			}

			// LOG(pOStream, ELOG_LEVEL_INFO) << poSock->NativeSocket() << ", " << pEvent->events << ", " << poSock->Name()
			// 	<< ", [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			if (pEvent->events & (EPOLLOUT | EPOLLERR | EPOLLHUP))
			{
				if (ErrorCode oError = poSock->NewWriteOperation()(*poSock, 0, pOStream))
				{
					this->DeregisterIO(poSock->NativeSocket(), pOStream);
					poSock->NewErrorOperation(oError)(*poSock, 0, pOStream);
					continue;
				}
			}
			if (pEvent->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
			{
				if (ErrorCode oError = poSock->NewReadOperation()(*poSock, 0, pOStream))
				{
					this->DeregisterIO(poSock->NativeSocket(), pOStream);
					poSock->NewErrorOperation(oError)(*poSock, 0, pOStream);
					continue;
				}
			}
		}
#endif // _WIN32

		return true;
	}

};


