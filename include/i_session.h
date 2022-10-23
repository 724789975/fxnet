#ifndef __ISESSION_H__
#define __ISESSION_H__

#include "include/socket_base.h"

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
		void SetSock(ISocketBase* opSock) { m_opSock = opSock; }
		ISocketBase* GetSocket() { return m_opSock; }
		virtual ISession& Send(const char* szData, unsigned int dwLen) = 0;
		virtual ISession& OnRecv(const char* szData, unsigned int dwLen) = 0;

		virtual void OnConnected(std::ostream* pOStream) = 0;
		virtual void OnError(int dwError, std::ostream* pOStream) = 0;
		virtual void OnClose(std::ostream* pOStream) = 0;

		virtual INetWorkStream& GetSendBuff() = 0;
		virtual INetWorkStream& GetRecvBuff() = 0;

		virtual MessageEventBase* NewRecvMessageEvent(std::string& refData) = 0;
		virtual MessageEventBase* NewConnectedEvent() = 0;
		virtual MessageEventBase* NewErrorEvent(int dwError) = 0;
		virtual MessageEventBase* NewCloseEvent() = 0;
	protected:
		ISocketBase* m_opSock;
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
