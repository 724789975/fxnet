#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"

#include <deque>
#include <string>

namespace FXNET
{

	void StartIOModule();

	void PostEvent(IOEventBase* pEvent);
	void SwapEvent(std::deque<MessageEventBase*>& refDeque);

	//在io线程执行 不要直接调用
	void UdpListen(const char* szIp, unsigned short wPort, std::ostream* pOStream);
	void UdpConnect(const char* szIp, unsigned short wPort, std::ostream* pOStream);

	class UDPListen : public IOEventBase
	{
	public:
		UDPListen(const char* szIp, unsigned short wPort)
			: m_szIp(szIp)
			, m_wPort(wPort)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPListen, this);
			UdpListen(m_szIp.c_str(), m_wPort, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
	};

	class UDPConnect : public IOEventBase
	{
	public:
		UDPConnect(const char* szIp, unsigned short wPort)
			: m_szIp(szIp)
			, m_wPort(wPort)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{
			DELETE_WHEN_DESTRUCT(UDPConnect, this);
			UdpConnect(m_szIp.c_str(), m_wPort, pOStream);
		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
	};
};


#endif //!__FXNET_INTERFACE_H__
