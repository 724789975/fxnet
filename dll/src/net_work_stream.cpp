#include "../../include/net_work_stream.h"
#include "../../include/net_stream_package.h"

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


bool INetWorkStream::CheckPackage()
{
	return false;
}

void INetWorkStream::Realloc(unsigned int dwLen)
{
	if (dwLen >= m_dwDataLen)
	{
		m_dwDataLen = (dwLen << 1) & (~0x3FF);
		unsigned char* pbtTmp = (unsigned char*)realloc(m_btData, m_dwDataLen);
		assert(pbtTmp);
		if (pbtTmp) { m_btData = pbtTmp; }
	}
}

void TextWorkStream::PopData(FXNET::CNetStreamPackage& refPackage)
{
	unsigned int dwLen = *(unsigned int*)(this->m_btData);
	dwLen = ntohl(dwLen);

	refPackage.~CNetStreamPackage();
	new (&refPackage) FXNET::CNetStreamPackage((char*)m_btData + HEADER_LENGTH, dwLen - HEADER_LENGTH);
	this->INetWorkStream::PopData(dwLen);
}

void TextWorkStream::PushData(const FXNET::CNetStreamPackage& refPackage)
{
	this->Realloc(m_dwUseLen + refPackage.GetDataLength() + HEADER_LENGTH);
	unsigned int& refLen = *(unsigned int*)(this->m_btData + this->GetSize());
	this->m_dwUseLen += HEADER_LENGTH;
	refLen = refPackage.GetDataLength() + HEADER_LENGTH;
	refLen = htonl(refLen);

	memcpy(this->m_btData + this->GetSize(), refPackage.GetData(), refPackage.GetDataLength());
	this->m_dwUseLen += refPackage.GetDataLength();
}

bool TextWorkStream::CheckPackage()
{
	if (this->m_dwUseLen < HEADER_LENGTH)
	{
		return false;
	}
	
	unsigned int dwReadLen = *(unsigned int*)m_btData;
	dwReadLen = ntohl(dwReadLen);
	
	return dwReadLen <= m_dwUseLen;
	// return INetWorkStream::CheckPackage();
}

bool GetWSMsgLength(const char* szData, unsigned int dwLen, unsigned long long& qwPayloadLen, unsigned int& dwHeaderLength)
{
	FXNET::CNetStreamPackage oHeaderStream(szData, dwLen);
	
	dwHeaderLength = 0;
	unsigned char bt1       = 0, bt2 = 0;
	if (!oHeaderStream.ReadByte(bt1))
	{
		return false;
	}
	if (!oHeaderStream.ReadByte(bt2))
	{
		return false;
	}
	dwHeaderLength += 2;

	unsigned char btFin    = (bt1 >> 7) & 0xff;
	unsigned char btOpCode = (bt1) & 0x0f;
	unsigned char btMask   = (bt2 >> 7) & 0xff;
	qwPayloadLen  		   = bt2 & 0x7f;
	unsigned char btMaskingKey[4];

	if (qwPayloadLen == 126)
	{
		unsigned short wTemp = 0;
		if (!oHeaderStream.ReadShort(wTemp))
		{
			return false;
		}
		qwPayloadLen    = wTemp;
		dwHeaderLength += 2;
	}
	else if (qwPayloadLen == 127)
	{
		unsigned long long qwTemp = 0;
		if (!oHeaderStream.ReadInt64(qwTemp))
		{
			return false;
		}
		qwPayloadLen    = qwTemp;
		dwHeaderLength += 8;
	}
	
	if (btMask)
	{
		if (!oHeaderStream.ReadData((char*)btMaskingKey, sizeof(btMaskingKey)))
		{
			return false;
		}
		dwHeaderLength += 4;
	}
	return true;
}

void WSWorkStream::PopData(FXNET::CNetStreamPackage& refPackage)
{
	if (m_oHeader())
	{
		unsigned long long qwMsgLength = 0;
		unsigned int dwHeaderLength = 0;
		GetWSMsgLength((char*)this->GetData(), this->GetSize(), qwMsgLength, dwHeaderLength);
		refPackage.~CNetStreamPackage();
		new (&refPackage) FXNET::CNetStreamPackage((char*)m_btData + dwHeaderLength, qwMsgLength);
	
		this->INetWorkStream::PopData(dwHeaderLength + qwMsgLength);
		return;
	}

	refPackage.~CNetStreamPackage();
	new (&refPackage) FXNET::CNetStreamPackage((char*)m_btData, this->GetSize());

	this->INetWorkStream::PopData(this->GetSize());
}

void WSWorkStream::PushData(const FXNET::CNetStreamPackage& refPackage)
{
	FXNET::CNetStreamPackage oPackage;
	if (m_oHeader())
	{
		unsigned int dwHeaderLen = 1;
		unsigned char btFinOpCode = 0x82;
		oPackage.WriteByte(btFinOpCode);
		if (refPackage.GetDataLength() < 126 && refPackage.GetDataLength() <= 0xFFFF)
		{
			unsigned char btLen = refPackage.GetDataLength();
			oPackage.WriteByte(btLen);
			dwHeaderLen += 1;
		}
		else if (refPackage.GetDataLength() >= 126 && refPackage.GetDataLength() <= 0xFFFF)
		{
			unsigned char btLen = 126;
			oPackage.WriteByte(btLen);
			unsigned short wLen = refPackage.GetDataLength();
			oPackage.WriteShort(wLen);
			dwHeaderLen += 3;
		}
		else
		{
			unsigned char btLen = 127;
			oPackage.WriteByte(btLen);
			unsigned long long qwLen = refPackage.GetDataLength() ;
			oPackage.WriteInt64(qwLen);
			dwHeaderLen += 9;
		}
	}
	oPackage.WriteData(refPackage.GetData(), refPackage.GetDataLength());
	this->INetWorkStream::PushData(oPackage.GetData(), oPackage.GetDataLength());
}

bool WSWorkStream::CheckPackage()
{
	if (!m_oHeader())
	{
		return 4 <= m_dwUseLen;
	}
	
	unsigned long long qwMsgLength = 0;
	unsigned int dwHeaderLength = 0;
	if (!GetWSMsgLength((char*)this->GetData(), this->GetSize(), qwMsgLength, dwHeaderLength))
	{
		return false;
	}
	return (qwMsgLength + dwHeaderLength) <= m_dwUseLen;
}
