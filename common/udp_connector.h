#ifndef __UDP_CONNECTOR_H__
#define __UDP_CONNECTOR_H__

#include "udp_socket.h"
#include "../include/connector_socket.h"
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
			friend class CUdpConnector;
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
			friend class CUdpConnector;
			virtual int operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream);
		};

		class UDPOnRecvOperator : public OnRecvOperator
		{
		public:
			UDPOnRecvOperator(CUdpConnector& refUdpConnector);
			virtual int operator() (char* szBuff, unsigned short wSize, std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPOnConnectedOperator : public OnConnectedOperator
		{
		public:
			UDPOnConnectedOperator(CUdpConnector& refUdpConnector);
			virtual int operator() (std::ostream* pOStream);
		private:
			CUdpConnector& m_refUdpConnector;
		};

		class UDPRecvOperator : public RecvOperator
		{
		public:
			UDPRecvOperator(CUdpConnector& refUdpConnector);
			virtual int operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* refOStream);

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
			virtual int operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream);
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

		int Init(std::ostream* pOStream, int dwState);

		virtual int Update(double dTimedouble, std::ostream* pOStream);

		sockaddr_in& GetRemoteAddr() { return m_stRemoteAddr; }
		CUdpConnector& SetRemoteAddr(const sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		int Connect(sockaddr_in address, std::ostream* pOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOWriteOperation& NewWriteOperation();
		virtual IOErrorOperation& NewErrorOperation(int dwError);

		virtual CUdpConnector& SendMessage(std::ostream* pOStream);
#ifdef _WIN32
		CUdpConnector& PostRecv(std::ostream* pOStream);
		int PostSend(char* pBuff, unsigned short wLen, std::ostream* pOStream);
#endif // _WIN32

		virtual void OnRead(std::ostream* refOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(int dwError, std::ostream* pOStream);
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

#ifdef _WIN32
		//windows下 如果删除了事件 会有内存泄露
		//因此需要将产生的事件记录记录下来 用于删除
		//error不需要 error为手动产生
		std::set<IOOperationBase*> m_setIOOperations;
#endif // _WIN32
	};
};

#endif // !__UDP_CONNECTOR_H__
