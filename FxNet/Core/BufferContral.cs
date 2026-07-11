using FxNet.IO;
using FxNet.Util;
using System.Text;

namespace FxNet.Core
{
    /// <summary>
    /// 数据接收回调操作接口，当滑动窗口收到有效数据后调用
    /// </summary>
    public interface IOnRecvOperator
    {
        void Execute(byte[] buffer, ushort size, ErrorCode error, TextWriter? output);
    }

    /// <summary>
    /// 连接建立回调操作接口
    /// </summary>
    public interface IOnConnectedOperator
    {
        void Execute(ErrorCode error, TextWriter? output);
    }

    /// <summary>
    /// 底层数据接收操作接口，从 Socket 读取原始 UDP 数据
    /// </summary>
    public interface IRecvOperator
    {
        void Execute(byte[] buffer, ushort buffSize, out int recvSize, ErrorCode error, TextWriter? output);
    }

    /// <summary>
    /// 底层数据发送操作接口，将原始 UDP 数据写 入 Socket
    /// </summary>
    public interface ISendOperator
    {
        void Execute(byte[] buffer, ushort bufferSize, out int sendLen, ErrorCode error, TextWriter? output);
    }

    /// <summary>
    /// 读流操作接口，用于在发送前处理发送缓冲区
    /// </summary>
    public interface IReadStreamOperator
    {
        uint Execute(TextWriter? output);
    }

    /// <summary>
    /// UDP 可靠传输连接状态枚举，类似 TCP 的状态机
    /// </summary>
    public enum ConnectionStatus
    {
        Idle,           // 空闲状态
        SynSend,        // 已发送同步请求
        SynRecv,        // 已接收同步请求
        SynRecvWait,    // 等待同步确认
        Established,    // 连接已建立
        FinWait1,       // 等待关闭1
        FinWait2,       // 等待关闭2
    }

    /// <summary>
    /// UDP 可靠传输核心控制类。
    /// 基于滑动窗口协议实现可靠 UDP，包含：
    /// 
    /// 【核心机制】
    /// 1. 序号确认(ACK): 每个包圲 3 字节头(Status+Syn+Ack)，接收方通过 Ack 字段确认序号
    /// 2. 超时重传: 发送方为每个包设置重传定时器，超时未确认则重传
    /// 3. 快速重传: 收到 3 个重复 ACK 时立即重传，不等定时器超时
    /// 
    /// 【拥塞控制】(类似 TCP AIMD)
    /// - 慢启动阶段 (cwnd &lt; ssthresh): 每收到一个 ACK，cwnd += 1（指数增长）
    /// - 拥塞避免阶段 (cwnd &gt;= ssthresh): 每收到一个 ACK，cwnd += 1/cwnd（线性增长）
    /// - 乘法减小: 检测到丢包时，ssthresh = cwnd/2, cwnd = ssthresh
    /// 
    /// 【RTT 估算】(Jacobian/Karels 算法)
    /// - SRTT = SRTT + 0.125 * (RTT - SRTT)
    /// - RTTVAR = RTTVAR + 0.25 * (|RTT - SRTT| - RTTVAR)
    /// - RTO = SRTT + 1.5 * RTTVAR
    /// 
    /// 【空包保活】
    /// - 无数据时按指数退避发送空包维持连接，上限 1.0 秒
    /// </summary>
    public class BufferContral
    {
        // 发送窗口和接收窗口
        private readonly SendWindow _sendWindow = new SendWindow();
        private readonly SlidingWindow _recvWindow = new SlidingWindow();

        // 回调操作接口
        private IOnRecvOperator? _onRecvOperator;
        private IOnConnectedOperator? _onConnectedOperator;
        private IRecvOperator? _recvOperator;
        private ISendOperator? _sendOperator;
        private IReadStreamOperator? _readStreamOperator;

        // 统计信息
        private uint _numBytesSend;      // 已发送字节数
        private uint _numBytesReceived;  // 已接收字节数

        // RTT (往返时间) 估算相关
        private double _delayTime;       // 当前 RTT 估算值（平滑后）
        private double _delayAverage;    // RTT 平均偏差
        private double _retryTime;       // 重传超时时间 = delayTime + factor * delayAverage

        // 发送频率控制
        private double _sendTime;        // 下次可发送时间
        private double _sendFrequency = SlidingWindowDef.UdpSendFrequency; // 发送间隔
        private double _sendWindowControl;    // 当前拥塞窗口大小
        private double _sendWindowThreshhold; // 拥塞窗口阈值（ssthresh）

        // 空包保活（无数据时发送空包维持连接）
        private double _sendEmptyDataTime;       // 下次发送空包时间
        private double _sendEmptyDataFrequency = 0.1; // 空包发送基础频率
        private int _sendEmptyDataFactor;        // 空包频率指数退避因子

        private uint _numPacketsSend;  // 已发送包数
        private uint _numPacketsRetry; // 重传包数

        private bool _connected;       // 连接是否已建立
        private double _ackRecvTime;   // 最后一次收到 ACK 的时间
        private int _ackTimeoutRetry;  // ACK 超时重试计数器
        private uint _status;          // 当前连接状态

        // 拥塞控制相关
        private int _ackSameCount;     // 重复 ACK 计数（用于触发快速重传）
        private bool _quickRetry;      // 是否处于快速重传模式
        private bool _sendAck;         // 是否需要发送纯 ACK 包
        private byte _ackLast;         // 上一次发送的 ACK 序号
        private byte _synLast;         // 上一次接收的 SYN 序号
        private double _ackOutTime = 5.0; // ACK 超时判定时间（秒）

        public BufferContral() { }

        /// <summary>
        /// 初始化可靠传输控制状态
        /// </summary>
        /// <param name="state">初始连接状态</param>
        /// <param name="ackRecvTime">初始 ACK 接收时间（用于超时计算起点）</param>
        public int Init(int state, double ackRecvTime)
        {
            _status = (uint)state;
            _delayTime = 0;
            _sendFrequency = SlidingWindowDef.UdpSendFrequency;
            _delayAverage = 3 * _sendFrequency;  // 初始 RTT 偏差设为发送间隔的 3 倍
            _retryTime = _delayTime + 2 * _delayAverage; // 初始重传时间
            _sendTime = 0;
            _ackRecvTime = ackRecvTime;
            _ackTimeoutRetry = 1;
            _ackSameCount = 0;
            _quickRetry = false;
            _sendEmptyDataTime = 0;
            _ackLast = 0;
            _synLast = 0;
            _sendAck = false;
            _sendWindowControl = SlidingWindow.WindowSize;    // 初始拥塞窗口 = 最大窗口
            _sendWindowThreshhold = SlidingWindow.WindowSize; // 初始阈值 = 最大窗口

            _recvWindow.ClearBuffer();
            _sendWindow.ClearBuffer();

            _sendWindow.Begin = 1;
            _sendWindow.End = _sendWindow.Begin;

            _recvWindow.Begin = 1;
            _recvWindow.End = (byte)(_recvWindow.Begin + SlidingWindow.WindowSize);

            // 初始化接收窗口序号映射：WindowSize 表示空槽位
            // （bufferId=0 是合法缓冲区，不能用 0 表示空）
            for (int i = 0; i < SlidingWindow.WindowSize; i++)
            {
                _recvWindow.SeqBufferId[i] = SlidingWindow.WindowSize;
                _recvWindow.SeqSize[i] = 0;
                _recvWindow.SeqTime[i] = 0;
                _recvWindow.SeqRetry[i] = 0;
                _recvWindow.SeqRetryCount[i] = 0;
            }

            // 服务端为已接受连接初始化时 state=Established，直接标记已连接
            _connected = state != (int)ConnectionStatus.Idle;
            return 0;
        }

        public int GetState() => (int)_status;

        /// <summary>设置各类回调操作接口</summary>
        public void SetOnRecvOperator(IOnRecvOperator op) => _onRecvOperator = op;
        public void SetOnConnectedOperator(IOnConnectedOperator op) => _onConnectedOperator = op;
        public void SetRecvOperator(IRecvOperator op) => _recvOperator = op;
        public void SetSendOperator(ISendOperator op) => _sendOperator = op;
        public void SetReadStreamOperator(IReadStreamOperator op) => _readStreamOperator = op;
        /// <summary>设置 ACK 超时时间</summary>
        public void SetAckOutTime(double time) => _ackOutTime = time;

        /// <summary>
        /// 将用户数据写入发送滑动窗口。
        /// 每个窗口槽位包含：3字节 UDP 包头(Status+Syn+Ack) + 有效载荷。
        /// 当窗口已满或超出拥塞窗口限制时停止写入。
        /// </summary>
        /// <param name="sendBuffer">待发送数据</param>
        /// <param name="size">数据长度</param>
        /// <returns>实际写入的字节数</returns>
        public uint Send(byte[] sendBuffer, uint size)
        {
            uint sendSize = 0;
            int offset = 0;

            // 循环填充发送窗口，直到窗口满、拥塞窗口满或数据写完
        while (_sendWindow.FreeBufferId < SlidingWindow.WindowSize && size > 0)
        {
            // 确认发送窗口有可发送空间（Begin..End 的槽位数恰好等于待发包数）
            int availableWindow = (int)(_sendWindow.End - _sendWindow.Begin);
            if (availableWindow >= SlidingWindow.WindowSize) break;

            // 拥塞控制：超出当前拥塞窗口则暂停
            if ((int)(_sendWindow.End - _sendWindow.Begin) >= (int)_sendWindowControl) break;

                // 从空闲缓冲区链表分配一个缓冲区
                byte id = (byte)(_sendWindow.End % SlidingWindow.WindowSize);
                byte bufferId = _sendWindow.FreeBufferId;
                _sendWindow.FreeBufferId = _sendWindow.Buffers[bufferId][0];

                byte[] buffer = _sendWindow.Buffers[bufferId];

                // 写入 UDP 包头（3字节：Status/Syn/Ack）
                var packet = new UDPPacketHeader
                {
                    Status = (byte)_status,
                    Syn = _sendWindow.End,              // 发送序号
                    Ack = (byte)(_recvWindow.Begin - 1) // 确认序号 = 期望接收的下一个序号
                };
                packet.WriteTo(buffer, 0);

                // 将用户数据拷贝到包头之后的载荷区域
                int copyOffset = UDPPacketHeader.Size;
                int copySize = SlidingWindow.BuffSize - copyOffset;
                if (copySize > size) copySize = (int)size;

                if (copySize > 0)
                {
                    Array.Copy(sendBuffer, offset, buffer, copyOffset, copySize);
                    size -= (uint)copySize;
                    offset += copySize;
                    sendSize += (uint)copySize;
                }

                // 将数据包加入发送窗口队列
                _sendWindow.Add2SendWindow(id, bufferId, (ushort)(copySize + copyOffset),
                    FxNetInterface.GetNow(), _retryTime);
            }

            return sendSize;
        }

        /// <summary>
        /// 定时调用的发送处理函数。
        /// 负责：ACK 超时检测、拥塞控制窗口调整、数据包重传、空包保活。
        /// 应由 IO 线程周期性调用。
        /// </summary>
        public void SendMessages(double time, ErrorCode error, TextWriter? output)
        {
            if (_sendOperator == null) return;

            // ACK 超时检测：长时间未收到对端 ACK，触发超时重试
            if (time - _ackRecvTime > _ackOutTime)
            {
                _ackRecvTime = time;
                if (--_ackTimeoutRetry <= 0)
                {
                    error.Set((int)UserError.CodeErrorNetUdpAckTimeOutRetry, $"{nameof(BufferContral)}:{nameof(SendMessages)}");
                    return;
                }
            }

            // 发送频率限制：未到下次发送时间则跳过
            if (time < _sendTime) return;

            bool forceRetry = false;

            // === 拥塞控制（连接已建立时生效）===
            if (_status == (uint)ConnectionStatus.Established)
            {
                // 重复 ACK 超过等于 3 次 → 触发快速重传/快速恢复（TCP 标准：dupthresh=3）
            if (_ackSameCount >= 3)
                {
                    if (!_quickRetry)
                    {
                        // 首次进入快速重传：ssthresh = cwnd/2, cwnd = ssthresh + dupAckCount
                        _quickRetry = true;
                        forceRetry = true;
                        _sendWindowThreshhold = _sendWindowControl / 2;
                        if (_sendWindowThreshhold < 2) _sendWindowThreshhold = 2;
                        _sendWindowControl = _sendWindowThreshhold + _ackSameCount - 1;
                        if (_sendWindowControl > SlidingWindow.WindowSize)
                            _sendWindowControl = SlidingWindow.WindowSize;
                    }
                    else
                    {
                        // 快速重传模式中，每多一个重复 ACK 窗口加 1
                        _sendWindowControl += 1;
                        if (_sendWindowControl > SlidingWindow.WindowSize)
                            _sendWindowControl = SlidingWindow.WindowSize;
                    }
                }
                else
                {
                    // 重复 ACK 恢复正常，退出快速重传
                    if (_quickRetry)
                    {
                        _sendWindowControl = _sendWindowThreshhold;
                        _quickRetry = false;
                    }
                }

                // 检测已重传且到达重传时间的包，触发超时重传（减半窗口）
                for (byte i = _sendWindow.Begin; i != _sendWindow.End; i++)
                {
                    byte id = (byte)(i % SlidingWindow.WindowSize);
                    if (_sendWindow.SeqRetryCount[id] > 0 && time >= _sendWindow.SeqRetry[id])
                    {
                        _sendWindowThreshhold = _sendWindowControl / 2;
                        if (_sendWindowThreshhold < 2) _sendWindowThreshhold = 2;
                        _sendWindowControl = _sendWindowThreshhold;
                        _quickRetry = false;
                        _ackSameCount = 0;
                        break;
                    }
                }

                _readStreamOperator?.Execute(output);
            }

            // === 空包保活逻辑 ===
            // 发送窗口为空时，按指数退避频率发送空包维持连接
            if (_sendWindow.Begin == _sendWindow.End)
            {
                if (time >= _sendEmptyDataTime)
                {
                    if (_sendWindow.FreeBufferId < SlidingWindow.WindowSize)
                    {
                        byte id = (byte)(_sendWindow.End % SlidingWindow.WindowSize);
                        byte bufferId = _sendWindow.FreeBufferId;
                        _sendWindow.FreeBufferId = _sendWindow.Buffers[bufferId][0];
                        byte[] buffer = _sendWindow.Buffers[bufferId];

                        // 构造仅含头部的空包
                        var packet = new UDPPacketHeader
                        {
                            Status = (byte)_status,
                            Syn = _sendWindow.End,
                            Ack = (byte)(_recvWindow.Begin - 1)
                        };
                        packet.WriteTo(buffer, 0);

                        _sendWindow.Add2SendWindow(id, bufferId, (ushort)UDPPacketHeader.Size, time, _retryTime);
                    }
                    // 指数退避：间隔逐渐增大，上限 1.0 秒
                    double tempFreq = _sendEmptyDataFrequency * (1 << _sendEmptyDataFactor);
                    if (1.0 < tempFreq) tempFreq = 1.0;
                    _sendEmptyDataTime = time + tempFreq;
                    if (_sendEmptyDataFactor < 30) _sendEmptyDataFactor++;
                }
            }
            else
            {
                // 有数据发送时重置空包退避
                _sendEmptyDataTime = time + _sendEmptyDataFrequency;
                _sendEmptyDataFactor = 0;
            }

            // === 实际发送窗口中的数据包 ===
            // 遍历 [Begin, End) 范围内的未确认包，超时则重传
            for (byte i = _sendWindow.Begin; i != _sendWindow.End; i++)
            {
                // 超出拥塞窗口则停止
                if (i - _sendWindow.Begin >= _sendWindowControl) break;

                byte id = (byte)(i % SlidingWindow.WindowSize);
                ushort size = _sendWindow.SeqSize[id];

                // 到达重传时间或强制重传
                if (time >= _sendWindow.SeqRetry[id] || forceRetry)
                {
                    forceRetry = false;
                    byte[] buffer = _sendWindow.Buffers[_sendWindow.SeqBufferId[id]];

                    // 更新包头中的序号和 ACK
                    var packet = new UDPPacketHeader();
                    packet.ReadFrom(buffer, 0);
                    packet.Status = (byte)_status;
                    packet.Syn = i;
                    packet.Ack = (byte)(_recvWindow.Begin - 1);
                    packet.WriteTo(buffer, 0);

                    // 调用底层发送
                    _sendOperator.Execute(buffer, size, out int dwLen, error, output);
                    if (error)
                    {
                        error.Set(0, "");
                        break;
                    }
                    _numBytesSend += (uint)dwLen;
                    _numPacketsSend++;
                    if (time != _sendWindow.SeqTime[id]) _numPacketsRetry++; // 非首次发送 = 重传

                    _sendTime = time + _sendFrequency; // 设置下次可发送时间
                    _sendAck = false;

                    // 更新重传计数和下次重传时间
                    _sendWindow.SeqRetryCount[id]++;
                    _sendWindow.SeqRetryTime[id] = 1.5 * _retryTime;
                    if (_sendWindow.SeqRetryTime[id] > 0.2) _sendWindow.SeqRetryTime[id] = 0.2; // 上限 200ms
                    _sendWindow.SeqRetry[id] = time + _sendWindow.SeqRetryTime[id];
                }
            }

            // === 发送纯 ACK 包（无数据载荷，仅确认对端数据）===
            if (_sendAck)
            {
                byte[] packetBuf = new byte[UDPPacketHeader.Size];
                var ackPacket = new UDPPacketHeader
                {
                    Status = (byte)_status,
                    Syn = (byte)(_sendWindow.Begin - 1),
                    Ack = (byte)(_recvWindow.Begin - 1)
                };
                ackPacket.WriteTo(packetBuf, 0);

                _sendOperator.Execute(packetBuf, (ushort)UDPPacketHeader.Size, out _, error, output);
                if (error) return;

                _sendTime = time + _sendFrequency;
                _sendAck = false;
            }
        }

        /// <summary>
        /// 处理从底层接收到的 UDP 数据包。
        /// 负责：解析包头、ACK 确认滑动发送窗口、RTT 估算、拥塞窗口调整、
        /// 将有序数据按序号放入接收窗口、向上层回调可交付的数据。
        /// </summary>
        /// <param name="time">当前时间</param>
        /// <param name="readable">输入/输出：是否继续读取（缓冲区耗尽时设为 false）</param>
        /// <param name="error">错误码</param>
        /// <param name="output">诊断输出</param>
        public void ReceiveMessages(double time, ref bool readable, ErrorCode error, TextWriter? output)
        {
            if (_recvOperator == null) return;

            bool packetReceived = false; // 是否收到了新的有序数据包

            while (readable)
            {
                // 从空闲缓冲区链表分配一个接收缓冲区
                byte bufferId = _recvWindow.FreeBufferId;
                byte[] buffer = _recvWindow.Buffers[bufferId];
                _recvWindow.FreeBufferId = buffer[0];

                if (bufferId >= SlidingWindow.WindowSize)
                {
                    error.Set((int)UserError.CodeErrorNetUdpAllocBuff, $"{nameof(BufferContral)}:{nameof(ReceiveMessages)}");
                    return;
                }

                // 调用底层读取原始 UDP 数据
                _recvOperator.Execute(buffer, (ushort)SlidingWindow.BuffSize, out int len, error, output);

                if (error)
                {
                    buffer[0] = _recvWindow.FreeBufferId;
                    _recvWindow.FreeBufferId = bufferId;
                    error.Set(0, "");
                    readable = false;
                    break;
                }

                // 无数据可读，退出循环
                if (len == 0)
                {
                    buffer[0] = _recvWindow.FreeBufferId;
                    _recvWindow.FreeBufferId = bufferId;
                    readable = false;
                    break;
                }

                // 数据不足一个包头，丢弃并回收缓冲区
                if (len < UDPPacketHeader.Size)
                {
                    buffer[0] = _recvWindow.FreeBufferId;
                    _recvWindow.FreeBufferId = bufferId;
                    continue;
                }

                // 首次连接：解析对端发来的建连包，触发 OnConnected 回调
                if (!_connected)
                {
                    var pkt = new UDPPacketHeader();
                    pkt.ReadFrom(buffer, 0);
                    _status = (uint)ConnectionStatus.Established;
                    if (pkt.Status == (byte)ConnectionStatus.Established)
                    {
                        _onConnectedOperator?.Execute(error, output);
                        _connected = true;

                        // 清空接收窗口序列信息
                        for (byte i = _recvWindow.Begin; i != _recvWindow.End; i++)
                        {
                            byte idx = (byte)(i % SlidingWindow.WindowSize);
                            _recvWindow.SeqBufferId[idx] = SlidingWindow.WindowSize;
                            _recvWindow.SeqSize[idx] = 0;
                            _recvWindow.SeqTime[idx] = 0;
                            _recvWindow.SeqRetry[idx] = 0;
                            _recvWindow.SeqRetryCount[idx] = 0;
                        }
                    }
                }

                _numBytesReceived += (uint)(len + 28); // +28 为 IP+UDP 头估算

                var packet = new UDPPacketHeader();
                packet.ReadFrom(buffer, 0);

                // === 处理 ACK：滑动发送窗口，回收已确认的缓冲区 ===
                if (_sendWindow.IsValidIndex(packet.Ack))
                {
                    _ackRecvTime = time;
                    _ackTimeoutRetry = 3;

                    // RTT 估算常数（类似 TCP 的 Jacobson/Karels 算法）
                    const double errFactor = 0.125;    // SRTT 平滑因子 (1/8)
                    const double averageFactor = 0.25; // RTTVAR 平滑因子 (1/4)
                    const double retryFactor = 1.5;    // 重传时间 = SRTT + 1.5 * RTTVAR

                    double rtt = _delayTime;
                    double dErrTime = 0;

                    double sendWindowMax = _sendWindowControl * 2;
                    if (sendWindowMax > SlidingWindow.WindowSize)
                        sendWindowMax = SlidingWindow.WindowSize;

                    // 滑动发送窗口：释放已确认的缓冲区
                    while (_sendWindow.Begin != (byte)(packet.Ack + 1))
                    {
                        byte sid = (byte)(_sendWindow.Begin % SlidingWindow.WindowSize);
                        byte sBufId = _sendWindow.SeqBufferId[sid];

                        // 仅对首次确认的包计算 RTT
                        if (_sendWindow.SeqBufferId[sid] == 1)
                        {
                            rtt = time - _sendWindow.SeqTime[sid];
                            dErrTime = rtt - _delayTime;
                            _delayTime += errFactor * dErrTime;
                            _delayAverage += averageFactor * (Math.Abs(dErrTime) - _delayAverage);
                        }

                        // 回收缓冲区到空闲链表
                        _sendWindow.Buffers[sBufId][0] = _sendWindow.FreeBufferId;
                        _sendWindow.FreeBufferId = sBufId;
                        _sendWindow.Begin++;

                        // 拥塞窗口增长（AIMD：加法增大）
                        if (_sendWindowControl <= _sendWindowThreshhold)
                            _sendWindowControl += 1;            // 慢启动阶段：指数增长（每 ACK +1）
                        else
                            _sendWindowControl += 1 / _sendWindowControl; // 拥塞避免阶段：线性增长

                        if (_sendWindowControl > sendWindowMax)
                            _sendWindowControl = sendWindowMax;
                    }

                    // 更新重传时间 = SRTT + 1.5 * RTTVAR
                    _retryTime = _delayTime + retryFactor * _delayAverage;
                    if (_retryTime < _sendFrequency) _retryTime = _sendFrequency; // 不低于发送间隔
                }

                // 检测重复 ACK（对端未收到新数据）
                if (_ackLast == _sendWindow.Begin - 1)
                    _ackSameCount++;
                else
                    _ackSameCount = 0;

                // === 处理接收数据：将包按序号放入接收窗口 ===
                if (_recvWindow.IsValidIndex(packet.Syn))
                {
                    byte rid = (byte)(packet.Syn % SlidingWindow.WindowSize);

                    // 该序号位置尚未有数据，存入接收窗口
                    if (_recvWindow.SeqBufferId[rid] >= SlidingWindow.WindowSize)
                    {
                        _recvWindow.SeqBufferId[rid] = bufferId;
                        _recvWindow.SeqSize[rid] = (ushort)len;
                        packetReceived = true; // 标记收到了新的有序数据

                        if (_recvWindow.FreeBufferId >= SlidingWindow.WindowSize) break; // 缓冲区耗尽
                        else continue; // 继续读取下一个包
                    }
                }

                // 重复包或无效包，回收缓冲区
                buffer[0] = _recvWindow.FreeBufferId;
                _recvWindow.FreeBufferId = bufferId;
            }

            // 发送窗口为空时重置重复 ACK 计数
            if (_sendWindow.Begin == _sendWindow.End) _ackSameCount = 0;
            _ackLast = (byte)(_sendWindow.Begin - 1);

            // === 向上层交付有序数据 ===
            // 从接收窗口 Begin 开始，连续交付直到遇到空缺
            if (packetReceived)
            {
                byte lastAck = (byte)(_recvWindow.Begin - 1);
                byte newAck = lastAck;

                // 查找连续可交付的最大序号
                for (byte i = _recvWindow.Begin; i != _recvWindow.End; i++)
                {
                    if (_recvWindow.SeqBufferId[i % SlidingWindow.WindowSize] >= SlidingWindow.WindowSize)
                        break;
                    newAck = i;
                }

                // 有新的可交付数据，逐包回调上层
                if (newAck != lastAck)
                {
                    while (_recvWindow.Begin != (byte)(newAck + 1))
                    {
                        byte rid = (byte)(_recvWindow.Begin % SlidingWindow.WindowSize);
                        byte rBufId = _recvWindow.SeqBufferId[rid];
                        byte[] rBuffer = _recvWindow.Buffers[rBufId];
                        ushort rSize = (ushort)(_recvWindow.SeqSize[rid] - UDPPacketHeader.Size); // 去掉包头

                        // 回调上层交付数据（跳过 3 字节包头）
                        _onRecvOperator?.Execute(
                            rBuffer.AsSpan(UDPPacketHeader.Size, rSize).ToArray(),
                            rSize, error, output);
                        if (error) return;

                        // 回收缓冲区并前移接收窗口
                        _recvWindow.Buffers[rBufId][0] = _recvWindow.FreeBufferId;
                        _recvWindow.FreeBufferId = rBufId;
                        _recvWindow.SeqSize[rid] = 0;
                        _recvWindow.SeqBufferId[rid] = SlidingWindow.WindowSize;
                        _recvWindow.Begin++;
                        _recvWindow.End++;

                        _sendAck = true; // 需要发送 ACK 确认
                    }
                }

                _synLast = (byte)(_recvWindow.Begin - 1);
            }
        }
    }
}
