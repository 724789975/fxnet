#ifndef __NetStream_H__
#define __NetStream_H__

#ifdef _WIN32
#include<Winsock2.h>
#else
#include <arpa/inet.h>
#endif // _WIN32

#include <string>
#include <string.h>
#include <assert.h>

namespace FXNET
{
	class CNetStream
	{
	public:
		CNetStream(char* pData, unsigned int dwLen)
			: m_pData(pData)
			, m_dwLen(dwLen)
		{
		}

		CNetStream(const char* pData, unsigned int dwLen)
			: m_pData(const_cast<char*>(pData))
			, m_dwLen(dwLen)
		{
		}
		~CNetStream() {}

		size_t GetDataLength() { return m_dwLen; }

		char* ReadData(unsigned int dwLen)
		{
			if (dwLen > m_dwLen)
			{
				return NULL;
			}
			char* pTemp = m_pData;
			m_pData += dwLen;
			m_dwLen -= dwLen;
			return pTemp;
		}

		bool ReadByte(char& cData)
		{
			char* pData = ReadData(sizeof(cData));
			if (!pData)
			{
				return false;
			}
			memcpy(&cData, pData, sizeof(cData));
			return true;
		}

		bool ReadByte(unsigned char& cData)
		{
			char* pData = ReadData(sizeof(cData));
			if (!pData)
			{
				return false;
			}
			memcpy(&cData, pData, sizeof(cData));
			return true;
		}

		bool ReadShort(short& wData)
		{
			char* pData = ReadData(sizeof(wData));
			if (!pData)
			{
				return false;
			}
			memcpy(&wData, pData, sizeof(wData));
			wData = ntohs(wData);
			return true;
		}

		bool ReadShort(unsigned short& wData)
		{
			char* pData = ReadData(sizeof(wData));
			if (!pData)
			{
				return false;
			}
			memcpy(&wData, pData, sizeof(wData));
			wData = ntohs(wData);
			return true;
		}

		bool ReadInt(int& dwData)
		{
			char* pData = ReadData(sizeof(dwData));
			if (!pData)
			{
				return false;
			}
			memcpy(&dwData, pData, sizeof(dwData));
			dwData = ntohl(dwData);
			return true;
		}

		bool ReadInt(unsigned int& dwData)
		{
			char* pData = ReadData(sizeof(dwData));
			if (!pData)
			{
				return false;
			}
			memcpy(&dwData, pData, sizeof(dwData));
			dwData = ntohl(dwData);
			return true;
		}

		bool ReadInt64(long long& llData)
		{
			char* pData = ReadData(sizeof(llData));
			if (!pData)
			{
				return false;
			}
			memcpy(&llData, pData, sizeof(llData));
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte = 0x0102;
			static const ByteOrder& oByteOrder = (ByteOrder&)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				llData = (long long)htonl((int)(llData >> 32)) | ((long long)htonl((int)llData) << 32);
			}
			return true;
		}

		bool ReadInt64(unsigned long long& ullData)
		{
			char* pData = ReadData(sizeof(ullData));
			if (!pData)
			{
				return false;
			}
			memcpy(&ullData, pData, sizeof(ullData));
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte = 0x0102;
			static const ByteOrder& oByteOrder = (ByteOrder&)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				ullData = (unsigned long long)htonl((int)(ullData >> 32)) | ((unsigned long long)htonl((int)ullData) << 32);
			}
			return true;
		}

		bool ReadFloat(float& fData)
		{
			int dwFloat = 0;
			if (!ReadInt(dwFloat))
			{
				return false;
			}
			fData = dwFloat / 256.0f;
			return true;
		}

		bool ReadString(char* pStr, unsigned int dwLen)
		{
			unsigned int dwStrLen = 0;
			if (!ReadInt(dwStrLen))
			{
				return false;
			}
			if (dwStrLen > dwLen)
			{
				return false;
			}
			char* pData = ReadData(dwStrLen);
			if (!pData)
			{
				return false;
			}
			memcpy(pStr, pData, dwStrLen);
			if (dwStrLen != std::string(pStr).size())
			{
				return false;
			}
			return true;
		}

		bool ReadString(std::string& refStr)
		{
			unsigned int dwStrLen = 0;
			if (!ReadInt(dwStrLen))
			{
				return false;
			}
			char* pData = ReadData(dwStrLen);
			if (!pData)
			{
				return false;
			}
			refStr = std::string(pData, dwStrLen);
			if (dwStrLen != refStr.size())
			{
				return false;
			}
			return true;
		}

		//  //
		const char* GetData(unsigned int dwLen)
		{
			if (dwLen <= m_dwLen)
			{
				return m_pData;
			}
			return NULL;
		}

		bool WriteData(char* pData, size_t dwLen)
		{
			if (dwLen > m_dwLen)
			{
				return false;
			}
			memcpy(m_pData, pData, dwLen);
			m_pData += dwLen;
			m_dwLen -= dwLen;
			return true;
		}

		bool WriteData(const char* pData, size_t dwLen)
		{
			if (dwLen > m_dwLen)
			{
				return false;
			}
			memcpy(m_pData, pData, dwLen);
			m_pData += dwLen;
			m_dwLen -= dwLen;
			return true;
		}

		bool WriteByte(char cData)
		{
			return WriteData(&cData, sizeof(cData));
		}

		bool WriteByte(unsigned char cData)
		{
			return WriteData((char*)(&cData), sizeof(cData));
		}

		bool WriteShort(short wData)
		{
			wData = htons(wData);
			return WriteData((char*)(&wData), sizeof(wData));
		}

		bool WriteShort(unsigned short wData)
		{
			wData = htons(wData);
			return WriteData((char*)(&wData), sizeof(wData));
		}

		bool WriteInt(int dwData)
		{
			dwData = htonl(dwData);
			return WriteData((char*)(&dwData), sizeof(dwData));
		}

		bool WriteInt(unsigned int dwData)
		{
			dwData = htonl(dwData);
			return WriteData((char*)(&dwData), sizeof(dwData));
		}

		bool WriteInt64(long long llData)
		{
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte = 0x0102;
			static const ByteOrder& oByteOrder = (ByteOrder&)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				llData = (long long)htonl((int)(llData >> 32)) | ((long long)htonl((int)llData) << 32);
			}
			return WriteData((char*)(&llData), sizeof(llData));
		}

		bool WriteInt64(unsigned long long ullData)
		{
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte = 0x0102;
			static const ByteOrder& oByteOrder = (ByteOrder&)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				ullData = (unsigned long long)htonl((int)(ullData >> 32)) | ((unsigned long long)htonl((int)ullData) << 32);
			}
			return WriteData((char*)(&ullData), sizeof(ullData));
		}

		bool WriteFloat(float fData)
		{
			int nData = (int)(fData * 256);
			return WriteInt(nData);
		}

		bool WriteString(std::string refStr)
		{
			if (!WriteInt((unsigned int)refStr.size())) return false;
			if (!WriteData(refStr.c_str(), refStr.size())) return false;
			return true;
		}

	private:
		char* m_pData;
		size_t m_dwLen;
	};
};


#endif	//	__NetStream_H__
