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
#define UDP_SEND_WINDOW_SIZE 32
/**
 * @brief 发送缓存帧长度
 */
#define UDP_SEND_WINDOW_BUFF_SIZE 512
/**
 * @brief 接收缓存帧数量
 */
#define UDP_RECV_WINDOW_SIZE 32
/**
 * @brief 接收缓存帧长度
 */
#define UDP_RECV_WINDOW_BUFF_SIZE 512

class OnRecvOperator
{
public:
	virtual void operator() (char* szBuff, unsigned int dwSize) = 0;
};

class SendOperator
{
public:
	virtual void operator() (char* szBuff, unsigned int dwSize) = 0;
};

#endif	//! __SLIDING_WINDOW_DEF_H__