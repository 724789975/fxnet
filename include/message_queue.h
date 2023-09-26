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
		 * 投递消息事件 在io线程执行 放入队列 在主线程消费
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
		 * 交换事件 在主线程执行 事件在io线程创建 放入队列 在主线程消费
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
