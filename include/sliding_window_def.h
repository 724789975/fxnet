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
	/**
	 * @brief 接收函数
	 * 
	 * @param szBuff 接收的数据 
	 * @param dwSize 接收的长度
	 * @return int 错误码
	 */
	virtual int operator() (char* szBuff, unsigned int dwSize) = 0;
};

class RecvOperator
{
public:
	/**
	 * @brief 接收函数
	 * 
	 * @param pBuff 接收的数据
	 * @param wBuffSize 接收缓冲区的长度
	 * @param wRecvSize 接收的长度
	 * @return int 错误码
	 */
	virtual int operator() (char* pBuff, unsigned short wBuffSize, int wRecvSize) = 0;
};

class SendOperator
{
public:
	/**
	 * @brief 发送处理函数
	 * 
	 * @param szBuff 要发送的数据
	 * @param wBufferSize 要发送的长度
	 * @param wSendLen 发送长度
	 * @return int 错误码
	 */
	virtual int operator() (char* szBuff, unsigned short wBufferSize, unsigned short& wSendLen) = 0;
};

#endif	//! __SLIDING_WINDOW_DEF_H__