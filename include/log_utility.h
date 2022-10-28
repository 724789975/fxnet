#ifndef __LOG_UTILITY_H__
#define __LOG_UTILITY_H__

#ifdef _WIN32
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __FUNCSIG__
#endif
#else
#ifndef __FUNCTION_DETAIL__
#define __FUNCTION_DETAIL__ __PRETTY_FUNCTION__
#endif
#endif //!_WIN32

enum ELogLevel
{
	ELOG_LEVEL_ERROR,
	ELOG_LEVEL_WARN,
	ELOG_LEVEL_INFO,
	ELOG_LEVEL_DEBUG,
	ELOG_LEVEL_DEBUG1,
	ELOG_LEVEL_DEBUG2,
	ELOG_LEVEL_DEBUG3,

	ELOG_LEVEL_ALL,
};

ELogLevel GetLogLevel();

#define LOG(STREAM, LOG_LEVEL)\
	if (STREAM && (GetLogLevel() > LOG_LEVEL))\
		*STREAM

#endif // !__LOG_UTILITY_H__
