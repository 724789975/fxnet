#ifndef __CONNECTOR_SOCKET_H__
#define __CONNECTOR_SOCKET_H__

#include "socket_base.h"
#include "i_session.h"

namespace FXNET
{
	class CConnectorSocket : public ISocketBase
	{
	public:
		CConnectorSocket(ISession* pSession)
			: m_pSession(pSession)
		{}
		virtual const char* Name()const { return "CConnectorSocket"; }

		/**
		 * @brief 
		 * 
		 * ·¢ËÍÏûÏ¢
		 * @param pOStream 
		 * @return int 
		 */
		virtual ErrorCode SendMessage(std::ostream* pOStream) = 0;
		/**
		 * @brief Get the Session object
		 * 
		 * @return ISession* 
		 */
		ISession* GetSession() { return m_pSession; }
		/**
		 * @brief Set the Session object
		 * 
		 * @param poSession 
		 * @return CConnectorSocket& 
		 */
		CConnectorSocket& SetSession(ISession* poSession) { m_pSession = poSession; return *this; }
	protected:

		ISession* m_pSession;
	private:
	};

};

#endif // !__CONNECTOR_SOCKET_H__

