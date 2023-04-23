#include "text_session.h"
#include "include/message_event.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"
#include "include/log_utility.h"
#include "include/net_stream_package.h"
#include "utility/time_utility.h"

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

	//LOG(pOStream, ELOG_LEVEL_INFO) << m_pSession->GetSocket()->NativeSocket() << ", " << m_szData.size() //<< ", " << m_szData
	//	<< "[" << __FILE__ << ":" << __LINE__ << "," << __FUNCTION__ << "]\n";
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

	// std::string sz;
	// sz.resize(m_dwLen);

	// m_pSession->Send(sz.data(), sz.size(), pOStream);
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
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
			
			poConnector->GetSession()->GetSendBuff().PushData(m_oPackage);
			poConnector->SendMessage(pOStream);
			LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->m_opSock->Name()
				<< ", " << this->m_opSock->NativeSocket()
				<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
		}
		FXNET::CConnectorSocket* m_opSock;
		FXNET::CNetStreamPackage m_oPackage;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(this->m_opSock);
	pOperator->m_oPackage.WriteInt('T' << 24 | 'E' << 16 | 'S' << 8 | 'T');
	pOperator->m_oPackage.WriteString(szData, dwLen);
	FXNET::PostEvent(pOperator);
	return *this;
}

CTextSession& CTextSession::OnRecv(FXNET::CNetStreamPackage& refPackage, std::ostream* pOStream)
{
	//std::string strData(szData, dwLen);
	//strData += ((szData[dwLen - 1] + 1 - '0') % 10 + '0');
	////if (strData.size() > 16 * 512)
	//if (strData.size() > 1024 * 32)
	//{
	//	strData = "0";
	//}
	//Send(strData.c_str(), (unsigned int)strData.size(), pOStream);

	this->m_dwPacketLength += refPackage.GetDataLength();
	// LOG(pOStream, ELOG_LEVEL_INFO) << m_opSock->Name()
	// 	<< ", total: " << m_dwPacketLength << ", average: "
	// 	<< m_dwPacketLength / (FXNET::FxIoModule::Instance()->FxGetCurrentTime() - m_dConnectedTime)
	// 	<< "\n";
	
	int dwMagicNum = 0;
	refPackage.ReadInt(dwMagicNum);
	std::string szData;
	refPackage.ReadString(szData);

	long long qwRecv = atoll(szData.c_str());
	if (this->m_mapSendTimes.end() == this->m_mapSendTimes.find(qwRecv))
	{
		this->Send(szData.c_str(), szData.size(), pOStream);
		LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", seq: " << (qwRecv & 0xFFFF)
			<< "\n";
		// for (size_t i = 0; i < 1000; i++)
		// {
		// 	this->Send(szData, dwLen, pOStream);
		// 	LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
		// 		<< ", " << this->m_opSock->NativeSocket()
		// 		<< ", seq: " << (qwRecv & 0xFFFF)
		// 		<< "\n";
		// }
	}
	else
	{
		// int dwRandLen = UTILITY::TimeUtility::GetTimeUS() % 2017;
		int dwRandLen = UTILITY::TimeUtility::GetTimeUS() % 970;
		double dCurrentTime = UTILITY::TimeUtility::GetTimeUS() / 1000000.;
		this->m_dCurrentDelay = dCurrentTime - this->m_mapSendTimes[qwRecv];
		++this->m_dwRecvPackagetNum;
		this->m_dAllDelayTime += this->m_dCurrentDelay;
		this->m_dAverageDelay = this->m_dAllDelayTime / this->m_dwRecvPackagetNum;

		LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", current delay: " << this->m_dCurrentDelay << ", average: "
			<< this->m_dAverageDelay
			<< ", package num: " << this->m_dwRecvPackagetNum
			<< ", seq: " << (qwRecv & 0xFFFF)
			<< "\n";

		char szBuff[2048] = {0};
		long long qwSend = (qwRecv & 0xFFFFFFFFFFFF0000) | ((qwRecv + 1) & 0xFFFF);
		sprintf(szBuff, "%lld", qwSend);
		std::string szSend(szBuff, 30 + dwRandLen);

		this->m_mapSendTimes.erase(qwRecv);
		this->m_mapSendTimes[qwSend] = dCurrentTime;
		this->Send(szSend.c_str(), (unsigned int)szSend.size(), pOStream);

		LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", " << szSend
			<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
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

	this->Send(sz.c_str(), (unsigned int)sz.size(), pOStream);
	this->m_mapSendTimes[qwSend] = UTILITY::TimeUtility::GetTimeUS() / 1000000.;

	this->m_dConnectedTime = this->m_mapSendTimes[qwSend];
	this->m_dwPacketLength = 0;
}

void CTextSession::OnError(const ErrorCode& refError, std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_ERROR) << this->GetSocket()->NativeSocket() << ", error: " << refError.What()
		<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION__ << "]\n";

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
	protected:
	private:
	};

	CloseOperator* pOperator = new CloseOperator(m_opSock);
	FXNET::PostEvent(pOperator);
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

