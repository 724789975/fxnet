#ifndef __NET_WORK_STREAM_H__
#define __NET_WORK_STREAM_H__

#include <string>

class INetWorkStream
{
public:
	enum { BUFF_SIZE = 64 * 1024 - 1, };
	INetWorkStream() : m_wUseLen(0) {}
	virtual ~INetWorkStream() {}
	unsigned char* GetData() { return m_btData; }
	virtual unsigned short GetSize() { return m_wUseLen; }
	unsigned short GetFreeSize() { return BUFF_SIZE - GetSize(); }

	void PopData(unsigned short wLen);
	virtual void PopData(std::string& refData) = 0;
	void PushData(const char* szData, unsigned short wLen);
	void PushData(unsigned short wLen);
	virtual bool CheckPackage() = 0;
protected:
	unsigned char m_btData[BUFF_SIZE];
	unsigned short m_wUseLen;
private:
};

class TextWorkStream : public INetWorkStream
{
public:
	TextWorkStream() {}

	TextWorkStream& WriteString(const char* szData, unsigned short wLen);
	TextWorkStream& ReadString(const char* szData, unsigned short wLen);
	TextWorkStream& ReadString(std::string& strBuff);
	virtual void PopData(std::string& refData);

	virtual bool CheckPackage();
protected:
private:
};


#endif // !__NET_WORK_STREAM_H__