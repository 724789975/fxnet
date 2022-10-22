#include "text_session.h"
#include "include/message_event.h"
#include "include/netstream.h"
#include "include/iothread.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"

#include <string>

CTextSession::TestMessageEvent::TestMessageEvent(ISession* pSession, std::string& refData)
	: m_pSession(pSession)
{
	assert(refData.size());
	m_szData.swap(refData);
}

void CTextSession::TestMessageEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(TestMessageEvent, this);

	if (pOStream)
	{
		*pOStream << m_pSession->NativeSocket() << ", " << m_szData.size() << ", " << m_szData
			<< "[" << __FILE__ << ":" << __LINE__ << "," << __FUNCTION__ << "]\n";
	}
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

CTextSession& CTextSession::Send(const char* szData, unsigned int dwLen)
{
	class SendOperator : public IOEventBase
	{
	public:
		SendOperator(NativeSocketType hSock)
			: m_hSock(hSock)
		{
		}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(SendOperator, this);

			FXNET::ISocketBase* poSock = FXNET::FxIoModule::Instance()->GetSocket(m_hSock);
			if (poSock)
			{
				FXNET::CConnectorSocket* poConnector = dynamic_cast<FXNET::CConnectorSocket*>(poSock);
				CTextSession* poSession = dynamic_cast<CTextSession*>(poConnector->GetSession());
				poSession->GetSendBuff().WriteString(m_szData.c_str(), (unsigned int)m_szData.size());
				poConnector->SendMessage(pOStream);
			}
			else
			{
				if (pOStream)
				{
					*pOStream << "find sock error " << m_hSock
						<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";
				}
			}
		}
		NativeSocketType m_hSock;
		std::string m_szData;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(m_hSock);
	pOperator->m_szData.assign(szData, dwLen);
	FXNET::PostEvent(pOperator);
	return *this;
}

CTextSession& CTextSession::OnRecv(const char* szData, unsigned int dwLen)
{
	std::string strData(szData, dwLen);
	strData += ((szData[dwLen - 1] + 1 - '0') % 10 + '0');
	//if (strData.size() > 16 * 512)
	if (strData.size() > 2048)
	{
		strData = "0";
	}
	Send(strData.c_str(), (unsigned int)strData.size());
	return *this;
}

void CTextSession::OnConnected(std::ostream* pOStream)
{
	if (pOStream)
	{
		*pOStream << NativeSocket() << ", connected!!!\n";
	}
	std::string sz("0");
	this->Send(sz.c_str(), (unsigned int)sz.size());
}

CTextSession::TestMessageEvent* CTextSession::NewRecvMessageEvent(std::string& refData)
{
	CTextSession::TestMessageEvent* pEvent = new CTextSession::TestMessageEvent(this, refData);
	return pEvent;
}

MessageEventBase* CTextSession::NewConnectedEvent()
{
	CTextSession::ConnectedEvent* pEvent = new CTextSession::ConnectedEvent(this);
	return pEvent;
}


