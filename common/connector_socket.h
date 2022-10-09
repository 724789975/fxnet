#ifndef __CONNECTOR_SOCKET_H__
#define __CONNECTOR_SOCKET_H__

#include "socket_base.h"
#include "buff_contral.h"

namespace FXNET
{
	class CConnectorSocket : public ISocketBase
	{
	public:
		virtual const char* Name()const { return "CConnectorSocket"; }
	protected:
	private:
	};

};

#endif // !__CONNECTOR_SOCKET_H__

