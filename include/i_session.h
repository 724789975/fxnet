#ifndef __SESSION_H__
#define __SESSION_H__


namespace FXNET
{
	class ISession
	{
	public:
		virtual ~ISession() {}
		virtual ISession& Send(const char* szData, unsigned int dwLen) = 0;
		virtual ISession& Recv(const char* szData, unsigned int dwLen) = 0;
	private:
	};


}

#endif //!__SESSION_H__
