#include "include/net_work_stream.h"
#include "include/netstream.h"

void INetWorkStream::PopData(unsigned short wLen)
{
	assert(wLen <= this->m_wUseLen);
	memmove(this->m_btData, this->m_btData + wLen, this->m_wUseLen - wLen);
	this->m_wUseLen -= wLen;
}

void INetWorkStream::PushData(const char* szData, unsigned short wLen)
{
	assert(wLen <= this->GetFreeSize());
	memcpy(this->m_btData + this->GetSize(), szData, wLen);
	this->m_wUseLen += wLen;
}

void INetWorkStream::PushData(unsigned short wLen)
{
	assert((int)this->m_wUseLen + wLen <= BUFF_SIZE);
	this->m_wUseLen += wLen;
}

TextWorkStream& TextWorkStream::WriteString(const char* szData, unsigned short wLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData + this->GetSize(), this->GetFreeSize());
	oNetStream.WriteShort((unsigned short)(wLen + sizeof(wLen)));
	oNetStream.WriteData(szData, wLen);
	this->m_wUseLen += (wLen + sizeof(wLen));
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(const char* szData, unsigned short wLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned short wReadLen = 0;
	oNetStream.ReadShort(wReadLen);
	memcpy((char*)szData, oNetStream.ReadData(wReadLen), wReadLen - sizeof(short));
	this->m_wUseLen -= wReadLen;
	memmove(this->m_btData, this->m_btData + wReadLen, this->m_wUseLen);
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(std::string& strBuff)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned short wReadLen = 0;
	oNetStream.ReadShort(wReadLen);
	oNetStream.ReadString(strBuff, wReadLen - sizeof(short));
	this->m_wUseLen -= wReadLen;
	memmove(this->m_btData, this->m_btData + wReadLen, this->m_wUseLen);

	return *this;
}

void TextWorkStream::PopData(std::string& refData)
{
	this->ReadString(refData);
}

bool TextWorkStream::CheckPackage()
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned short wReadLen = 0;
	if (!oNetStream.ReadShort(wReadLen))
	{
		return false;
	}

	if (wReadLen > this->GetSize())
	{
		return false;
	}

	return true;
}

