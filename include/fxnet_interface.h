#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"
#include "message_queue.h"
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
	void StartIOModule(MessageEventQueue* pQueue);

	/**
	 * @brief 
	 * 
	 * ������־�߳�
	 */
	void StartLogModule();

	/**
	 * @brief Get the Fx Io Module Index object
	 * 
	 * @return unsigned int 
	 */
	unsigned int GetFxIoModuleIndex();

	/**
	 * @brief 
	 * ���߳�ִ��
	 * @param pStrstream 
	 */
	void ProcSignelThread(std::stringstream*& pStrstream);

	/**
	 * @brief 
	 * 
	 * ��io�߳�Ͷ���¼�
	 * @param pEvent 
	 */
	void PostEvent(unsigned int dwIoModuleIndex, IOEventBase* pEvent);

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
	void UdpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
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
	void UdpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

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
	void TcpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
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
	void TcpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

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
			, m_dwIOModuleIndex(GetFxIoModuleIndex())
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPListen, this);
			UdpListen(m_dwIOModuleIndex, m_szIp.c_str(), m_wPort, m_pSessionMaker, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		SessionMaker* m_pSessionMaker;
		unsigned int m_dwIOModuleIndex;
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
			, m_dwIOModuleIndex(GetFxIoModuleIndex())
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnect, this);
			UdpConnect(m_dwIOModuleIndex, m_szIp.c_str(), m_wPort, m_pSession, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		ISession* m_pSession;
		unsigned int m_dwIOModuleIndex;
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
			, m_dwIOModuleIndex(GetFxIoModuleIndex())
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPListen, this);
			TcpListen(m_dwIOModuleIndex, m_szIp.c_str(), m_wPort, m_pSessionMaker, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		SessionMaker* m_pSessionMaker;
		unsigned int m_dwIOModuleIndex;
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
			, m_dwIOModuleIndex(GetFxIoModuleIndex())
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(TCPConnect, this);
			TcpConnect(m_dwIOModuleIndex, m_szIp.c_str(), m_wPort, m_pSession, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
		ISession* m_pSession;
		unsigned int m_dwIOModuleIndex;
	};

	std::stringstream* GetStream();
	void PushLog(std::stringstream*& pStrstream);
};


#endif //!__FXNET_INTERFACE_H__
