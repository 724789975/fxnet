#ifndef __UDP_CONNECTOR_H__
#define __UDP_CONNECTOR_H__

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
			char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
		};

		class IOWriteOperation : public IOOperationBase
		{
		public:
			friend class CTcpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			sockaddr_in m_stRemoteAddr;
#endif // _WIN32
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CTcpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		};

		class UDPOnRecvOperator : public OnRecvOperator
		{
		public:
			UDPOnRecvOperator(CTcpConnector& refTcpConnector);
			virtual int operator() (char* szBuff, unsigned short wSize, std::ostream* pOStream);
		private:
			CTcpConnector& m_refTcpConnector;
		};

		class UDPOnConnectedOperator : public OnConnectedOperator
		{
		public:
			UDPOnConnectedOperator(CTcpConnector& refTcpConnector);
			virtual int operator() (std::ostream* pOStream);
		private:
			CTcpConnector& m_refTcpConnector;
		};

		class UDPRecvOperator : public RecvOperator
		{
		public:
			UDPRecvOperator(CTcpConnector& refTcpConnector);
			virtual int operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream);

#ifdef _WIN32
			UDPRecvOperator& SetIOReadOperation(IOReadOperation* pReadOperation);
#endif // _WIN32

		private:
			CTcpConnector& m_refTcpConnector;
#ifdef _WIN32
			IOReadOperation* m_pReadOperation;
#endif // _WIN32
		};

		class UDPSendOperator : public SendOperator
		{
		public:
			UDPSendOperator(CTcpConnector& refTcpConnector);
			virtual int operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream);
		private:
			CTcpConnector& m_refTcpConnector;
		};

		class UDPReadStreamOperator : public ReadStreamOperator
		{
		public:
			UDPReadStreamOperator(CTcpConnector& refTcpConnector);
			virtual int operator() (std::ostream* pOStream);
		private:
			CTcpConnector& m_refTcpConnector;
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

		virtual CTcpConnector& SendMessage(std::ostream* pOStream);
#ifdef _WIN32
		/**
		 * @brief 提交一个recv的OVERLAPPED
		 * 
		 * note 只提交一个 防止中间有错误导致崩溃
		 * @param pOStream 
		 * @return CTcpConnector& 
		 */
		CTcpConnector& PostRecv(std::ostream* pOStream);
		int PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream);
#endif // _WIN32

		virtual void OnRead(std::ostream* refOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(int dwError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		int Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);
		sockaddr_in m_stRemoteAddr;

		UDPOnRecvOperator m_funOnRecvOperator;
		UDPOnConnectedOperator m_funOnConnectedOperator;
		UDPRecvOperator m_funRecvOperator;
		UDPSendOperator m_funSendOperator;
		UDPReadStreamOperator m_funReadStreamOperator;
		BufferContral<UDP_WINDOW_BUFF_SIZE, UDP_WINDOW_SIZE> m_oBuffContral;
	};
};

#endif // !__UDP_CONNECTOR_H__
