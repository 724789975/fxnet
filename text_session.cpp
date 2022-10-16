#include "text_session.h"
#include "include/message_event.h"
#include "include/netstream.h"
#include "include/iothread.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"

#include <string>

int my_atoi(std::string s)
{
	bool b_readed = false;

	int flag = 1;

	long long num = 0;

	for (auto p : s)
	{
		if ((p > '9' || p < '0') && (p != '-' && p != '+'))
		{
			if (' ' != p)
			{
				break;
			}
			if (b_readed)
			{
				break;
			}
			continue;
		}
		if (p == '-')
		{
			if (b_readed)
			{
				break;
			}
			else
			{
				flag = -1;
				b_readed = true;
				continue;
			}
		}
		if ('+' == p)
		{
			b_readed = true;
			continue;
		}
		if (0x80000000 <= 10 * num + (p - '0'))
		{
			if (1 == flag) return 0x7FFFFFFF;
			else return 0x80000000;
		}
		num = 10 * num + (p - '0');

		b_readed = true;
	}

	return flag * num;
}

void my_itoa(unsigned int dwData, std::string& szDest)
{
	char szBuff[128] = { 0 };

}

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
		*pOStream << m_pSession->NativeSocket() << ", " << m_szData.size() << ", " << m_szData << "\n";
	}
	m_pSession->OnRecv(m_szData.c_str(), m_szData.size());
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
				poSession->GetSendBuff().WriteString(m_szData.c_str(), m_szData.size());
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
	strData += ((strData.back() + 1 - '0') % 10 + '0');
	if (strData.size() > 16 * 512)
	{
		strData = "0";
	}
	Send(strData.c_str(), strData.size());
	return *this;
}

void CTextSession::OnConnected(std::ostream* pOStream)
{
	if (pOStream)
	{
		*pOStream << NativeSocket() << ", connected!!!\n";
	}
	std::string sz("0");
	//this->Send(sz.c_str(), sz.size());
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


