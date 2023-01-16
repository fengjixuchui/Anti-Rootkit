#pragma once
#include<ntifs.h>

extern "C" {
	NTKERNELAPI UCHAR* NTAPI PsGetProcessImageFileName(_In_ PEPROCESS Process);
	NTKERNELAPI HANDLE PsGetProcessInheritedFromUniqueProcessId(IN PEPROCESS Process);
	NTKERNELAPI PPEB PsGetProcessPeb(PEPROCESS Process);
	NTKERNELAPI NTSTATUS ZwOpenThread(
		_Out_  PHANDLE ThreadHandle,
		_In_   ACCESS_MASK DesiredAccess,
		_In_   POBJECT_ATTRIBUTES ObjectAttributes,
		_In_   PCLIENT_ID ClientId
	);
}


//�����߳�ID�����߳�ETHREAD��ʧ�ܷ���NULL
PETHREAD LookupThread(HANDLE Tid);

//ö��ָ�����̵��߳�
void EnumThread(PEPROCESS Process);

//ö��ָ�����̵�ģ��
void EnumModule(PEPROCESS Process);


//���淽����������
void ZwKillProcess(HANDLE Pid);

//���淽�������߳�
void ZwKillThread(HANDLE Tid);