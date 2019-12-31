#pragma once

#include <Windows.h>

class CDecodeProc {
public:
	enum class enumDecodeWaitStatus : DWORD {
		eDecodeWaitFailed = WAIT_FAILED,
		eDecodeWaitTerminateRequest = WAIT_OBJECT_0,
		eDecodeWaitRecvEvent = WAIT_OBJECT_0 + 1,
		eDecodeWaitTimeout = WAIT_TIMEOUT,
	};

public:
	CDecodeProc(void);
	virtual ~CDecodeProc(void);
	HANDLE CreateThread(LPTHREAD_START_ROUTINE proc, LPVOID param, int priority);
	void TerminateThread(void);
	void NotifyThreadStarted(void);
	enumDecodeWaitStatus WaitRequest(DWORD msec, HANDLE hRecvEvent);
private:
	HANDLE hThread	= NULL;				// �X���b�h�n���h��
	HANDLE hThreadInitComp = NULL;		// �X���b�h�����������ʒm
	HANDLE hTerminateRequest = NULL;	// �X���b�h�I���v��
};
