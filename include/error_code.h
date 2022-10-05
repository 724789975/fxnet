#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__

enum UserError
{
	////////////////////�豸 ģ��(������ ����������������)//////////////////////
	FACILITY_BEGIN = 0x100,
	FACILITY_NET,								//����ģ��
	FACILITY_END = 0x7FF,
	////////////////////�豸 ģ��(������ ����������������)//////////////////////

	////////////////////////////////////����//////////////////////////////////////
	CODE_SUCCESS_BEGIN = 1 << 29 | FACILITY_BEGIN << 16,
	CODE_SUCCESS_NO_BUFF_READ = 1 << 29 | FACILITY_NET << 16,		//�޻���ɶ�
	CODE_SUCCESS_NET_EOF,											//�Ͽ�����
	CODE_SUCCESS_END = 1 << 29 | FACILITY_END << 16 | 15,
	////////////////////////////////////����//////////////////////////////////////

	////////////////////////////////////��Ϣ//////////////////////////////////////
	CODE_INFO_BEGIN = 1 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_INFO_END = 1 << 30 | 1 << 29 | FACILITY_END << 16 | 15,
	////////////////////////////////////��Ϣ//////////////////////////////////////

	////////////////////////////////////����//////////////////////////////////////
	CODE_WARN_BEGIN = 2 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_WARN_END = 2 << 30 | 1 << 29 | FACILITY_END << 16 | 15,
	////////////////////////////////////����//////////////////////////////////////

	////////////////////////////////////����//////////////////////////////////////
	CODE_ERROR_BEGIN = 3 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_ERROR_NET_UDP_ALLOC_BUFF = 3 << 30 | 1 << 29 | FACILITY_NET << 16,			//UDP��������û��buff��
	CODE_ERROR_NET_UDP_ACK_TIME_OUT_RETRY,											//udp�ȴ�ack��ʱ��15s��
	CODE_ERROR_NET_PARSE_MESSAGE,													//��������
	CODE_ERROR_NET_ERROR_SOCKET,													//socket < 0
	CODE_ERROR_NET_ERROR_COMPLETION_PORT,											//CompletionPort < 0
	CODE_ERROR_NET_ERROR_EPOLL_HANDLE,												//epoll < 0
	CODE_ERROR_END = 3 << 30 | 1 << 29 | FACILITY_END << 16 | 15,
	////////////////////////////////////����//////////////////////////////////////
};

#endif // !__ERROR_CODE_H__
