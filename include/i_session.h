#ifndef __ISESSION_H__
#define __ISESSION_H__

#include "error_code.h"
#include "socket_base.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

#include <string>

class INetWorkStream;
class MessageEventBase;
class MessageRecvEventBase;

namespace FXNET
{
	class CNetStreamPackage;
	class CConnectorSocket;
	/**
	 * @brief 
	 * 
	 * session
	 * �������ڹ����׽���
	 * ����ʱ���׽��ֽ���� ����socket�����¼�Ͷ�ݵ�ioִ��
	 */
	class ISession
	{
	public:
#ifdef _WIN32
		typedef SOCKET NativeSocketType;
#else //_WIN32
		typedef int NativeSocketType;
#endif //_WIN32
		
		ISession() : m_opSock(0) {}
		virtual ~ISession() {}
		void SetSock(CConnectorSocket* opSock) { m_opSock = opSock; }
		CConnectorSocket* GetSocket() { return m_opSock; }

		/**
		 * @brief 
		 * 
		 * ��Ҫ���͵�����Ͷ�ݵ�io�߳� Ȼ����뻺����
		 * @param szData 
		 * @param dwLen 
		 * @return ISession& ��������
		 */
		virtual ISession& Send(const char* szData, unsigned int dwLen, std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * �����յ�����ʱ�Ĵ���
		 * @param szData 
		 * @param dwLen 
		 * @return ISession& 
		 */
		virtual ISession& OnRecv(CNetStreamPackage& refPackage, std::ostream* pOStream) = 0;

		/**
		 * @brief 
		 * 
		 * �����ӽ���ʱ
		 * @param pOStream 
		 */
		virtual void OnConnected(std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * ��������ʱ
		 * @param dwError ������
		 * @param pOStream 
		 */
		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * �����ӹر�ʱ
		 * @param pOStream 
		 */
		virtual void OnClose(std::ostream* pOStream) = 0;

		/**
		 * @brief Get the Send Buff object
		 * 
		 * ��ȡ���ͻ���
		 * @return INetWorkStream& 
		 */
		virtual INetWorkStream& GetSendBuff() = 0;
		/**
		 * @brief Get the Recv Buff object
		 * 
		 * ��ȡ���ջ���
		 * @return INetWorkStream& 
		 */
		virtual INetWorkStream& GetRecvBuff() = 0;

		/**
		 * @brief 
		 * 
		 * ���������¼� ��io�̴߳��� �����߳�ִ��
		 * @param refData 
		 * @return MessageEventBase* 
		 */
		virtual MessageRecvEventBase* NewRecvMessageEvent() = 0;
		/**
		 * @brief 
		 * 
		 * ������������¼� ��io�̴߳��� �����߳�ִ��
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewConnectedEvent() = 0;
		/**
		 * @brief 
		 * 
		 * ���������¼� ��io�̴߳��� �����߳�ִ��
		 * @param dwError ������
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewErrorEvent(const ErrorCode& refError) = 0;
		/**
		 * @brief 
		 * 
		 * �������ӹر��¼� ��io�̴߳��� �����߳�ִ��
		 * @return MessageEventBase* 
		 */
		virtual MessageEventBase* NewCloseEvent() = 0;

		virtual MessageEventBase* NewOnSendEvent(int dwLen) = 0;
	protected:
		CConnectorSocket* m_opSock;
	private:
	};

	class SessionMaker
	{
	public:
		virtual ~SessionMaker() {}

		virtual ISession* operator()() = 0;
	};


}

#endif //!__ISESSION_H__
