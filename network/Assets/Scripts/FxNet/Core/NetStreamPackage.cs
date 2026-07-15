using System;
using System.Buffers.Binary;
using System.Text;

namespace FxNet.Core
{
    /// <summary>
    /// 网络流数据包，支持网络字节序（大端）的序列化/反序列化。
    /// 内部维护一个可自动扩容的字节缓冲区，支持顺序读写。
    /// 浮点数采用定点数表示（value * 256 转 int）。
    /// </summary>
    public class NetStreamPackage
    {
        private byte[] _data;    // 数据缓冲区
        private int _offset;     // 当前读偏移
        private int _length;     // 当前有效数据长度

        public NetStreamPackage()
        {
            _data = new byte[2 * 1024]; // 初始 2KB 缓冲区
            _offset = 0;
            _length = 0;
        }

        /// <summary>从已有数据构造数据包</summary>
        public NetStreamPackage(byte[] data, int len)
        {
            _data = new byte[len];
            Array.Copy(data, _data, len);
            _offset = 0;
            _length = len;
        }

        public int DataLength => _length;  // 当前可读数据长度
        public byte[] GetData() => _data;  // 获取内部缓冲区
        public int GetOffset() => _offset; // 获取当前读偏移

        /// <summary>确保缓冲区至少有 size 字节可读数据，不足时扩容</summary>
        private void EnsureData(int size)
        {
            if (_length >= size) return;
            int newSize = _data.Length * 2;
            while (newSize < size) newSize *= 2;
            var newData = new byte[newSize];
            Array.Copy(_data, _offset, newData, 0, _length);
            _data = newData;
            _offset = 0;
        }

        /// <summary>消费已读数据，必要时前移或压缩缓冲区</summary>
        private void Consume(int size)
        {
            _offset += size;
            _length -= size;
            if (_length == 0)
            {
                _offset = 0;
            }
            else if (_offset > _data.Length / 2)
            {
                Array.Copy(_data, _offset, _data, 0, _length);
                _offset = 0;
            }
        }

        // === 读取方法（网络字节序/大端） ===

        public bool ReadByte(out sbyte value)
        {
            value = 0;
            if (_length < 1) return false;
            value = (sbyte)_data[_offset];
            Consume(1);
            return true;
        }

        public bool ReadByte(out byte value)
        {
            value = 0;
            if (_length < 1) return false;
            value = _data[_offset];
            Consume(1);
            return true;
        }

        public bool ReadShort(out short value)
        {
            value = 0;
            if (_length < 2) return false;
            value = BinaryPrimitives.ReadInt16BigEndian(_data.AsSpan(_offset));
            Consume(2);
            return true;
        }

        public bool ReadShort(out ushort value)
        {
            value = 0;
            if (_length < 2) return false;
            value = BinaryPrimitives.ReadUInt16BigEndian(_data.AsSpan(_offset));
            Consume(2);
            return true;
        }

        public bool ReadInt(out int value)
        {
            value = 0;
            if (_length < 4) return false;
            value = BinaryPrimitives.ReadInt32BigEndian(_data.AsSpan(_offset));
            Consume(4);
            return true;
        }

        public bool ReadInt(out uint value)
        {
            value = 0;
            if (_length < 4) return false;
            value = BinaryPrimitives.ReadUInt32BigEndian(_data.AsSpan(_offset));
            Consume(4);
            return true;
        }

        public bool ReadInt64(out long value)
        {
            value = 0;
            if (_length < 8) return false;
            value = BinaryPrimitives.ReadInt64BigEndian(_data.AsSpan(_offset));
            Consume(8);
            return true;
        }

        public bool ReadInt64(out ulong value)
        {
            value = 0;
            if (_length < 8) return false;
            value = BinaryPrimitives.ReadUInt64BigEndian(_data.AsSpan(_offset));
            Consume(8);
            return true;
        }

        /// <summary>读取浮点数（定点数，精度 1/256）</summary>
        public bool ReadFloat(out float value)
        {
            value = 0;
            if (!ReadInt(out int intVal)) return false;
            value = intVal / 256.0f; // 定点数转浮点
            return true;
        }

        /// <summary>读取字符串：先读 4 字节长度，再读 UTF-8 数据</summary>
        public bool ReadString(out string value, int maxLen = int.MaxValue)
        {
            value = "";
            if (!ReadInt(out int strLen)) return false;
            if (strLen > _length || strLen > maxLen) return false;
            value = Encoding.UTF8.GetString(_data, _offset, strLen);
            Consume(strLen);
            return true;
        }

        /// <summary>读取原始数据到指定缓冲区</summary>
        public bool ReadData(byte[] buffer, int len)
        {
            if (_length < len) return false;
            Array.Copy(_data, _offset, buffer, 0, len);
            Consume(len);
            return true;
        }

        // === 写入方法（网络字节序/大端） ===

        public void WriteByte(byte value)
        {
            EnsureCapacity(1);
            _data[_offset + _length] = value;
            _length += 1;
        }

        public void WriteByte(sbyte value)
        {
            EnsureCapacity(1);
            _data[_offset + _length] = (byte)value;
            _length += 1;
        }

        public void WriteShort(short value)
        {
            EnsureCapacity(2);
            BinaryPrimitives.WriteInt16BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 2;
        }

        public void WriteShort(ushort value)
        {
            EnsureCapacity(2);
            BinaryPrimitives.WriteUInt16BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 2;
        }

        public void WriteInt(int value)
        {
            EnsureCapacity(4);
            BinaryPrimitives.WriteInt32BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 4;
        }

        public void WriteInt(uint value)
        {
            EnsureCapacity(4);
            BinaryPrimitives.WriteUInt32BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 4;
        }

        public void WriteInt64(long value)
        {
            EnsureCapacity(8);
            BinaryPrimitives.WriteInt64BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 8;
        }

        public void WriteInt64(ulong value)
        {
            EnsureCapacity(8);
            BinaryPrimitives.WriteUInt64BigEndian(_data.AsSpan(_offset + _length), value);
            _length += 8;
        }

        /// <summary>写入浮点数（定点数，精度 1/256）</summary>
        public void WriteFloat(float value)
        {
            WriteInt((int)(value * 256)); // 浮点转定点数
        }

        /// <summary>写入字符串：先写 4 字节长度，再写 UTF-8 数据</summary>
        public void WriteString(string value)
        {
            var bytes = Encoding.UTF8.GetBytes(value);
            WriteInt(bytes.Length);
            WriteData(bytes, bytes.Length);
        }

        /// <summary>写入原始数据</summary>
        public void WriteData(byte[] data, int len)
        {
            EnsureCapacity(len);
            Array.Copy(data, 0, _data, _offset + _length, len);
            _length += len;
        }

        /// <summary>从指定偏移写入原始数据</summary>
        public void WriteData(byte[] data, int srcOffset, int len)
        {
            EnsureCapacity(len);
            Array.Copy(data, srcOffset, _data, _offset + _length, len);
            _length += len;
        }

        /// <summary>确保缓冲区有足够空间，必要时自动扩容</summary>
        private void EnsureCapacity(int additionalBytes)
        {
            int required = _offset + _length + additionalBytes;
            if (required <= _data.Length) return;

            int newSize = _data.Length * 2;
            while (newSize < required) newSize *= 2;

            var newData = new byte[newSize];
            Array.Copy(_data, _offset, newData, 0, _length);
            _data = newData;
            _offset = 0;
        }
    }
}
