#include "common/buff_contral.h"
#include "common/cas_lock.h"
#include "common/iothread.h"

int main()
{
	FXNET::BufferContral<> buffcontral;
	FXNET::CCasLock l;

	FXNET::FxIoModule::CreateInstance();
}