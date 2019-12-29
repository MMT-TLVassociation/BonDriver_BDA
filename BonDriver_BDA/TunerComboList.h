#pragma once

#include <Windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>

struct IBaseFilter;
class CDSFilterEnum;

class CTunerComboList
{
public:
	// �R���X�g���N�^
	CTunerComboList(void);

	// �����f�[�^�̃N���A
	void ClearSearchData(void);

	// �����f�[�^�̒ǉ�
	void AddSearchData(unsigned int tunerGroup, unsigned int order, std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName);

	// �����f�[�^����Tuner / Capture�̑g����DataBase���쐬
	void BuildComboDB(void);

	// Tuner���X�g�̐擪�ֈړ�
	BOOL ResetTuner(unsigned int group);

	// Tuner���X�g�̎��̑Ώۂֈړ�
	BOOL NextTuner(void);

	// Tuner�̃f�[�^���擾
	void GetTunerData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order);

	// Tuner�f�o�C�X�t�B���^���擾
	HRESULT GetTunerFilter(IBaseFilter** ppFilter);

	// Capture���X�g�̐擪�ֈړ�
	BOOL ResetCapture(void);

	// Capture���X�g�̎��̑Ώۂֈړ�
	BOOL NextCapture(void);

	// Capture�̃f�[�^���擾
	void GetCaptureData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order);

	// Capture�f�o�C�X�t�B���^���擾
	HRESULT GetCaptureFilter(IBaseFilter** ppFilter);

	// �g�p����Tuner���L��
	void Remenber(void);

	// �L������Tuner���N���A
	void Forget();

	// �L������Tuner��List�̍Ō���Ɉړ�����
	void PutOff();

private:
	static void ReassembleDeviceInstancePath(std::wstring* deviceInstancePath);

public:
	// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ��ꍇTRUE
	BOOL bNotExistCaptureDevice;

	// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
	BOOL bCheckDeviceInstancePath;

private:
	// �`���[�i�E�L���v�`�������Ɏg�p����GUID�������FriendlyName������̑g����
	struct TunerSearchData {
		std::wstring TunerGUID;					// Tuner�f�o�C�X��Display Name
		std::wstring TunerFriendlyName;			// Tuner�f�o�C�X��Frriendly Name
		std::wstring CaptureGUID;				// Capture�f�o�C�X��Display Name
		std::wstring CaptureFriendlyName;		// Capture�f�o�C�X��Frriendly Name
		// �f�t�H���g�R���X�g���N�^
		TunerSearchData(void);
		// �R���X�g���N�^
		TunerSearchData(std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName);
	};

	// TunerGroup 1����INI�t�@�C���Ŏw�肳�ꂽ Guid / FriendlyName / CaptureGuid / CaptureFriendlyName ��List
	typedef std::map<unsigned int, TunerSearchData> TunerMap;

	// TunerGroup 10���� Guid / FriendlyName / CaptureGuid / CaptureFriendlyName �� DataBase
	std::map<unsigned int, TunerMap> TunerSearchDB;

	// DS�t�B���^�[�� CDSFilterEnum
	CDSFilterEnum* pDSFilterEnumTuner;
	CDSFilterEnum* pDSFilterEnumCapture;

	// Tuner / Capture �P�̂�DS�t�B���^�[���
	struct DSListData {
		std::wstring GUID;						// Display Name
		std::wstring FriendlyName;				// Friendly Name
		ULONG Order;							// CDSFilterEnum�񋓂�Order�ԍ�
		// �R���X�g���N�^
		DSListData(std::wstring _GUID, std::wstring _FriendlyName, ULONG _Order)
			: GUID(_GUID),
			FriendlyName(_FriendlyName),
			Order(_Order)
		{
		}
	};

	// Tuner�P�̂Ƃ���ɕR�Â�������Capture�̑g����
	struct TunerCaptureListData {
		DSListData Tuner;						// Tuner�̏��
		std::vector<DSListData> CaptureList;	// Tuner�ƕR�Â��镡����Capture�̏��
		// �R���X�g���N�^
		TunerCaptureListData(std::wstring TunerGUID, std::wstring TunerFriendlyName, ULONG TunerOrder)
			: Tuner(TunerGUID, TunerFriendlyName, TunerOrder)
		{
		}
		// �R���X�g���N�^
		TunerCaptureListData(DSListData _Tuner)
			: Tuner(_Tuner)
		{
		}
	};

	// TunerGroup 1���� Tuner��� / Capture���̑g����List
	typedef std::list<TunerCaptureListData> TunerCaptureList;

	// TunerGroup 10���� Tuner��� / Capture��� DataBase
	std::map<unsigned int, TunerCaptureList> TunerComboDB;

	// Tuner��� / Capture��� DataBase�̃C�e���[�^
	TunerCaptureList::iterator itTuner;

	// Captur���̃C�e���[�^
	std::vector<DSListData>::iterator itCapture;

	// �J�����gTunerGroup�ԍ�
	unsigned int CurrentGroup;

	// Connect���������g�����L���p
	TunerCaptureList::iterator LastConnecttedTuner;
	
	// Connect��������TunerGroup�ԍ��L���p
	unsigned int LastConnecttedGroup;

	// Connect���������L������������LastConnecttedGroup�l
	static constexpr unsigned int NO_MEMORY = (unsigned int)-1;
};
