#ifndef __LOG_UTILITY_H__
#define __LOG_UTILITY_H__

#if 1
#define __FUNCTION_DETAIL__ __FUNCTION__
#else
#ifdef _WIN32
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __FUNCSIG__
#endif
#else
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __PRETTY_FUNCTION__
#endif
#endif //!_WIN32
#endif

#include <sstream>
#include <vector>
#include "util.h"

enum ELogLevel
{
	ELOG_LEVEL_ERROR = 1,
	ELOG_LEVEL_WARN = 1 << 1,
	ELOG_LEVEL_INFO = 1 << 2,
	ELOG_LEVEL_DEBUG = 1 << 3,
	ELOG_LEVEL_DEBUG1 = 1 << 4,
	ELOG_LEVEL_DEBUG2 = 1 << 5,
	ELOG_LEVEL_DEBUG3 = 1 << 6,
	ELOG_LEVEL_DEBUG4 = 1 << 7,

};

int GetLogLevel();

#define LOG(STREAM, LOG_LEVEL)\
	if (STREAM && (GetLogLevel() & LOG_LEVEL))\
		*STREAM << "[" << #LOG_LEVEL << "]\t[" << FXNET::GetNow() << "]\t" << " [" << __FILE__ << ":" << __LINE__ << ", " << __FUNCTION_DETAIL__ << "]\t"

#endif // !__LOG_UTILITY_H__
