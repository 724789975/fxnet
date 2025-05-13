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
	 * notice Ϊ��ֹ�յ�ͬһ����ַ�����Ķ����Ϣ windows��ÿ��Ͷ��һ��WSARecvfrom
	 * linux�»��ڴ���ʱ �ж��ǲ���ͬ������
	 * ����windows�¾ͻ���������� �������ӵ�Ч�ʻ���
	 * ���ǲ�ʹ���ص�io
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

