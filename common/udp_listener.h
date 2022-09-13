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

		virtual void OnRead();
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

		//这些函数只是为了防止连接过来的时候重复建立
		unsigned int GenerateAcceptHash(const sockaddr_in & addr);
		AcceptReq* GetAcceptReq(const sockaddr_in & addr);
		void AddAcceptReq(AcceptReq* pReq);
		void RemoveAcceptReq(const sockaddr_in & addr);

		//防止同一连接 同一时间请求多次
		AcceptReq* m_arroAcceptQueue[UDP_ACCEPT_HASH_SIZE];
		CCasLockQueue<AcceptReq, UDP_ACCEPT_MAX_SIZE> m_oAcceptPool;

		sockaddr_in m_oAddr;
	};


};

#endif // !__UDP_LISTENER_H__

