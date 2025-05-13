#ifndef __GetTime_H__
#define __GetTime_H__

#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else
#include <sys/time.h>
#endif  //!_WIN32

namespace UTILITY
{
	class TimeUtility
	{
	public: 
		static unsigned long long GetTimeUS( void )
		{
#ifdef _WIN32
			// ��1601��1��1��0:0:0:000��1970��1��1��0:0:0:000��ʱ��(��λ100ns)
#define EPOCHFILETIME   (116444736000000000UL)
			FILETIME ft;
			LARGE_INTEGER li;
			unsigned long long qwTime = 0;
			GetSystemTimeAsFileTime(&ft);
			li.LowPart  = ft.dwLowDateTime;
			li.HighPart = ft.dwHighDateTime;
			// ��1970��1��1��0:0:0:000�����ڵ�΢����(UTCʱ��)
			qwTime = (li.QuadPart - EPOCHFILETIME) /10;
			return qwTime;
#else
			timeval tv;
			gettimeofday(&tv, 0);
			return (unsigned long long)tv.tv_sec * 1000000 + (unsigned long long)tv.tv_usec;
#endif
		}

		static unsigned long long GetTimeMS( void )
		{
			return GetTimeUS() / 1000;
		}
		
		static unsigned long long GetTime( void )
		{
			return GetTimeMS() / 1000;
		}
	};
	
};

#endif  //!__GetTime_H__