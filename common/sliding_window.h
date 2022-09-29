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
		enum { buff_size = BUFF_SIZE };
		enum { window_size = WINDOW_SIZE };

		/**
		 * @brief 已发送待确认/已接收待确认
		 *
		 * @return unsigned char 待确认数量
		 */
		inline unsigned char Count()
		{
			return m_btEnd - m_btBegin;
		}

		/**
		 * @brief 是否有效id
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
		 * 
		 * @return SlidingWindow& 返回自身
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
		 * @brief 发送/接收 缓冲区
		 */
		unsigned char m_btarrBuffer[WINDOW_SIZE][BUFF_SIZE];
		/**
		 * @brief 空闲的缓冲区帧号
		 */
		unsigned char m_btFreeBufferId;

		/**
		 * @brief 发送/接收 区帧号
		 */
		unsigned char m_btarrSeqBufferId[WINDOW_SIZE];
		/**
		 * @brief 发送/接收 帧大小
		 */
		unsigned short m_warrSeqSize[WINDOW_SIZE];
		/**
		 * @brief 发送时间
		 */
		double m_darrSeqTime[WINDOW_SIZE];
		/**
		 * @brief 重传时间(重传的定时器)
		 */
		double m_darrSeqRetry[WINDOW_SIZE];
		/**
		 * @brief 重传时间间隔(rto)
		 */
		double m_darrSeqRetryTime[WINDOW_SIZE];
		/**
		 * @brief 重传次数
		 */
		unsigned int m_dwarrSeqRetryCount[WINDOW_SIZE];

		/**
		 * @brief 滑动窗口起始位置
		 */
		unsigned char m_btBegin;
		/**
		 * @brief 滑动窗口结束位置
		 */
		unsigned char m_btEnd;
		
	};

	template <unsigned int BUFF_SIZE = 512, unsigned int WINDOW_SIZE = 32>
	class SendWindow : public SlidingWindow<BUFF_SIZE, WINDOW_SIZE>
	{
	public:
		/**
		 * @brief 添加到发送窗口
		 * 
		 * @param btId 帧号
		 * @param btBuffId 缓冲区id
		 * @param wPacketSize 包大小
		 * @param dTime 添加时间
		 * @param dRetryTime 等待多久后开始重试
		 */
		SendWindow& Add2SendWindow(unsigned char btId, unsigned char btBuffId, unsigned short wPacketSize, double dTime, double dRetryTime)
		{
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_btarrSeqBufferId[btId] = btBuffId;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_warrSeqSize[btId] = wPacketSize;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqTime[btId] = dTime;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqRetry[btId] = dTime;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_darrSeqRetryTime[btId] = dRetryTime;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_dwarrSeqRetryCount[btId] = 0;
			SlidingWindow<BUFF_SIZE, WINDOW_SIZE>::m_btEnd++;

			return *this;
		}
		
	};
} // namespace FXNET

#endif //!__SLIDING_WINDOW_H__
