#ifndef __SOCKET_BASE_H__
#define __SOCKET_BASE_H__

#include "error_code.h"

#ifdef _WIN32
#include <WinSock2.h>
#include <Windows.h>
#include <MSWSock.h>
#include <Ws2tcpip.h>
#else
#include<netinet/in.h>
#include<arpa/inet.h>
#endif  // _WIN32

#include <ostream>
#include <iostream>
#include <string.h>

#ifdef _WIN32
#pragma comment(lib,"ws2_32.lib")
#endif  //_WIN32

namespace FXNET
{

	class IOOperationBase;

	class ISocketBase
	{
	public: 
#ifdef _WIN32
		typedef HANDLE NativeHandleType;
		typedef SOCKET NativeSocketType;
#else  //_WIN32
		typedef int NativeHandleType;
		typedef int NativeSocketType;
#endif  //_WIN32

		ISocketBase()
			: m_hNativeHandle((NativeHandleType)-1)
			, m_oError(0, "")
#ifndef _WIN32
			, m_bReadable(false)
			, m_bWritable(false)
#endif
		{
			memset(&m_stLocalAddr, 0, sizeof(m_stLocalAddr));
		}
		virtual ~ISocketBase() {}
		virtual const char* Name()const { return "CSocketBase"; }
		virtual ISocketBase& Update(double dTime, ErrorCode& refError, std::ostream* POStream) = 0;

		static NativeHandleType InvalidNativeHandle() { return (NativeHandleType)-1; };
		NativeHandleType& NativeHandle() { return this->m_hNativeHandle; }
		NativeSocketType& NativeSocket() { return (NativeSocketType&)this->m_hNativeHandle; }
		void SetIOModuleIndex(unsigned int dwIndex) { this->m_dwIOModuleIndex = dwIndex; }
		unsigned int GetIOModuleIndex() { return this->m_dwIOModuleIndex; }

		/**
		 * @brief Get the Local Addr object
		 * 
		 * @return sockaddr_in& 
		 */
		sockaddr_in& GetLocalAddr() { return m_stLocalAddr; }
		/**
		 * @brief Get the Error object
		 * 
		 * @return int 
		 */
		int GetError() { return m_oError; }

		/**
		 * @brief 
		 * 
		 * 关闭连接
		 * @param pOStream 
		 */
		virtual void Close(std::ostream* pOStream) = 0;

		/**
		 * @brief 
		 * 
		 * 创建读操作
		 * @return IOOperationBase& 
		 */
		virtual IOOperationBase& NewReadOperation() = 0;
		/**
		 * @brief 
		 * 
		 * 创建写操作
		 * @return IOOperationBase& 
		 */
		virtual IOOperationBase& NewWriteOperation() = 0;
		/**
		 * @brief 
		 * 
		 * 创建错误时操作
		 * @param dwError error code
		 * @return IOOperationBase& 
		 */
		virtual IOOperationBase& NewErrorOperation(const ErrorCode& oError) = 0;

		/**
		 * @brief 
		 * 
		 * @param dwError 
		 * @param pOStream 
		 */
		virtual void OnError(const ErrorCode& oError, std::ostream* pOStream) = 0;
		/**
		 * @brief 
		 * 
		 * @param pOStream 
		 */
		virtual void OnClose(std::ostream* pOStream) = 0;
	protected: 
		NativeHandleType m_hNativeHandle;
		sockaddr_in m_stLocalAddr;
		ErrorCode m_oError;
		unsigned int m_dwIOModuleIndex;

#ifdef _WIN32
#else
		//linux下et模式 需要表示是否刻度可写
		bool m_bReadable;
		bool m_bWritable;
#endif  // _WIN32
	private: 
	};

	class IOOperationBase
#ifdef _WIN32
		: public OVERLAPPED
#endif  // _WIN32
	{
	public: 
		IOOperationBase()
			: m_oError(0, "")
		{
#ifdef _WIN32
			memset((OVERLAPPED*)this, 0, sizeof(OVERLAPPED));
#endif  // _WIN32
		}
		virtual ~IOOperationBase() {}
		virtual IOOperationBase& operator()(ISocketBase& refSocketBase, unsigned int dwLen, ErrorCode& refError, std::ostream* pOStream) = 0;

		ErrorCode m_oError;
	protected:
	private:
	};
};

#endif  // !__SOCKET_BASE_H__


