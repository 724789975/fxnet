#ifndef __MESSAGE_QUEUE_H__
#define __MESSAGE_QUEUE_H__

#include "message_event.h"
#include "cas_lock.h"
#include <deque>

namespace FXNET
{
	class MessageEventQueue
	{
	public:
		MessageEventQueue() {}
		~MessageEventQueue() {}

		/**
		 * @brief
		 *
		 * Ͷ����Ϣ�¼� ��io�߳�ִ�� ������� �����߳�����
		 * @param pMessageEvent
		 */
		void					PushMessageEvent(MessageEventBase* pMessageEvent)
		{
			CLockImp oImp(this->m_lockEventLock);
			this->m_dequeEvents.push_back(pMessageEvent);
		}

		/**
		 * @brief
		 *
		 * �����¼� �����߳�ִ�� �¼���io�̴߳��� ������� �����߳�����
		 * @param refDeque
		 */
		void					SwapEvent(std::deque<MessageEventBase*>& refDeque)
		{
			CLockImp oImp(this->m_lockEventLock);
			this->m_dequeEvents.swap(refDeque);
		}

	private:
		std::deque<MessageEventBase*> m_dequeEvents;
		CCasLock				m_lockEventLock;
	};


}

#endif // !__MESSAGE_QUEUE_H__
