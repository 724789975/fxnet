#ifndef __TCP_CONNECTOR_H__
#define __TCP_CONNECTOR_H__

#include "include/connector_socket.h"
#include "buff_contral.h"


#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif //_WIN32

#include <set>

namespace FXNET
{
	class CTcpConnector : public CConnectorSocket
	{
	public:
		class IOReadOperation : public IOOperationBase
		{
		public:
			friend class CTcpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			sockaddr_in m_stRemoteAddr;
			unsigned int m_dwLen;
#endif // _WIN32
		};

		class IOWriteOperation : public IOOperationBase
		{
		public:
			friend class CTcpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			std::string m_strData;
#endif // _WIN32
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CTcpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		};

		friend class IOReadOperation;
		friend class IOErrorOperation;
		friend class CTcpListener;

		CTcpConnector(ISession* pSession);
		~CTcpConnector();

		virtual const char* Name()const { return "CTcpConnector"; }

		int Init(std::ostream* pOStream, int dwState);

		virtual int Update(double dTimedouble, std::ostream* pOStream);

		const sockaddr_in& GetRemoteAddr()const { return m_stRemoteAddr; }
		CTcpConnector& SetRemoteAddr(const sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		int Connect(sockaddr_in address, std::ostream* pOStream);

		void Close(std::ostream* pOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOWriteOperation& NewWriteOperation();
		virtual IOErrorOperation& NewErrorOperation(int dwError);

		virtual int SendMessage(std::ostream* pOStream);
#ifdef _WIN32
		/**
		 * @brief 提交一个recv的OVERLAPPED
		 * 
		 * note 只提交一个 防止中间有错误导致崩溃
		 * @param pOStream 
		 * @return CTcpConnector& 
		 */
		CTcpConnector& PostRecv(std::ostream* pOStream);
		int PostSend(std::ostream* pOStream);
#endif // _WIN32

		virtual void OnRead(std::ostream* pOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(int dwError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		int Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);
		sockaddr_in m_stRemoteAddr;

#ifndef _WIN32
		bool m_bConnecting;
#endif // _WIN32

	};
};

#endif // !__TCP_CONNECTOR_H__
