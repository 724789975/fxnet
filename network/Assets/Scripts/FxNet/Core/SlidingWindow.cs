using System;

namespace FxNet.Core
{
    /// <summary>
    /// 滑动窗口默认常量定义
    /// </summary>
    public static class SlidingWindowDef
    {
        public const ushort UdpWindowSize = 32;       // UDP 滑动窗口大小（包数）
        public const ushort UdpWindowBuffSize = 1024; // 每个缓冲区大小（字节）
        public const double UdpSendFrequency = 0.005; // 发送间隔（秒）= 5ms
    }

    /// <summary>
    /// UDP 可靠传输包头（3字节）。
    /// Status: 连接状态 | Syn: 发送序号 | Ack: 确认序号
    /// </summary>
    public struct UDPPacketHeader
    {
        public byte Status; // 连接状态
        public byte Syn;    // 发送序号（当前包的序列号）
        public byte Ack;    // 确认序号（期望收到对端的下一个序号）

        public const int Size = 3; // 包头固定 3 字节

        public void WriteTo(byte[] buffer, int offset)
        {
            buffer[offset] = Status;
            buffer[offset + 1] = Syn;
            buffer[offset + 2] = Ack;
        }

        public void ReadFrom(byte[] buffer, int offset)
        {
            Status = buffer[offset];
            Syn = buffer[offset + 1];
            Ack = buffer[offset + 2];
        }
    }

    /// <summary>
    /// 滑动窗口基类，管理一组固定大小的缓冲区池。
    /// 
    /// 【窗口结构】
    ///   Begin          End
    ///     |   已发送未确认   |   空闲窗口   |
    ///     |←— WindowSize ——→|
    /// 
    /// 【缓冲区管理】
    /// - Buffers[]: 固定大小的字节数组池，每个缓冲区大小为 BuffSize
    /// - FreeBufferId: 空闲链表头，通过 buffer[0] 存储下一个空闲索引
    /// - 分配: bufferId = FreeBufferId; FreeBufferId = buffer[bufferId][0]
    /// - 回收: buffer[bufferId][0] = FreeBufferId; FreeBufferId = bufferId
    /// 
    /// 【序号映射】
    /// - SeqBufferId[seq % WindowSize]: 序号 → 缓冲区索引
    /// - SeqSize/SeqTime/SeqRetry: 序号 → 包大小/首次发送时间/重传时间
    /// </summary>
    public class SlidingWindow
    {
        public const int BuffSize = SlidingWindowDef.UdpWindowBuffSize; // 单缓冲区大小
        public const int WindowSize = SlidingWindowDef.UdpWindowSize;   // 窗口大小

        public byte[][] Buffers;     // 缓冲区池，每个元素为固定大小的字节数组
        public byte FreeBufferId;    // 空闲缓冲区链表头（用 buffer[0] 存储下一个空闲索引）

        public byte[] SeqBufferId;   // 序号 → 缓冲区索引映射
        public ushort[] SeqSize;     // 序号 → 数据包大小
        public double[] SeqTime;     // 序号 → 首次发送时间
        public double[] SeqRetry;    // 序号 → 下次重传时间
        public double[] SeqRetryTime;// 序号 → 重传间隔
        public uint[] SeqRetryCount; // 序号 → 重传次数

        public byte Begin; // 窗口起始序号（未确认的最小序号）
        public byte End;   // 窗口结束序号（下一个待发送/接收的序号）

        public SlidingWindow()
        {
            Buffers = new byte[WindowSize][];
            for (int i = 0; i < WindowSize; i++)
                Buffers[i] = new byte[BuffSize];

            SeqBufferId = new byte[WindowSize];
            SeqSize = new ushort[WindowSize];
            SeqTime = new double[WindowSize];
            SeqRetry = new double[WindowSize];
            SeqRetryTime = new double[WindowSize];
            SeqRetryCount = new uint[WindowSize];
        }

        /// <summary>窗口中当前包数量</summary>
        public byte Count() => (byte)(End - Begin);

        /// <summary>判断给定序号是否在当前窗口 [Begin, End) 范围内</summary>
        public bool IsValidIndex(byte id)
        {
            byte pos = (byte)(id - Begin);
            byte count = (byte)(End - Begin);
            return pos < count;
        }

        /// <summary>初始化空闲缓冲区链表，每个 buffer[0] 指向下一个空闲索引</summary>
        public void ClearBuffer()
        {
            FreeBufferId = 0;
            for (int i = 0; i < WindowSize; i++)
                Buffers[i][0] = (byte)(i + 1);
        }
    }

    /// <summary>
    /// 发送窗口，继承自 SlidingWindow，增加将数据包加入发送队列的方法
    /// </summary>
    public class SendWindow : SlidingWindow
    {
        /// <summary>将数据包加入发送窗口队列，并前移 End 指针</summary>
        public SendWindow Add2SendWindow(byte id, byte buffId, ushort packetSize, double time, double retryTime)
        {
            SeqBufferId[id] = buffId;
            SeqSize[id] = packetSize;
            SeqTime[id] = time;
            SeqRetry[id] = time;
            SeqRetryTime[id] = retryTime;
            SeqRetryCount[id] = 0;
            End++;
            return this;
        }
    }
}
