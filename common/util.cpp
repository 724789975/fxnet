#include "util.h"
#include "../utility/time_utility.h"

namespace FXNET
{
	double Now()
	{
		return UTILITY::TimeUtility::GetTimeUS() / 1000000.;
	}

	TimeFunc* g_timeFunc = Now;

	void SetTimeFunc(TimeFunc* func)
	{
		g_timeFunc = func;
	}

	double GetNow()
	{
		if (g_timeFunc)
			return g_timeFunc();
		else
			return 0;
	}
};
