#ifndef __CAS_LOCK_H__
#define __CAS_LOCK_H__

namespace FXNET
{
	class CCasLock
	{
	public:
		CCasLock();
		CCasLock& Lock();
		CCasLock& Unlock();
	private:
#ifdef _WIN32
		long
#else // _WIN32
		unsigned int
#endif // _WIN32
			m_lLock;
	};

	class CLockImp
	{
	public:
		CLockImp(CCasLock& refLock);
		~CLockImp();
	private:
		CCasLock& m_refLock;
	};

};

#endif // !__CAS_LOCK_H__
