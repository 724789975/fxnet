#include "udp_connector.h"

namespace FXNET
{
	ISocketBase& CUdpConnector::Update(double dTimedouble, std::ostream* pOStream)
	{
		// TODO: 在此处插入 return 语句
		return *this;
	}

	CUdpConnector::IOReadOperation& CUdpConnector::NewReadOperation()
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	IOOperationBase& CUdpConnector::NewWriteOperation()
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	IOOperationBase& CUdpConnector::NewErrorOperation(int dwError)
	{
		// TODO: 在此处插入 return 语句
		CUdpConnector::IOReadOperation t;
		return t;
	}

	void CUdpConnector::OnRead(std::ostream* refOStream)
	{
	}

	void CUdpConnector::OnWrite(std::ostream* pOStream)
	{
	}

	void CUdpConnector::OnError(std::ostream* pOStream)
	{
	}

	void CUdpConnector::IOReadOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* pOStream)
	{}

	void CUdpConnector::IOErrorOperation::operator()(ISocketBase& refSocketBase, unsigned int dwLen, std::ostream* refOStream)
	{
		delete this;
		return;
	}
};
