namespace FxNet.Core
{
    /// <summary>
    /// 用户错误码枚举。
    /// 采用位运算构造分层错误码：高位表示严重级别(成功/信息/警告/错误)，
    /// 中间位表示设施代码(Facility)，低位表示具体错误编号。
    /// </summary>
    public enum UserError
    {
        // 设施代码范围
        FacilityBegin = 0x100,
        FacilityNet   = 0x101,  // 网络模块设施
        FacilityEnd   = 0x7FF,

        // 成功类错误码（最高位 29 = 1）
        CodeSuccessBegin       = (1 << 29) | (0x100 << 16),
        CodeSuccessNetBegin    = (1 << 29) | (0x101 << 16),
        CodeSuccessNoBuffRead  = CodeSuccessNetBegin + 1,  // 无缓冲可读
        CodeSuccessNetEOF      = CodeSuccessNetBegin + 2,  // 网络流结束(EOF)
        CodeSuccessNetEnd      = (1 << 29) | (0x101 << 16) | 0xF,
        CodeSuccessEnd         = (1 << 29) | (0x7FF << 16) | 0xF,

        // 信息类错误码（最高位 30+29 = 1）
        CodeInfoBegin       = (1 << 30) | (1 << 29) | (0x100 << 16),
        CodeInfoNetBegin    = (1 << 30) | (1 << 29) | (0x101 << 16),
        CodeInfoNetEnd      = (1 << 30) | (1 << 29) | (0x101 << 16) | 0xF,
        CodeInfoEnd         = (1 << 30) | (1 << 29) | (0x7FF << 16) | 0xF,

        // 警告类错误码（最高位 31+29 = 1）
        CodeWarnBegin       = (2 << 30) | (1 << 29) | (0x100 << 16),
        CodeWarnNetBegin    = (2 << 30) | (1 << 29) | (0x101 << 16),
        CodeWarnNetEnd      = (2 << 30) | (1 << 29) | (0x101 << 16) | 0xF,
        CodeWarnEnd         = (2 << 30) | (1 << 29) | (0x7FF << 16) | 0xF,

        // 错误类错误码（最高位 31+30 = 1）
        CodeErrorBegin       = (3 << 30) | (1 << 29) | (0x100 << 16),
        CodeErrorNetBegin    = (3 << 30) | (1 << 29) | (0x101 << 16),
        CodeErrorNetUdpAllocBuff         = CodeErrorNetBegin + 1,  // UDP 缓冲区分配失败
        CodeErrorNetUdpAckTimeOutRetry   = CodeErrorNetBegin + 2,  // UDP ACK 超时重试
        CodeErrorNetParseMessage         = CodeErrorNetBegin + 3,  // 消息解析失败
        CodeErrorNetErrorSocket          = CodeErrorNetBegin + 4,  // Socket 创建/操作失败
        CodeErrorNetErrorCompletionPort  = CodeErrorNetBegin + 5,  // IOCP 完成端口失败
        CodeErrorNetErrorEpollHandle     = CodeErrorNetBegin + 6,  // epoll 句柄失败
        CodeErrorNetSessionAlreadyConnected = CodeErrorNetBegin + 7, // Session 已连接
        CodeErrorNetEnd      = (3 << 30) | (1 << 29) | (0x101 << 16) | 0xF,
        CodeErrorEnd         = (3 << 30) | (1 << 29) | (0x7FF << 16) | 0xF,
    }

    /// <summary>
    /// 统一错误码类，支持隐式转换为 bool（是否有错误）和 int（错误码值）。
    /// Code == 0 表示无错误，非零表示有错误。
    /// </summary>
    public class ErrorCode
    {
        public int Code { get; set; }       // 错误码数值
        public string Message { get; set; }   // 错误描述信息

        public ErrorCode() : this(0, "") { }
        public ErrorCode(int code) : this(code, "") { }
        public ErrorCode(int code, string what)
        {
            Code = code;
            Message = what;
        }

        /// <summary>设置错误码和描述信息</summary>
        public ErrorCode Set(int code, string what)
        {
            Code = code;
            Message = what;
            return this;
        }

        /// <summary>从另一个 ErrorCode 复制</summary>
        public ErrorCode Set(ErrorCode other)
        {
            Code = other.Code;
            Message = other.Message;
            return this;
        }

        /// <summary>获取格式化的错误描述</summary>
        public string GetWhat()
        {
            return $"{Message},error:{Code}";
        }

        /// <summary>隐式转换为 bool：Code != 0 时为 true（表示有错误）</summary>
        public static implicit operator bool(ErrorCode e) => e.Code != 0;
        /// <summary>隐式转换为 int：返回原始错误码数值</summary>
        public static implicit operator int(ErrorCode e) => e.Code;
    }
}
