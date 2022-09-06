#ifndef __BUFF_CONTRAL_H__
#define __BUFF_CONTRAL_H__

#include "include/sliding_window_def.h"
#include "sliding_window.h"

namespace FXNET
{
	template <unsigned int SEND_BUFF_SIZE = 512, unsigned int SEND_WINDOW_SIZE = 32
		, unsigned int RECV_BUFF_SIZE = 512, unsigned int RECV_WINDOW_SIZE = 32>
	class BuffContral
	{
	public:
		enum { send_buff_size = SEND_BUFF_SIZE };
		enum { send_window_size = SEND_WINDOW_SIZE };
		enum { recv_buff_size = RECV_BUFF_SIZE };
		enum { recv_window_size = RECV_WINDOW_SIZE };

		typedef SendWindow<SEND_BUFF_SIZE, SEND_WINDOW_SIZE> SendWindow;
		typedef SlidingWindow<RECV_BUFF_SIZE, RECV_WINDOW_SIZE> RecvWindow;
		typedef BuffContral<SEND_BUFF_SIZE, SEND_WINDOW_SIZE, RECV_BUFF_SIZE, RECV_WINDOW_SIZE> BUFF_CONTRAL;

		BuffContral()
		{
			//TODO
		}
		~BuffContral()
		{
			//TODO
		}

		BUFF_CONTRAL& SetOnRecvOperator(OnRecvOperator* p)
		{
			m_pOnRecvOperator = p;

			return *this;
		}

		// TODO是否需要 不需要后面删掉
		BUFF_CONTRAL& SetSendOperator(SendOperator* p)
		{
			m_pSendOperator = p;

			return *this;
		}

		BUFF_CONTRAL& SetAckOutTime(double dOutTime)
		{
			m_dAckOutTime = dOutTime;
		}

		/**
		 * @brief 将数据放入发送缓冲区
		 * 
		 * @param pSendBuffer 待发送数据
		 * @param wSize 要发送的长度
		 * @return unsigned short 放入发送缓冲的长度 <= wSize
		 */
		unsigned short Send(const char* pSendBuffer, unsigned short wSize, double dTime)
		{
			unsigned short wSendSize = 0;
			// put buffer to send window
			while ((m_oSendWindow.m_btFreeBufferId < m_oSendWindow::window_size) && // there is a free buffer
				   (wSize > 0))
			{
				// if send window more than m_SendWindowControl, break
				if (m_oSendWindow.m_btEnd - m_oSendWindow.m_btBegin > m_dSendWindowControl)
					break;

				unsigned char id = m_oSendWindow.m_btEnd % m_oSendWindow::window_size;

				// 获取一个帧
				unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
				m_oSendWindow.m_btFreeBufferId = m_oSendWindow.m_btarrBuffer[btBufferId][0];

				// 发送的缓存
				unsigned char *pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

				// 处理数据头
				PacketHeader &oPacket = *(PacketHeader *)pBuffer;
				oPacket.m_btStatus = m_dwStatus;
				oPacket.m_btSyn = m_oSendWindow.m_btEnd;
				oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

				// 复制数据
				unsigned int dwCopyOffset = sizeof(oPacket);
				unsigned int dwCopySize = m_oSendWindow::buffer_size - dwCopyOffset;
				if (dwCopySize > wSize)
					dwCopySize = wSize;

				if (dwCopySize > 0)
				{
					memcpy(pBuffer + dwCopyOffset, pSendBuffer, dwCopySize);

					wSize -= dwCopySize;
					wSendSize += dwCopySize;
				}

				// 添加到发送窗口
				m_oSendWindow.Add2SendWindow(id, btBufferId, dwCopySize + dwCopyOffset, dTime, m_dRetryTime);
			}

			return wSendSize;
		}

		//TCP不需要?
		int SendMessages(double dTime)
		{
			// check ack received time
			if (dTime - m_dAckRecvTime > m_dAckOutTime)
			{
				m_dAckRecvTime = dTime;

				if (--m_dwAckTimeoutRetry <= 0)
				{
					//TODO
					OnClose();
					return 1;
				}
			}

			if (dTime < m_dSendTime)
				return 0;

			if (m_dwStatus == ST_SYN_RECV)
				return 0;

			bool bForceRetry = false;

			if (m_dwStatus == ST_ESTABLISHED)
			{
				//3次相同ack 开始快速重传
				if (m_dwAckSameCount > 3)
				{
					if (m_bQuickRetry == false)
					{
						m_bQuickRetry = true;
						bForceRetry = true;

						m_dSendWindowThreshhold = m_dSendWindowControl / 2;
						if (m_dSendWindowThreshhold < 2) m_dSendWindowThreshhold = 2;
						m_dSendWindowControl = m_dSendWindowThreshhold + m_dwAckSameCount - 1;
						if (m_dSendWindowControl > m_oSendWindow.window_size)
							m_dSendWindowControl = m_oSendWindow.window_size;
					}
					else
					{
						//相同ack时 拥塞控制窗口+1
						m_dSendWindowControl += 1;
						if (m_dSendWindowControl > m_oSendWindow.window_size)
							m_dSendWindowControl = m_oSendWindow.window_size;
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
					unsigned char id = i % m_oSendWindow.window_size;
					ushort size = m_oSendWindow.seq_size[id];

					if (m_oSendWindow.seq_retry_count[id] > 0
						&& dTime >= m_oSendWindow.seq_retry[id])
					{
						m_dSendWindowThreshhold = m_dSendWindowControl / 2;
						if (m_dSendWindowThreshhold < 2) m_dSendWindowThreshhold = 2;
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

						// allocate buffer
						unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
						m_oSendWindow.m_btFreeBufferId = m_oSendWindow.buffer[btBufferId][0];

						// send window buffer
						unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

						// packet header
						PacketHeader& oPacket = *(PacketHeader*)pBuffer;
						oPacket.m_btStatus = m_dwStatus;
						oPacket.m_btSyn = m_oSendWindow.end;
						oPacket.m_btAck = m_oRecvWindow.begin - 1;

						// 添加到发送窗口
						m_oSendWindow.Add2SendWindow(btId, btBufferId, sizeof(oPacket), dTime, m_dRetryTime);
					}
				}
			}
			else
				m_dSendDataTime = dTime + m_dSendDataFrequency;

			//如果有待发送数据 那么 先发送待发送数据
			if (m_oSendWindow.m_btpWaitSendBuff)
			{
				unsigned short wLen = 0;
				int dwErrorCode = (*m_pSendOperator)(m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen)
				if (dwErrorCode)
				{
					//TODO 发送出错 断开连接
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
				// send packets
				for (unsigned char i = m_oSendWindow.m_btBegin; i != m_oSendWindow.m_btEnd; i++)
				{
					// if send packets more than m_SendWindowControl, break
					if (i - m_oSendWindow.m_btBegin >= m_dSendWindowControl)
						break;

					unsigned char btId = i % m_oSendWindow.window_size;
					ushort size = m_oSendWindow.seq_size[btId];

					// send packet
					if (dTime >= m_oSendWindow.seq_retry[btId] || bForceRetry)
					{
						bForceRetry = false;

						unsigned char* buffer = m_oSendWindow.m_btarrBuffer[m_oSendWindow.m_btarrSeqBufferId[btId]];

						// packet header
						PacketHeader& packet = *(PacketHeader*)buffer;
						packet.m_btStatus = m_dwStatus;
						packet.m_btSyn = i;
						packet.m_btAck = m_oRecvWindow.begin - 1;

						unsigned short wLen = 0;
						int dwErrorCode = (*m_pSendOperator)(m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen)
						if (dwErrorCode)
						{
							//TODO 发送出错 断开连接

							break;
						}
						else
						{
							m_oSendWindow.m_btpWaitSendBuff = buffer + wLen;
							m_oSendWindow.m_dwWaitSendSize = size - wLen;
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

						// num send
						m_dwNumPacketsSend++;

						// num retry send
						if (dTime != m_oSendWindow.seq_time[btId])
							m_dwNumPacketsRetry++;

						m_dSendTime = dTime + m_dSendFrequency;
						m_dSendDataTime = dTime + m_dSendDataFrequency;
						m_bSendAck = false;

						m_oSendWindow.seq_retry_count[btId]++;
						//m_oSendWindow.seq_retry_time[id] *= 2;
						m_oSendWindow.seq_retry_time[btId] = 1.5 * m_dRetryTime;
						if (m_oSendWindow.seq_retry_time[btId] > 0.2) m_oSendWindow.seq_retry_time[btId] = 0.2;
						m_oSendWindow.seq_retry[btId] = dTime + m_oSendWindow.seq_retry_time[btId];
					}
				}
			}
			

			// send ack
			if (!m_oSendWindow.m_btpWaitSendBuff && m_bSendAck)
			{
				m_oSendWindow.m_oSendAckPacket.m_btStatus = m_dwStatus;
				m_oSendWindow.m_oSendAckPacket.m_btSyn = m_oSendWindow.begin - 1;
				m_oSendWindow.m_oSendAckPacket.m_btAck = m_oRecvWindow.begin - 1;

				m_oSendWindow.m_btpWaitSendBuff = (unsigned char*)(&m_oSendWindow.m_oSendAckPacket);
				m_oSendWindow.m_dwWaitSendSize = sizeof(m_oSendWindow.m_oSendAckPacket);

				unsigned short wLen = 0;
				int dwErrorCode = (*m_pSendOperator)(m_oSendWindow.m_btpWaitSendBuff, m_oSendWindow.m_dwWaitSendSize, wLen)
				if (dwErrorCode)
				{
					//TODO 发送出错 断开连接

					break;
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

		int ReceiveMessages(double dTime)
		{
			// packet received
			bool bPacketReceived = false;
			bool bCloseConnection = false;

			bool bRecvAble = true;
			while (bRecvAble)
			{
				// allocate buffer
				unsigned char btBufferId = m_oRecvWindow.m_btFreeBufferId;
				unsigned char* pBuffer = m_oRecvWindow.buffer[btBufferId];
				m_oRecvWindow.m_btFreeBufferId = pBuffer[0];

				// can't allocate buffer, disconnect.
				if (btBufferId >= m_oRecvWindow.window_size)
				{
					//TODO
					Disconnect();
					return 1;
				}

				unsigned short wLen = 0;
				int dwErrorCode = (*m_pRecvOperator)(pBuffer, RecvWindow::buff_size, wLen);

				if (dwErrorCode && (EAGAIN != dwErrorCode))
				{
					if (EAGAIN == dwErrorCode)
					{
						bRecvAble = false;
						pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
						m_oRecvWindow.m_btFreeBufferId = btBufferId;
						break;
					}
					
					//TODO
					Disconnect();
					return dwErrorCode;
				}

				if (0 == wLen)
				{
					//TODO
					Disconnect();
					return 0;
				}

				if (wLen < (unsigned short)sizeof(PacketHeader))
				{
					bRecvAble = false;
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}
				
				// num unsigned chars received
				m_dwNumBytesReceived += wLen + 28;
				

			}
			
			

			// receive packets
			while (readable)
			{
				// allocate buffer
				unsigned char btBufferId = m_oRecvWindow.m_btFreeBufferId;
				unsigned char* pBuffer = m_oRecvWindow.buffer[btBufferId];
				m_oRecvWindow.m_btFreeBufferId = pBuffer[0];

				// can't allocate buffer, disconnect.
				if (btBufferId >= m_oRecvWindow.window_size)
				{
					Disconnect();
					return;
				}

				// receive packet
				int n = recv(connected_socket, pBuffer, m_oRecvWindow.buffer_size, 0);

				if (n == 0)
				{
					Disconnect();
					return;
				}
				else if (n < (int)sizeof(PacketHeader))
				{
					// free buffer
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;

					if (n < 0)
						readable = false;

					continue;
				}

				// num unsigned chars received
				m_dwNumBytesReceived += n + 28;

				// packet header
				PacketHeader& packet = *(PacketHeader*)pBuffer;

				bool connect_success = false;
				if (m_dwStatus == ST_SYN_RECV)
				{
					// recv side wait for syn_send, send ack and syn_recv_wait
					if (packet.m_btStatus == ST_SYN_SEND)
					{
						if (packet.m_btAck == m_oSendWindow.begin - 1)
						{
							// initialize recv window
							for (unsigned char i = m_oRecvWindow.begin; i != m_oRecvWindow.end; i++)
							{
								unsigned char id = i % m_oRecvWindow.window_size;
								m_oRecvWindow.seq_buffer_id[id] = m_oRecvWindow.window_size;
								m_oRecvWindow.seq_size[id] = 0;
								m_oRecvWindow.seq_time[id] = 0;
								m_oRecvWindow.seq_retry[id] = 0;
								m_oRecvWindow.seq_retry_count[id] = 0;
							}

							m_dwStatus = ST_SYN_RECV_WAIT;
						}
						else
							continue;
					}
					else
						continue;
				}
				else if (m_dwStatus == ST_SYN_SEND)
				{
					// send side wait for syn_recv_wait, send ack
					if (packet.m_btStatus == ST_SYN_RECV_WAIT)
					{
						if (packet.m_btAck == m_oSendWindow.begin)
						{
							connect_success = true;
						}
						else
							continue;
					}
					else
						continue;
				}
				else if (m_dwStatus == ST_SYN_RECV_WAIT)
				{
					// recv side wait for ack
					if (packet.m_btStatus == ST_ESTABLISHED)
					{
						if (packet.m_btAck == m_oSendWindow.begin)
						{
							connect_success = true;
						}
						else
							continue;
					}
					else
						continue;
				}

				if (connect_success)
				{
					for (unsigned char i = m_oRecvWindow.begin; i != m_oRecvWindow.end; i++)
					{
						unsigned char id = i % m_oRecvWindow.window_size;
						m_oRecvWindow.seq_buffer_id[id] = m_oRecvWindow.window_size;
						m_oRecvWindow.seq_size[id] = 0;
						m_oRecvWindow.seq_time[id] = 0;
						m_oRecvWindow.seq_retry[id] = 0;
						m_oRecvWindow.seq_retry_count[id] = 0;
					}

					m_dwStatus = ST_ESTABLISHED;

					// on connected
					stream->OnConnected();
				}

				// close connection
				if (packet.m_btStatus == ST_FIN_WAIT_1)
				{
					if (m_dwStatus == ST_ESTABLISHED)
						m_dwStatus = ST_FIN_WAIT_2;

					bCloseConnection = true;
				}

				if (m_dwStatus == ST_ESTABLISHED
					|| m_dwStatus == ST_SYN_RECV_WAIT
					|| m_dwStatus == ST_FIN_WAIT_1)
				{
					// receive ack, process send buffer.
					if (m_oSendWindow.IsValidIndex(packet.m_btAck))
					{
						// got a valid packet
						m_dAckRecvTime = dTime;
						m_dwAckTimeoutRetry = 3;

						// static value for calculate delay
						static const double err_factor = 0.125;
						static const double average_factor = 0.25;
						static const double retry_factor = 2;

						double rtt = m_dDelayTime;
						double err_time = 0;

						// m_SendWindowControl not more than double m_SendWindowControl 
						double send_window_control_max = m_dSendWindowControl * 2;
						if (send_window_control_max > m_oSendWindow.window_size)
							send_window_control_max = m_oSendWindow.window_size;

						while (m_oSendWindow.begin != (unsigned char)(packet.m_btAck + 1))
						{
							unsigned char id = m_oSendWindow.begin % m_oSendWindow.window_size;
							unsigned char buffer_id = m_oSendWindow.seq_buffer_id[id];

							// calculate delay only use no retry packet
							if (m_oSendWindow.seq_retry_count[id] == 1)
							{
								// rtt(packet delay)
								rtt = dTime - m_oSendWindow.seq_time[id];
								// err_time(difference between rtt and m_dDelayTime)
								err_time = rtt - m_dDelayTime;
								// revise m_dDelayTime with err_time 
								m_dDelayTime = m_dDelayTime + err_factor * err_time;
								// revise m_dDelayAverage with err_time
								m_dDelayAverage = m_dDelayAverage + average_factor * (fabs(err_time) - m_dDelayAverage);
							}

							// free buffer
							m_oSendWindow.buffer[buffer_id][0] = m_oSendWindow.free_buffer_id;
							m_oSendWindow.free_buffer_id = buffer_id;
							m_oSendWindow.begin++;

							// get new ack
							// if m_SendWindowControl more than m_dSendWindowThreshhold in congestion avoidance,
							// else in slow start
							// in congestion avoidance m_SendWindowControl increase 1
							// in slow start m_SendWindowControl increase 1 when get m_SendWindowControl count ack
							if (m_dSendWindowControl <= m_dSendWindowThreshhold)
								m_dSendWindowControl += 1;
							else
								m_dSendWindowControl += 1 / m_dSendWindowControl;

							if (m_dSendWindowControl > send_window_control_max)
								m_dSendWindowControl = send_window_control_max;
						}

						// calculate retry with m_dDelayTime and m_dDelayAverage
						m_dRetryTime = m_dDelayTime + retry_factor * m_dDelayAverage;
						if (m_dRetryTime < m_dSendFrequency) m_dRetryTime = m_dSendFrequency;
					}

					// get same ack
					if (m_btAckLast == m_oSendWindow.begin - 1)
						m_dwAckSameCount++;
					else
						m_dwAckSameCount = 0;

					// packet is valid
					if (m_oRecvWindow.IsValidIndex(packet.m_btSyn))
					{
						unsigned char id = packet.m_btSyn % m_oRecvWindow.window_size;

						if (m_oRecvWindow.seq_buffer_id[id] >= m_oRecvWindow.window_size)
						{
							m_oRecvWindow.seq_buffer_id[id] = btBufferId;
							m_oRecvWindow.seq_size[id] = n;
							bPacketReceived = true;

							// no more buffer, try parse first.
							if (m_oRecvWindow.m_btFreeBufferId >= m_oRecvWindow.window_size)
								break;
							else
								continue;
						}
					}
				}

				// free buffer.
				pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
				m_oRecvWindow.m_btFreeBufferId = btBufferId;
			}

			if (m_oSendWindow.begin == m_oSendWindow.end)
				m_dwAckSameCount = 0;

			// record ack last
			m_btAckLast = m_oSendWindow.begin - 1;

			// update recv window
			if (bPacketReceived)
			{
				unsigned char last_ack = m_oRecvWindow.begin - 1;
				unsigned char new_ack = last_ack;
				bool parse_message = false;

				// calculate new ack
				for (unsigned char i = m_oRecvWindow.begin; i != m_oRecvWindow.end; i++)
				{
					// recv buffer is invalid
					if (m_oRecvWindow.seq_buffer_id[i % m_oRecvWindow.window_size] >= m_oRecvWindow.window_size)
						break;

					new_ack = i;
				}

				// ack changed
				if (new_ack != last_ack)
				{
					while (m_oRecvWindow.begin != (unsigned char)(new_ack + 1))
					{
						const unsigned char head_size = sizeof(PacketHeader);
						unsigned char id = m_oRecvWindow.begin % m_oRecvWindow.window_size;
						unsigned char buffer_id = m_oRecvWindow.seq_buffer_id[id];
						unsigned char* buffer = m_oRecvWindow.buffer[buffer_id] + head_size;
						ushort size = m_oRecvWindow.seq_size[id] - head_size;

						// copy buffer
						if (stream->recv_offset + size < stream->recv_buffer_size)
						{
							// add data to receive buffer
							memcpy(stream->recv_buffer + stream->recv_offset, buffer, size);
							stream->recv_offset += size;

							// free buffer
							m_oRecvWindow.buffer[buffer_id][0] = m_oRecvWindow.m_btFreeBufferId;
							m_oRecvWindow.m_btFreeBufferId = buffer_id;

							// remove sequence
							m_oRecvWindow.seq_size[id] = 0;
							m_oRecvWindow.seq_buffer_id[id] = m_oRecvWindow.window_size;
							m_oRecvWindow.begin++;
							m_oRecvWindow.end++;

							// mark for parse message
							parse_message = true;

							// send ack when get packet
							m_bSendAck = true;
						}
						else
						{
							if (stream->RecvBufferResize(2 * stream->recv_buffer_size) == false)
								break;
						}
					}
				}

				// record receive syn last
				m_btSynLast = m_oRecvWindow.begin - 1;

				// parse message
				if (parse_message)
				{
					try
					{
						stream->OnParseMessage();
					}
					catch (...)
					{
						bCloseConnection = true;
					}
				}
			}

			if (bCloseConnection)
				OnClose();
		}
	private:
		SendWindow<SEND_BUFF_SIZE, SEND_WINDOW_SIZE> m_oSendWindow;
		SlidingWindow<RECV_BUFF_SIZE, RECV_WINDOW_SIZE> m_oRecvWindow;

		OnRecvOperator* m_pOnRecvOperator;
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


	private:
		double m_dAckRecvTime;
		int m_dwAckTimeoutRetry;
		unsigned int m_dwStatus;

		int m_dwAckSameCount;
		bool m_bQuickRetry;
		bool m_bSendAck;
		unsigned char m_btAckLast;
		unsigned char m_btSynLast;

		double m_dAckOutTime;		//默认是5
	};


} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__
