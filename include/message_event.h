#ifndef __MESSAGE_EVENT_H__
#define __MESSAGE_EVENT_H__

#include "net_stream_package.h"

#ifdef _WIN32
#include <WinSock2.h>
#endif // _WIN32

#include <ostream>

#define DELETE_WHEN_DESTRUCT(CLASS_NAME, __point)\
class _\
{\
public:\
	_(CLASS_NAME* _p) : p(_p) {}\
	~_() { delete p; }\
	CLASS_NAME* p;\
}____(__point);\

/**
 * @brief 
 * 
 * 消息事件
 */
class MessageEventBase
{
public:
	virtual ~MessageEventBase() {}
	virtual void operator ()(std::ostream* pOStream) = 0;
protected:
private:
};

class MessageRecvEventBase : public MessageEventBase
{
public:
	FXNET::CNetStreamPackage m_oPackage;
protected:
private:
};

/**
 * @brief 
 * 
 * io事件
 */
class IOEventBase
#ifdef _WIN32
	: public OVERLAPPED
	, public MessageEventBase
#else
	: public MessageEventBase
#endif // _WIN32
{
public:
	IOEventBase()
	{
#ifdef _WIN32
		memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
#endif // _WIN32

	}
	virtual ~IOEventBase() {}
protected:
private:
};


#endif // !__MESSAGE_EVENT_H__
