#include "text_session.h"
#include "include/message_event.h"
#include "include/netstream.h"
#include "include/iothread.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"
#include "include/log_utility.h"
#include "include/iothread.h"

#include <string>

CTextSession::TextMessageEvent::TextMessageEvent(ISession* pSession, std::string& refData)
	: m_pSession(pSession)
{
	assert(refData.size());
	this->m_szData.swap(refData);
}

void CTextSession::TextMessageEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(TextMessageEvent, this);

	//LOG(pOStream, ELOG_LEVEL_INFO) << m_pSession->GetSocket()->NativeSocket() << ", " << m_szData.size() //<< ", " << m_szData
	//	<< "[" << __FILE__ << ":" << __LINE__ << "," << __FUNCTION__ << "]\n";
	this->m_pSession->OnRecv(this->m_szData.c_str(), (unsigned int)this->m_szData.size(), pOStream);
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

CTextSession::SessionErrorEvent::SessionErrorEvent(ISession* pSession, int dwError)
	: m_dwError(dwError)
	, m_pSession(pSession)
{
}

void CTextSession::SessionErrorEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionErrorEvent, this);
	this->m_pSession->OnError(this->m_dwError, pOStream);
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

	// std::string sz;
	// sz.resize(m_dwLen);

	// m_pSession->Send(sz.data(), sz.size(), pOStream);
}

CTextSession& CTextSession::Send(const char* szData, unsigned int dwLen, std::ostream* pOStream)
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

			FXNET::CConnectorSocket* poConnector = dynamic_cast<FXNET::CConnectorSocket*>(this->m_opSock);
			if (poConnector->GetError())
			{
				return;
			}
			CTextSession* poSession = dynamic_cast<CTextSession*>(poConnector->GetSession());
			poSession->GetSendBuff().WriteString(m_szData.c_str(), (unsigned int)this->m_szData.size());
			poConnector->SendMessage(pOStream);
		}
		FXNET::ISocketBase* m_opSock;
		std::string m_szData;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(this->m_opSock);
	pOperator->m_szData.assign(szData, dwLen);
	FXNET::PostEvent(pOperator);
	return *this;
}

CTextSession& CTextSession::OnRecv(const char* szData, unsigned int dwLen, std::ostream* pOStream)
{
	//std::string strData(szData, dwLen);
	//strData += ((szData[dwLen - 1] + 1 - '0') % 10 + '0');
	////if (strData.size() > 16 * 512)
	//if (strData.size() > 1024 * 32)
	//{
	//	strData = "0";
	//}
	//Send(strData.c_str(), (unsigned int)strData.size(), pOStream);

	this->m_dwPacketLength += dwLen;
	// LOG(pOStream, ELOG_LEVEL_INFO) << m_opSock->Name()
	// 	<< ", total: " << m_dwPacketLength << ", average: "
	// 	<< m_dwPacketLength / (FXNET::FxIoModule::Instance()->FxGetCurrentTime() - m_dConnectedTime)
	// 	<< "\n";
	
	long long qwRecv = atoll(szData);
	if (this->m_mapSendTimes.end() == this->m_mapSendTimes.find(qwRecv))
	{
		this->Send(szData, dwLen, pOStream);
		LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", seq: " << (qwRecv & 0xFFFF)
			<< "\n";
	}
	else
	{
		double dCurrentTime = FXNET::FxIoModule::Instance()->FxGetCurrentTime();
		this->m_dCurrentDelay = dCurrentTime - this->m_mapSendTimes[qwRecv];
		++this->m_dwRecvPackagetNum;
		this->m_dAllDelayTime += this->m_dCurrentDelay;
		this->m_dAverageDelay = this->m_dAllDelayTime / this->m_dwRecvPackagetNum;

		LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", current delay: " << this->m_dCurrentDelay << ", average: "
			<< this->m_dAverageDelay
			<< ", package num: " << this->m_dwRecvPackagetNum
			<< "\n";

		char szBuff[512] = {0};
		long long qwSend = (qwRecv & 0xFFFFFFFFFFFF0000) | ((qwRecv + 1) & 0xFFFF);
		sprintf(szBuff, "%lld", qwSend);
		std::string szSend(szBuff, 400);

		this->m_mapSendTimes.erase(qwRecv);
		this->m_mapSendTimes[qwSend] = dCurrentTime;
		this->Send(szSend.c_str(), (unsigned int)szSend.size(), pOStream);
	}
	
	return *this;
}

void CTextSession::OnConnected(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << GetSocket()->NativeSocket() << ", connected!!!"
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";

	long long qwSend = (long long)(this->GetSocket()->GetLocalAddr().sin_addr.s_addr) << 32 | (this->GetSocket()->GetLocalAddr().sin_port << 16);
	char szBuff[512] = {0};
	sprintf(szBuff, "%lld", qwSend);
	std::string sz(szBuff, 400);
	// std::string sz;
	// sz.resize(1024 * 8);

	Send(sz.c_str(), (unsigned int)sz.size(), pOStream);
	this->m_mapSendTimes[qwSend] = FXNET::FxIoModule::Instance()->FxGetCurrentTime();

	this->m_dConnectedTime = FXNET::FxIoModule::Instance()->FxGetCurrentTime();
	this->m_dwPacketLength = 0;
}

void CTextSession::OnError(int dwError, std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_ERROR) << this->GetSocket()->NativeSocket() << ", error: " << dwError
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

			this->m_opSock->OnError(m_dwError, pOStream);
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
	LOG(pOStream, ELOG_LEVEL_INFO) << this->GetSocket()->NativeSocket()
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

			this->m_opSock->OnClose(pOStream);
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

	this->SetSock(NULL);

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

			this->m_opSock->Close(pOStream);
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

CTextSession::SessionOnSendEvent* CTextSession::NewOnSendEvent(int dwLen)
{
	CTextSession::SessionOnSendEvent* pEvent = new CTextSession::SessionOnSendEvent(this);
	pEvent->m_dwLen = dwLen;
	return pEvent;
}

