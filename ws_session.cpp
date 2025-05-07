#include "ws_session.h"
#include "include/message_event.h"
#include "include/connector_socket.h"
#include "include/fxnet_interface.h"
#include "include/log_utility.h"
#include "include/net_stream_package.h"
#include "include/base64.h"
#include "include/sha1.h"
#include "utility/time_utility.h"

#include <string>
#include <stdlib.h>
#include <stdio.h>

std::string ResponseKey(std::string szWebInfo)
{
	std::istringstream s(szWebInfo);
	std::string szRequest;

	std::getline(s, szRequest);
	if (szRequest[szRequest.size() - 1] == '\r')
	{
		szRequest.erase(szRequest.end() - 1);
	}
	else
	{
		return "";
	}

	std::string szHeader;
	std::string::size_type sizetypeEnd;

	std::map<std::string, std::string> mapHeader;

	while (std::getline(s, szHeader) && szHeader != "\r")
	{
		if (szHeader[szHeader.size() - 1] != '\r')
		{
			continue; //end
		}
		else
		{
			szHeader.erase(szHeader.end() - 1);    //remove last char
		}

		sizetypeEnd = szHeader.find(": ", 0);
		if (sizetypeEnd != std::string::npos)
		{
			std::string key = szHeader.substr(0, sizetypeEnd);
			std::string value = szHeader.substr(sizetypeEnd + 2);
			mapHeader[key] = value;
		}
	}

	std::string szServerKey = mapHeader["Sec-WebSocket-Key"];
	szServerKey += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

	SHA1 sha;
	unsigned int dwarrMessageDigest[5];
	sha.Reset();
	sha << szServerKey.c_str();
	memset(dwarrMessageDigest, 0, sizeof(dwarrMessageDigest));
	sha.Result(dwarrMessageDigest);
	for (int i = 0; i < 5; i++)
	{
		dwarrMessageDigest[i] = htonl(dwarrMessageDigest[i]);
	}
	return base64::base64_encode((unsigned char*)dwarrMessageDigest, 20);
}

CWSSession::WSMessageEvent::WSMessageEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CWSSession::WSMessageEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(WSMessageEvent, this);

	this->m_pSession->OnRecv(this->m_oPackage, pOStream);
}

CWSSession::ConnectedEvent::ConnectedEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CWSSession::ConnectedEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(ConnectedEvent, this);
	this->m_pSession->OnConnected(pOStream);
}

CWSSession::SessionErrorEvent::SessionErrorEvent(ISession* pSession, const ErrorCode& oError)
	: m_oError(oError)
	, m_pSession(pSession)
{
}

void CWSSession::SessionErrorEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionErrorEvent, this);
	this->m_pSession->OnError(this->m_oError, pOStream);
}

CWSSession::CloseSessionEvent::CloseSessionEvent(ISession* pSession)
	: m_pSession(pSession)
{
}

void CWSSession::CloseSessionEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(CloseSessionEvent, this);
	this->m_pSession->OnClose(pOStream);
}

CWSSession::SessionOnSendEvent::SessionOnSendEvent(CWSSession* pSession)
	: m_pSession(pSession)
	, m_dwLen(0)
{
}

void CWSSession::SessionOnSendEvent::operator()(std::ostream* pOStream)
{
	DELETE_WHEN_DESTRUCT(SessionOnSendEvent, this);
	m_pSession->m_eSocketHandShakeState = WSHSS_Connected;
}

CWSSession& CWSSession::Send(const char* szData, unsigned int dwLen, std::ostream* pOStream)
{
	class SendOperator : public IOEventBase
	{
	public:
		SendOperator(FXNET::CConnectorSocket* opSock, CWSSession* pSession)
			: m_opSock(opSock)
			, m_pSession(pSession)
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
			
			m_pSession->GetSendBuff().PushData(m_oPackage);
			ErrorCode oError;
			poConnector->SendMessage(oError, pOStream);
			LOG(pOStream, ELOG_LEVEL_DEBUG4) << this->m_opSock->Name()
				<< ", " << this->m_opSock->NativeSocket()
				<< "\n";
		}
		FXNET::CConnectorSocket* m_opSock;
		FXNET::CNetStreamPackage m_oPackage;
		CWSSession * m_pSession;
	protected:
	private:
	};

	SendOperator* pOperator = new SendOperator(this->m_opSock, this);
	pOperator->m_oPackage.WriteData(szData, dwLen);
	FXNET::PostEvent(this->m_opSock->GetIOModuleIndex(), pOperator);
	return *this;
}

CWSSession& CWSSession::OnRecv(FXNET::CNetStreamPackage& refPackage, std::ostream* pOStream)
{
	this->m_dwPacketLength += refPackage.GetDataLength();

	if (WSHSS_Request == this->m_eSocketHandShakeState)
	{
		int nLen = refPackage.GetDataLength();
		refPackage.ReadData(m_szWebInfo, nLen);
		//判断pUseBuf 最后四位是不是"\r\n\r\n" 就可以了
		if (strncmp(m_szWebInfo + nLen - 4, "\r\n\r\n", 4) != 0)
		{
			this->Close(pOStream);
			return *this;
		}
		m_eSocketHandShakeState = WSHSS_Response;

		char szResponse[1024] = { 0 };
		sprintf(szResponse, "HTTP/1.1 101 Switching Protocols\r\n"
			"Connection: Upgrade\r\n"
			"Upgrade: WebSocket\r\n"
			"Server: %s\r\n"
			"Sec-WebSocket-Accept: %s\r\n"
			"\r\n",
			"FXNET", // TODO: 服务器名称
			ResponseKey(m_szWebInfo).c_str()
		);

		this->Send(szResponse, strlen(szResponse), pOStream);
		LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
			<< ", " << this->m_opSock->NativeSocket()
			<< ", handshake response: " << szResponse
			<< "\n";
		// m_eSocketHandShakeState = WSHSS_Connected;
		return *this;
	}
	
	int dwMagicNum = 0;
	// refPackage.ReadInt(dwMagicNum);
	std::string szData;
	szData.resize(refPackage.GetDataLength() + 1);

	char* pBuf = (char*)szData.data();
	refPackage.ReadData(pBuf, refPackage.GetDataLength());

	this->Send(szData.c_str(), szData.size(), pOStream);
	LOG(pOStream, ELOG_LEVEL_INFO) << this->m_opSock->Name()
		<< ", " << this->m_opSock->NativeSocket();

	return *this;
}

void CWSSession::OnConnected(std::ostream* pOStream)
{
	LOG(pOStream, ELOG_LEVEL_INFO) << GetSocket()->NativeSocket() << ", connected!!!"
		<< "\n";
	m_eSocketHandShakeState = WSHSS_Request;
}

void CWSSession::OnError(const ErrorCode& refError, std::ostream* pOStream)
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
}

void CWSSession::OnClose(std::ostream* pOStream)
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

	//TODO 先在这里处理
	delete this;
}

void CWSSession::Close(std::ostream* pOStream)
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

CWSSession::WSMessageEvent* CWSSession::NewRecvMessageEvent()
{
	CWSSession::WSMessageEvent* pEvent = new CWSSession::WSMessageEvent(this);
	return pEvent;
}

MessageEventBase* CWSSession::NewConnectedEvent()
{
	CWSSession::ConnectedEvent* pEvent = new CWSSession::ConnectedEvent(this);
	return pEvent;
}

CWSSession::SessionErrorEvent* CWSSession::NewErrorEvent(const ErrorCode& refError)
{
	CWSSession::SessionErrorEvent* pEvent = new CWSSession::SessionErrorEvent(this, refError);
	return pEvent;
}

CWSSession::CloseSessionEvent* CWSSession::NewCloseEvent()
{
	CWSSession::CloseSessionEvent* pEvent = new CWSSession::CloseSessionEvent(this);
	return pEvent;
}

CWSSession::SessionOnSendEvent* CWSSession::NewOnSendEvent(int dwLen)
{
	CWSSession::SessionOnSendEvent* pEvent = new CWSSession::SessionOnSendEvent(this);
	pEvent->m_dwLen = dwLen;
	return pEvent;
}

