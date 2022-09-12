#ifndef __UDP_LISTENER_H__
#define __UDP_LISTENER_H__

#include "udp_socket.h"
#include "cas_lock_queue.h"

#include <sstream>


namespace FXNET
{
	class CUdpListener : public CUdpSocket
	{
	public:
		virtual const char* Name()const { return "CUdpListener"; }
		virtual CSocketBase& Update(double dTime);

		int Listen(const char* szIp, unsigned short wPort, std::ostream& refOStream);
	protected:
	private:

		static const unsigned int UDP_ACCEPT_HASH_SIZE = 64;
		static const unsigned int UDP_ACCEPT_MAX_SIZE = 2048;

		struct AcceptReq
		{
			AcceptReq* m_pNext;
			sockaddr_in addr;
			NativeSocketType m_oAcceptSocket;
		};

		CCasLockQueue<AcceptReq, UDP_ACCEPT_MAX_SIZE> s_oAcceptPool;

		sockaddr_in m_oAddr;
	};


};

#endif // !__UDP_LISTENER_H__

