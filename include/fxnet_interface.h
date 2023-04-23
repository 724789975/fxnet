#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"
#include "i_session.h"

#include <deque>
#include <string>
#include <sstream>

namespace FXNET
{
	/**
	 * @brief 
	 * 
	 * ����io�߳�
	 */
	void StartIOModule();

	/**
	 * @brief 
	 * 
	 * ������־�߳�
	 */
	void StartLogModule();

	/**
	 * @brief 
	 * 
	 * ��io�߳�Ͷ���¼�
	 * @param pEvent 
	 */
	void PostEvent(IOEventBase* pEvent);
	/**
	 * @brief 
	 * 
	 * swap io �̲߳������¼�
	 * @param refDeque 
	 */
	void SwapEvent(std::deque<MessageEventBase*>& refDeque);

	/**
	 * @brief 
	 * 
	 * ��io�߳�ִ�� ��Ҫֱ�ӵ���
	 * udp listen
	 * udp����� 0.0.0.0 wPort ���Բ�֧�ֶ����� ��ͬip ��ͬport�ļ���
	 * @param szIp ������ip
	 * @param wPort �����Ķ˿�
	 * @param pSessionMaker session��maker
	 * @param pOStream 
	 */
	void UdpListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	/**
	 * @brief 
	 * 
	 * ��io�߳�ִ�� ��Ҫֱ�ӵ���
	 * udp���ӵ�Ŀ��ip port
	 * @param szIp 
	 * @param wPort 
	 * @param pSession �󶨵�session
	 * @param pOStream 
	 */
	void UdpConnect(const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

	/**
	 * @brief 
	 * 
	 * ��io�߳�ִ�� ��Ҫֱ�ӵ���
	 * tcp listen
	 * @param szIp ������ip
	 * @param wPort �����Ķ˿�
	 * @param pSessionMaker session��maker
	 * @param pOStream 
	 */
	void TcpListen(const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	/**
	 * @brief 
	 * 
	 * ��io�߳�ִ�� ��Ҫֱ�ӵ���
	 * udp ���ӵ�Ŀ��ip port
	 * @param szIp 
	 * @param wPort 
	 * @param pSession �󶨵�session
	 * @param pOStream 
	 */
	void TcpConnect(const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

	/**
	 * @brief 
	 * 
	 * udp listen �¼� Ͷ�ݵ�io�߳�ִ��
	 */
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

	/**
	 * @brief 
	 * 
	 * udp connect�¼� Ͷ�ݵ�io�߳�ִ��
	 */
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
			UdpConnect(m_szIp.c_str(), m_wPort, m_pSession, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		ISession* m_pSession;
	};

	/**
	 * @brief 
	 * 
	 * tcp listen�¼� Ͷ�ݵ�io�߳�ִ��
	 */
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

	/**
	 * @brief 
	 * 
	 * tcp connect �¼� Ͷ�ݵ�io�߳�ִ��
	 */
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

	std::stringstream* GetStream();
	void PushLog(std::stringstream*);
};


#endif //!__FXNET_INTERFACE_H__
