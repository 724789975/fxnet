#include "include/log_utility.h"

ELogLevel g_sLogLevel = ELOG_LEVEL_ALL;

ELogLevel GetLogLevel()
{
	return g_sLogLevel;
}
