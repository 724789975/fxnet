#ifndef __ISESSION_H__
#define __ISESSION_H__

#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

#include <string>

class INetWorkStream;
class MessageEventBase;

namespace FXNET
{
	//session创建早于套接字
	//销毁晚于套接字
	class ISession
	{
	public:
#ifdef _WIN32
		typedef SOCKET NativeSocketType;
#else //_WIN32
		typedef int NativeSocketType;
#endif //_WIN32

		virtual ~ISession() {}
		void SetSock(NativeSocketType hSock) { m_hSock = hSock; }
		NativeSocketType& NativeSocket() { return m_hSock; }
		virtual ISession& Send(const char* szData, unsigned int dwLen) = 0;
		virtual ISession& OnRecv(const char* szData, unsigned int dwLen) = 0;

		virtual void OnConnected(std::ostream* pOStream) = 0;

		virtual INetWorkStream& GetSendBuff() = 0;
		virtual INetWorkStream& GetRecvBuff() = 0;

		virtual MessageEventBase* NewRecvMessageEvent(std::string& refData) = 0;
		virtual MessageEventBase* NewConnectedEvent() = 0;
	protected:
		NativeSocketType m_hSock;
	private:
	};

	class SessionMaker
	{
	public:
		virtual ~SessionMaker() {}

		virtual ISession* operator()() = 0;
	};


}

#endif //!__ISESSION_H__
