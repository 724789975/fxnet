#ifndef __THREAD_H__
#define __THREAD_H__

#include <stdarg.h>

namespace FXNET
{
	class IFxThread
	{
	public:
		virtual ~IFxThread()
		{
		}

		virtual void ThrdFunc() = 0;

		virtual void Stop() = 0;
	};

	class IFxThreadHandler
	{
	public:
		virtual ~IFxThreadHandler()
		{
		}

		virtual void Stop(void) = 0;

		virtual bool Kill(unsigned int dwExitCode) = 0;

		virtual bool WaitFor(unsigned int dwWaitTime = 0xffffffff) = 0;

		virtual unsigned int GetThreadId(void) = 0;

		virtual IFxThread* GetThread(void) = 0;

		virtual void Release(void) = 0;

		bool IsStop(void)
		{
			return m_bIsStop;
		}

#ifdef _WIN32
		void* GetHandle(void)
		{
			return m_hHandle;
		}
	protected:
		void* m_hHandle;		// 
#endif // _WIN32

	protected:
		bool m_bIsStop;
	};

	bool FxCreateThreadHandler(IFxThread* poThread, bool bNeedWaitfor, IFxThreadHandler*& refpIFxThreadHandler);
};


#endif // !__THREAD_H__
