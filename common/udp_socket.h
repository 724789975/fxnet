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
	};

};

#endif // !__UDP_SOCKET_H__

