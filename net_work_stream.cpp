#include "include/net_work_stream.h"
#include "include/netstream.h"

void INetWorkStream::PopData(unsigned int dwLen)
{
	assert(dwLen <= this->m_dwUseLen);
	memmove(this->m_btData, this->m_btData + dwLen, this->m_dwUseLen - dwLen);
	this->m_dwUseLen -= dwLen;
}

void INetWorkStream::PushData(const char* szData, unsigned int dwLen)
{
	assert(dwLen <= this->GetFreeSize());
	memcpy(this->m_btData + this->GetSize(), szData, dwLen);
	this->m_dwUseLen += dwLen;
}

void INetWorkStream::PushData(unsigned int dwLen)
{
	assert((int)this->m_dwUseLen + dwLen <= BUFF_SIZE);
	this->m_dwUseLen += dwLen;
}

TextWorkStream& TextWorkStream::WriteString(const char* szData, unsigned int dwLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData + this->GetSize(), this->GetFreeSize());
	oNetStream.WriteInt((unsigned int)(dwLen + sizeof(dwLen)));
	oNetStream.WriteData(szData, dwLen);
	this->m_dwUseLen += (dwLen + sizeof(dwLen));
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(const char* szData, unsigned int dwLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned int dwReadLen = 0;
	oNetStream.ReadInt(dwReadLen);
	memcpy((char*)szData, oNetStream.ReadData(dwReadLen), dwReadLen - sizeof(int));
	this->m_dwUseLen -= dwReadLen;
	memmove(this->m_btData, this->m_btData + dwReadLen, this->m_dwUseLen);
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(std::string& strBuff)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned int dwReadLen = 0;
	oNetStream.ReadInt(dwReadLen);
	oNetStream.ReadString(strBuff, dwReadLen - sizeof(int));
	this->m_dwUseLen -= dwReadLen;
	memmove(this->m_btData, this->m_btData + dwReadLen, this->m_dwUseLen);

	return *this;
}

void TextWorkStream::PopData(std::string& refData)
{
	this->ReadString(refData);
}

bool TextWorkStream::CheckPackage()
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned int dwReadLen = 0;
	if (!oNetStream.ReadInt(dwReadLen))
	{
		return false;
	}

	assert(dwReadLen);

	assert(dwReadLen <= INetWorkStream::BUFF_SIZE);

	if (dwReadLen > this->GetSize())
	{
		return false;
	}

	return true;
}

