#ifndef __BUFF_CONTRAL_H__
#define __BUFF_CONTRAL_H__

#include "include/sliding_window_def.h"
#include "sliding_window.h"
#include "iothread.h"

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
	class OnRecvOperator
	{
	public:
		virtual ~OnRecvOperator() {};

		/**
		 * @brief 接收函数
		 *
		 * @param szBuff 接收的数据
		 * @param dwSize 接收的长度
		 * @return int 错误码
		 */
		virtual int operator() (char* szBuff, unsigned short wSize, std::ostream* refOStream) = 0;
	};

	class OnConnectedOperator
	{
	public:
		virtual ~OnConnectedOperator() {};
		/**
		 * @brief 连接处理
		 *
		 */
		virtual int operator() (std::ostream* refOStream) = 0;
	};

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
		 * @return int 错误码
		 */
		virtual int operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* refOStream) = 0;
	};

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
		 * @return int 错误码
		 */
		virtual int operator() (char* szBuff, unsigned short wBufferSize, unsigned short& wSendLen, std::ostream* refOStream) = 0;
	};

	enum ConnectionStatus
	{
		ST_IDLE,
		ST_SYN_SEND,		//使用
		ST_SYN_RECV,		//使用
		ST_SYN_RECV_WAIT,
		ST_ESTABLISHED,		//使用
		ST_FIN_WAIT_1,
		ST_FIN_WAIT_2,
	};

	template <unsigned short BUFF_SIZE = 512, unsigned short WINDOW_SIZE = 32>
	class BufferContral
	{
	public:
		enum { buff_size = BUFF_SIZE };
		enum { window_size = WINDOW_SIZE };

		typedef SendWindow<buff_size, window_size> _SendWindow;
		typedef SlidingWindow<buff_size, window_size> _RecvWindow;
		typedef BufferContral<buff_size, window_size> BUFF_CONTRAL;

		BufferContral();
		~BufferContral();

		int Init(int dwState);

		BufferContral& SetOnRecvOperator(OnRecvOperator* p);
		BufferContral& SetOnConnectedOperator(OnConnectedOperator* p);

		BufferContral& SetRecvOperator(RecvOperator* p);
		// TODO是否需要 不需要后面删掉
		BufferContral& SetSendOperator(SendOperator* p);

		BufferContral& SetAckOutTime(double dOutTime);

		/**
		 * @brief 将数据放入发送缓冲区
		 *
		 * @param pSendBuffer 待发送数据
		 * @param wSize 要发送的长度
		 * @return unsigned short 放入发送缓冲的长度 <= wSize
		 */
		unsigned short Send(const char* pSendBuffer, unsigned short wSize, double dTime);

		//TCP不需要?
		int SendMessages(double dTime, std::ostream* pOStream);

		int ReceiveMessages(double dTime, std::ostream* refOStream);
		
	private:
		_SendWindow m_oSendWindow;
		_RecvWindow m_oRecvWindow;

		OnRecvOperator* m_pOnRecvOperator;
		OnConnectedOperator* m_pOnConnectedOperator;
		RecvOperator* m_pRecvOperator;
		SendOperator* m_pSendOperator;

		// bytes send and recieved
		unsigned int m_dwNumBytesSend;
		unsigned int m_dwNumBytesReceived;

		// network delay time.
		double m_dDelayTime;
		double m_dDelayAverage;
		double m_dRetryTime;
		double m_dSendTime;
		double m_dSendFrequency;
		double m_dSendWindowControl;
		double m_dSendWindowThreshhold;
		double m_dSendDataTime;
		double m_dSendDataFrequency;

		// packets count send and retry
		unsigned int m_dwNumPacketsSend;
		unsigned int m_dwNumPacketsRetry;

		bool m_bConnected;

	private:
		double m_dAckRecvTime;
		int m_dwAckTimeoutRetry;
		unsigned int m_dwStatus;

		int m_dwAckSameCount;
		bool m_bQuickRetry;
		bool m_bSendAck;
		unsigned char m_btAckLast;
		unsigned char m_btSynLast;

		double m_dAckOutTime; //默认是5
	};

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::BufferContral()
	{ }

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::~BufferContral()
	{ }

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::Init(int dwState)
	{
		m_dwStatus = dwState;
		m_dDelayTime = 0;
		m_dDelayAverage = 3 * m_dSendFrequency;
		m_dRetryTime = m_dDelayTime + 2 * m_dDelayAverage;
		m_dSendTime = 0;

		m_dAckRecvTime = FxIoModule::Instance()->FxGetCurrentTime();
		m_dwAckTimeoutRetry = 1;
		m_dwAckSameCount = 0;
		m_bQuickRetry = false;
		m_dSendDataTime = 0;

		m_btAckLast = 0;
		m_btSynLast = 0;
		m_bSendAck = false;
		m_dSendWindowControl = 1;
		m_dSendWindowThreshhold = _SendWindow::window_size;

		// 清空滑动窗口
		m_oRecvWindow.ClearBuffer();
		m_oSendWindow.ClearBuffer();

		// 初始化发送窗口
		m_oSendWindow.m_btBegin = 1;
		m_oSendWindow.m_btEnd = m_oSendWindow.m_btBegin;

		// 初始化接收窗口
		m_oRecvWindow.m_btBegin = 1;
		m_oRecvWindow.m_btEnd = m_oRecvWindow.m_btBegin + _RecvWindow::window_size;

		m_bConnected = false;
		return 0;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline  BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetOnRecvOperator(OnRecvOperator* p)
	{
		m_pOnRecvOperator = p;

		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline  BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetOnConnectedOperator(OnConnectedOperator* p)
	{
		m_pOnConnectedOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetRecvOperator(RecvOperator* p)
	{
		m_pRecvOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetSendOperator(SendOperator* p)
	{
		m_pSendOperator = p;

		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetAckOutTime(double dOutTime)
	{
		m_dAckOutTime = dOutTime;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline unsigned short BufferContral<BUFF_SIZE, WINDOW_SIZE>
		::Send(const char* pSendBuffer, unsigned short wSize, double dTime)
	{
		unsigned short wSendSize = 0;
		while ((m_oSendWindow.m_btFreeBufferId < _SendWindow::window_size) && // there is a free buffer
			(wSize > 0))
		{
			//长度不会大于拥塞窗口
			if (m_oSendWindow.m_btEnd - m_oSendWindow.m_btBegin > m_dSendWindowControl) break;

			unsigned char btId = m_oSendWindow.m_btEnd % _SendWindow::window_size;

			// 获取一个帧
			unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
			m_oSendWindow.m_btFreeBufferId = m_oSendWindow.m_btarrBuffer[btBufferId][0];

			// 发送的缓存
			unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

			// 处理数据头
			UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
			oPacket.m_btStatus = m_dwStatus;
			oPacket.m_btSyn = m_oSendWindow.m_btEnd;
			oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

			// 复制数据
			unsigned int dwCopyOffset = sizeof(oPacket);
			unsigned int dwCopySize = _SendWindow::buff_size - dwCopyOffset;
			if (dwCopySize > wSize)
				dwCopySize = wSize;

			if (dwCopySize > 0)
			{
				memcpy((void*)(pBuffer + dwCopyOffset), pSendBuffer, dwCopySize);

				wSize -= dwCopySize;
				wSendSize += dwCopySize;
			}

			// 添加到发送窗口
			m_oSendWindow.Add2SendWindow(btId, btBufferId, dwCopySize + dwCopyOffset, dTime, m_dRetryTime);
		}

		return wSendSize;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::SendMessages(double dTime, std::ostream* pOStream)
	{
		// check ack received time
		if (dTime - m_dAckRecvTime > m_dAckOutTime)
		{
			m_dAckRecvTime = dTime;

			if (--m_dwAckTimeoutRetry <= 0)
			{
				//TODO
				// OnClose();
				return 1;
			}
		}

		if (dTime < m_dSendTime) { return 0; }

		// if (m_dwStatus == ST_SYN_RECV) {return 0;}

		bool bForceRetry = false;

		// if (m_dwStatus == ST_ESTABLISHED)
		{
			//3次相同ack 开始快速重传
			if (m_dwAckSameCount > 3)
			{
				if (m_bQuickRetry == false)
				{
					m_bQuickRetry = true;
					bForceRetry = true;

					m_dSendWindowThreshhold = m_dSendWindowControl / 2;
					if (m_dSendWindowThreshhold < 2) { m_dSendWindowThreshhold = 2; }

					m_dSendWindowControl = m_dSendWindowThreshhold + m_dwAckSameCount - 1;
					if (m_dSendWindowControl > _SendWindow::window_size)
					{
						m_dSendWindowControl = _SendWindow::window_size;
					}
				}
				else
				{
					//相同ack时 拥塞控制窗口+1
					m_dSendWindowControl += 1;
					if (m_dSendWindowControl > _SendWindow::window_size)
					{
						m_dSendWindowControl = _SendWindow::window_size;
					}
				}
			}
			else
			{
				//有新的ack 那么快速重传结束
				if (m_bQuickRetry == true)
				{
					m_dSendWindowControl = m_dSendWindowThreshhold;
					m_bQuickRetry = false;
				}
			}

			//如果超时(超过rto) 那么重新进入慢启动
			for (unsigned char i = m_oSendWindow.m_btBegin; i != m_oSendWindow.m_btEnd; i++)
			{
				unsigned char btId = i % _SendWindow::window_size;
				unsigned short size = m_oSendWindow.m_warrSeqSize[btId];

				if (m_oSendWindow.m_dwarrSeqRetryCount[btId] > 0
					&& dTime >= m_oSendWindow.m_warrSeqSize[btId])
				{
					m_dSendWindowThreshhold = m_dSendWindowControl / 2;
					if (m_dSendWindowThreshhold < 2) { m_dSendWindowThreshhold = 2; }
					//m_SendWindowControl = 1;
					m_dSendWindowControl = m_dSendWindowThreshhold;
					//break;

					m_bQuickRetry = false;
					m_dwAckSameCount = 0;
					break;
				}
			}

		}

		//没有数据发送 那么创建一个空的 用来同步窗口数据
		if (m_oSendWindow.m_btBegin == m_oSendWindow.m_btEnd)
		{
			if (dTime >= m_dSendDataTime)
			{
				if (m_oSendWindow.m_btFreeBufferId < m_oSendWindow.window_size)
				{
					unsigned char btId = m_oSendWindow.m_btEnd % m_oSendWindow.window_size;

					// 获取缓存
					unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
					m_oSendWindow.m_btFreeBufferId = m_oSendWindow.m_btarrBuffer[btBufferId][0];
					unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

					// packet header
					UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
					oPacket.m_btStatus = m_dwStatus;
					oPacket.m_btSyn = m_oSendWindow.m_btEnd;
					oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

					// 添加到发送窗口
					m_oSendWindow.Add2SendWindow(btId, btBufferId, sizeof(oPacket), dTime, m_dRetryTime);
				}
			}
		}
		else { m_dSendDataTime = dTime + m_dSendDataFrequency; }

		//如果有待发送数据 那么 先发送待发送数据
		if (m_oSendWindow.m_btpWaitSendBuff)
		{
			unsigned short wLen = 0;
			int dwErrorCode = (*m_pSendOperator)((char*)m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen, pOStream);
			if (dwErrorCode)
			{
				//TODO 发送出错 断开连接
				return dwErrorCode;
			}
			else
			{
				m_oSendWindow.m_btpWaitSendBuff += wLen;
				m_oSendWindow.m_dwWaitSendSize -= wLen;
				if (0 == m_oSendWindow.m_dwWaitSendSize)
				{
					m_oSendWindow.m_btpWaitSendBuff = NULL;
				}
				m_dwNumBytesSend += wLen;
			}
		}
		//如果待发送数据已发 那么就继续发送
		if (!m_oSendWindow.m_btpWaitSendBuff)
		{
			//开始发送
			for (unsigned char i = m_oSendWindow.m_btBegin; i != m_oSendWindow.m_btEnd; i++)
			{
				//如果发送长度超过拥塞窗口 就停止
				if (i - m_oSendWindow.m_btBegin >= m_dSendWindowControl) { break; }

				unsigned char btId = i % _SendWindow::window_size;
				unsigned short wSize = m_oSendWindow.m_warrSeqSize[btId];

				//开始发送 或者 重传
				if (dTime >= m_oSendWindow.m_darrSeqRetry[btId] || bForceRetry)
				{
					bForceRetry = false;

					unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[m_oSendWindow.m_btarrSeqBufferId[btId]];

					// packet header
					UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;
					packet.m_btStatus = m_dwStatus;
					packet.m_btSyn = i;
					packet.m_btAck = m_oRecvWindow.m_btBegin - 1;

					unsigned short wLen = 0;
					int dwErrorCode = (*m_pSendOperator)((char*)m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen, pOStream);
					if (dwErrorCode)
					{
						if (EAGAIN == dwErrorCode || EINTR == dwErrorCode)
						{
							break;
						}
						//TODO 发送出错 断开连接
						return dwErrorCode;
					}
					else
					{
						m_oSendWindow.m_btpWaitSendBuff = pBuffer + wLen;
						m_oSendWindow.m_dwWaitSendSize = wSize - wLen;
						if (0 == m_oSendWindow.m_dwWaitSendSize)
						{
							m_oSendWindow.m_btpWaitSendBuff = NULL;
						}
						else
						{
							//等待下次发送
							break;
						}
						m_dwNumBytesSend += wLen;
					}

					// 发送包数量
					m_dwNumPacketsSend++;

					// 重试次数
					if (dTime != m_oSendWindow.m_darrSeqTime[btId]) { m_dwNumPacketsRetry++; }

					m_dSendTime = dTime + m_dSendFrequency;
					m_dSendDataTime = dTime + m_dSendDataFrequency;
					m_bSendAck = false;

					m_oSendWindow.m_dwarrSeqRetryCount[btId]++;
					//m_oSendWindow.m_darrSeqRetryTime[id] *= 2;
					m_oSendWindow.m_darrSeqRetryTime[btId] = 1.5 * m_dRetryTime;
					if (m_oSendWindow.m_darrSeqRetryTime[btId] > 0.2) m_oSendWindow.m_darrSeqRetryTime[btId] = 0.2;
					m_oSendWindow.m_darrSeqRetry[btId] = dTime + m_oSendWindow.m_darrSeqRetryTime[btId];
				}
			}
		}

		// send ack
		if (!m_oSendWindow.m_btpWaitSendBuff && m_bSendAck)
		{
			m_oSendWindow.m_oSendAckPacket.m_btStatus = m_dwStatus;
			m_oSendWindow.m_oSendAckPacket.m_btSyn = m_oSendWindow.m_btBegin - 1;
			m_oSendWindow.m_oSendAckPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

			m_oSendWindow.m_btpWaitSendBuff = (unsigned char*)(&m_oSendWindow.m_oSendAckPacket);
			m_oSendWindow.m_dwWaitSendSize = sizeof(m_oSendWindow.m_oSendAckPacket);

			unsigned short wLen = 0;
			int dwErrorCode = (*m_pSendOperator)((char*)m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen, pOStream);
			if (dwErrorCode)
			{
				//TODO 发送出错 断开连接
				return dwErrorCode;
			}
			else
			{
				m_oSendWindow.m_btpWaitSendBuff += wLen;
				m_oSendWindow.m_dwWaitSendSize -= wLen;
				if (0 == m_oSendWindow.m_dwWaitSendSize)
				{
					m_oSendWindow.m_btpWaitSendBuff = NULL;
				}
			}

			m_dSendTime = dTime + m_dSendFrequency;
			m_bSendAck = false;
		}

		return 0;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::ReceiveMessages(double dTime, std::ostream* pOStream)
	{
		// packet received
		bool bPacketReceived = false;
		bool bCloseConnection = false;

		bool bRecvAble = true;
		while (bRecvAble)
		{
			// allocate buffer
			unsigned char btBufferId = m_oRecvWindow.m_btFreeBufferId;
			unsigned char* pBuffer = m_oRecvWindow.m_btarrBuffer[btBufferId];
			m_oRecvWindow.m_btFreeBufferId = pBuffer[0];

			// can't allocate buffer, disconnect.
			if (btBufferId >= m_oRecvWindow.window_size)
			{
				return CODE_ERROR_NET_UDP_ALLOC_BUFF;
			}

			int dwLen = 0;
			int dwErrorCode = (*m_pRecvOperator)((char*)pBuffer, _RecvWindow::buff_size, dwLen, pOStream);

			if (dwErrorCode && (EAGAIN != dwErrorCode))
			{
				if (EINTR == dwErrorCode)
				{
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					continue;
				}

#ifdef _WIN32
				if (WSA_IO_PENDING == dwErrorCode || CODE_SUCCESS_NO_BUFF_READ == dwErrorCode)
#else
				if (EAGAIN == dwErrorCode || EWOULDBLOCK == dwErrorCode)
#endif	//_WIN32
				{
					bRecvAble = false;
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}

				return dwErrorCode;
			}

			if (dwLen < (unsigned short)sizeof(UDPPacketHeader))
			{
				bRecvAble = false;
				pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
				m_oRecvWindow.m_btFreeBufferId = btBufferId;
				continue;
			}
			if (!m_bConnected)
			{
				if (dwLen > (unsigned short)sizeof(UDPPacketHeader))
				{
					bRecvAble = false;
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}
				UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;
				m_dwStatus = ST_ESTABLISHED;
				if (ST_ESTABLISHED == packet.m_btStatus)
				{
					(*m_pOnConnectedOperator)(pOStream);
					m_bConnected = true;
				}
			}

			// num unsigned chars received
			m_dwNumBytesReceived += dwLen + 28;

			// packet header
			UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;

			//收到一个有效的ack 那么要更新发送窗口的状态
			if (m_oSendWindow.IsValidIndex(packet.m_btAck))
			{
				// 获得到的是一个有效的包
				m_dAckRecvTime = dTime;
				m_dwAckTimeoutRetry = 3;

				// static value for calculate delay
				static const double err_factor = 0.125;
				static const double average_factor = 0.25;
				static const double retry_factor = 2;

				double rtt = m_dDelayTime;
				double dErrTime = 0;

				// m_SendWindowControl not more than double m_SendWindowControl 
				double send_window_control_max = m_dSendWindowControl * 2;
				if (send_window_control_max > _SendWindow::window_size)
				{
					send_window_control_max = _SendWindow::window_size;
				}

				while (m_oSendWindow.m_btBegin != (unsigned char)(packet.m_btAck + 1))
				{
					unsigned char id = m_oSendWindow.m_btBegin % _SendWindow::window_size;
					unsigned char btBufferId = m_oSendWindow.m_btarrSeqBufferId[id];

					//只使用没有重发的包计算延迟
					if (m_oSendWindow.m_btarrSeqBufferId[id] == 1)
					{
						// rtt(收包延迟时间)
						rtt = dTime - m_oSendWindow.m_darrSeqTime[id];
						//使用 rtt 与 m_dDelayTime 获取 dErrTime
						dErrTime = rtt - m_dDelayTime;
						//使用 dErrTime 重新计算 m_dDelayTime
						m_dDelayTime = m_dDelayTime + err_factor * dErrTime;
						//使用 dErrTime 重新计算 m_dDelayAverage
						m_dDelayAverage = m_dDelayAverage + average_factor * (fabs(dErrTime) - m_dDelayAverage);
					}

					// 释放缓存
					m_oSendWindow.m_btarrBuffer[btBufferId][0] = m_oSendWindow.m_btFreeBufferId;
					m_oSendWindow.m_btFreeBufferId = btBufferId;
					m_oSendWindow.m_btBegin++;

					//收到新的ack
					//如果 m_SendWindowControl 比 m_dSendWindowThreshhold 大 为拥塞避免
					//否则为慢启动
					//拥塞避免中 m_SendWindowControl 每次增加 1 / m_dSendWindowControl
					//慢启动中 m_SendWindowControl 每次增加 1
					if (m_dSendWindowControl <= m_dSendWindowThreshhold)
					{
						m_dSendWindowControl += 1;
					}
					else
					{
						m_dSendWindowControl += 1 / m_dSendWindowControl;
					}

					if (m_dSendWindowControl > send_window_control_max)
					{
						m_dSendWindowControl = send_window_control_max;
					}
				}

				//使用 m_dDelayTime 和 m_dDelayAverage 计算重试的时间
				m_dRetryTime = m_dDelayTime + retry_factor * m_dDelayAverage;
				if (m_dRetryTime < m_dSendFrequency) m_dRetryTime = m_dSendFrequency;
			}

			//收到相同ack(超过3次要开始选择重传)
			if (m_btAckLast == m_oSendWindow.m_btBegin - 1)
			{
				m_dwAckSameCount++;
			}
			else
			{
				m_dwAckSameCount = 0;
			}

			//接收到的是一个有效的包
			if (m_oRecvWindow.IsValidIndex(packet.m_btSyn))
			{
				unsigned char btId = packet.m_btSyn % _RecvWindow::window_size;

				if (m_oRecvWindow.m_btarrSeqBufferId[btId] >= _RecvWindow::window_size)
				{
					m_oRecvWindow.m_btarrSeqBufferId[btId] = btBufferId;
					m_oRecvWindow.m_warrSeqSize[btId] = dwLen;
					bPacketReceived = true;

					// 没有更多的接收缓冲 那么先处理接收
					if (m_oRecvWindow.m_btFreeBufferId >= _RecvWindow::window_size) { break; }
					else { continue; }
				}
			}

			// 释放buffer
			pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
			m_oRecvWindow.m_btFreeBufferId = btBufferId;
		}

		if (m_oSendWindow.m_btBegin == m_oSendWindow.m_btEnd) { m_dwAckSameCount = 0; }

		// 记录最后一个ack
		m_btAckLast = m_oSendWindow.m_btBegin - 1;

		// 更新接收窗口
		if (bPacketReceived)
		{
			unsigned char btLastAck = m_oRecvWindow.m_btBegin - 1;
			unsigned char btNewAck = btLastAck;
			bool bParseMessage = false;

			//计算新的ack
			for (unsigned char i = m_oRecvWindow.m_btBegin; i != m_oRecvWindow.m_btEnd; i++)
			{
				// 接收到的buff是否有效
				if (m_oRecvWindow.m_btarrSeqBufferId[i % _RecvWindow::window_size] >= _RecvWindow::window_size)
					break;

				btNewAck = i;
			}

			// 有新的ack
			if (btNewAck != btLastAck)
			{
				while (m_oRecvWindow.m_btBegin != (unsigned char)(btNewAck + 1))
				{
					const unsigned char cbtHeadSize = sizeof(UDPPacketHeader);
					unsigned char btId = m_oRecvWindow.m_btBegin % _RecvWindow::window_size;
					unsigned char btBufferId = m_oRecvWindow.m_btarrSeqBufferId[btId];
					unsigned char* pBuffer = m_oRecvWindow.m_btarrBuffer[btBufferId] + cbtHeadSize;
					unsigned short wSize = m_oRecvWindow.m_warrSeqSize[btId] - cbtHeadSize;

					if ((*m_pOnRecvOperator)((char*)pBuffer, wSize, pOStream))
					{
						//TODO
						break;
					}

					// 接收成功 释放缓存
					m_oRecvWindow.m_btarrBuffer[btBufferId][0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;

					// 从队列移除
					m_oRecvWindow.m_warrSeqSize[btId] = 0;
					m_oRecvWindow.m_btarrSeqBufferId[btId] = _RecvWindow::window_size;
					m_oRecvWindow.m_btBegin++;
					m_oRecvWindow.m_btEnd++;

					// 是否可以解析
					bParseMessage = true;

					// 需要发送ack
					m_bSendAck = true;
				}
			}

			// 标记最后一个syn
			m_btSynLast = m_oRecvWindow.m_btBegin - 1;
		}

		return 0;
	}

} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__

