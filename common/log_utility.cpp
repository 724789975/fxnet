#include "include/log_utility.h"



TimeFunc pTimeFunc = 0;

int g_sLogLevel = ELOG_LEVEL_ERROR | ELOG_LEVEL_WARN | ELOG_LEVEL_INFO
	//| ELOG_LEVEL_DEBUG | ELOG_LEVEL_DEBUG1 | ELOG_LEVEL_DEBUG2 | ELOG_LEVEL_DEBUG3
	;

int GetLogLevel()
{
	return g_sLogLevel;
}

void SetTimeFunc(TimeFunc f)
{
	pTimeFunc = f;
}

TimeFunc GetTimeFunc()
{
	return pTimeFunc;
}

double GetNow()
{
	if (pTimeFunc)
	{
		return pTimeFunc();
	}
	return 0.;
}

