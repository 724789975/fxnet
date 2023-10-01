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
	 * 启动io线程
	 */
	void StartIOModule(MessageEventQueue* pQueue);

	/**
	 * @brief 
	 * 
	 * 启动日志线程
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
	 * 单线程执行
	 * @param pStrstream 
	 */
	void ProcSignelThread(std::stringstream*& pStrstream);

	/**
	 * @brief 
	 * 
	 * 向io线程投递事件
	 * @param pEvent 
	 */
	void PostEvent(unsigned int dwIoModuleIndex, IOEventBase* pEvent);

	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * udp listen
	 * udp会监听 0.0.0.0 wPort 所以不支持多网卡 不同ip 相同port的监听
	 * @param szIp 监听的ip
	 * @param wPort 监听的端口
	 * @param pSessionMaker session的maker
	 * @param pOStream 
	 */
	void UdpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * udp连接到目的ip port
	 * @param szIp 
	 * @param wPort 
	 * @param pSession 绑定的session
	 * @param pOStream 
	 */
	void UdpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * tcp listen
	 * @param szIp 监听的ip
	 * @param wPort 监听的端口
	 * @param pSessionMaker session的maker
	 * @param pOStream 
	 */
	void TcpListen(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, SessionMaker* pSessionMaker, std::ostream* pOStream);
	/**
	 * @brief 
	 * 
	 * 在io线程执行 不要直接调用
	 * udp 连接到目的ip port
	 * @param szIp 
	 * @param wPort 
	 * @param pSession 绑定的session
	 * @param pOStream 
	 */
	void TcpConnect(unsigned int dwIOModuleIndex, const char* szIp, unsigned short wPort, ISession* pSession, std::ostream* pOStream);

	/**
	 * @brief 
	 * 
	 * udp listen 事件 投递到io线程执行
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
	 * udp connect事件 投递到io线程执行
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
	 * tcp listen事件 投递到io线程执行
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
	 * tcp connect 事件 投递到io线程执行
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
