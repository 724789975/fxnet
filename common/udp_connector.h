#ifndef __UDP_CONNECTOR_H__
#define __UDP_CONNECTOR_H__

#include "udp_socket.h"
#include "buff_contral.h"

#ifndef _WIN32
#include<netinet/in.h>
#include<arpa/inet.h>
#endif //_WIN32

namespace FXNET
{
	class CUdpConnector : public CUdpSocket
	{
	public:
		class IOReadOperation : public IOOperationBase
		{
		public:
			friend class CUdpConnector;
			virtual void operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream);
		private:
#ifdef _WIN32
			WSABUF m_stWsaBuff;
			sockaddr_in m_stRemoteAddr;
#endif // _WIN32
			char m_szRecvBuff[UDP_WINDOW_BUFF_SIZE];
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CUdpConnector;
			virtual void operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream);
		};

		friend class IOReadOperation;
		friend class IOErrorOperation;
		friend class CUdpListener;

		CUdpConnector();

		virtual const char* Name()const { return "CUdpConnector"; }

		int Init(std::ostream* pOStream, int dwState);

		virtual ISocketBase& Update(double dTimedouble, std::ostream* pOStream);

		sockaddr_in& GetRemoteAddr() { return m_stRemoteAddr; }
		CUdpConnector& SetRemoteAddr(sockaddr_in& refAddr) { m_stRemoteAddr = refAddr; return *this; }

		int Connect(sockaddr_in address, std::ostream* pOStream);

		virtual IOReadOperation& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(int dwError);

		virtual void OnRead(std::ostream* refOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(std::ostream* pOStream);
		void OnConnected(std::ostream* pOStream);
	protected:
	private:
		int Connect(NativeSocketType hSock, sockaddr_in address, std::ostream* pOStream);
		sockaddr_in m_stRemoteAddr;
		BufferContral<UDP_WINDOW_BUFF_SIZE, UDP_WINDOW_SIZE> m_oBuffContral;
	};
};

#endif // !__UDP_CONNECTOR_H__
