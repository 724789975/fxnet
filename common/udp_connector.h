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
		friend class UDPConnectorIOReadOperation;

		class UDPOnRecvOperator : public OnRecvOperator
		{
		public:
			UDPOnRecvOperator(CUdpConnector& refUdpConnector);
			virtual UDPOnRecvOperator& operator() (char* szBuff, unsigned short wSize, ErrorCode& refError, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPOnConnectedOperator : public OnConnectedOperator
		{
		public:
			UDPOnConnectedOperator(CUdpConnector& refUdpConnector);
			virtual UDPOnConnectedOperator& operator() (ErrorCode& refError, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPRecvOperator : public RecvOperator
		{
		public:
			UDPRecvOperator(CUdpConnector& refUdpConnector);
			virtual UDPRecvOperator& operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, ErrorCode& refError, std::ostream* pOStream);

#ifdef _WIN32
			UDPRecvOperator& SetIOReadOperation(UDPConnectorIOReadOperation* pReadOperation);
#endif // _WIN32

			CUdpConnector& m_refUdpConnector;
#ifdef _WIN32
			UDPConnectorIOReadOperation* m_pReadOperation;
#endif // _WIN32
		};

		class UDPSendOperator : public SendOperator
		{
		public:
			UDPSendOperator(CUdpConnector& refUdpConnector);
			virtual UDPSendOperator& operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, ErrorCode& refError, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPReadStreamOperator : public ReadStreamOperator
		{
		public:
			UDPReadStreamOperator(CUdpConnector& refUdpConnector);
			virtual unsigned int operator() (std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		friend class UDPConnectorIOWriteOperation;
		friend class UDPConnectorIOErrorOperation;
		friend class UDPReadStreamOperator;
		friend class UDPConnectorIOReadOperation;
		friend class CUdpListener;

		CUdpConnector(ISession* pSession);
		~CUdpConnector();

		virtual const char* Name()const { return "CUdpConnector"; }

		ErrorCode Init(std::ostream* pOStream, int dwState);

		virtual CUdpConnector& Update(double dTimedouble, ErrorCode& refError, std::ostream* pOStream);

		const sockaddr_in& GetRemoteAddr()const { return m_stRemoteAddr; }
		CUdpConnector& SetRemoteAddr(const sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		CUdpConnector& Connect(sockaddr_in address, ErrorCode& refError, std::ostream* pOStream);

		void Close(std::ostream* pOStream);

		virtual IOOperationBase& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(const ErrorCode& refError);

		virtual CUdpConnector& SendMessage(ErrorCode& refError, std::ostream* pOStream);
#ifdef _WIN32
		/**
		 * @brief 提交一个recv的OVERLAPPED
		 * 
		 * note 只提交一个 防止中间有错误导致崩溃
		 * @param pOStream 
		 * @return CUdpConnector& 
		 */
		CUdpConnector& PostRecv(std::ostream* pOStream);
		CUdpConnector& PostSend(char* pBuff, unsigned short wLen, ErrorCode& refError, std::ostream* pOStream);
#endif // _WIN32

		virtual void OnError(const ErrorCode& refError, std::ostream* pOStream);
		virtual void OnClose(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		CUdpConnector& Connect(NativeSocketType hSock, const sockaddr_in& address, ErrorCode& refError, std::ostream* pOStream);
		sockaddr_in m_stRemoteAddr;

		UDPOnRecvOperator m_funOnRecvOperator;
		UDPOnConnectedOperator m_funOnConnectedOperator;
		UDPRecvOperator m_funRecvOperator;
		UDPSendOperator m_funSendOperator;
		UDPReadStreamOperator m_funReadStreamOperator;
		BufferContral<UDP_WINDOW_BUFF_SIZE, UDP_WINDOW_SIZE> m_oBuffContral;

#ifdef _WIN32
		std::set<IOOperationBase*>		m_setOperation;
#endif // _WIN32

	};
};

#endif // !__UDP_CONNECTOR_H__
