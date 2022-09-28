#ifndef __MESSAGE_STREAM_H__
#define __MESSAGE_STREAM_H__

#include <string.h>

namespace FXNET
{
	template<int DATA_LEN>
	class MessageStream
	{
	public:
		enum { data_len = DATA_LEN, };

		MessageStream();

		bool PushData(char* pBuff, unsigned int dwLen);
		int PopData(char* pBuff, unsigned int dwLen);
		
		int GetUseLenth() const;
		int GetFreeLenth() const;
		char* GetData();
		char* GetFreeData()const;
	protected:
	private:
		char m_szData[DATA_LEN];
		unsigned int m_dwLen;			//已使用长度
	};

	template<int DATA_LEN>
	inline MessageStream<DATA_LEN>::MessageStream()
		: m_dwLen(0)
	{
	}

	template<int DATA_LEN>
	inline bool MessageStream<DATA_LEN>::PushData(char* pBuff, unsigned int dwLen)
	{
		if (dwLen > data_len - m_dwLen)
		{
			return false;
		}
		memcpy(m_szData + m_dwLen, pBuff, dwLen);
		m_dwLen += dwLen;
		return true;
	}

	template<int DATA_LEN>
	inline int MessageStream<DATA_LEN>::PopData(char* pBuff, unsigned int dwLen)
	{
		if (dwLen > m_dwLen)
		{
			dwLen = m_dwLen;
		}

		memcpy(pBuff, m_szData, dwLen);
		memmove(m_szData, m_szData + dwLen, m_dwLen - dwLen);
		m_dwLen -= dwLen;

		return dwLen;
	}

	template<int DATA_LEN>
	inline int MessageStream<DATA_LEN>::GetUseLenth() const
	{
		return m_dwLen;
	}

	template<int DATA_LEN>
	inline int MessageStream<DATA_LEN>::GetFreeLenth() const
	{
		return data_len - m_dwLen;
	}

	template<int DATA_LEN>
	inline char* MessageStream<DATA_LEN>::GetData()
	{
		return m_szData;
	}

	template<int DATA_LEN>
	inline char* MessageStream<DATA_LEN>::GetFreeData() const
	{
		return m_szData + m_dwLen;
	}

};

#endif	//!__MESSAGE_STREAM_H__

