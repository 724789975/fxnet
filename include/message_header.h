#ifndef __MESSAGE_HEADER_H__
#define __MESSAGE_HEADER_H__

#include "netstream.h"
#include "message_event.h"

#include <string>

namespace FXNET
{
	class MessageParseBase
	{
	public:
		virtual							~MessageParseBase() {}

		MessageParseBase&				Init(char* pBuff, unsigned int dwLen);

		virtual bool					CheckRecvMessage();
		virtual unsigned short			GetMessageLen() = 0;
		virtual MessageEventBase	*	ParseMessage() = 0;
	protected:
		char* m_pBuff;
		unsigned int m_dwLen;
	private:
		virtual unsigned int			GetHeaderLength() = 0;		// 消息头长度
	};

	inline MessageParseBase& MessageParseBase::Init(char* pBuff, unsigned int dwLen)
	{
		m_pBuff = pBuff;
		m_dwLen = dwLen;
		return *this;
	}

	inline bool MessageParseBase::CheckRecvMessage()
	{
		return m_dwLen && m_dwLen >= GetMessageLen();
	}

	//结构为 {unsigned short wLen; char szMagic[4]; int dwProtoId;}
	class BinaryMessageParse : public MessageParseBase
	{
	public:

		class MessageOperator : public MessageEventBase
		{
		public:
			virtual void operator ()(std::ostream* pOStream) {}
			int dwProtoId;
			std::string szData;
		protected:
		private:
		};

		virtual unsigned short			GetMessageLen();
		virtual MessageEventBase	 *	ParseMessage();
	protected:
	private:
		virtual unsigned int			GetHeaderLength() { return 10; }

		static const unsigned int s_dwMagic = 'T' << 24 | 'E' << 16 | 'S' << 8 | 'T';
	};

	inline unsigned short BinaryMessageParse::GetMessageLen()
	{
		if (m_dwLen >= GetHeaderLength())
		{
			CNetStream oNetStream(m_pBuff, m_dwLen);
			unsigned short wLen = 0;
			oNetStream.ReadShort(wLen);

			return wLen;
		}
		return 0;
	}

	inline MessageEventBase* BinaryMessageParse::ParseMessage()
	{
		CNetStream oNetStream(m_pBuff, m_dwLen);
		unsigned short wLen = 0;
		oNetStream.ReadShort(wLen);

		unsigned int dwMagic = 0;
		oNetStream.ReadInt(dwMagic);
		if (dwMagic != s_dwMagic)
		{
			return NULL;
		}

		MessageOperator* pOperator = new MessageOperator;
		oNetStream.ReadInt(pOperator->dwProtoId);

		pOperator->szData.append(oNetStream.ReadData(wLen - GetHeaderLength()), wLen - GetHeaderLength());
		
		return pOperator;
	}

	

};

#endif // !__MESSAGE_HEADER_H__
