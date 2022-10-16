#pragma once
#include "include/i_session.h"
#include "include/net_work_stream.h"
#include "include/message_event.h"

class CTextSession : public FXNET::ISession
{
public:
	class TestMessageEvent : public MessageEventBase
	{
	public:
		TestMessageEvent(ISession* pSession, std::string& refData);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
		std::string m_szData;
	};
	class ConnectedEvent : public MessageEventBase
	{
	public:
		ConnectedEvent(ISession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
	};
	virtual CTextSession& Send(const char* szData, unsigned int dwLen);
	virtual CTextSession& OnRecv(const char* szData, unsigned int dwLen);

	virtual void OnConnected(std::ostream* pOStream);

	virtual TextWorkStream& GetSendBuff() { return m_oSendBuff; }
	virtual TextWorkStream& GetRecvBuff() { return m_oRecvBuff; }

	virtual TestMessageEvent* NewRecvMessageEvent(std::string& refData);
	virtual MessageEventBase* NewConnectedEvent();

protected:
	TextWorkStream m_oSendBuff;
	TextWorkStream m_oRecvBuff;
};

class TextSessionMaker : public FXNET::SessionMaker
{
public:
	virtual CTextSession* operator()() { return new CTextSession(); };
};

