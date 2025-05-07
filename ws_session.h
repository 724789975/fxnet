#pragma once
#include "include/i_session.h"
#include "include/net_work_stream.h"
#include "include/message_event.h"

#include <map>

class CWSSession : public FXNET::ISession
{
	enum ESocketHandShakeState
	{
		WSHSS_None = 0,
		WSHSS_Request,
		WSHSS_Response,
		WSHSS_Connected,
	};

public:
	class WSMessageEvent : public MessageRecvEventBase
	{
	public:
		WSMessageEvent(ISession* pSession);
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
		SessionOnSendEvent(CWSSession* pSession);
		virtual void operator ()(std::ostream* pOStream);
		CWSSession* m_pSession;
		int m_dwLen;
	};

	class HeaderCheck : public WSWorkStream::WSHeaderCheck
	{
	public:
		HeaderCheck(CWSSession* pSession)
			: m_pSession(pSession)
			{}
		virtual bool operator ()()
		{
			return WSHSS_Connected == m_pSession->m_eSocketHandShakeState;
		}
		CWSSession* m_pSession;
	};

	CWSSession()
		: m_dConnectedTime(0.)
		, m_dwPacketLength(0)
		, m_dwRecvPackagetNum(0)
		, m_dAllDelayTime(0.)
		, m_dCurrentDelay(0.)
		, m_dAverageDelay(0.)
		, m_eSocketHandShakeState(WSHSS_None)
		, m_szWebInfo("")
		, m_oHeaderCheck(this)
		, m_oSendBuff(m_oHeaderCheck)
		, m_oRecvBuff(m_oHeaderCheck)
		{}
	~CWSSession(){}

	virtual CWSSession& Send(const char* szData, unsigned int dwLen, std::ostream* pOStream);
	virtual CWSSession& OnRecv(FXNET::CNetStreamPackage& refPackage, std::ostream* pOStream);

	virtual void OnConnected(std::ostream* pOStream);
	virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
	virtual void OnClose(std::ostream* pOStream);

	virtual void Close(std::ostream* pOStream);

	virtual WSWorkStream& GetSendBuff() { return m_oSendBuff; }
	virtual WSWorkStream& GetRecvBuff() { return m_oRecvBuff; }

	virtual WSMessageEvent* NewRecvMessageEvent();
	virtual MessageEventBase* NewConnectedEvent();
	virtual SessionErrorEvent* NewErrorEvent(const ErrorCode& refError);
	virtual CloseSessionEvent* NewCloseEvent();
	virtual SessionOnSendEvent* NewOnSendEvent(int dwLen);

protected:
	WSWorkStream m_oSendBuff;
	WSWorkStream m_oRecvBuff;

	ESocketHandShakeState m_eSocketHandShakeState;
	char								m_szWebInfo[1024];

	double m_dConnectedTime;
	int m_dwPacketLength;

	std::map<long long, double> m_mapSendTimes;
	int m_dwRecvPackagetNum;
	double m_dAllDelayTime;
	double m_dCurrentDelay;
	double m_dAverageDelay;

	HeaderCheck m_oHeaderCheck;

};

class WSSessionMaker : public FXNET::SessionMaker
{
public:
	virtual CWSSession* operator()() { return new CWSSession(); };
};

