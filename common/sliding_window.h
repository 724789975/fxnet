#ifndef __SLIDING_WINDOW_H__
#define __SLIDING_WINDOW_H__

namespace FXNET
{
	struct UDPPacketHeader
	{
		unsigned char m_btStatus;
		unsigned char m_btSyn;
		unsigned char m_btAck;
	};

	template <unsigned short BUFF_SIZE = 512, unsigned short WINDOW_SIZE = 32>
	class SlidingWindow
	{
	public:
		enum { buff_size   = BUFF_SIZE };
		enum { window_size = WINDOW_SIZE };

		/**
		 * @brief �ѷ��ʹ�ȷ��/�ѽ��մ�ȷ��
		 *
		 * @return unsigned char ��ȷ������
		 */
		inline unsigned char Count()
		{
			return m_btEnd - m_btBegin;
		}

		/**
		 * @brief �Ƿ���Чid
		 *
		 * @param btId
		 * @return true
		 * @return false
		 */
		inline bool IsValidIndex(unsigned char btId)
		{
			unsigned char btPos = btId - m_btBegin;
			unsigned char btCount = m_btEnd - m_btBegin;
			return btPos < btCount;
		}

		/**
		 * @brief 
		 * ��մ�������
		 * @return SlidingWindow& ��������
		 */
		inline SlidingWindow& ClearBuffer()
		{
			// link buffers
			m_btFreeBufferId = 0;
			for (int i = 0; i < WINDOW_SIZE; i++)
			{
				m_btarrBuffer[i][0] = i + 1;
			}

			return *this;
		}

	protected:
	public:
		/**
		 * @brief ����/���� ������
		 */
		unsigned char m_btarrBuffer[WINDOW_SIZE][BUFF_SIZE];
		/**
		 * @brief ���еĻ�����֡��
		 */
		unsigned char m_btFreeBufferId;

		/**
		 * @brief ����/���� ��֡��
		 */
		unsigned char m_btarrSeqBufferId[WINDOW_SIZE];
		/**
		 * @brief ����/���� ֡��С
		 */
		unsigned short m_warrSeqSize[WINDOW_SIZE];
		/**
		 * @brief ����ʱ��
		 */
		double m_darrSeqTime[WINDOW_SIZE];
		/**
		 * @brief �ش�ʱ��(�ش��Ķ�ʱ��)
		 */
		double m_darrSeqRetry[WINDOW_SIZE];
		/**
		 * @brief �ش�ʱ����(rto)
		 */
		double m_darrSeqRetryTime[WINDOW_SIZE];
		/**
		 * @brief �ش�����
		 */
		unsigned int m_dwarrSeqRetryCount[WINDOW_SIZE];

		/**
		 * @brief ����������ʼλ��
		 */
		unsigned char m_btBegin;
		/**
		 * @brief �������ڽ���λ��
		 */
		unsigned char m_btEnd;
		
	};

	template <unsigned int BUFF_SIZE = 512, unsigned int WINDOW_SIZE = 32>
	class SendWindow : public SlidingWindow<BUFF_SIZE, WINDOW_SIZE>
	{
	public:
		/**
		 * @brief ��ӵ����ʹ���
		 * 
		 * @param btId ֡��
		 * @param btBuffId ������id
		 * @param wPacketSize ����С
		 * @param dTime ���ʱ��
		 * @param dRetryTime �ȴ���ú�ʼ����
		 * @return SendWindow& ��������
		 */
		SendWindow& Add2SendWindow(unsigned char btId, unsigned char btBuffId, unsigned short wPacketSize, double dTime, double dRetryTime)
		{
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_btarrSeqBufferId[btId]   = btBuffId;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_warrSeqSize[btId]        = wPacketSize;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqTime[btId]        = dTime;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqRetry[btId]       = dTime;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqRetryTime[btId]   = dRetryTime;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_dwarrSeqRetryCount[btId] = 0;
			this->SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_btEnd++;

			return *this;
		}
		
	};
} // namespace FXNET

#endif //!__SLIDING_WINDOW_H__
