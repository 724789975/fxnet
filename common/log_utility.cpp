#include "../include/log_utility.h"
#include "../utility/time_utility.h"

#include <iostream>

#ifndef _WIN32
#include <unistd.h>
#endif

TimeFunc pTimeFunc = 0;

int g_sLogLevel = ELOG_LEVEL_ERROR | ELOG_LEVEL_WARN | ELOG_LEVEL_INFO
	//| ELOG_LEVEL_DEBUG | ELOG_LEVEL_DEBUG1 | ELOG_LEVEL_DEBUG2 | ELOG_LEVEL_DEBUG3
	// | ELOG_LEVEL_DEBUG4
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
	// if (pTimeFunc)
	// {
	// 	return pTimeFunc();
	// }
	return UTILITY::TimeUtility::GetTimeUS() / 1000000.;
	return 0.;
}

void LogModule::ThrdFunc()
{
	while (!m_bStop)
	{
		std::vector<std::stringstream> vecTemp;
		{
			FXNET::CLockImp oImp(this->m_lockEventLock);
			vecTemp.swap(this->m_vecLogStream);
		}

		if (0 == vecTemp.size())
		{
#ifdef _WIN32
			Sleep(1);
#else
			usleep(1000);
#endif // _WIN32
			continue;
		}

		for (std::vector<std::stringstream>::iterator it = vecTemp.begin()
			; it != vecTemp.end(); ++it)
		{
			std::cout << it->str();
		}

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

void LogModule::PushLog(std::stringstream& refStream)
{
	FXNET::CLockImp oImp(this->m_lockEventLock);
#if __cplusplus < 201103L
	m_vecLogStream.push_back(refStream.str());
	refStream.clear();
	refStream.str("");
#else
	m_vecLogStream.push_back(std::stringstream());
	std::swap(m_vecLogStream.back(), refStream);
#endif
}

