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
		BUFF_CONTRAL& SetSendOperator(SendOperator* p)
		{
			m_pSendOperator = p;

			return *this;
		}

		void SendMessages()
		{
			if (stream == NULL)
				return;

			if (m_dwStatus == ST_IDLE)
				return;

			double time = Event::GetTime();

			// check ack received time
			if (time - m_dAckRecvTime > 5)
			{
				m_dAckRecvTime = time;

				if (--m_dwAckTimeoutRetry <= 0)
				{
					log_write(LOG_DEBUG1, "connection timeout.");
					OnClose();
					return;
				}
			}

			if (time < m_dSendTime)
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
						if (m_SendWindowControl > send_window.window_size)
							m_SendWindowControl = send_window.window_size;
					}
					else
					{
						// in quick retry
						// m_SendWindowControl increase 1 when get same ack 
						m_SendWindowControl += 1;
						if (m_SendWindowControl > send_window.window_size)
							m_SendWindowControl = send_window.window_size;
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
				for (byte i = send_window.begin; i != send_window.end; i++)
				{
					byte id = i % send_window.window_size;
					ushort size = send_window.seq_size[id];

					if (send_window.seq_retry_count[id] > 0
						&& time >= send_window.seq_retry[id])
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
				while ((send_window.free_buffer_id < send_window.window_size) &&	// there is a free buffer
					(size > 0))
				{
					// if send window more than m_SendWindowControl, break
					if (send_window.end - send_window.begin > m_SendWindowControl)
						break;

					byte id = send_window.end % send_window.window_size;

					// allocate buffer
					byte buffer_id = send_window.free_buffer_id;
					send_window.free_buffer_id = send_window.buffer[buffer_id][0];

					// send window buffer
					byte* buffer = send_window.buffer[buffer_id];

					// packet header
					PacketHeader& packet = *(PacketHeader*)buffer;
					packet.m_btStatus = m_dwStatus;
					packet.m_btSyn = send_window.end;
					packet.m_btAck = recv_window.begin - 1;

					// copy data
					uint copy_offset = sizeof(packet);
					uint copy_size = send_window.buffer_size - copy_offset;
					if (copy_size > size)
						copy_size = size;

					if (copy_size > 0)
					{
						memcpy(buffer + copy_offset, send_buffer + offset, copy_size);

						size -= copy_size;
						offset += copy_size;
					}

					// add to send window
					send_window.seq_buffer_id[id] = buffer_id;
					send_window.seq_size[id] = copy_size + copy_offset;
					send_window.seq_time[id] = time;
					send_window.seq_retry[id] = time;
					send_window.seq_retry_time[id] = m_dRetryTime;
					send_window.seq_retry_count[id] = 0;
					send_window.end++;
				}

				// remove data from send buffer.
				if (offset > 0)
				{
					memmove(send_buffer, send_buffer + offset, size);
					stream->send_offset = size;
				}
			}

			// if there is no data to send, make an empty one
			if (send_window.begin == send_window.end)
			{
				if (time >= m_dSendDataTime)
				{
					if (send_window.free_buffer_id < send_window.window_size)
					{
						byte id = send_window.end % send_window.window_size;

						// allocate buffer
						byte buffer_id = send_window.free_buffer_id;
						send_window.free_buffer_id = send_window.buffer[buffer_id][0];

						// send window buffer
						byte* buffer = send_window.buffer[buffer_id];

						// packet header
						PacketHeader& packet = *(PacketHeader*)buffer;
						packet.m_btStatus = m_dwStatus;
						packet.m_btSyn = send_window.end;
						packet.m_btAck = recv_window.begin - 1;

						// add to send window
						send_window.seq_buffer_id[id] = buffer_id;
						send_window.seq_size[id] = sizeof(packet);
						send_window.seq_time[id] = time;
						send_window.seq_retry[id] = time;
						send_window.seq_retry_time[id] = m_dRetryTime;
						send_window.seq_retry_count[id] = 0;
						send_window.end++;
					}
				}
			}
			else
				m_dSendDataTime = time + m_dSendDataFrequency;

			// send packets
			for (byte i = send_window.begin; i != send_window.end; i++)
			{
				// if send packets more than m_SendWindowControl, break
				if (i - send_window.begin >= m_SendWindowControl)
					break;

				byte id = i % send_window.window_size;
				ushort size = send_window.seq_size[id];

				// send packet
				if (time >= send_window.seq_retry[id] || force_retry)
				{
					force_retry = false;

					byte* buffer = send_window.buffer[send_window.seq_buffer_id[id]];

					// packet header
					PacketHeader& packet = *(PacketHeader*)buffer;
					packet.m_btStatus = m_dwStatus;
					packet.m_btSyn = i;
					packet.m_btAck = recv_window.begin - 1;

					int n = send(connected_socket, buffer, size, 0);

					if (n < 0)
					{
						log_write(LOG_DEBUG1, "send error(%m)");
						break;
					}
					else
						m_dwNumBytesSend += n + 28;

					// num send
					m_dwNumPacketsSend++;

					// num retry send
					if (time != send_window.seq_time[id])
						m_dwNumPacketsRetry++;

					m_dSendTime = time + m_dSendFrequency;
					m_dSendDataTime = time + m_dSendDataFrequency;
					m_bSendAck = false;

					send_window.seq_retry_count[id]++;
					//send_window.seq_retry_time[id] *= 2;
					send_window.seq_retry_time[id] = 1.5 * m_dRetryTime;
					if (send_window.seq_retry_time[id] > 0.2) send_window.seq_retry_time[id] = 0.2;
					send_window.seq_retry[id] = time + send_window.seq_retry_time[id];
				}
			}

			// send ack
			if (m_bSendAck)
			{
				PacketHeader packet;
				packet.m_btStatus = m_dwStatus;
				packet.m_btSyn = send_window.begin - 1;
				packet.m_btAck = recv_window.begin - 1;

				int n = send(connected_socket, &packet, sizeof(packet), 0);

				if (n < 0)
					log_write(LOG_DEBUG1, "send error(%m)");

				m_dSendTime = time + m_dSendFrequency;
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
				byte buffer_id = recv_window.free_buffer_id;
				byte* buffer = recv_window.buffer[buffer_id];
				recv_window.free_buffer_id = buffer[0];

				// can't allocate buffer, disconnect.
				if (buffer_id >= recv_window.window_size)
				{
					Disconnect();
					return;
				}

				// receive packet
				int n = recv(connected_socket, buffer, recv_window.buffer_size, 0);

				if (n == 0)
				{
					Disconnect();
					return;
				}
				else if (n < (int)sizeof(PacketHeader))
				{
					// free buffer
					buffer[0] = recv_window.free_buffer_id;
					recv_window.free_buffer_id = buffer_id;

					if (n < 0)
						readable = false;

					continue;
				}

				// num bytes received
				m_dwNumBytesReceived += n + 28;

				// packet header
				PacketHeader& packet = *(PacketHeader*)buffer;

				bool connect_success = false;
				if (m_dwStatus == ST_SYN_RECV)
				{
					// recv side wait for syn_send, send ack and syn_recv_wait
					if (packet.m_btStatus == ST_SYN_SEND)
					{
						if (packet.m_btAck == send_window.begin - 1)
						{
							// initialize recv window
							for (byte i = recv_window.begin; i != recv_window.end; i++)
							{
								byte id = i % recv_window.window_size;
								recv_window.seq_buffer_id[id] = recv_window.window_size;
								recv_window.seq_size[id] = 0;
								recv_window.seq_time[id] = 0;
								recv_window.seq_retry[id] = 0;
								recv_window.seq_retry_count[id] = 0;
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
						if (packet.m_btAck == send_window.begin)
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
						if (packet.m_btAck == send_window.begin)
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
					for (byte i = recv_window.begin; i != recv_window.end; i++)
					{
						byte id = i % recv_window.window_size;
						recv_window.seq_buffer_id[id] = recv_window.window_size;
						recv_window.seq_size[id] = 0;
						recv_window.seq_time[id] = 0;
						recv_window.seq_retry[id] = 0;
						recv_window.seq_retry_count[id] = 0;
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
					if (send_window.IsValidIndex(packet.m_btAck))
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
						if (send_window_control_max > send_window.window_size)
							send_window_control_max = send_window.window_size;

						while (send_window.begin != (byte)(packet.m_btAck + 1))
						{
							byte id = send_window.begin % send_window.window_size;
							byte buffer_id = send_window.seq_buffer_id[id];

							// calculate delay only use no retry packet
							if (send_window.seq_retry_count[id] == 1)
							{
								// rtt(packet delay)
								rtt = time - send_window.seq_time[id];
								// err_time(difference between rtt and m_dDelayTime)
								err_time = rtt - m_dDelayTime;
								// revise m_dDelayTime with err_time 
								m_dDelayTime = m_dDelayTime + err_factor * err_time;
								// revise m_dDelayAverage with err_time
								m_dDelayAverage = m_dDelayAverage + average_factor * (fabs(err_time) - m_dDelayAverage);
							}

							// free buffer
							send_window.buffer[buffer_id][0] = send_window.free_buffer_id;
							send_window.free_buffer_id = buffer_id;
							send_window.begin++;

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
					if (m_btAckLast == send_window.begin - 1)
						m_dwAckSameCount++;
					else
						m_dwAckSameCount = 0;

					// packet is valid
					if (recv_window.IsValidIndex(packet.m_btSyn))
					{
						byte id = packet.m_btSyn % recv_window.window_size;

						if (recv_window.seq_buffer_id[id] >= recv_window.window_size)
						{
							recv_window.seq_buffer_id[id] = buffer_id;
							recv_window.seq_size[id] = n;
							packet_received = true;

							// no more buffer, try parse first.
							if (recv_window.free_buffer_id >= recv_window.window_size)
								break;
							else
								continue;
						}
					}
				}

				// free buffer.
				buffer[0] = recv_window.free_buffer_id;
				recv_window.free_buffer_id = buffer_id;
			}

			if (send_window.begin == send_window.end)
				m_dwAckSameCount = 0;

			// record ack last
			m_btAckLast = send_window.begin - 1;

			// update recv window
			if (packet_received)
			{
				byte last_ack = recv_window.begin - 1;
				byte new_ack = last_ack;
				bool parse_message = false;

				// calculate new ack
				for (byte i = recv_window.begin; i != recv_window.end; i++)
				{
					// recv buffer is invalid
					if (recv_window.seq_buffer_id[i % recv_window.window_size] >= recv_window.window_size)
						break;

					new_ack = i;
				}

				// ack changed
				if (new_ack != last_ack)
				{
					while (recv_window.begin != (byte)(new_ack + 1))
					{
						const byte head_size = sizeof(PacketHeader);
						byte id = recv_window.begin % recv_window.window_size;
						byte buffer_id = recv_window.seq_buffer_id[id];
						byte* buffer = recv_window.buffer[buffer_id] + head_size;
						ushort size = recv_window.seq_size[id] - head_size;

						// copy buffer
						if (stream->recv_offset + size < stream->recv_buffer_size)
						{
							// add data to receive buffer
							memcpy(stream->recv_buffer + stream->recv_offset, buffer, size);
							stream->recv_offset += size;

							// free buffer
							recv_window.buffer[buffer_id][0] = recv_window.free_buffer_id;
							recv_window.free_buffer_id = buffer_id;

							// remove sequence
							recv_window.seq_size[id] = 0;
							recv_window.seq_buffer_id[id] = recv_window.window_size;
							recv_window.begin++;
							recv_window.end++;

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
				m_btSynLast = recv_window.begin - 1;

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
		SlidingWindow<SEND_BUFF_SIZE, SEND_WINDOW_SIZE> m_oSendWindow;
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
