#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"
#include "i_session.h"

#include <deque>
#include <string>

namespace FXNET
{

	void StartIOModule();

	void PostEvent(IOEventBase* pEvent);
	void SwapEvent(std::deque<MessageEventBase*>& refDeque);

	//在io线程执行 不要直接调用
	void UdpListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	void UdpConnect(const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);
	void TcpListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	void TcpConnect(const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

	class UDPListen : public IOEventBase
	{
	public:
		UDPListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker)
			: m_szIp(szIp)
			, m_wPort(wPort)
			, m_pSessionMaker(pSessionMaker)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPListen, this);
			UdpListen(m_szIp.c_str(), m_wPort, m_pSessionMaker, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		SessionMaker* m_pSessionMaker;
	};

	class UDPConnect : public IOEventBase
	{
	public:
		UDPConnect(const char* szIp, unsigned short wPort, ISession* pSession)
			: m_szIp(szIp)
			, m_wPort(wPort)
			, m_pSession(pSession)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnect, this);
			TcpConnect(m_szIp.c_str(), m_wPort, m_pSession, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		ISession* m_pSession;
	};

	class TCPListen : public IOEventBase
	{
	public:
		TCPListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker)
			: m_szIp(szIp)
			, m_wPort(wPort)
			, m_pSessionMaker(pSessionMaker)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPListen, this);
			TcpListen(m_szIp.c_str(), m_wPort, m_pSessionMaker, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		SessionMaker* m_pSessionMaker;
	};

	class TCPConnect : public IOEventBase
	{
	public:
		TCPConnect(const char* szIp, unsigned short wPort, ISession* pSession)
			: m_szIp(szIp)
			, m_wPort(wPort)
			, m_pSession(pSession)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPConnect, this);
			TcpConnect(m_szIp.c_str(), m_wPort, m_pSession, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		ISession* m_pSession;
	};

};


#endif //!__FXNET_INTERFACE_H__
