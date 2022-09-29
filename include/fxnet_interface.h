#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"

#include <deque>
#include <string>

namespace FXNET
{
	class UDPListen : public IOEventBase
	{
	public:
		UDPListen(const char* szIp, unsigned short wPort)
			: m_szIp(szIp)
			, m_wPort(wPort)
		{}
		virtual void operator ()(std::ostream* pOStream)
		{

		}
	protected:
	private:
		std::string m_szIp;
		unsigned short m_wPort;
	};

	void StartIOModule();

	void SwapEvent(std::deque<MessageEventBase*>& refDeque);

	void UdpListen(const char* szIp, unsigned short wPort);
};


#endif //!__FXNET_INTERFACE_H__
