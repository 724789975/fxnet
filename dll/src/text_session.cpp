#include "text_session.h"
#include "../../include/message_event.h"
#include "../../include/connector_socket.h"
#include "../../include/fxnet_interface.h"
#include "../../include/log_utility.h"
#include "../../include/net_stream_package.h"
#include "../../utility/time_utility.h"

#include <string>
#include <stdlib.h>
#include <stdio.h>

CTextSession::TextMessageEvent::TextMessageEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CTextSession::TextMessageEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(TextMessageEvent, this);

	this->m_pSession->OnRecv(this->m_oPackage, pOStream);
}

CTextSession::ConnectedEvent::ConnectedEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CTextSession::ConnectedEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(ConnectedEvent, this);
	this->m_pSession->OnConnected(pOStream);
}

CTextSession::SessionErrorEvent::SessionErrorEvent(ISession* pSession, const ErrorCode& oError)
	: m_oError(oError)
	, m_pSession(pSession)
{
}

void CTextSession::SessionErrorEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionErrorEvent, this);
	this->m_pSession->OnError(this->m_oError, pOStream);
}

CTextSession::CloseSessionEvent::CloseSessionEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CTextSession::CloseSessionEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(CloseSessionEvent, this);
	this->m_pSession->OnClose(pOStream);
}

CTextSession::SessionOnSendEvent::SessionOnSendEvent(ISession* pSession)
	: m_pSession(pSession)
	, m_dwLen(0)
{
}

void CTextSession::SessionOnSendEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionOnSendEvent, this);
}

CTextSession& CTextSession::Send(const char* szData, unsigned int dwLen, std::ostream* pOStream)
{
	class SendOperator : public IOEventBase
	{
	public:
		SendOperator(FXNET::CConnectorSocket* opSock)
			: m_opSock(opSock)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(SendOperator, this);

			FXNET::CConnectorSocket* poConnector = this->m_opSock;
			if (poConnector->GetError())
			{
				return;
			}

			LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->m_opSock->Name()
				<< ", " << this->m_opSock->NativeSocket()
				<< "\n";
			
			poConnector->GetSession()->GetSendBuff().PushData(m_oPackage);
			ErrorCode oError;
			poConnector->SendMessage(oError, pOStream);
			LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->m_opSock->Name()
				<< ", " << this->m_opSock->NativeSocket()
				<< "\n";
		}
		FXNET::CConnectorSocket* m_opSock;
		FXNET::CNetStreamPackage m_oPackage;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(this->m_opSock);
	pOperator->m_oPackage.WriteString(szData, dwLen);
	FXNET::PostEvent(this->m_opSock->GetIOModuleIndex(), pOperator);
	return *this;
}

CTextSession& CTextSession::OnRecv(FXNET::CNetStreamPackage& refPackage, std::ostream* pOStream)
{
	int dwMagicNum = 0;
	std::string szData;
	this->m_pOnRecv(m_pConnector, refPackage.GetData(), refPackage.GetDataLength());

	return *this;
}

void CTextSession::OnConnected(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << GetSocket()->NativeSocket() << ", connected!!!"
		<< "\n";

	this->m_pOnConnected(m_pConnector);
}

void CTextSession::OnError(const ErrorCode& refError, std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_ERROR) << this->GetSocket()->NativeSocket() << ", error: " << refError.What()
		<< "\n";

	class ErrorOperator : public IOEventBase
	{
	public:
		ErrorOperator(FXNET::ISocketBase* opSock, ErrorCode dwError)
			: m_opSock(opSock)
			, m_oError(dwError)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(ErrorOperator, this);

			this->m_opSock->OnError(m_oError, pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		ErrorCode m_oError;
	protected:
	private:
	};

	ErrorOperator* pOperator = new ErrorOperator(m_opSock, refError);
	FXNET::PostEvent(this->m_opSock->GetIOModuleIndex(), pOperator);
	this->m_pOnError(m_pConnector, refError);
}

void CTextSession::OnClose(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << this->GetSocket()->NativeSocket()
		<< "\n";

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

			this->m_opSock->OnClose(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		std::string m_szData;
	protected:
	private:
	};

	OnCloseOperator* pOperator = new OnCloseOperator(m_opSock);
	FXNET::PostEvent(this->m_opSock->GetIOModuleIndex(), pOperator);

	LOG(pOStream, ELOG_LEVEL_INFO) << "session close, " << this
		<< "\n";

	this->SetSock(NULL);

	m_pOnClose(m_pConnector);
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

			this->m_opSock->Close(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
	protected:
	private:
	};

	CloseOperator* pOperator = new CloseOperator(m_opSock);
	FXNET::PostEvent(this->m_opSock->GetIOModuleIndex(), pOperator);
}

CTextSession::TextMessageEvent* CTextSession::NewRecvMessageEvent()
{
	CTextSession::TextMessageEvent* pEvent = new CTextSession::TextMessageEvent(this);
	return pEvent;
}

MessageEventBase* CTextSession::NewConnectedEvent()
{
	CTextSession::ConnectedEvent* pEvent = new CTextSession::ConnectedEvent(this);
	return pEvent;
}

CTextSession::SessionErrorEvent* CTextSession::NewErrorEvent(const ErrorCode& refError)
{
	CTextSession::SessionErrorEvent* pEvent = new CTextSession::SessionErrorEvent(this, refError);
	return pEvent;
}

CTextSession::CloseSessionEvent* CTextSession::NewCloseEvent()
{
	CTextSession::CloseSessionEvent* pEvent = new CTextSession::CloseSessionEvent(this);
	return pEvent;
}

CTextSession::SessionOnSendEvent* CTextSession::NewOnSendEvent(int dwLen)
{
	CTextSession::SessionOnSendEvent* pEvent = new CTextSession::SessionOnSendEvent(this);
	pEvent->m_dwLen = dwLen;
	return pEvent;
}

