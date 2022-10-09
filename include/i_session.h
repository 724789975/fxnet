#ifndef __ISESSION_H__
#define __ISESSION_H__


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

#endif //!__ISESSION_H__
