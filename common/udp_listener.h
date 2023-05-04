#ifndef __UDP_LISTENER_H__
#define __UDP_LISTENER_H__

#include "../include/sliding_window_def.h"
#include "../include/i_session.h"
#include "../include/error_code.h"
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
	 * notice Ϊ��ֹ�յ�ͬһ����ַ�����Ķ����Ϣ windows��ÿ��Ͷ��һ��WSARecvfrom
	 * linux�»��ڴ���ʱ �ж��ǲ���ͬ������
	 * ����windows�¾ͻ���������� �������ӵ�Ч�ʻ���
	 * ���ǲ�ʹ���ص�io
	 */
	class CUdpListener : public CListenSocket
	{
	public:
		friend class UDPListenIOReadOperation;

		CUdpListener(SessionMaker* pMaker);
		virtual const char* Name()const { return "CUdpListener"; }
		virtual ErrorCode Update(double dTimedouble, std::ostream* pOStream);

		ErrorCode Listen(const char* szIp, unsigned short wPort, std::ostream* pOStream);

		virtual void Close(std::ostream* pOStream);

		virtual IOOperationBase& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(const ErrorCode& refError);

		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
	protected:
	private:
		static const unsigned int UDP_ACCEPT_HASH_SIZE = 64;
		static const unsigned int UDP_ACCEPT_MAX_SIZE = 2048;

		CUdpListener& OnClientConnected(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);

#ifdef _WIN32
		ErrorCode PostAccept(std::ostream* pOStream);
#else
		struct AcceptReq
		{
			AcceptReq* m_pNext;
			sockaddr_in addr;
			NativeSocketType m_oAcceptSocket;
		};
		//��Щ����ֻ��Ϊ�˷�ֹ���ӹ�����ʱ���ظ�����
		unsigned int GenerateAcceptHash(const sockaddr_in & addr);
		AcceptReq* GetAcceptReq(const sockaddr_in & addr);
		void AddAcceptReq(AcceptReq* pReq);
		void RemoveAcceptReq(const sockaddr_in & addr);

		//��ֹͬһ���� ͬһʱ��������
		AcceptReq* m_arroAcceptQueue[UDP_ACCEPT_HASH_SIZE];
		CCasLockQueue<AcceptReq, UDP_ACCEPT_MAX_SIZE> m_oAcceptPool;
#endif // _WIN32

		SessionMaker* m_pSessionMaker;

	};


};

#endif // !__UDP_LISTENER_H__

