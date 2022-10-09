#ifndef __SESSION_H__
#define __SESSION_H__


namespace FXNET
{
	class ISession
	{
	public:
		ISession& Send(const char* szData, unsigned int dwLen) = 0;
		ISession& Recv(const char* szData, unsigned int dwLen) = 0;
	private:
	};


}

#endif //!__SESSION_H__
