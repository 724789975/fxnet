#ifndef __BUFF_CONTRAL_H__
#define __BUFF_CONTRAL_H__

#include "include/sliding_window_def.h"
#include "include/error_code.h"
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
		 * @brief ���պ���
		 *
		 * @param szBuff ���յ�����
		 * @param dwSize ���յĳ���
		 * @return int ������
		 */
		virtual int operator() (char* szBuff, unsigned short wSize, std::ostream* refOStream) = 0;
	};

	class OnConnectedOperator
	{
	public:
		virtual ~OnConnectedOperator() {};
		/**
		 * @brief ���Ӵ���
		 *
		 */
		virtual int operator() (std::ostream* refOStream) = 0;
	};

	class RecvOperator
	{
	public:
		virtual ~RecvOperator() {};
		/**
		 * @brief ���պ���
		 *
		 * @param pBuff ���յ�����
		 * @param wBuffSize ���ջ������ĳ���
		 * @param wRecvSize ���յĳ���
		 * @return int ������
		 */
		virtual int operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* refOStream) = 0;
	};

	class SendOperator
	{
	public:
		virtual ~SendOperator() {};
		/**
		 * @brief ���ʹ�����
		 *
		 * @param szBuff Ҫ���͵�����
		 * @param wBufferSize Ҫ���͵ĳ���
		 * @param wSendLen ���ͳ���
		 * @return int ������
		 */
		virtual int operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* refOStream) = 0;
	};

	class ReadStreamOperator
	{
	public:
		virtual ~ReadStreamOperator() {};
		virtual int operator() () = 0;
	protected:
	private:
	};

	enum ConnectionStatus
	{
		ST_IDLE,
		ST_SYN_SEND,		//ʹ��
		ST_SYN_RECV,		//ʹ��
		ST_SYN_RECV_WAIT,
		ST_ESTABLISHED,		//ʹ��
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

		int GetState()const;

		BufferContral& SetOnRecvOperator(OnRecvOperator* p);
		BufferContral& SetOnConnectedOperator(OnConnectedOperator* p);
		BufferContral& SetRecvOperator(RecvOperator* p);
		// TODO�Ƿ���Ҫ ����Ҫ����ɾ��
		BufferContral& SetSendOperator(SendOperator* p);
		BufferContral& SetReadStreamOperator(ReadStreamOperator* p);

		BufferContral& SetAckOutTime(double dOutTime);

		/**
		 * @brief �����ݷ��뷢�ͻ�����
		 *
		 * @param pSendBuffer ����������
		 * @param wSize Ҫ���͵ĳ���
		 * @return unsigned short ���뷢�ͻ���ĳ��� <= wSize
		 */
		unsigned short Send(const char* pSendBuffer, unsigned short wSize);

		int SendMessages(double dTime, std::ostream* pOStream);

		int ReceiveMessages(double dTime, bool& refbReadable, std::ostream* refOStream);
		
	private:
		_SendWindow m_oSendWindow;
		_RecvWindow m_oRecvWindow;

		OnRecvOperator* m_pOnRecvOperator;
		OnConnectedOperator* m_pOnConnectedOperator;
		RecvOperator* m_pRecvOperator;
		SendOperator* m_pSendOperator;
		ReadStreamOperator* m_pReadStreamOperator;

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

		double m_dAckRecvTime;
		int m_dwAckTimeoutRetry;
		unsigned int m_dwStatus;

		int m_dwAckSameCount;
		bool m_bQuickRetry;
		bool m_bSendAck;
		unsigned char m_btAckLast;
		unsigned char m_btSynLast;

		double m_dAckOutTime; //Ĭ����5
	};

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::BufferContral()
		: m_dwNumBytesSend(0)
		, m_dwNumBytesReceived(0)
		, m_dSendTime(0.)
		, m_dSendFrequency(0.02)
		, m_dwNumPacketsSend(0)
		, m_dwNumPacketsRetry(0)
		, m_bConnected(false)
		, m_btAckLast(0)
		, m_btSynLast(0)
	{
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::~BufferContral()
	{ }

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::Init(int dwState)
	{
		m_dwStatus = dwState;
		m_dDelayTime = 0;
		m_dSendFrequency = 0.05;
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

		// ��ջ�������
		m_oRecvWindow.ClearBuffer();
		m_oSendWindow.ClearBuffer();

		// ��ʼ�����ʹ���
		m_oSendWindow.m_btBegin = 1;
		m_oSendWindow.m_btEnd = m_oSendWindow.m_btBegin;

		// ��ʼ�����մ���
		m_oRecvWindow.m_btBegin = 1;
		m_oRecvWindow.m_btEnd = m_oRecvWindow.m_btBegin + _RecvWindow::window_size;

		m_bConnected = false;
		return 0;
	}

	template <unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	int BufferContral<BUFF_SIZE, WINDOW_SIZE>::GetState() const
	{
		return m_dwStatus;
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
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetReadStreamOperator(ReadStreamOperator* p)
	{
		m_pReadStreamOperator = p;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>&
		BufferContral<BUFF_SIZE, WINDOW_SIZE>::SetAckOutTime(double dOutTime)
	{
		m_dAckOutTime = dOutTime;
		return *this;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline unsigned short BufferContral<BUFF_SIZE, WINDOW_SIZE>
		::Send(const char* pSendBuffer, unsigned short wSize)
	{
		unsigned short wSendSize = 0;
		while ((m_oSendWindow.m_btFreeBufferId < _SendWindow::window_size) && // there is a free buffer
			(wSize > 0))
		{
			//���Ȳ������ӵ������
			if (m_oSendWindow.m_btEnd - m_oSendWindow.m_btBegin > m_dSendWindowControl) break;

			unsigned char btId = m_oSendWindow.m_btEnd % _SendWindow::window_size;

			// ��ȡһ��֡
			unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
			m_oSendWindow.m_btFreeBufferId = m_oSendWindow.m_btarrBuffer[btBufferId][0];

			// ���͵Ļ���
			unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

			// ��������ͷ
			UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
			oPacket.m_btStatus = m_dwStatus;
			oPacket.m_btSyn = m_oSendWindow.m_btEnd;
			oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

			// ��������
			unsigned int dwCopyOffset = sizeof(oPacket);
			unsigned int dwCopySize = _SendWindow::buff_size - dwCopyOffset;
			if (dwCopySize > wSize)
				dwCopySize = wSize;

			if (dwCopySize > 0)
			{
				memcpy((void*)(pBuffer + dwCopyOffset), pSendBuffer + wSendSize, dwCopySize);

				wSize -= dwCopySize;
				wSendSize += dwCopySize;
			}

			// ��ӵ����ʹ���
			m_oSendWindow.Add2SendWindow(btId, btBufferId, dwCopySize + dwCopyOffset
				, FxIoModule::Instance()->FxGetCurrentTime(), m_dRetryTime);
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
				return CODE_ERROR_NET_UDP_ACK_TIME_OUT_RETRY;
			}
		}

		if (dTime < m_dSendTime) { return 0; }

		//if (m_dwStatus == ST_SYN_RECV) {return 0;}

		bool bForceRetry = false;

		if (m_dwStatus == ST_ESTABLISHED)
		{
			//3����ͬack ��ʼ�����ش�
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
					//��ͬackʱ ӵ�����ƴ���+1
					m_dSendWindowControl += 1;
					if (m_dSendWindowControl > _SendWindow::window_size)
					{
						m_dSendWindowControl = _SendWindow::window_size;
					}
				}
			}
			else
			{
				//���µ�ack ��ô�����ش�����
				if (m_bQuickRetry == true)
				{
					m_dSendWindowControl = m_dSendWindowThreshhold;
					m_bQuickRetry = false;
				}
			}

			//�����ʱ(����rto) ��ô���½���������
			for (unsigned char i = m_oSendWindow.m_btBegin; i != m_oSendWindow.m_btEnd; i++)
			{
				unsigned char btId = i % _SendWindow::window_size;
				//unsigned short size = m_oSendWindow.m_warrSeqSize[btId];

				if (m_oSendWindow.m_dwarrSeqRetryCount[btId] > 0
					&& dTime >= m_oSendWindow.m_darrSeqRetry[btId])
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

			(* m_pReadStreamOperator)();
		}

		//û�����ݷ��� ��ô����һ���յ� ����ͬ����������
		if (m_oSendWindow.m_btBegin == m_oSendWindow.m_btEnd)
		{
			if (dTime >= m_dSendDataTime)
			{
				if (m_oSendWindow.m_btFreeBufferId < m_oSendWindow.window_size)
				{
					unsigned char btId = m_oSendWindow.m_btEnd % m_oSendWindow.window_size;

					// ��ȡ����
					unsigned char btBufferId = m_oSendWindow.m_btFreeBufferId;
					m_oSendWindow.m_btFreeBufferId = m_oSendWindow.m_btarrBuffer[btBufferId][0];
					unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[btBufferId];

					// packet header
					UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
					oPacket.m_btStatus = m_dwStatus;
					oPacket.m_btSyn = m_oSendWindow.m_btEnd;
					oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

					if (pOStream)
					{
						*pOStream << "status:" << (int)oPacket.m_btStatus << ", syn:" << (int)oPacket.m_btSyn << ", ack:" << (int)oPacket.m_btAck
							<< ", m_oSendWindow.m_btEnd:" << (int)m_oSendWindow.m_btEnd << ", m_oRecvWindow.m_btBegin::" << (int)m_oRecvWindow.m_btBegin
							<< "\n";
					}

					// ��ӵ����ʹ���
					m_oSendWindow.Add2SendWindow(btId, btBufferId, sizeof(oPacket), dTime, m_dRetryTime);
				}
			}
		}
		else { m_dSendDataTime = dTime + m_dSendDataFrequency; }

		//��ʼ����
		for (unsigned char i = m_oSendWindow.m_btBegin; i != m_oSendWindow.m_btEnd; i++)
		{
			//������ͳ��ȳ���ӵ������ ��ֹͣ
			if (i - m_oSendWindow.m_btBegin >= m_dSendWindowControl) { break; }

			unsigned char btId = i % _SendWindow::window_size;
			unsigned short wSize = m_oSendWindow.m_warrSeqSize[btId];

			//��ʼ���� ���� �ش�
			if (dTime >= m_oSendWindow.m_darrSeqRetry[btId] || bForceRetry)
			{
				bForceRetry = false;

				unsigned char* pBuffer = m_oSendWindow.m_btarrBuffer[m_oSendWindow.m_btarrSeqBufferId[btId]];

				// packet header
				UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
				oPacket.m_btStatus = m_dwStatus;
				oPacket.m_btSyn = i;
				oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

				int dwLen = 0;
				int dwErrorCode = (*m_pSendOperator)((char*)pBuffer, wSize, dwLen, pOStream);
				if (dwErrorCode)
				{
					if (EAGAIN == dwErrorCode || EINTR == dwErrorCode)
					{
						break;
					}
					//���ͳ��� �Ͽ�����
					return dwErrorCode;
				}
				else
				{
					m_dwNumBytesSend += dwLen;
				}

				// ���Ͱ�����
				m_dwNumPacketsSend++;

				// ���Դ���
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

		// send ack
		if (m_bSendAck)
		{
			UDPPacketHeader oPacket;
			oPacket.m_btStatus = m_dwStatus;
			oPacket.m_btSyn = m_oSendWindow.m_btBegin - 1;
			oPacket.m_btAck = m_oRecvWindow.m_btBegin - 1;

			int dwLen = 0;
			if (int dwErrorCode = (*m_pSendOperator)((char*)(&oPacket), sizeof(oPacket), dwLen, pOStream))
			{
				//���ͳ��� �Ͽ�����
				return dwErrorCode;
			}

			m_dSendTime = dTime + m_dSendFrequency;
			m_bSendAck = false;
		}

		return 0;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::ReceiveMessages(double dTime, bool& refbReadable, std::ostream* pOStream)
	{
		// packet received
		bool bPacketReceived = false;
		//bool bCloseConnection = false;

		while (refbReadable)
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

			if (dwErrorCode)
			{
				if (EINTR == dwErrorCode)
				{
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					continue;
				}

				if (EAGAIN == dwErrorCode)
				{
					refbReadable = false;
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}

#ifdef _WIN32
				if (WSA_IO_PENDING == dwErrorCode || CODE_SUCCESS_NO_BUFF_READ == dwErrorCode)
#else
				if (EAGAIN == dwErrorCode || EWOULDBLOCK == dwErrorCode)
#endif	//_WIN32
				{
					refbReadable = false;
					pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}

				return dwErrorCode;
			}

			if (dwLen < (unsigned short)sizeof(UDPPacketHeader))
			{
				pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
				m_oRecvWindow.m_btFreeBufferId = btBufferId;
				continue;
			}
			if (!m_bConnected)
			{
				//if (dwLen > (unsigned short)sizeof(UDPPacketHeader))
				//{
				//	pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
				//	m_oRecvWindow.m_btFreeBufferId = btBufferId;
				//	break;
				//}
				UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;
				m_dwStatus = ST_ESTABLISHED;
				if (ST_ESTABLISHED == packet.m_btStatus)
				{
					(*m_pOnConnectedOperator)(pOStream);
					m_bConnected = true;
					
					for (unsigned char i = m_oRecvWindow.m_btBegin; i != m_oRecvWindow.m_btEnd; ++i)
					{
						unsigned char btId = i % m_oRecvWindow.window_size;
						m_oRecvWindow.m_btarrSeqBufferId[btId] = m_oRecvWindow.window_size;
						m_oRecvWindow.m_warrSeqSize[btId] = 0;
						m_oRecvWindow.m_darrSeqTime[btId] = 0;
						m_oRecvWindow.m_darrSeqRetry[btId] = 0;
						m_oRecvWindow.m_dwarrSeqRetryCount[btId] = 0;
					}
				}
			}

			// num unsigned chars received
			m_dwNumBytesReceived += dwLen + 28;

			// packet header
			UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;

			if (pOStream)
			{
				*pOStream << "status:" << (int)packet.m_btStatus << ", syn:" << (int)packet.m_btSyn << ", ack:" << (int)packet.m_btAck << "\n";
			}

			//�յ�һ����Ч��ack ��ôҪ���·��ʹ��ڵ�״̬
			if (m_oSendWindow.IsValidIndex(packet.m_btAck))
			{
				// ��õ�����һ����Ч�İ�
				m_dAckRecvTime = dTime;
				m_dwAckTimeoutRetry = 3;

				// ���ڼ���delay������
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

					//ֻʹ��û���ط��İ������ӳ�
					if (m_oSendWindow.m_btarrSeqBufferId[id] == 1)
					{
						// rtt(�հ��ӳ�ʱ��)
						rtt = dTime - m_oSendWindow.m_darrSeqTime[id];
						//ʹ�� rtt �� m_dDelayTime ��ȡ dErrTime
						dErrTime = rtt - m_dDelayTime;
						//ʹ�� dErrTime ���¼��� m_dDelayTime
						m_dDelayTime = m_dDelayTime + err_factor * dErrTime;
						//ʹ�� dErrTime ���¼��� m_dDelayAverage
						m_dDelayAverage = m_dDelayAverage + average_factor * (fabs(dErrTime) - m_dDelayAverage);
					}

					// �ͷŻ���
					m_oSendWindow.m_btarrBuffer[btBufferId][0] = m_oSendWindow.m_btFreeBufferId;
					m_oSendWindow.m_btFreeBufferId = btBufferId;
					m_oSendWindow.m_btBegin++;

					//�յ��µ�ack
					//��� m_SendWindowControl �� m_dSendWindowThreshhold �� Ϊӵ������
					//����Ϊ������
					//ӵ�������� m_SendWindowControl ÿ������ 1 / m_dSendWindowControl
					//�������� m_SendWindowControl ÿ������ 1
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

				//ʹ�� m_dDelayTime �� m_dDelayAverage �������Ե�ʱ��
				m_dRetryTime = m_dDelayTime + retry_factor * m_dDelayAverage;
				if (m_dRetryTime < m_dSendFrequency) m_dRetryTime = m_dSendFrequency;
			}

			//�յ���ͬack(����3��Ҫ��ʼѡ���ش�)
			if (m_btAckLast == m_oSendWindow.m_btBegin - 1)
			{
				m_dwAckSameCount++;
			}
			else
			{
				m_dwAckSameCount = 0;
			}

			//���յ�����һ����Ч�İ�
			if (m_oRecvWindow.IsValidIndex(packet.m_btSyn))
			{
				unsigned char btId = packet.m_btSyn % _RecvWindow::window_size;

				if (pOStream)
				{
					*pOStream << "recv new:" << (int)packet.m_btSyn << ", m_oRecvWindow.m_btarrSeqBufferId[btId]" << (int)m_oRecvWindow.m_btarrSeqBufferId[btId] << "\n";
				}
				if (m_oRecvWindow.m_btarrSeqBufferId[btId] >= _RecvWindow::window_size)
				{
					m_oRecvWindow.m_btarrSeqBufferId[btId] = btBufferId;
					m_oRecvWindow.m_warrSeqSize[btId] = dwLen;
					bPacketReceived = true;

					// û�и���Ľ��ջ��� ��ô�ȴ������
					if (m_oRecvWindow.m_btFreeBufferId >= _RecvWindow::window_size) { break; }
					else { continue; }
				}
			}

			// �ͷ�buffer
			pBuffer[0] = m_oRecvWindow.m_btFreeBufferId;
			m_oRecvWindow.m_btFreeBufferId = btBufferId;
		}

		if (m_oSendWindow.m_btBegin == m_oSendWindow.m_btEnd) { m_dwAckSameCount = 0; }

		// ��¼���һ��ack
		m_btAckLast = m_oSendWindow.m_btBegin - 1;

		// ���½��մ���
		if (bPacketReceived)
		{
			unsigned char btLastAck = m_oRecvWindow.m_btBegin - 1;
			unsigned char btNewAck = btLastAck;
			//bool bParseMessage = false;

			//�����µ�ack
			for (unsigned char i = m_oRecvWindow.m_btBegin; i != m_oRecvWindow.m_btEnd; i++)
			{
				// ���յ���buff�Ƿ���Ч
				if (m_oRecvWindow.m_btarrSeqBufferId[i % _RecvWindow::window_size] >= _RecvWindow::window_size)
					break;

				btNewAck = i;

				if (pOStream)
				{
					*pOStream << "recv new_ack:" << (int)btNewAck << "\n";
				}
			}

			// ���µ�ack
			if (btNewAck != btLastAck)
			{
				while (m_oRecvWindow.m_btBegin != (unsigned char)(btNewAck + 1))
				{
					const unsigned char cbtHeadSize = sizeof(UDPPacketHeader);
					unsigned char btId = m_oRecvWindow.m_btBegin % _RecvWindow::window_size;
					unsigned char btBufferId = m_oRecvWindow.m_btarrSeqBufferId[btId];
					unsigned char* pBuffer = m_oRecvWindow.m_btarrBuffer[btBufferId] + cbtHeadSize;
					unsigned short wSize = m_oRecvWindow.m_warrSeqSize[btId] - cbtHeadSize;

					if (int dwError = (*m_pOnRecvOperator)((char*)pBuffer, wSize, pOStream))
					{
						return dwError;
					}

					// ���ճɹ� �ͷŻ���
					m_oRecvWindow.m_btarrBuffer[btBufferId][0] = m_oRecvWindow.m_btFreeBufferId;
					m_oRecvWindow.m_btFreeBufferId = btBufferId;

					// �Ӷ����Ƴ�
					m_oRecvWindow.m_warrSeqSize[btId] = 0;
					m_oRecvWindow.m_btarrSeqBufferId[btId] = _RecvWindow::window_size;
					m_oRecvWindow.m_btBegin++;
					m_oRecvWindow.m_btEnd++;

					// �Ƿ���Խ���
					//bParseMessage = true;

					// ��Ҫ����ack
					m_bSendAck = true;
				}
			}

			// ������һ��syn
			m_btSynLast = m_oRecvWindow.m_btBegin - 1;
		}

		return 0;
	}

} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__

