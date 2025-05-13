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
		unsigned long m_lLock;
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

#endif  // !__CAS_LOCK_H__
