#ifndef __UDP_LISTENER_H__
#define __UDP_LISTENER_H__

#include "include/sliding_window_def.h"
#include "udp_socket.h"
#include "cas_lock_queue.h"

#include <sstream>

#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif //_WIN32


namespace FXNET
{
	/**
	 * @brief 
	 * CUdpListener
	 * notice 为防止收到同一个地址发来的多个消息 windows下每次投放一个WSARecvfrom
	 * linux下会在处理时 判断是不是同个链接
	 * 这样windows下就会带来副作用 接受连接的效率会变低
	 * 除非不使用重叠io
	 */
	class CUdpListener : public CUdpSocket
	{
	public:
		class IOReadOperation : public IOOperationBase
		{
		public:
			friend class CUdpListener;
			virtual IOOperationBase& operator()(CSocketBase& refSocketBase, std::ostream& refOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			char m_szRecvBuff[UDP_RECV_WINDOW_BUFF_SIZE];
			sockaddr_in m_stRemoteAddr;
#endif // _WIN32

		};

		virtual const char* Name()const { return "CUdpListener"; }
		virtual CSocketBase& Update(double dTime);

		int Listen(const char* szIp, unsigned short wPort, std::ostream& refOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();

		virtual void OnRead(std::ostream& refOStream);
		virtual void OnWrite(std::ostream& refOStream);
	protected:
	private:
		static const unsigned int UDP_ACCEPT_HASH_SIZE = 64;
		static const unsigned int UDP_ACCEPT_MAX_SIZE = 2048;



#ifdef _WIN32
		int PostAccept(std::ostream& refOStream);
#else
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
#endif // _WIN32


		sockaddr_in m_oAddr;
	};


};

#endif // !__UDP_LISTENER_H__

