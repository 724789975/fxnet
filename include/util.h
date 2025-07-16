#ifndef __UTIL_H__
#define __UTIL_H__

namespace FXNET
{
	typedef double TimeFunc();

	void SetTimeFunc(TimeFunc* func);

	double GetNow();
}


#endif // __UTIL_H__