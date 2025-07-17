#ifndef __LOG_MODULE_H__
#define __LOG_MODULE_H__

#include "thread.h"
#include "singleton.h"
#include "../include/cas_lock.h"

#include <sstream>
#include <vector>

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
	std::string				GetLogStr();

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
	std::stringstream		m_oStream;
};

#endif	//!__LOG_MODULE_H__