#include "include\session.h"
#include "include\message_event.h"
#include "include\message_header.h"
#include "include\message_event.h"
#include "include\fxnet_interface.h"
#include "iothread.h"
#include "socket_base.h"
#include "udp_connector.h"

#ifdef _WIN32
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __FUNCSIG__
#endif
#else
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __PRETTY_FUNCTION__
#endif
#endif //!_WIN32

namespace FXNET
{
	CSession& CSession::Send(const char* szData, unsigned int dwLen)
	{
		class UdpSendOperator : public IOEventBase
		{
		public:
			UdpSendOperator(NativeSocketType hSock, std::string* pszData)
				: m_hSock(hSock)
				, m_pszData(pszData)
			{
			}
			virtual void operator ()(std::ostream* pOStream)
			{
				DELETE_WHEN_DESTRUCT(UdpSendOperator, this);

				ISocketBase* poSock = FxIoModule::Instance()->GetSocket(m_hSock);
				if (poSock)
				{
					CUdpConnector* poUdpConnector = dynamic_cast<CUdpConnector*>(poSock);
				}
				else
				{
					if (pOStream)
					{
						*pOStream << "find sock error " << m_hSock
							<< "[" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\n";
					}
				}
			}
		protected:
		private:
			NativeSocketType m_hSock;
			std::string* m_pszData;
		};


		UdpSendOperator* pOperator = new UdpSendOperator(m_hSock, m_oParse.MakeMessage(0, szData, dwLen));
		PostEvent(pOperator);

		return *this;
	}
};
