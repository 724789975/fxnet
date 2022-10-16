#include "include/net_work_stream.h"
#include "include/netstream.h"

void INetWorkStream::PopData(unsigned short wLen)
{
	assert(wLen <= m_wUseLen);
	memmove(m_btData, m_btData + wLen, m_wUseLen - wLen);
	m_wUseLen -= wLen;
}

void INetWorkStream::PushData(const char* szData, unsigned short wLen)
{
	assert(wLen <= GetFreeSize());
	memcpy(m_btData + GetSize(), szData, wLen);
	m_wUseLen += wLen;
}

void INetWorkStream::PushData(unsigned short wLen)
{
	assert((int)m_wUseLen + wLen < BUFF_SIZE);
	m_wUseLen += wLen;
}

TextWorkStream& TextWorkStream::WriteString(const char* szData, unsigned short wLen)
{
	FXNET::CNetStream oNetStream((char*)m_btData + GetSize(), GetFreeSize());
	oNetStream.WriteShort((unsigned short)(wLen + sizeof(wLen)));
	oNetStream.WriteData(szData, wLen);
	m_wUseLen += (wLen + sizeof(wLen));
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(const char* szData, unsigned short wLen)
{
	FXNET::CNetStream oNetStream((char*)m_btData, GetSize());
	unsigned short wReadLen = 0;
	oNetStream.ReadShort(wReadLen);
	memcpy((char*)szData, oNetStream.ReadData(wReadLen), wReadLen - sizeof(short));
	m_wUseLen -= wReadLen;
	memmove(m_btData, m_btData + wReadLen, m_wUseLen);
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(std::string& strBuff)
{
	FXNET::CNetStream oNetStream((char*)m_btData, GetSize());
	unsigned short wReadLen = 0;
	oNetStream.ReadShort(wReadLen);
	oNetStream.ReadString(strBuff, wReadLen - sizeof(short));
	m_wUseLen -= wReadLen;
	memmove(m_btData, m_btData + wReadLen, m_wUseLen);

	return *this;
}

void TextWorkStream::PopData(std::string& refData)
{
	ReadString(refData);
}

bool TextWorkStream::CheckPackage()
{
	FXNET::CNetStream oNetStream((char*)m_btData, GetSize());
	unsigned short wReadLen = 0;
	if (!oNetStream.ReadShort(wReadLen))
	{
		return false;
	}

	if (wReadLen > GetSize())
	{
		return false;
	}

	return true;
}

