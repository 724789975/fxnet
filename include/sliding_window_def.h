#ifndef __SLIDING_WINDOW_DEF_H__
#define __SLIDING_WINDOW_DEF_H__

/**
 * @brief 发送缓存帧数量
 */
#define TCP_SEND_WINDOW_SIZE 32
/**
 * @brief 发送缓存帧长度
 */
#define TCP_SEND_WINDOW_BUFF_SIZE 512
/**
 * @brief 接收缓存帧数量
 */
#define TCP_RECV_WINDOW_SIZE 32
/**
 * @brief 接收缓存帧长度
 */
#define TCP_RECV_WINDOW_BUFF_SIZE 512

/**
 * @brief 发送缓存帧数量
 */
#define UDP_WINDOW_SIZE 32
/**
 * @brief 发送缓存帧长度
 */
#define UDP_WINDOW_BUFF_SIZE 512

enum UserError
{
	//设备 模块(不可用 参与下面错误码计算)
	FACILITY_BEGIN = 0x100,
	FACILITY_NET,
	FACILITY_END = 0x7FF,
	
	//正常
	CODE_SUCCESS_BEGIN = 1 << 29 | FACILITY_BEGIN << 16,
	//无缓存可读
	CODE_SUCCESS_NO_BUFF_READ = 1 << 29 | FACILITY_NET << 16,
	CODE_SUCCESS_NET_EOF,

	CODE_SUCCESS_END = 1 << 29 | FACILITY_END << 16 | 15,

	//信息
	CODE_INFO_BEGIN = 1 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_INFO_END = 1 << 30 | 1 << 29 | FACILITY_END << 16 | 15,

	//警告
	CODE_WARN_BEGIN = 2 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_WARN_END = 2 << 30 | 1 << 29 | FACILITY_END << 16 | 15,

	//错误
	CODE_ERROR_BEGIN = 3 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_ERROR_NET_UDP_ALLOC_BUFF = 3 << 30 | 1 << 29 | FACILITY_NET << 16,
	CODE_ERROR_END = 3 << 30 | 1 << 29 | FACILITY_END << 16 | 15,
};

namespace FXNET
{
	class Message
	{
	public:
	protected:
	private:
	};



};

#endif	//! __SLIDING_WINDOW_DEF_H__