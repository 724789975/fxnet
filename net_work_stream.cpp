#include "include/net_work_stream.h"
#include "include/netstream.h"
#include "include/net_stream_package.h"

#ifdef _WIN32
#include <Winsock2.h>
#else
#include <arpa/inet.h>
#endif // _WIN32
#include <stdlib.h>

INetWorkStream::INetWorkStream()
	: m_dwUseLen(0)
{
	m_btData = (unsigned char *)malloc(32 * 1024);
	m_dwDataLen = 32 * 1024;
}

INetWorkStream::~INetWorkStream()
{
	free(m_btData);
}

void INetWorkStream::PopData(unsigned int dwLen)
{
	assert(dwLen <= this->m_dwUseLen);
	memmove(this->m_btData, this->m_btData + dwLen, this->m_dwUseLen - dwLen);
	this->m_dwUseLen -= dwLen;
}

void INetWorkStream::PopData(FXNET::CNetStreamPackage& refPackage)
{
	unsigned int dwLen = *(unsigned int*)(this->m_btData);
	dwLen = ntohl(dwLen);

	new (&refPackage) FXNET::CNetStreamPackage((char*)m_btData + HEADER_LENGTH, dwLen - HEADER_LENGTH);
	memmove(this->m_btData, this->m_btData + dwLen, this->m_dwUseLen - dwLen);
	m_dwUseLen -= dwLen;
}

void* INetWorkStream::PushData(const char* szData, unsigned int dwLen)
{
	this->Realloc(m_dwUseLen + dwLen);
	assert(dwLen <= this->GetFreeSize());
	memcpy(this->m_btData + this->GetSize(), szData, dwLen);
	this->m_dwUseLen += dwLen;
	return this->m_btData + this->GetSize() - dwLen;
}

void* INetWorkStream::PushData(unsigned int dwLen)
{
	this->Realloc(m_dwUseLen + dwLen);
	assert((int)this->m_dwUseLen + dwLen <= m_dwDataLen);
	this->m_dwUseLen += dwLen;
	return this->m_btData + this->GetSize() - dwLen;
}

void INetWorkStream::PushData(const FXNET::CNetStreamPackage& refPackage)
{
	this->Realloc(m_dwUseLen + refPackage.GetDataLength() + HEADER_LENGTH);
	unsigned int& refLen = *(unsigned int*)(this->m_btData + this->GetSize());
	this->m_dwUseLen += HEADER_LENGTH;
	refLen = refPackage.GetDataLength() + HEADER_LENGTH;
	refLen = htonl(refLen);

	memcpy(this->m_btData + this->GetSize(), refPackage.GetData(), refPackage.GetDataLength());
	this->m_dwUseLen += refPackage.GetDataLength();
}

bool INetWorkStream::CheckPackage()
{
	if (this->m_dwUseLen < HEADER_LENGTH)
	{
		return false;
	}
	
	unsigned int dwReadLen = *(unsigned int*)m_btData;
	dwReadLen = ntohl(dwReadLen);

	return dwReadLen <= m_dwUseLen;
}

void INetWorkStream::Realloc(unsigned int dwLen)
{
	if (dwLen > m_dwDataLen)
	{
		m_dwDataLen = (dwLen << 1) & (~0x3FF);
		m_btData = (unsigned char*)realloc(m_btData, m_dwDataLen);
	}
}

TextWorkStream& TextWorkStream::WriteString(const char* szData, unsigned int dwLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData + this->GetSize(), this->GetFreeSize());
	oNetStream.WriteInt((unsigned int)(dwLen + 2 * sizeof(dwLen)));
	oNetStream.WriteInt('T' << 24 | 'E' << 16 | 'S' << 8 | 'T');
	oNetStream.WriteData(szData, dwLen);
	this->m_dwUseLen += (dwLen + 2 * sizeof(dwLen)); 
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(const char* szData, unsigned int dwLen)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned int dwReadLen = 0;
	oNetStream.ReadInt(dwReadLen);
	unsigned int dwMagicNum = 0;
	oNetStream.ReadInt(dwMagicNum);
	memcpy((char*)szData, oNetStream.ReadData(dwReadLen), dwReadLen - 2 * sizeof(int));
	this->m_dwUseLen -= dwReadLen;
	memmove(this->m_btData, this->m_btData + dwReadLen, this->m_dwUseLen);
	return *this;
}

TextWorkStream& TextWorkStream::ReadString(std::string& strBuff)
{
	FXNET::CNetStream oNetStream((char*)this->m_btData, this->GetSize());
	unsigned int dwReadLen = 0;
	oNetStream.ReadInt(dwReadLen);
	unsigned int dwMagicNum = 0;
	oNetStream.ReadInt(dwMagicNum);
	oNetStream.ReadString(strBuff, dwReadLen - 2 * sizeof(int));
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

	// assert(dwReadLen <= this->GetSize());

	if (dwReadLen > this->GetSize())
	{
		return false;
	}

	unsigned int dwMagicNum = 0;
	oNetStream.ReadInt(dwMagicNum);

	assert(dwMagicNum == ('T' << 24 | 'E' << 16 | 'S' << 8 | 'T'));

	return true;
}

