#ifndef __NET_WORK_STREAM_H__
#define __NET_WORK_STREAM_H__

#include "net_stream_package.h"
#include <string>

class INetWorkStream
{
public:
	INetWorkStream();
	virtual ~INetWorkStream();
	unsigned char *GetData() { return m_btData; }
	virtual unsigned int GetSize() { return m_dwUseLen; }
	unsigned int GetFreeSize() { return m_dwDataLen - GetSize(); }

	void PopData(unsigned int wLen);
	virtual void PopData(FXNET::CNetStreamPackage& refPackage) = 0;
	void* PushData(const char *szData, unsigned int wLen);
	void* PushData(unsigned int wLen);
	virtual void PushData(const FXNET::CNetStreamPackage& refPackage) = 0;
	virtual bool CheckPackage();

protected:
	void Realloc(unsigned int dwLen);

protected:
	unsigned char *m_btData;
	unsigned int m_dwUseLen;
	unsigned int m_dwDataLen;

private:
};

class TextWorkStream : public INetWorkStream
{
public:
	enum {HEADER_LENGTH = sizeof(unsigned int)};
	TextWorkStream() {}

	virtual void PopData(FXNET::CNetStreamPackage& refPackage);
	virtual void PushData(const FXNET::CNetStreamPackage& refPackage);
	virtual bool CheckPackage();

protected:
private:
};

class WSWorkStream : public INetWorkStream
{
public:
	class WSHeaderCheck
	{
	public:
		virtual bool operator ()() = 0;
	};
	WSWorkStream(WSHeaderCheck& oHeader) : m_oHeader(oHeader) {}

	virtual void PopData(FXNET::CNetStreamPackage& refPackage);
	virtual void PushData(const FXNET::CNetStreamPackage& refPackage);
	virtual bool CheckPackage();

protected:
private:
	WSHeaderCheck& m_oHeader;
};

#endif // !__NET_WORK_STREAM_H__
