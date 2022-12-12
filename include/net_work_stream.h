#ifndef __NET_WORK_STREAM_H__
#define __NET_WORK_STREAM_H__

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
	virtual void PopData(std::string &refData) = 0;
	void* PushData(const char *szData, unsigned int wLen);
	void* PushData(unsigned int wLen);
	virtual bool CheckPackage() = 0;

private:
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
	TextWorkStream() {}

	TextWorkStream &WriteString(const char *szData, unsigned int dwLen);
	TextWorkStream &ReadString(const char *szData, unsigned int dwLen);
	TextWorkStream &ReadString(std::string &strBuff);
	virtual void PopData(std::string &refData);

	virtual bool CheckPackage();

protected:
private:
};

#endif // !__NET_WORK_STREAM_H__
