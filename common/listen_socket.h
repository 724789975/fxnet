#ifndef __LISTEN_SOCKET_H__
#define __LISTEN_SOCKET_H__

#include "../include/socket_base.h"
#include "buff_contral.h"

namespace FXNET
{
	class CListenSocket : public ISocketBase
	{
	public:
		virtual const char* Name()const { return "CListenSocket"; }
	protected:
	private:
	};

};

#endif // !__LISTEN_SOCKET_H__

