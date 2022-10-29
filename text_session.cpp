#include "text_session.h"
#include "include/message_event.h"
#include "include/netstream.h"
#include "include/iothread.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"
#include "include/log_utility.h"

#include <string>

CTextSession::TextMessageEvent::TextMessageEvent(ISession* pSession, std::string& refData)
	: m_pSession(pSession)
{
	assert(refData.size());
	m_szData.swap(refData);
}

void CTextSession::TextMessageEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(TextMessageEvent, this);

	LOG(pOStream, ELOG_LEVEL_INFO) << m_pSession->GetSocket()->NativeSocket() << ", " << m_szData.size() << ", " << m_szData
		<< "[" << __FILE__ << ":" << __LINE__ << "," << __FUNCTION__ << "]\n";
	m_pSession->OnRecv(m_szData.c_str(), (unsigned int)m_szData.size());
}

CTextSession::ConnectedEvent::ConnectedEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CTextSession::ConnectedEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(ConnectedEvent, this);
	m_pSession->OnConnected(pOStream);
}

CTextSession::SessionErrorEvent::SessionErrorEvent(ISession* pSession, int dwError)
	: m_dwError(dwError)
	, m_pSession(pSession)
{
}

void CTextSession::SessionErrorEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionErrorEvent, this);
	m_pSession->OnError(m_dwError, pOStream);
}

CTextSession::CloseSessionEvent::CloseSessionEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CTextSession::CloseSessionEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(CloseSessionEvent, this);
	m_pSession->OnClose(pOStream);
}

CTextSession& CTextSession::Send(const char* szData, unsigned int dwLen)
{
	class SendOperator : public IOEventBase
	{
	public:
		SendOperator(FXNET::ISocketBase* opSock)
			: m_opSock(opSock)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(SendOperator, this);

			FXNET::CConnectorSocket* poConnector = dynamic_cast<FXNET::CConnectorSocket*>(m_opSock);
			if (poConnector->GetError())
			{
				return;
			}
			CTextSession* poSession = dynamic_cast<CTextSession*>(poConnector->GetSession());
			poSession->GetSendBuff().WriteString(m_szData.c_str(), (unsigned int)m_szData.size());
			poConnector->SendMessage(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		std::string m_szData;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(m_opSock);
	pOperator->m_szData.assign(szData, dwLen);
	FXNET::PostEvent(pOperator);
	return *this;
}

CTextSession& CTextSession::OnRecv(const char* szData, unsigned int dwLen)
{
	std::string strData(szData, dwLen);
	strData += ((szData[dwLen - 1] + 1 - '0') % 10 + '0');
	//if (strData.size() > 16 * 512)
	if (strData.size() > 1024 * 32)
	{
		strData = "0";
	}
	Send(strData.c_str(), (unsigned int)strData.size());
	return *this;
}

void CTextSession::OnConnected(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << GetSocket()->NativeSocket() << ", connected!!!"
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
	std::string sz("0");
	Send(sz.c_str(), (unsigned int)sz.size());
}

void CTextSession::OnError(int dwError, std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_ERROR) << GetSocket()->NativeSocket() << ", error: " << dwError
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

	class ErrorOperator : public IOEventBase
	{
	public:
		ErrorOperator(FXNET::ISocketBase* opSock, int dwError)
			: m_opSock(opSock)
			, m_dwError(dwError)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(ErrorOperator, this);

			m_opSock->OnError(m_dwError, pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		int m_dwError;
	protected:
	private:
	};

	ErrorOperator* pOperator = new ErrorOperator(m_opSock, dwError);
	FXNET::PostEvent(pOperator);
}

void CTextSession::OnClose(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << GetSocket()->NativeSocket()
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

	class OnCloseOperator : public IOEventBase
	{
	public:
		OnCloseOperator(FXNET::ISocketBase* opSock)
			: m_opSock(opSock)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(OnCloseOperator, this);

			m_opSock->OnClose(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		std::string m_szData;
	protected:
	private:
	};

	OnCloseOperator* pOperator = new OnCloseOperator(m_opSock);
	FXNET::PostEvent(pOperator);

	LOG(pOStream, ELOG_LEVEL_INFO) << "session close, " << this
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

	SetSock(NULL);

	//TODO 先在这里处理
	delete this;
}

void CTextSession::Close(std::ostream* pOStream)
{
	class CloseOperator : public IOEventBase
	{
	public:
		CloseOperator(FXNET::ISocketBase* opSock)
			: m_opSock(opSock)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(CloseOperator, this);

			m_opSock->Close(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		std::string m_szData;
	protected:
	private:
	};

	CloseOperator* pOperator = new CloseOperator(m_opSock);
	FXNET::PostEvent(pOperator);
}

CTextSession::TextMessageEvent* CTextSession::NewRecvMessageEvent(std::string& refData)
{
	CTextSession::TextMessageEvent* pEvent = new CTextSession::TextMessageEvent(this, refData);
	return pEvent;
}

MessageEventBase* CTextSession::NewConnectedEvent()
{
	CTextSession::ConnectedEvent* pEvent = new CTextSession::ConnectedEvent(this);
	return pEvent;
}

CTextSession::SessionErrorEvent* CTextSession::NewErrorEvent(int dwError)
{
	CTextSession::SessionErrorEvent* pEvent = new CTextSession::SessionErrorEvent(this, dwError);
	return pEvent;
}

CTextSession::CloseSessionEvent* CTextSession::NewCloseEvent()
{
	CTextSession::CloseSessionEvent* pEvent = new CTextSession::CloseSessionEvent(this);
	return pEvent;
}

