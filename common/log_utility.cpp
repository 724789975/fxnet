#include "../include/log_utility.h"
#include "../utility/time_utility.h"
#include "log_module.h"

#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#endif

int g_sLogLevel = ELOG_LEVEL_ERROR | ELOG_LEVEL_WARN | ELOG_LEVEL_INFO
	//| ELOG_LEVEL_DEBUG | ELOG_LEVEL_DEBUG1 | ELOG_LEVEL_DEBUG2 | ELOG_LEVEL_DEBUG3
	// | ELOG_LEVEL_DEBUG4
	;

int GetLogLevel()
{
	return g_sLogLevel;
}

void LogModule::ThrdFunc()
{
	while (!m_bStop)
	{
		std::stringstream strTemp;
		{
			FXNET::CLockImp oImp(this->m_lockEventLock);
			strTemp.swap(this->m_oStream);
			m_oStream.str("");
		}

		if (0 == strTemp.str().length())
		{
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif // _WIN32
			continue;
		}

		std::cout << strTemp.str();

		std::cout << std::flush;
	}
}

void LogModule::Stop()
{
	this->m_bStop = true;
	if (this->m_poThrdHandler != NULL)
	{
		this->m_poThrdHandler->WaitFor(3000);
		this->m_poThrdHandler->Release();
		this->m_poThrdHandler = NULL;
	}
}

bool LogModule::Start()
{
	FxCreateThreadHandler(this, true, this->m_poThrdHandler);
	if (NULL == this->m_poThrdHandler)
	{
		return false;
	}
	return true;
}

unsigned int LogModule::GetThreadId()
{
	if (this->m_poThrdHandler)
	{
		return this->m_poThrdHandler->GetThreadId();
	}
	return 0;
}

bool LogModule::Init()
{
	return this->Start();
}

void LogModule::Uninit()
{
	this->Stop();
}

void LogModule::PushLog(std::stringstream*& refpStream)
{
	FXNET::CLockImp oImp(this->m_lockEventLock);
	m_oStream << refpStream->str();
}

const char* LogModule::GetLogStr()
{
	std::stringstream strTemp;
	{
		FXNET::CLockImp oImp(this->m_lockEventLock);
		strTemp.swap(this->m_oStream);
		m_oStream.str("");
	}
	return strTemp.str().c_str();
}
