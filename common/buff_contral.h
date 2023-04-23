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
	class OnRecvOperator
	{
	public:
		virtual ~OnRecvOperator() {};

		/**
		 * @brief ���պ���
		 *
		 * @param szBuff ���յ�����
		 * @param dwSize ���յĳ���
		 * @return ErrorCode ������
		 */
		virtual ErrorCode operator() (char* szBuff, unsigned short wSize, std::ostream* pOStream) = 0;
	};

	class OnConnectedOperator
	{
	public:
		virtual ~OnConnectedOperator() {};
		/**
		 * @brief ���Ӵ���
		 *
		 */
		virtual ErrorCode operator() (std::ostream* pOStream) = 0;
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
		 * @return ErrorCode ������
		 */
		virtual ErrorCode operator() (char* pBuff, unsigned short wBuffSize, int& wRecvSize, std::ostream* pOStream) = 0;
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
		 * @return ErrorCode ������
		 */
		virtual ErrorCode operator() (char* szBuff, unsigned short wBufferSize, int& dwSendLen, std::ostream* pOStream) = 0;
	};

	class ReadStreamOperator
	{
	public:
		virtual ~ReadStreamOperator() {};
		virtual unsigned int operator() (std::ostream* pOStream) = 0;
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

		/**
		 * @brief Set the On Recv Operator object
		 * 
		 * @param p 
		 * @return BufferContral& 
		 */
		BufferContral& SetOnRecvOperator(OnRecvOperator* p);
		/**
		 * @brief Set the On Connected Operator object
		 * 
		 * @param p 
		 * @return BufferContral& 
		 */
		BufferContral& SetOnConnectedOperator(OnConnectedOperator* p);
		/**
		 * @brief Set the Recv Operator object
		 * 
		 * @param p 
		 * @return BufferContral& 
		 */
		BufferContral& SetRecvOperator(RecvOperator* p);
		/**
		 * @brief Set the Send Operator object
		 * 
		 * @param p 
		 * @return BufferContral& 
		 */
		BufferContral& SetSendOperator(SendOperator* p);
		/**
		 * @brief Set the Read Stream Operator object
		 * 
		 * @param p 
		 * @return BufferContral& 
		 */
		BufferContral& SetReadStreamOperator(ReadStreamOperator* p);

		BufferContral& SetAckOutTime(double dOutTime);

		/**
		 * @brief �����ݷ��뷢�ͻ�����
		 *
		 * @param pSendBuffer ����������
		 * @param wSize Ҫ���͵ĳ���
		 * @return unsigned short ���뷢�ͻ���ĳ��� <= wSize
		 */
		unsigned int Send(const char* pSendBuffer, unsigned int dwSize);

		/**
		 * @brief 
		 * 
		 * ������Ϣ �����������ݷ��ͳ�ȥ
		 * @param dTime 
		 * @param pOStream 
		 * @return int 
		 */
		ErrorCode SendMessages(double dTime, std::ostream* pOStream);
		/**
		 * @brief 
		 * 
		 * ��������
		 * @param dTime 
		 * @param refbReadable 
		 * @param pOStream 
		 * @return int 
		 */
		ErrorCode ReceiveMessages(double dTime, bool& refbReadable, std::ostream* pOStream);
		
	private:
		_SendWindow m_oSendWindow;
		_RecvWindow m_oRecvWindow;

		OnRecvOperator* m_pOnRecvOperator;
		OnConnectedOperator* m_pOnConnectedOperator;
		RecvOperator* m_pRecvOperator;
		SendOperator* m_pSendOperator;
		ReadStreamOperator* m_pReadStreamOperator;

		/**
		 * @brief 
		 * 
		 * �ѷ����ֽ���
		 */
		unsigned int m_dwNumBytesSend;
		/**
		 * @brief 
		 * 
		 * �ѽ����ֽ���
		 */
		unsigned int m_dwNumBytesReceived;

		/**
		 * @brief 
		 * 
		 * rtt(�հ��ӳ�ʱ��)
		 */
		double m_dDelayTime;
		/**
		 * @brief 
		 * 
		 * ƽ��rtt
		 */
		double m_dDelayAverage;
		/**
		 * @brief 
		 * 
		 * �ش�ʱ��
		 * һ��ʱ���û�з��ͳ�ȥ ��ô�Ϳ�ʼ�ش�
		 */
		double m_dRetryTime;
		/**
		 * @brief 
		 * 
		 * ��һ�η�����ʱ��
		 */
		double m_dSendTime;
		/**
		 * @brief 
		 * 
		 * ����Ƶ��
		 */
		double m_dSendFrequency;
		/**
		 * @brief 
		 * 
		 * ӵ������
		 */
		double m_dSendWindowControl;
		/**
		 * @brief 
		 * 
		 * ���ʹ�����ֵ
		 */
		double m_dSendWindowThreshhold;
		/**
		 * @brief 
		 * 
		 * �´η��Ϳհ�ʱ��
		 */
		double m_dSendEmptyDataTime;
		/**
		 * @brief 
		 * 
		 * ���Ϳհ�Ƶ��
		 */
		double m_dSendEmptyDataFrequency;
		/**
		 * @brief 
		 * 
		 * ���Ϳհ����� ����һ�ηǿհ�ʱ ����
		 */
		int m_dwSendEmptyDataFactor;

		/**
		 * @brief 
		 * 
		 * ��������(ÿ512�ֽڻ�����512 ��һ����)
		 */
		unsigned int m_dwNumPacketsSend;
		/**
		 * @brief 
		 * 
		 * �հ�����
		 */
		unsigned int m_dwNumPacketsRetry;

		bool m_bConnected;

		/**
		 * @brief 
		 * 
		 * ���һ����Ч��ʱ��
		 */
		double m_dAckRecvTime;
		/**
		 * @brief 
		 * 
		 * ��ʱ���Դ���
		 */
		int m_dwAckTimeoutRetry;
		/**
		 * @brief 
		 * 
		 * ����״̬
		 */
		unsigned int m_dwStatus;

		/**
		 * @brief 
		 * 
		 * ��ͬack����
		 */
		int m_dwAckSameCount;
		/**
		 * @brief 
		 * 
		 * �Ƿ����ڿ����ش�
		 */
		bool m_bQuickRetry;
		/**
		 * @brief 
		 * 
		 * �Ƿ���Ҫ����ack
		 */
		bool m_bSendAck;
		/**
		 * @brief 
		 * 
		 * ���һ��ack
		 */
		unsigned char m_btAckLast;
		/**
		 * @brief 
		 * 
		 * ���һ��syn
		 */
		unsigned char m_btSynLast;
		/**
		 * @brief 
		 * 
		 * ��ʱʱ��
		 */
		double m_dAckOutTime; //Ĭ����5
	};

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

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline BufferContral<BUFF_SIZE, WINDOW_SIZE>::~BufferContral()
	{ }

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline int BufferContral<BUFF_SIZE, WINDOW_SIZE>::Init(int dwState)
	{
		this->m_dwStatus = dwState;
		this->m_dDelayTime = 0;
		this->m_dSendFrequency = UDP_SEND_FREQUENCY;
		this->m_dDelayAverage = 3 * m_dSendFrequency;
		this->m_dRetryTime = m_dDelayTime + 2 * m_dDelayAverage;
		this->m_dSendTime = 0;

		this->m_dAckRecvTime = FxIoModule::Instance()->FxGetCurrentTime();
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

		// ��ջ�������
		this->m_oRecvWindow.ClearBuffer();
		this->m_oSendWindow.ClearBuffer();

		// ��ʼ�����ʹ���
		this->m_oSendWindow.m_btBegin = 1;
		this->m_oSendWindow.m_btEnd = m_oSendWindow.m_btBegin;

		// ��ʼ�����մ���
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
			//���Ȳ������ӵ������
			if (this->m_oSendWindow.m_btEnd - this->m_oSendWindow.m_btBegin > this->m_dSendWindowControl) break;

			unsigned char btId = this->m_oSendWindow.m_btEnd % _SendWindow::window_size;

			// ��ȡһ��֡
			unsigned char btBufferId = this->m_oSendWindow.m_btFreeBufferId;
			this->m_oSendWindow.m_btFreeBufferId = this->m_oSendWindow.m_btarrBuffer[btBufferId][0];

			// ���͵Ļ���
			unsigned char* pBuffer = this->m_oSendWindow.m_btarrBuffer[btBufferId];

			// ��������ͷ
			UDPPacketHeader& oPacket = *(UDPPacketHeader*)pBuffer;
			oPacket.m_btStatus = this->m_dwStatus;
			oPacket.m_btSyn = this->m_oSendWindow.m_btEnd;
			oPacket.m_btAck = this->m_oRecvWindow.m_btBegin - 1;

			// ��������
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

			// ��ӵ����ʹ���
			this->m_oSendWindow.Add2SendWindow(btId, btBufferId, dwCopySize + dwCopyOffset
				, FxIoModule::Instance()->FxGetCurrentTime(), m_dRetryTime);
		}

		return dwSendSize;
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline ErrorCode BufferContral<BUFF_SIZE, WINDOW_SIZE>::SendMessages(double dTime, std::ostream* pOStream)
	{
		// ����Ƿ�ʱ
		if (dTime - this->m_dAckRecvTime > m_dAckOutTime)
		{
			this->m_dAckRecvTime = dTime;

			if (--this->m_dwAckTimeoutRetry <= 0)
			{
				return ErrorCode(CODE_ERROR_NET_UDP_ACK_TIME_OUT_RETRY, __FILE__ ":" __LINE2STR__(__LINE__));
			}
		}

		if (dTime < this->m_dSendTime) { return ErrorCode(); }

		//if (m_dwStatus == ST_SYN_RECV) {return 0;}

		bool bForceRetry = false;

		if (this->m_dwStatus == ST_ESTABLISHED)
		{
			//3����ͬack ��ʼ�����ش�
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
					//��ͬackʱ ӵ�����ƴ���+1
					this->m_dSendWindowControl += 1;
					if (this->m_dSendWindowControl > _SendWindow::window_size)
					{
						this->m_dSendWindowControl = _SendWindow::window_size;
					}
				}
			}
			else
			{
				//���µ�ack ��ô�����ش�����
				if (this->m_bQuickRetry == true)
				{
					this->m_dSendWindowControl = this->m_dSendWindowThreshhold;
					this->m_bQuickRetry = false;
				}
			}

			//�����ʱ(����rto) ��ô���½���������
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

		//û�����ݷ��� ��ô����һ���յ� ����ͬ����������
		if (this->m_oSendWindow.m_btBegin == this->m_oSendWindow.m_btEnd)
		{
			if (dTime >= this->m_dSendEmptyDataTime)
			{
				if (this->m_oSendWindow.m_btFreeBufferId < this->m_oSendWindow.window_size)
				{
					unsigned char btId = this->m_oSendWindow.m_btEnd % this->m_oSendWindow.window_size;

					// ��ȡ����
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

					// ��ӵ����ʹ���
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

		//��ʼ����
		for (unsigned char i = this->m_oSendWindow.m_btBegin; i != this->m_oSendWindow.m_btEnd; i++)
		{
			//������ͳ��ȳ���ӵ������ ��ֹͣ
			if (i - this->m_oSendWindow.m_btBegin >= this->m_dSendWindowControl) { break; }

			unsigned char btId = i % _SendWindow::window_size;
			unsigned short wSize = this->m_oSendWindow.m_warrSeqSize[btId];

			//��ʼ���� ���� �ش�
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
				ErrorCode oErrorCode = (*this->m_pSendOperator)((char*)pBuffer, wSize, dwLen, pOStream);
				if (oErrorCode)
				{
					if (EAGAIN == oErrorCode || EINTR == oErrorCode)
					{
						break;
					}
					//���ͳ��� �Ͽ�����
					return oErrorCode;
				}
				else
				{
					this->m_dwNumBytesSend += dwLen;
				}

				// ���Ͱ�����
				this->m_dwNumPacketsSend++;

				// ���Դ���
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
			if (ErrorCode oErrorCode = (*this->m_pSendOperator)((char*)(&oPacket), sizeof(oPacket), dwLen, pOStream))
			{
				//���ͳ��� �Ͽ�����
				return oErrorCode;
			}

			this->m_dSendTime = dTime + this->m_dSendFrequency;
			this->m_bSendAck = false;
		}

		return ErrorCode();
	}

	template<unsigned short BUFF_SIZE, unsigned short WINDOW_SIZE>
	inline ErrorCode BufferContral<BUFF_SIZE, WINDOW_SIZE>::ReceiveMessages(double dTime, bool& refbReadable, std::ostream* pOStream)
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

			// û��buffer�� �Ͽ�����
			if (btBufferId >= this->m_oRecvWindow.window_size)
			{
				return ErrorCode(CODE_ERROR_NET_UDP_ALLOC_BUFF, __FILE__ ":" __LINE2STR__(__LINE__));
			}

			int dwLen = 0;
			ErrorCode oErrorCode = (*this->m_pRecvOperator)((char*)pBuffer, _RecvWindow::buff_size, dwLen, pOStream);

			if (oErrorCode)
			{
				if (EINTR == oErrorCode)
				{
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					continue;
				}

				if (EAGAIN == oErrorCode)
				{
					refbReadable = false;
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}

#ifdef _WIN32
				if (WSA_IO_PENDING == oErrorCode || CODE_SUCCESS_NO_BUFF_READ == oErrorCode)
#else
				if (EAGAIN == oErrorCode || EWOULDBLOCK == oErrorCode)
#endif	//_WIN32
				{
					refbReadable = false;
					pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
					break;
				}

				return oErrorCode;
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
					(*this->m_pOnConnectedOperator)(pOStream);
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

			//�����ֽ���
			this->m_dwNumBytesReceived += dwLen + 28;

			// packet header
			UDPPacketHeader& packet = *(UDPPacketHeader*)pBuffer;

			LOG(pOStream, ELOG_LEVEL_DEBUG3) << "status:" << (int)packet.m_btStatus << ", syn:" << (int)packet.m_btSyn << ", ack:" << (int)packet.m_btAck << "\n";

			//�յ�һ����Ч��ack ��ôҪ���·��ʹ��ڵ�״̬
			if (this->m_oSendWindow.IsValidIndex(packet.m_btAck))
			{
				// ��õ�����һ����Ч�İ�
				this->m_dAckRecvTime = dTime;
				this->m_dwAckTimeoutRetry = 3;

				// ���ڼ���delay������
				static const double err_factor = 0.125;
				static const double average_factor = 0.25;
				// static const double retry_factor = 2;
				static const double retry_factor = 1.5;

				double rtt = m_dDelayTime;
				double dErrTime = 0;

				// m_SendWindowControl �����m_SendWindowControl ����
				double send_window_control_max = this->m_dSendWindowControl * 2;
				if (send_window_control_max > _SendWindow::window_size)
				{
					send_window_control_max = _SendWindow::window_size;
				}

				while (this->m_oSendWindow.m_btBegin != (unsigned char)(packet.m_btAck + 1))
				{
					unsigned char id = this->m_oSendWindow.m_btBegin % _SendWindow::window_size;
					unsigned char btBufferId = this->m_oSendWindow.m_btarrSeqBufferId[id];

					//ֻʹ��û���ط��İ������ӳ�
					if (this->m_oSendWindow.m_btarrSeqBufferId[id] == 1)
					{
						// rtt(�հ��ӳ�ʱ��)
						rtt = dTime - this->m_oSendWindow.m_darrSeqTime[id];
						//ʹ�� rtt �� m_dDelayTime ��ȡ dErrTime
						dErrTime = rtt - m_dDelayTime;
						//ʹ�� dErrTime ���¼��� m_dDelayTime
						this->m_dDelayTime = this->m_dDelayTime + err_factor * dErrTime;
						//ʹ�� dErrTime ���¼��� m_dDelayAverage
						this->m_dDelayAverage = this->m_dDelayAverage + average_factor * (fabs(dErrTime) - this->m_dDelayAverage);
					}

					// �ͷŻ���
					this->m_oSendWindow.m_btarrBuffer[btBufferId][0] = this->m_oSendWindow.m_btFreeBufferId;
					this->m_oSendWindow.m_btFreeBufferId = btBufferId;
					this->m_oSendWindow.m_btBegin++;

					//�յ��µ�ack
					//��� m_SendWindowControl �� m_dSendWindowThreshhold �� Ϊӵ������
					//����Ϊ������
					//ӵ�������� m_SendWindowControl ÿ������ 1 / m_dSendWindowControl
					//�������� m_SendWindowControl ÿ������ 1
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

				//ʹ�� m_dDelayTime �� m_dDelayAverage �������Ե�ʱ��
				this->m_dRetryTime = this->m_dDelayTime + retry_factor * this->m_dDelayAverage;
				if (this->m_dRetryTime < this->m_dSendFrequency) this->m_dRetryTime = this->m_dSendFrequency;
			}

			//�յ���ͬack(����3��Ҫ��ʼѡ���ش�)
			if (this->m_btAckLast == this->m_oSendWindow.m_btBegin - 1)
			{
				this->m_dwAckSameCount++;
			}
			else
			{
				this->m_dwAckSameCount = 0;
			}

			//���յ�����һ����Ч�İ�
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

					// û�и���Ľ��ջ��� ��ô�ȴ������
					if (this->m_oRecvWindow.m_btFreeBufferId >= _RecvWindow::window_size) { break; }
					else { continue; }
				}
			}

			// �ͷ�buffer
			pBuffer[0] = this->m_oRecvWindow.m_btFreeBufferId;
			this->m_oRecvWindow.m_btFreeBufferId = btBufferId;
		}

		if (this->m_oSendWindow.m_btBegin == this->m_oSendWindow.m_btEnd) { this->m_dwAckSameCount = 0; }

		// ��¼���һ��ack
		this->m_btAckLast = this->m_oSendWindow.m_btBegin - 1;

		// ���½��մ���
		if (bPacketReceived)
		{
			unsigned char btLastAck = this->m_oRecvWindow.m_btBegin - 1;
			unsigned char btNewAck = btLastAck;
			//bool bParseMessage = false;

			//�����µ�ack
			for (unsigned char i = this->m_oRecvWindow.m_btBegin; i != this->m_oRecvWindow.m_btEnd; i++)
			{
				// ���յ���buff�Ƿ���Ч
				if (this->m_oRecvWindow.m_btarrSeqBufferId[i % _RecvWindow::window_size] >= _RecvWindow::window_size)
					break;

				btNewAck = i;

				LOG(pOStream, ELOG_LEVEL_DEBUG3) << "recv new_ack:" << (int)btNewAck << "\n";
			}

			// ���µ�ack
			if (btNewAck != btLastAck)
			{
				while (this->m_oRecvWindow.m_btBegin != (unsigned char)(btNewAck + 1))
				{
					const unsigned char cbtHeadSize = sizeof(UDPPacketHeader);
					unsigned char btId = this->m_oRecvWindow.m_btBegin % _RecvWindow::window_size;
					unsigned char btBufferId = this->m_oRecvWindow.m_btarrSeqBufferId[btId];
					unsigned char* pBuffer = this->m_oRecvWindow.m_btarrBuffer[btBufferId] + cbtHeadSize;
					unsigned short wSize = this->m_oRecvWindow.m_warrSeqSize[btId] - cbtHeadSize;

					if (int dwError = (*this->m_pOnRecvOperator)((char*)pBuffer, wSize, pOStream))
					{
						return dwError;
					}

					// ���ճɹ� �ͷŻ���
					this->m_oRecvWindow.m_btarrBuffer[btBufferId][0] = this->m_oRecvWindow.m_btFreeBufferId;
					this->m_oRecvWindow.m_btFreeBufferId = btBufferId;

					// �Ӷ����Ƴ�
					this->m_oRecvWindow.m_warrSeqSize[btId] = 0;
					this->m_oRecvWindow.m_btarrSeqBufferId[btId] = _RecvWindow::window_size;
					this->m_oRecvWindow.m_btBegin++;
					this->m_oRecvWindow.m_btEnd++;

					// �Ƿ���Խ���
					//bParseMessage = true;

					// ��Ҫ����ack
					this->m_bSendAck = true;
				}
			}

			// ������һ��syn
			this->m_btSynLast = this->m_oRecvWindow.m_btBegin - 1;
		}

		return ErrorCode();
	}

} // namespace FXNET

#endif	//!__BUFF_CONTRAL_H__

