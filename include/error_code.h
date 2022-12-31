#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__

#include <string>
#include <stdio.h>

enum UserError
{
	////////////////////设备 模块(不可用 参与下面错误码计算)//////////////////////
	FACILITY_BEGIN = 0x100,
	FACILITY_NET,								//网络模块
	FACILITY_END = 0x7FF,
	////////////////////设备 模块(不可用 参与下面错误码计算)//////////////////////

	////////////////////////////////////正常//////////////////////////////////////
	CODE_SUCCESS_BEGIN = 1 << 29 | FACILITY_BEGIN << 16,
	CODE_SUCCESS_NET_BEGIN = 1 << 29 | FACILITY_NET << 16,
	CODE_SUCCESS_NO_BUFF_READ,		//无缓存可读
	CODE_SUCCESS_NET_EOF,											//断开连接
	CODE_SUCCESS_NET_END = 1 << 29 | FACILITY_NET << 16 | 0xF,
	CODE_SUCCESS_END = 1 << 29 | FACILITY_END << 16 | 0xF,
	////////////////////////////////////正常//////////////////////////////////////

	////////////////////////////////////信息//////////////////////////////////////
	CODE_INFO_BEGIN = 1 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_INFO_NET_BEGIN = 1 << 30 | 1 << 29 | FACILITY_NET << 16,
	CODE_INFO_NET_END = 1 << 30 | 1 << 29 | FACILITY_NET << 16 | 0xF,
	CODE_INFO_END = 1 << 30 | 1 << 29 | FACILITY_END << 16 | 0xF,
	////////////////////////////////////信息//////////////////////////////////////

	////////////////////////////////////警告//////////////////////////////////////
	CODE_WARN_BEGIN = 2 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_WARN_NET_BEGIN = 2 << 30 | 1 << 29 | FACILITY_NET << 16,
	CODE_WARN_NET_END = 2 << 30 | 1 << 29 | FACILITY_NET << 16 | 0xF,
	CODE_WARN_END = 2 << 30 | 1 << 29 | FACILITY_END << 16 | 0xF,
	////////////////////////////////////警告//////////////////////////////////////

	////////////////////////////////////错误//////////////////////////////////////
	CODE_ERROR_BEGIN = 3 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_ERROR_NET_BEGIN = 3 << 30 | 1 << 29 | FACILITY_NET << 16,
	CODE_ERROR_NET_UDP_ALLOC_BUFF,													//UDP滑动窗口没有buff了
	CODE_ERROR_NET_UDP_ACK_TIME_OUT_RETRY,											//udp等待ack超时（15s）
	CODE_ERROR_NET_PARSE_MESSAGE,													//解析错误
	CODE_ERROR_NET_ERROR_SOCKET,													//socket < 0
	CODE_ERROR_NET_ERROR_COMPLETION_PORT,											//CompletionPort < 0
	CODE_ERROR_NET_ERROR_EPOLL_HANDLE,												//epoll < 0
	CODE_ERROR_NET_END = 3 << 30 | 1 << 29 | FACILITY_NET << 16 | 0xF,
	CODE_ERROR_END = 3 << 30 | 1 << 29 | FACILITY_END << 16 | 0xF,
	////////////////////////////////////错误//////////////////////////////////////
};

#define __ERROR_CONCAT__(a, b) __ERROR_CONCAT__1(a, b)
#define __ERROR_CONCAT__1(a, b) __ERROR_CONCAT__2(a) __ERROR_CONCAT__2(b)
#define __ERROR_CONCAT__2(a) #a

#define __LINE2STR__(a) __LINE2STR__2(a)
#define __LINE2STR__2(a) #a
class ErrorCode
{
public:
	ErrorCode(int dwError, const char* szWhat)
		: m_dwError(dwError)
		, m_strWhat(szWhat)
	{}

	ErrorCode(int dwError)
		: m_dwError(dwError)
	{}

	ErrorCode()
		: m_dwError(0)
	{}

	operator const int&() const { return m_dwError; }
	std::string What() const
	{
		char buff[32] = { 0 };
		sprintf(buff, "%d", m_dwError);
		return m_strWhat + ",error:" + buff;
	}
	// private:
	int m_dwError;
	std::string m_strWhat;
};

#endif // !__ERROR_CODE_H__
