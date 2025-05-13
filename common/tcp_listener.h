#ifndef __TCP_LISTENER_H__
#define __TCP_LISTENER_H__

#include "../include/sliding_window_def.h"
#include "../include/i_session.h"
#include "../include/error_code.h"
#include "listen_socket.h"
#include "cas_lock_queue.h"

#include <sstream>

#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif  //_WIN32


namespace FXNET
{
	/**
	 * @brief 
	 * CTcpListener
	 * notice 为防止收到同一个地址发来的多个消息 windows下每次投放一个WSARecvfrom
	 * linux下会在处理时 判断是不是同个链接
	 * 这样windows下就会带来副作用 接受连接的效率会变低
	 * 除非不使用重叠io
	 */
	class CTcpListener: public CListenSocket
	{
	public: 
		friend class TCPListenIOAcceptOperation;
		CTcpListener(SessionMaker* pMaker);
		virtual const char* Name()const { return "CTcpListener"; }
		virtual CTcpListener& Update(double dTimedouble, ErrorCode& refError, std::ostream* pOStream);

		CTcpListener& Listen(const char* szIp, unsigned short wPort, ErrorCode& refError, std::ostream* pOStream);

		virtual void Close(std::ostream* pOStream);

		virtual IOOperationBase& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(const ErrorCode& refError);

		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
	protected:
	private:
		static const unsigned int UDP_ACCEPT_HASH_SIZE = 64;
		static const unsigned int UDP_ACCEPT_MAX_SIZE  = 2048;

		CTcpListener& OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);

#ifdef _WIN32
		int								PostAccept(std::ostream* pOStream);
		int								InitAcceptEx(std::ostream* pOStream);

#else
#endif  // _WIN32

#ifdef _WIN32
		LPFN_ACCEPTEX						m_lpfnAcceptEx;
		LPFN_GETACCEPTEXSOCKADDRS			m_lpfnGetAcceptExSockaddrs;
#endif  // _WIN32
		SessionMaker* m_pSessionMaker;

	};


};

#endif  // !__TCP_LISTENER_H__

