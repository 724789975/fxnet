using System.Buffers.Binary;

namespace FxNet.Core
{
    /// <summary>
    /// 网络流缓冲区抽象基类。
    /// 提供数据推入/弹出、自动扩容等基本功能。
    /// 子类需实现粘包处理逻辑（CheckPackage）。
    /// </summary>
    public abstract class NetWorkStream
    {
        protected byte[] Data;    // 数据缓冲区
        protected int UsedLength;     // 已用长度
        protected int DataLength;    // 缓冲区总容量

        public NetWorkStream(int initialCapacity = 4096)
        {
            Data = new byte[initialCapacity];
            UsedLength = 0;
            DataLength = initialCapacity;
        }

        public byte[] GetData() => Data;
        public virtual int GetSize() => UsedLength;   // 已用数据长度
        public int GetFreeSize() => DataLength - GetSize(); // 剩余可用空间

        /// <summary>从头部弹出 len 字节（前移数据）</summary>
        public void PopData(int len)
        {
            if (len >= UsedLength)
            {
                UsedLength = 0;
            }
            else
            {
                Array.Copy(Data, len, Data, 0, UsedLength - len);
                UsedLength -= len;
            }
        }

        public virtual void PopData(NetStreamPackage package) { }

        /// <summary>将数据推入缓冲区尾部</summary>
        public void PushData(byte[] data, int len)
        {
            Realloc(len);
            Array.Copy(data, 0, Data, UsedLength, len);
            UsedLength += len;
        }

        /// <summary>预留 len 字节空间，返回写入起始位置</summary>
        public int PushData(int len)
        {
            Realloc(len);
            int oldLen = UsedLength;
            UsedLength += len;
            return oldLen;
        }

        public virtual void PushData(NetStreamPackage package) { }

        /// <summary>检查缓冲区中是否有完整的消息包（粘包处理）</summary>
        public virtual bool CheckPackage() => false;

        /// <summary>缓冲区自动扩容（2 倍增长）</summary>
        protected void Realloc(int len)
        {
            if (UsedLength + len <= DataLength) return;
            int newSize = DataLength * 2;
            while (newSize < UsedLength + len) newSize *= 2;
            var newData = new byte[newSize];
            Array.Copy(Data, newData, UsedLength);
            Data = newData;
            DataLength = newSize;
        }
    }

    /// <summary>
    /// TCP 文本协议流处理（4字节大端长度头 + 数据体）。
    /// 用于解决 TCP 粘包/拆包问题。
    /// </summary>
    public class TextWorkStream : NetWorkStream
    {
        public const int HeaderLength = 4; // 消息头长度（4字节 int32 大端）

        public TextWorkStream() : base() { }

        /// <summary>从缓冲区解析一个完整包写入 package（先读长度头再拷贝数据）</summary>
        public override void PopData(NetStreamPackage package)
        {
            if (UsedLength < HeaderLength) return;
            int pkgLen = BinaryPrimitives.ReadInt32BigEndian(Data);
            if (pkgLen < 0 || pkgLen > 1024 * 1024 || UsedLength < HeaderLength + pkgLen) return;

            package.WriteData(Data, HeaderLength, pkgLen);
            PopData(HeaderLength + pkgLen);
        }

        public override void PushData(NetStreamPackage package)
        {
            var data = package.GetData();
            int len = package.DataLength;
            PushData(data, len);
        }

        /// <summary>检查缓冲区是否有完整包：先读 4 字节长度头，再判断数据是否足够</summary>
        public override bool CheckPackage()
        {
            if (UsedLength < HeaderLength) return false;
            int pkgLen = BinaryPrimitives.ReadInt32BigEndian(Data);
            return UsedLength >= HeaderLength + pkgLen;
        }
    }

    /// <summary>
    /// WebSocket 协议流处理，通过自定义头部检查接口支持灵活的消息边界判定
    /// </summary>
    public class WSWorkStream : NetWorkStream
    {
        /// <summary>自定义头部检查接口</summary>
        public interface IHeaderCheck
        {
            bool Check();
        }

        private readonly IHeaderCheck _headerCheck;

        public WSWorkStream(IHeaderCheck headerCheck) : base()
        {
            _headerCheck = headerCheck;
        }

        public override void PopData(NetStreamPackage package)
        {
            var data = package.GetData();
            int len = package.DataLength;
            PushData(data, len);
        }

        public override void PushData(NetStreamPackage package)
        {
            var data = package.GetData();
            int len = package.DataLength;
            PushData(data, len);
        }

        public override bool CheckPackage()
        {
            return _headerCheck.Check();
        }
    }
}
