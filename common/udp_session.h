#include "include/session.h"
#include "include/sliding_window_def.h"
#include "buff_contral.h"

#ifndef __UDP_SESSION_H__
#define __UDP_SESSION_H__

namespace FXNET
{

	class CUdpSession : public CSession
	{
	public:
		CUdpSession();

		~CUdpSession();

	private:

		BufferContral<UDP_SEND_WINDOW_BUFF_SIZE, UDP_SEND_WINDOW_SIZE, UDP_RECV_WINDOW_BUFF_SIZE, UDP_RECV_WINDOW_SIZE> m_oBuffContral;

	};


}

#endif	//!__UDP_SESSION_H__