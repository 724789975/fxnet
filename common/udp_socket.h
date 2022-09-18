#ifndef __UDP_SOCKET_H__
#define __UDP_SOCKET_H__

#include "socket_base.h"
#include "buff_contral.h"

namespace FXNET
{
	class CUdpSocket : public ISocketBase
	{
	public:
		virtual const char* Name()const { return "CUdpSocket"; }
		virtual ISocketBase& Update(double dTime);
	protected:
	private:
		BufferContral<UDP_SEND_WINDOW_BUFF_SIZE, UDP_SEND_WINDOW_SIZE, UDP_RECV_WINDOW_BUFF_SIZE, UDP_RECV_WINDOW_SIZE> m_oBuffContral;
	};

};

#endif // !__UDP_SOCKET_H__

