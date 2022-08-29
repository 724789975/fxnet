#ifndef __BUFF_CONTRAL_H__
#define __BUFF_CONTRAL_H__

#include "include/sliding_window_def.h"
#include "sliding_window.h"

namespace FXNET
{
	struct PacketHeader
	{
		unsigned char m_btStatus;
		unsigned char m_btSyn;
		unsigned char m_btAck;
	};

	template <unsigned int SEND_BUFF_SIZE = 512, unsigned int SEND_WINDOW_SIZE = 32
		, unsigned int RECV_BUFF_SIZE = 512, unsigned int RECV_WINDOW_SIZE = 32>
	class BuffContral
	{
	public:
		enum { send_buff_size = SEND_BUFF_SIZE };
		enum { send_window_size = SEND_WINDOW_SIZE };
		enum { recv_buff_size = RECV_BUFF_SIZE };
		enum { recv_window_size = RECV_WINDOW_SIZE };

		typede BuffContral<SEND_BUFF_SIZE, SEND_WINDOW_SIZE, RECV_BUFF_SIZE, RECV_WINDOW_SIZE> BUFF_CONTRAL;

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

		// TUDO是否需要 不需要后面删掉
		BUFF_CONTRAL& SetSendOperator(SendOperator* p)
		{
			m_pSendOperator = p;

			return *this;
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
				if (m_oSendWindow.m_btEnd - m_oSendWindow.m_btBegin > m_SendWindowControl)
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
				m_oSendWindow.Add2SendWindow(btBufferId, btBufferId, dwCopySize + dwCopyOffset, dTime, m_dRetryTime);
			}

			return wSendSize;
		}

		//TCP不需要?
		void SendMessages(double dTime)
		{
			// check ack received time
			if (dTime - m_dAckRecvTime > 5)
			{
				m_dAckRecvTime = dTime;

				if (--m_dwAckTimeoutRetry <= 0)
				{
					//TODO
					OnClose();
					return;
				}
			}

			if (dTime < m_dSendTime)
				return;

			if (m_dwStatus == ST_SYN_RECV)
				return;

			bool force_retry = false;

			if (m_dwStatus == ST_ESTABLISHED
				|| m_dwStatus == ST_FIN_WAIT_1)
			{
				// enter quick retry when get 3 same ack
				if (m_dwAckSameCount > 3)
				{
					if (m_bQuickRetry == false)
					{
						m_bQuickRetry = true;
						force_retry = true;

						m_dSendWindowThreshhold = m_SendWindowControl / 2;
						if (m_dSendWindowThreshhold < 2) m_dSendWindowThreshhold = 2;
						m_SendWindowControl = m_dSendWindowThreshhold + m_dwAckSameCount - 1;
						if (m_SendWindowControl > m_oSendWindow.window_size)
							m_SendWindowControl = m_oSendWindow.window_size;
					}
					else
					{
						// in quick retry
						// m_SendWindowControl increase 1 when get same ack 
						m_SendWindowControl += 1;
						if (m_SendWindowControl > m_oSendWindow.window_size)
							m_SendWindowControl = m_oSendWindow.window_size;
					}
				}
				else
				{
					// quick retry finished when get new ack
					if (m_bQuickRetry == true)
					{
						m_SendWindowControl = m_dSendWindowThreshhold;
						m_bQuickRetry = false;
					}
				}

				// enter slow start when send data timeout 
				for (unsigned char i = m_oSendWindow.begin; i != m_oSendWindow.end; i++)
				{
					unsigned char id = i % m_oSendWindow.window_size;
					ushort size = m_oSendWindow.seq_size[id];

					if (m_oSendWindow.seq_retry_count[id] > 0
						&& dTime >= m_oSendWindow.seq_retry[id])
					{
						m_dSendWindowThreshhold = m_SendWindowControl / 2;
						if (m_dSendWindowThreshhold < 2) m_dSendWindowThreshhold = 2;
						//m_SendWindowControl = 1;
						m_SendWindowControl = m_dSendWindowThreshhold;
						//break;

						m_bQuickRetry = false;
						m_dwAckSameCount = 0;
						break;
					}
				}

				uint offset = 0;
				uint size = stream->send_offset;
				char* send_buffer = stream->send_buffer;

				// put buffer to send window
				while ((m_oSendWindow.free_buffer_id < m_oSendWindow.window_size) &&	// there is a free buffer
					(size > 0))
				{
					// if send window more than m_SendWindowControl, break
					if (m_oSendWindow.end - m_oSendWindow.begin > m_SendWindowControl)
						break;

					unsigned char id = m_oSendWindow.end % m_oSendWindow.window_size;

					// allocate buffer
					unsigned char buffer_id = m_oSendWindow.free_buffer_id;
					m_oSendWindow.free_buffer_id = m_oSendWindow.buffer[buffer_id][0];

					// send window buffer
					unsigned char* buffer = m_oSendWindow.buffer[buffer_id];

					// packet header
					PacketHeader& packet = *(PacketHeader*)buffer;
					packet.m_btStatus = m_dwStatus;
					packet.m_btSyn = m_oSendWindow.end;
					packet.m_btAck = m_oRecvWindow.begin - 1;

					// copy data
					uint copy_offset = sizeof(packet);
					uint copy_size = m_oSendWindow.buffer_size - copy_offset;
					if (copy_size > size)
						copy_size = size;

					if (copy_size > 0)
					{
						memcpy(buffer + copy_offset, send_buffer + offset, copy_size);

						size -= copy_size;
						offset += copy_size;
					}

					// add to send window
					m_oSendWindow.seq_buffer_id[id] = buffer_id;
					m_oSendWindow.seq_size[id] = copy_size + copy_offset;
					m_oSendWindow.seq_time[id] = dTime;
					m_oSendWindow.seq_retry[id] = dTime;
					m_oSendWindow.seq_retry_time[id] = m_dRetryTime;
					m_oSendWindow.seq_retry_count[id] = 0;
					m_oSendWindow.end++;
				}

				// remove data from send buffer.
				if (offset > 0)
				{
					memmove(send_buffer, send_buffer + offset, size);
					stream->send_offset = size;
				}
			}

			// if there is no data to send, make an empty one
			if (m_oSendWindow.begin == m_oSendWindow.end)
			{
				if (dTime >= m_dSendDataTime)
				{
					if (m_oSendWindow.free_buffer_id < m_oSendWindow.window_size)
					{
						unsigned char id = m_oSendWindow.end % m_oSendWindow.window_size;

						// allocate buffer
						unsigned char buffer_id = m_oSendWindow.free_buffer_id;
						m_oSendWindow.free_buffer_id = m_oSendWindow.buffer[buffer_id][0];

						// send window buffer
						unsigned char* buffer = m_oSendWindow.buffer[buffer_id];

						// packet header
						PacketHeader& packet = *(PacketHeader*)buffer;
						packet.m_btStatus = m_dwStatus;
						packet.m_btSyn = m_oSendWindow.end;
						packet.m_btAck = m_oRecvWindow.begin - 1;

						// add to send window
						m_oSendWindow.seq_buffer_id[id] = buffer_id;
						m_oSendWindow.seq_size[id] = sizeof(packet);
						m_oSendWindow.seq_time[id] = dTime;
						m_oSendWindow.seq_retry[id] = dTime;
						m_oSendWindow.seq_retry_time[id] = m_dRetryTime;
						m_oSendWindow.seq_retry_count[id] = 0;
						m_oSendWindow.end++;
					}
				}
			}
			else
				m_dSendDataTime = dTime + m_dSendDataFrequency;

			// send packets
			for (unsigned char i = m_oSendWindow.begin; i != m_oSendWindow.end; i++)
			{
				// if send packets more than m_SendWindowControl, break
				if (i - m_oSendWindow.begin >= m_SendWindowControl)
					break;

				unsigned char id = i % m_oSendWindow.window_size;
				ushort size = m_oSendWindow.seq_size[id];

				// send packet
				if (dTime >= m_oSendWindow.seq_retry[id] || force_retry)
				{
					force_retry = false;

					unsigned char* buffer = m_oSendWindow.buffer[m_oSendWindow.seq_buffer_id[id]];

					// packet header
					PacketHeader& packet = *(PacketHeader*)buffer;
					packet.m_btStatus = m_dwStatus;
					packet.m_btSyn = i;
					packet.m_btAck = m_oRecvWindow.begin - 1;

					int n = send(connected_socket, buffer, size, 0);

					if (n < 0)
					{
						log_write(LOG_DEBUG1, "send error(%m)");
						break;
					}
					else
						m_dwNumunsigned charsSend += n + 28;

					// num send
					m_dwNumPacketsSend++;

					// num retry send
					if (dTime != m_oSendWindow.seq_time[id])
						m_dwNumPacketsRetry++;

					m_dSendTime = dTime + m_dSendFrequency;
					m_dSendDataTime = dTime + m_dSendDataFrequency;
					m_bSendAck = false;

					m_oSendWindow.seq_retry_count[id]++;
					//m_oSendWindow.seq_retry_time[id] *= 2;
					m_oSendWindow.seq_retry_time[id] = 1.5 * m_dRetryTime;
					if (m_oSendWindow.seq_retry_time[id] > 0.2) m_oSendWindow.seq_retry_time[id] = 0.2;
					m_oSendWindow.seq_retry[id] = dTime + m_oSendWindow.seq_retry_time[id];
				}
			}

			// send ack
			if (m_bSendAck)
			{
				PacketHeader packet;
				packet.m_btStatus = m_dwStatus;
				packet.m_btSyn = m_oSendWindow.begin - 1;
				packet.m_btAck = m_oRecvWindow.begin - 1;

				int n = send(connected_socket, &packet, sizeof(packet), 0);

				if (n < 0)
					log_write(LOG_DEBUG1, "send error(%m)");

				m_dSendTime = dTime + m_dSendFrequency;
				m_bSendAck = false;
			}
		}

		void ReceiveMessages()
		{
			if (stream == NULL)
				return;

			if (m_dwStatus == ST_IDLE)
				return;

			// current time
			double time = Event::GetTime();

			// packet received
			bool packet_received = false;
			bool close_connection = false;

			// receive packets
			while (readable)
			{
				// allocate buffer
				unsigned char buffer_id = m_oRecvWindow.free_buffer_id;
				unsigned char* buffer = m_oRecvWindow.buffer[buffer_id];
				m_oRecvWindow.free_buffer_id = buffer[0];

				// can't allocate buffer, disconnect.
				if (buffer_id >= m_oRecvWindow.window_size)
				{
					Disconnect();
					return;
				}

				// receive packet
				int n = recv(connected_socket, buffer, m_oRecvWindow.buffer_size, 0);

				if (n == 0)
				{
					Disconnect();
					return;
				}
				else if (n < (int)sizeof(PacketHeader))
				{
					// free buffer
					buffer[0] = m_oRecvWindow.free_buffer_id;
					m_oRecvWindow.free_buffer_id = buffer_id;

					if (n < 0)
						readable = false;

					continue;
				}

				// num unsigned chars received
				m_dwNumBytesReceived += n + 28;

				// packet header
				PacketHeader& packet = *(PacketHeader*)buffer;

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

					close_connection = true;
				}

				if (m_dwStatus == ST_ESTABLISHED
					|| m_dwStatus == ST_SYN_RECV_WAIT
					|| m_dwStatus == ST_FIN_WAIT_1)
				{
					// receive ack, process send buffer.
					if (m_oSendWindow.IsValidIndex(packet.m_btAck))
					{
						// got a valid packet
						m_dAckRecvTime = time;
						m_dwAckTimeoutRetry = 3;

						// static value for calculate delay
						static const double err_factor = 0.125;
						static const double average_factor = 0.25;
						static const double retry_factor = 2;

						double rtt = m_dDelayTime;
						double err_time = 0;

						// m_SendWindowControl not more than double m_SendWindowControl 
						double send_window_control_max = m_SendWindowControl * 2;
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
								rtt = time - m_oSendWindow.seq_time[id];
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
							if (m_SendWindowControl <= m_dSendWindowThreshhold)
								m_SendWindowControl += 1;
							else
								m_SendWindowControl += 1 / m_SendWindowControl;

							if (m_SendWindowControl > send_window_control_max)
								m_SendWindowControl = send_window_control_max;
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
							m_oRecvWindow.seq_buffer_id[id] = buffer_id;
							m_oRecvWindow.seq_size[id] = n;
							packet_received = true;

							// no more buffer, try parse first.
							if (m_oRecvWindow.free_buffer_id >= m_oRecvWindow.window_size)
								break;
							else
								continue;
						}
					}
				}

				// free buffer.
				buffer[0] = m_oRecvWindow.free_buffer_id;
				m_oRecvWindow.free_buffer_id = buffer_id;
			}

			if (m_oSendWindow.begin == m_oSendWindow.end)
				m_dwAckSameCount = 0;

			// record ack last
			m_btAckLast = m_oSendWindow.begin - 1;

			// update recv window
			if (packet_received)
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
							m_oRecvWindow.buffer[buffer_id][0] = m_oRecvWindow.free_buffer_id;
							m_oRecvWindow.free_buffer_id = buffer_id;

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
						close_connection = true;
					}
				}
			}

			if (close_connection)
				OnClose();
		}
	private:
		SendWindow<SEND_BUFF_SIZE, SEND_WINDOW_SIZE> m_oSendWindow;
		SlidingWindow<RECV_BUFF_SIZE, RECV_WINDOW_SIZE> m_oRecvWindow;

		OnRecvOperator* m_pOnRecvOperator;
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
		double m_SendWindowControl;
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
	};


} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__
