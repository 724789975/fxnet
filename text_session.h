#pragma once
#include "include/i_session.h"
#include "include/net_work_stream.h"
#include "include/message_event.h"

#include <map>

class CTextSession : public FXNET::ISession
{
public:
	class TextMessageEvent : public MessageRecvEventBase
	{
	public:
		TextMessageEvent(ISession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
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
		SessionErrorEvent(ISession* pSession, const ErrorCode& oError);
		virtual void operator ()(std::ostream* pOStream);
		ErrorCode m_oError;
		ISession* m_pSession;
	};
	class CloseSessionEvent : public MessageEventBase
	{
	public:
		CloseSessionEvent(ISession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
	};
	class SessionOnSendEvent : public MessageEventBase
	{
	public:
		SessionOnSendEvent(ISession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		ISession* m_pSession;
		int m_dwLen;
	};

	CTextSession()
		: m_dConnectedTime(0.)
		, m_dwPacketLength(0)
		, m_dwRecvPackagetNum(0)
		, m_dAllDelayTime(0.)
		, m_dCurrentDelay(0.)
		, m_dAverageDelay(0.)
		{}
	~CTextSession(){}

	virtual CTextSession& Send(const char* szData, unsigned int dwLen, std::ostream* pOStream);
	virtual CTextSession& OnRecv(FXNET::CNetStreamPackage& refPackage, std::ostream* pOStream);

	virtual void OnConnected(std::ostream* pOStream);
	virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
	virtual void OnClose(std::ostream* pOStream);

	virtual void Close(std::ostream* pOStream);

	virtual TextWorkStream& GetSendBuff() { return m_oSendBuff; }
	virtual TextWorkStream& GetRecvBuff() { return m_oRecvBuff; }

	virtual TextMessageEvent* NewRecvMessageEvent();
	virtual MessageEventBase* NewConnectedEvent();
	virtual SessionErrorEvent* NewErrorEvent(const ErrorCode& refError);
	virtual CloseSessionEvent* NewCloseEvent();
	virtual SessionOnSendEvent* NewOnSendEvent(int dwLen);

protected:
	TextWorkStream m_oSendBuff;
	TextWorkStream m_oRecvBuff;

	double m_dConnectedTime;
	int m_dwPacketLength;

	std::map<long long, double> m_mapSendTimes;
	int m_dwRecvPackagetNum;
	double m_dAllDelayTime;
	double m_dCurrentDelay;
	double m_dAverageDelay;

};

class TextSessionMaker : public FXNET::SessionMaker
{
public:
	virtual CTextSession* operator()() { return new CTextSession(); };
};

