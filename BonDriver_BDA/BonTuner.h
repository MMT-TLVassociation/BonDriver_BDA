//------------------------------------------------------------------------------
// File: BonTuner.h
//   Define CBonTuner class
//------------------------------------------------------------------------------
#pragma once

#include "common.h"

#include <Windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <atlbase.h>		// CComPtr
#include <strmif.h>
#include <tuner.h>

#include "IBonDriver2.h"
#include "LockChannel.h"
#include "TunerComboList.h"
#include "TSMF.h"
#include "EnumSettingValue.h"

#pragma warning (push)
#pragma warning (disable: 4310)
#include "..\3rdParties\muparser\include\muParser.h"
#pragma warning (pop)

#pragma comment(lib, "muparser.lib")

class TS_DATA;
class TS_BUFF;
class CTSMFParser;
class CBitRate;
class CCOMProc;
class CDecodeProc;
class IBdaSpecials;
class IBdaSpecials2b5;

struct IMediaControl;
struct ITsWriter;

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
	// �ÓI�����o�֐�
	////////////////////////////////////////

	// �K�v�ȐÓI�ϐ�������
	static void Init(HMODULE hModule);

	// �ÓI�ϐ��̉��
	static void Finalize(void);

	////////////////////////////////////////
	// �ÓI�����o�ϐ�
	////////////////////////////////////////

	// Dll�̃��W���[���n���h��
	static HMODULE st_hModule;

	// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
	static std::list<CBonTuner*> st_InstanceList;

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
	static int CALLBACK RecvProc(void* pParam, BYTE* pbData, size_t size);

	// �f�[�^��M�X�^�[�g�E��~
	void StartRecv(void);
	void StopRecv(void);

	// ini �t�@�C���Ǎ�
	void ReadIniFile(void);

	// �M����Ԃ��擾
	HRESULT GetSignalState(double* pdbStrength, double* pdbQuality, double* pdbLocked, double* pdbPresent);

	// �`�����l���ؑ�
	BOOL LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice);

	// �`���[�i�ŗLDll�̃��[�h
	HRESULT CheckAndInitTunerDependDll(IBaseFilter * pTunerDevice, std::wstring tunerGUID, std::wstring tunerFriendlyName);

	// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
	HRESULT CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);
		
	// �`���[�i�ŗL�֐��̃��[�h
	void LoadTunerDependCode(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);

	// �`���[�i�ŗL�֐���Dll�̉��
	void ReleaseTunerDependCode(void);

	// GraphBuilder
	HRESULT InitializeGraphBuilder(void);
	void CleanupGraph(void);
	HRESULT RunGraph(void);
	void StopGraph(void);

	struct DVBSystemTypeData;

	// TuningSpace
	HRESULT CreateTuningSpace(DVBSystemTypeData* pDVBSystemTypeData);
	void UnloadTuningSpace(void);
	HRESULT InitTuningSpace(void);

	// NetworkProvider
	HRESULT LoadNetworkProvider(DVBSystemTypeData* pDVBSystemTypeData);
	void UnloadNetworkProvider(void);

	// �`���[�i�E�L���v�`���f�o�C�X���܂߂Ă��ׂẴt�B���^�O���t�����[�h����Run�����݂�
	HRESULT LoadAndConnectDevice(unsigned int tunerGroup);

	// TunerDevice
	void UnloadTunerDevice(void);
	
	// CaptureDevice
	void UnloadCaptureDevice(void);
	
	// TsWriter
	HRESULT LoadAndConnectTsWriter(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);
	void UnloadTsWriter(void);

	// Demultiplexer
	HRESULT LoadAndConnectDemux(void);
	void UnloadDemux(void);
	
	// TIF (Transport Information Filter)
	HRESULT LoadAndConnectTif(void);
	void UnloadTif(void);

	// TsWriter/Demultiplexer/TIF��Load&Connect��Run����
	HRESULT LoadAndConnectMiscFilters(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	HRESULT LoadTunerSignalStatisticsTunerNode(void);
	HRESULT LoadTunerSignalStatisticsDemodNode(void);
	void UnloadTunerSignalStatistics(void);

	// Pin �̐ڑ�
	HRESULT Connect(IBaseFilter* pFrom, IBaseFilter* pTo);

	// �S�Ă� Pin ��ؒf����
	void DisconnectAll(IBaseFilter* pFilter);

	// CCOM������p�X���b�h����Ăяo�����֐�
	BOOL _OpenTuner(void);
	void _CloseTuner(BOOL putoff);
	BOOL _SetChannel(const DWORD dwSpace, const DWORD dwChannel);
	float _GetSignalLevel(void);
	BOOL _IsTunerOpening(void);
	DWORD _GetCurSpace(void);
	DWORD _GetCurChannel(void);


protected:
	////////////////////////////////////////
	// �����o�ϐ�
	////////////////////////////////////////

	// COM������p�X���b�h�p
	CCOMProc* m_pCOMProc = NULL;

	// Decode������p�X���b�h�p
	CDecodeProc* m_pDecodeProc = NULL;
	BOOL m_bNeedDecodeProc = FALSE;

	////////////////////////////////////////
	// �`���[�i�p�����[�^�֌W
	////////////////////////////////////////

	// INI �t�@�C���Ŏw��ł���TunerGroup�ő吔
	static constexpr unsigned int MAX_TUNER_GROUP = 10U;

	// INI�t�@�C���Ŏw��ł���GUID/FriendlyName�ő吔
	static constexpr unsigned int MAX_GUID = 100U;

	// �`���[�i�E�L���v�`���f�o�C�X�̓Ǎ��݃��X�g�擾�N���X
	CTunerComboList m_TunerComboList;

	// GetTunerName�ŕԂ����O
	std::basic_string<TCHAR> m_sTunerName = L"DVB-S2";

	// �ŗLDLL��
	std::wstring m_sDLLBaseName;

	// Tone�M���ؑ֎���Wait����
	unsigned int m_nToneWait = 0;

	// CH�ؑ֌��Lock�m�F����
	unsigned int m_nLockWait = 2000;

	// CH�ؑ֌��Lock�m�FDelay����
	unsigned int m_nLockWaitDelay = 0;

	// CH�ؑ֌��Lock�m�FRetry��
	unsigned int m_nLockWaitRetry = 0;

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	BOOL m_bLockTwice = FALSE;

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����(msec)
	unsigned int m_nLockTwiceDelay = 100;

	// SignalLocked�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogSignalLocked = 0;

	// BitRate�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogBitRate = 0;

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	unsigned int m_nReOpenWhenGiveUpReLock = 0;

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	BOOL m_bTryAnotherTuner = FALSE;

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	BOOL m_bBackgroundChannelLock = FALSE;

	// SignalLevel �Z�o���@
	EnumSettingValue::SignalLevelCalcType m_nSignalLevelCalcType = EnumSettingValue::SignalLevelCalcType::SSStrength;
	BOOL m_bSignalLevelGetTypeSS = FALSE;			// SignalLevel �Z�o�� RF Tuner Node �� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLevelGetTypeTuner = FALSE;		// SignalLevel �Z�o�� ITuner ���g�p����
	BOOL m_bSignalLevelGetTypeDemodSS = FALSE;		// SignalLevel �Z�o�� Demodulator Node �� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLevelGetTypeBR= FALSE;			// SignalLevel �Z�o�� �r�b�g���[�g�l���g�p����
	BOOL m_bSignalLevelNeedStrength = FALSE;		// SignalLevel �Z�o�� SignalStrength �l���g�p����
	BOOL m_bSignalLevelNeedQuality = FALSE;			// SignalLevel �Z�o�� SignalQuality �l���g�p����

	// SignalLevel�Z�o�pmuparser
	mu::Parser m_muParser;

	// ���݂�Strength�l
	double m_dbStrength = 0.0;

	// ���݂�Quality�l
	double m_dbQuality = 0.0;

	// ���݂�Lock��Ԓl
	double m_dbSignalLocked = 0.0;

	// ���݂̐M���񋟏�Ԓl
	double m_dbSignalPresent = 0.0;

	// Strength �l�␳�W��
	double m_dbStrengthCoefficient = 1.0;

	// Quality �l�␳�W��
	double m_dbQualityCoefficient = 1.0;

	// Strength �l�␳�o�C�A�X
	double m_dbStrengthBias = 0.0;

	// Quality �l�␳�o�C�A�X
	double m_dbQualityBias = 0.0;

	// SignalLevel�Z�o�p���[�U�[��`����
	std::wstring m_sSignalLevelCalcFormula;

	// �`���[�j���O��Ԃ̔��f���@
	EnumSettingValue::SignalLockedJudgeType m_nSignalLockedJudgeType = EnumSettingValue::SignalLockedJudgeType::SS;
	BOOL m_bSignalLockedJudgeTypeSS = FALSE;		// �`���[�j���O��Ԃ̔��f�� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLockedJudgeTypeTuner = FALSE;		// �`���[�j���O��Ԃ̔��f�� ITuner ���g�p����
	BOOL m_bSignalLockedJudgeTypeDemodSS = FALSE;	// �`���[�j���O��Ԃ̔��f�� Demodulator Node �� IBDA_SignalStatistics ���g�p����

	////////////////////////////////////////
	// BonDriver �p�����[�^�֌W
	////////////////////////////////////////

	// �o�b�t�@1������̃T�C�Y
	size_t m_nBuffSize = 1024;

	// �ő�o�b�t�@��
	size_t m_nMaxBuffCount = 512;

	// m_hOnDecodeEvent���Z�b�g����f�[�^�o�b�t�@��
	unsigned int m_nWaitTsCount = 1;

	// WaitTsStream�ōŒ���ҋ@���鎞��
	unsigned int m_nWaitTsSleep = 100;

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	BOOL m_bAlwaysAnswerLocked = FALSE;

	// COMProcThread�̃X���b�h�v���C�I���e�B
	int m_nThreadPriorityCOM = THREAD_PRIORITY_ERROR_RETURN;

	// DecodeProcThread�̃X���b�h�v���C�I���e�B
	int m_nThreadPriorityDecode = THREAD_PRIORITY_ERROR_RETURN;

	// �X�g���[���X���b�h�v���C�I���e�B
	int m_nThreadPriorityStream = THREAD_PRIORITY_ERROR_RETURN;

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	unsigned int m_nPeriodicTimer = 0;

	////////////////////////////////////////
	// �`�����l���p�����[�^
	////////////////////////////////////////

	// �`�����l���f�[�^
	struct ChData {
		std::basic_string<TCHAR> sServiceName;	// EnumChannelName�ŕԂ��`�����l����
		unsigned int Satellite = 0;			// �q����M�ݒ�ԍ�
		unsigned int Polarisation = 0;		// �Δg��ޔԍ� (0 .. ���w��, 1 .. H, 2 .. V, 3 .. L, 4 .. R)
		unsigned int ModulationType = 0;	// �ϒ������ݒ�ԍ�
		long Frequency = 0;					// ���g��(KHz)
		union {
			long SID = -1;					// �T�[�r�XID
			long PhysicalChannel;			// ATSC / Digital Cable�p
		};
		union {
			long TSID = -1;					// �g�����X�|�[�g�X�g���[��ID
			long Channel;					// ATSC / Digital Cable�p
		};
		union {
			long ONID = -1;					// �I���W�i���l�b�g���[�NID
			long MinorChannel;				// ATSC / Digital Cable�p
		};
		long MajorChannel = -1;				// Digital Cable�p
		long SourceID = -1;					// Digital Cable�p
		BOOL LockTwiceTarget = FALSE;		// CH�ؑ֓���������I��2�x�s���Ώ�
	};

	// �`���[�j���O��ԃf�[�^
	struct TuningSpaceData {
		std::basic_string<TCHAR> sTuningSpaceName;								// EnumTuningSpace�ŕԂ�Tuning Space��
		long FrequencyOffset = 0;												// ���g���I�t�Z�b�g�l
		unsigned int DVBSystemTypeNumber = 0;									// TuningSpace�̎�ޔԍ�
		EnumSettingValue::TSMFMode TSMFMode = EnumSettingValue::TSMFMode::Off;	// TSMF�̏������[�h
		std::map<unsigned int, ChData> Channels;								// �`�����l���ԍ��ƃ`�����l���f�[�^
		DWORD dwNumChannel = 0;													// �`�����l����
	};

	// �`���[�j���O�X�y�[�X�ꗗ
	struct TuningData {
		std::map<unsigned int, TuningSpaceData> Spaces;	// �`���[�j���O�X�y�[�X�ԍ��ƃf�[�^
		DWORD dwNumSpace = 0;							// �`���[�j���O�X�y�[�X��
	};
	TuningData m_TuningData;

	////////////////////////////////////////
	// �q����M�p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Ŏ�t����Δg��ސ�
	static constexpr unsigned int POLARISATION_SIZE = 5U;

	// CBonTuner�Ŏg�p����Δg��ޔԍ���Polarisation�^��Mapping
	static constexpr Polarisation PolarisationMapping[POLARISATION_SIZE] = {
		BDA_POLARISATION_NOT_SET,
		BDA_POLARISATION_LINEAR_H,
		BDA_POLARISATION_LINEAR_V,
		BDA_POLARISATION_CIRCULAR_L,
		BDA_POLARISATION_CIRCULAR_R
	};

	// �Δg��ޖ���ini�t�@�C���ł̋L��
	static constexpr WCHAR PolarisationChar[POLARISATION_SIZE] = {
		L' ',
		L'H',
		L'V',
		L'L',
		L'R'
	};

	// ini�t�@�C���Őݒ�ł���ő�q���� + 1
	static constexpr unsigned int MAX_SATELLITE = 10U;

	// �q����M�ݒ�f�[�^
	struct Satellite {
		AntennaParam Polarisation[POLARISATION_SIZE];	// �Δg��ޖ��̃A���e�i�ݒ�
	};
	Satellite m_aSatellite[MAX_SATELLITE];

	// �`�����l�����̎��������Ɏg�p����q���̖���
	std::wstring m_sSatelliteName[MAX_SATELLITE];

	////////////////////////////////////////
	// �ϒ������p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Őݒ�ł���ő�ϒ�������
	static constexpr unsigned int MAX_MODULATION = 10U;

	// �ϒ������ݒ�f�[�^
	ModulationMethod m_aModulationType[MAX_MODULATION];

	// �`�����l�����̎��������Ɏg�p����ϒ������̖���
	std::wstring m_sModulationName[MAX_MODULATION];

	////////////////////////////////////////
	// BonDriver �֘A
	////////////////////////////////////////

	// ini�t�@�C����Path
	std::wstring m_sIniFilePath;

	// ��M�C�x���g
	HANDLE m_hOnStreamEvent = NULL;

	// �f�R�[�h�����C�x���g
	HANDLE m_hOnDecodeEvent = NULL;

	// WaitTsStream�Ŏg�p����C�x���g�̃|�C���^
	HANDLE* m_phOnWaitTsEvent = NULL;

	// ��MTS�f�[�^�o�b�t�@
	TS_BUFF* m_pTsBuff = NULL;

	// Decode�����̏I�����TS�f�[�^�o�b�t�@
	TS_BUFF* m_pDecodedTsBuff = NULL;

	// GetTsStream�Ŏg�p����f�[�^�o�b�t�@�̃|�C���^
	TS_BUFF** m_ppGetTsBuff = NULL;

	// GetTsStream�ŎQ�Ƃ����o�b�t�@
	TS_DATA* m_LastBuff = NULL;

	// �f�[�^��M��
	BOOL m_bRecvStarted = FALSE;

	// �v���Z�X�n���h��
	HANDLE m_hProcess = NULL;

	// �X�g���[���X���b�h�̃n���h��
	HANDLE m_hStreamThread = NULL;

	// �X�g���[���X���b�h�n���h���ʒm�t���O
	BOOL m_bIsSetStreamThread = FALSE;

	// �r�b�g���[�g�v�Z�p
	CBitRate* m_pBitRate = NULL;
	BOOL m_bNeedBitRate = FALSE;

	// TSMF�����p
	CTSMFParser* m_pTSMFParser = NULL;
	BOOL m_bNeedTSMFParser = FALSE;

	////////////////////////////////////////
	// �`���[�i�֘A
	////////////////////////////////////////

	// �`���[�i�f�o�C�X�r�������p
	HANDLE m_hSemaphore = NULL;

	// Graph
	CComPtr<IGraphBuilder> m_pIGraphBuilder;	// Filter Graph Manager �� IGraphBuilder interface
	CComPtr<IMediaControl> m_pIMediaControl;	// Filter Graph Manager �� IMediaControl interface
	CComPtr<IBaseFilter> m_pNetworkProvider;	// NetworkProvider �� IBaseFilter interface
	CComPtr<ITuner> m_pITuner;					// NetworkProvider �� ITuner interface
	CComPtr<ITuningSpace> m_pITuningSpace;		// NetworkProvider ���g�p���� TuningSpace �� ITuningSpace interface
	CComPtr<IBaseFilter> m_pTunerDevice;		// Tuner Device �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pCaptureDevice;		// Capture Device �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pTsWriter;			// CTsWriter �� IBaseFilter interface
	CComPtr<ITsWriter> m_pITsWriter;			// CTsWriter �� ITsWriter interface
	CComPtr<IBaseFilter> m_pDemux;				// MPEG2 Demultiplexer �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pTif;				// MPEG2 Transport Information Filter �� IBaseFilter interface

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsTunerNode;
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsDemodNode;

	// �`���[�i�[�̎g�p����TuningSpace�̎�ރf�[�^
	struct DVBSystemTypeData {
		EnumSettingValue::TunerType nDVBSystemType = EnumSettingValue::TunerType::None;								// �`���[�i�[�̎g�p����TuningSpace�̎��
		EnumSettingValue::TuningSpace nTuningSpace = EnumSettingValue::TuningSpace::Auto;							// �g�p����TuningSpace �I�u�W�F�N�g
		EnumSettingValue::Locator nLocator = EnumSettingValue::Locator::Auto;										// �g�p����Locator �I�u�W�F�N�g
		EnumSettingValue::NetworkType nITuningSpaceNetworkType = EnumSettingValue::NetworkType::Auto;				// ITuningSpace�ɐݒ肷��NetworkType
		EnumSettingValue::DVBSystemType nIDVBTuningSpaceSystemType = EnumSettingValue::DVBSystemType::Auto;			// IDVBTuningSpace�ɐݒ肷��SystemType
		EnumSettingValue::TunerInputType nIAnalogTVTuningSpaceInputType = EnumSettingValue::TunerInputType::Auto;	// IAnalogTVTuningSpace�ɐݒ肷��InputType
		EnumSettingValue::NetworkProvider nNetworkProvider = EnumSettingValue::NetworkProvider::Auto;				// �`���[�i�[�Ɏg�p����NetworkProvider
		unsigned int nTunerGroup = 0;																				// �g�p����TunerGroup�ԍ�
	};

	// TuningSpace�̎�ރf�[�^�x�[�X
	struct DVBSystemTypeDB {
		std::map<unsigned int, DVBSystemTypeData> SystemType;	// TuningSpace�̎�ޔԍ���TuningSpace�̎�ރf�[�^
		unsigned int nNumType = 0;								// TuningSpace�̎�ސ�
		BOOL IsExist(unsigned int number)
		{
			auto it = SystemType.find(number);
			if (it == SystemType.end())
				return FALSE;
			return TRUE;
		}
	};
	DVBSystemTypeDB m_DVBSystemTypeDB;

	// �ύX�����݂�TuningSpace�̎�ޔԍ�
	long m_nTargetDvbSystemTypeNum = 0;

	// �J�����gTuningSpace�̎�ޔԍ�
	long m_nCurrentDvbSystemTypeNum = -1;

	// ini�t�@�C���Œ�`�ł���ő�TuningSpace�̎�ރf�[�^��
	static constexpr unsigned int MAX_DVB_SYSTEM_TYPE = 10U;

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	EnumSettingValue::DefaultNetwork m_nDefaultNetwork = EnumSettingValue::DefaultNetwork::SPHD;

	// Tuner is opened
	BOOL m_bOpened = FALSE;

	// �`���[�j���O�X�y�[�X�ԍ��s��
	static constexpr DWORD SPACE_INVALID = 0xFFFFFFFFUL;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwTargetSpace = SPACE_INVALID;

	// �J�����g�`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwCurSpace = SPACE_INVALID;

	// �`�����l���ԍ��s��
	static constexpr DWORD CHANNEL_INVALID = 0xFFFFFFFFUL;

	// SetChannel()�����݂��`�����l���ԍ�
	DWORD m_dwTargetChannel = CHANNEL_INVALID;

	// �J�����g�`�����l���ԍ�
	DWORD m_dwCurChannel = CHANNEL_INVALID;

	// �g�[���ؑ֏�ԕs��
	static constexpr long TONE_UNKNOWN = -1L;

	// ���݂̃g�[���ؑ֏��
	long m_nCurTone = TONE_UNKNOWN;

	// �Ō��LockChannel���s�������̃`���[�j���O�p�����[�^
	TuningParam m_LastTuningParam;

	// TunerSpecial DLL module handle
	HMODULE m_hModuleTunerSpecials = NULL;

	// �`���[�i�ŗL�֐� IBdaSpecials
	IBdaSpecials* m_pIBdaSpecials = NULL;
	IBdaSpecials2b5* m_pIBdaSpecials2 = NULL;

	// �`���[�i�ŗL�̊֐����K�v���ǂ������������ʂ���DB
	// GUID ���L�[�� DLL ���𓾂�
	struct TUNER_SPECIAL_DLL {
		const WCHAR * const sTunerGUID;
		const WCHAR * const sDLLBaseName;
	};
	static constexpr TUNER_SPECIAL_DLL aTunerSpecialData[] = {
		// �����̓v���O���}����������Ȃ��Ǝv���̂ŁA�v���O��������GUID ���������ɐ��K�����Ȃ��̂ŁA
		// �ǉ�����ꍇ�́AGUID�͏������ŏ����Ă�������

		/* TBS6980A */
		{ L"{e9ead02c-8b8c-4d9b-97a2-2ec0324360b1}", L"TBS" },

		/* TBS6980B, Prof 8000 */
		{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

		/* Prof 7300, 7301, TBS 8920 */
		{ L"{91b0cc87-9905-4d65-a0d1-5861c6f22cbf}", L"TBS" },	// 7301 �͌ŗL�֐��łȂ��Ă�OK������

		/* TBS 6920 */
		{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

		/* Prof Prof 7500, Q-BOX II */
		{ L"{b45b50ff-2d09-4bf2-a87c-ee4a7ef00857}", L"TBS" },

		/* DVBWorld 2002, 2004, 2006 */
		{ L"{4c807f36-2db7-44ce-9582-e1344782cb85}", L"DVBWorld" },

		/* DVBWorld 210X, 2102X, 2104X */
		{ L"{5a714cad-60f9-4124-b922-8a0557b8840e}", L"DVBWorld" },

		/* DVBWorld 2005 */
		{ L"{ede18552-45e6-469f-93b5-27e94296de38}", L"DVBWorld" }, // 2005 �͌ŗL�֐��͕K�v�Ȃ�����

		{ L"", L"" }, // terminator
	};

	// �`�����l������������ inline �֐�
	inline std::basic_string<TCHAR> MakeChannelName(const CBonTuner::ChData* const pChData)
	{
		std::basic_string<TCHAR> format;
		long m = pChData->Frequency / 1000;
		long k = pChData->Frequency % 1000;
		if (k == 0)
			format = _T("%s/%05ld%c/%s");
		else
			format = _T("%s/%05ld.%03ld%c/%s");
		return common::TStringPrintf(format.c_str(), m_sSatelliteName[pChData->Satellite].c_str(), m, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());
	}
};

