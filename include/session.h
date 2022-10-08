#ifndef __SESSION_H__
#define __SESSION_H__

#include "include/message_header.h"

namespace FXNET
{
	class CSession
	{
#ifdef _WIN32
		typedef SOCKET NativeSocketType;
#else //_WIN32
		typedef int NativeSocketType;
#endif //_WIN32
	public:
		CSession& Send(const char* szData, unsigned int dwLen);
	private:
		/* data */
		NativeSocketType m_hSock;

		BinaryMessageParse m_oParse;
		
	};


}

#endif //!__SESSION_H__
