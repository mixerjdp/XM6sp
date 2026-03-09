//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ MFC �T�E���h ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "mfc.h"
#include "xm6.h"
#include "vm.h"
#include "opmif.h"
#include "opm.h"
#include "adpcm.h"
#include "scsi.h"
#include "schedule.h"
#include "config.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_asm.h"
#include "mfc_snd.h"

//===========================================================================
//
//	�T�E���h
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
CSound::CSound(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// �R���|�[�l���g�p�����[�^
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	m_strDesc = _T("Sound Renderer");

	// ���[�N������(�ݒ�p�����[�^)
	m_uRate = 0;
	m_uTick = 90;
	m_uPoll = 7;
	m_uBufSize = 0;
	m_bPlay = FALSE;
	m_uCount = 0;
	m_dwWrite = 0;
	m_nMaster = 90;
	m_nFMVol = 54;
	m_nADPCMVol = 52;

	// ���[�N������(DirectSound�ƃI�u�W�F�N�g)
	m_lpDS = NULL;
	m_lpDSp = NULL;
	m_lpDSb = NULL;
	m_lpBuf = NULL;
	m_pScheduler = NULL;
	m_pOPM = NULL;
	m_pOPMIF = NULL;
	m_pADPCM = NULL;
	m_pSCSI = NULL;
	m_nDeviceNum = 0;
	m_nSelectDevice = 0;

	// ���[�N������(WAV�^��)
	m_pWav = NULL;
	m_nWav = 0;
	m_dwWav = 0;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::Init()
{
	// ��{�N���X
	if (!CComponent::Init()) {
		return FALSE;
	}

	// �X�P�W���[���擾
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);

	// OPMIF�擾
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// ADPCM�擾
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// SCSI�擾
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// �f�o�C�X��
	EnumDevice();

	// �����ł͏��������Ȃ�(ApplyCfg�ɔC����)
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�������T�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::InitSub()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX wfex;

	// rate==0�Ȃ�A�������Ȃ�
	if (m_uRate == 0) {
		return TRUE;
	}

	ASSERT(!m_lpDS);
	ASSERT(!m_lpDSp);
	ASSERT(!m_lpDSb);
	ASSERT(!m_lpBuf);
	ASSERT(!m_pOPM);

	// �f�o�C�X���Ȃ����0�Ŏ����A����ł��Ȃ����return
	if (m_nDeviceNum <= m_nSelectDevice) {
		if (m_nDeviceNum == 0) {
			return TRUE;
		}
		m_nSelectDevice = 0;
	}

	// DiectSound�I�u�W�F�N�g�쐬
	if (FAILED(DirectSoundCreate(m_lpGUID[m_nSelectDevice], &m_lpDS, NULL))) {
		// �f�o�C�X�͎g�p��
		return TRUE;
	}

	// �������x����ݒ�(�D�拦��)
	if (FAILED(m_lpDS->SetCooperativeLevel(m_pFrmWnd->m_hWnd, DSSCL_PRIORITY))) {
		return FALSE;
	}

	// �v���C�}���o�b�t�@���쐬
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSp, NULL))) {
		return FALSE;
	}

	// �v���C�}���o�b�t�@�̃t�H�[�}�b�g���w��
	memset(&wfex, 0, sizeof(wfex));
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.nSamplesPerSec = m_uRate;
	wfex.nBlockAlign = 4;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
	wfex.wBitsPerSample = 16;
	if (FAILED(m_lpDSp->SetFormat(&wfex))) {
		return FALSE;
	}

	// �Z�J���_���o�b�t�@���쐬
	memset(&pcmwf, 0, sizeof(pcmwf));
	pcmwf.wf.wFormatTag = WAVE_FORMAT_PCM;
	pcmwf.wf.nChannels = 2;
	pcmwf.wf.nSamplesPerSec = m_uRate;
	pcmwf.wf.nBlockAlign = 4;
	pcmwf.wf.nAvgBytesPerSec = pcmwf.wf.nSamplesPerSec * pcmwf.wf.nBlockAlign;
	pcmwf.wBitsPerSample = 16;
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_STICKYFOCUS | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_CTRLVOLUME;
	dsbd.dwBufferBytes = (pcmwf.wf.nAvgBytesPerSec * m_uTick) / 1000;
	dsbd.dwBufferBytes = ((dsbd.dwBufferBytes + 7) >> 3) << 3;	// 8�o�C�g���E
	m_uBufSize = dsbd.dwBufferBytes;
	dsbd.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSb, NULL))) {
		return FALSE;
	}

	// �T�E���h�o�b�t�@���쐬(�Z�J���_���o�b�t�@�Ɠ���̒����A1�P��DWORD)
	try {
		m_lpBuf = new DWORD [ m_uBufSize / 2 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!m_lpBuf) {
		return FALSE;
	}
	memset(m_lpBuf, sizeof(DWORD) * (m_uBufSize / 2), m_uBufSize);

	// OPM�f�o�C�X(�W��)���쐬
	m_pOPM = new FM::OPM;
	m_pOPM->Init(4000000, m_uRate, true);
	m_pOPM->Reset();
	m_pOPM->SetVolume(m_nFMVol);

	// OPMIF�֒ʒm
	m_pOPMIF->InitBuf(m_uRate);
	m_pOPMIF->SetEngine(m_pOPM);

	// �C�l�[�u���Ȃ牉�t�J�n
	if (m_bEnable) {
		Play();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Cleanup()
{
	// �N���[���A�b�v�T�u
	CleanupSub();

	// ��{�N���X
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v�T�u
//
//---------------------------------------------------------------------------
void FASTCALL CSound::CleanupSub()
{
	// �T�E���h��~
	Stop();

	// OPMIF�֒ʒm
	if (m_pOPMIF) {
		m_pOPMIF->SetEngine(NULL);
	}

	// OPM�����
	if (m_pOPM) {
		delete m_pOPM;
		m_pOPM = NULL;
	}

	// �T�E���h�쐬�o�b�t�@�����
	if (m_lpBuf) {
		delete[] m_lpBuf;
		m_lpBuf = NULL;
	}

	// DirectSoundBuffer�����
	if (m_lpDSb) {
		m_lpDSb->Release();
		m_lpDSb = NULL;
	}
	if (m_lpDSp) {
		m_lpDSp->Release();
		m_lpDSp = NULL;
	}

	// DirectSound�����
	if (m_lpDS) {
		m_lpDS->Release();
		m_lpDS = NULL;
	}

	// uRate���N���A
	m_uRate = 0;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL CSound::ApplyCfg(const Config *pConfig)
{
	BOOL bFlag;

	ASSERT(this);
	ASSERT(pConfig);

	// �ď������`�F�b�N
	bFlag = FALSE;
	if (m_nSelectDevice != pConfig->sound_device) {
		bFlag = TRUE;
	}
	if (m_uRate != RateTable[pConfig->sample_rate]) {
		bFlag = TRUE;
	}
	if (m_uTick != (UINT)(pConfig->primary_buffer * 10)) {
		bFlag = TRUE;
	}

	// �ď�����
	if (bFlag) {
		CleanupSub();
		m_nSelectDevice = pConfig->sound_device;
		m_uRate = RateTable[pConfig->sample_rate];
		m_uTick = pConfig->primary_buffer * 10;

		// 62.5kHz�̏ꍇ�́A��x96kHz�ɃZ�b�g���Ă���(Prodigy7.1�΍�)
		if (m_uRate == 62500) {
			// 96kHz�ŏ�����
			m_uRate = 96000;
			InitSub();

			// �������ł����ꍇ�́A�����������t����
			if (m_lpDSb) {
				// �X�^�[�g
				if (!m_bEnable) {
					m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
				}

				// ��������
				::Sleep(20);

				// �~�߂�
				if (!m_bEnable) {
					m_lpDSb->Stop();
				}
			}

			// 62.5kHz�ŏ�����
			CleanupSub();
			m_uRate = 62500;
		}
		InitSub();
	}

	// ��ɐݒ�
	if (m_pOPM) {
		SetVolume(pConfig->master_volume);
		m_pOPMIF->EnableFM(pConfig->fm_enable);
		SetFMVol(pConfig->fm_volume);
		m_pADPCM->EnableADPCM(pConfig->adpcm_enable);
		SetADPCMVol(pConfig->adpcm_volume);
	}
	m_nMaster = pConfig->master_volume;
	m_uPoll = (UINT)pConfig->polling_buffer;
}

//---------------------------------------------------------------------------
//
//	�T���v�����O���[�g�e�[�u��
//
//---------------------------------------------------------------------------
const UINT CSound::RateTable[] = {
	0,
	44100,
	48000,
	88200,
	96000,
	62500
};

//---------------------------------------------------------------------------
//
//	�L���t���O�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Enable(BOOL bEnable)
{
	if (bEnable) {
		// �������L�� ���t�J�n
		if (!m_bEnable) {
			m_bEnable = TRUE;
			Play();
		}
	}
	else {
		// �L�������� ���t��~
		if (m_bEnable) {
			m_bEnable = FALSE;
			Stop();
		}
	}
}

//---------------------------------------------------------------------------
//
//	���t�J�n
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Play()
{
	ASSERT(m_bEnable);

	// ���ɉ��t�J�n�Ȃ�K�v�Ȃ�
	if (m_bPlay) {
		return;
	}

	// �|�C���^���L���Ȃ牉�t�J�n
	if (m_pOPM) {
		m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
		m_bPlay = TRUE;
		m_uCount = 0;
		m_dwWrite = 0;
	}
}

//---------------------------------------------------------------------------
//
//	���t��~
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Stop()
{
	// ���ɉ��t��~�Ȃ�K�v�Ȃ�
	if (!m_bPlay) {
		return;
	}

	// WAV�Z�[�u�I��
	if (m_pWav) {
		EndSaveWav();
	}

	// �|�C���^���L���Ȃ牉�t��~
	if (m_pOPM) {
		m_lpDSb->Stop();
		m_bPlay = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	�i�s
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Process(BOOL bRun)
{
	HRESULT hr;
	DWORD dwOffset;
	DWORD dwWrite;
	DWORD dwRequest;
	DWORD dwReady;
	WORD *pBuf1;
	WORD *pBuf2;
	DWORD dwSize1;
	DWORD dwSize2;

	ASSERT(this);

	// �J�E���g����(m_nPoll��ɂP��A������VM��~���͏펞)
	m_uCount++;
	if ((m_uCount < m_uPoll) && bRun) {
		return;
	}
	m_uCount = 0;

	// �f�B�Z�[�u���Ȃ�A�������Ȃ�
	if (!m_bEnable) {
		return;
	}

	// ����������Ă��Ȃ���΁A�������Ȃ�
	if (!m_pOPM) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// �v���C��ԂłȂ���΁A�֌W�Ȃ�
	if (!m_bPlay) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// ���݂̃v���C�ʒu�𓾂�(�o�C�g�P��)
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);
	if (FAILED(m_lpDSb->GetCurrentPosition(&dwOffset, &dwWrite))) {
		return;
	}
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);

	// �O�񏑂����񂾈ʒu����A�󂫃T�C�Y���v�Z(�o�C�g�P��)
	if (m_dwWrite <= dwOffset) {
		dwRequest = dwOffset - m_dwWrite;
	}
	else {
		dwRequest = m_uBufSize - m_dwWrite;
		dwRequest += dwOffset;
	}

	// �󂫃T�C�Y���S�̂�1/4�𒴂��Ă��Ȃ���΁A���̋@���
	if (dwRequest < (m_uBufSize / 4)) {
		return;
	}

	// �󂫃T���v���Ɋ��Z(L,R��1�Ɛ�����)
	ASSERT((dwRequest & 3) == 0);
	dwRequest /= 4;

	// m_lpBuf�Ƀo�b�t�@�f�[�^���쐬�B�܂�bRun�`�F�b�N
	if (!bRun) {
		memset(m_lpBuf, 0, m_uBufSize * 2);
		m_pOPMIF->InitBuf(m_uRate);
	}
	else {
		// OPM�ɑ΂��āA�����v���Ƒ��x����
		dwReady = m_pOPMIF->ProcessBuf();
		m_pOPMIF->GetBuf(m_lpBuf, (int)dwRequest);
		if (dwReady < dwRequest) {
			dwRequest = dwReady;
		}

		// ADPCM�ɑ΂��āA�f�[�^��v��(���Z���邱��)
		m_pADPCM->GetBuf(m_lpBuf, (int)dwRequest);

		// ADPCM�̓�������
		if (dwReady > dwRequest) {
			m_pADPCM->Wait(dwReady - dwRequest);
		}
		else {
			m_pADPCM->Wait(0);
		}

		// SCSI�ɑ΂��āA�f�[�^��v��(���Z���邱��)
		m_pSCSI->GetBuf(m_lpBuf, (int)dwRequest, m_uRate);
	}

	// �����Ń��b�N
	hr = m_lpDSb->Lock(m_dwWrite, (dwRequest * 4),
						(void**)&pBuf1, &dwSize1,
						(void**)&pBuf2, &dwSize2,
						0);
	// �o�b�t�@�������Ă���΁A���X�g�A
	if (hr == DSERR_BUFFERLOST) {
		m_lpDSb->Restore();
	}
	// ���b�N�������Ȃ���΁A�����Ă��Ӗ����Ȃ�
	if (FAILED(hr)) {
		m_dwWrite = dwOffset;
		return;
	}

	// �ʎq��bit=16��O��Ƃ���
	ASSERT((dwSize1 & 1) == 0);
	ASSERT((dwSize2 & 1) == 0);

	// MMX���߂ɂ��p�b�N(dwSize1+dwSize2�ŁA����5000�`15000���x�͏�������)
	SoundMMXPortable(m_lpBuf, pBuf1, dwSize1);
	if (dwSize2 > 0) {
		SoundMMXPortable(&m_lpBuf[dwSize1 / 2], pBuf2, dwSize2);
	}
	SoundEMMSPortable();

	// �A�����b�N
	m_lpDSb->Unlock(pBuf1, dwSize1, pBuf2, dwSize2);

	// m_dwWrite�X�V
	m_dwWrite += dwSize1;
	m_dwWrite += dwSize2;
	if (m_dwWrite >= m_uBufSize) {
		m_dwWrite -= m_uBufSize;
	}
	ASSERT(m_dwWrite < m_uBufSize);

	// ���쒆�Ȃ�WAV�X�V
	if (bRun && m_pWav) {
		ProcessSaveWav((int*)m_lpBuf, (dwSize1 + dwSize2));
	}
}

//---------------------------------------------------------------------------
//
//	���ʃZ�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetVolume(int nVolume)
{
	LONG lVolume;

	ASSERT(this);
	ASSERT((nVolume >= 0) && (nVolume <= 100));

	// ����������Ă��Ȃ���ΐݒ肵�Ȃ�
	if (!m_pOPM) {
		return;
	}

	// �l��ϊ�
	lVolume = 100 - nVolume;
	lVolume *= (DSBVOLUME_MAX - DSBVOLUME_MIN);
	lVolume /= -200;

	// �ݒ�
	m_lpDSb->SetVolume(lVolume);
}

//---------------------------------------------------------------------------
//
//	���ʎ擾
//
//---------------------------------------------------------------------------
int FASTCALL CSound::GetVolume()
{
	LONG lVolume;

	ASSERT(this);

	// ����������Ă��Ȃ���΁A����̒l���󂯎��
	if (!m_pOPM) {
		return m_nMaster;
	}

	// �l���擾
	ASSERT(m_lpDSb);
	if (FAILED(m_lpDSb->GetVolume(&lVolume))) {
		return 0;
	}

	// �l��␳
	lVolume *= -200;
	lVolume /= (DSBVOLUME_MAX - DSBVOLUME_MIN);
	ASSERT((lVolume >= 0) && (lVolume <= 200));
	if (lVolume >= 100) {
		lVolume = 0;
	}
	else {
		lVolume = 100 - lVolume;
	}

	return (int)lVolume;
}

//---------------------------------------------------------------------------
//
//	FM�������ʃZ�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetFMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// �R�s�[
	m_nFMVol = nVol;

	// �ݒ�
	if (m_pOPM) {
		m_pOPM->SetVolume(m_nFMVol);
	}
}

//---------------------------------------------------------------------------
//
//	ADPCM�������ʃZ�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetADPCMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// �R�s�[
	m_nADPCMVol = nVol;

	// �ݒ�
	ASSERT(m_pADPCM);
	m_pADPCM->SetVolume(m_nADPCMVol);
}

//---------------------------------------------------------------------------
//
//	�f�o�C�X��
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EnumDevice()
{
	// ������
	m_nDeviceNum = 0;

	// �񋓊J�n
	DirectSoundEnumerate(EnumCallback, this);
}

//---------------------------------------------------------------------------
//
//	�f�o�C�X�񋓃R�[���o�b�N
//
//---------------------------------------------------------------------------
BOOL CALLBACK CSound::EnumCallback(LPGUID lpGuid, LPCTSTR lpDescr, LPCTSTR /*lpModule*/, LPVOID lpContext)
{
	CSound *pSound;
	int index;

	// this�|�C���^�󂯎��
	pSound = (CSound*)lpContext;
	ASSERT(pSound);

	// �J�����g��16�����Ȃ�L��
	if (pSound->m_nDeviceNum < 16) {
		index = pSound->m_nDeviceNum;

		// �o�^
		pSound->m_lpGUID[index] = lpGuid;
		pSound->m_DeviceDescr[index] = lpDescr;
		pSound->m_nDeviceNum++;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�}�X�^���ʎ擾
//
//---------------------------------------------------------------------------
int FASTCALL CSound::GetMasterVol(int& nMaximum)
{
	MMRESULT mmResult;
	HMIXER hMixer;
	MIXERLINE mixLine;
	MIXERLINECONTROLS mixLCs;
	MIXERCONTROL mixCtrl;
	MIXERCONTROLDETAILS mixDetail;
	MIXERCONTROLDETAILS_UNSIGNED *pData;
	int nNum;
	int nValue;

	ASSERT(this);

	// �g�p���Ă���f�o�C�X�ԍ���0�ł��邱�Ƃ��K�v
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return -1;
	}

	// �~�L�T���I�[�v��
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		// �~�L�T�I�[�v���G���[
		return -1;
	}

	// ���C�����𓾂�
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		::mixerClose(hMixer);
		return -1;
	}

	// �R���g���[�����𓾂�
	memset(&mixLCs, 0, sizeof(mixLCs));
	mixLCs.cbStruct = sizeof(mixLCs);
	mixLCs.dwLineID = mixLine.dwLineID;
	mixLCs.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mixLCs.cbmxctrl = sizeof(mixCtrl);
	mixLCs.pamxctrl = &mixCtrl;
	memset(&mixCtrl, 0, sizeof(mixCtrl));
	mixCtrl.cbStruct = sizeof(mixCtrl);
	mmResult = ::mixerGetLineControls((HMIXEROBJ)hMixer, &mixLCs,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		::mixerClose(hMixer);
		return -1;
	}

	// �R���g���[���̌������āA�������m��
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// �R���g���[���̒l�𓾂�
	memset(&mixDetail, 0, sizeof(mixDetail));
	mixDetail.cbStruct = sizeof(mixDetail);
	mixDetail.dwControlID = mixCtrl.dwControlID;
	mixDetail.cChannels = mixLine.cChannels;
	mixDetail.cMultipleItems = mixCtrl.cMultipleItems;
	mixDetail.cbDetails = nNum * sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mixDetail.paDetails = pData;
	mmResult = ::mixerGetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// �ŏ��l��0�̏ꍇ�̂�
	if (mixCtrl.Bounds.lMinimum != 0) {
		// �N���[�Y���ďI��
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// �l������
	nValue = pData[0].dwValue;
	nMaximum = mixCtrl.Bounds.lMaximum;

	// ����
	delete[] pData;
	::mixerClose(hMixer);

	return nValue;
}

//---------------------------------------------------------------------------
//
//	�}�X�^���ʃZ�b�g
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetMasterVol(int nVolume)
{
	MMRESULT mmResult;
	HMIXER hMixer;
	MIXERLINE mixLine;
	MIXERLINECONTROLS mixLCs;
	MIXERCONTROL mixCtrl;
	MIXERCONTROLDETAILS mixDetail;
	MIXERCONTROLDETAILS_UNSIGNED *pData;
	int nIndex;
	int nNum;

	ASSERT(this);

	// �g�p���Ă���f�o�C�X�ԍ���0�ł��邱�Ƃ��K�v
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return;
	}

	// �~�L�T���I�[�v��
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		// �~�L�T�I�[�v���G���[
		return;
	}

	// ���C�����𓾂�
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		::mixerClose(hMixer);
		return;
	}

	// �R���g���[�����𓾂�
	memset(&mixLCs, 0, sizeof(mixLCs));
	mixLCs.cbStruct = sizeof(mixLCs);
	mixLCs.dwLineID = mixLine.dwLineID;
	mixLCs.dwControlType = MIXERCONTROL_CONTROLTYPE_VOLUME;
	mixLCs.cbmxctrl = sizeof(mixCtrl);
	mixLCs.pamxctrl = &mixCtrl;
	memset(&mixCtrl, 0, sizeof(mixCtrl));
	mixCtrl.cbStruct = sizeof(mixCtrl);
	mmResult = ::mixerGetLineControls((HMIXEROBJ)hMixer, &mixLCs,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINECONTROLSF_ONEBYTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		::mixerClose(hMixer);
		return;
	}

	// �R���g���[���̌������āA�������m��
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// �R���g���[���̒l�𓾂�
	memset(&mixDetail, 0, sizeof(mixDetail));
	mixDetail.cbStruct = sizeof(mixDetail);
	mixDetail.dwControlID = mixCtrl.dwControlID;
	mixDetail.cChannels = mixLine.cChannels;
	mixDetail.cMultipleItems = mixCtrl.cMultipleItems;
	mixDetail.cbDetails = nNum * sizeof(MIXERCONTROLDETAILS_UNSIGNED);
	mixDetail.paDetails = pData;
	mmResult = ::mixerGetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		// �N���[�Y���ďI��
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// �l������
	ASSERT(mixCtrl.Bounds.lMinimum <= nVolume);
	ASSERT(nVolume <= mixCtrl.Bounds.lMaximum);
	for (nIndex=0; nIndex<nNum; nIndex++) {
		pData[nIndex].dwValue = (DWORD)nVolume;
	}

	// �R���g���[���̒l��ݒ�
	mmResult = mixerSetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// ����
	::mixerClose(hMixer);
	delete[] pData;
}

//---------------------------------------------------------------------------
//
//	WAV�Z�[�u�J�n
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::StartSaveWav(LPCTSTR lpszWavFile)
{
	DWORD dwSize;
	WAVEFORMATEX wfex;

	ASSERT(this);
	ASSERT(lpszWavFile);

	// ���ɘ^�����Ȃ�G���[
	if (m_pWav) {
		return FALSE;
	}
	// �Đ����łȂ���΃G���[
	if (!m_bEnable || !m_bPlay) {
		return FALSE;
	}

	// �t�@�C���쐬�����݂�
	if (!m_WavFile.Open(lpszWavFile, Fileio::WriteOnly)) {
		return FALSE;
	}

	// RIFF�w�b�_��������
	if (!m_WavFile.Write((BYTE*)"RIFF0123WAVEfmt ", 16)) {
		m_WavFile.Close();
		return FALSE;
	}

	// WAVEFORMATEX��������
	dwSize = sizeof(wfex);
	if (!m_WavFile.Write((BYTE*)&dwSize, sizeof(dwSize))) {
		m_WavFile.Close();
		return FALSE;
	}
	memset(&wfex, 0, sizeof(wfex));
	wfex.cbSize = sizeof(wfex);
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nChannels = 2;
	wfex.nSamplesPerSec = m_uRate;
	wfex.nBlockAlign = 4;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;
	wfex.wBitsPerSample = 16;
	if (!m_WavFile.Write((BYTE*)&wfex, sizeof(wfex))) {
		m_WavFile.Close();
		return FALSE;
	}

	// data�T�u�w�b�_��������
	if (!m_WavFile.Write((BYTE*)"data0123", 8)) {
		m_WavFile.Close();
		return FALSE;
	}

	// �^���o�b�t�@���m��
	try {
		m_pWav = new WORD[0x20000];
	}
	catch (...) {
		return FALSE;
	}
	if (!m_pWav) {
		return FALSE;
	}

	// ���[�N������
	m_nWav = 0;
	m_dwWav = 0;

	// �R���|�[�l���g�̃t���O���N���A
	m_pOPMIF->ClrStarted();
	m_pADPCM->ClrStarted();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WAV�Z�[�u����
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::IsSaveWav() const
{
	ASSERT(this);

	// �o�b�t�@�Ń`�F�b�N
	if (m_pWav) {
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	WAV�Z�[�u
//
//---------------------------------------------------------------------------
void FASTCALL CSound::ProcessSaveWav(int *pStream, DWORD dwLength)
{
	DWORD dwPrev;
	int i;
	int nLen;
	int rawData;

	ASSERT(this);
	ASSERT(pStream);
	ASSERT(dwLength > 0);
	ASSERT((dwLength & 3) == 0);

	// Started�t���O�𒲂ׁA�Ƃ��ɃN���A�Ȃ�X�L�b�v
	if (!m_pOPMIF->IsStarted() && !m_pADPCM->IsStarted()) {
		return;
	}

	// �O���E�㔼�ɕ�����K�v�����邩�`�F�b�N
	dwPrev = 0;
	if ((dwLength + m_nWav) >= 256 * 1024) {
		dwPrev = 256 * 1024 - m_nWav;
		dwLength -= dwPrev;
	}

	// �O��
	if (dwPrev > 0) {
		nLen = (int)(dwPrev >> 1);
		for (i=0; i<nLen; i++) {
			rawData = *pStream++;
			if (rawData > 0x7fff) {
				rawData = 0x7fff;
			}
			if (rawData < -0x8000) {
				rawData = -0x8000;
			}
			m_pWav[(m_nWav >> 1) + i] = (WORD)rawData;
		}

		m_WavFile.Write(m_pWav, 256 * 1024);
		m_nWav = 0;
	}

	// �㔼
	nLen = (int)(dwLength >> 1);
	for (i=0; i<nLen; i++) {
		rawData = *pStream++;
		if (rawData > 0x7fff) {
			rawData = 0x7fff;
		}
		if (rawData < -0x8000) {
			rawData = -0x8000;
		}
		m_pWav[(m_nWav >> 1) + i] = (WORD)rawData;
	}
	m_nWav += dwLength;
	m_dwWav += dwPrev;
	m_dwWav += dwLength;
}

//---------------------------------------------------------------------------
//
//	WAV�Z�[�u�I��
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EndSaveWav()
{
	DWORD dwLength;

	// �c�����f�[�^����������
	if (m_nWav > 0) {
		m_WavFile.Write(m_pWav, m_nWav);
		m_nWav = 0;
	}

	// �w�b�_�C��
	m_WavFile.Seek(4);
	dwLength = m_dwWav + sizeof(WAVEFORMATEX) + 20;
	m_WavFile.Write((BYTE*)&dwLength, sizeof(dwLength));
	m_WavFile.Seek(sizeof(WAVEFORMATEX) + 24);
	m_WavFile.Write((BYTE*)&m_dwWav, sizeof(m_dwWav));

	// �t�@�C���N���[�Y
	m_WavFile.Close();

	// ���������
	delete[] m_pWav;
	m_pWav = NULL;
}

#endif	// _WIN32
