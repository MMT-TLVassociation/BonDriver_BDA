//------------------------------------------------------------------------------
// File: BonTuner.h
//   Define CBonTuner class
//------------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <stdio.h>

#include <list>
#include <vector>
#include <map>

#include "IBonDriver2.h"
#include "IBdaSpecials2.h"

#include "LockChannel.h"

#include <iostream>
#include <dshow.h>

#include <tuner.h>

#include "common.h"

using namespace std;

class CTsWriter;

// CBonTuner class
////////////////////////////////
class CBonTuner : public IBonDriver2
{
public:
	////////////////////////////////////////
	// �R���X�g���N�^ & �f�X�g���N�^
	////////////////////////////////////////
	CBonTuner();
	virtual ~CBonTuner();

	////////////////////////////////////////
	// IBonDriver �����o�֐�
	////////////////////////////////////////
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE byCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

	////////////////////////////////////////
	// IBonDriver2 �����o�֐�
	////////////////////////////////////////
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

	void Release(void);
	
	////////////////////////////////////////
	// �ÓI�����o�ϐ�
	////////////////////////////////////////

	// Dll�̃��W���[���n���h��
	static HMODULE st_hModule;

	// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
	static list<CBonTuner*> st_InstanceList;

	// st_InstanceList����p
	static CRITICAL_SECTION st_LockInstanceList;

protected:
	////////////////////////////////////////
	// ���������o�֐�
	////////////////////////////////////////

	// COM������p�X���b�h
	static DWORD WINAPI COMProcThread(LPVOID lpParameter);

	// Decode������p�X���b�h
	static DWORD WINAPI DecodeProcThread(LPVOID lpParameter);

	// TsWriter �R�[���o�b�N�֐�
	static int CALLBACK RecvProc(void* pParam, BYTE* pbData, DWORD dwSize);

	// �f�[�^��M�X�^�[�g�E��~
	void StartRecv(void);
	void StopRecv(void);

	// ini �t�@�C���Ǎ�
	void ReadIniFile(void);

	// �M����Ԃ��擾
	void GetSignalState(int* pnStrength, int* pnQuality, int* pnLock);

	// �`�����l���ؑ�
	BOOL LockChannel(const TuningParam *pTuningParam);

	// �`���[�i�ŗLDll�̃��[�h
	HRESULT CheckAndInitTunerDependDll(void);

	// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
	HRESULT CheckCapture(wstring displayName, wstring friendlyName);
		
	// �`���[�i�ŗL�֐��̃��[�h
	void LoadTunerDependCode(void);

	// �`���[�i�ŗL�֐���Dll�̉��
	void ReleaseTunerDependCode(void);

	// GraphBuilder
	HRESULT InitializeGraphBuilder(void);
	void CleanupGraph(void);
	HRESULT RunGraph(void);
	void StopGraph(void);

	// TuningSpace
	HRESULT CreateTuningSpace(void);
	void UnloadTuningSpace(void);
	HRESULT InitTuningSpace(void);

	// NetworkProvider
	HRESULT LoadNetworkProvider(void);
	void UnloadNetworkProvider(void);

	// TunerDevice
	HRESULT LoadAndConnectTunerDevice(void);
	void UnloadTunerDevice(void);
	
	// CaptureDevice
	HRESULT LoadAndConnectCaptureDevice(void);
	void UnloadCaptureDevice(void);
	
	// TsWriter
	HRESULT LoadAndConnectTsWriter(void);
	void UnloadTsWriter(void);

	// Demultiplexer
	HRESULT LoadAndConnectDemux(void);
	void UnloadDemux(void);
	
	// TIF (Transport Information Filter)
	HRESULT LoadAndConnectTif(void);
	void UnloadTif(void);

	// Pin �̐ڑ�
	HRESULT Connect(const WCHAR* pszName, IBaseFilter* pFrom, IBaseFilter* pTo);

	// �S�Ă� Pin ��ؒf����
	void DisconnectAll(IBaseFilter* pFilter);

	// COM���������ۂɍs���֐��iCOM������p�X���b�h����Ăяo�����j
	const BOOL _OpenTuner(void);
	void _CloseTuner(void);
	const BOOL _SetChannel(const DWORD dwSpace, const DWORD dwChannel);
	const float _GetSignalLevel(void);

protected:
	////////////////////////////////////////
	// �����o�ϐ�
	////////////////////////////////////////

	////////////////////////////////////////
	// COM������p�X���b�h�p
	////////////////////////////////////////

	enum enumCOMRequest {
		eCOMReqOpenTuner = 1,
		eCOMReqCloseTuner,
		eCOMReqSetChannel,
		eCOMReqGetSignalLevel,
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
	};

	struct COMProc {
		HANDLE hThread;					// �X���b�h�n���h��
		HANDLE hReqEvent;				// COMProc�X���b�h�ւ̃R�}���h���s�v��
		HANDLE hEndEvent;				// COMProc�X���b�h����̃R�}���h�����ʒm
		CRITICAL_SECTION csLock;		// �r���p
		enumCOMRequest nRequest;		// ���N�G�X�g
		COMReqParm uParam;				// �p�����[�^
		COMReqRetVal uRetVal;			// �߂�l
		HANDLE hTerminateRequest;		// �X���b�h�I���v��
		COMProc(void)
			: hThread(NULL),
			  hReqEvent(NULL),
			  hEndEvent(NULL),
			  hTerminateRequest(NULL)
		{
			hReqEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hEndEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			::InitializeCriticalSection(&csLock);
		};
		~COMProc(void)
		{
			::CloseHandle(hReqEvent);
			hReqEvent = NULL;
			::CloseHandle(hEndEvent);
			hEndEvent = NULL;
			::CloseHandle(hTerminateRequest);
			hTerminateRequest = NULL;
			::DeleteCriticalSection(&csLock);
		};
	};
	COMProc m_aCOMProc;

	////////////////////////////////////////
	// Decode������p�X���b�h�p
	////////////////////////////////////////

	struct DecodeProc {
		HANDLE hThread;					// �X���b�h�n���h��
		HANDLE hTerminateRequest;		// �X���b�h�I���v��
		DecodeProc(void)
			: hThread(NULL),
			hTerminateRequest(NULL)
		{
			hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		};
		~DecodeProc(void)
		{
			::CloseHandle(hTerminateRequest);
			hTerminateRequest = NULL;
		};
	};
	DecodeProc m_aDecodeProc;

	////////////////////////////////////////
	// �`���[�i�p�����[�^�֌W
	////////////////////////////////////////

	// INI�t�@�C���Ŏw��ł���GUID/FriendryName�ő吔
	static const unsigned int MAX_GUID = 10;

	// INI �t�@�C���Ŏw�肷��`���[�i�p�����[�^
	struct TunerParam {
		wstring sTunerGUID[MAX_GUID];			// Tuner��GUID�w��
		wstring sTunerFriendlyName[MAX_GUID];	// Tuner��FriendlyName�w��
		wstring sCaptureGUID[MAX_GUID];			// Capture��GUID�w��
		wstring sCaptureFriendlyName[MAX_GUID];	// Capture��FriendlyName�w��
		BOOL bCheckDeviceInstancePath;			// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
#ifdef UNICODE
		wstring sTunerName;						// GetTunerName�ŕԂ����O
#else
		string sTunerName;						// GetTunerName�ŕԂ����O
#endif
		wstring sDLLBaseName;					// �ŗLDLL
		TunerParam(void)
			: bCheckDeviceInstancePath(TRUE)
		{
		};
	};
	TunerParam m_aTunerParam;

	// Tone�M���ؑ֎���Wait����
	unsigned int m_nToneWait;

	// CH�ؑ֌��Lock�m�F����
	unsigned int m_nLockWait;

	// CH�ؑ֌��Lock�m�FDelay����
	unsigned int m_nLockWaitDelay;

	// CH�ؑ֌��Lock�m�FRetry��
	unsigned int m_nLockWaitRetry;

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	BOOL m_bLockTwice;

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����(msec)
	unsigned int m_nLockTwiceDelay;

#if 0
	// EnumTuningSpace�ŕԂ�Tuning Space��
#ifdef UNICODE
	wstring m_sTuningSpaceName;
#else
	string m_sTuningSpaceName;
#endif
#endif

	// SignalLevel �Z�o���@
	// 0 .. Strength�l / StrengthCoefficient
	// 1 .. Quality�l / QualityCoefficient
	// 2 .. (Strength�l / StrengthCoefficient) * (Quality�l / QualityCoefficient)
	unsigned int m_nSignalLevelCalcType;

	// Strength �l�␳�W��
	float m_fStrengthCoefficient;

	// Quality �l�␳�W��
	float m_fQualityCoefficient;

	////////////////////////////////////////
	// BonDriver �p�����[�^�֌W
	////////////////////////////////////////

	// �o�b�t�@1������̃T�C�Y
	DWORD m_dwBuffSize;

	// �ő�o�b�t�@��
	DWORD m_dwMaxBuffCount;

	// m_hOnDecodeEvent���Z�b�g����f�[�^�o�b�t�@��
	unsigned int m_nWaitTsCount;

	// WaitTsStream�ōŒ���ҋ@���鎞��
	unsigned int m_nWaitTsSleep;

	////////////////////////////////////////
	// �`�����l���p�����[�^
	////////////////////////////////////////

	// �`�����l���f�[�^
	struct ChData {
#ifdef UNICODE
		wstring sServiceName;
#else
		string sServiceName;
#endif
		unsigned int Satellite;			// �q����M�ݒ�ԍ�
		unsigned int Polarisation;		// �Δg��ޔԍ� (0 .. ���w��, 1 .. H, 2 .. V, 3 .. L, 4 .. R)
		unsigned int ModulationType;	// �ϒ������ݒ�ԍ�
		long Frequency;					// ���g��(KHz)
		long SID;						// �T�[�r�XID
		long TSID;						// �g�����X�|�[�g�X�g���[��ID
		long ONID;						// �I���W�i���l�b�g���[�NID
		ChData(void)
			: Satellite(0),
			  Polarisation(0),
			  ModulationType(0),
			  Frequency(0),
			  SID(-1),
			  TSID(-1),
			  ONID(-1)
		{
		};
	};

	// �`���[�j���O��ԃf�[�^
	struct TuningSpaceData {
#ifdef UNICODE
		wstring sTuningSpaceName;		// EnumTuningSpace�ŕԂ�Tuning Space��
#else
		string sTuningSpaceName;		// EnumTuningSpace�ŕԂ�Tuning Space��
#endif
		map<int, ChData*> Channels;		// �`�����l���ԍ��ƃ`�����l���f�[�^
		DWORD dwNumChannel;				// �`�����l����
		TuningSpaceData(void)
			: dwNumChannel(0)
		{
		};
		~TuningSpaceData(void)
		{
			for (map<int, ChData*>::iterator it = Channels.begin(); it != Channels.end(); it++) {
				SAFE_DELETE(it->second);
			}
			Channels.clear();
		};
	};

	// �`���[�j���O�X�y�[�X�ꗗ
	struct TuningData {
		map<int, TuningSpaceData*> Spaces;	// �`���[�j���O�X�y�[�X�ԍ��ƃf�[�^
		DWORD dwNumSpace;					// �`���[�j���O�X�y�[�X��
		TuningData(void)
			: dwNumSpace(0)
		{
		};
		~TuningData(void)
		{
			for (map<int, TuningSpaceData*>::iterator it = Spaces.begin(); it != Spaces.end(); it++) {
				SAFE_DELETE(it->second);
			}
			Spaces.clear();
		};
	};
	TuningData m_TuningData;

	// ini�t�@�C������CH�ݒ��Ǎ��ލۂ�
	// �g�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
	// FALSE .. �g�p����ĂȂ��ԍ����������ꍇ�O�l���A��������
	// TRUE .. �g�p����Ă��Ȃ��ԍ������̂܂܋�CH�Ƃ��Ċm�ۂ��Ă���
	BOOL m_bReserveUnusedCh;

	////////////////////////////////////////
	// �q����M�p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Ŏ�t����Δg��ސ�
	static const unsigned int POLARISATION_SIZE = 5;

	// CBonTuner�Ŏg�p����Δg��ޔԍ���Polarisation�^��Mapping
	static const Polarisation PolarisationMapping[POLARISATION_SIZE];

	// �Δg��ޖ���ini�t�@�C���ł̋L��
	static const WCHAR PolarisationChar[POLARISATION_SIZE];

	// ini�t�@�C���Őݒ�ł���ő�q���� + 1
	static const unsigned int MAX_SATELLITE = 5;

	// �q����M�ݒ�f�[�^
	struct Satellite {
		AntennaParam Polarisation[POLARISATION_SIZE];	// �Δg��ޖ��̃A���e�i�ݒ�
	};
	Satellite m_aSatellite[MAX_SATELLITE];

	// �`�����l�����̎��������Ɏg�p����q���̖���
	wstring m_sSatelliteName[MAX_SATELLITE];

	////////////////////////////////////////
	// �ϒ������p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Őݒ�ł���ő�ϒ�������
	static const unsigned int MAX_MODULATION = 4;

	// �ϒ������ݒ�f�[�^
	ModulationMethod m_aModulationType[MAX_MODULATION];

	// �`�����l�����̎��������Ɏg�p����ϒ������̖���
	wstring m_sModulationName[MAX_MODULATION];

	////////////////////////////////////////
	// BonDriver �֘A
	////////////////////////////////////////

	// ini�t�@�C����Path
	WCHAR m_szIniFilePath[_MAX_PATH + 1];

	// TS�o�b�t�@����p
	CRITICAL_SECTION m_csTSBuff;

	// Decode������TS�o�b�t�@����p
	CRITICAL_SECTION m_csDecodedTSBuff;

	// ��M�C�x���g
	HANDLE m_hOnStreamEvent;

	// �f�R�[�h�C�x���g
	HANDLE m_hOnDecodeEvent;

	// ��MTS�f�[�^�o�b�t�@
	struct TS_DATA{
		BYTE* pbyBuff;
		DWORD dwSize;
		TS_DATA(void)
			: pbyBuff(NULL),
			  dwSize(0)
		{
		};
		~TS_DATA(void) {
			SAFE_DELETE_ARRAY(pbyBuff);
		};
	};
	vector<TS_DATA*> m_TsBuff;

	// Decode�����̏I�����TS�f�[�^�o�b�t�@
	vector<TS_DATA*> m_DecodedTsBuff;

	// GetTsStream�ŎQ�Ƃ����o�b�t�@
	TS_DATA* m_LastBuff;

	// �o�b�t�@�쐬�p�e���|�����o�b�t�@
	BYTE *m_pbyRecvBuff;

	// �e���|�����o�b�t�@�̈ʒu
	DWORD m_dwBuffOffset;

	// �f�[�^��M��
	BOOL m_bRecvStarted;

	////////////////////////////////////////
	// �`���[�i�֘A
	////////////////////////////////////////

	// �`���[�i�f�o�C�X�r�������p
	HANDLE m_hSemaphore;

	// Graph
	ITuningSpace *m_pITuningSpace;
	IBaseFilter *m_pNetworkProvider;
	IBaseFilter *m_pTunerDevice;
	IBaseFilter *m_pCaptureDevice;
	IBaseFilter *m_pTsWriter;
	IBaseFilter *m_pDemux;
	IBaseFilter *m_pTif;
	IGraphBuilder *m_pIGraphBuilder;
	IMediaControl *m_pIMediaControl;
	CTsWriter *m_pCTsWriter;

	// �`���[�i�[��GUID��FriendryName
	wstring m_sTunerDisplayName;
	wstring m_sTunerFriendryName;

	// �`���[�i�[�̎g�p����TuningSpace/NetworkProvider���̎��
	enum enumTunerType {
		eTunerTypeDVBS = 1,
		eTunerTypeDVBT = 2,
	};
	DWORD m_nDVBSystemType;

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l 1 .. SPHD, 2 .. BS/CS110, 3 .. UHF/CATV
	DWORD m_nDefaultNetwork;

	// Tuner is opened
	BOOL m_bOpened;

	// �J�����g�`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwCurSpace;

	// �`���[�j���O�X�y�[�X�ԍ��s��
	static const DWORD SPACE_INVALID = 0xFFFFFFFF;

	// �J�����g�`�����l���ԍ�
	DWORD m_dwCurChannel;

	// �`�����l���ԍ��s��
	static const DWORD CHANNEL_INVALID = 0xFFFFFFFF;

	// ���݂̃g�[���ؑ֏��
	long m_nCurTone; // current tone signal state

	// �g�[���ؑ֏�ԕs��
	static const long TONE_UNKNOWN = -1;

	// TunerSpecial DLL module handle
	HMODULE m_hModuleTunerSpecials;

	// �`���[�i�ŗL�֐� IBdaSpecials
	IBdaSpecials *m_pIBdaSpecials;
	IBdaSpecials2a *m_pIBdaSpecials2;

	// �`���[�i�ŗL�̊֐����K�v���ǂ������������ʂ���DB
	// GUID ���L�[�� DLL ���𓾂�
	struct TUNER_SPECIAL_DLL {
		wstring sTunerGUID;
		wstring sDLLBaseName;
	};
	static const TUNER_SPECIAL_DLL aTunerSpecialData[];

	// �`�����l������������ inline �֐�
	inline int MakeChannelName(WCHAR* pszName, size_t size, CBonTuner::ChData* pChData)
	{
		long m = pChData->Frequency / 1000;
		long k = pChData->Frequency % 1000;
		if (k == 0)
			return ::swprintf_s(pszName, size, L"%s/%05ld%c/%s", m_sSatelliteName[pChData->Satellite].c_str(), m, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());
		else {
			return ::swprintf_s(pszName, size, L"%s/%05ld.%03ld%c/%s", m_sSatelliteName[pChData->Satellite].c_str(), m, k, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());

		}
	}
};

