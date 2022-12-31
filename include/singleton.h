#ifndef __SINGLETON_H__
#define __SINGLETON_H__

namespace FXNET
{
	template<class T>
	class TSingleton
	{
	public:
		virtual ~TSingleton() {}
		static T* const & Instance() { return (T*)(m_pInstance); }
		static bool CreateInstance()
		{
			if (!m_pInstance)
			{
				m_pInstance = new T();
				return m_pInstance != 0;
			}
			return false;
		}

		static bool CreateInstance(T* pInstance)
		{
			if (!m_pInstance)
			{
				m_pInstance = pInstance;
				return m_pInstance != 0;
			}
			return false;
		}

		static bool DestroyInstance()
		{
			if (m_pInstance)
			{
				delete m_pInstance;
				m_pInstance = 0;
				return true;
			}
			return false;
		}

	private:
		//static volatile T* m_pInstance;
		static T* m_pInstance;
	protected:
		TSingleton() {}
	};

	template<class T>
	T* TSingleton<T>::m_pInstance = 0;
};



#endif // __SINGLETON_H__
