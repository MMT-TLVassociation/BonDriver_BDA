#pragma once

#include <string>
#include <map>
#include "CIniFileAccess.h"

namespace EnumSettingValue {
	// SignalLevel �Z�o���@
	enum class SignalLevelCalcType : int {
		SSMin = 0,
		SSStrength = 0,			// RF Tuner Node��IBDA_SignalStatistics����擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		SSQuality = 1,			// RF Tuner Node��IBDA_SignalStatistics����擾����Quality�l �� QualityCoefficient �{ QualityBias
		SSMul = 2,				// RF Tuner Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		SSAdd = 3,				// RF Tuner Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		SSFormula = 9,			// RF Tuner Node��IBDA_SignalStatistics����擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		SSMax = 9,
		TunerMin = 10,
		TunerStrength = 10,		// ITuner::get_SignalStrength�Ŏ擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		TunerQuality = 11,		// ITuner::get_SignalStrength�Ŏ擾����Quality�l �� QualityCoefficient �{ QualityBias
		TunerMul = 12,			// ITuner::get_SignalStrength�Ŏ擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		TunerAdd = 13,			// ITuner::get_SignalStrength�Ŏ擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		TunerFormula = 19,		// ITuner::get_SignalStrength�Ŏ擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		TunerMax = 19,
		DemodSSMin = 20,
		DemodSSStrength = 20,	// Demodulator Node��IBDA_SignalStatistics����擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		DemodSSQuality = 21,	// Demodulator Node��IBDA_SignalStatistics����擾����Quality�l �� QualityCoefficient �{ QualityBias
		DemodSSMul = 22,		// Demodulator Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		DemodSSAdd = 23,		// Demodulator Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		DemodSSFormula = 29,	// Demodulator Node��IBDA_SignalStatistics����擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		DemodSSMax = 29,
		BR = 100,				// �r�b�g���[�g�l(Mibps)
	};

	// �`���[�j���O��Ԃ̔��f���@
	enum class SignalLockedJudgeType : int {
		Always = 0,				// ��Ƀ`���[�j���O�ɐ������Ă����ԂƂ��Ĕ��f����
		SS = 1,					// RF Tuner Node �� IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
		Tuner = 2,				// ITuner::get_SignalStrength�Ŏ擾�����l�Ŕ��f����
		DemodSS = 3,			// Demodulator Node �� IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
	};

	// �`���[�i�[�̎g�p����TuningSpace�̎��
	enum class TunerType : int {
		None = -1,
		DVBS = 1,				// DBV-S/DVB-S2
		DVBT = 2,				// DVB-T
		DVBC = 3,				// DVB-C
		DVBT2 = 4,				// DVB-T2
		ISDBS = 11,				// ISDB-S
		ISDBT = 12,				// ISDB-T
		ISDBC = 13,				// ISDB-C
		ATSC_Antenna = 21,		// ATSC
		ATSC_Cable = 22,		// ATSC Cable
		DigitalCable = 23,		// Digital Cable
	};

	// �g�p����TuningSpace �I�u�W�F�N�g
	enum class TuningSpace : int {
		Auto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		DVB = 1,				// DVBTuningSpace
		DVBS = 2,				// DVBSTuningSpace
		AnalogTV = 21,			// AnalogTVTuningSpace
		ATSC = 22,				// ATSCTuningSpace
		DigitalCable = 23,		// DigitalCableTuningSpace
	};

	// �g�p����Locator �I�u�W�F�N�g
	enum class Locator : int {
		Auto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		DVBT = 1,				// DVBTLocator
		DVBT2 = 2,				// DVBTLocator2
		DVBS = 3,				// DVBSLocator
		DVBC = 4,				// DVBCLocator
		ISDBS = 11,				// ISDBSLocator
		ATSC = 21,				// ATSCLocator
		DigitalCable = 22,		// DigitalCableLocator
	};

	// ITuningSpace�ɐݒ肷��NetworkType
	enum class NetworkType : int {
		Auto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		DVBT = 1,				// STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE
		DVBS = 2,				// STATIC_DVB_SATELLITE_TV_NETWORK_TYPE
		DVBC = 3,				// STATIC_DVB_CABLE_TV_NETWORK_TYPE
		ISDBT = 11,				// STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE
		ISDBS = 12,				// STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE
		ISDBC = 13,				// STATIC_ISDB_CABLE_TV_NETWORK_TYPE
		ATSC = 21,				// STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE
		DigitalCable = 22,		// STATIC_DIGITAL_CABLE_NETWORK_TYPE
		BSkyB = 101,			// STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE
		DIRECTV = 102,			// STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE
		EchoStar = 103,			// STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE
	};

	// IDVBTuningSpace�ɐݒ肷��SystemType
	enum class DVBSystemType : int {
		Auto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		DVBC = 0,				// DVB_Cable
		DVBT = 1,				// DVB_Terrestrial
		DVBS = 2,				// DVB_Satellite
		ISDBT = 3,				// ISDB_Terrestrial
		ISDBS = 4,				// ISDB_Satellite
	};

	// IAnalogTVTuningSpace�ɐݒ肷��InputType
	enum class TunerInputType : int {
		Auto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		Cable = 0,				// TunerInputCable
		Antenna = 1,			// TunerInputAntenna
	};

	// �`���[�i�[�Ɏg�p����NetworkProvider 
	enum class NetworkProvider : int {
		Auto = 0,				// DVBSystemType�̒l�ɂ���Ď����I��
		Generic = 1,			// Microsoft Network Provider
		DVBS = 2,				// Microsoft DVB-S Network Provider
		DVBT = 3,				// Microsoft DVB-T Network Provider
		DVBC = 4,				// Microsoft DVB-C Network Provider
		ATSC = 5,				// Microsoft ATSC Network Provider
	};

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	enum class DefaultNetwork : int {
		None = 0,				// �ݒ肵�Ȃ�
		SPHD = 1,				// SPHD
		BSCS = 2,				// BS/CS110
		UHF = 3,				// UHF/CATV
		Dual = 4,				// Dual Mode (BS/CS110��UHF/CATV)
	};

	// TSMF�����ݒ�
	enum class TSMFMode : int {
		Off = 0,				// �������Ȃ�
		TSID = 1,				// TSID�w�胂�[�h
		Relarive = 2,			// ���Δԍ��w�胂�[�h
	};

	extern CIniFileAccess::Map mapThreadPriority;
	extern CIniFileAccess::Map mapModulationType;
	extern CIniFileAccess::Map mapFECMethod;
	extern CIniFileAccess::Map mapBinaryConvolutionCodeRate;
	extern CIniFileAccess::Map mapTuningSpaceType;
	extern CIniFileAccess::Map mapSpecifyTuningSpace;
	extern CIniFileAccess::Map mapSpecifyLocator;
	extern CIniFileAccess::Map mapSpecifyITuningSpaceNetworkType;
	extern CIniFileAccess::Map mapSpecifyIDVBTuningSpaceSystemType;
	extern CIniFileAccess::Map mapSpecifyIAnalogTVTuningSpaceInputType;
	extern CIniFileAccess::Map mapNetworkProvider;
	extern CIniFileAccess::Map mapDefaultNetwork;
	extern CIniFileAccess::Map mapSignalLevelCalcType;
	extern CIniFileAccess::Map mapSignalLockedJudgeType;
	extern CIniFileAccess::Map mapDiSEqC;
	extern CIniFileAccess::Map mapTSMFMode;
}
