#include "../include/cas_lock.h"

#ifdef _WIN32
#include <windows.h>
#endif	//_WIN32

namespace FXNET
{
#ifndef _WIN32
	static bool atomic_cas_uint32(unsigned int* p, unsigned int c, unsigned int s)
	{
		unsigned char btSuccess;

		__asm volatile (
		"lock; cmpxchgl %4, %0;"
			"sete %1;"
			: "=m" (*p), "=a" (btSuccess) /* Outputs. */
			: "m" (*p), "a" (c), "r" (s) /* Inputs. */
			: "memory"
			);

		return (!(bool)btSuccess);
	}
#endif //_WIN32

	CCasLock::CCasLock()
		: m_lLock(0)
	{ }

	// _WIN32
	CCasLock& CCasLock::Lock()
	{
		for (;;)
		{
#ifdef _WIN32
			volatile long& lLock = reinterpret_cast<volatile long&>(this->m_lLock);
			if (0 == InterlockedCompareExchange(&lLock, 1, 0)) break;
#else // _WIN32
			//if (atomic_cas_uint32(&m_lLock, 1, 0)) break;
			if (__sync_bool_compare_and_swap(&this->m_lLock, 0, 1)) break;
#endif // _WIN32
		}
		return *this;
	}
	CCasLock& CCasLock::Unlock()
	{
		for (;;)
		{
#ifdef _WIN32
			volatile long& lLock = reinterpret_cast<volatile long&>(this->m_lLock);
			if (1 == InterlockedCompareExchange(&lLock, 0, 1)) break;
#else // _WIN32
			//if (atomic_cas_uint32(&m_lLock, 0, 1)) break;
			if (__sync_bool_compare_and_swap(&this->m_lLock, 1, 0)) break;
#endif // _WIN32
		}

		return *this;
	}

	CLockImp::CLockImp(CCasLock& refLock)
		: m_refLock(refLock)
	{
		this->m_refLock.Lock();
	}
	CLockImp::~CLockImp()
	{
		this->m_refLock.Unlock();
	}
}

