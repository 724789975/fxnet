/**
 * @file buff_contral.h
 * @brief UDP可靠传输缓冲区控制器
 * 
 * 该模块实现了基于UDP的可靠传输机制，包含以下核心功能：
 * - 滑动窗口协议（发送窗口和接收窗口）
 * - 拥塞控制（慢启动、拥塞避免）
 * - 超时重传和快速重传
 * - RTT/RTO计算
 * - 连接状态管理
 */

#ifndef __BUFF_CONTRAL_H__
#define __BUFF_CONTRAL_H__

#include "../include/sliding_window_def.h"
#include "../include/error_code.h"
#include "../include/log_utility.h"
#include "iothread.h"
#include "sliding_window.h"

#include <math.h>
#include <errno.h>
#include <string.h>
#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
#include <time.h>
#else
#include <sys/time.h>
#endif // _WIN32

namespace FXNET
{
	/**
	 * @brief 数据接收回调操作符基类
	 * 用于处理接收到的有效数据
	 */
	class OnRecvOperator
	{
	public:
		virtual ~OnRecvOperator() {};

		/**
		 * @brief 接收函数
		 *
		 * @param szBuff 接收的数据
		 * @param dwSize 接收的长度
		 * @return ErrorCode 错误码
		 */
		virtual OnRecvOperator& operator() (char* szBuff, unsigned short wSize, ErrorCode& refError, std::ostream* pOStream) = 0;
	};

	/**
	 * @brief 连接建立回调操作符基类
	 * 用于处理连接建立成功时的操作
	 */
	class OnConnectedOperator
	{
	public:
		virtual ~OnConnectedOperator() {};
		/**
		 * @brief 连接处理
		 *
		 */
		virtual OnConnectedOperator& operator() (ErrorCode& refError, std::ostream* pOStream) = 0;
	};

	/**
	 * @brief 底层接收操作符基类
	 * 用于从socket实际接收数据
	 */
	class RecvOperator
	{
	public:
		virtual ~RecvOperator() {};
		/**
		 * @brief 接收函数
		 *
		 * @param pBuff 接收的数据
		 * @param wBuffSize 接收缓冲区的长度
		 * @param wRecvSize 接收的长度
		 * @return ErrorCode 错误码
		 */
		virtual RecvOperator& operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, ErrorCode& refError, std::ostream* pOStream) = 0;
	};

	/**
	 * @brief 底层发送操作符基类
	 * 用于通过socket实际发送数据
	 */
	class SendOperator
	{
	public:
		virtual ~SendOperator() {};
		/**
		 * @brief 发送处理函数
		 *
		 * @param szBuff 要发送的数据
		 * @param wBufferSize 要发送的长度
		 * @param wSendLen 发送长度
		 * @return ErrorCode 错误码
		 */
		virtual SendOperator& operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, ErrorCode& refError, std::ostream* pOStream) = 0;
	};

	/**
	 * @brief 流读取操作符基类
	 * 用于读取并输出统计信息
	 */
	class ReadStreamOperator
	{
	public:
		virtual ~ReadStreamOperator() {};
		virtual unsigned int operator() (std::ostream* pOStream) = 0;
	protected:
	private:
	};

	/**
	 * @brief 连接状态枚举
	 * 模拟TCP的三次握手和四次挥手状态机
	 */
	enum ConnectionStatus
	{
		ST_IDLE,			//空闲状态
		ST_SYN_SEND,		//已发送SYN包 使用中
		ST_SYN_RECV,		//已收到SYN包 使用中
		ST_SYN_RECV_WAIT,	//等待SYN确认
		ST_ESTABLISHED,		//连接已建立 使用中
		ST_FIN_WAIT_1,		//等待FIN确认
		ST_FIN_WAIT_2,		//等待对方FIN
	};

	/**
	 * @brief 缓冲区控制器模板类
	 * 实现基于UDP的可靠传输，包含滑动窗口、拥塞控制、重传等TCP特性
	 * @tparam BUFF_SIZE 单个缓冲区大小，默认512字节
	 * @tparam WINDOW_SIZE 滑动窗口大小，默认32个包
	 */
	template <unsigned short BUFF_SIZE = 512, unsigned short WINDOW_SIZE = 32>
	class BufferContral
	{
	public:
		enum { buff_size = BUFF_SIZE };		//单个缓冲区大小
		enum { window_size = WINDOW_SIZE };	//滑动窗口大小

		typedef SendWindow<buff_size, window_size> _SendWindow;		//发送窗口类型
		typedef SlidingWindow<buff_size, window_size> _RecvWindow;	//接收窗口类型
		typedef BufferContral<buff_size, window_size> BUFF_CONTRAL;	//自身类型

		BufferContral();
		~BufferContral();

		/**
		 * @brief 初始化缓冲区控制器
		 * @param dwState 初始连接状态
		 * @param dAckRecvTime 初始ACK接收时间
		 * @return 初始化结果，0表示成功
		 */
		int Init(int dwState, double dAckRecvTime);

		/**
		 * @brief 获取当前连接状态
		 * @return 当前连接状态
		 */
		int GetState()const;

		/**
		 * @brief 设置数据接收回调操作符
		 * @param p 接收回调操作符指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetOnRecvOperator(OnRecvOperator* p);
		/**
		 * @brief 设置连接建立回调操作符
		 * @param p 连接建立回调操作符指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetOnConnectedOperator(OnConnectedOperator* p);
		/**
		 * @brief 设置底层接收操作符
		 * @param p 底层接收操作符指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetRecvOperator(RecvOperator* p);
		/**
		 * @brief 设置底层发送操作符
		 * @param p 底层发送操作符指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetSendOperator(SendOperator* p);
		/**
		 * @brief 设置流读取操作符
		 * @param p 流读取操作符指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetReadStreamOperator(ReadStreamOperator* p);

		/**
		 * @brief 设置ACK超时时间
		 * @param dOutTime ACK超时时间（秒）
		 * @return 自身引用，支持链式调用
		 */
		BufferContral& SetAckOutTime(double dOutTime);

		/**
		 * @brief 将数据放入发送缓冲区
		 *
		 * @param pSendBuffer 待发送数据
		 * @param wSize 要发送的长度
		 * @return unsigned short 放入发送缓冲的长度 <= wSize
		 */
		unsigned int Send(const char* pSendBuffer, unsigned int dwSize);

		/**
		 * @brief 发送消息，将发送窗口中的数据发送出去
		 * 包含拥塞控制、快速重传、超时重传等机制
		 * @param dTime 当前时间戳
		 * @param refErrorCode 错误码引用
		 * @param pOStream 日志输出流指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral<BUFF_SIZE, WINDOW_SIZE>& SendMessages
			(double dTime, ErrorCode& refErrorCode, std::ostream* pOStream);
		/**
		 * @brief 接收数据并处理
		 * 从socket接收数据，更新接收窗口，处理ACK，交付数据给上层
		 * @param dTime 当前时间戳
		 * @param refbReadable 是否还有数据可读的引用
		 * @param refError 错误码引用
		 * @param pOStream 日志输出流指针
		 * @return 自身引用，支持链式调用
		 */
		BufferContral<BUFF_SIZE, WINDOW_SIZE>& ReceiveMessages
			(double dTime, bool& refbReadable, ErrorCode& refError, std::ostream* pOStream);
		
	private:
		_SendWindow m_oSendWindow;			//发送滑动窗口
		_RecvWindow m_oRecvWindow;			//接收滑动窗口

		OnRecvOperator* m_pOnRecvOperator;			//数据接收回调
		OnConnectedOperator* m_pOnConnectedOperator;	//连接建立回调
		RecvOperator* m_pRecvOperator;				//底层接收操作
		SendOperator* m_pSendOperator;				//底层发送操作
		ReadStreamOperator* m_pReadStreamOperator;	//流读取操作

		/**
		 * @brief 已发送字节数统计
		 */
		unsigned int m_dwNumBytesSend;
		/**
		 * @brief 已接收字节数统计
		 */
		unsigned int m_dwNumBytesReceived;

		/**
		 * @brief RTT（往返时间）测量值
		 */
		double m_dDelayTime;
		/**
		 * @brief 平均RTT（平滑往返时间）
		 */
		double m_dDelayAverage;
		/**
		 * @brief RTO（重传超时时间）
		 * 超过该时间未收到ACK则进行重传
		 */
		double m_dRetryTime;
		/**
		 * @brief 下一次发送数据包的时间
		 */
		double m_dSendTime;
		/**
		 * @brief 发送频率控制（秒）
		 */
		double m_dSendFrequency;
		/**
		 * @brief 拥塞窗口大小（拥塞控制）
		 */
		double m_dSendWindowControl;
		/**
		 * @brief 慢启动阈值
		 * 超过该值进入拥塞避免阶段
		 */
		double m_dSendWindowThreshhold;
		/**
		 * @brief 下次发送空包的时间
		 * 用于保活和窗口同步
		 */
		double m_dSendEmptyDataTime;
		/**
		 * @brief 发送空包的基础频率（秒）
		 */
		double m_dSendEmptyDataFrequency;
		/**
		 * @brief 空包发送因子（指数退避）
		 * 发送下一个非空包时置零
		 */
		int m_dwSendEmptyDataFactor;

		/**
		 * @brief 发送数据包数量统计
		 * 每512字节或少于512字节算一个包
		 */
		unsigned int m_dwNumPacketsSend;
		/**
		 * @brief 重传数据包数量统计
		 */
		unsigned int m_dwNumPacketsRetry;

		bool m_bConnected;		//连接是否已建立标志

		/**
		 * @brief 最后一次收到有效ACK的时间
		 */
		double m_dAckRecvTime;
		/**
		 * @brief ACK超时重试次数
		 */
		int m_dwAckTimeoutRetry;
		/**
		 * @brief 当前连接状态
		 */
		unsigned int m_dwStatus;

		/**
		 * @brief 连续收到相同ACK的次数
		 * 用于快速重传判断
		 */
		int m_dwAckSameCount;
		/**
		 * @brief 是否正在进行快速重传
		 */
		bool m_bQuickRetry;
		/**
		 * @brief 是否需要发送ACK
		 */
		bool m_bSendAck;
		/**
		 * @brief 最后处理的ACK序号
		 */
		unsigned char m_btAckLast;
		/**
		 * @brief 最后处理的SYN序号
		 */
		unsigned char m_btSynLast;
		/**
		 * @brief ACK超时时间（秒），默认5秒
		 */
		double m_dAckOutTime;
	};

	/**
	 * @brief 构造函数
	 * 初始化各成员变量的默认值
	 */
	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::BufferContral()
		: m_dwNumBytesSend(0)
		, m_dwNumBytesReceived(0)
		, m_dSendTime(0.)
		, m_dSendFrequency(0.02)
		, m_dSendEmptyDataFrequency(0.1)
		, m_dwSendEmptyDataFactor(0)
		, m_dwNumPacketsSend(0)
		, m_dwNumPacketsRetry(0)
		, m_bConnected(false)
		, m_btAckLast(0)
		, m_btSynLast(0)
	{
	}

	/**
	 * @brief 析构函数
	 */
	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::~BufferContral()
	{ }

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::Init(int dwState, double dAckRecvTime)
	{
		this->m_dwStatus = dwState;
		this->m_dDelayTime = 0;
		this->m_dSendFrequency = UDP_SEND_FREQUENCY;
		this->m_dDelayAverage = 3 * m_dSendFrequency;
		this->m_dRetryTime = m_dDelayTime + 2 * m_dDelayAverage;
		this->m_dSendTime = 0;

		this->m_dAckRecvTime = dAckRecvTime;
		this->m_dwAckTimeoutRetry = 1;
		this->m_dwAckSameCount = 0;
		this->m_bQuickRetry = false;
		this->m_dSendEmptyDataTime = 0;

		this->m_btAckLast = 0;
		this->m_btSynLast = 0;
		this->m_bSendAck = false;
		// this->m_dSendWindowControl = 1;
		this->m_dSendWindowControl = _SendWindow::window_size;
		this->m_dSendWindowThreshhold = _SendWindow::window_size;

		// 清空滑动窗口
		this->m_oRecvWindow.ClearBuffer();
		this->m_oSendWindow.ClearBuffer();

		// 初始化发送窗口
		this->m_oSendWindow.m_btBegin = 1;
		this->m_oSendWindow.m_btEnd = m_oSendWindow.m_btBegin;

		// 初始化接收窗口
		this->m_oRecvWindow.m_btBegin = 1;
		this->m_oRecvWindow.m_btEnd = m_oRecvWindow.m_btBegin + _RecvWindow::window_size;

		this->m_bConnected = false;
		return 0;
	}

	template <unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	int BufferContral<BUFF_SIZE, WINDOW_SIZE>::GetState() const
	{
		return this->m_dwStatus;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline  BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetOnRecvOperator(OnRecvOperator* p)
	{
		this->m_pOnRecvOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline  BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetOnConnectedOperator(OnConnectedOperator* p)
	{
		this->m_pOnConnectedOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetRecvOperator(RecvOperator* p)
	{
		this->m_pRecvOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetSendOperator(SendOperator* p)
	{
		this->m_pSendOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetReadStreamOperator(ReadStreamOperator* p)
	{
		this->m_pReadStreamOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetAckOutTime(double dOutTime)
	{
		this->m_dAckOutTime = dOutTime;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline unsigned int BufferContral<BUFF_SIZE, WINDOW_SIZE>
		::Send(const char* pSendBuffer, unsigned int dwSize)
	{
		unsigned int dwSendSize = 0;
		while ((this->m_oSendWindow.m_btFreeBufferId < _SendWindow::window_size) && // there is a free buffer
			(dwSize > 0))
		{
			//长度不会大于拥塞窗口
			if (this->m_oSendWindow.m_btEnd - this->m_oSendWindow.m_btBegin > this->m_dSendWindowControl) break;

			unsigned char btId = this->m_oSendWindow.m_btEnd % _SendWindow::window_size;

			// 获取一个帧
			unsigned char btBufferId = this->m_oSendWindow.m_btFreeBufferId;
			this->m_oSendWindow.m_btFreeBufferId = this->m_oSendWindow.m_btarrBuffer[btBufferId][0];

			// 发送的缓存
			unsigned char* pBuffer = this->m_oSendWindow.m_btarrBuffer[btBufferId];

			// 处理数据头
			UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
			oPacket.m_btStatus = this->m_dwStatus;
			oPacket.m_btSyn = this->m_oSendWindow.m_btEnd;
			oPacket.m_btAck = this->m_oRecvWindow.m_btBegin - 1;

			// 复制数据
			unsigned int dwCopyOffset = sizeof(oPacket);
			unsigned int dwCopySize = _SendWindow::buff_size - dwCopyOffset;
			if (dwCopySize > dwSize)
				dwCopySize = dwSize;

			if (dwCopySize > 0)
			{
				memcpy((void*)(pBuffer + dwCopyOffset), pSendBuffer + dwSendSize, dwCopySize);

				dwSize -= dwCopySize;
				dwSendSize += dwCopySize;
			}

			// 添加到发送窗口
			this->m_oSendWindow.Add2SendWindow(btId, btBufferId, dwCopySize + dwCopyOffset
				, GetFxIoModule(0)->FxGetCurrentTime(), m_dRetryTime);
		}

		return dwSendSize;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SendMessages
		(double dTime, ErrorCode& refError, std::ostream* pOStream)
	{
		// 检查是否超时
		if (dTime - this->m_dAckRecvTime > m_dAckOutTime)
		{
			this->m_dAckRecvTime = dTime;

			if (--this->m_dwAckTimeoutRetry <= 0)
			{
				refError(CODE_ERROR_NET_UDP_ACK_TIME_OUT_RETRY, __FILE__ ":" __LINE2STR__(__LINE__));
				return *this;
			}
		}

		if (dTime < this->m_dSendTime) { return *this; }

		//if (m_dwStatus == ST_SYN_RECV) {return 0;}

		bool bForceRetry = false;

		if (this->m_dwStatus == ST_ESTABLISHED)
		{
			//3次相同ack 开始快速重传
			if (this->m_dwAckSameCount > 3)
			{
				if (this->m_bQuickRetry == false)
				{
					this->m_bQuickRetry = true;
					bForceRetry = true;

					this->m_dSendWindowThreshhold = this->m_dSendWindowControl / 2;
					if (this->m_dSendWindowThreshhold < 2) { this->m_dSendWindowThreshhold = 2; }

					this->m_dSendWindowControl = this->m_dSendWindowThreshhold + this->m_dwAckSameCount - 1;
					if (this->m_dSendWindowControl > _SendWindow::window_size)
					{
						this->m_dSendWindowControl = _SendWindow::window_size;
					}
				}
				else
				{
					//相同ack时 拥塞控制窗口+1
					this->m_dSendWindowControl += 1;
					if (this->m_dSendWindowControl > _SendWindow::window_size)
					{
						this->m_dSendWindowControl = _SendWindow::window_size;
					}
				}
			}
			else
			{
				//有新的ack 那么快速重传结束
				if (this->m_bQuickRetry == true)
				{
					this->m_dSendWindowControl = this->m_dSendWindowThreshhold;
					this->m_bQuickRetry = false;
				}
			}

			//如果超时(超过rto) 那么重新进入慢启动
			for (unsigned char i = this->m_oSendWindow.m_btBegin; i != this->m_oSendWindow.m_btEnd; i++)
			{
				unsigned char btId = i % _SendWindow::window_size;
				//unsigned short size = m_oSendWindow.m_warrSeqSize[btId];

				if (this->m_oSendWindow.m_dwarrSeqRetryCount[btId] > 0
					&& dTime >= this->m_oSendWindow.m_darrSeqRetry[btId])
				{
					this->m_dSendWindowThreshhold = this->m_dSendWindowControl / 2;
					if (this->m_dSendWindowThreshhold < 2) { this->m_dSendWindowThreshhold = 2; }
					//m_SendWindowControl = 1;
					this->m_dSendWindowControl = this->m_dSendWindowThreshhold;
					//break;

					this->m_bQuickRetry = false;
					this->m_dwAckSameCount = 0;
					break;
				}
			}

			(* this->m_pReadStreamOperator)(pOStream);
		}

		//没有数据发送 那么创建一个空的 用来同步窗口数据
		if (this->m_oSendWindow.m_btBegin == this->m_oSendWindow.m_btEnd)
		{
			if (dTime >= this->m_dSendEmptyDataTime)
			{
				if (this->m_oSendWindow.m_btFreeBufferId < this->m_oSendWindow.window_size)
				{
					unsigned char btId = this->m_oSendWindow.m_btEnd % this->m_oSendWindow.window_size;

					// 获取缓存
					unsigned char btBufferId = this->m_oSendWindow.m_btFreeBufferId;
					this->m_oSendWindow.m_btFreeBufferId = this->m_oSendWindow.m_btarrBuffer[btBufferId][0];
					unsigned char* pBuffer = this->m_oSendWindow.m_btarrBuffer[btBufferId];

					// packet header
					UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
					oPacket.m_btStatus = this->m_dwStatus;
					oPacket.m_btSyn = this->m_oSendWindow.m_btEnd;
					oPacket.m_btAck = this->m_oRecvWindow.m_btBegin - 1;

					LOG(pOStream, ELOG_LEVEL_DEBUG2) << "status:" << (int)oPacket.m_btStatus
						<< ", syn:" << (int)oPacket.m_btSyn << ", ack:" << (int)oPacket.m_btAck
						<< ", m_oSendWindow.m_btEnd:" << (int)this->m_oSendWindow.m_btEnd
						<< ", m_oRecvWindow.m_btBegin:" << (int)this->m_oRecvWindow.m_btBegin
						<< "\n";

					// 添加到发送窗口
					this->m_oSendWindow.Add2SendWindow(btId, btBufferId, sizeof(oPacket), dTime, this->m_dRetryTime);
				}
				m_dwSendEmptyDataFactor = 1 << m_dwSendEmptyDataFactor;

				double dTempFrequency = this->m_dSendEmptyDataFrequency * (1 << m_dwSendEmptyDataFactor);
				if (1. < dTempFrequency)
				{
					dTempFrequency = 1.;
				}

				this->m_dSendEmptyDataTime = dTime + dTempFrequency;
			}
		}
		else
		{
			this->m_dSendEmptyDataTime = dTime + this->m_dSendEmptyDataFrequency;
			m_dwSendEmptyDataFactor = 0;
		}

		//开始发送
		for (unsigned char i = this->m_oSendWindow.m_btBegin; i != this->m_oSendWindow.m_btEnd; i++)
		{
			//如果发送长度超过拥塞窗口 就停止
			if (i - this->m_oSendWindow.m_btBegin >= this->m_dSendWindowControl) { break; }

			unsigned char btId = i % _SendWindow::window_size;
			unsigned short wSize = this->m_oSendWindow.m_warrSeqSize[btId];

			//开始发送 或者 重传
			if (dTime >= this->m_oSendWindow.m_darrSeqRetry[btId] || bForceRetry)
			{
				bForceRetry = false;

				unsigned char* pBuffer = this->m_oSendWindow.m_btarrBuffer[this->m_oSendWindow.m_btarrSeqBufferId[btId]];

				// packet header
				UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
				oPacket.m_btStatus = this->m_dwStatus;
				oPacket.m_btSyn = i;
				oPacket.m_btAck = this->m_oRecvWindow.m_btBegin - 1;

				int dwLen = 0;
				(*this->m_pSendOperator)((char*)pBuffer, wSize, dwLen, refError, pOStream);
				if (refError)
				{
					if (EAGAIN == refError || EINTR == refError)
					{
						refError(0, "");
						break;
					}
					//发送出错 断开连接
					return *this;
				}
				else
				{
					this->m_dwNumBytesSend += dwLen;
				}

				// 发送包数量
				this->m_dwNumPacketsSend++;

				// 重试次数
				if (dTime != this->m_oSendWindow.m_darrSeqTime[btId]) { this->m_dwNumPacketsRetry++; }

				this->m_dSendTime = dTime + this->m_dSendFrequency;
				// this->m_dSendEmptyDataTime = dTime + this->m_dSendEmptyDataFrequency;
				this->m_bSendAck = false;

				this->m_oSendWindow.m_dwarrSeqRetryCount[btId]++;
				//m_oSendWindow.m_darrSeqRetryTime[id] *= 2;
				this->m_oSendWindow.m_darrSeqRetryTime[btId] = 1.5 * this->m_dRetryTime;
				if (this->m_oSendWindow.m_darrSeqRetryTime[btId] > 0.2) this->m_oSendWindow.m_darrSeqRetryTime[btId] = 0.2;
				this->m_oSendWindow.m_darrSeqRetry[btId] = dTime + this->m_oSendWindow.m_darrSeqRetryTime[btId];
			}
		}

		// send ack
		if (m_bSendAck)
		{
			UDPPacketHeader oPacket;
			oPacket.m_btStatus = this->m_dwStatus;
			oPacket.m_btSyn = this->m_oSendWindow.m_btBegin - 1;
			oPacket.m_btAck = this->m_oRecvWindow.m_btBegin - 1;

			int dwLen = 0;
			(*this->m_pSendOperator)((char*)(&oPacket), sizeof(oPacket), dwLen, refError, pOStream);
			if (refError)
			{
				//发送出错 断开连接
				return *this;
			}

			this->m_dSendTime = dTime + this->m_dSendFrequency;
			this->m_bSendAck = false;
		}

		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::ReceiveMessages
		(double dTime, bool& refbReadable, ErrorCode& refError, std::ostream* pOStream)
	{
		// packet received
		bool bPacketReceived = false;
		//bool bCloseConnection = false;

		while (refbReadable)
		{
			// allocate buffer
			unsigned char btBufferId = this->m_oRecvWindow.m_btFreeBufferId;
			unsigned char* pBuffer = this->m_oRecvWindow.m_btarrBuffer[btBufferId];
			this->m_oRecvWindow.m_btFreeBufferId = pBuffer[0];

			// 没有buffer了 断开连接
			if (btBufferId >= this->m_oRecvWindow.window_size)
			{
				refError(CODE_ERROR_NET_UDP_ALLOC_BUFF, __FILE__ ":" __LINE2STR__(__LINE__));
				return *this;
			}

			int dwLen = 0;
			(*this->m_pRecvOperator)((char*)pBuffer, _RecvWindow::buff_size, dwLen, refError, pOStream);

			if (refError)
			{
				if (EINTR == refError)
				{
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					refError(0, "");
					continue;
				}

				if (EAGAIN == refError)
				{
					refbReadable = false;
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					refError(0, "");
					break;
				}

#ifdef _WIN32
				if (WSA_IO_PENDING == refError || CODE_SUCCESS_NO_BUFF_READ == refError)
#else
				if (EAGAIN == refError || EWOULDBLOCK == refError)
#endif	//_WIN32
				{
					refbReadable = false;
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					refError(0, "");
					break;
				}

				return *this;
			}

			if (dwLen < (unsigned short)sizeof(UDPPacketHeader))
			{
				pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
				this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
				continue;
			}
			if (!this->m_bConnected)
			{
				UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;
				this->m_dwStatus = ST_ESTABLISHED;
				if (ST_ESTABLISHED == packet.m_btStatus)
				{
					(*this->m_pOnConnectedOperator)(refError, pOStream);
					this->m_bConnected = true;
					
					for (unsigned char i = this->m_oRecvWindow.m_btBegin; i != this->m_oRecvWindow.m_btEnd; ++i)
					{
						unsigned char btId = i % this->m_oRecvWindow.window_size;
						this->m_oRecvWindow.m_btarrSeqBufferId[btId] = this->m_oRecvWindow.window_size;
						this->m_oRecvWindow.m_warrSeqSize[btId] = 0;
						this->m_oRecvWindow.m_darrSeqTime[btId] = 0;
						this->m_oRecvWindow.m_darrSeqRetry[btId] = 0;
						this->m_oRecvWindow.m_dwarrSeqRetryCount[btId] = 0;
					}
				}
			}

			//接收字节数
			this->m_dwNumBytesReceived += dwLen + 28;

			// packet header
			UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;

			LOG(pOStream, ELOG_LEVEL_DEBUG3) << "status:" << (int)packet.m_btStatus << ", syn:" << (int)packet.m_btSyn << ", ack:" << (int)packet.m_btAck << "\n";

			//收到一个有效的ack 那么要更新发送窗口的状态
			if (this->m_oSendWindow.IsValidIndex(packet.m_btAck))
			{
				// 获得到的是一个有效的包
				this->m_dAckRecvTime = dTime;
				this->m_dwAckTimeoutRetry = 3;

				// 用于计算delay的因子
				static const double err_factor = 0.125;
				static const double average_factor = 0.25;
				// static const double retry_factor = 2;
				static const double retry_factor = 1.5;

				double rtt = m_dDelayTime;
				double dErrTime = 0;

				// m_SendWindowControl 不会比m_SendWindowControl 更大
				double send_window_control_max = this->m_dSendWindowControl * 2;
				if (send_window_control_max > _SendWindow::window_size)
				{
					send_window_control_max = _SendWindow::window_size;
				}

				while (this->m_oSendWindow.m_btBegin != (unsigned char)(packet.m_btAck + 1))
				{
					unsigned char id = this->m_oSendWindow.m_btBegin % _SendWindow::window_size;
					unsigned char btBufferId = this->m_oSendWindow.m_btarrSeqBufferId[id];

					//只使用没有重发的包计算延迟
					if (this->m_oSendWindow.m_btarrSeqBufferId[id] == 1)
					{
						// rtt(收包延迟时间)
						rtt = dTime - this->m_oSendWindow.m_darrSeqTime[id];
						//使用 rtt 与 m_dDelayTime 获取 dErrTime
						dErrTime = rtt - m_dDelayTime;
						//使用 dErrTime 重新计算 m_dDelayTime
						this->m_dDelayTime = this->m_dDelayTime + err_factor * dErrTime;
						//使用 dErrTime 重新计算 m_dDelayAverage
						this->m_dDelayAverage = this->m_dDelayAverage + average_factor * (fabs(dErrTime) - this->m_dDelayAverage);
					}

					// 释放缓存
					this->m_oSendWindow.m_btarrBuffer[btBufferId][0] = this->m_oSendWindow.m_btFreeBufferId;
					this->m_oSendWindow.m_btFreeBufferId = btBufferId;
					this->m_oSendWindow.m_btBegin++;

					//收到新的ack
					//如果 m_SendWindowControl 比 m_dSendWindowThreshhold 大 为拥塞避免
					//否则为慢启动
					//拥塞避免中 m_SendWindowControl 每次增加 1 / m_dSendWindowControl
					//慢启动中 m_SendWindowControl 每次增加 1
					if (this->m_dSendWindowControl <= this->m_dSendWindowThreshhold)
					{
						this->m_dSendWindowControl += 1;
					}
					else
					{
						this->m_dSendWindowControl += 1 / this->m_dSendWindowControl;
					}

					if (this->m_dSendWindowControl > send_window_control_max)
					{
						this->m_dSendWindowControl = send_window_control_max;
					}
				}

				//使用 m_dDelayTime 和 m_dDelayAverage 计算重试的时间
				this->m_dRetryTime = this->m_dDelayTime + retry_factor * this->m_dDelayAverage;
				if (this->m_dRetryTime < this->m_dSendFrequency) this->m_dRetryTime = this->m_dSendFrequency;
			}

			//收到相同ack(超过3次要开始选择重传)
			if (this->m_btAckLast == this->m_oSendWindow.m_btBegin - 1)
			{
				this->m_dwAckSameCount++;
			}
			else
			{
				this->m_dwAckSameCount = 0;
			}

			//接收到的是一个有效的包
			if (this->m_oRecvWindow.IsValidIndex(packet.m_btSyn))
			{
				unsigned char btId = packet.m_btSyn % _RecvWindow::window_size;

				LOG(pOStream, ELOG_LEVEL_DEBUG3) << "recv new:" << (int)packet.m_btSyn
					<< ", m_oRecvWindow.m_btarrSeqBufferId[btId]" << (int)this->m_oRecvWindow.m_btarrSeqBufferId[btId] << "\n";

				if (this->m_oRecvWindow.m_btarrSeqBufferId[btId] >= _RecvWindow::window_size)
				{
					this->m_oRecvWindow.m_btarrSeqBufferId[btId] = btBufferId;
					this->m_oRecvWindow.m_warrSeqSize[btId] = dwLen;
					bPacketReceived = true;

					// 没有更多的接收缓冲 那么先处理接收
					if (this->m_oRecvWindow.m_btFreeBufferId >= _RecvWindow::window_size) { break; }
					else { continue; }
				}
			}

			// 释放buffer
			pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
			this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
		}

		if (this->m_oSendWindow.m_btBegin == this->m_oSendWindow.m_btEnd) { this->m_dwAckSameCount = 0; }

		// 记录最后一个ack
		this->m_btAckLast = this->m_oSendWindow.m_btBegin - 1;

		// 更新接收窗口
		if (bPacketReceived)
		{
			unsigned char btLastAck = this->m_oRecvWindow.m_btBegin - 1;
			unsigned char btNewAck = btLastAck;
			//bool bParseMessage = false;

			//计算新的ack
			for (unsigned char i = this->m_oRecvWindow.m_btBegin; i != this->m_oRecvWindow.m_btEnd; i++)
			{
				// 接收到的buff是否有效
				if (this->m_oRecvWindow.m_btarrSeqBufferId[i % _RecvWindow::window_size] >= _RecvWindow::window_size)
					break;

				btNewAck = i;

				LOG(pOStream, ELOG_LEVEL_DEBUG3) << "recv new_ack:" << (int)btNewAck << "\n";
			}

			// 有新的ack
			if (btNewAck != btLastAck)
			{
				while (this->m_oRecvWindow.m_btBegin != (unsigned char)(btNewAck + 1))
				{
					const unsigned char cbtHeadSize = sizeof(UDPPacketHeader);
					unsigned char btId = this->m_oRecvWindow.m_btBegin % _RecvWindow::window_size;
					unsigned char btBufferId = this->m_oRecvWindow.m_btarrSeqBufferId[btId];
					unsigned char* pBuffer = this->m_oRecvWindow.m_btarrBuffer[btBufferId] + cbtHeadSize;
					unsigned short wSize = this->m_oRecvWindow.m_warrSeqSize[btId] - cbtHeadSize;

					(*this->m_pOnRecvOperator)((char*)pBuffer, wSize, refError, pOStream);
					if (refError)
					{
						return *this;
					}

					// 接收成功 释放缓存
					this->m_oRecvWindow.m_btarrBuffer[btBufferId][0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;

					// 从队列移除
					this->m_oRecvWindow.m_warrSeqSize[btId] = 0;
					this->m_oRecvWindow.m_btarrSeqBufferId[btId] = _RecvWindow::window_size;
					this->m_oRecvWindow.m_btBegin++;
					this->m_oRecvWindow.m_btEnd++;

					// 是否可以解析
					//bParseMessage = true;

					// 需要发送ack
					this->m_bSendAck = true;
				}
			}

			// 标记最后一个syn
			this->m_btSynLast = this->m_oRecvWindow.m_btBegin - 1;
		}

		return *this;
	}

} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__

