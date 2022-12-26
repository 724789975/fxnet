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

#include "thread.h"
#include "singleton.h"
#include "cas_lock.h"

#include <sstream>
#include <vector>

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

typedef double (*TimeFunc)();

void SetTimeFunc(TimeFunc f);
TimeFunc GetTimeFunc();

double GetNow();

#define LOG(STREAM, LOG_LEVEL)\
	if (STREAM && (GetLogLevel() & LOG_LEVEL))\
		*STREAM << "[" << #LOG_LEVEL << "]\t[" << GetNow() << "]\t"

/**
 * @brief
 */
class LogModule: public FXNET::IFxThread, public FXNET::TSingleton<LogModule>
{
public:
	LogModule() : m_bStop(false) {}

	/**
	 * @brief
	 *
	 * 线程函数
	 */
	virtual void			ThrdFunc();
	/**
	 * @brief
	 *
	 * 停止执行
	 */
	virtual void			Stop();
	void					SetStoped() { m_bStop = true; }
	bool					Start();
	unsigned int			GetThreadId();

	bool					Init();
	void					Uninit();

	void					PushLog(std::stringstream*& refpStream);
	std::stringstream*		GetStream();

private:

protected:

	/**
	 * @brief
	 *
	 * thread的句柄
	 */
	FXNET::IFxThreadHandler* m_poThrdHandler;
	bool					m_bStop;
	FXNET::CCasLock			m_lockEventLock;
	std::vector<std::stringstream*>	m_vecLogStream;
};

#endif // !__LOG_UTILITY_H__
