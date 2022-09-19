#ifndef __UDP_CONNECTOR_H__
#define __UDP_CONNECTOR_H__

#include "udp_socket.h"

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
			char m_szRecvBuff[UDP_RECV_WINDOW_BUFF_SIZE];
		};

		class IOErrorOperation : public IOOperationBase
		{
		public:
			friend class CUdpListener;
			virtual void operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream);
		};

		virtual const char* Name()const { return "CUdpConnector"; }
		virtual ISocketBase& Update(double dTimedouble, std::ostream* pOStream);


		virtual IOReadOperation& NewReadOperation();
		virtual IOOperationBase& NewWriteOperation();
		virtual IOOperationBase& NewErrorOperation(int dwError);

		virtual void OnRead(std::ostream* refOStream);
		virtual void OnWrite(std::ostream* pOStream);
		virtual void OnError(std::ostream* pOStream);
	protected:
	private:
	};
};

#endif // !__UDP_CONNECTOR_H__
