#include "../include/cas_lock.h"

#ifdef _WIN32
#include <windows.h>
#endif  //_WIN32

namespace FXNET
{
#ifndef _WIN32
	static inline unsigned long atomic_cas_uint32(unsigned long* p, unsigned long dwNew, unsigned long dwOld)
	{
		unsigned long dwPrev;

		__asm volatile(
			"lock cmpxchgq %2, %1"
			:   "=a"(dwPrev), "+m"(*(volatile long*)(p))
			:   "r"(dwNew),   "0"(dwOld)
			:   "memory");
		return dwPrev;
	}
#endif  //_WIN32

	CCasLock::CCasLock()
		:   m_lLock(0)
	{ }

	// _WIN32
	CCasLock& CCasLock::Lock()
	{
		for (;;)
		{
#ifdef _WIN32
			volatile long& lLock = reinterpret_cast<volatile long&>(this->m_lLock);
			if (0 == InterlockedCompareExchange(&lLock, 1, 0)) break;
#else  // _WIN32
			// if (0 == atomic_cas_uint32(&m_lLock, 1, 0)) break;
			if (__sync_bool_compare_and_swap(&this->m_lLock, 0, 1)) break;
#endif  // _WIN32
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
#else  // _WIN32
			// if (1 == atomic_cas_uint32(&m_lLock, 0, 1)) break;
			if (__sync_bool_compare_and_swap(&this->m_lLock, 1, 0)) break;
#endif  // _WIN32
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

