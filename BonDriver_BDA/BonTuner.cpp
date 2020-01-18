// BonTuner.cpp: CBonTuner �N���X�̃C���v�������e�[�V����
//
//////////////////////////////////////////////////////////////////////

#include "common.h"

#include "BonTuner.h"

#include <Windows.h>
#include <string>
#include <regex>
#include <clocale>

#include <DShow.h>

#include "tswriter.h"

// KSCATEGORY_...
#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#include <bdamedia.h>

// bstr_t
#include <comdef.h>

#include "IBdaSpecials2.h"
#include "CIniFileAccess.h"
#include "WaitWithMsg.h"
#include "DSFilterEnum.h"

#include "TS_BUFF.h"
#include "TSMF.h"
#include "CBitRate.h"
#include "CCOMProc.h"
#include "CDecodeProc.h"

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#pragma comment(lib, "winmm.lib")

//////////////////////////////////////////////////////////////////////
// �ÓI�����o�ϐ�
//////////////////////////////////////////////////////////////////////

// Dll�̃��W���[���n���h��
HMODULE CBonTuner::st_hModule = NULL;

// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
std::list<CBonTuner*> CBonTuner::st_InstanceList;

// st_InstanceList����p
CRITICAL_SECTION CBonTuner::st_LockInstanceList;

// �K�v�ȐÓI�ϐ�������
void CBonTuner::Init(HMODULE hModule)
{
	st_hModule = hModule;
	::InitializeCriticalSection(&st_LockInstanceList);
	return;
}

// �ÓI�ϐ��̉��
void CBonTuner::Finalize(void)
{
	// ������̃C���X�^���X���c���Ă���Ή��
	for (auto it = st_InstanceList.begin(); it != st_InstanceList.end();) {
		SAFE_RELEASE(*it);
		it = st_InstanceList.erase(it);
	}

	::DeleteCriticalSection(&CBonTuner::st_LockInstanceList);

	// �f�o�b�O���O�t�@�C���̃N���[�Y
	CloseDebugLog();
	return;
}

//////////////////////////////////////////////////////////////////////
// �C���X�^���X�������\�b�h
//////////////////////////////////////////////////////////////////////
#pragma warning(disable : 4273)
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	return (IBonDriver *) new CBonTuner;
}
#pragma warning(default : 4273)

//////////////////////////////////////////////////////////////////////
// �\�z/����
//////////////////////////////////////////////////////////////////////
CBonTuner::CBonTuner()
{
	// �C���X�^���X���X�g�Ɏ��g��o�^
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.push_back(this);
	::LeaveCriticalSection(&st_LockInstanceList);

	std::setlocale(LC_CTYPE, "ja_JP.SJIS");

	// �v���Z�X�n���h���𕡐�
	HANDLE h = ::GetCurrentProcess();
	::DuplicateHandle(h, h, h, &m_hProcess, 0, FALSE, DUPLICATE_SAME_ACCESS);

	// ini�t�@�C���Ǎ�
	ReadIniFile();

	// TSMF�p�[�T�[�̃C���X�^���X�쐬
	if (m_bNeedTSMFParser) {
		m_pTSMFParser = new CTSMFParser();
	}

	// �r�b�g���[�g�v�Z�N���X�̃C���X�^���X�쐬
	if (m_bNeedBitRate) {
		m_pBitRate = new CBitRate();
	}

	// COM������p�X���b�h�N��
	m_pCOMProc = new CCOMProc();
	m_pCOMProc->CreateThread(CBonTuner::COMProcThread, this, m_nThreadPriorityCOM);

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	if (m_nPeriodicTimer != 0) {
		if (timeBeginPeriod(m_nPeriodicTimer) == TIMERR_NOCANDO) {
			m_nPeriodicTimer = 0;
		}
	}
}

CBonTuner::~CBonTuner()
{
	OutputDebug(L"~CBonTuner called.\n");
	CloseTuner();

	// timeBeginPeriod()�̌�n��
	if (m_nPeriodicTimer != 0) {
		timeEndPeriod(m_nPeriodicTimer);
	}

	// COM������p�X���b�h�I��
	if (m_pCOMProc) {
		m_pCOMProc->TerminateThread();
		SAFE_DELETE(m_pCOMProc);
	}

	SAFE_CLOSE_HANDLE(m_hStreamThread);
	SAFE_CLOSE_HANDLE(m_hProcess);

	// �C���X�^���X���X�g���玩�g���폜
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.remove(this);
	::LeaveCriticalSection(&st_LockInstanceList);
}

/////////////////////////////////////
//
// IBonDriver2 APIs
//
/////////////////////////////////////

const BOOL CBonTuner::OpenTuner(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqOpenTuner;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.OpenTuner;
	}
	return FALSE;
}

BOOL CBonTuner::_OpenTuner(void)
{
	if (m_bOpened)
		return TRUE;

	HRESULT hr;

	do {
		auto it = m_DVBSystemTypeDB.SystemType.find(m_nTargetDvbSystemTypeNum);
		if (it == m_DVBSystemTypeDB.SystemType.end()) {
			OutputDebug(L"Can not find DVBSystemType%ld.\n", m_nTargetDvbSystemTypeNum);
			return FALSE;
		}
		OutputDebug(L"Building DVBSystemType%ld\n", m_nTargetDvbSystemTypeNum);

		// �t�B���^�O���t�̍쐬
		if (FAILED(hr = InitializeGraphBuilder()))
			break;

		// �`���[�j���O�X�y�[�X�̓Ǎ�
		if (FAILED(hr = CreateTuningSpace(&it->second)))
			break;

		// �l�b�g���[�N�v���o�C�_
		if (FAILED(hr = LoadNetworkProvider(&it->second)))
			break;

		// �`���[�j���O�X�y�[�X������
		if (FAILED(hr = InitTuningSpace()))
			break;

		// �`���[�i�E�L���v�`���Ȍ�̍\�z�Ǝ��s
		if (FAILED(hr = LoadAndConnectDevice(it->second.nTunerGroup)))
			break;

		OutputDebug(L"Build graph Successfully.\n");

		// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X�̎擾�i���s���Ă����s�j
		if (m_bSignalLockedJudgeTypeSS || m_bSignalLevelGetTypeSS) {
			hr = LoadTunerSignalStatisticsTunerNode();
		}
		if (m_bSignalLockedJudgeTypeDemodSS || m_bSignalLevelGetTypeDemodSS) {
			hr = LoadTunerSignalStatisticsDemodNode();
		}

		// ��MTS�f�[�^�o�b�t�@������
		m_pTsBuff = new TS_BUFF(188 * m_nBuffSize, m_nMaxBuffCount);

		// TS��M�C�x���g�쐬
		m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		if (!m_bNeedDecodeProc) {
			m_ppGetTsBuff = &m_pTsBuff;
			m_phOnWaitTsEvent = &m_hOnStreamEvent;
		}
		else {
			// Decode�����̏I�����TS�f�[�^�o�b�t�@������
			m_pDecodedTsBuff = new TS_BUFF(0, m_nMaxBuffCount);

			// Decode�C�x���g�쐬
			m_hOnDecodeEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

			// Decode������p�X���b�h�N��
			m_pDecodeProc = new CDecodeProc();
			m_pDecodeProc->CreateThread(CBonTuner::DecodeProcThread, this, THREAD_PRIORITY_ERROR_RETURN);

			m_ppGetTsBuff = &m_pDecodedTsBuff;
			m_phOnWaitTsEvent = &m_hOnDecodeEvent;
		}

		// �R�[���o�b�N�֐��Z�b�g
		StartRecv();

		m_nCurrentDvbSystemTypeNum = m_nTargetDvbSystemTypeNum;
		m_bOpened = TRUE;

		return TRUE;

	} while(0);

	// �����ɓ��B�����Ƃ������Ƃ͉��炩�̃G���[�Ŏ��s����
	_CloseTuner(m_bTryAnotherTuner);

	return FALSE;
}

void CBonTuner::CloseTuner(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqCloseTuner;
	m_pCOMProc->RequestCOMReq(&arg);
	return;
}

void CBonTuner::_CloseTuner(BOOL putoff)
{
	m_bOpened = FALSE;

	// �O���t��~
	StopGraph();

	// �R�[���o�b�N�֐���~
	StopRecv();

	// Decode������p�X���b�h�I��
	if (m_pDecodeProc) {
		m_pDecodeProc->TerminateThread();
		SAFE_DELETE(m_pDecodeProc);
	}

	// Decode�C�x���g�J��
	SAFE_CLOSE_HANDLE(m_hOnDecodeEvent);

	// TS��M�C�x���g���
	SAFE_CLOSE_HANDLE(m_hOnStreamEvent);

	// �o�b�t�@���
	PurgeTsStream();
	SAFE_DELETE(m_LastBuff);

	// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X���
	UnloadTunerSignalStatistics();

	// �O���t���
	CleanupGraph();

	if (putoff) {
		// �g�p���Ă���Tuner��List�̍Ō���Ɉړ�����
		m_TunerComboList.PutOff();
	}

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;
	m_nCurTone = CBonTuner::TONE_UNKNOWN;

	if (m_hSemaphore) {
		try {
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
			SAFE_CLOSE_HANDLE(m_hSemaphore);
		} catch (...) {
			OutputDebug(L"Exception in ReleaseSemaphore.\n");
		}
	}

	return;
}

const BOOL CBonTuner::SetChannel(const BYTE byCh)
{
	// IBonDriver (not IBonDriver2) �p�C���^�[�t�F�[�X; obsolete?
	return SetChannel(0UL, DWORD(byCh));
}

const float CBonTuner::GetSignalLevel(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqGetSignalLevel;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.GetSignalLevel;
	}
	return 0.0F;
}

float CBonTuner::_GetSignalLevel(void)
{
	HRESULT hr;
	double db = 0.0;

	do {
		if (!m_bOpened) {
			db = -1.0;
			break;
		}

		// SetChannel()����x���Ă΂�Ă��Ȃ��ꍇ��0��Ԃ�
		if (m_dwTargetChannel == CBonTuner::CHANNEL_INVALID) {
			break;
		}

		// �r�b�g���[�g��Ԃ��ꍇ
		if (m_bSignalLevelGetTypeBR) {
			if (m_pBitRate) {
				db = m_pBitRate->GetRate();
			}
			else {
				db = 0.0;
			}
			break;
		}

		// IBdaSpecials2�ŗL�֐�������Ίۓ���
		float f = 0.0F;
		if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->GetSignalStrength(&f)) != E_NOINTERFACE) {
			db = (double)f;
			break;
		}

		// GetSignalState
		//   strength : �f�V�x���P�ʂ̐M���̋��x�������l 
		//   quality  : �M���̕i�������� 1 �` 100 �̒l
		//   lock     : �M����Lock�ł��Ă��邩
		//   present  : �M�����񋟂���Ă��邩
		GetSignalState(&m_dbStrength, &m_dbQuality, &m_dbSignalLocked, &m_dbSignalPresent);

		// Lock�o���Ă��Ȃ��ꍇ��0��Ԃ�
		if (!m_dbSignalLocked) {
			db = 0.0;
			break;
		}

		// Strength��-1��Ԃ��ꍇ������ �� ���̂܂܂̒l��Ԃ�
		if (m_dbStrength == -1.0) {
			db = m_dbStrength;
			break;
		}

		// �v�Z���̕]��
		try {
			db = (double)m_muParser.Eval();
		}
		catch (...) {
		}
	} while (0);

	return (float)db;
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if(*m_phOnWaitTsEvent == NULL ){
		return WAIT_ABANDONED;
	}

	DWORD dwRet;
	if (m_nWaitTsSleep) {
		// WaitTsSleep ���w�肳��Ă���ꍇ
		dwRet = ::WaitForSingleObject(*m_phOnWaitTsEvent, 0);
		// �C�x���g���V�O�i����ԂłȂ���Ύw�莞�ԑҋ@����
		if (dwRet != WAIT_TIMEOUT)
			return dwRet;

		::Sleep(m_nWaitTsSleep);
	}

	// �C�x���g���V�O�i����ԂɂȂ�̂�҂�
	do {
		dwRet = ::WaitForSingleObject(*m_phOnWaitTsEvent, (dwTimeOut) ? dwTimeOut : INFINITE);
	} while ((*m_ppGetTsBuff)->Size() < m_nWaitTsCount);
	return dwRet;
}

const DWORD CBonTuner::GetReadyCount(void)
{
	return (DWORD)(*m_ppGetTsBuff)->Size();
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;
	if (GetTsStream(&pSrc, pdwSize, pdwRemain)) {
		if (*pdwSize)
			memcpy(pDst, pSrc, *pdwSize);
		return TRUE;
	}
	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	SAFE_DELETE(m_LastBuff);
	BOOL bRet = TRUE;
	m_LastBuff = (*m_ppGetTsBuff)->Get();
	if (m_LastBuff) {
		*pdwSize = (DWORD)m_LastBuff->Size;
		*ppDst = m_LastBuff->pbyBuff;
		*pdwRemain = (DWORD)(*m_ppGetTsBuff)->Size();
	}
	else {
		*pdwSize = 0;
		*ppDst = NULL;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	// m_LastBuff �͎Q�Ƃ���Ă���\��������̂� delete ���Ȃ�

	// ��MTS�o�b�t�@
	if (m_pTsBuff) {
		m_pTsBuff->Purge();
	}

	// �f�R�[�h��TS�o�b�t�@
	if (m_pDecodedTsBuff) {
		m_pDecodedTsBuff->Purge();
	}

	// �r�b�g���[�g�v�Z�p�N���X
	if (m_pBitRate)
		m_pBitRate->Clear();
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_sTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqIsTunerOpening;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.IsTunerOpening;
	}
	return FALSE;
}

BOOL CBonTuner::_IsTunerOpening(void)
{
	return m_bOpened;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if (dwSpace < m_TuningData.dwNumSpace) {
		auto itSpace = m_TuningData.Spaces.find(dwSpace);
		if (itSpace != m_TuningData.Spaces.end())
			return itSpace->second.sTuningSpaceName.c_str();
		else
			return _T("-");
	}
	return NULL;
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	auto itSpace = m_TuningData.Spaces.find(dwSpace);
	if (itSpace != m_TuningData.Spaces.end()) {
		if (dwChannel < itSpace->second.dwNumChannel) {
			auto itCh = itSpace->second.Channels.find(dwChannel);
			if (itCh != itSpace->second.Channels.end())
				return itCh->second.sServiceName.c_str();
			else
				return _T("----");
		}
	}
	return NULL;
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqSetChannel;
	arg.uParam.SetChannel.dwSpace = dwSpace;
	arg.uParam.SetChannel.dwChannel = dwChannel;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.SetChannel;
	}
	return FALSE;
}

BOOL CBonTuner::_SetChannel(DWORD dwSpace, DWORD dwChannel)
{
	HRESULT hr;

	OutputDebug(L"SetChannel(%d, %d)\n", dwSpace, dwChannel);

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;

	auto itSpace = m_TuningData.Spaces.find(dwSpace);
	if (itSpace == m_TuningData.Spaces.end()) {
		OutputDebug(L"    Invalid channel space.\n");
		return FALSE;
	}

	if (dwChannel >= itSpace->second.dwNumChannel) {
		OutputDebug(L"    Invalid channel number.\n");
		return FALSE;
	}

	auto itCh = itSpace->second.Channels.find(dwChannel);
	if (itCh == itSpace->second.Channels.end()) {
		OutputDebug(L"    Reserved channel number.\n");
		return FALSE;
	}

	if (!m_bOpened) {
		OutputDebug(L"    Tuner not opened.\n");
		return FALSE;
	}

	m_bRecvStarted = FALSE;
	PurgeTsStream();
	TuningSpaceData * TuningSpace = &itSpace->second;
	ChData * Ch = &itCh->second;

	// TuningSpace�̎�ޔԍ�
	m_nTargetDvbSystemTypeNum = TuningSpace->DVBSystemTypeNumber;
	if (m_nTargetDvbSystemTypeNum != m_nCurrentDvbSystemTypeNum) {
		// TuningSpace�̎�ޔԍ����قȂ�̏ꍇ�͍č\�z
		_CloseTuner(FALSE);
		if (!_OpenTuner()) {
			OutputDebug(L"    Failed to change DVBSystemTypeNumber.\n");
			return FALSE;
		}
	}
	m_LastTuningParam.Frequency = Ch->Frequency + TuningSpace->FrequencyOffset;					// ���g��(MHz)
	m_LastTuningParam.Polarisation = PolarisationMapping[Ch->Polarisation];						// �M���̕Δg
	m_LastTuningParam.Antenna = m_aSatellite[Ch->Satellite].Polarisation[Ch->Polarisation];		// �A���e�i�ݒ�f�[�^
	m_LastTuningParam.Modulation = m_aModulationType[Ch->ModulationType];						// �ϒ������ݒ�f�[�^
	m_LastTuningParam.ONID = Ch->ONID;															// �I���W�i���l�b�g���[�NID / PhysicalChannel (ATSC / Digital Cable)
	m_LastTuningParam.TSID = Ch->TSID;															// �g�����X�|�[�g�X�g���[��ID / Channel (ATSC / Digital Cable)
	m_LastTuningParam.SID = Ch->SID;															// �T�[�r�XID / MinorChannel (ATSC / Digital Cable)
	m_LastTuningParam.MajorChannel = Ch->MajorChannel;											// MajorChannel (Digital Cable)
	m_LastTuningParam.SourceID = Ch->SourceID;													// SourceID (Digital Cable)
	m_LastTuningParam.IniSpaceID = dwSpace;														// ini�t�@�C���œǍ��܂ꂽ�`���[�j���O�X�y�[�X�ԍ�
	m_LastTuningParam.IniChannelID = dwChannel;													// ini�t�@�C���œǍ��܂ꂽ�`�����l���ԍ�
	m_LastTuningParam.LockTwice = m_bLockTwice && Ch->LockTwiceTarget;							// CH�ؑ֓���������I��2�x�s�����ǂ���
	m_LastTuningParam.LockTwiceDelay = m_nLockTwiceDelay;										// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����(msec)

	// IBdaSpecials�Ŏ��O�̏������K�v�Ȃ�s��
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PreLockChannel(&m_LastTuningParam);

	BOOL bRet = LockChannel(&m_LastTuningParam);

	// IBdaSpecials�Œǉ��̏������K�v�Ȃ�s��
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PostLockChannel(&m_LastTuningParam);

	SleepWithMessageLoop(100);
	PurgeTsStream();

	// TSMF�����ݒ�
	if (m_pTSMFParser) {
		switch (itSpace->second.TSMFMode) {
		case EnumSettingValue::TSMFMode::Off:
			m_pTSMFParser->Disable();
			break;
		case EnumSettingValue::TSMFMode::TSID:
			m_pTSMFParser->SetTSID((WORD)Ch->ONID, (WORD)Ch->TSID, FALSE);
			break;
		case EnumSettingValue::TSMFMode::Relarive:
			m_pTSMFParser->SetTSID(0xffff, (WORD)Ch->TSID, TRUE);
			break;
		}
	}

	m_bRecvStarted = TRUE;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ��ƃ`�����l���ԍ�
	m_dwTargetSpace = dwSpace;
	m_dwTargetChannel = dwChannel;

	if (bRet) {
		OutputDebug(L"SetChannel success.\n");
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}
	// m_byCurTone = CBonTuner::TONE_UNKNOWN;

	OutputDebug(L"SetChannel failed.\n");
	if (m_bAlwaysAnswerLocked)
		return TRUE;
	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqGetCurSpace;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.GetCurSpace;
	}
	return CCOMProc::SPACE_INVALID;
}

DWORD CBonTuner::_GetCurSpace(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetSpace;

	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	CCOMProc::COMReqArgs arg;
	arg.nRequest = CCOMProc::enumCOMRequest::eCOMReqGetCurChannel;
	if (m_pCOMProc->RequestCOMReq(&arg)) {
		return arg.uRetVal.GetCurChannel;
	}
	return CCOMProc::CHANNEL_INVALID;
}

DWORD CBonTuner::_GetCurChannel(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetChannel;

	return m_dwCurChannel;
}

void CBonTuner::Release(void)
{
	OutputDebug(L"CBonTuner::Release called.\n");

	delete this;
}

DWORD WINAPI CBonTuner::COMProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*) lpParameter;
	CCOMProc* pCOMProc = pSys->m_pCOMProc;
	HRESULT hr;

	OutputDebug(L"COMProcThread: Thread created.\n");

	// COM������
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// ���[�h���ׂ��`���[�i�E�L���v�`���̃��X�g�쐬
	pSys->m_TunerComboList.BuildComboDB();

	// muparser������
	try {
		// �Q�Ƃ���ϐ���ݒ�
		pSys->m_muParser.DefineVar(_T("S"), &pSys->m_dbStrength);				// ���݂�Strength�l
		pSys->m_muParser.DefineVar(_T("Q"), &pSys->m_dbQuality);				// ���݂�Quality�l
		pSys->m_muParser.DefineVar(_T("L"), &pSys->m_dbSignalLocked);			// ���݂�Lock��Ԓl
		pSys->m_muParser.DefineVar(_T("P"), &pSys->m_dbSignalPresent);			// ���݂̐M���񋟏�Ԓl
		pSys->m_muParser.DefineVar(_T("SC"), &pSys->m_dbStrengthCoefficient);	// Strength �l�␳�W��
		pSys->m_muParser.DefineVar(_T("SB"), &pSys->m_dbStrengthBias);			// Quality �l�␳�W��
		pSys->m_muParser.DefineVar(_T("QC"), &pSys->m_dbQualityCoefficient);	// Strength �l�␳�o�C�A�X
		pSys->m_muParser.DefineVar(_T("QB"), &pSys->m_dbQualityBias);			// Quality �l�␳�o�C�A�X
		// ������ݒ�
		pSys->m_muParser.SetExpr(common::WStringToTString(pSys->m_sSignalLevelCalcFormula));
	}
	catch (...) {
		OutputDebug(L"muParser exception. Wrong formula format?\n");
	}

	// �X���b�h�N��������ʒm
	pCOMProc->NotifyThreadStarted();

	CCOMProc::COMReqArgs arg;
	while (!terminate) {
		CCOMProc::enumCOMWaitStatus ret = pCOMProc->WaitRequest(1000, &arg.nRequest, &arg.uParam);
		switch (ret)
		{
		case CCOMProc::enumCOMWaitStatus::eCOMWaitTerminateRequest:
			terminate = TRUE;
			break;

		case CCOMProc::enumCOMWaitStatus::eCOMWaitRequestEvent:
			switch (arg.nRequest)
			{

			case CCOMProc::enumCOMRequest::eCOMReqOpenTuner:
				arg.uRetVal.OpenTuner = pSys->_OpenTuner();
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqCloseTuner:
				// OpenTuner�ELockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReOpenTuner();
				pCOMProc->ResetReLockChannel();

				pSys->_CloseTuner(FALSE);
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqSetChannel:
				// �ُ팟�m�Ď��^�C�}�[������
				pCOMProc->ResetWatchDog();

				// LockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReLockChannel();

				// OpenTuner�̍Ď��s���Ȃ�FALSE��Ԃ�
				if (pCOMProc->NeedReOpenTuner()) {
					pCOMProc->ClearReOpenChannel();
					arg.uRetVal.SetChannel = FALSE;
				}
				else {
					arg.uRetVal.SetChannel = pSys->_SetChannel(arg.uParam.SetChannel.dwSpace, arg.uParam.SetChannel.dwChannel);
				}
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqGetSignalLevel:
				// OpenTuner�̍Ď��s���Ȃ�0��Ԃ�
				if (pCOMProc->NeedReOpenTuner()) {
					arg.uRetVal.GetSignalLevel = 0.0F;
				}
				else {
					arg.uRetVal.GetSignalLevel = pSys->_GetSignalLevel();
				}
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqIsTunerOpening:
				// OpenTuner�̍Ď��s���Ȃ�TRUE��Ԃ�
				if (pCOMProc->NeedReOpenTuner()) {
					arg.uRetVal.IsTunerOpening = TRUE;
				}
				else {
					arg.uRetVal.IsTunerOpening = pSys->_IsTunerOpening();
				}
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqGetCurSpace:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->NeedReOpenTuner()) {
					arg.uRetVal.GetCurSpace = pCOMProc->GetReOpenSpace();
				}
				else {
					arg.uRetVal.GetCurSpace = pSys->_GetCurSpace();
				}
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			case CCOMProc::enumCOMRequest::eCOMReqGetCurChannel:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->NeedReOpenTuner()) {
					arg.uRetVal.GetCurChannel = pCOMProc->GetReOpenChannel();
				}
				else {
					arg.uRetVal.GetCurChannel = pSys->_GetCurChannel();
				}
				pCOMProc->NotifyComplete(arg.uRetVal);
				break;

			default:
				break;
			}
			break;

		case CCOMProc::enumCOMWaitStatus::eCOMWaitTimeout:
			break;

		case CCOMProc::enumCOMWaitStatus::eCOMWaitFailed:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"COMProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}

		if (terminate)
			break;

		// �X�g���[���X���b�h�v���C�I���e�B�̕ύX
		if (pSys->m_bIsSetStreamThread) {
			if (pSys->m_nThreadPriorityStream != THREAD_PRIORITY_ERROR_RETURN) {
				OutputDebug(L"COMProcThread: Current stream Thread priority = %d.\n", ::GetThreadPriority(pSys->m_hStreamThread));
				::SetThreadPriority(pSys->m_hStreamThread, pSys->m_nThreadPriorityStream);
				OutputDebug(L"COMProcThread: After changed stream Thread priority = %d.\n", ::GetThreadPriority(pSys->m_hStreamThread));
			}
			pSys->m_bIsSetStreamThread = FALSE;
		}

		// �ُ팟�m�����J�o���[
		// 1000ms������
		if (pCOMProc->CheckTick()) {

			// SetChannel()���s���̃o�b�N�O�����hCH�ؑ֊J�n
			if (pSys->m_bBackgroundChannelLock && pSys->m_dwCurChannel == CBonTuner::CHANNEL_INVALID && pSys->m_dwTargetChannel != CBonTuner::CHANNEL_INVALID) {
				OutputDebug(L"COMProcThread: Background retry.\n");
				pCOMProc->SetReLockChannel();
			}

			// �ُ팟�m
			if (!pCOMProc->NeedReLockChannel() && !pCOMProc->NeedReOpenTuner() && pSys->m_dwCurChannel != CBonTuner::CHANNEL_INVALID) {

				// SignalLock�̏�Ԋm�F
				if (pSys->m_nWatchDogSignalLocked != 0) {
					double lock = 0.0;
					pSys->GetSignalState(NULL, NULL, &lock, NULL);
					if (pCOMProc->CheckSignalLockErr((BOOL)lock, pSys->m_nWatchDogSignalLocked * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogSignalLocked time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // SignalLock�̏�Ԋm�F

				// BitRate�m�F
				if (pSys->m_nWatchDogBitRate != 0 && pSys->m_pBitRate) {
					if (pCOMProc->CheckBitRateErr((pSys->m_pBitRate->GetRate() > 0.0), pSys->m_nWatchDogBitRate * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogBitRate time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // BitRate�m�F
			} // �ُ팟�m

			// CH�ؑ֓��쎎�s���OpenTuner�Ď��s
			if (pCOMProc->NeedReOpenTuner()) {
				// OpenTuner�Ď��s
				pSys->_CloseTuner(pSys->m_bTryAnotherTuner);
				if (pSys->_OpenTuner() && (!pCOMProc->CheckReOpenChannel() || pSys->_SetChannel(pCOMProc->GetReOpenSpace(), pCOMProc->GetReOpenChannel()))) {
					// OpenTuner�ɐ������ASetChannnel�ɐ����������͕K�v�Ȃ�
					OutputDebug(L"COMProcThread: Re-OpenTuner SUCCESS.\n");
					pCOMProc->ResetReOpenTuner();
				}
				else {
					// ���s...���̂܂܎�����`�������W����
					OutputDebug(L"COMProcThread: Re-OpenTuner FAILED.\n");
				}
			} // CH�ؑ֓��쎎�s���OpenTuner�Ď��s

			// �ُ팟�m��`�����l�����b�N�Ď��s
			if (!pCOMProc->NeedReOpenTuner() && pCOMProc->NeedReLockChannel()) {
				// �`�����l�����b�N�Ď��s
				if (pSys->LockChannel(&pSys->m_LastTuningParam)) {
					// LockChannel�ɐ�������
					OutputDebug(L"COMProcThread: Re-LockChannel SUCCESS.\n");
					pCOMProc->ResetReLockChannel();
				}
				else {
					// LockChannel���s
					OutputDebug(L"COMProcThread: Re-LockChannel FAILED.\n");
					if (pSys->m_nReOpenWhenGiveUpReLock != 0) {
						// CH�ؑ֓��쎎�s�񐔐ݒ�l��0�ȊO
						if (pCOMProc->CheckReLockFailCount(pSys->m_nReOpenWhenGiveUpReLock)) {
							// CH�ؑ֓��쎎�s�񐔂𒴂����̂�OpenTuner�Ď��s
							OutputDebug(L"COMProcThread: ReOpenWhenGiveUpReLock count is up.\n");
							pCOMProc->SetReOpenTuner(pSys->m_dwTargetSpace, pSys->m_dwTargetChannel);
							pCOMProc->ResetReLockChannel();
						}
					}
				}
			} // �ُ팟�m��`�����l�����b�N�Ď��s
		} // 1000ms������
	} // while (!terminate)

	// �O���t�֌W�̉��
	pSys->_CloseTuner(FALSE);

	::CoUninitialize();
	OutputDebug(L"COMProcThread: Thread terminated.\n");

	return 0;
}

DWORD WINAPI CBonTuner::DecodeProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*)lpParameter;
	CDecodeProc* pDecodeProc = pSys->m_pDecodeProc;

	BOOL bNeedDecode = FALSE;

	HRESULT hr;

	OutputDebug(L"DecodeProcThread: Thread created.\n");

	// COM������
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// IBdaSpecials�ɂ��f�R�[�h�������K�v���ǂ���
	BOOL b = FALSE;
	if (pSys->m_pIBdaSpecials2 && SUCCEEDED(hr = pSys->m_pIBdaSpecials2->IsDecodingNeeded(&b))) {
		if (b)
			bNeedDecode = TRUE;
	}
	OutputDebug(L"DecodeProcThread: Detected IBdaSpecials decoding=%d.\n", bNeedDecode);

	// �X���b�h�N��������ʒm
	pDecodeProc->NotifyThreadStarted();

	while (!terminate) {
		DWORD remain = (pSys->m_pBitRate) ? pSys->m_pBitRate->CheckRate() : INFINITE;
		CDecodeProc::enumDecodeWaitStatus ret = pDecodeProc->WaitRequest(remain, pSys->m_hOnStreamEvent);
		switch (ret)
		{
		case CDecodeProc::enumDecodeWaitStatus::eDecodeWaitTerminateRequest:
			terminate = TRUE;
			break;
		case CDecodeProc::enumDecodeWaitStatus::eDecodeWaitRecvEvent:
			if (pSys->m_pTsBuff) {
				// TS�o�b�t�@����̃f�[�^�擾
				while (TS_DATA* pBuff = pSys->m_pTsBuff->Get()) {
					// �K�v�Ȃ��IBdaSpecials�ɂ��f�R�[�h�������s��
					if (bNeedDecode && pSys->m_pIBdaSpecials2) {
						pSys->m_pIBdaSpecials2->Decode(pBuff->pbyBuff, (DWORD)pBuff->Size);
					}

					// �擾�����o�b�t�@���f�R�[�h�ς݃o�b�t�@�ɒǉ�
					if (pSys->m_pTSMFParser && pSys->m_pTSMFParser->IsEnabled()) {
						// TSMF�̏������s��
						BYTE* newBuf = NULL;
						size_t newBufSize = 0;
						pSys->m_pTSMFParser->ParseTsBuffer(pBuff->pbyBuff, pBuff->Size, &newBuf, &newBufSize);
						if (newBuf) {
							TS_DATA* pNewTS = new TS_DATA(newBuf, (DWORD)newBufSize, FALSE);
							pSys->m_pDecodedTsBuff->Add(pNewTS);
						}
						SAFE_DELETE(pBuff);
					}
					else {
						// TSMF�̏������s��Ȃ��ꍇ�͂��̂܂ܒǉ�
						pSys->m_pDecodedTsBuff->Add(pBuff);
					}

					// ��M�C�x���g�Z�b�g
					::SetEvent(pSys->m_hOnDecodeEvent);
				}
			}
			break;
		case CDecodeProc::enumDecodeWaitStatus::eDecodeWaitTimeout:
			break;
		case CDecodeProc::enumDecodeWaitStatus::eDecodeWaitFailed:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"DecodeProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}
	}

	::CoUninitialize();
	OutputDebug(L"DecodeProcThread: Thread terminated.\n");

	return 0;
}

int CALLBACK CBonTuner::RecvProc(void* pParam, BYTE* pbData, size_t size)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	// �X�g���[���X���b�h�̃n���h���𕡐�
	if (pSys->m_hStreamThread == NULL) {
		::DuplicateHandle(pSys->m_hProcess, GetCurrentThread(), GetCurrentProcess(), &(pSys->m_hStreamThread), 0, FALSE, 2);
		pSys->m_bIsSetStreamThread = TRUE;
	}

	if (pSys->m_pBitRate) {
		pSys->m_pBitRate->AddRate((DWORD)size);
	}

	if (pSys->m_bRecvStarted && pSys->m_pTsBuff) {
		if (pSys->m_pTsBuff->AddData(pbData, size)) {
			::SetEvent(pSys->m_hOnStreamEvent);
		}
	}

	return 0;
}

void CBonTuner::StartRecv(void)
{
	if (m_pITsWriter)
		m_pITsWriter->SetCallBackRecv(RecvProc, this);
	m_bRecvStarted = TRUE;
}

void CBonTuner::StopRecv(void)
{
	if (m_pITsWriter)
		m_pITsWriter->SetCallBackRecv(NULL, this);
	m_bRecvStarted = FALSE;
}

void CBonTuner::ReadIniFile(void)
{
	// INI�t�@�C���̃t�@�C�����擾
	std::wstring tempPath = common::GetModuleName(st_hModule);
	m_sIniFilePath = tempPath + L"ini";

	CIniFileAccess IniFileAccess(m_sIniFilePath.c_str());
	int val;

	// DebugLog���L�^���邩�ǂ���
	if (IniFileAccess.ReadKeyB(L"BONDRIVER", L"DebugLog", FALSE)) {
		SetDebugLog(tempPath + L"log");
	}

	//
	// Tuner �Z�N�V����
	//
	IniFileAccess.ReadSection(L"TUNER");
	IniFileAccess.CreateSectionData();

	// GUID0 - GUID99: Tuner�f�o�C�X��GUID ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// FriendlyName0 - FriendlyName99: Tuner�f�o�C�X��FriendlyName ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// CaptureGUID0 - CaptureGUID99: Capture�f�o�C�X��GUID ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	// CaptureFriendlyName0 - CaptureFriendlyName99: Capture�f�o�C�X��FriendlyName ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	for (unsigned int i = 0; i < MAX_TUNER_GROUP; i++) {
		size_t count = 0;
		for (unsigned int j = 0; j < MAX_GUID; j++) {
			std::wstring key;
			std::wstring tunerGuid;
			std::wstring tunerFriendlyName;
			std::wstring captureGuid;
			std::wstring captureFriendlyName;
			if (i == 0) {
				// TunerGroup �w��Ȃ��`���œǍ���
				key = L"GUID" + std::to_wstring(j);
				tunerGuid = IniFileAccess.ReadKeySSectionData(key.c_str(), L"");
				key = L"FriendlyName" + std::to_wstring(j);
				tunerFriendlyName = IniFileAccess.ReadKeySSectionData(key.c_str(), L"");
				key = L"CaptureGUID" + std::to_wstring(j);
				captureGuid = IniFileAccess.ReadKeySSectionData(key.c_str(), L"");
				key = L"CaptureFriendlyName" + std::to_wstring(j);
				captureFriendlyName = IniFileAccess.ReadKeySSectionData(key.c_str(), L"");
			}
			// TunerGroup0 �` TunerGroup9 �`���œǍ���
			std::wstring prefix = L"TunerGroup" + std::to_wstring(i);
			key = prefix + L"GUID" + std::to_wstring(j);
			tunerGuid = IniFileAccess.ReadKeySSectionData(key.c_str(), tunerGuid.c_str());
			key = prefix + L"FriendlyName" + std::to_wstring(j);
			tunerFriendlyName = IniFileAccess.ReadKeySSectionData(key.c_str(), tunerFriendlyName.c_str());
			key = prefix + L"CaptureGUID" + std::to_wstring(j);
			captureGuid = IniFileAccess.ReadKeySSectionData(key.c_str(), captureGuid.c_str());
			key = prefix + L"CaptureFriendlyName" + std::to_wstring(j);
			captureFriendlyName = IniFileAccess.ReadKeySSectionData(key.c_str(), captureFriendlyName.c_str());

			if (tunerGuid.length() == 0 && tunerFriendlyName.length() == 0 && captureGuid.length() == 0 && captureFriendlyName.length() == 0) {
				// �ǂ���w�肳��Ă��Ȃ�
				if (i == 0 && j == 0) {
					// �ԍ��Ȃ��̌^���œǍ���
					tunerGuid = IniFileAccess.ReadKeySSectionData(L"GUID", L"");
					tunerFriendlyName = IniFileAccess.ReadKeySSectionData(L"FriendlyName", L"");
					captureGuid = IniFileAccess.ReadKeySSectionData(L"CaptureGUID", L"");
					captureFriendlyName = IniFileAccess.ReadKeySSectionData(L"CaptureFriendlyName", L"");
					// GUID0 / FriendlyName0 �͂ǂ���w�肳��Ă��Ȃ��ꍇ�ł��o�^
				}
				else
					// �ǂ���w�肪�Ȃ��Ȃ������_�Ń��[�v���I���
					break;
			}

			// �o�^
			m_TunerComboList.AddSearchData(i, j, tunerGuid, tunerFriendlyName, captureGuid, captureFriendlyName);
			count++;
		}
		if (count == 0) {
			// TunerGroup �ɓo�^�������̂��Ȃ��Ȃ������_�Ń��[�v���I���
			break;
		}
	}

	// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ�
	m_TunerComboList.bNotExistCaptureDevice = IniFileAccess.ReadKeyBSectionData(L"NotExistCaptureDevice", m_TunerComboList.bNotExistCaptureDevice);

	// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
	m_TunerComboList.bCheckDeviceInstancePath = IniFileAccess.ReadKeyBSectionData(L"CheckDeviceInstancePath", m_TunerComboList.bCheckDeviceInstancePath);

	// Tuner��: GetTunerName�ŕԂ��`���[�i�� ... �w�肳��Ȃ���΃f�t�H���g����
	//   �g����B���̏ꍇ�A�����`���[�i�𖼑O�ŋ�ʂ��鎖�͂ł��Ȃ�
	m_sTunerName = common::WStringToTString(IniFileAccess.ReadKeySSectionData(L"Name", m_sTunerName.c_str()));

	// �`���[�i�ŗL�֐����g�p���邩�ǂ����B
	//   �ȉ��� INI �t�@�C���Ŏw��\
	//     "" ... �g�p���Ȃ�(default); "AUTO" ... AUTO
	//     "DLLName" ... �`���[�i�ŗL�֐��̓�����DLL���𒼐ڎw��
	m_sDLLBaseName = IniFileAccess.ReadKeySSectionData(L"UseSpecial", m_sDLLBaseName.c_str());

	// Tone�M���ؑ֎���Wait����
	m_nToneWait = IniFileAccess.ReadKeyISectionData(L"ToneSignalWait", m_nToneWait);

	// CH�ؑ֌��Lock�m�F����
	m_nLockWait = IniFileAccess.ReadKeyISectionData(L"ChannelLockWait", m_nLockWait);

	// CH�ؑ֌��Lock�m�FDelay����
	m_nLockWaitDelay = IniFileAccess.ReadKeyISectionData(L"ChannelLockWaitDelay", m_nLockWaitDelay);

	// CH�ؑ֌��Lock�m�FRetry��
	m_nLockWaitRetry = IniFileAccess.ReadKeyISectionData(L"ChannelLockWaitRetry", m_nLockWaitRetry);

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	m_bLockTwice = IniFileAccess.ReadKeyBSectionData(L"ChannelLockTwice", m_bLockTwice);

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����
	m_nLockTwiceDelay = IniFileAccess.ReadKeyISectionData(L"ChannelLockTwiceDelay", m_nLockTwiceDelay);

	// SignalLock�ُ̈팟�m����(�b)
	m_nWatchDogSignalLocked = IniFileAccess.ReadKeyISectionData(L"WatchDogSignalLocked", m_nWatchDogSignalLocked);

	// BitRate�ُ̈팟�m����(�b)
	m_nWatchDogBitRate = IniFileAccess.ReadKeyISectionData(L"WatchDogBitRate", m_nWatchDogBitRate);
	if (m_nWatchDogBitRate > 0) {
		m_bNeedBitRate = TRUE;
		m_bNeedDecodeProc = TRUE;
	}

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	m_nReOpenWhenGiveUpReLock = IniFileAccess.ReadKeyISectionData(L"ReOpenWhenGiveUpReLock", m_nReOpenWhenGiveUpReLock);

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	m_bTryAnotherTuner = IniFileAccess.ReadKeyBSectionData(L"TryAnotherTuner", m_bTryAnotherTuner);

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	m_bBackgroundChannelLock = IniFileAccess.ReadKeyBSectionData(L"BackgroundChannelLock", m_bBackgroundChannelLock);

	// Tuning Space���i�݊��p�j
	std::wstring sTempTuningSpaceName = IniFileAccess.ReadKeySSectionData(L"TuningSpaceName", L"�X�J�p�[");

	// SignalLevel �Z�o���@
	m_nSignalLevelCalcType = (EnumSettingValue::SignalLevelCalcType)IniFileAccess.ReadKeyIValueMapSectionData(L"SignalLevelCalcType", (int)m_nSignalLevelCalcType, &EnumSettingValue::mapSignalLevelCalcType);
	if (m_nSignalLevelCalcType >= EnumSettingValue::SignalLevelCalcType::SSMin && m_nSignalLevelCalcType <= EnumSettingValue::SignalLevelCalcType::SSMax)
		m_bSignalLevelGetTypeSS = TRUE;
	else if (m_nSignalLevelCalcType >= EnumSettingValue::SignalLevelCalcType::TunerMin && m_nSignalLevelCalcType <= EnumSettingValue::SignalLevelCalcType::TunerMax)
		m_bSignalLevelGetTypeTuner = TRUE;
	else if (m_nSignalLevelCalcType >= EnumSettingValue::SignalLevelCalcType::DemodSSMin && m_nSignalLevelCalcType <= EnumSettingValue::SignalLevelCalcType::DemodSSMax)
		m_bSignalLevelGetTypeDemodSS = TRUE;
	else if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::BR) {
		m_bSignalLevelGetTypeBR = TRUE;
		m_bNeedBitRate = TRUE;
		m_bNeedDecodeProc = TRUE;
	}

	if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::SSStrength || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::TunerStrength || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::DemodSSStrength) {
		m_bSignalLevelNeedStrength = TRUE;
		m_sSignalLevelCalcFormula = L"S / SC + SB";
	}
	if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::SSQuality || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::TunerQuality || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::DemodSSQuality) {
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"Q / QC + QB";
	}
	if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::SSMul || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::TunerMul || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::DemodSSMul) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"(S / SC + SB) * (Q / QC + QB)";
	}
	if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::SSAdd || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::TunerAdd || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::DemodSSAdd) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"(S / SC + SB) + (Q / QC + QB)";
	}
	if (m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::SSFormula || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::TunerFormula || m_nSignalLevelCalcType == EnumSettingValue::SignalLevelCalcType::DemodSSFormula) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = IniFileAccess.ReadKeySSectionData(L"SignalLevelCalcFormula", L"S / SC + SB");
	}

	// Strength �l�␳�W��
	m_dbStrengthCoefficient = (double)IniFileAccess.ReadKeyFSectionData(L"StrengthCoefficient", m_dbStrengthCoefficient);
	if (m_dbStrengthCoefficient == 0.0)
		m_dbStrengthCoefficient = 1.0;

	// Quality �l�␳�W��
	m_dbQualityCoefficient = (double)IniFileAccess.ReadKeyFSectionData(L"QualityCoefficient", m_dbQualityCoefficient);
	if (m_dbQualityCoefficient == 0.0)
		m_dbQualityCoefficient = 1.0;

	// Strength �l�␳�o�C�A�X
	m_dbStrengthBias = (double)IniFileAccess.ReadKeyFSectionData(L"StrengthBias", m_dbStrengthBias);

	// Quality �l�␳�o�C�A�X
	m_dbQualityBias = (double)IniFileAccess.ReadKeyFSectionData(L"QualityBias", m_dbQualityBias);

	// �`���[�j���O��Ԃ̔��f���@
	m_nSignalLockedJudgeType = (EnumSettingValue::SignalLockedJudgeType)IniFileAccess.ReadKeyIValueMapSectionData(L"SignalLockedJudgeType", (int)m_nSignalLockedJudgeType, &EnumSettingValue::mapSignalLockedJudgeType);
	if (m_nSignalLockedJudgeType == EnumSettingValue::SignalLockedJudgeType::SS)
		m_bSignalLockedJudgeTypeSS = TRUE;
	else if (m_nSignalLockedJudgeType == EnumSettingValue::SignalLockedJudgeType::Tuner)
		m_bSignalLockedJudgeTypeTuner = TRUE;
	else if (m_nSignalLockedJudgeType == EnumSettingValue::SignalLockedJudgeType::DemodSS)
		m_bSignalLockedJudgeTypeDemodSS = TRUE;

	// �`���[�i�[�Ɏg�p����NetworkProvider(�f�t�H���g)
	EnumSettingValue::NetworkProvider nNetworkProvider = (EnumSettingValue::NetworkProvider)IniFileAccess.ReadKeyIValueMapSectionData(L"NetworkProvider", (int)EnumSettingValue::NetworkProvider::Auto, &EnumSettingValue::mapNetworkProvider);

	for (unsigned int i = 0; i < MAX_DVB_SYSTEM_TYPE; i++) {
		std::wstring key, prefix[2];
		DVBSystemTypeData typeData;
		if (i == 0) {
			// �`���[�i�[�̎g�p����TuningSpace�̎��
			typeData.nDVBSystemType = EnumSettingValue::TunerType::DVBS;
		}

		unsigned int st = i == 0 ? 0 : 1;
		prefix[0] = L"DVBSystemType";
		prefix[1] = L"DVBSystemType" + std::to_wstring(i);
		for (unsigned int j = st; j < 2; j++) {
			// �`���[�i�[�̎g�p����TuningSpace�̎��
			key = prefix[j];
			typeData.nDVBSystemType = (EnumSettingValue::TunerType)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nDVBSystemType, &EnumSettingValue::mapTuningSpaceType);

			// �g�p����ITuningSpace interface
			key = prefix[j] + L"TuningSpace";
			typeData.nTuningSpace = (EnumSettingValue::TuningSpace)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nTuningSpace, &EnumSettingValue::mapSpecifyTuningSpace);

			// �g�p����ILocator interface
			key = prefix[j] + L"Locator";
			typeData.nLocator = (EnumSettingValue::Locator)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nLocator, &EnumSettingValue::mapSpecifyLocator);

			// ITuningSpace�ɐݒ肷��NetworkType
			key = prefix[j] + L"ITuningSpaceNetworkType";
			typeData.nITuningSpaceNetworkType = (EnumSettingValue::NetworkType)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nITuningSpaceNetworkType, &EnumSettingValue::mapSpecifyITuningSpaceNetworkType);

			// IDVBTuningSpace�ɐݒ肷��SystemType
			key = prefix[j] + L"IDVBTuningSpaceSystemType";
			typeData.nIDVBTuningSpaceSystemType = (EnumSettingValue::DVBSystemType)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nIDVBTuningSpaceSystemType, &EnumSettingValue::mapSpecifyIDVBTuningSpaceSystemType);

			// IAnalogTVTuningSpace�ɐݒ肷��InputType
			key = prefix[j] + L"IAnalogTVTuningSpaceInputType";
			typeData.nIAnalogTVTuningSpaceInputType = (EnumSettingValue::TunerInputType)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)typeData.nIAnalogTVTuningSpaceInputType, &EnumSettingValue::mapSpecifyIAnalogTVTuningSpaceInputType);

			// �`���[�i�[�Ɏg�p����NetworkProvider
			key = prefix[j] + L"NetworkProvider";
			typeData.nNetworkProvider = (EnumSettingValue::NetworkProvider)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), (int)nNetworkProvider, &EnumSettingValue::mapNetworkProvider);

			// �g�p����TunerGroup�ԍ�
			key = prefix[j] + L"TunerGroup";
			typeData.nTunerGroup = IniFileAccess.ReadKeyISectionData(key.c_str(), typeData.nTunerGroup);
		}

		if (typeData.nDVBSystemType == EnumSettingValue::TunerType::None && (typeData.nTuningSpace == EnumSettingValue::TuningSpace::Auto || typeData.nLocator == EnumSettingValue::Locator::Auto)) {
			continue;
		}

		// DB�ɓo�^
		auto it = m_DVBSystemTypeDB.SystemType.find(i);
		if (it == m_DVBSystemTypeDB.SystemType.end()) {
			it = m_DVBSystemTypeDB.SystemType.emplace(i, typeData).first;
			m_DVBSystemTypeDB.nNumType++;
		}
	}

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	m_nDefaultNetwork = (EnumSettingValue::DefaultNetwork)IniFileAccess.ReadKeyIValueMapSectionData(L"DefaultNetwork", (int)m_nDefaultNetwork, &EnumSettingValue::mapDefaultNetwork);

	//
	// BonDriver �Z�N�V����
	//
	IniFileAccess.ReadSection(L"BONDRIVER");
	IniFileAccess.CreateSectionData();

	// �X�g���[���f�[�^�o�b�t�@1���̃T�C�Y
	// 188�~�ݒ萔(bytes)
	m_nBuffSize = (size_t)IniFileAccess.ReadKeyISectionData(L"BuffSize", (int)m_nBuffSize);

	// �X�g���[���f�[�^�o�b�t�@�̍ő��
	m_nMaxBuffCount = (size_t)IniFileAccess.ReadKeyISectionData(L"MaxBuffCount", (int)m_nMaxBuffCount);

	// WaitTsStream���A�w�肳�ꂽ�����̃X�g���[���f�[�^�o�b�t�@�����܂�܂őҋ@����
	// �`���[�i��CPU���ׂ������Ƃ��͐��l��傫�ڂɂ���ƌ��ʂ�����ꍇ������
	m_nWaitTsCount = IniFileAccess.ReadKeyISectionData(L"WaitTsCount", m_nWaitTsCount);
	if (m_nWaitTsCount < 1)
		m_nWaitTsCount = 1;

	// WaitTsStream���X�g���[���f�[�^�o�b�t�@�����܂��Ă��Ȃ��ꍇ�ɍŒ���ҋ@���鎞��(msec)
	// �`���[�i��CPU���ׂ������Ƃ���100msec���x���w�肷��ƌ��ʂ�����ꍇ������
	m_nWaitTsSleep = IniFileAccess.ReadKeyISectionData(L"WaitTsSleep", m_nWaitTsSleep);

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	m_bAlwaysAnswerLocked = IniFileAccess.ReadKeyBSectionData(L"AlwaysAnswerLocked", m_bAlwaysAnswerLocked);

	// COMProcThread�̃X���b�h�v���C�I���e�B
	m_nThreadPriorityCOM = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityCOM", m_nThreadPriorityCOM, &EnumSettingValue::mapThreadPriority);

	// DecodeProcThread�̃X���b�h�v���C�I���e�B
	m_nThreadPriorityDecode = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityDecode", m_nThreadPriorityDecode, &EnumSettingValue::mapThreadPriority);

	// �X�g���[���X���b�h�v���C�I���e�B
	m_nThreadPriorityStream = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityStream", m_nThreadPriorityStream, &EnumSettingValue::mapThreadPriority);

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	m_nPeriodicTimer = IniFileAccess.ReadKeyISectionData(L"PeriodicTimer", m_nPeriodicTimer);

	//
	// Satellite �Z�N�V����
	//
	IniFileAccess.ReadSection(L"SATELLITE");
	IniFileAccess.CreateSectionData();

	// �q���ʎ�M�p�����[�^
	std::wstring sateliteSettingsAuto[MAX_SATELLITE];

	// Satellite0�`�͉q���ݒ薳���p�iini�t�@�C������̓Ǎ��͍s��Ȃ��j
	m_sSatelliteName[0] = L"not set";						// �`�����l���������p�q������
	// ���̈ȊO�̓R���X�g���N�^�̃f�t�H���g�l�g�p

	// DefaultNetwork�ɂ��f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case EnumSettingValue::DefaultNetwork::SPHD:
		// SPHD
		sateliteSettingsAuto[1] = L"JCSAT-3";
		sateliteSettingsAuto[2] = L"JCSAT-4";
		break;

	case EnumSettingValue::DefaultNetwork::BSCS:
	case EnumSettingValue::DefaultNetwork::Dual:
		sateliteSettingsAuto[1] = L"BS/CS110";
		break;
	}

	// BS/CS110��CH�ݒ莩���������Ɏg�p����q���ݒ�ԍ�
	unsigned int satelliteNumberBSCS110R = 1;

	// SPHD��CH�ݒ莩���������Ɏg�p����q���ݒ�ԍ�
	unsigned int satelliteNumberJCSAT3 = 1;
	unsigned int satelliteNumberJCSAT4 = 2;

	// �q���ݒ�1�`9�̐ݒ��Ǎ�
	for (unsigned int satellite = 1; satellite < MAX_SATELLITE; satellite++) {
		std::wstring key, prefix1, prefix2;
		prefix1 = L"Satellite" + std::to_wstring(satellite);

		// �q���ݒ莩������
		key = prefix1 + L"SettingsAuto";
		sateliteSettingsAuto[satellite] = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(key.c_str(), sateliteSettingsAuto[satellite].c_str()));

		if (sateliteSettingsAuto[satellite] == L"JCSAT-3") {
			// JCSAT-3A
			satelliteNumberJCSAT3 = satellite;
			m_sSatelliteName[satellite] = L"128.0E";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[1].HighOscillator = m_aSatellite[satellite].Polarisation[1].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[1].Tone = 0;																			// �����Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[2].HighOscillator = m_aSatellite[satellite].Polarisation[2].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[2].Tone = 0;																			// �����Δg���g�[���M��
		}
		else if (sateliteSettingsAuto[satellite] == L"JCSAT-4") {
			// JCSAT-4B
			satelliteNumberJCSAT4 = satellite;
			m_sSatelliteName[satellite] = L"124.0E";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[1].HighOscillator = m_aSatellite[satellite].Polarisation[1].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[1].Tone = 1;																			// �����Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[2].HighOscillator = m_aSatellite[satellite].Polarisation[2].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[2].Tone = 1;																			// �����Δg���g�[���M��
		}
		else if (sateliteSettingsAuto[satellite] == L"BS/CS110") {
			// BS/CS110 �E���~�Δg�ƍ����~�Δg
			satelliteNumberBSCS110R = satellite;
			m_sSatelliteName[satellite] = L"BS/CS110";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[3].HighOscillator = m_aSatellite[satellite].Polarisation[3].LowOscillator = 9505000;	// �����~�Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[3].Tone = 0;																			// �����~�Δg�Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[4].HighOscillator = m_aSatellite[satellite].Polarisation[4].LowOscillator = 10678000;	// �E���~�Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[4].Tone = 0;																			// �E���~�Δg�Δg���g�[���M��
		}

		// �T�[�r�X�\���p�q������
		key = prefix1 + L"Name";
		m_sSatelliteName[satellite] = IniFileAccess.ReadKeySSectionData(key.c_str(), m_sSatelliteName[satellite].c_str());

		// �Δg���1�`4�̃A���e�i�ݒ��Ǎ�
		for (unsigned int polarisation = 1; polarisation < POLARISATION_SIZE; polarisation++) {
			prefix2 = prefix1 + PolarisationChar[polarisation];
			// �ǔ����g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"Oscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix1 + L"HighOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix1 + L"LowOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].LowOscillator);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"Oscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix2 + L"HighOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix2 + L"LowOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].LowOscillator);

			// LNB�ؑ֎��g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"LNBSwitch";
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"LNBSwitch";
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch);

			// �g�[���M�� (0 or 1)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"ToneSignal";
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)IniFileAccess.ReadKeyBSectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].Tone);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"ToneSignal";
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)IniFileAccess.ReadKeyBSectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].Tone);

			// DiSEqC
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"DiSEqC";
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, &EnumSettingValue::mapDiSEqC);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"DiSEqC";
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, &EnumSettingValue::mapDiSEqC);
		}
	}

	//
	// Modulation �Z�N�V����
	//
	IniFileAccess.ReadSection(L"MODULATION");
	IniFileAccess.CreateSectionData();

	// �ϒ������ʃp�����[�^�i0�`9�̏��Ȃ̂Œ��Ӂj
	std::wstring modulationSettingsAuto[MAX_MODULATION];

	// DefaultNetwork�ɂ��f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case EnumSettingValue::DefaultNetwork::SPHD:
		//SPHD
		modulationSettingsAuto[0] = L"DVB-S";
		modulationSettingsAuto[1] = L"DVB-S2";
		break;

	case EnumSettingValue::DefaultNetwork::BSCS:
		// BS/CS110
		modulationSettingsAuto[0] = L"ISDB-S";
		break;

	case EnumSettingValue::DefaultNetwork::UHF:
		// UHF/CATV
		modulationSettingsAuto[0] = L"ISDB-T";
		break;

	case EnumSettingValue::DefaultNetwork::Dual:
		modulationSettingsAuto[0] = L"ISDB-T";
		modulationSettingsAuto[1] = L"ISDB-S";
		break;
	}

	// UHF / CATV��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberISDBT = 0;

	// BS/CS110��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberISDBS = 0;

	// SPHD��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberDVBS = 0;
	unsigned int modulationNumberDVBS2 = 1;

	// ISDB-C�g�����X���W�����[�V������CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberJ83C64QAM = 0;
	unsigned int modulationNumberJ83C256QAM = 1;

	// �X�J�p�[!�v���~�A���T�[�r�X����CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberOpticast = 0;

	// �ϒ������ݒ�0�`9�̒l��Ǎ�
	for (unsigned int modulation = 0; modulation < MAX_MODULATION; modulation++) {
		std::wstring key, prefix;
		prefix = L"ModulationType" + std::to_wstring(modulation);

		// �ϒ������ݒ莩������
		key = prefix + L"SettingsAuto";
		modulationSettingsAuto[modulation] = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(key.c_str(), modulationSettingsAuto[modulation].c_str()));

		if (modulationSettingsAuto[modulation] == L"DVB-S") {
			// SPHD DVB-S
			modulationNumberDVBS = modulation;
			m_sModulationName[modulation] = L"DVB-S";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_QPSK;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 21096;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"DVB-S2") {
			// SPHD DVB-S2
			modulationNumberDVBS2 = modulation;
			m_sModulationName[modulation] = L"DVB-S2";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_NBC_8PSK;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_LDPC;				// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_5;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_BCH;				// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 23303;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"ISDB-S") {
			// BS/CS110
			modulationNumberISDBS = modulation;
			m_sModulationName[modulation] = L"ISDB-S";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_ISDB_S_TMCC;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_2_3;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 28860;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"ISDB-T") {
			// UHF/CATV
			modulationNumberISDBT = modulation;
			m_sModulationName[modulation] = L"ISDB-T";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_ISDB_T_TMCC;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = -1;						// �V���{�����[�g
			m_aModulationType[modulation].BandWidth = 6;						// �ш敝(MHz)
		}
		else if (modulationSettingsAuto[modulation] == L"J.83C-64QAM") {
			// ISDB-C�g�����X���W�����[�V���� 64QAM
			modulationNumberJ83C64QAM = modulation;
			m_sModulationName[modulation] = L"J.83C-64QAM";						// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_64QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_METHOD_NOT_SET;	// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_NOT_SET;	// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 5274;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83C-256QAM") {
			// ISDB-C�g�����X���W�����[�V���� 256QAM
			modulationNumberJ83C256QAM = modulation;
			m_sModulationName[modulation] = L"J.83C-256QAM";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_METHOD_NOT_SET;	// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_NOT_SET;	// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 5274;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-OPTICAST") {
			// �X�J�p�[!�v���~�A���T�[�r�X�� 256QAM
			modulationNumberOpticast = modulation;
			m_sModulationName[modulation] = L"J.83B-Opticast";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5600;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-64QAM") {
			// J.83 Annex-B 64QAM
			m_sModulationName[modulation] = L"J.83B-64QAM";						// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_64QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5057;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-256QAM") {
			// J.83 Annex-B 256QAM
			m_sModulationName[modulation] = L"J.83B-256QAM";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5361;					// �V���{�����[�g
		}

		// �`�����l���������p�ϒ���������
		key = prefix + L"Name";
		m_sModulationName[modulation] = IniFileAccess.ReadKeySSectionData(key.c_str(), m_sModulationName[modulation].c_str());

		// �ϒ��^�C�v
		key = prefix + L"Modulation";
		m_aModulationType[modulation].Modulation
			= (ModulationType)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aModulationType[modulation].Modulation, &EnumSettingValue::mapModulationType);

		// �����O���������^�C�v
		key = prefix + L"InnerFEC";
		m_aModulationType[modulation].InnerFEC
			= (FECMethod)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aModulationType[modulation].InnerFEC, &EnumSettingValue::mapFECMethod);

		// ����FEC���[�g
		key = prefix + L"InnerFECRate";
		m_aModulationType[modulation].InnerFECRate
			= (BinaryConvolutionCodeRate)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aModulationType[modulation].InnerFECRate, &EnumSettingValue::mapBinaryConvolutionCodeRate);

		// �O���O���������^�C�v
		key = prefix + L"OuterFEC";
		m_aModulationType[modulation].OuterFEC
			= (FECMethod)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aModulationType[modulation].OuterFEC, &EnumSettingValue::mapFECMethod);

		// �O��FEC���[�g
		key = prefix + L"OuterFECRate";
		m_aModulationType[modulation].OuterFECRate
			= (BinaryConvolutionCodeRate)IniFileAccess.ReadKeyIValueMapSectionData(key.c_str(), m_aModulationType[modulation].OuterFECRate, &EnumSettingValue::mapBinaryConvolutionCodeRate);

		// �V���{�����[�g
		key = prefix + L"SymbolRate";
		m_aModulationType[modulation].SymbolRate
			= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aModulationType[modulation].SymbolRate);

		// �ш敝(MHz)
		key = prefix + L"BandWidth";
		m_aModulationType[modulation].BandWidth
			= (long)IniFileAccess.ReadKeyISectionData(key.c_str(), m_aModulationType[modulation].BandWidth);
	}

	//
	// Channel �Z�N�V����
	//

	// ini�t�@�C������CH�ݒ��Ǎ��ލۂɎg�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
	// [Channel]�Z�N�V�����ł̒�` ... �S�Ẵ`���[�j���O��Ԃɉe��
	BOOL bReserveUnusedChGlobal = IniFileAccess.ReadKeyB(L"CHANNEL", L"ReserveUnusedCh", FALSE);

	// �`���[�j���O���00�`99�̐ݒ��Ǎ�
	for (DWORD space = 0; space < 100; space++)	{
		std::wstring section = common::WStringPrintf(L"TUNINGSPACE%02d", space);
		if (IniFileAccess.ReadSection(section.c_str()) <= 0) {
			// TuningSpaceXX�̃Z�N�V���������݂��Ȃ��ꍇ
			if (space != 0)
				continue;
			// TuningSpace00�̎���Channel�Z�N�V����������
			IniFileAccess.ReadSection(L"CHANNEL");
		}
		IniFileAccess.CreateSectionData();

		// ini�t�@�C������CH�ݒ��Ǎ��ލۂɎg�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
		// [TuningSpaceXX]�Z�N�V�����ł̒�` ... ���݂̃`���[�j���O��Ԃɉe��
		BOOL bReserveUnusedCh = IniFileAccess.ReadKeyBSectionData(L"ReserveUnusedCh", bReserveUnusedChGlobal);

		// ���Ƀ`���[�j���O��ԃf�[�^�����݂���ꍇ�͂��̓��e������������
		// �����ꍇ�͋�̃`���[�j���O��Ԃ��쐬
		auto itSpace = m_TuningData.Spaces.find(space);
		if (itSpace == m_TuningData.Spaces.end()) {
			itSpace = m_TuningData.Spaces.emplace(space, TuningSpaceData()).first;
		}

		// Tuning Space��
		std::wstring temp;
		if (space == 0)
			temp = sTempTuningSpaceName;
		else
			temp = L"NoName";
		
		itSpace->second.sTuningSpaceName = common::WStringToTString(IniFileAccess.ReadKeySSectionData(L"TuningSpaceName", temp.c_str()));

		// ini�t�@�C����1�̃`���[�j���O��ԂŒ�`�ł���ő�ChannelGenerate�̐�
		static constexpr unsigned int MAX_CH_GENERATE = 100;

		// �`�����l�����������^�C�v
		enum enumChGenerate {
			eChGenerateNone = 0,
			eChGenerateVHF_L = 1,				// VHF 1ch�`3ch
			eChGenerateVHF_H = 2,				// VHF 4ch�`12ch
			eChGenerateUHF = 3,					// UHF 13ch�`62ch
			eChGenerateCATV_L = 4,				// CATV C13ch�`C22ch
			eChGenerateCATV_H = 5,				// CATV C23ch�`C63ch
			eChGenerateVHF_4Plus = 8,			// VHF 4ch+
			eChGenerateCATV_22Plus = 9,			// CATV C22ch+
			eChGenerateCATV_24Plus = 10,		// CATV C24ch+�`C27ch+
			eChGenerateBS1 = 16,				// BS-R BS1,BS3,BS5...�`BS23
			eChGenerateND2 = 20,				// CS110-R ND2,ND4,ND6...�`ND24
			eChGenerateJD1 = 32,				// JCSAT-3A/JCSAT-4B JD1�`JD16
			eChGenerateJD17A = 33,				// JCSAT-3A JD17�`JD28
			eChGenerateJD17B = 34,				// JCSAT-4B JD17�`JD32
			eChGenerateOpticast = 64,			// SKY PerfecTV! Premium Service Hikari H001�`H058
			eChGenerateOpticast_11Plus = 72,	// SKY PerfecTV! Premium Service Hikari H011+�`H012+
			eChGenerateOpticast_26Plus = 73,	// SKY PerfecTV! Premium Service Hikari H026+�`H028+
		};

		// �`�����l�����������p�p�����[�^
		struct ChGenerate {
			enumChGenerate Space;
			unsigned int Offset;			// �J�n�I�t�Z�b�g
			unsigned int Count;				// �쐬CH��
			unsigned int RelativeTS;		// ����TS�쐬��
			unsigned int ModulationNumber;	// �ϒ������ԍ�
			unsigned int SatelliteNumber;	// �q���ԍ�
			unsigned int StartCh;			// �擪�`�����l���ԍ�
			unsigned int TuningFreq;		// �`���[�j���O���g���I�t�Z�b�g
			std::wstring NameFormat;		// �`�����l�����t�H�[�}�b�g
			unsigned int NameOffset;		// �`�����l�����Ɏg�p����`�����l���ԍ��I�t�Z�b�g
			unsigned int NameStep;			// �`�����l�����Ɏg�p����`�����l���ԍ��X�e�b�v
			unsigned int NameOffsetTS;		// �`�����l�����Ɏg�p����TS�ԍ��I�t�Z�b�g
			ChGenerate(void)
				: Space(eChGenerateNone),
				Offset(0),
				Count(0),
				RelativeTS(0),
				ModulationNumber(0),
				SatelliteNumber(0),
				StartCh(0),
				TuningFreq(0),
				NameOffset(0),
				NameStep(1),
				NameOffsetTS(0)
			{
			};
			~ChGenerate(void)
			{
			};
		};

		// �`�����l���̎���������`�s
		std::wstring ChannelGenerate[MAX_CH_GENERATE];

		// UHF/CATV����CH�ݒ��������������
		std::wstring chAuto = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(L"ChannelSettingsAuto", L""));
		std::wstring chAutoOpt = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(L"ChannelSettingsAutoOptions", L""));
		BOOL bOptVHFPlus = FALSE;
		BOOL bOptC24Plus = FALSE;
		BOOL bOptOnly64QAM = FALSE;
		BOOL bOptOnly256QAM = FALSE;
		BOOL bOptH11Plus = FALSE;
		BOOL bOptH26Plus = FALSE;
		BOOL bOptOnlySD = FALSE;
		BOOL bOptOnlyHD = FALSE;
		BOOL bOptSpinel = FALSE;
		BOOL bOptAll = FALSE;
		std::wstring sOptRelativeTS = L"";
		std::wstring sOptRelativeTS64QAM = L"";
		{
			// �J���}��؂��7�ɕ���
			std::wstring t(chAutoOpt);
			for (int n = 0; n < 10; n++) {
				std::wstring opt;
				std::wstring::size_type pos = common::WStringSplit(&t, L',', &opt);
				if (opt == L"VHF+") {
					bOptVHFPlus = TRUE;
				}
				else if (opt == L"C24+") {
					bOptC24Plus = TRUE;
				}
				else if (opt == L"ONLY64QAM") {
					bOptOnly64QAM = TRUE;
				}
				else if (opt == L"ONLY256QAM") {
					bOptOnly256QAM = TRUE;
				}
				else if (opt == L"H011+") {
					bOptH11Plus = TRUE;
				}
				else if (opt == L"H026+") {
					bOptH26Plus = TRUE;
				}
				else if (opt == L"ONLYSD") {
					bOptOnlySD = TRUE;
				}
				else if (opt == L"ONLYHD") {
					bOptOnlyHD = TRUE;
				}
				else if (opt == L"SPINEL") {
					bOptSpinel = TRUE;
				}
				else if (opt == L"ALL") {
					bOptAll = TRUE;
				}
				else if (opt.substr(0, 11) == L"RELATIVETS:") {
					sOptRelativeTS = opt.substr(11);
				}
				else if (opt.substr(0, 18) == L"RELATIVETS-256QAM:") {
					sOptRelativeTS = opt.substr(18);
				}
				else if (opt.substr(0, 17) == L"RELATIVETS-64QAM:") {
					sOptRelativeTS64QAM = opt.substr(17);
				}
				if (pos == std::wstring::npos)
					break;
			}

		}
		if (chAuto == L"UHF") {
			if (bOptAll) {
				ChannelGenerate[0] = L"UHF," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[0] = L"UHF:0:40," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"CATV") {
			int num = 0;
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"CATV-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-L:0:9," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll || !bOptC24Plus) {
				ChannelGenerate[num++] = L"CATV-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-H:0:1," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-H:5," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"PASSTHROUGH") {
			int num = 0;
			ChannelGenerate[num++] = L"VHF-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"VHF-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"VHF-4+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"VHF-H:4," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			ChannelGenerate[num++] = L"UHF," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"CATV-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-L:0:9," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll || !bOptC24Plus) {
				ChannelGenerate[num++] = L"CATV-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-H:0:1," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-H:5," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"VHF-4+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"TRANSMODULATION") {
			std::wstring sRelative64;
			std::wstring sRelative256;
			if (sOptRelativeTS64QAM != L"") {
				sRelative64 = L":" + sOptRelativeTS64QAM;
			}
			if (sOptRelativeTS != L"") {
				sRelative256 = L":" + sOptRelativeTS;
			}
			int num = 0;
			unsigned int startCh = 0;
			if (!bOptOnly256QAM) {
				ChannelGenerate[num++] = L"VHF-L::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",," + std::to_wstring(startCh) + L",0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"VHF-H::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"VHF-H:4:" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				ChannelGenerate[num++] = L"UHF::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"CATV-L::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-L:0:9" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				if (bOptAll || !bOptC24Plus) {
					ChannelGenerate[num++] = L"CATV-H::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-H:0:1" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-H:5:" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				startCh += max(common::WStringToLong(sOptRelativeTS64QAM), 1) * (12 + 50 + 51);
				if (bOptAll) {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					startCh += max(common::WStringToLong(sOptRelativeTS64QAM), 1) * (3 + 1 + 4);
				}
				startCh = ((startCh + 99) / 100) * 100;
			}
			if (!bOptOnly64QAM) {
				ChannelGenerate[num++] = L"VHF-L::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",," + std::to_wstring(startCh) + L",0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"VHF-H::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"VHF-H:4:" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				ChannelGenerate[num++] = L"UHF::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"CATV-L::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-L:0:9" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				if (bOptAll || !bOptC24Plus) {
					ChannelGenerate[num++] = L"CATV-H::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-H:0:1" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-H:5:" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				if (bOptAll) {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
			}
		}
		else if (chAuto == L"BS") {
			std::wstring sRelative;
			if (sOptRelativeTS != L"") {
				sRelative = L":" + sOptRelativeTS;
			}
			int num = 0;
			ChannelGenerate[num++] = L"BS1::" + sRelative + L"," + std::to_wstring(modulationNumberISDBS) + L"," + std::to_wstring(satelliteNumberBSCS110R) + L",,,BS%02d/TS%d,1,2,0";
		}
		else if (chAuto == L"CS110") {
			int num = 0;
			ChannelGenerate[num++] = L"ND2," + std::to_wstring(modulationNumberISDBS) + L"," + std::to_wstring(satelliteNumberBSCS110R) + L",,,ND%02d/TS0,2,2";
		}
		else if (chAuto == L"SPHD") {
		std::wstring sName4 = bOptSpinel ? L"JCSAT4A-TP%02d" : L"JCSAT4B-TP%02d";
		int num = 0;
			unsigned int startCh = 1;
			if (!bOptOnlyHD) {
				ChannelGenerate[num++] = L"JD17A," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L"," + std::to_wstring(startCh) + L",,JCSAT3A-TP%02d,1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L",,,JCSAT3A-TP%02d,13";
				startCh += 100;
				ChannelGenerate[num++] = L"JD17B," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L"," + std::to_wstring(startCh) + L",," + sName4 + L",1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L",,," + sName4 + L",17";
				startCh += 100;
			}
			if (!bOptOnlySD) {
				ChannelGenerate[num++] = L"JD17A," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L"," + std::to_wstring(startCh) + L",,JCSAT3A-TP%02d,1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L",,,JCSAT3A-TP%02d,13";
				startCh += 100;
				ChannelGenerate[num++] = L"JD17B," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L"," + std::to_wstring(startCh) + L",," + sName4 + L",1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L",,," + sName4 + L",17";
			}
		}
		else if (chAuto == L"OPTICAST") {
			int num = 0;
			if (bOptAll || (!bOptH11Plus && !bOptH26Plus)) {
				ChannelGenerate[num++] = L"OPTICAST," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			else if (bOptH11Plus && !bOptH26Plus) {
				ChannelGenerate[num++] = L"OPTICAST:0:10," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:12," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			else if (!bOptH11Plus && bOptH26Plus) {
				ChannelGenerate[num++] = L"OPTICAST:0:25," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:28," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			} else {
				ChannelGenerate[num++] = L"OPTICAST:0:10," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:12:13," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:28," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
		}

		unsigned int nextCh = 0;	// �����擪�`�����l���ԍ�

		// ini�t�@�C������̓Ǎ�
		for (int i = 0; i < MAX_CH_GENERATE; i++) {
			ChGenerate Generate;
			std::wstring key = L"ChannelGenerate" + std::to_wstring(i);
			ChannelGenerate[i] = IniFileAccess.ReadKeySSectionData(key.c_str(), ChannelGenerate[i].c_str());
			if (ChannelGenerate[i].length() == 0)
				break;
			// �J���}��؂��9�ɕ���
			std::wstring t(ChannelGenerate[i]);
			std::wstring buf[9];
			for (int n = 0; n < 9; n++) {
				if (std::wstring::npos == common::WStringSplit(&t, L',', &buf[n]))
					break;
			}
			// �쐬���:�J�n�I�t�Z�b�g:�쐬CH��:����TS�쐬�� �𕪉�
			std::wstring t2(buf[0]);
			std::wstring buf2[4];
			for (int n = 0; n < 4; n++) {
				if (std::wstring::npos == common::WStringSplit(&t2, L':', &buf2[n]))
					break;
			}
			std::wstring genSpace = common::WStringToUpperCase(buf2[0]);
			unsigned int offset = common::WStringToLong(buf2[1]);
			unsigned int count = common::WStringToLong(buf2[2]);
			if (count < 1) {
				count = 999;
			}
			unsigned int relativeTS = 0;
			std::wstring postfix = L"/TS%d";
			if (genSpace == L"VHF-L") {
				Generate.Space = eChGenerateVHF_L;
				Generate.Offset = min(offset, 3UL - 1UL);
				Generate.Count = min(count, 3UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"VHF-H") {
				Generate.Space = eChGenerateVHF_H;
				Generate.Offset = min(offset, 9UL - 1UL);
				Generate.Count = min(count, 9UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 4UL;
			}
			else if (genSpace == L"VHF-4+") {
				Generate.Space = eChGenerateVHF_4Plus;
				Generate.Offset = min(offset, 4UL - 1UL);
				Generate.Count = min(count, 4UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch+";
				Generate.NameOffset = 4UL;
			}
			else if (genSpace == L"UHF") {
				Generate.Space = eChGenerateUHF;
				Generate.Offset = min(offset, 50UL - 1UL);
				Generate.Count = min(count, 50UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 13UL;
			}
			else if (genSpace == L"CATV-L") {
				Generate.Space = eChGenerateCATV_L;
				Generate.Offset = min(offset, 10UL - 1UL);
				Generate.Count = min(count, 10UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch";
				Generate.NameOffset = 13UL;
			}
			else if (genSpace == L"CATV-22+") {
				Generate.Space = eChGenerateCATV_22Plus;
				Generate.Offset = min(offset, 1UL - 1UL);
				Generate.Count = min(count, 1UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch+";
				Generate.NameOffset = 22UL;
			}
			else if (genSpace == L"CATV-H") {
				Generate.Space = eChGenerateCATV_H;
				Generate.Offset = min(offset, 41UL - 1UL);
				Generate.Count = min(count, 41UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch";
				Generate.NameOffset = 23UL;
			}
			else if (genSpace == L"CATV-24+") {
				Generate.Space = eChGenerateCATV_24Plus;
				Generate.Offset = min(offset, 4UL - 1UL);
				Generate.Count = min(count, 4UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch+";
				Generate.NameOffset = 24UL;
			}
			else if (genSpace == L"BS1") {
				Generate.Space = eChGenerateBS1;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"BS%02d/TS%d";
				postfix = L"";
				Generate.NameOffset = 1UL;
				Generate.NameStep = 2UL;
				Generate.NameOffsetTS = 0UL;
				relativeTS = 3UL;
			}
			else if (genSpace == L"ND2") {
				Generate.Space = eChGenerateND2;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"ND%02d/TS%d";
				postfix = L"";
				Generate.NameOffset = 2UL;
				Generate.NameStep = 2UL;
				Generate.NameOffsetTS = 0UL;
			}
			else if (genSpace == L"JD17A") {
				Generate.Space = eChGenerateJD17A;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 17UL;
			}
			else if (genSpace == L"JD17B") {
				Generate.Space = eChGenerateJD17B;
				Generate.Offset = min(offset, 16UL - 1UL);
				Generate.Count = min(count, 16UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 17UL;
			}
			else if (genSpace == L"JD1") {
				Generate.Space = eChGenerateJD1;
				Generate.Offset = min(offset, 16UL - 1UL);
				Generate.Count = min(count, 16UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"OPTICAST") {
				Generate.Space = eChGenerateOpticast;
				Generate.Offset = min(offset, 58UL - 1UL);
				Generate.Count = min(count, 58UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"OPTICAST-11+") {
				Generate.Space = eChGenerateOpticast_11Plus;
				Generate.Offset = min(offset, 2UL - 1UL);
				Generate.Count = min(count, 2UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d+";
				Generate.NameOffset = 11UL;
			}
			else if (genSpace == L"OPTICAST-26+") {
				Generate.Space = eChGenerateOpticast_26Plus;
				Generate.Offset = min(offset, 3UL - 1UL);
				Generate.Count = min(count, 3UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d+";
				Generate.NameOffset = 26UL;
			}
			// ����TS�쐬��
			if (buf2[3] != L"") {
				Generate.RelativeTS = common::WStringToLong(buf2[3]);
				Generate.NameFormat += postfix;
			}
			else {
				Generate.RelativeTS = relativeTS;
			}

			// �ϒ������ԍ�
			if (buf[1] != L"") {
				Generate.ModulationNumber = common::WStringToLong(buf[1]);
			}
			// �q���ԍ�
			if (buf[2] != L"") {
				Generate.SatelliteNumber = common::WStringToLong(buf[2]);
			}
			// �擪�`�����l���ԍ�
			if (buf[3] != L"") {
				Generate.StartCh = common::WStringToLong(buf[3]);
			}
			else {
				Generate.StartCh = nextCh;
			}
			// �`���[�j���O���g���I�t�Z�b�g
			if (buf[4] != L"") {
				Generate.TuningFreq = common::WStringToLong(buf[4]);
			}
			// �`�����l�����t�H�[�}�b�g
			if (buf[5] != L"") {
				Generate.NameFormat = buf[5];
			}
			// �`�����l�����I�t�Z�b�g
			if (buf[6] != L"") {
				Generate.NameOffset = common::WStringToLong(buf[6]);
			}
			// �`�����l�����X�e�b�v
			if (buf[7] != L"") {
				Generate.NameStep = common::WStringToLong(buf[7]);
			}
			// TS�ԍ��I�t�Z�b�g
			if (buf[8] != L"") {
				Generate.NameOffsetTS = common::WStringToLong(buf[8]);
			}

			unsigned int freqBase = 0UL;			// �擪�`�����l���̎��g��
			unsigned int freqStep = 0UL;			// ���g���X�e�b�v
			unsigned int polarisastionType = 0UL;	// �Δg�^�C�v 0 .. �g�p���Ȃ�, 1 .. V/H�̌J��Ԃ�, 8 .. R�Œ�

			switch (Generate.Space) {
			case eChGenerateVHF_L:
				freqBase = 93000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateVHF_H:
				freqBase = 171000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateVHF_4Plus:
				freqBase = 173000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateUHF:
				freqBase = 473000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_L:
				freqBase = 111000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_22Plus:
				freqBase = 167000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_H:
				freqBase = 225000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_24Plus:
				freqBase = 233000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateBS1:
				freqBase = 11727480UL;
				freqStep = 38360UL;
				polarisastionType = 8UL;
				break;
			case eChGenerateND2:
				freqBase = 12291000UL;
				freqStep = 40000UL;
				polarisastionType = 8UL;
				break;
			case eChGenerateJD1:
				freqBase = 12508000UL;
				freqStep = 15000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateJD17A:
				freqBase = 12268000UL;
				freqStep = 20000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateJD17B:
				freqBase = 12268000UL;
				freqStep = 15000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateOpticast:
				freqBase = 93250UL;
				freqStep = 6500UL;
				break;
			case eChGenerateOpticast_11Plus:
				freqBase = 159250UL;
				freqStep = 6500UL;
				break;
			case eChGenerateOpticast_26Plus:
				freqBase = 259750UL;
				freqStep = 6500UL;
				break;
			}

			if (Generate.Count && freqBase) {
				bReserveUnusedCh = TRUE;
				unsigned int tsCount = max(Generate.RelativeTS, 1);
				unsigned int chNum = Generate.StartCh;
				for (unsigned int ch = Generate.Offset; ch < Generate.Offset + Generate.Count; ch++) {
					for (unsigned int ts = 0; ts < tsCount; ts++) {
						auto itCh = itSpace->second.Channels.find(chNum);
						if (itCh == itSpace->second.Channels.end()) {
							itCh = itSpace->second.Channels.emplace(chNum, ChData()).first;
						}
						else {
							OutputDebug(L"    Replaced to :\n");
						}
						switch (polarisastionType) {
						case 1UL:
							itCh->second.Polarisation = (ch % 2UL) ? 1UL : 2UL;
							break;
						case 8UL:
							itCh->second.Polarisation = 4UL;
							break;
						default:
							itCh->second.Polarisation = 0UL;
							break;
						}
						itCh->second.ModulationType = Generate.ModulationNumber;
						itCh->second.Satellite = Generate.SatelliteNumber;
						itCh->second.Frequency = freqBase + freqStep * ch + Generate.TuningFreq;
						if (Generate.RelativeTS) {
							itCh->second.TSID = ts;
						}
						itCh->second.sServiceName = common::TStringPrintf(common::WStringToTString(Generate.NameFormat).c_str(), ch * Generate.NameStep + Generate.NameOffset, ts + Generate.NameOffsetTS);
						OutputDebug(L"%s: (auto) CH%03ld=%ld,%ld.%03ld,%c,%ld,%s,%ld,%ld,%ld,%ld,%ld\n", section.c_str(), itCh->first, itCh->second.Satellite, itCh->second.Frequency / 1000L,
							itCh->second.Frequency % 1000L, PolarisationChar[itCh->second.Polarisation], itCh->second.ModulationType, itCh->second.sServiceName.c_str(), itCh->second.SID, 
							itCh->second.TSID, itCh->second.ONID, itCh->second.MajorChannel, itCh->second.SourceID);
						chNum++;
						nextCh = max(nextCh, chNum);
					}
				}
				OutputDebug(L".\n");
			}
		}

		// ���g���I�t�Z�b�g�l
		itSpace->second.FrequencyOffset = (long)IniFileAccess.ReadKeyISectionData(L"FrequencyOffset", itSpace->second.FrequencyOffset);

		// TuningSpace�̎�ޔԍ�
		itSpace->second.DVBSystemTypeNumber = IniFileAccess.ReadKeyISectionData(L"DVBSystemTypeNumber", itSpace->second.DVBSystemTypeNumber);

		// TSMF�̏������[�h
		itSpace->second.TSMFMode = (EnumSettingValue::TSMFMode)IniFileAccess.ReadKeyIValueMapSectionData(L"TSMFMode", (int)itSpace->second.TSMFMode, &EnumSettingValue::mapTSMFMode);
		if (itSpace->second.TSMFMode != EnumSettingValue::TSMFMode::Off) {
			m_bNeedTSMFParser = TRUE;
			m_bNeedDecodeProc = TRUE;
		}

		// CH�ݒ�
		//    �`�����l���ԍ� = �q���ԍ�,���g��,�Δg,�ϒ�����[,�`�����l����[,SID/MinorChannel[,TSID/Channel[,ONID/PhysicalChannel[,MajorChannel[,SourceID]]]]]]
		//    ��: CH001 = 1,12658,V,0
		//      �`�����l���ԍ� : CH000�`CH999�Ŏw��
		//      �q���ԍ�       : Satellite�Z�N�V�����Őݒ肵���q���ԍ�(1�`4) �܂��� 0(���w�莞)
		//                       (�n�f�W�`���[�i�[����0���w�肵�Ă�������)
		//      ���g��         : ���g����MHz�Ŏw��
		//                       (�����_��t���邱�Ƃɂ��KHz�P�ʂł̎w�肪�\�ł�)
		//      �Δg           : 'V' = �����Δg 'H'=�����Δg 'L'=�����~�Δg 'R'=�E���~�Δg ' '=���w��
		//                       (�n�f�W�`���[�i�[���͖��w��)
		//      �ϒ�����       : Modulation�Z�N�V�����Őݒ肵���ϒ������ԍ�(0�`3)
		//      �`�����l����   : �`�����l������
		//                       (�ȗ������ꍇ�� 128.0E / 12658H / DVB - S �̂悤�Ȍ`���Ŏ�����������܂�)
		//      SID            : DVB / ISDB�̃T�[�r�XID
		//      TSID           : DVB / ISDB�̃g�����X�|�[�g�X�g���[��ID
		//      ONID           : DVB / ISDB�̃I���W�i���l�b�g���[�NID
		//      MinorChannel   : ATSC / Digital Cable��Minor Channel
		//      Channel        : ATSC / Digital Cable��Channel
		//      PhysicalChannel: ATSC / Digital Cable��Physical Channel
		//      MajorChannel   : Digital Cable��Major Channel
		//      SourceID       : Digital Cable��SourceID
		std::wstring key, data;
		while (IniFileAccess.ReadSectionData(&key, &data) == true) {
			std::wregex re(LR"(^CH(?:\d{3}|)$)", std::regex_constants::icase);
			if (std::regex_match(key, re) == true) {
				if (data.length() != 0) {
					// CH�ݒ�L��
					DWORD ch = common::WStringDecimalToLong(key.substr(2));

					// ReserveUnusedCh���w�肳��Ă���ꍇ��CH�ԍ����㏑������
					// �w�肳��Ă��Ȃ��ꍇ�͓o�^����CH�ԍ���U��
					DWORD chNum = bReserveUnusedCh ? ch : (DWORD)(itSpace->second.Channels.size());
					auto itCh = itSpace->second.Channels.find(chNum);
					if (itCh == itSpace->second.Channels.end()) {
						itCh = itSpace->second.Channels.emplace(chNum, ChData()).first;
					}

					// �J���}��؂��10�ɕ���
					std::wstring t(data);
					std::wstring buf[10];
					for (int n = 0; n < 10; n++) {
						if (std::wstring::npos == common::WStringSplit(&t, L',', &buf[n]))
							break;
					}

					// �q���ԍ�
					val = common::WStringToLong(buf[0]);
					if (val >= 0 && val < MAX_SATELLITE) {
						itCh->second.Satellite = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Bird.\n");

					// ���g��
					val = (int)(common::WstringToDouble(buf[1]) * 1000.0);
					if ((val > 0) && (val <= 30000000)) {
						itCh->second.Frequency = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Frequency.\n");

					// �Δg���
					WCHAR c = buf[2].c_str()[0];
					if (c == L'\0') {
						c = L' ';
					}
					auto it = std::find(std::begin(PolarisationChar), std::end(PolarisationChar), c);
					if (it != std::end(PolarisationChar)) {
						itCh->second.Polarisation = (unsigned int)(it - std::begin(PolarisationChar));
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Polarisation.\n");

					// �ϒ�����
					val = common::WStringToLong(buf[3]);
					if (val >= 0 && val < MAX_MODULATION) {
						itCh->second.ModulationType = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Method.\n");

					// �`�����l����
					std::basic_string<TCHAR> name(common::WStringToTString(buf[4]));
					if (name.length() == 0)
						// ini�t�@�C���Ŏw�肵�����̂��Ȃ����128.0E/12658H/DVB-S �̂悤�Ȍ`���ō쐬����
						name = MakeChannelName(&itCh->second);
					itCh->second.sServiceName = name;

					// SID / PhysicalChannel
					if (buf[5] != L"") {
						val = common::WStringToLong(buf[5]);
						itCh->second.SID = val;
					}

					// TSID / Channel
					if (buf[6] != L"") {
						val = common::WStringToLong(buf[6]);
						itCh->second.TSID = val;
					}

					// ONID / MinorChannel
					if (buf[7] != L"") {
						val = common::WStringToLong(buf[7]);
						itCh->second.ONID = val;
					}

					// MajorChannel
					if (buf[8] != L"") {
						val = common::WStringToLong(buf[8]);
						itCh->second.MajorChannel = val;
					}

					// SourceID
					if (buf[9] != L"") {
						val = common::WStringToLong(buf[9]);
						itCh->second.SourceID = val;
					}
					OutputDebug(L"%s: (manual) CH%03ld=%ld,%ld.%03ld,%c,%ld,%s,%ld,%ld,%ld,%ld,%ld\n", section.c_str(), itCh->first, itCh->second.Satellite, itCh->second.Frequency / 1000L,
						itCh->second.Frequency % 1000L, PolarisationChar[itCh->second.Polarisation], itCh->second.ModulationType, itCh->second.sServiceName.c_str(), itCh->second.SID,
						itCh->second.TSID, itCh->second.ONID, itCh->second.MajorChannel, itCh->second.SourceID);
				}
			}
		}

		//�`�����l����`�̐�
		auto itChEnd = itSpace->second.Channels.end();
		if (itChEnd == itSpace->second.Channels.begin()) {
			// �`�����l����`��1���Ȃ�
			itSpace->second.dwNumChannel = 0;
		}
		else {
			// CH�ԍ��̍ő�l + 1
			itChEnd--;
			itSpace->second.dwNumChannel = itChEnd->first + 1;
		}
		OutputDebug(L"%s: dwNumChannel = %d.\n", section.c_str(), itSpace->second.dwNumChannel);

		// CH�ؑ֓���������I��2�x�s���ꍇ�̑Ώ�CH
		if (m_bLockTwice) {
			std::wstring s = IniFileAccess.ReadKeySSectionData(L"ChannelLockTwiceTarget", L"");
			if (s != L"") {
				while (1) {
					// �J���}��؂�܂ł̕�������擾
					std::wstring token;
					std::wstring::size_type pos = common::WStringSplit(&s, L',', &token);
					if (token != L"") {
						DWORD begin = 0;
						DWORD end = itSpace->second.dwNumChannel - 1;
						// �����'-'��؂�̐��l�ɕ���
						std::wstring left;
						std::wstring right(token);
						if (std::wstring::npos == common::WStringSplit(&right, L'-', &left)) {
							// "-"�L��������
							begin = end = common::WStringToLong(left);
						}
						else {
							// "-"�L�����L��
							if (left != L"") {
								// "-"�L���̑O�ɐ��l������
								begin = common::WStringToLong(left);
							}
							if (right != L"") {
								// "-"�L���̌�ɐ��l������
								end = common::WStringToLong(right);
							}
						}
						// �Ώ۔͈͂�CH��Flag��Set����
						for (DWORD ch = begin; ch <= end; ch++) {
							auto itCh = itSpace->second.Channels.find(ch);
							if (itCh != itSpace->second.Channels.end()) {
								itCh->second.LockTwiceTarget = TRUE;
							}
						}
					}
					if (pos == std::wstring::npos)
						break;
				}
			}
			else {
				// ChannelLockTwiceTarget�̎w�肪�����ꍇ�͂��ׂĂ�CH���Ώ�
				for (auto itCh = itSpace->second.Channels.begin(); itCh != itSpace->second.Channels.end(); itCh++) {
					itCh->second.LockTwiceTarget = TRUE;
				}
			}
		}
	}

	// �`���[�j���O��Ԕԍ�0��T��
	auto itSpace0 = m_TuningData.Spaces.find(0);
	if (itSpace0 == m_TuningData.Spaces.end()) {
		// �����ɂ͗��Ȃ��͂������ǈꉞ
		// ���TuningSpaceData���`���[�j���O��Ԕԍ�0�ɑ}��
		itSpace0 = m_TuningData.Spaces.emplace(0, TuningSpaceData()).first;
	}

	if (!itSpace0->second.Channels.size()) {
		// CH��`���������Ă��Ȃ�
		if (m_nDefaultNetwork == EnumSettingValue::DefaultNetwork::SPHD) {
			// SPHD�̏ꍇ�̂݉ߋ��̃o�[�W�����݊�����
			// 3��TP���f�t�H���g�ŃZ�b�g���Ă���
			//   128.0E 12.658GHz V DVB-S *** 2015-10-10���݁ANIT�ɂ͑��݂��邯�ǒ�g��
			auto itCh = itSpace0->second.Channels.emplace(0, ChData()).first;
			itCh->second.Satellite = 1;
			itCh->second.Polarisation = 2;
			itCh->second.ModulationType = 0;
			itCh->second.Frequency = 12658000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			//   124.0E 12.613GHz H DVB-S2
			itCh = itSpace0->second.Channels.emplace(1, ChData()).first;
			itCh->second.Satellite = 2;
			itCh->second.Polarisation = 1;
			itCh->second.ModulationType = 1;
			itCh->second.Frequency = 12613000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			//   128.0E 12.733GHz H DVB-S2
			itCh = itSpace0->second.Channels.emplace(2, ChData()).first;
			itCh->second.Satellite = 1;
			itCh->second.Polarisation = 1;
			itCh->second.ModulationType = 1;
			itCh->second.Frequency = 12733000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			itSpace0->second.dwNumChannel = 3;
		}
	}

	// �`���[�j���O��Ԃ̐�
	auto itSpaceEnd = m_TuningData.Spaces.end();
	if (itSpaceEnd == m_TuningData.Spaces.begin()) {
		// ���������ꉞ
		m_TuningData.dwNumSpace = 0;
	}
	else {
		itSpaceEnd--;
		m_TuningData.dwNumSpace = itSpaceEnd->first + 1;
	}
}

HRESULT CBonTuner::GetSignalState(double* pdbStrength, double* pdbQuality, double* pdbLocked, double* pdbPresent)
{
	if (pdbStrength)
		*pdbStrength = 0.0;
	if (pdbQuality)
		*pdbQuality = 0.0;
	if (pdbLocked)
		*pdbLocked = 1.0;
	if (pdbPresent)
		*pdbPresent = 1.0;

	HRESULT hr;
	// �`���[�i�ŗL GetSignalState ������΁A�ۓ���
	int nStrength = 0;
	int nQuality = 0;
	int nLock = 1;
	if ((m_pIBdaSpecials) && (hr = m_pIBdaSpecials->GetSignalState(&nStrength, &nQuality, &nLock)) != E_NOINTERFACE) {
		if (pdbStrength)
			*pdbStrength = (double)nStrength;
		if (pdbQuality)
			*pdbQuality = (double)nQuality;
		if (pdbLocked)
			*pdbLocked = (double)nLock;
		return S_OK;
	}

	if (m_pTunerDevice == NULL)
		return E_POINTER;

	BOOL iTunerStrength = m_bSignalLevelGetTypeTuner && m_bSignalLevelNeedStrength && pdbStrength;
	BOOL iTunerQuality = m_bSignalLevelGetTypeTuner && m_bSignalLevelNeedQuality && pdbQuality;
	BOOL iTunerLocked = m_bSignalLockedJudgeTypeTuner && pdbLocked;
	BOOL needITuner = iTunerStrength || iTunerQuality || iTunerLocked;
	BOOL tunerSSStrength = m_bSignalLevelGetTypeSS && m_bSignalLevelNeedStrength && pdbStrength;
	BOOL tunerSSQuality = m_bSignalLevelGetTypeSS && m_bSignalLevelNeedQuality && pdbQuality;
	BOOL tunerSSLocked = m_bSignalLockedJudgeTypeSS && pdbLocked;
	BOOL tunerSSPresent = m_bSignalLockedJudgeTypeSS && pdbPresent;
	BOOL demodSSStrength = m_bSignalLevelGetTypeDemodSS && m_bSignalLevelNeedStrength && pdbStrength;
	BOOL demodSSQuality = m_bSignalLevelGetTypeDemodSS && m_bSignalLevelNeedQuality && pdbQuality;
	BOOL demodSSLocked = m_bSignalLockedJudgeTypeDemodSS && pdbLocked;
	BOOL demodSSPresent = m_bSignalLockedJudgeTypeDemodSS && pdbPresent;

	if (m_pITuner) {
		if (needITuner) {
			long val = 0;
			if (SUCCEEDED(hr = m_pITuner->get_SignalStrength(&val))) {
				// ����16�r�b�g(�����t)��Signal�l�Ƃ��Ď��o��
				short strength = (short)(val & 0xffffL);
				// ���16�r�b�g��Quality�l�Ƃ��Ď��o��
				short quality = (short)(val >> 16);
				if (iTunerStrength)
					// ���̒l/0/-1�̏ꍇ�͂��̂܂ܕԂ�
					// -1�ȉ��̏ꍇ�͂��̒l��2�̕␔�Ƃ��Đ��̒l�ɕϊ����ĕԂ�
					*pdbStrength = (double)(strength < -1i16 ? -1i16 - strength : strength);
				if (iTunerQuality)
					// 0�`100�͈͓̔��ŕԂ�
					*pdbQuality = (double)min(max(quality, 0i16), 100i16);
				if (iTunerLocked)
					// Strength�l��0�ȊO�Ȃ��1.0(Lock���)��Ԃ�
					*pdbLocked = strength ? 1.0 : 0.0;
			}
		}
	}

	if (m_pIBDA_SignalStatisticsTunerNode) {
		if (tunerSSStrength) {
			LONG val = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalStrength(&val))) {
				// �����t16�r�b�g�l�Ƃ��Ď��o��
				short strength = (short)(val & 0xffffL);
				// �}�C�i�X�l�̏ꍇ�����̂܂ܕԂ�
				*pdbStrength = (double)strength;
			}
		}

		if (tunerSSQuality) {
			LONG val = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalQuality(&val))) {
				// �����t16�r�b�g�l�Ƃ��Ď��o��
				short quality = (short)(val & 0xffffL);
				// ���̂܂ܕԂ�
				*pdbQuality = (double)quality;
			}
		}

		if (tunerSSLocked) {
			BOOLEAN val = FALSE;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalLocked(&val))) {
				*pdbLocked = (double)val;
			}
		}

		if (tunerSSPresent) {
			BOOLEAN val = FALSE;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalPresent(&val))) {
				*pdbLocked = (double)val;
			}
		}
	}

	if (m_pIBDA_SignalStatisticsDemodNode) {
		if (demodSSStrength) {
			LONG val = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalStrength(&val))) {
				// �����t16�r�b�g�l�Ƃ��Ď��o��
				short strength = (short)(val & 0xffffL);
				// �}�C�i�X�l�̏ꍇ�����̂܂ܕԂ�
				*pdbStrength = (double)strength;
			}
		}

		if (demodSSQuality) {
			LONG val = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalQuality(&val))) {
				// �����t16�r�b�g�l�Ƃ��Ď��o��
				short quality = (short)(val & 0xffffL);
				// ���̂܂ܕԂ�
				*pdbQuality = (double)quality;
			}
		}

		if (demodSSLocked) {
			BOOLEAN val = FALSE;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalLocked(&val))) {
				*pdbLocked = (double)val;
			}
		}

		if (demodSSPresent) {
			BOOLEAN val = FALSE;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalPresent(&val))) {
				*pdbLocked = (double)val;
			}
		}
	}

	return S_OK;
}

BOOL CBonTuner::LockChannel(const TuningParam *pTuningParam)
{
	HRESULT hr;

	// �`���[�i�ŗL LockChannel ������΁A�ۓ���
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->LockChannel(pTuningParam)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna.Tone;
		if (SUCCEEDED(hr) && pTuningParam->LockTwice) {
			OutputDebug(L"  TwiceLock 1st[Special2] SUCCESS.\n");
			SleepWithMessageLoop(pTuningParam->LockTwiceDelay);
			hr = m_pIBdaSpecials2->LockChannel(pTuningParam);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  LockChannel[Special2] SUCCESS.\n");
			return TRUE;
		} else {
			OutputDebug(L"  LockChannel[Special2] FAIL.\n");
			return FALSE;
		}
	}

	if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna.Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
			(pTuningParam->Modulation.Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation.Modulation == BDA_MOD_8PSK) ? TRUE : FALSE)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna.Tone;
		if (SUCCEEDED(hr) && pTuningParam->LockTwice) {
			OutputDebug(L"  TwiceLock 1st[Special] SUCCESS.\n");
			SleepWithMessageLoop(pTuningParam->LockTwiceDelay);
			hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna.Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
					(pTuningParam->Modulation.Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation.Modulation == BDA_MOD_8PSK) ? TRUE : FALSE);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  LockChannel[Special] SUCCESS.\n");
			return TRUE;
		}
		else {
			OutputDebug(L"  LockChannel[Special] FAIL.\n");
			return FALSE;
		}
	}

	// �`���[�i�ŗL�g�[������֐�������΁A����������ŌĂяo��
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->Set22KHz(pTuningParam->Antenna.Tone)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  Set22KHz[Special2] successfully.\n");
			if (pTuningParam->Antenna.Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna.Tone;
				if (m_nToneWait) {
					SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
				}
			}
		}
		else {
			OutputDebug(L"  Set22KHz[Special2] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->Set22KHz(pTuningParam->Antenna.Tone ? 1 : 0)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  Set22KHz[Special] successfully.\n");
			if (pTuningParam->Antenna.Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna.Tone;
				if (m_nToneWait) {
					SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
				}
			}
		}
		else {
			OutputDebug(L"  Set22KHz[Special] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else {
		// �ŗL�֐����Ȃ������Ȃ̂ŁA��������
	}

	// ITuningSpace�p�����F
	//   ITuningSpace �� IDVBTuningSpace �� IDVBTuningSpace2 �� IDVBSTuningSpace
	//                �� IAnalogTVTuningSpace �� IATSCTuningSpace �� IDigitalCableTuningSpace
	//                �� IAnalogRadioTuningSpace �� IAnalogRadioTuningSpace2
	//                �� IAuxInTuningSpace �� IAuxInTuningSpace2
	OutputDebug(L"    ITuningSpace\n");
	// IDVBSTuningSpace���L
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(m_pITuningSpace);
		if (pIDVBSTuningSpace) {
			OutputDebug(L"    ->IDVBSTuningSpace\n");
			// LNB ���g����ݒ�
			if (pTuningParam->Antenna.HighOscillator != -1) {
				pIDVBSTuningSpace->put_HighOscillator(pTuningParam->Antenna.HighOscillator);
			}
			if (pTuningParam->Antenna.LowOscillator != -1) {
				pIDVBSTuningSpace->put_LowOscillator(pTuningParam->Antenna.LowOscillator);
			}

			// LNB�X�C�b�`�̎��g����ݒ�
			if (pTuningParam->Antenna.LNBSwitch != -1) {
				// LNBSwitch���g���̐ݒ肪����Ă���
				pIDVBSTuningSpace->put_LNBSwitch(pTuningParam->Antenna.LNBSwitch);
			}
			else {
				// 10GHz��ݒ肵�Ă�����High���ɁA20GHz��ݒ肵�Ă�����Low���ɐؑւ��͂�
				pIDVBSTuningSpace->put_LNBSwitch((pTuningParam->Antenna.Tone != 0) ? 10000000 : 20000000);
			}

			// �ʑ��ϒ��X�y�N�g�����]�̎��
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);
		}
	}

	// ILocator�擾
	//
	// ILocator�p�����F
	//   ILocator �� IDigitalLocator �� IDVBTLocator �� IDVBTLocator2
	//                               �� IDVBSLocator �� IDVBSLocator2
	//                                               �� IISDBSLocator
	//                               �� IDVBCLocator
	//                               �� IATSCLocator �� IATSCLocator2 �� IDigitalCableLocator
	//            �� IAnalogLocator
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = m_pITuningSpace->get_DefaultLocator(&pILocator))) {
		OutputDebug(L"  Fail to get ILocator. hr=0x%08lx\n", hr);
		return FALSE;
	}

	OutputDebug(L"    ILocator\n");
	// RF �M���̎��g����ݒ�
	pILocator->put_CarrierFrequency(pTuningParam->Frequency);

	// �����O���������̃^�C�v��ݒ�
	pILocator->put_InnerFEC(pTuningParam->Modulation.InnerFEC);

	// ���� FEC ���[�g��ݒ�
	// �O�������������Ŏg���o�C�i�� �R���{���[�V�����̃R�[�h ���[�g DVB-S�� 3/4 S2�� 3/5
	pILocator->put_InnerFECRate(pTuningParam->Modulation.InnerFECRate);

	// �ϒ��^�C�v��ݒ�
	// DVB-S��QPSK�AS2�̏ꍇ�� 8PSK
	pILocator->put_Modulation(pTuningParam->Modulation.Modulation);

	// �O���O���������̃^�C�v��ݒ�
	//	���[�h-�\������ 204/188 (�O�� FEC), DVB-S2�ł�����
	pILocator->put_OuterFEC(pTuningParam->Modulation.OuterFEC);

	// �O�� FEC ���[�g��ݒ�
	pILocator->put_OuterFECRate(pTuningParam->Modulation.OuterFECRate);

	// QPSK �V���{�� ���[�g��ݒ�
	pILocator->put_SymbolRate(pTuningParam->Modulation.SymbolRate);

	// IDVBSLocator���L
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			OutputDebug(L"    ->IDVBSLocator\n");
			// �M���̕Δg��ݒ�
			pIDVBSLocator->put_SignalPolarisation(pTuningParam->Polarisation);
		}
	}

	// IDVBSLocator2���L
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			OutputDebug(L"    ->IDVBSLocator2\n");
			// DiSEqC��ݒ�
			if (pTuningParam->Antenna.DiSEqC >= BDA_LNB_SOURCE_A) {
				pIDVBSLocator2->put_DiseqLNBSource((LNB_Source)(pTuningParam->Antenna.DiSEqC));
			}
			else {
				pIDVBSLocator2->put_DiseqLNBSource(BDA_LNB_SOURCE_NOT_SET);
			}
		}
	}

	// IDVBTLocator���L
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			OutputDebug(L"    ->IDVBTLocator\n");
			// ���g���̑ш敝 (MHz)��ݒ�
			if (pTuningParam->Modulation.BandWidth != -1) {
				pIDVBTLocator->put_Bandwidth(pTuningParam->Modulation.BandWidth);
			}
		}
	}

	// IATSCLocator���L
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			OutputDebug(L"    ->IATSCLocator\n");
			// ATSC PhysicalChannel
			if (pTuningParam->PhysicalChannel != -1) {
				pIATSCLocator->put_PhysicalChannel(pTuningParam->PhysicalChannel);
			}
		}
	}

	// ITuneRequest�쐬
	//
	// ITuneRequest�p�����F
	//   ITuneRequest �� IDVBTuneRequest
	//                �� IChannelTuneRequest �� IATSCChannelTuneRequest �� IDigitalCableTuneRequest
	//                �� IChannelIDTuneRequest
	//                �� IMPEG2TuneRequest
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"  Fail to create ITuneRequest. hr=0x%08lx\n", hr);
		return FALSE;
	}

	OutputDebug(L"    ITuneRequest\n");
	// ITuneRequest��ILocator��ݒ�
	hr = pITuneRequest->put_Locator(pILocator);

	// IDVBTuneRequest���L
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			OutputDebug(L"    ->IDVBTuneRequest\n");
			// DVB Triplet ID�̐ݒ�
			pIDVBTuneRequest->put_ONID(pTuningParam->ONID);
			pIDVBTuneRequest->put_TSID(pTuningParam->TSID);
			pIDVBTuneRequest->put_SID(pTuningParam->SID);
		}
	}

	// IChannelTuneRequest���L
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			OutputDebug(L"    ->IChannelTuneRequest\n");
			// ATSC Channel
			pIChannelTuneRequest->put_Channel(pTuningParam->Channel);
		}
	}

	// IATSCChannelTuneRequest���L
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			OutputDebug(L"    ->IATSCChannelTuneRequest\n");
			// ATSC MinorChannel
			pIATSCChannelTuneRequest->put_MinorChannel(pTuningParam->MinorChannel);
		}
	}

	// IDigitalCableTuneRequest���L
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			OutputDebug(L"    ->IDigitalCableTuneRequest\n");
			// Digital Cable MinorChannel
			pIDigitalCableTuneRequest->put_MajorChannel(pTuningParam->MinorChannel);
			// Digital Cable SourceID
			pIDigitalCableTuneRequest->put_SourceID(pTuningParam->SourceID);
		}
	}

	if (m_pIBdaSpecials2) {
		// �`���[�i�ŗL��TSID�֐�������ΌĂяo��
		hr = m_pIBdaSpecials2->SetTSid(pTuningParam->TSID);

		// m_pIBdaSpecials��put_TuneRequest�̑O�ɉ��炩�̏������K�v�Ȃ�s��
		hr = m_pIBdaSpecials2->PreTuneRequest(pTuningParam, pITuneRequest);
	}

	if (pTuningParam->Antenna.Tone != m_nCurTone && m_nToneWait) {
		//�g�[���ؑւ���̏ꍇ�A��Ɉ�xTuneRequest���Ă���
		OutputDebug(L"  Requesting pre tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put pre tune request.  hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  Pre tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}

		SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
	}
	m_nCurTone = pTuningParam->Antenna.Tone;

	if (pTuningParam->LockTwice) {
		// TuneRequest�������I��2�x�s��
		OutputDebug(L"  Requesting 1st twice tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put 1st twice tune request. hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  1st Twice tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}
		SleepWithMessageLoop(pTuningParam->LockTwiceDelay);
	}

	unsigned int nRetryRemain = m_nLockWaitRetry;
	double dbLock = 0;
	do {
		OutputDebug(L"  Requesting tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put tune request. hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  Tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}

		static constexpr int LockRetryTime = 50;
		unsigned int nWaitRemain = m_nLockWait;
		SleepWithMessageLoop(m_nLockWaitDelay);
		GetSignalState(NULL, NULL, &dbLock, NULL);
		while (!dbLock && nWaitRemain) {
			DWORD dwSleepTime = (nWaitRemain > LockRetryTime) ? LockRetryTime : nWaitRemain;
			OutputDebug(L"    Waiting lock status remaining %d msec.\n", nWaitRemain);
			SleepWithMessageLoop(dwSleepTime);
			nWaitRemain -= dwSleepTime;
			GetSignalState(NULL, NULL, &dbLock, NULL);
		}
	} while (!dbLock && nRetryRemain--);

	if (dbLock)
		OutputDebug(L"  LockChannel success.\n");
	else
		OutputDebug(L"  LockChannel failed.\n");

	return (BOOL)dbLock;
}

// �`���[�i�ŗLDll�̃��[�h
HRESULT CBonTuner::CheckAndInitTunerDependDll(IBaseFilter * pTunerDevice, std::wstring tunerGUID, std::wstring tunerFriendlyName)
{
	if (m_sDLLBaseName == L"") {
		// �`���[�i�ŗL�֐����g��Ȃ��ꍇ
		return S_OK;
	}

	// �t�H���_���擾
	std::wstring sPath;
	common::GetModuleFilePath(st_hModule, &sPath, NULL, NULL);
	std::wstring sDllName = sPath + m_sDLLBaseName + L".dll";

	if ((m_hModuleTunerSpecials = ::LoadLibraryW(sDllName.c_str())) == NULL) {
		// ���[�h�ł��Ȃ��ꍇ�A�ǂ�����? 
		//  �� �f�o�b�O���b�Z�[�W�����o���āA�ŗL�֐����g��Ȃ����̂Ƃ��Ĉ���
		OutputDebug(L"CheckAndInitTunerDependDll: DLL Not found.\n");
		return S_OK;
	} else {
		OutputDebug(L"CheckAndInitTunerDependDll: Load Library successfully.\n");
	}

	HRESULT (* func)(IBaseFilter *, const WCHAR *, const WCHAR *, const WCHAR *) =
		(HRESULT (*)(IBaseFilter *, const WCHAR *, const WCHAR *, const WCHAR *))::GetProcAddress(m_hModuleTunerSpecials, "CheckAndInitTuner");
	if (!func) {
		// �������R�[�h������
		// ���������s�v
		return S_OK;
	}

	return (* func)(pTunerDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), m_sIniFilePath.c_str());
}

// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
HRESULT CBonTuner::CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName)
{
	if (m_hModuleTunerSpecials == NULL) {
		return S_OK;
	}

	HRESULT (* func)(const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *) =
		(HRESULT (*)(const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *))::GetProcAddress(m_hModuleTunerSpecials, "CheckCapture");
	if (!func) {
		return S_OK;
	}

	return (* func)(tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str(), m_sIniFilePath.c_str());
}

// �`���[�i�ŗL�֐��̃��[�h
void CBonTuner::LoadTunerDependCode(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName)
{
	if (!m_hModuleTunerSpecials)
		return;

	IBdaSpecials* (*func)(CComPtr<IBaseFilter>);
	func = (IBdaSpecials* (*)(CComPtr<IBaseFilter>))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials");
	IBdaSpecials* (*func2)(CComPtr<IBaseFilter>, CComPtr<IBaseFilter>, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*);
	func2 = (IBdaSpecials * (*)(CComPtr<IBaseFilter>, CComPtr<IBaseFilter>, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials2");
	if (!func2 && !func) {
		OutputDebug(L"LoadTunerDependCode: Cannot find CreateBdaSpecials.\n");
		::FreeLibrary(m_hModuleTunerSpecials);
		m_hModuleTunerSpecials = NULL;
		return;
	}
	if (func2)
	{
		OutputDebug(L"LoadTunerDependCode: CreateBdaSpecials2 found.\n");
		m_pIBdaSpecials = func2(m_pTunerDevice, m_pCaptureDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str());
	}
	else {
		OutputDebug(L"LoadTunerDependCode: CreateBdaSpecials found.\n");
		m_pIBdaSpecials = func(m_pTunerDevice);
	}

	m_pIBdaSpecials2 = dynamic_cast<IBdaSpecials2b5 *>(m_pIBdaSpecials);
	if (!m_pIBdaSpecials2)
		OutputDebug(L"LoadTunerDependCode: Not IBdaSpecials2 Interface DLL.\n");

	//  BdaSpecials��ini�t�@�C����ǂݍ��܂���
	HRESULT hr;
	if (m_pIBdaSpecials2) {
		hr = m_pIBdaSpecials2->ReadIniFile(m_sIniFilePath.c_str());
	}

	// �`���[�i�ŗL�������֐��������Ŏ��s���Ă���
	if (m_pIBdaSpecials)
		m_pIBdaSpecials->InitializeHook();

	return;
}

// �`���[�i�ŗL�֐���Dll�̉��
void CBonTuner::ReleaseTunerDependCode(void)
{
	HRESULT hr;

	// �`���[�i�ŗL�֐�����`����Ă���΁A�����Ŏ��s���Ă���
	if (m_pIBdaSpecials) {
		if ((hr = m_pIBdaSpecials->FinalizeHook()) == E_NOINTERFACE) {
			// �ŗLFinalize�֐����Ȃ������Ȃ̂ŁA��������
		}
		else if (SUCCEEDED(hr)) {
			OutputDebug(L"ReleaseTunerDependCode: Tuner Special Finalize successfully.\n");
		}
		else {
			OutputDebug(L"ReleaseTunerDependCode: Tuner Special Finalize failed.\n");
		}

		SAFE_RELEASE(m_pIBdaSpecials);
		m_pIBdaSpecials2 = NULL;
	}

	if (m_hModuleTunerSpecials) {
		if (::FreeLibrary(m_hModuleTunerSpecials) == 0) {
			OutputDebug(L"ReleaseTunerDependCode: FreeLibrary failed.\n");
		}
		else {
			OutputDebug(L"ReleaseTunerDependCode: FreeLibrary Success.\n");
			m_hModuleTunerSpecials = NULL;
		}
	}
}

HRESULT CBonTuner::InitializeGraphBuilder(void)
{
	HRESULT hr = E_FAIL;
	
	// pIGraphBuilder interface���擾
	CComPtr<IGraphBuilder> pIGraphBuilder;
	if (FAILED(hr = pIGraphBuilder.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[InitializeGraphBuilder] Fail to get IGraphBuilder interface.\n");
	}
	else {
		// pIGraphBuilder interface�̎擾����
		// IMediaControl interface���擾
		CComQIPtr<IMediaControl> pIMediaControl(pIGraphBuilder);
		if (!pIMediaControl) {
			OutputDebug(L"[InitializeGraphBuilder] Fail to get IMediaControl interface.\n");
			hr = E_FAIL;
		}
		else {
			// �����Ȃ̂ł��̂܂܏I��
			m_pIGraphBuilder = pIGraphBuilder;
			m_pIMediaControl = pIMediaControl;
			return hr;
		}
	}

	// ���s
	return hr;
}

void CBonTuner::CleanupGraph(void)
{
	// DisconnectAll(m_pTif);
	// DisconnectAll(m_pDemux);
	// DisconnectAll(m_pTsWriter);
	// DisconnectAll(m_pCaptureDevice);
	// DisconnectAll(m_pTunerDevice);
	// DisconnectAll(m_pNetworkProvider);

	UnloadTif();
	UnloadDemux();
	UnloadTsWriter();

	// Tuner �� Capture �̏��� Release ���Ȃ���
	// ���������[�N���N�����f�o�C�X������
	UnloadTunerDevice();
	UnloadCaptureDevice();

	UnloadNetworkProvider();
	UnloadTuningSpace();

	m_pIMediaControl.Release();
	m_pIGraphBuilder.Release();

	return;
}

HRESULT CBonTuner::RunGraph(void)
{
	HRESULT hr;
	if (!m_pIMediaControl)
		return E_POINTER;

	SAFE_CLOSE_HANDLE(m_hStreamThread);
	m_bIsSetStreamThread = FALSE;

	if (FAILED(hr =  m_pIMediaControl->Run())) {
		return hr;
	}

	return S_OK;
}

void CBonTuner::StopGraph(void)
{
	HRESULT hr;
	if (m_pIMediaControl) {
		SAFE_CLOSE_HANDLE(m_hStreamThread);
		m_bIsSetStreamThread = FALSE;

		// a workaround for WinXP SP3
		// CBonTuner::LoadAndConnectDevice() �ɂē��삷��`���[�i������Ȃ������Ƃ��A
		// m_pIMediaControl->Stop() �̓����� MsDvbNp.ax �� access violation ���N�����B
		// �Ȃ̂ŁAStop ����K�v�̂Ȃ��Ƃ��͉������Ȃ��悤�ɂ���
		OAFilterState fs;
		if (FAILED(hr = m_pIMediaControl->GetState(100, &fs))) {
			OutputDebug(L"IMediaControl::GetState failed.\n");
		}
		else {
			if (fs == State_Stopped)
				return;
		}

		if (FAILED(hr = m_pIMediaControl->Pause())) {
			OutputDebug(L"IMediaControl::Pause failed.\n");
		}

		if (FAILED(hr = m_pIMediaControl->Stop())) {
			OutputDebug(L"IMediaControl::Stop failed.\n");
		}
	}
}

HRESULT CBonTuner::CreateTuningSpace(DVBSystemTypeData* pDVBSystemTypeData)
{
	m_pITuningSpace.Release();

	// �I�u�W�F�N�g�쐬�p�ϐ�
	EnumSettingValue::TuningSpace specifyTuningSpace = EnumSettingValue::TuningSpace::Auto;				// �g�p����TuningSpace�I�u�W�F�N�g
	CLSID clsidTuningSpace = CLSID_NULL;																// TuningSpace�I�u�W�F�N�g�̃N���Xid
	EnumSettingValue::Locator specifyLocator = EnumSettingValue::Locator::Auto;							// �g�p����Locator�I�u�W�F�N�g
	CLSID clsidLocator = CLSID_NULL;																	// Locator�I�u�W�F�N�g�̃N���Xid

	// TuningSpace�ݒ�p�ϐ�
	// ITuningSpace
	_bstr_t bstrUniqueName;																				// ITuningSpace�ɐݒ肷��UniqueName
	_bstr_t bstrFriendlyName;																			// ITuningSpace�ɐݒ肷��FriendlyName
	EnumSettingValue::NetworkType specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::Auto;	// ITuningSpace�ɐݒ肷��NetworkType
	IID iidNetworkType = IID_NULL;																		// ITuningSpace�ɐݒ肷��NetworkType��GUID
	// IDVBTuningSpace
	DVBSystemType dvbSystemType = DVB_Satellite;														// DVB�̃V�X�e���^�C�v
	// IDVBTuningSpace2
	long networkID = -1;																				// Network ID
	// IDVBSTuningSpace
	long highOscillator = -1;																			// High��Oscillator���g��
	long lowOscillator = -1;																			// Low��Oscillator���g��
	long lnbSwitch = -1;																				// LNB�X�C�b�`���g��
	SpectralInversion spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;								// �X�y�N�g�����]
	// IAnalogTVTuningSpace
	TunerInputType inputType = TunerInputCable;															// �A���e�i�E�P�[�u���̓��̓^�C�v
	long countryCode = 0;																				// ���E�n��R�[�h
	long minChannel = 0;																				// Channel�ԍ��̍ŏ��l
	long maxChannel = 0;																				// Channel�ԍ��̍ő�l
	// IATSCTuningSpace
	long minPhysicalChannel = 0;																		// Physical Channel�ԍ��̍ŏ��l
	long maxPhysicalChannel = 0;																		// Physical Channel�ԍ��̍ő�l
	long minMinorChannel = 0;																			// Minor Channel�ԍ��̍ŏ��l
	long maxMinorChannel = 0;																			// Minor Channel�ԍ��̍ő�l
	// IDigitalCableTuningSpace
	long minMajorChannel = 0;																			// Major Channel�ԍ��̍ŏ��l
	long maxMajorChannel = 0;																			// Major Channel�ԍ��̍ő�l
	long minSourceID = 0;																				// Source ID�̍ŏ��l
	long maxSourceID = 0;																				// Source ID�̍ő�l

	// Default Locator�ݒ�p�ϐ�
	// ILocator
	long frequency = -1;																				// RF�M���̎��g��
	long symbolRate = -1;																				// �V���{�����[�g
	FECMethod innerFECMethod = BDA_FEC_METHOD_NOT_SET;													// �����O���������^�C�v
	BinaryConvolutionCodeRate innerFECRate = BDA_BCC_RATE_NOT_SET;										// ����FEC���[�g
	FECMethod outerFECMethod = BDA_FEC_METHOD_NOT_SET;													// �O���O���������^�C�v
	BinaryConvolutionCodeRate outerFECRate = BDA_BCC_RATE_NOT_SET;										// �O��FEC���[�g
	ModulationType modulationType = BDA_MOD_NOT_SET;													// �ϒ��^�C�v
	// IDVBSLocator
	VARIANT_BOOL westPosition = VARIANT_TRUE;															// �q���̌o�x�𐼌o�Ɠ��o�̂ǂ���ŕ\�����邩(True�Ő��o)
	long orbitalPosition = -1;																			// �q���̌o�x(1/10��)
	long elevation = -1;																				// �q���̋p(1/10��)
	long azimuth = -1;																					// �q���̕��ʊp(1/10��)
	Polarisation polarisation = BDA_POLARISATION_NOT_SET;												// �Δg�l
	// IDVBSLocator2
	LNB_Source diseqLNBSource = BDA_LNB_SOURCE_NOT_SET;													// DiSeqC LNB���̓\�[�X
	Pilot pilot = BDA_PILOT_NOT_SET;																	// DVB-S2�p�C���b�g���[�h
	RollOff rollOff = BDA_ROLL_OFF_NOT_SET;																// DVB-S2���[���I�t�W��
	// IDVBTLocator
	long bandwidth = -1;																				// �ш敝(MHz)
	GuardInterval guardInterval = BDA_GUARD_NOT_SET;													// �K�[�h�C���^�[�o��
	HierarchyAlpha hierarchyAlpha = BDA_HALPHA_NOT_SET;													// �K�w��A���t�@
	FECMethod lpInnerFECMethod = BDA_FEC_METHOD_NOT_SET;												// LP�X�g���[���̓����O���������^�C�v
	BinaryConvolutionCodeRate lpInnerFECRate = BDA_BCC_RATE_NOT_SET;									// LP�X�g���[���̓���FEC���[�g
	TransmissionMode transmissionMode = BDA_XMIT_MODE_NOT_SET;											// �`�����[�h
	VARIANT_BOOL otherFrequencyInUse = VARIANT_TRUE;													// �ʂ�DVB-T�u���[�h�L���X�^�Ŏg���Ă��邩�ǂ���
	// IDVBTLocator2
	long physicalLayerPipeId = -1;																		// PLP ID
	// IATSCLocator
	long physicalChannel = -1;																			// Physical Channel�ԍ�
	long transportStreamID = -1;																		// TSID
	// IATSCLocator2
	long programNumber = -1;																			// Program Number

	switch (pDVBSystemTypeData->nDVBSystemType) {
	case EnumSettingValue::TunerType::DVBT:
	case EnumSettingValue::TunerType::DVBT2:
		bstrUniqueName = L"DVB-T";
		bstrFriendlyName = L"Local DVB-T Digital Antenna";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVB;
		if (pDVBSystemTypeData->nDVBSystemType == EnumSettingValue::TunerType::DVBT2) {
			specifyLocator = EnumSettingValue::Locator::DVBT2;
		}
		else {
			specifyLocator = EnumSettingValue::Locator::DVBT;
		}
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::DVBT;
		dvbSystemType = DVB_Terrestrial;
		networkID = 0;
		break;

	case EnumSettingValue::TunerType::DVBC:
		bstrUniqueName = L"DVB-C";
		bstrFriendlyName = L"Local DVB-C Digital Cable";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVB;
		specifyLocator = EnumSettingValue::Locator::DVBC;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::DVBC;
		dvbSystemType = DVB_Cable;
		networkID = 0;
		break;

	case EnumSettingValue::TunerType::ISDBT:
		bstrUniqueName = L"ISDB-T";
		bstrFriendlyName = L"Local ISDB-T Digital Antenna";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVB;
		specifyLocator = EnumSettingValue::Locator::DVBT;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::ISDBT;
		dvbSystemType = ISDB_Terrestrial;
		networkID = -1;
		break;

	case EnumSettingValue::TunerType::ISDBC:
		bstrUniqueName = L"ISDB-C";
		bstrFriendlyName = L"Local ISDB-C Digital Cable";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVB;
		specifyLocator = EnumSettingValue::Locator::DVBC;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::ISDBC;
		dvbSystemType = DVB_Cable;
		networkID = -1;
		break;

	case EnumSettingValue::TunerType::ISDBS:
		bstrUniqueName = L"ISDB-S";
		bstrFriendlyName = L"Default Digital ISDB-S Tuning Space";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVBS;
		specifyLocator = EnumSettingValue::Locator::ISDBS;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::ISDBS;
		dvbSystemType = ISDB_Satellite;
		networkID = -1;
		highOscillator = -1;
		lowOscillator = -1;
		lnbSwitch = -1;
		spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;
		break;

	case EnumSettingValue::TunerType::ATSC_Antenna:
		bstrUniqueName = L"ATSC";
		bstrFriendlyName = L"Local ATSC Digital Antenna";
		specifyTuningSpace = EnumSettingValue::TuningSpace::ATSC;
		specifyLocator = EnumSettingValue::Locator::ATSC;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::ATSC;
		inputType = TunerInputAntenna;
		countryCode = 0;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 69;
		minMinorChannel = 0;
		maxMinorChannel = 999;
		modulationType = BDA_MOD_128QAM;
		break;

	case EnumSettingValue::TunerType::ATSC_Cable:
		bstrUniqueName = L"ATSCCable";
		bstrFriendlyName = L"Local ATSC Digital Cable";
		specifyTuningSpace = EnumSettingValue::TuningSpace::ATSC;
		specifyLocator = EnumSettingValue::Locator::ATSC;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::ATSC;
		inputType = TunerInputCable;
		countryCode = 0;
		minChannel = 1;
		maxChannel = 99;
		minPhysicalChannel = 1;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		maxMinorChannel = 999;
		modulationType = BDA_MOD_128QAM;
		break;

	case EnumSettingValue::TunerType::DigitalCable:
		bstrUniqueName = L"Digital Cable";
		bstrFriendlyName = L"Local Digital Cable";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DigitalCable;
		specifyLocator = EnumSettingValue::Locator::DigitalCable;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::DigitalCable;
		inputType = TunerInputCable;
		countryCode = 0;
		minChannel = 2;
		maxChannel = 9999;
		minPhysicalChannel = 2;
		maxPhysicalChannel = 158;
		minMinorChannel = 0;
		maxMinorChannel = 999;
		minMajorChannel = 1;
		maxMajorChannel = 99;
		minSourceID = 0;
		maxSourceID = 0x7fffffff;
		break;

	case EnumSettingValue::TunerType::DVBS:
	default:
		bstrUniqueName = L"DVB-S";
		bstrFriendlyName = L"Default Digital DVB-S Tuning Space";
		specifyTuningSpace = EnumSettingValue::TuningSpace::DVBS;
		specifyLocator = EnumSettingValue::Locator::DVBS;
		specifyITuningSpaceNetworkType = EnumSettingValue::NetworkType::DVBS;
		dvbSystemType = DVB_Satellite;
		networkID = -1;
		highOscillator = 10600000;
		lowOscillator = 9750000;
		lnbSwitch = 11700000;
		spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;
		westPosition = VARIANT_FALSE;
		break;
	}

	if (pDVBSystemTypeData->nTuningSpace != EnumSettingValue::TuningSpace::Auto) {
		specifyTuningSpace = pDVBSystemTypeData->nTuningSpace;
	}
	switch (specifyTuningSpace) {
	case EnumSettingValue::TuningSpace::DVB:
		clsidTuningSpace = __uuidof(DVBTuningSpace);
		break;
	case EnumSettingValue::TuningSpace::DVBS:
		clsidTuningSpace = __uuidof(DVBSTuningSpace);
		break;
	case EnumSettingValue::TuningSpace::AnalogTV:
		clsidTuningSpace = __uuidof(AnalogTVTuningSpace);
		break;
	case EnumSettingValue::TuningSpace::ATSC:
		clsidTuningSpace = __uuidof(ATSCTuningSpace);
		break;
	case EnumSettingValue::TuningSpace::DigitalCable:
		clsidTuningSpace = __uuidof(DigitalCableTuningSpace);
		break;
	}

	if (pDVBSystemTypeData->nLocator != EnumSettingValue::Locator::Auto) {
		specifyLocator = pDVBSystemTypeData->nLocator;
	}
	switch (specifyLocator) {
	case EnumSettingValue::Locator::DVBT:
		clsidLocator = __uuidof(DVBTLocator);
		break;
	case EnumSettingValue::Locator::DVBT2:
		clsidLocator = __uuidof(DVBTLocator2);
		break;
	case EnumSettingValue::Locator::DVBS:
		clsidLocator = __uuidof(DVBSLocator);
		break;
	case EnumSettingValue::Locator::DVBC:
		clsidLocator = __uuidof(DVBCLocator);
		break;
	case EnumSettingValue::Locator::ISDBS:
		clsidLocator = __uuidof(ISDBSLocator);
		break;
	case EnumSettingValue::Locator::ATSC:
		clsidLocator = __uuidof(ATSCLocator);
		break;
	case EnumSettingValue::Locator::DigitalCable:
		clsidLocator = __uuidof(DigitalCableLocator);
		break;
	}

	if (pDVBSystemTypeData->nITuningSpaceNetworkType != EnumSettingValue::NetworkType::Auto) {
		specifyITuningSpaceNetworkType = pDVBSystemTypeData->nITuningSpaceNetworkType;
	}
	switch (specifyITuningSpaceNetworkType) {
	case EnumSettingValue::NetworkType::DVBT:
		iidNetworkType = { STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::DVBS:
		iidNetworkType = { STATIC_DVB_SATELLITE_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::DVBC:
		iidNetworkType = { STATIC_DVB_CABLE_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::ISDBT:
		iidNetworkType = { STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::ISDBS:
		iidNetworkType = { STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::ISDBC:
		iidNetworkType = { STATIC_ISDB_CABLE_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::ATSC:
		iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::DigitalCable:
		iidNetworkType = { STATIC_DIGITAL_CABLE_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::BSkyB:
		iidNetworkType = { STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::DIRECTV:
		iidNetworkType = { STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE };
		break;
	case EnumSettingValue::NetworkType::EchoStar:
		iidNetworkType = { STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE };
		break;
	}

	if (pDVBSystemTypeData->nIDVBTuningSpaceSystemType != EnumSettingValue::DVBSystemType::Auto) {
		dvbSystemType = (DVBSystemType)pDVBSystemTypeData->nIDVBTuningSpaceSystemType;
	}

	if (pDVBSystemTypeData->nIAnalogTVTuningSpaceInputType != EnumSettingValue::TunerInputType::Auto) {
		inputType = (tagTunerInputType)pDVBSystemTypeData->nIAnalogTVTuningSpaceInputType;
	}

	HRESULT hr;

	CComPtr<ITuningSpace> pITuningSpace;
	// ITuningSpace���쐬
	//
	// ITuningSpace�p�����F
	//   ITuningSpace �� IDVBTuningSpace �� IDVBTuningSpace2 �� IDVBSTuningSpace
	//                �� IAnalogTVTuningSpace �� IATSCTuningSpace �� IDigitalCableTuningSpace
	//                �� IAnalogRadioTuningSpace �� IAnalogRadioTuningSpace2
	//                �� IAuxInTuningSpace �� IAuxInTuningSpace2
	if (FAILED(hr = pITuningSpace.CoCreateInstance(clsidTuningSpace, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[CreateTuningSpace] Fail to get ITuningSpace interface\n");
	}
	else {
		// ITuningSpace �� NetworkType ��ݒ�
		if (FAILED(hr = pITuningSpace->put__NetworkType(iidNetworkType))) {
			OutputDebug(L"[CreateTuningSpace] put__NetworkType failed\n");
		}
		else {
			OutputDebug(L"[CreateTuningSpace] %s is created.\n", (wchar_t*)bstrFriendlyName);

			// ITuningSpace
			pITuningSpace->put_FrequencyMapping(L"");
			pITuningSpace->put_UniqueName(bstrUniqueName);
			pITuningSpace->put_FriendlyName(bstrFriendlyName);
			OutputDebug(L"  ITuningSpace is initialized.\n");

			// IDVBTuningSpace���L
			{
				CComQIPtr<IDVBTuningSpace> pIDVBTuningSpace(pITuningSpace);
				if (pIDVBTuningSpace) {
					pIDVBTuningSpace->put_SystemType(dvbSystemType);
					OutputDebug(L"  IDVBTuningSpace is initialized.\n");
				}
			}

			// IDVBTuningSpace2���L
			{
				CComQIPtr<IDVBTuningSpace2> pIDVBTuningSpace2(pITuningSpace);
				if (pIDVBTuningSpace2) {
					pIDVBTuningSpace2->put_NetworkID(networkID);
					OutputDebug(L"  IDVBTuningSpace2 is initialized.\n");
				}
			}

			// IDVBSTuningSpace���L
			{
				CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(pITuningSpace);
				if (pIDVBSTuningSpace) {
					pIDVBSTuningSpace->put_HighOscillator(highOscillator);
					pIDVBSTuningSpace->put_LowOscillator(lowOscillator);
					pIDVBSTuningSpace->put_LNBSwitch(lnbSwitch);
					pIDVBSTuningSpace->put_SpectralInversion(spectralInversion);
					OutputDebug(L"  IDVBSTuningSpace is initialized.\n");
				}
			}

			// IAnalogTVTuningSpace���L
			{
				CComQIPtr<IAnalogTVTuningSpace> pIAnalogTVTuningSpace(pITuningSpace);
				if (pIAnalogTVTuningSpace) {
					pIAnalogTVTuningSpace->put_InputType(inputType);
					pIAnalogTVTuningSpace->put_MinChannel(minChannel);
					pIAnalogTVTuningSpace->put_MaxChannel(maxChannel);
					pIAnalogTVTuningSpace->put_CountryCode(countryCode);
					OutputDebug(L"  IAnalogTVTuningSpace is initialized.\n");
				}
			}

			// IATSCTuningSpace���L
			{
				CComQIPtr<IATSCTuningSpace> pIATSCTuningSpace(pITuningSpace);
				if (pIATSCTuningSpace) {
					pIATSCTuningSpace->put_MinPhysicalChannel(minPhysicalChannel);
					pIATSCTuningSpace->put_MaxPhysicalChannel(maxPhysicalChannel);
					pIATSCTuningSpace->put_MinMinorChannel(minMinorChannel);
					pIATSCTuningSpace->put_MaxMinorChannel(maxMinorChannel);
					OutputDebug(L"  IATSCTuningSpace is initialized.\n");
				}
			}

			// IDigitalCableTuningSpace���L
			{
				CComQIPtr<IDigitalCableTuningSpace> pIDigitalCableTuningSpace(pITuningSpace);
				if (pIDigitalCableTuningSpace) {
					pIDigitalCableTuningSpace->put_MinMajorChannel(minMajorChannel);
					pIDigitalCableTuningSpace->put_MaxMajorChannel(maxMajorChannel);
					pIDigitalCableTuningSpace->put_MinSourceID(minSourceID);
					pIDigitalCableTuningSpace->put_MaxSourceID(maxSourceID);
					OutputDebug(L"  IDigitalCableTuningSpace is initialized.\n");
				}
			}

			// pILocator���쐬
			//
			// ILocator�p�����F
			//   ILocator �� IDigitalLocator �� IDVBTLocator �� IDVBTLocator2
			//                               �� IDVBSLocator �� IDVBSLocator2
			//                                               �� IISDBSLocator
			//                               �� IDVBCLocator
			//                               �� IATSCLocator �� IATSCLocator2 �� IDigitalCableLocator
			//            �� IAnalogLocator
			CComPtr<ILocator> pILocator;
			if (FAILED(hr = pILocator.CoCreateInstance(clsidLocator))) {
				OutputDebug(L"[CreateTuningSpace] Failed to get ILocator interface.\n");
			}
			else {
				// Default Locator�̒l���쐬 
				// ILocator
				pILocator->put_CarrierFrequency(frequency);
				pILocator->put_SymbolRate(symbolRate);
				pILocator->put_InnerFEC(innerFECMethod);
				pILocator->put_InnerFECRate(innerFECRate);
				pILocator->put_OuterFEC(outerFECMethod);
				pILocator->put_OuterFECRate(outerFECRate);
				pILocator->put_Modulation(modulationType);
				OutputDebug(L"  ILocator is initialized.\n");

				// IDigitalLocator���L�v���p�e�B�͖�������Log�Ɏc��
				{
					CComQIPtr<IDigitalLocator> pIDigitalLocator(pILocator);
					if (pIDigitalLocator) {
						OutputDebug(L"  IDigitalLocator is initialized.\n");
					}
				}

				// IDVBSLocator���L
				{
					CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
					if (pIDVBSLocator) {
						pIDVBSLocator->put_WestPosition(westPosition);
						pIDVBSLocator->put_OrbitalPosition(orbitalPosition);
						pIDVBSLocator->put_Elevation(elevation);
						pIDVBSLocator->put_Azimuth(azimuth);
						pIDVBSLocator->put_SignalPolarisation(polarisation);
						OutputDebug(L"  IDVBSLocator is initialized.\n");
					}
				}

				// IDVBSLocator2���L
				{
					CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
					if (pIDVBSLocator2) {
						pIDVBSLocator2->put_LocalOscillatorOverrideHigh(-1);
						pIDVBSLocator2->put_LocalOscillatorOverrideLow(-1);
						pIDVBSLocator2->put_LocalLNBSwitchOverride(-1);
						pIDVBSLocator2->put_LocalSpectralInversionOverride(BDA_SPECTRAL_INVERSION_NOT_SET);
						pIDVBSLocator2->put_DiseqLNBSource(diseqLNBSource);
						pIDVBSLocator2->put_SignalPilot(pilot);
						pIDVBSLocator2->put_SignalRollOff(rollOff);
						OutputDebug(L"  IDVBSLocator2 is initialized.\n");
					}
				}

				// IISDBSLocator���L�v���p�e�B�͖�������Log�Ɏc��
				{
					CComQIPtr<IISDBSLocator> pIISDBSLocator(pILocator);
					if (pIISDBSLocator) {
						OutputDebug(L"  IISDBSLocator is initialized.\n");
					}
				}

				// IDVBTLocator���L
				{
					CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
					if (pIDVBTLocator) {
						pIDVBTLocator->put_Bandwidth(bandwidth);
						pIDVBTLocator->put_Guard(guardInterval);
						pIDVBTLocator->put_HAlpha(hierarchyAlpha);
						pIDVBTLocator->put_LPInnerFEC(lpInnerFECMethod);
						pIDVBTLocator->put_LPInnerFECRate(lpInnerFECRate);
						pIDVBTLocator->put_Mode(transmissionMode);
						pIDVBTLocator->put_OtherFrequencyInUse(otherFrequencyInUse);
						OutputDebug(L"  IDVBTLocator is initialized.\n");
					}
				}

				// IDVBTLocator2���L
				{
					CComQIPtr<IDVBTLocator2> pIDVBTLocator2(pILocator);
					if (pIDVBTLocator2) {
						pIDVBTLocator2->put_PhysicalLayerPipeId(physicalLayerPipeId);
						OutputDebug(L"  IDVBTLocator2 is initialized.\n");
					}
				}

				// IDVBCLocator���L�v���p�e�B�͖�������Log�Ɏc��
				{
					CComQIPtr<IDVBCLocator> pIDVBCLocator(pILocator);
					if (pIDVBCLocator) {
						OutputDebug(L"  IDVBCLocator is initialized.\n");
					}
				}

				// IATSCLocator���L
				{
					CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
					if (pIATSCLocator) {
						pIATSCLocator->put_PhysicalChannel(physicalChannel);
						pIATSCLocator->put_TSID(transportStreamID);
						OutputDebug(L"  IATSCLocator is initialized.\n");
					}
				}

				// IATSCLocator2���L
				{
					CComQIPtr<IATSCLocator2> pIATSCLocator2(pILocator);
					if (pIATSCLocator2) {
						pIATSCLocator2->put_ProgramNumber(programNumber);
						OutputDebug(L"  IATSCLocator2 is initialized.\n");
					}
				}

				// IDigitalCableLocator���L�v���p�e�B�͖�������Log�Ɏc��
				{
					CComQIPtr<IDigitalCableLocator> pIDigitalCableLocator(pILocator);
					if (pIDigitalCableLocator) {
						OutputDebug(L"  IDigitalCableLocator is initialized.\n");
					}
				}

				pITuningSpace->put_DefaultLocator(pILocator);

				// �S�Đ���
				m_pITuningSpace = pITuningSpace;
				OutputDebug(L"[CreateTuningSpace] Successfully created ITuningSpace.\n");
				return S_OK;
			}
		}
	}

	// ���s
	OutputDebug(L"[CreateTuningSpace] ITuningSpace creation failed.\n");
	return E_FAIL;
}

void CBonTuner::UnloadTuningSpace(void)
{
	m_pITuningSpace.Release();
}

// Tuning Request �𑗂��� Tuning Space ������������
//   ��������Ȃ��� output pin ���o�����Ȃ��`���[�i�t�B���^��
//   ����炵��
HRESULT CBonTuner::InitTuningSpace(void)
{
	if (!m_DVBSystemTypeDB.IsExist(0)) {
		OutputDebug(L"[InitTuningSpace] TuningSpace NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pITuner) {
		OutputDebug(L"[InitTuningSpace] ITuner NOT SET.\n");
		return E_POINTER;
	}

	HRESULT hr = E_FAIL;

	// ITuneRequest�쐬
	//
	// ITuneRequest�p�����F
	//   ITuneRequest �� IDVBTuneRequest
	//                �� IChannelTuneRequest �� IATSCChannelTuneRequest �� IDigitalCableTuneRequest
	//                �� IChannelIDTuneRequest
	//                �� IMPEG2TuneRequest
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"[InitTuningSpace] Fail to get ITuneRequest interface.\n");
	}
	else {
		// IDVBTuneRequest���L
		{
			CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
			if (pIDVBTuneRequest) {
				pIDVBTuneRequest->put_ONID(-1);
				pIDVBTuneRequest->put_TSID(-1);
				pIDVBTuneRequest->put_SID(-1);
			}
		}

		// IChannelTuneRequest���L
		{
			CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
			if (pIChannelTuneRequest) {
				pIChannelTuneRequest->put_Channel(-1);
			}
		}

		// IATSCChannelTuneRequest���L
		{
			CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
			if (pIATSCChannelTuneRequest) {
				pIATSCChannelTuneRequest->put_MinorChannel(-1);
			}
		}

		// IDigitalCableTuneRequest���L
		{
			CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
			if (pIDigitalCableTuneRequest) {
				pIDigitalCableTuneRequest->put_MajorChannel(-1);
				pIDigitalCableTuneRequest->put_SourceID(-1);
			}
		}

		hr = m_pITuner->put_TuningSpace(m_pITuningSpace);
		hr = m_pITuner->put_TuneRequest(pITuneRequest);

		// �S�Đ���
		return S_OK;
	}

	// ���s
	return hr;
}

HRESULT CBonTuner::LoadNetworkProvider(DVBSystemTypeData* pDVBSystemTypeData)
{
	m_pNetworkProvider.Release();

	CLSID clsidNetworkProvider = CLSID_NULL;

	switch (pDVBSystemTypeData->nNetworkProvider) {
	case EnumSettingValue::NetworkProvider::Generic:
		clsidNetworkProvider = CLSID_NetworkProvider;
		break;
	case EnumSettingValue::NetworkProvider::DVBS:
		clsidNetworkProvider = CLSID_DVBSNetworkProvider;
		break;
	case EnumSettingValue::NetworkProvider::DVBT:
		clsidNetworkProvider = CLSID_DVBTNetworkProvider;
		break;
	case EnumSettingValue::NetworkProvider::DVBC:
		clsidNetworkProvider = CLSID_DVBCNetworkProvider;
		break;
	case EnumSettingValue::NetworkProvider::ATSC:
		clsidNetworkProvider = CLSID_ATSCNetworkProvider;
		break;
	case EnumSettingValue::NetworkProvider::Auto:
	default:
		switch (pDVBSystemTypeData->nDVBSystemType) {
		case EnumSettingValue::TunerType::DVBS:
		case EnumSettingValue::TunerType::ISDBS:
			clsidNetworkProvider = CLSID_DVBSNetworkProvider;
			break;
		case EnumSettingValue::TunerType::DVBT:
		case EnumSettingValue::TunerType::DVBT2:
		case EnumSettingValue::TunerType::ISDBT:
			clsidNetworkProvider = CLSID_DVBTNetworkProvider;
			break;
		case EnumSettingValue::TunerType::DVBC:
			clsidNetworkProvider = CLSID_DVBCNetworkProvider;
			break;
		case EnumSettingValue::TunerType::ATSC_Antenna:
		case EnumSettingValue::TunerType::ATSC_Cable:
		case EnumSettingValue::TunerType::DigitalCable:
			clsidNetworkProvider = CLSID_ATSCNetworkProvider;
			break;
		default:
			clsidNetworkProvider = CLSID_NetworkProvider;
			break;
		}
		break;
	}

	HRESULT hr = E_FAIL;

	CComPtr<IBaseFilter> pNetworkProvider;
	// Network Proveider�t�B���^���擾
	if (FAILED(hr = pNetworkProvider.CoCreateInstance(clsidNetworkProvider, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[LoadNetworkProvider] Fail to get NetworkProvider IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pNetworkProvider);
		OutputDebug(L"[LoadNetworkProvider] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pNetworkProvider, strName.c_str()))) {
			OutputDebug(L"[LoadNetworkProvider] Fail to add NetworkProvider IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// ITuner interface���擾
			CComQIPtr<ITuner> pITuner(pNetworkProvider);
			if (!pITuner) {
				OutputDebug(L"[LoadNetworkProvider] Fail to get ITuner interface.\n");
				hr = E_FAIL;
			}
			else {
				// ITuner interface�̎擾����
				// �S�Đ���
				m_pNetworkProvider = pNetworkProvider;
				m_pITuner = pITuner;
				return hr;
			}
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadNetworkProvider(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pNetworkProvider)
		hr = m_pIGraphBuilder->RemoveFilter(m_pNetworkProvider);

	m_pITuner.Release();
	m_pNetworkProvider.Release();
}

// �`���[�i�E�L���v�`���̑g���킹���X�g���瓮�삷����̂�T��
HRESULT CBonTuner::LoadAndConnectDevice(unsigned int tunerGroup)
{
	HRESULT hr;
	if (!m_DVBSystemTypeDB.IsExist(0) || !m_pNetworkProvider) {
		OutputDebug(L"[P->T] TuningSpace or NetworkProvider NOT SET.\n");
		return E_POINTER;
	}
	for (BOOL bTuner = m_TunerComboList.ResetTuner(tunerGroup); bTuner == TRUE; bTuner = m_TunerComboList.NextTuner()) {
		// �`���[�i�f�o�C�X���[�v
		const std::wstring* pTunerGUID = NULL;
		const std::wstring* pTunerFriendlyName = NULL;
		m_TunerComboList.GetTunerData(&pTunerGUID, &pTunerFriendlyName, NULL);
		OutputDebug(L"[P->T] Trying tuner device=FriendlyName:%s,  GUID:%s\n", pTunerFriendlyName->c_str(), pTunerGUID->c_str());

		// �r�������p�ɃZ�}�t�H�p��������쐬 ('\' -> '/')
		std::wstring semName = *pTunerGUID;
		std::replace(semName.begin(), semName.end(), L'\\', L'/');
		semName = L"Global\\" + semName;

		// �r������
		m_hSemaphore = ::CreateSemaphoreW(NULL, 1, 1, semName.c_str());
		DWORD result = WaitForSingleObjectWithMessageLoop(m_hSemaphore, 0);
		if (result != WAIT_OBJECT_0) {
			// �g�p���Ȃ̂Ŏ��̃`���[�i��T��
			OutputDebug(L"[P->T] Another is using.\n");
		}
		else {
			// �r���m�FOK
			CComPtr<IBaseFilter> pTunerDevice;
			// �`���[�i�f�o�C�X�̃t�B���^���擾
			if (FAILED(hr = m_TunerComboList.GetTunerFilter(&pTunerDevice))) {
				// �t�B���^���擾�ł��Ȃ������̂Ŏ��̃`���[�i��
				OutputDebug(L"[P->T] Fail to get TunerDevice IBaseFilter interface.\n");
			}
			else {
				// �t�B���^�擾����
				// Graph Builder�Ƀ`���[�i�f�o�C�X�̃t�B���^��ǉ�
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTunerDevice, pTunerFriendlyName->c_str()))) {
					// �t�B���^�̒ǉ��Ɏ��s�����̂Ŏ��̃`���[�i��
					OutputDebug(L"[P->T] Fail to add TunerDevice IBaseFilter into graph.\n");
				}
				else {
					// �t�B���^�ǉ�����
					// �`���[�i�f�o�C�X��connect ���Ă݂�
					if (FAILED(hr = Connect(m_pNetworkProvider, pTunerDevice))) {
						// connect�Ɏ��s�����̂Ŏ��̃`���[�i��
						OutputDebug(L"[P->T] Fail to connect.\n");
					}
					else {
						// connect ����
						OutputDebug(L"[P->T] Connect OK.\n");
						// �`���[�i�ŗLDll���K�v�Ȃ�Ǎ��݁A�ŗL�̏���������������ΌĂяo��
						if (FAILED(hr = CheckAndInitTunerDependDll(pTunerDevice, *pTunerGUID, *pTunerFriendlyName))) {
							// �ŗLDll�̏��������s�����̂Ŏ��̃`���[�i��
							OutputDebug(L"[P->T] Discarded by BDASpecial's CheckAndInitTuner function.\n");
						}
						else {
							// �ŗLDll����OK
							if (!m_TunerComboList.bNotExistCaptureDevice) {
								// �L���v�`���f�o�C�X���g�p����ꍇ
								for (BOOL bCapture = m_TunerComboList.ResetCapture(); bCapture == TRUE; bCapture = m_TunerComboList.NextCapture()) {
									// �L���v�`���f�o�C�X���[�v
									const std::wstring* pCaptureGUID = NULL;
									const std::wstring* pCaptureFriendlyName = NULL;
									m_TunerComboList.GetCaptureData(&pCaptureGUID, &pCaptureFriendlyName, NULL);
									OutputDebug(L"[T->C] Trying capture device=FriendlyName:%s,  GUID:%s\n", pCaptureFriendlyName->c_str(), pCaptureGUID->c_str());
									// �`���[�i�ŗLDll�ł̊m�F����������ΌĂяo��
									if (FAILED(hr = CheckCapture(*pTunerGUID, *pTunerFriendlyName, *pCaptureGUID, *pCaptureFriendlyName))) {
										// �ŗLDll���_���ƌ����Ă���̂Ŏ��̃L���v�`���f�o�C�X��
										OutputDebug(L"[T->C] Discarded by BDASpecial's CheckCapture function.\n");
									}
									else {
										// �ŗLDll�̊m�FOK
										CComPtr<IBaseFilter> pCaptureDevice;
										// �L���v�`���f�o�C�X�̃t�B���^���擾
										if (FAILED(hr = m_TunerComboList.GetCaptureFilter(&pCaptureDevice))) {
											// �t�B���^���擾�ł��Ȃ������̂Ŏ��̃L���v�`���f�o�C�X��
											OutputDebug(L"[T->C] Fail to get CaptureDevice IBaseFilter interface.\n");
										}
										else {
											// �t�B���^�擾����
											// Graph Builder�ɃL���v�`���f�o�C�X�̃t�B���^��ǉ�
											if (FAILED(hr = m_pIGraphBuilder->AddFilter(pCaptureDevice, pCaptureFriendlyName->c_str()))) {
												// �t�B���^�̒ǉ��Ɏ��s�����̂Ŏ��̃L���v�`���f�o�C�X��
												OutputDebug(L"[T->C] Fail to add CaptureDevice IBaseFilter into graph.\n");
											}
											else {
												// �t�B���^�ǉ�����
												// �L���v�`���f�o�C�X��connect ���Ă݂�
												if (FAILED(hr = Connect(pTunerDevice, pCaptureDevice))) {
													// connect�Ɏ��s�����̂Ŏ��̃L���v�`���f�o�C�X��
													OutputDebug(L"[T->C] Fail to connect.\n");
												}
												else {
													// connect ����
													OutputDebug(L"[T->C] Connect OK.\n");
													// TsWriter�ȍ~�Ɛڑ��`Run
													if (SUCCEEDED(LoadAndConnectMiscFilters(pTunerDevice, pCaptureDevice))) {
														// ���ׂĐ���
														m_pTunerDevice = pTunerDevice;
														m_pCaptureDevice = pCaptureDevice;
														// �`���[�i�ŗL�֐��̃��[�h
														LoadTunerDependCode(*pTunerGUID, *pTunerFriendlyName, *pCaptureGUID, *pCaptureFriendlyName);
														// ����̑g�������`���[�i�E�L���v�`�����X�g�ɋL��
														m_TunerComboList.Remenber();
														return S_OK;
													}
													// �L���v�`���f�o�C�X��disconnect
													// DisconnectAll(pCaptureDevice);
												}
												// Graph Builder����L���v�`���f�o�C�X��remove
												m_pIGraphBuilder->RemoveFilter(pCaptureDevice);
											}
										}
									}
									// ���̃L���v�`���f�o�C�X�փ��[�v
								}
								// �L���v�`���f�o�C�X���[�v�I���
								// ���삷��g������������Ȃ������̂Ŏ��̃`���[�i��
							}
							else {
								// �L���v�`���f�o�C�X���g�p���Ȃ��ꍇ
								// TsWriter�ȍ~�Ɛڑ��`Run
								if (SUCCEEDED(hr = LoadAndConnectMiscFilters(pTunerDevice, NULL))) {
									// ���ׂĐ���
									m_pTunerDevice = pTunerDevice;
									// �`���[�i�ŗL�֐��̃��[�h
									LoadTunerDependCode(*pTunerGUID, *pTunerFriendlyName, L"", L"");
									// ����̑g�������`���[�i�E�L���v�`�����X�g�ɋL��
									m_TunerComboList.Remenber();
									return S_OK;
								}
							}
							// �`���[�i�ŗL�֐���Dll���
							ReleaseTunerDependCode();
						}
						// Graph Builder����`���[�i��remove
						// DisconnectAll(pTunerDevice);
					}
					// Graph Builder����`���[�i��remove
					m_pIGraphBuilder->RemoveFilter(pTunerDevice);
				}
			}
			// �r�������I��
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
		}
		// �Z�}�t�H���
		SAFE_CLOSE_HANDLE(m_hSemaphore);
		// ���̃`���[�i�f�o�C�X�փ��[�v
	}
	// �`���[�i�f�o�C�X���[�v�I���
	// ���삷��g�ݍ��킹��������Ȃ�����
	OutputDebug(L"[P->T] Can not found a connectable pair of TunerDevice and CaptureDevice.\n");
	return E_FAIL;
}

void CBonTuner::UnloadTunerDevice(void)
{
	HRESULT hr;

	ReleaseTunerDependCode();

	if (m_pIGraphBuilder && m_pTunerDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);

	m_pTunerDevice.Release();
}

void CBonTuner::UnloadCaptureDevice(void)
{
	HRESULT hr;

	if (m_pIGraphBuilder && m_pCaptureDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);

	m_pCaptureDevice.Release();
}

HRESULT CBonTuner::LoadAndConnectMiscFilters(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice)
{
	HRESULT hr = E_FAIL;

	// TsWriter�Ɛڑ�
	if (SUCCEEDED(hr = LoadAndConnectTsWriter(pTunerDevice, pCaptureDevice))) {
		// TsDemuxer�Ɛڑ�
		if (SUCCEEDED(hr = LoadAndConnectDemux())) {
			// TIF�Ɛڑ�
			if (SUCCEEDED(hr = LoadAndConnectTif())) {
				// Run���Ă݂�
				if (SUCCEEDED(hr = RunGraph())) {
					// ����
					OutputDebug(L"RunGraph OK.\n");
					return hr;
				}
				OutputDebug(L"RunGraph Failed.\n");
				// DisconnectAll(m_pTif);
				UnloadTif();
			}
			// DisconnectAll(m_pDemux);
			UnloadDemux();
		}
		// DisconnectAll(m_pTsWriter);
		UnloadTsWriter();
	}

	// ���s
	return hr;
}

HRESULT CBonTuner::LoadAndConnectTsWriter(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice)
{
	// TS Writer�̖���:AddFilter���ɓo�^���閼�O
	static constexpr WCHAR* const FILTER_GRAPH_NAME_TSWRITER = L"TS Writer";

	HRESULT hr = E_FAIL;
	std::wstring sDebugPrefix = m_TunerComboList.bNotExistCaptureDevice ? L"[T->W]" : L"[C->W]";

	if (!pTunerDevice || (!pCaptureDevice && !m_TunerComboList.bNotExistCaptureDevice)) {
		OutputDebug(L"%s TunerDevice or CaptureDevice NOT SET.\n", sDebugPrefix.c_str());
		return E_POINTER;
	}

	// �t�B���^�N���X�̃��[�h
	CTsWriter* pCTsWriter = (CTsWriter*)CTsWriter::CreateInstance(NULL, &hr);
	if (!pCTsWriter) {
		OutputDebug(L"%s Fail to create CTsWriter filter class instance.\n", sDebugPrefix.c_str());
		hr = E_FAIL;
	}
	else {
		// �t�B���^���擾
		CComQIPtr<IBaseFilter> pTsWriter(pCTsWriter);
		if (!pTsWriter) {
			OutputDebug(L"%s Fail to get TsWriter IBaseFilter interface.\n", sDebugPrefix.c_str());
			hr = E_FAIL;
		}
		else {
			// �t�B���^�擾����
			OutputDebug(L"%s %s is loaded.\n", sDebugPrefix.c_str(), FILTER_GRAPH_NAME_TSWRITER);
			// Graph Builder�Ƀt�B���^��ǉ�
			if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTsWriter, FILTER_GRAPH_NAME_TSWRITER))) {
				OutputDebug(L"%s Fail to add TsWriter IBaseFilter into graph.\n", sDebugPrefix.c_str());
			}
			else {
				// �t�B���^�ǉ�����
				// ITsWriter interface���擾
				CComQIPtr<ITsWriter> pITsWriter(pTsWriter);
				if (!pITsWriter) {
					OutputDebug(L"%s Fail to get ITsWriter interface.\n", sDebugPrefix.c_str());
					hr = E_FAIL;
				}
				else {
					// ITsWriter interface�̎擾����
					// connect ���Ă݂�
					if (m_TunerComboList.bNotExistCaptureDevice) {
						// Capture�f�o�C�X�����݂��Ȃ��ꍇ��Tuner�Ɛڑ�
						hr = Connect(pTunerDevice, pTsWriter);
					}
					else {
						// Capture�f�o�C�X�Ɛڑ�
						hr = Connect(pCaptureDevice, pTsWriter);
					}
					if (FAILED(hr)) {
						OutputDebug(L"%s Failed to connect.\n", sDebugPrefix.c_str());
					}
					else {
						// connect �����Ȃ̂ł��̂܂܏I��
						OutputDebug(L"%s Connect OK.\n", sDebugPrefix.c_str());
						m_pTsWriter = pTsWriter;
						m_pITsWriter = pITsWriter;
						return hr;
					}
				}
				m_pIGraphBuilder->RemoveFilter(pTsWriter);
			}
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadTsWriter(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTsWriter)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTsWriter);

	m_pITsWriter.Release();
	m_pTsWriter.Release();
}

HRESULT CBonTuner::LoadAndConnectDemux(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTsWriter) {
			OutputDebug(L"[W->M] TsWriter NOT SET.\n");
			return E_POINTER;
	}

	CComPtr<IBaseFilter> pDemux;
	// �t�B���^���擾
	if (FAILED(hr = pDemux.CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[W->M] Fail to get MPEG2Demultiplexer IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pDemux);
		OutputDebug(L"[W->M] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pDemux, strName.c_str()))) {
			OutputDebug(L"[W->M] Fail to add MPEG2Demultiplexer IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// connect ���Ă݂�
			if (FAILED(hr = Connect(m_pTsWriter, pDemux))) {
				OutputDebug(L"[W->M] Fail to connect.\n");
			}
			else {
				// connect �����Ȃ̂ł��̂܂܏I��
				OutputDebug(L"[W->M] Connect OK.\n");
				m_pDemux = pDemux;
				return hr;
			}
			m_pIGraphBuilder->RemoveFilter(pDemux);
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadDemux(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pDemux)
		hr = m_pIGraphBuilder->RemoveFilter(m_pDemux);

	m_pDemux.Release();
}

HRESULT CBonTuner::LoadAndConnectTif(void)
{
	// MPEG2 TIF��CLSID
	static constexpr CLSID CLSID_MPEG2TransportInformationFilter = { 0xfc772ab0, 0x0c7f, 0x11d3, 0x8f, 0xf2, 0x00, 0xa0, 0xc9, 0x22, 0x4c, 0xf4 };

	HRESULT hr = E_FAIL;

	if (!m_pDemux) {
			OutputDebug(L"[M->I] MPEG2Demultiplexer NOT SET.\n");
			return E_POINTER;
	}

	CComPtr<IBaseFilter> pTif;
	// �t�B���^���擾
	if (FAILED(hr = pTif.CoCreateInstance(CLSID_MPEG2TransportInformationFilter, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[M->I] Fail to get TIF IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pTif);
		OutputDebug(L"[M->I] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTif, strName.c_str()))) {
			OutputDebug(L"[M->I] Fail to add TIF IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// connect ���Ă݂�
			if (FAILED(hr = Connect(m_pDemux, pTif))) {
				OutputDebug(L"[M->I] Fail to connect.\n");
			}
			else {
				// connect �����Ȃ̂ł��̂܂܏I��
				OutputDebug(L"[M->I] Connect OK.\n");
				m_pTif = pTif;
				return hr;
			}
			m_pIGraphBuilder->RemoveFilter(pTif);
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadTif(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTif)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTif);

	m_pTif.Release();
}

HRESULT CBonTuner::LoadTunerSignalStatisticsTunerNode(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CDSEnumNodes DSEnumNodes(m_pTunerDevice);
	CComPtr<IUnknown> pControlNode;
	if (FAILED(hr = DSEnumNodes.getControlNode(__uuidof(IBDA_FrequencyFilter), &pControlNode))) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] Fail to get control node.\n");
		return E_FAIL;
	}

	CComQIPtr<IBDA_SignalStatistics> pIBDA_SignalStatistics(pControlNode);
	if (!pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] Fail to get IBDA_SignalStatistics interface.\n");
		return E_FAIL;
	}

	OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] SUCCESS.\n");
	m_pIBDA_SignalStatisticsTunerNode = pIBDA_SignalStatistics;

	return S_OK;
}

HRESULT CBonTuner::LoadTunerSignalStatisticsDemodNode(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CDSEnumNodes DSEnumNodes(m_pTunerDevice);
	CComPtr<IUnknown> pControlNode;
	if (FAILED(hr = DSEnumNodes.getControlNode(__uuidof(IBDA_DigitalDemodulator), &pControlNode))) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] Fail to get control node.\n");
		return E_FAIL;
	}

	CComQIPtr<IBDA_SignalStatistics> pIBDA_SignalStatistics(pControlNode);
	if (!pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] Fail to get IBDA_SignalStatistics interface.\n");
		return E_FAIL;
	}

	OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] SUCCESS.\n");
	m_pIBDA_SignalStatisticsDemodNode = pIBDA_SignalStatistics;

	return S_OK;
}

void CBonTuner::UnloadTunerSignalStatistics(void)
{
	m_pIBDA_SignalStatisticsTunerNode.Release();
	m_pIBDA_SignalStatisticsDemodNode.Release();
}

// Connect pins (Common subroutine)
//  �S�Ẵs����ڑ����Đ���������I��
//
HRESULT CBonTuner::Connect(IBaseFilter* pFilterUp, IBaseFilter* pFilterDown)
{
	HRESULT hr;

	CDSEnumPins DSEnumPinsUp(pFilterUp);
	CDSEnumPins DSEnumPinsDown(pFilterDown);

	// �㗬�t�B���^��Output�s���̐��������[�v
	while (1) {
		CComPtr<IPin> pIPinUp;
		if (S_OK != (hr = DSEnumPinsUp.getNextPin(&pIPinUp, PIN_DIRECTION::PINDIR_OUTPUT))) {
			// ���[�v�I���
			break;
		}
		do {
			CComPtr<IPin> pIPinPeerOfUp;
			// �㗬�t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̏㗬�s����
			if (pIPinUp->ConnectedTo(&pIPinPeerOfUp) != VFW_E_NOT_CONNECTED){
				OutputDebug(L"  An already connected pin was found.\n");
				break;
			}

			// �����t�B���^��Input�s���̐��������[�v
			DSEnumPinsDown.Reset();
			while (1) {
				CComPtr<IPin> pIPinDown;
				if (S_OK != (hr = DSEnumPinsDown.getNextPin(&pIPinDown, PIN_DIRECTION::PINDIR_INPUT))) {
					// ���[�v�I���
					break;
				}
				do {
					CComPtr<IPin> pIPinPeerOfDown;
					// �����t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̉����s����
					if (pIPinDown->ConnectedTo(&pIPinPeerOfDown) != VFW_E_NOT_CONNECTED) {
						OutputDebug(L"  An already connected pin was found.\n");
						break;
					}

					// �ڑ������݂�
					if (SUCCEEDED(hr = m_pIGraphBuilder->ConnectDirect(pIPinUp, pIPinDown, NULL))) {
						// �ڑ�����
						return hr;
					} else {
						// �Ⴄ�`���[�i���j�b�g�̃t�B���^��ڑ����悤�Ƃ��Ă�ꍇ�Ȃ�
						// �R�l�N�g�ł��Ȃ��ꍇ�A���̉����s����
						OutputDebug(L"  A pair of pins that can not be connected was found.\n");
					}
				} while(0);
			} // while; ���̉����s����
		} while (0);
	} // while ; ���̏㗬�s����

	// �R�l�N�g�\�ȑg�ݍ��킹��������Ȃ�����
	OutputDebug(L"  Can not found a pair of connectable pins.\n");
	return E_FAIL;
}

void CBonTuner::DisconnectAll(IBaseFilter* pFilter)
{
	if (!m_pIGraphBuilder || !pFilter)
		return;
	
	HRESULT hr;

	CDSEnumPins DSEnumPins(pFilter);
	// �s���̐��������[�v
	while (1) {
		CComPtr<IPin> pIPin;
		CComPtr<IPin> pIPinPeerOf;
		if (S_OK != (hr = DSEnumPins.getNextPin(&pIPin))) {
			// ���[�v�I���
			break;
		}
		// �s�����ڑ��ς�������ؒf
		if (SUCCEEDED(hr = pIPin->ConnectedTo(&pIPinPeerOf))) {
			hr = m_pIGraphBuilder->Disconnect(pIPinPeerOf);
			hr = m_pIGraphBuilder->Disconnect(pIPin);
		}
	}
}
