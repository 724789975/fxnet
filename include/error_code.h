#ifndef __ERROR_CODE_H__
#define __ERROR_CODE_H__

enum UserError
{
	//�豸 ģ��(������ ����������������)
	FACILITY_BEGIN = 0x100,
	FACILITY_NET,
	FACILITY_END = 0x7FF,

	//����
	CODE_SUCCESS_BEGIN = 1 << 29 | FACILITY_BEGIN << 16,
	//�޻���ɶ�
	CODE_SUCCESS_NO_BUFF_READ = 1 << 29 | FACILITY_NET << 16,
	CODE_SUCCESS_NET_EOF,

	CODE_SUCCESS_END = 1 << 29 | FACILITY_END << 16 | 15,

	//��Ϣ
	CODE_INFO_BEGIN = 1 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_INFO_END = 1 << 30 | 1 << 29 | FACILITY_END << 16 | 15,

	//����
	CODE_WARN_BEGIN = 2 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_WARN_END = 2 << 30 | 1 << 29 | FACILITY_END << 16 | 15,

	//����
	CODE_ERROR_BEGIN = 3 << 30 | 1 << 29 | FACILITY_BEGIN << 16,
	CODE_ERROR_NET_UDP_ALLOC_BUFF = 3 << 30 | 1 << 29 | FACILITY_NET << 16,
	CODE_ERROR_END = 3 << 30 | 1 << 29 | FACILITY_END << 16 | 15,
};

#endif // !__ERROR_CODE_H__
