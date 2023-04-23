#ifndef __CAS_QUEUE_H__
#define __CAS_QUEUE_H__

#include "cas_lock.h"

namespace FXNET
{
	template<typename T, unsigned int Size>
	class CCasLockQueue
	{
	public:
		enum { size = Size };
		CCasLockQueue();
		~CCasLockQueue();

		CCasLockQueue& Init();

		T* Allocate();

		void Free(T* pT);

	protected:
	private:

		struct Element
		{
			T m_oElement;
			Element* m_pNext;
			int m_dwUseCount;
		};

		Element m_oElements[size];

		Element* m_pFreeNode;

		CCasLock m_oLock;
	};

	template<typename T, unsigned int Size>
	inline CCasLockQueue<T, Size>::CCasLockQueue()
		: m_pFreeNode(NULL)
	{ }

	template<typename T, unsigned int Size>
	inline CCasLockQueue<T, Size>::~CCasLockQueue()
	{
	}

	template<typename T, unsigned int Size>
	inline CCasLockQueue<T, Size>& CCasLockQueue<T, Size>::Init()
	{
		for (unsigned int i = 0; i < Size; ++i)
		{
			this->m_oElements[i].m_pNext = this->m_oElements + i;
			this->m_oElements[i].m_dwUseCount = 0;
		}
		this->m_oElements[Size - 1].m_pNext = NULL;
		this->m_pFreeNode = this->m_oElements;

		return *this;
	}

	template<typename T, unsigned int Size>
	inline T* CCasLockQueue<T, Size>::Allocate()
	{
		if (!this->m_pFreeNode)
		{
			return NULL;
		}
		CLockImp oLockImp(m_oLock);
		if (!this->m_pFreeNode)
		{
			return NULL;
		}

		Element* pElement = this->m_pFreeNode;
		this->m_pFreeNode = this->m_pFreeNode->m_pNext;
		pElement->m_pNext = NULL;

#ifdef _DEBUG
		if (pElement->m_dwUseCount)
		{
			throw;
		}
		++pElement->m_dwUseCount;
#endif // DEBUG
		return &pElement->m_oElement;
	}

	template<typename T, unsigned int Size>
	inline void CCasLockQueue<T, Size>::Free(T* pT)
	{
		CLockImp oLockImp(m_oLock);
		Element* pElement = (Element*)pT;
#ifdef _DEBUG
		if (!pElement->m_dwUseCount)
		{
			throw;
		}
		--pElement->m_dwUseCount;
#endif // DEBUG
		pElement->m_pNext = this->m_pFreeNode;
		this->m_pFreeNode = pElement;
	}
};

#endif // !__CAS_QUEUE_H__
