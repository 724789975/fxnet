#ifndef __UDP_CONNECTOR_H__
#define __UDP_CONNECTOR_H__

#include "../include/connector_socket.h"
#include "../include/error_code.h"
#include "buff_contral.h"


#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif //_WIN32

#include <set>

namespace FXNET
{
	class CUdpConnector : public CConnectorSocket
	{
	public:
		class IOReadOperation : public IOOperationBase
		{
		public:
			friend class CUdpConnector;
			virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
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
			friend class CUdpConnector;
			virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			sockaddr_in m_stRemoteAddr;
#endif // _WIN32
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CUdpConnector;
			virtual ErrorCode operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		};

		class UDPOnRecvOperator : public OnRecvOperator
		{
		public:
			UDPOnRecvOperator(CUdpConnector& refUdpConnector);
			virtual ErrorCode operator() (char* szBuff, unsigned short wSize, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPOnConnectedOperator : public OnConnectedOperator
		{
		public:
			UDPOnConnectedOperator(CUdpConnector& refUdpConnector);
			virtual ErrorCode operator() (std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPRecvOperator : public RecvOperator
		{
		public:
			UDPRecvOperator(CUdpConnector& refUdpConnector);
			virtual ErrorCode operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream);

#ifdef _WIN32
			UDPRecvOperator& SetIOReadOperation(IOReadOperation* pReadOperation);
#endif // _WIN32

		private:
			CUdpConnector& m_refUdpConnector;
#ifdef _WIN32
			IOReadOperation* m_pReadOperation;
#endif // _WIN32
		};

		class UDPSendOperator : public SendOperator
		{
		public:
			UDPSendOperator(CUdpConnector& refUdpConnector);
			virtual ErrorCode operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPReadStreamOperator : public ReadStreamOperator
		{
		public:
			UDPReadStreamOperator(CUdpConnector& refUdpConnector);
			virtual int operator() (std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		friend class IOReadOperation;
		friend class IOErrorOperation;
		friend class CUdpListener;

		CUdpConnector(ISession* pSession);
		~CUdpConnector();

		virtual const char* Name()const { return "CUdpConnector"; }

		ErrorCode Init(std::ostream* pOStream, int dwState);

		virtual ErrorCode Update(double dTimedouble, std::ostream* pOStream);

		const sockaddr_in& GetRemoteAddr()const { return m_stRemoteAddr; }
		CUdpConnector& SetRemoteAddr(const sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		ErrorCode Connect(sockaddr_in address, std::ostream* pOStream);

		void Close(std::ostream* pOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOWriteOperation& NewWriteOperation();
		virtual IOErrorOperation& NewErrorOperation(const ErrorCode& refError);

		virtual ErrorCode SendMessage(std::ostream* pOStream);
#ifdef _WIN32
		/**
		 * @brief 提交一个recv的OVERLAPPED
		 * 
		 * note 只提交一个 防止中间有错误导致崩溃
		 * @param pOStream 
		 * @return CUdpConnector& 
		 */
		CUdpConnector& PostRecv(std::ostream* pOStream);
		ErrorCode PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream);
#endif // _WIN32

		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		ErrorCode Connect(NativeSocketType hSock, const sockaddr_in& address, std::ostream* pOStream);
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
