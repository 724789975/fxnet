#ifndef __UTIL_H__
#define __UTIL_H__

namespace FXNET
{
#ifdef _WIN32
#define fx_snprintf sprintf_s
#else
#define fx_snprintf snprintf
#endif

	typedef double TimeFunc();

	void SetTimeFunc(TimeFunc* func);

	double GetNow();
}


#endif // __UTIL_H__