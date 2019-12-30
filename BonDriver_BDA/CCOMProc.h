#pragma once

#include <Windows.h>

class CCOMProc {
public:
	enum class enumCOMRequest {
		eCOMReqNone = 0,
		eCOMReqOpenTuner,
		eCOMReqCloseTuner,
		eCOMReqSetChannel,
		eCOMReqGetSignalLevel,
		eCOMReqIsTunerOpening,
		eCOMReqGetCurSpace,
		eCOMReqGetCurChannel,
	};

	enum class enumCOMWaitStatus : DWORD {
		eCOMWaitFailed = WAIT_FAILED,
		eCOMWaitTerminateRequest = WAIT_OBJECT_0,
		eCOMWaitRequestEvent = WAIT_OBJECT_0 + 1,
		eCOMWaitTimeout = WAIT_TIMEOUT,
	};

	struct COMReqParamSetChannel {
		DWORD dwSpace;
		DWORD dwChannel;
	};

	union COMReqParm {
		COMReqParamSetChannel SetChannel;
	};

	union COMReqRetVal {
		BOOL OpenTuner;
		BOOL SetChannel;
		float GetSignalLevel;
		BOOL IsTunerOpening;
		DWORD GetCurSpace;
		DWORD GetCurChannel;
	};

	struct COMReqArgs {
		enumCOMRequest nRequest;	// ���N�G�X�g
		COMReqParm uParam;			// �p�����[�^
		COMReqRetVal uRetVal;		// �߂�l
		COMReqArgs(void)
			: nRequest(enumCOMRequest::eCOMReqNone),
			uParam(),
			uRetVal()
		{
		}
	};

public:
	CCOMProc(void);
	~CCOMProc(void);
	HANDLE CreateThread(LPTHREAD_START_ROUTINE proc, LPVOID param, int priority);
	void TerminateThread(void);
	void NotifyThreadStarted(void);
	BOOL RequestCOMReq(COMReqArgs* args);
	enumCOMWaitStatus WaitRequest(DWORD msec, enumCOMRequest* req, COMReqParm* param);
	void NotifyComplete(COMReqRetVal val);
	BOOL CheckTick(void);
	void ResetWatchDog(void);
	BOOL CheckSignalLockErr(BOOL state, DWORD threshold);
	BOOL CheckBitRateErr(BOOL state, DWORD threshold);
	void SetReLockChannel(void);
	void ResetReLockChannel(void);
	BOOL NeedReLockChannel(void);
	BOOL CheckReLockFailCount(unsigned int threshold);
	void SetReOpenTuner(DWORD space, DWORD channel);
	void ResetReOpenTuner(void);
	BOOL NeedReOpenTuner(void);
	DWORD GetReOpenSpace(void);
	DWORD GetReOpenChannel(void);
	void ClearReOpenChannel(void);
	BOOL CheckReOpenChannel(void);

	// �`���[�j���O�X�y�[�X�ԍ��s��
	static constexpr DWORD SPACE_INVALID = 0xFFFFFFFFUL;

	// �`�����l���ԍ��s��
	static constexpr DWORD CHANNEL_INVALID = 0xFFFFFFFFUL;

private:
	HANDLE hThread;					// �X���b�h�n���h��
	HANDLE hThreadInitComp;			// �X���b�h�����������ʒm
	HANDLE hReqEvent;				// COMProc�X���b�h�ւ̃R�}���h���s�v��
	HANDLE hEndEvent;				// COMProc�X���b�h����̃R�}���h�����ʒm
	CRITICAL_SECTION csLock;		// �r���p
	enumCOMRequest nRequest;		// ���N�G�X�g
	COMReqParm uParam;				// �p�����[�^
	COMReqRetVal uRetVal;			// �߂�l
	DWORD dwTick;					// ���݂�TickCount
	DWORD dwTickLastCheck;			// �Ō�Ɉُ�Ď��̊m�F���s����TickCount
	DWORD dwTickSignalLockErr;		// SignalLock�ُ̈픭��TickCount
	DWORD dwTickBitRateErr;			// BitRate�ُ̈픭��TckCount
	BOOL bSignalLockErr;			// SignalLock�ُ̈픭����Flag
	BOOL bBitRateErr;				// BitRate�ُ̈픭����Flag
	BOOL bDoReLockChannel;			// �`�����l�����b�N�Ď��s��
	BOOL bDoReOpenTuner;			// �`���[�i�[�ăI�[�v����
	unsigned int nReLockFailCount;	// Re-LockChannel���s��
	DWORD dwReOpenSpace;			// �`���[�i�[�ăI�[�v�����̃J�����g�`���[�j���O�X�y�[�X�ԍ��ޔ�
	DWORD dwReOpenChannel;			// �`���[�i�[�ăI�[�v�����̃J�����g�`�����l���ԍ��ޔ�
	HANDLE hTerminateRequest;		// �X���b�h�I���v��
};
