#include "../include/thread.h"

#ifdef _WIN32
#include <Windows.h>
#include <process.h>

#include <psapi.h>
#include <cstddef>
#include <dbghelp.h>
#else
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <netinet/in.h>
#include <execinfo.h>
#endif // _WIN32

namespace FXNET
{
	class FxThreadHandler : public IFxThreadHandler
	{
	public:
		FxThreadHandler(IFxThread* pThread, bool bNeedWaitfor)
		{
			m_dwThreadId = 0;
			m_bIsStop = true;
			m_bNeedWaitfor = bNeedWaitfor;
			m_pThread = pThread;
#ifdef _WIN32
			m_hHandle = INVALID_HANDLE_VALUE;
#endif // _WIN32
		}

		virtual ~FxThreadHandler()
		{
#ifdef _WIN32
			if (m_hHandle != INVALID_HANDLE_VALUE)
#endif // _WIN32
			{
				unsigned int dwErrCode = 0;
				Kill(dwErrCode);
			}
		}

	public:
		inline virtual void Stop(void)
		{
			if (NULL != m_pThread)
			{
				m_pThread->Stop();
			}
		}

		inline virtual bool Kill(unsigned int dwExitCode)
		{
#ifdef _WIN32
			if (m_hHandle == INVALID_HANDLE_VALUE)
			{
				return false;
			}

			if (TerminateThread(m_hHandle, dwExitCode))
			{
				CloseHandle(m_hHandle);
				m_hHandle = INVALID_HANDLE_VALUE;
				return true;
			}
			return false;
#else
			pthread_cancel(m_dwThreadId);
			return false;
#endif // _WIN32
		}

		inline virtual bool WaitFor(unsigned int dwWaitTime = 0xffffffff)
		{
			if (!m_bNeedWaitfor)
			{
				return false;
			}
#ifdef _WIN32
			if (INVALID_HANDLE_VALUE == m_hHandle)
			{
				return false;
			}
			DWORD dwRet = WaitForSingleObject(m_hHandle, dwWaitTime);
			CloseHandle(m_hHandle);
			m_hHandle = INVALID_HANDLE_VALUE;
			m_bIsStop = true;

			if (WAIT_OBJECT_0 == dwRet)
			{
				return true;
			}
#else
			pthread_join(m_dwThreadId, NULL);
			return true;
#endif // _WIN32
			return false;
		}

		inline virtual void Release(void)
		{
			delete this;
		}
		inline virtual unsigned int GetThreadId(void)
		{
			return m_dwThreadId;
		}
		inline virtual IFxThread* GetThread(void)
		{
			return m_pThread;
		}

		inline bool Start()
		{
#ifdef _WIN32
			m_hHandle = (HANDLE)_beginthreadex(0, 0, __StaticThreadFunc, this, 0, &m_dwThreadId);
			if (m_hHandle == NULL)
			{
				return false;
			}
#else
			if (0 != pthread_create(&m_dwThreadId, NULL, (void*
				(*)(void*))__StaticThreadFunc, this))
			{
				return false;
			}
#endif // _WIN32
			return true;
		}

	private:
		static unsigned int
#ifdef _WIN32
			__stdcall
#endif // _WIN32
			__StaticThreadFunc(void* arg)
		{
			FxThreadHandler* pThreadCtrl = (FxThreadHandler*)arg;
			pThreadCtrl->m_bIsStop = false;

#ifdef _WIN32
#else
			pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, 0);
			pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, 0);

			sigset_t new_set, old_set;
			sigemptyset(&new_set);
			sigemptyset(&old_set);
			sigaddset(&new_set, SIGHUP);
			sigaddset(&new_set, SIGINT);
			sigaddset(&new_set, SIGQUIT);
			sigaddset(&new_set, SIGTERM);
			sigaddset(&new_set, SIGUSR1);
			sigaddset(&new_set, SIGUSR2);
			sigaddset(&new_set, SIGPIPE);
			pthread_sigmask(SIG_BLOCK, &new_set, &old_set);

			if (false == pThreadCtrl->m_bNeedWaitfor)
			{
				pthread_detach(pthread_self());
			}
#endif // _WIN32
			pThreadCtrl->m_pThread->ThrdFunc();

#ifdef _WIN32
			if (!pThreadCtrl->m_bNeedWaitfor)
			{
				CloseHandle(pThreadCtrl->m_hHandle);
				pThreadCtrl->m_hHandle = INVALID_HANDLE_VALUE;
				pThreadCtrl->m_bIsStop = true;
			}
#endif // _WIN32
			return 0;
		}

	protected:

		bool m_bNeedWaitfor;
#ifdef _WIN32
		unsigned int m_dwThreadId;
#else
		pthread_t m_dwThreadId;
#endif // _WIN32
		IFxThread* m_pThread;
	};

	bool FxCreateThreadHandler(IFxThread* poThread, bool bNeedWaitfor, IFxThreadHandler*& refpIFxThreadHandler)
	{
		FxThreadHandler* pThreadCtrl = new FxThreadHandler(poThread, bNeedWaitfor);
		refpIFxThreadHandler = pThreadCtrl;
		if (NULL == pThreadCtrl)
		{
			return false;
		}

		if (false == pThreadCtrl->Start())
		{
			delete pThreadCtrl;
			refpIFxThreadHandler = NULL;
			return false;
		}
		return true;
	}
};


