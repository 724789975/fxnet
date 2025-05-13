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
public     : 
LogModule(): m_bStop(false) {}

	/**
	 * @brief
	 *
	 * �̺߳���
	 */
	virtual void			ThrdFunc();
	/**
	 * @brief
	 *
	 * ִֹͣ��
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
	 * thread�ľ��
	 */
	FXNET::IFxThreadHandler* m_poThrdHandler;
	bool					m_bStop;
	FXNET::CCasLock			m_lockEventLock;
	std::vector<std::stringstream*>	m_vecLogStream;
};

#endif  //!__LOG_MODULE_H__