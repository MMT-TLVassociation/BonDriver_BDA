# Release Note

## 2014-03-14

- BonDriver_BDA���Ƃ��Č��J

## 2014-03-19

- `BpnDriver`������Ɉ�ʕی�ᔽ���N�����s����C��

## 2014-03-23

- TS�擾�p�̃t�B���^�O���t�Ƃ���`Infinite Pin Tee`�̎g�p����߁A�y�ʂȂ��̂ɏ���������
- �t�B���^�O���t�̉�����L���v�`�����`���[�i�̏�������`���[�i���L���v�`���ɕύX�i���������[�N���N�������Ƃ����邽�߁j
- �`���[�i�̃I�[�v���Ɏ��s������I�[�v���o���Ȃ��Ȃ�����C��
- ���̑��A�ׂ����C��

## 2015-08-07

- ini�t�@�C���ŃL���v�`���f�o�C�X�̎w�肪�ł���悤�ɂ����i`CaptureGuid`�E`CaptureFriendlyName`�j
- ini�t�@�C���ł�`ChannelLock`�֌W�̍��ڂ�ǉ��i`ChannelLockWaitDelay`�E`ChannelLockWaitRetry`�E`ChannelLockTwice`�E`ChannelLockTwiceDelay`�j
- �T���v��ini�t�@�C���Ńf�t�H���g�l�𖾎��w�肷��悤�ɕύX
- �T���v��ini�t�@�C����CH�ݒ���ŐV�̂��̂ɍX�V
- �����̊��ŏ������Ɏ��s����s����C��
- 2�ڈȍ~�̃`���[�i���J���Ȃ��s����C��
- �`���[�i�[�E�L���v�`���̑g�������������郍�W�b�N��ύX
- ���̑��A�ׂ����C��
- �J������Visual Studio Community 2013�ɕύX

## 2015-09-21

- ini�t�@�C����`Modulation`�Z�N�V������V�݂��A�ϒ������p�����[�^�̐ݒ肪�ł���悤�ɂ���
    - ��L�ύX�ɔ����A`put_Modulation()`�̃f�t�H���g�l��`BDA_MOD_NBC_8PSK`/`BDA_MOD_NBC_QPSK`���g�p����悤�ɂ��܂���
    - ��肪��������ꍇ��ini�t�@�C�����̉��L2���ڂ�ύX���Ă݂Ă�������
    - `ModulationType0Modulation=20`
    - `ModulationType1Modulation=27`
- CH�ݒ�ɍ����~�Δg�E�E���~�Δg�̎w�肪�ł���悤�ɂ���
- `CoInitialize`/`CoUnInitialize`���܂ނ��ׂĂ�COM������Ɨ������ʃX���b�h�ōs���悤�ɂ���
- �J������Visual Studio Community 2015�ɕύX
    - �ʏ�ł̎��s�ɂ́uVisual Studio 2015 �� Visual C++ �ĔЕz�\�p�b�P�[�W�v�̃C���X�g�[�����K�v�ł�

## 2015-10-07

�i`BonDriver_BDA_with_PX-Q3W3S3`�Ƀo�C�i�������j

- �`�����l����Ԃ������ݒ�ł���悤�ɂȂ�܂���
    - `CH000`�`�̃`�����l����`��`TuningSpace00`�`�̃Z�N�V�������ł̒�`�ɂȂ�܂���
    - `TuningSpaceName`��`TuningSpace00`�`�̃Z�N�V�������ɐݒ肵�Ă�������
- `DebugLog`���t�@�C���ɋL�^����I�v�V������ǉ�
- �n�f�W/�q���`���[�i�[�p�̐ݒ��ǉ�
- ����̃v���Z�X���畡��`Create`����Ă����v��������Ȃ������ɂ��Ă݂�
- ���̊֌W�������������Ǝv����A�`���[�i�f�o�C�X�������Ɏg�p����������`Release`���Ȃ����[����p�~
- �`���[�i�ŗL�֐����g�����A�`���[�i�������E�f�R�[�h�E�I�ǎ��̒ǉ����������s����悤�ɂ���
- `LoadTuningSpace`�̏�����p�~���A`CreateTuningSpace`�ō쐬����`TuningSpace`���V�X�e���ɓo�^���Ȃ��悤�ɕύX
- ���̑��A�ׂ����C��

## 2015-10-09

- �\�[�X�̃R�����g�𐮗�
- �C�������E�T���v��ini�t�@�C�����X�V
- �ꕔ�R�[�h�̍œK��

## 2015-10-10

�s��C����

- `DefaultNetwork`��1(SPHD)/2(BS/CS110)���w�肵�����̉q���ݒ�f�t�H���g�l���Ԉ���Ă����_���C��
- �`���[�j���O�X�y�[�X����CH��`���������ĂȂ������ꍇ�ɗ�����s��C��
- `IDVBTuningSpace`�̑����`IDVBTuningSpace2`���g�p����悤�ɂ���
- CH��`�����݂��Ȃ������ꍇ�̓�����]���ʂ�s���悤�C��

## 2015-10-12

- `GetCurSpace()`�����-1��Ԃ��s��C��
- �`���[�i�f�o�C�X�ƃL���v�`���f�o�C�X�̃f�o�C�X�C���X�^���X�p�X���`�F�b�N���A�Ԉ�����g�ݍ��킹�ł�`Connect`���s��Ȃ��悤�ɂ���
    - ��肪��������ꍇ��ini�t�@�C����`[Tuner]`�Z�N�V������
    - `CheckDeviceInstancePath=0`
    - ��ǉ����邱�Ƃɂ��]���ʂ�̓���i�`�F�b�N���s��Ȃ��j�ɂȂ�܂�
- `Connect`���Ă��悢`Capture`���ǂ���`BDASpceials`���ɖ₢���킹��֐���ǉ�
    - `BDASpecial-PlexPX`��2015-10-12�ňȌ���g�p����ꍇ��`BonDriver_BDA`�̂��̃o�[�W�����Ȍ�Ƒg�ݍ��킹�Ďg�p���Ă�������

## 2015-11-23

- ini�t�@�C����`[TuningSpaceNN]`�Z�N�V������`ChannelLockTwiceTarget`��ǉ�
    - `ChannelLockTwice`�̑ΏۂƂȂ�CH�ԍ����w�肷�邱�Ƃ��o���܂��B
- `doc/Readme-BonDriver_BDA.txt`�ǉ�
- �T���v��ini�t�@�C���̌�L�C��
- `DLL_PROCESS_DETACH`���Ƀf�o�b�O���O�t�@�C���̃N���[�Y���s���悤�ɂ���

## 2015-12-23

- �J������Visual Studio Community 2015 Update 1�ɕύX
    - �ʏ�ł̎��s�ɂ́uVisual Studio 2015 Update 1 �� Visual C++ �ĔЕz�\�p�b�P�[�W�v�̃C���X�g�[�����K�v�ł�
    - �uVisual Studio 2015 �� Visual C++ �ĔЕz�\�p�b�P�[�W�v�����ɃC���X�g�[�����Ă���ꍇ�ł��ʓr�C���X�g�[�����K�v�ł��̂Œ��ӂ��Ă�������
- ini�t�@�C����`SignalLevelCalcType`�i`SignalLevel`�Z�o���@�j�Ƀr�b�g���[�g�l���w��ł���悤�ɂ���
- `CBonTuner::GetSignalState`����̃R�[�h������
- ini�t�@�C����`[Tuner]`�Z�N�V�����ɉ��L3���ڂ�ǉ����A��M�ُ펞�Ɏ����ł̍ă`���[�j���O/�ăI�[�v�����s����悤�ɂ���
    - `WatchDogSignalLocked`
    - `WatchDogBitRate`
    - `ReOpenWhenGiveUpReLock`
- ini�t�@�C��`[Tuner]`�Z�N�V������`GUID`/`FriendlyName`/`CaptureGUID`/`CaptureFriendlyName`�̎d�l�ύX
    - �����w�肷��ꍇ�Ɏw��ł��鐔�l��0�`9��10����0�`99��100�ɕύX
    - ���L�̏����Ō�������悤�ɂ���
        1. `Guid0` / `FriendlyName0` / `CaptureGuid0` / `CaptureFriendlyName0` �̑g����
        1. `Guid1` / `FriendlyName1` / `CaptureGuid1` / `CaptureFriendlyName1` �̑g����
        1. `Guid2` / `FriendlyName2` / `CaptureGuid2` / `CaptureFriendlyName2` �̑g����
- �����v���Z�X���瓯��`BonDriver`���J���Ă���ꍇ�ł�`DebugLog`�I�v�V���������ꂼ��ŗL���ɂȂ�悤�ɂ���

## 2016-01-10

- �`���[�i�̃I�[�v���Ɏ��s�����ꍇ�ɕs���I�����Ă��܂����Ƃ�����s����C��
- �L���v�`���f�o�C�X���g�p���Ȃ��^�C�v�̃`���[�i�ɑΉ�����I�v�V������ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`NotExistCaptureDevice`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`ModulationTypeXBandWidth`��ݒ肵�Ă��Œ��6�ɂȂ��Ă����_���C��
- �q����M�p�����[�^�̎d�l�ύX
    - `HighOscillator` / `LowOscillator` / `LNBSwitch` �̑g�����ł̎w����\�ɂ���
    - �@��ŗLAPI�����ł�`DiSEqC`�̎w����\�ɂ���
- `SignalLevel`�Z�o���@�ɁA`ITuner::get_SignalStrength`���g�p����ݒ��ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`SignalLevelCalcType`�̍��ڂ��Q�Ƃ��Ă������� 
- `SignalLocked`�̔�����@�̐ݒ��ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`SignalLockedJudgeType`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`DVBSystemType`�Ŏw��ł����ނ�`DVB-C`/`ISDB-S`/`ISDB-T`/`ATSC`/`Digital-Cable`����ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`DVBSystemType`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C���ł�`Network Provider`�w���ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`NetworkProvider`�̍��ڂ��Q�Ƃ��Ă�������
- `ATSC` / `Digital-Cable`�p��Ch��`�ݒ��ǉ�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��CH�ݒ�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`BuffSize`��0��ݒ肵���ꍇ�ɁA�㗬���������X�g���[���f�[�^�T�C�Y���̂܂܂ň����悤�ɂ���

## 2016-12-30

- �J������Visual Studio Community 2015 Update 3�ɕύX
- ini�t�@�C����`StrengthBias`�^`QualityBias`���w��\�ɂ���
    - `SignalLevel`�Z�o���Ɉ��̒l�������Z���܂�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`SignalLevelCalcType`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`AlwaysAnswerLocked`���w��ł���悤�ɂ���
    - `TVTest`�Ń`�����l���X�L�������s�������̂��m�点�_�C�A���O��}���ł��܂�
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`AlwaysAnswerLocked`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`BackgroundChannelLock`���w��ł���悤�ɂ���
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`BackgroundChannelLock`�̍��ڂ��Q�Ƃ��Ă�������
- ini�t�@�C����`TryAnotherTuner`���w��ł���悤�ɂ���
    - �ڍׂ�`BonDriver_BDA.sample.ini`��`TryAnotherTuner`�̍��ڂ��Q�Ƃ��Ă�������

## 2017-01-22

- ini �t�@�C���� `SignalLevelCalcType` �� 3��13��ǉ�
- `ITuner::get_SignalStrength`��`Strength` �l���������擾�ł��Ȃ������C��
- `IBDA_SignalStatistics::get_SignalStrength` �Ŏ擾����l�� 16 bit mask ����悤�ɂ���

## 2017-02-06

- �ꕔ�̊��ɂ����āA`BonDriver`�̉�����ɃN���b�V�����Ă��܂������C��

## 2018-07-16

- �v���b�g�t�H�[���c�[���Z�b�g��Visual Studio 2017 - Windows XP�ɕύX
- `CoInitialize` �����X���b�h�Ń��b�Z�[�W�̃f�B�X�p�b�`�������s���悤�ɂ���
- ini�t�@�C�������CH�ݒ�Ǎ����x�����P
- TS�o�b�t�@�r�������̕s����C��
- �X�J�p�[!SD��`ModulationType0Modulation`�̃f�t�H���g�l��`20`(BDA_MOD_QPSK)�ɏC��
- ini�t�@�C���̉q���ݒ���ő�9�܂Őݒ�ł���悤�ɕύX
- ini�t�@�C���̕ϒ������ݒ���ő�10�܂Őݒ�ł���悤�ɕύX
- `UHF`/`CATV`�̎��g���𐳊m�Ȓ��S���g���ɏC��
- `UHF` 53ch�`62ch���폜
- `CATV` C24ch�`C27ch�̎��g����ύX
- `BonDriver_BDA-UHF-sample.ini`�̃`���[�j���O�X�y�[�X `"CATV"` ���R�����g�A�E�g
- `BonDriver_BDA-UHF-sample.ini`�̃R�����g���e�ύX
- `BonDriver_BDA-BSCS-sample.ini`��CH�ݒ���ŐV�̂��̂ɍX�V
- `BonDriver_BDA_SPHD-sample.ini`��`JCSAT3A-TP21`��L���ɂ���
- `BonDriver_BDA_SPHD-sample.ini`��`JCSAT4A`�̕\�L��`JCSAT4B`�ɏC��

## 2018-08-18

- �`���[�j���O��Ԃ��ƂɎ��g���I�t�Z�b�g���w��ł���悤�ɂ���
- `ThreadPriority`�֌W��ini�t�@�C���I�v�V������ǉ�
- ini�t�@�C���̋L�q���@�𐔒l�`�������ł͂Ȃ�������ł̎w����ł���悤�ɕύX
    - �]����ini�t�@�C�������̂܂܂ł��F���\�ł�
- `NotExistCaptureDevice=YES`�ȃ`���[�i�[���������݂������`BonDriver`�̏������Ɏ��s���邱�Ƃ�����s����C��
- ini�t�@�C����`DefaultNetwork="Dual"`�̐ݒ���\�ɂ���
- ini�t�@�C����`UseSpecial`�̃f�t�H���g��`"AUTO"`����`""`(�g�p���Ȃ�)�ɕύX
- �ꕔ�̋@���`ITuner::get_SignalStrength()`�̒l���s���ɂȂ�s����C��

## 2019-02-02

- Log��`"Format Error in readIniFile; Wrong Polarization."`���s���ɋL�^����Ă��܂��s����C��
- ini�t�@�C���Ŏw��ł��鍀�� `Satellite1SettingsAuto` �` `Satellite9SettingsAuto` ��ǉ�
- ini�t�@�C���Ŏw��ł��鍀�� `ModulationType0SettingsAuto` �` `ModulationType9SettingsAuto` ��ǉ�
- ini�t�@�C���Ŏw��ł��鍀�� `ChannelSettingsAuto` �̓��e�̕ύX�A�֘A�I�v�V�����̒ǉ�
- ini�t�@�C���Ŏw��ł��鍀�� `ReserveUnusedCh` ���`���[�j���O��Ԃ��Ƃɐݒ�ł���悤�ɂ���
- ini�t�@�C���Ŏw��ł��鍀�� `ToneSignalWait` ��0���ݒ肳��Ă��鎞�̓`���[�j���O�����2�x�s��Ȃ��悤�ɂ���
- ini�t�@�C���Ŏw��ł��鍀�� `DVBSystemType` �ɐݒ�ł���l��`"ISDB-C"`��ǉ�
- ini�t�@�C����CH��` `SID`/`TSID`/`ONID` ���� `0` ���ݒ肳��Ă��鎞�� `0` �Ƃ��ĔF������悤�ύX
- ini�t�@�C���Ŏw��ł��鍀�� `TSMFMode` ��ǉ�
- ini�t�@�C���Ŏw��ł��鍀�� `PeriodicTimer` ��ǉ�
    - **�� 20190202���O�̃o�[�W������`BDASpecial`�v���O�C���͓��삵�܂���̂ł����ӂ��������B**
    - (`Bon_SPHD_BDA_PATCH_2`������`BDASpecial`�v���O�C��������)

