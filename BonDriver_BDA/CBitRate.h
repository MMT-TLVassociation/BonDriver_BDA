#pragma once

#include <Windows.h>

// �r�b�g���[�g�v�Z�p
class CBitRate {
private:
	DWORD Rate1sec;					// 1�b�Ԃ̃��[�g���Z�p (bytes/sec)
	DWORD RateLast[5];				// ����5�b�Ԃ̃��[�g (bytes/sec)
	DWORD DataCount;				// ����5�b�Ԃ̃f�[�^�� (0�`5)
	double Rate;					// ���σr�b�g���[�g (Mibps)
	DWORD LastTick;					// �O���TickCount�l
	CRITICAL_SECTION csRate1Sec;	// nRate1sec �r���p
	CRITICAL_SECTION csRateLast;	// nRateLast �r���p

public:
	CBitRate(void);
	~CBitRate(void);
	void AddRate(DWORD Count);
	DWORD CheckRate(void);
	void Clear(void);
	double GetRate(void);
};
