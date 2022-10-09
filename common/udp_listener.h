#ifndef __UDP_LISTENER_H__
#define __UDP_LISTENER_H__

#include "include/sliding_window_def.h"
#include "udp_socket.h"
#include "listen_socket.h"
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
	class CUdpListener : public CListenSocket
	{
	public:
		class IOReadOperation : public IOOperationBase
		{
		public:
			friend class CUdpListener;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			sockaddr_in m_stRemoteAddr;
#endif // _WIN32
			char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CUdpListener;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		};

		virtual const char* Name()const { return "CUdpListener"; }
		virtual int Update(double dTimedouble, std::ostream* pOStream);

		int Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOErrorOperation& NewErrorOperation(int dwError);

		virtual void OnRead(std::ostream* pOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(int dwError, std::ostream* pOStream);

	protected:
	private:
		static const unsigned int UDP_ACCEPT_HASH_SIZE = 64;
		static const unsigned int UDP_ACCEPT_MAX_SIZE = 2048;

		CUdpListener& OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);

#ifdef _WIN32
		int PostAccept(std::ostream* pOStream);
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

	};


};

#endif // !__UDP_LISTENER_H__

