#pragma once
#include "include/i_session.h"
#include "include/net_work_stream.h"
#include "include/message_event.h"

class CTextSession : public FXNET::ISession
{
public:
	class TextMessageEvent : public MessageEventBase
	{
	public:
		TextMessageEvent(ISession* pSession, std::string& refData);
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
	class SessionErrorEvent : public MessageEventBase
	{
	public:
		SessionErrorEvent(ISession* pSession, int dwError);
		virtual void operator ()(std::ostream* pOStream);
		int m_dwError;
		ISession* m_pSession;
	};
	class CloseSessionEvent : public MessageEventBase
	{
	public:
		CloseSessionEvent(ISession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
	};
	virtual CTextSession& Send(const char* szData, unsigned int dwLen);
	virtual CTextSession& OnRecv(const char* szData, unsigned int dwLen);

	virtual void OnConnected(std::ostream* pOStream);
	virtual void OnError(int dwError, std::ostream* pOStream);
	virtual void OnClose(std::ostream* pOStream);

	virtual TextWorkStream& GetSendBuff() { return m_oSendBuff; }
	virtual TextWorkStream& GetRecvBuff() { return m_oRecvBuff; }

	virtual TextMessageEvent* NewRecvMessageEvent(std::string& refData);
	virtual MessageEventBase* NewConnectedEvent();
	virtual SessionErrorEvent* NewErrorEvent(int dwError);
	virtual CloseSessionEvent* NewCloseEvent();

protected:
	TextWorkStream m_oSendBuff;
	TextWorkStream m_oRecvBuff;
};

class TextSessionMaker : public FXNET::SessionMaker
{
public:
	virtual CTextSession* operator()() { return new CTextSession(); };
};

