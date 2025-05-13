#ifndef __TCP_CONNECTOR_H__
#define __TCP_CONNECTOR_H__

#include "../include/connector_socket.h"
#include "../include/error_code.h"
#include "buff_contral.h"


#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif  //_WIN32

#include <set>

namespace FXNET
{
	class CTcpConnector: public CConnectorSocket
	{
	public: 
		friend class TCPConnectorIOReadOperation;
		friend class TCPConnectorIOWriteOperation;
		friend class TCPConnectorIOErrorOperation;
		friend class CTcpListener;

		CTcpConnector(ISession* pSession);
		~CTcpConnector();

		SET_CLASS_NAME(CTcpConnector);

		int Init(std::ostream* pOStream, int dwState);

		virtual CTcpConnector& Update(double dTimedouble, ErrorCode& refError, std::ostream* pOStream);

		const sockaddr_in& GetRemoteAddr()const { return m_stRemoteAddr; }
		CTcpConnector& SetRemoteAddr(const sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		CTcpConnector& Connect(sockaddr_in address, ErrorCode& refError, std::ostream* pOStream);

		void Close(std::ostream* pOStream);

		virtual IOOperationBase& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(const ErrorCode& refError);

		virtual CTcpConnector& SendMessage(ErrorCode& refError, std::ostream* pOStream);
#ifdef _WIN32
		/**
		 * @brief 提交一个recv的OVERLAPPED
		 * 
		 * note 只提交一个 防止中间有错误导致崩溃
		 * @param pOStream 
		 * @return CTcpConnector& 
		 */
		CTcpConnector& PostRecv(std::ostream* pOStream);
		CTcpConnector& PostSend(ErrorCode& refError, std::ostream* pOStream);
#endif  // _WIN32

		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		CTcpConnector& Connect(NativeSocketType hSock, const sockaddr_in& address, ErrorCode& refError, std::ostream* pOStream);
		sockaddr_in m_stRemoteAddr;

#ifndef _WIN32
		bool m_bConnecting;
#else
		std::set<IOOperationBase*>		m_setOperation;
#endif  // _WIN32

	};
};

#endif  // !__TCP_CONNECTOR_H__
