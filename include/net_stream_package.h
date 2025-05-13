#ifndef __NetStreamPackage_H__
#define __NetStreamPackage_H__

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif  // _WIN32

#include <string>
#include <string.h>
#include <assert.h>

namespace FXNET
{
	// read write 不能同时使用
	class CNetStreamPackage
	{
	public: 
		CNetStreamPackage(const char *pData, unsigned int dwLen)
			: m_strData(pData, dwLen)
		{
		}

		CNetStreamPackage()
		{
			m_strData.reserve(2 * 1024);
		}

		~CNetStreamPackage() {}

		unsigned int GetDataLength() const { return (unsigned int)m_strData.size(); }
		const char* GetData() const { return m_strData.data(); }

		bool ReadByte(char &cData)
		{
			if (m_strData.size() < sizeof(cData))
			{
				return false;
			}
			cData = m_strData[0];
			m_strData.erase(0, sizeof(cData));
			return true;
		}

		bool ReadByte(unsigned char &cData)
		{
			if (m_strData.size() < sizeof(cData))
			{
				return false;
			}
			cData = m_strData[0];
			m_strData.erase(0, sizeof(cData));
			return true;
		}

		bool ReadShort(short &wData)
		{
			if (m_strData.size() < sizeof(wData))
			{
				return false;
			}
			wData = *(short *)m_strData.data();
			m_strData.erase(0, sizeof(wData));
			wData = ntohs(wData);
			return true;
		}

		bool ReadShort(unsigned short &wData)
		{
			if (m_strData.size() < sizeof(wData))
			{
				return false;
			}
			wData = *(unsigned short *)m_strData.data();
			m_strData.erase(0, sizeof(wData));
			wData = ntohs(wData);
			return true;
		}

		bool ReadInt(int &dwData)
		{
			if (m_strData.size() < sizeof(dwData))
			{
				return false;
			}
			dwData = *(int *)m_strData.data();
			m_strData.erase(0, sizeof(dwData));
			dwData = ntohl(dwData);
			return true;
		}

		bool ReadInt(unsigned int &dwData)
		{
			if (m_strData.size() < sizeof(dwData))
			{
				return false;
			}
			dwData = *(unsigned int *)m_strData.data();
			m_strData.erase(0, sizeof(dwData));
			dwData = ntohl(dwData);
			return true;
		}

		bool ReadInt64(long long &llData)
		{
			if (m_strData.size() < sizeof(llData))
			{
				return false;
			}
			llData = *(long long *)m_strData.data();
			m_strData.erase(0, sizeof(llData));
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte           = 0x0102;
			static const ByteOrder &oByteOrder = (ByteOrder &)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				llData = (long long)htonl((int)(llData >> 32)) | ((long long)htonl((int)llData) << 32);
			}
			return true;
		}

		bool ReadInt64(unsigned long long &ullData)
		{
			if (m_strData.size() < sizeof(ullData))
			{
				return false;
			}
			ullData = *(unsigned long long *)m_strData.data();
			m_strData.erase(0, sizeof(ullData));
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte           = 0x0102;
			static const ByteOrder &oByteOrder = (ByteOrder &)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				ullData = (long long)htonl((int)(ullData >> 32)) | ((long long)htonl((int)ullData) << 32);
			}
			return true;
		}

		bool ReadFloat(float &fData)
		{
			int dwFloat = 0;
			if (!ReadInt(dwFloat))
			{
				return false;
			}
			fData = dwFloat / 256.0f;
			return true;
		}

		bool ReadString(char *pStr, unsigned int dwLen)
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
			memcpy(pStr, m_strData.data(), dwStrLen);
			m_strData.erase(0, dwStrLen);
			return true;
		}

		bool ReadString(std::string &refStr)
		{
			unsigned int dwStrLen = 0;
			if (!ReadInt(dwStrLen))
			{
				return false;
			}
			refStr.append(m_strData.data(), dwStrLen);
			m_strData.erase(0, dwStrLen);
			return true;
		}

		bool ReadData(char *pData, unsigned int dwLen)
		{
			if (dwLen < m_strData.size())
			{
				return false;
			}
			memcpy(pData, m_strData.data(), dwLen);
			m_strData.erase(0, dwLen);
			return true;
		}

		void WriteData(char *pData, size_t dwLen)
		{
			m_strData.append(pData, dwLen);
		}

		void WriteData(const char *pData, size_t dwLen)
		{
			m_strData.append(pData, dwLen);
		}

		void WriteByte(char cData)
		{
			WriteData(&cData, sizeof(cData));
		}

		void WriteByte(unsigned char cData)
		{
			WriteData((char *)(&cData), sizeof(cData));
		}

		void WriteShort(short wData)
		{
			wData = htons(wData);
			WriteData((char *)(&wData), sizeof(wData));
		}

		void WriteShort(unsigned short wData)
		{
			wData = htons(wData);
			return WriteData((char *)(&wData), sizeof(wData));
		}

		void WriteInt(int dwData)
		{
			dwData = htonl(dwData);
			return WriteData((char *)(&dwData), sizeof(dwData));
		}

		void WriteInt(unsigned int dwData)
		{
			dwData = htonl(dwData);
			return WriteData((char *)(&dwData), sizeof(dwData));
		}

		void WriteInt64(long long llData)
		{
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte           = 0x0102;
			static const ByteOrder &oByteOrder = (ByteOrder &)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				llData = (long long)htonl((int)(llData >> 32)) | ((long long)htonl((int)llData) << 32);
			}
			return WriteData((char *)(&llData), sizeof(llData));
		}

		void WriteInt64(unsigned long long ullData)
		{
			union ByteOrder
			{
				unsigned short wByte;
				unsigned char ucByte[2];
			};
			static const short wByte           = 0x0102;
			static const ByteOrder &oByteOrder = (ByteOrder &)wByte;
			if (oByteOrder.ucByte[0] != 0x01)
			{
				// 小端
				ullData = (unsigned long long)htonl((int)(ullData >> 32)) | ((unsigned long long)htonl((int)ullData) << 32);
			}
			return WriteData((char *)(&ullData), sizeof(ullData));
		}

		void WriteFloat(float fData)
		{
			int nData = (int)(fData * 256);
			return WriteInt(nData);
		}

		void WriteString(std::string refStr)
		{
			WriteInt((unsigned int)refStr.size());
			WriteData(refStr.c_str(), refStr.size());
		}

		void WriteString(const char *pData, unsigned int dwLen)
		{
			WriteInt(dwLen);
			WriteData(pData, dwLen);
		}

	private: 
		std::string m_strData;
	};
};

#endif  //	__NetStreamPackage_H__
