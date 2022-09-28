#ifndef __FXNET_INTERFACE_H__
#define __FXNET_INTERFACE_H__

#include "message_event.h"

#include <deque>

namespace FXNET
{
	void StartIOModule();
	void SwapEvent(std::deque<MessageEventBase*>& refDeque);
};


#endif //!__FXNET_INTERFACE_H__
