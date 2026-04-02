//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC sound ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "opmif.h"
#include "opm.h"
#if defined(XM6CORE_ENABLE_YMFM)
#include "ymfm_opm_engine.h"
#endif
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
//	Sound
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSound::CSound(CFrmWnd *pWnd) : CComponent(pWnd)
{
	m_dwID = MAKEID('S', 'N', 'D', ' ');
	m_strDesc = _T("Sound Renderer");

	// Initialize (settings)
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
	m_bYmfm = FALSE;
	m_bYmfmActive = FALSE;
	m_uConfiguredRate = 0;

	// Initialize (DirectSound and objects)
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

	// Initialize (WAV save)
	m_pWav = NULL;
	m_nWav = 0;
	m_dwWav = 0;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::Init()
{
	// Base class initialization
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Get scheduler
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);

	// Get OPMIF
	m_pOPMIF = (OPMIF*)::GetVM()->SearchDevice(MAKEID('O', 'P', 'M', ' '));
	ASSERT(m_pOPMIF);

	// Get ADPCM
	m_pADPCM = (ADPCM*)::GetVM()->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(m_pADPCM);

	// Get SCSI
	m_pSCSI = (SCSI*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'S', 'I'));
	ASSERT(m_pSCSI);

	// Enumerate devices
	EnumDevice();

	// Nothing to do here (ApplyCfg will be called later)
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Initialization sub
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::InitSub()
{
	PCMWAVEFORMAT pcmwf;
	DSBUFFERDESC dsbd;
	WAVEFORMATEX wfex;

	// If rate==0, do nothing
	if (m_uRate == 0) {
		return TRUE;
	}

	ASSERT(!m_lpDS);
	ASSERT(!m_lpDSp);
	ASSERT(!m_lpDSb);
	ASSERT(!m_lpBuf);
	ASSERT(!m_pOPM);

	// If no devices or selected device beyond range, use device 0
	if (m_nDeviceNum <= m_nSelectDevice) {
		if (m_nDeviceNum == 0) {
			return TRUE;
		}
		m_nSelectDevice = 0;
	}

	// DirectSound object creation
	if (FAILED(DirectSoundCreate(m_lpGUID[m_nSelectDevice], &m_lpDS, NULL))) {
		return TRUE;
	}

	// Set cooperative level
	if (FAILED(m_lpDS->SetCooperativeLevel(m_pFrmWnd->m_hWnd, DSSCL_PRIORITY))) {
		return FALSE;
	}

	// Create primary buffer
	memset(&dsbd, 0, sizeof(dsbd));
	dsbd.dwSize = sizeof(dsbd);
	dsbd.dwFlags = DSBCAPS_PRIMARYBUFFER;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSp, NULL))) {
		return FALSE;
	}

	// Set primary buffer format
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

	// Create secondary buffer
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
	dsbd.dwBufferBytes = ((dsbd.dwBufferBytes + 7) >> 3) << 3;	// 8-byte aligned
	m_uBufSize = dsbd.dwBufferBytes;
	dsbd.lpwfxFormat = (LPWAVEFORMATEX)&pcmwf;
	if (FAILED(m_lpDS->CreateSoundBuffer(&dsbd, &m_lpDSb, NULL))) {
		return FALSE;
	}

	// Allocate sound buffer (secondary buffer and primary buffer share 1 extra DWORD)
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

	// OPM device (synthesizer) initialization
#if defined(XM6CORE_ENABLE_YMFM)
	m_pOPM = m_bYmfm ? CreateYmfmOpmEngine(FALSE) : new FM::OPM;
#else
	m_pOPM = new FM::OPM;
#endif
	if (!m_pOPM) {
		CleanupSub();
		return FALSE;
	}
	if (!m_pOPM->Init(4000000, m_uRate, true)) {
		delete m_pOPM;
		m_pOPM = NULL;
		CleanupSub();
		return FALSE;
	}
	m_pOPM->Reset();
	m_pOPM->SetVolume(m_nFMVol);
#if defined(XM6CORE_ENABLE_YMFM)
	m_bYmfmActive = m_bYmfm;
#else
	m_bYmfmActive = FALSE;
#endif

	// Register with OPMIF
	m_pOPMIF->InitBuf(m_uRate);
	m_pOPMIF->SetEngine(m_pOPM);

	// Start playback if already enabled
	if (m_bEnable) {
		Play();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Cleanup()
{
	// Cleanup sub
	CleanupSub();

	// Base class cleanup
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Cleanup sub
//
//---------------------------------------------------------------------------
void FASTCALL CSound::CleanupSub()
{
	// Stop playback
	Stop();

	// Unregister from OPMIF
	if (m_pOPMIF) {
		m_pOPMIF->SetEngine(NULL);
	}

	// Delete OPM
	if (m_pOPM) {
		delete m_pOPM;
		m_pOPM = NULL;
	}
	m_bYmfmActive = FALSE;

	// Free sound buffer
	if (m_lpBuf) {
		delete[] m_lpBuf;
		m_lpBuf = NULL;
	}

	// Release DirectSoundBuffer
	if (m_lpDSb) {
		m_lpDSb->Release();
		m_lpDSb = NULL;
	}
	if (m_lpDSp) {
		m_lpDSp->Release();
		m_lpDSp = NULL;
	}

	// Release DirectSound
	if (m_lpDS) {
		m_lpDS->Release();
		m_lpDS = NULL;
	}

	// Reset rate
	m_uRate = 0;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CSound::ApplyCfg(const Config *pConfig)
{
	BOOL bFlag;
	UINT uConfiguredRate;
	UINT uRate;

	ASSERT(this);
	ASSERT(pConfig);

	uConfiguredRate = RateTable[pConfig->sample_rate];
	uRate = uConfiguredRate;
#if defined(XM6CORE_ENABLE_YMFM)
	if (m_bYmfm) {
		uRate = 62500;
	}
#endif

	// Check for changes
	bFlag = FALSE;
	if (m_nSelectDevice != pConfig->sound_device) {
		bFlag = TRUE;
	}
	if (m_uRate != uRate) {
		bFlag = TRUE;
	}
	if (m_uTick != (UINT)(pConfig->primary_buffer * 10)) {
		bFlag = TRUE;
	}
#if defined(XM6CORE_ENABLE_YMFM)
	if (m_bYmfmActive != m_bYmfm) {
		bFlag = TRUE;
	}
#endif

	// Apply changes
	if (bFlag) {
		CleanupSub();
		m_nSelectDevice = pConfig->sound_device;
		m_uConfiguredRate = uConfiguredRate;
		m_uRate = uRate;
		m_uTick = pConfig->primary_buffer * 10;

		// For 62.5kHz, temporarily switch to 96kHz (Prodigy7.1 workaround)
#if defined(XM6CORE_ENABLE_YMFM)
		if (m_uRate == 62500) {
			// First try at 96kHz
			m_uRate = 96000;
			InitSub();

			// If buffer was successfully created, briefly start then stop
			if (m_lpDSb) {
				if (!m_bEnable) {
					m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
				}

				::Sleep(20);

				if (!m_bEnable) {
					m_lpDSb->Stop();
				}
			}

			// Then return to 62.5kHz
			CleanupSub();
			m_uRate = 62500;
		}
#endif
		InitSub();
	}
	else {
		m_uConfiguredRate = uConfiguredRate;
	}

	// Apply volume settings
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
//	Sampling rate table
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
//	Enable control
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Enable(BOOL bEnable)
{
	if (bEnable) {
		// Enable component: start playback
		if (!m_bEnable) {
			m_bEnable = TRUE;
			Play();
		}
	}
	else {
		// Disable component: stop playback
		if (m_bEnable) {
			m_bEnable = FALSE;
			Stop();
		}
	}
}

//---------------------------------------------------------------------------
//
//	Start playback
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Play()
{
	ASSERT(m_bEnable);

	// If already playing, do nothing
	if (m_bPlay) {
		return;
	}

	// Start playback if component is enabled
	if (m_pOPM) {
		m_lpDSb->Play(0, 0, DSBPLAY_LOOPING);
		m_bPlay = TRUE;
		m_uCount = 0;
		m_dwWrite = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Stop playback
//
//---------------------------------------------------------------------------
void FASTCALL CSound::Stop()
{
	// If not playing, do nothing
	if (!m_bPlay) {
		return;
	}

	// End WAV save if active
	if (m_pWav) {
		EndSaveWav();
	}

	// Stop playback if component is enabled
	if (m_pOPM) {
		m_lpDSb->Stop();
		m_bPlay = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Process
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

	// Wait for poll (m_nPoll times before first call, but VM stop detection continues)
	m_uCount++;
	if ((m_uCount < m_uPoll) && bRun) {
		return;
	}
	m_uCount = 0;

	// If disabled, do nothing
	if (!m_bEnable) {
		return;
	}

	// If not ready, do nothing
	if (!m_pOPM) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// If not playing, do nothing
	if (!m_bPlay) {
		m_pScheduler->SetSoundTime(0);
		return;
	}

	// Get current playback position (in bytes)
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);
	if (FAILED(m_lpDSb->GetCurrentPosition(&dwOffset, &dwWrite))) {
		return;
	}
	ASSERT(m_lpDSb);
	ASSERT(m_lpBuf);

	// Calculate bytes from last write to current position, considering wraparound
	if (m_dwWrite <= dwOffset) {
		dwRequest = dwOffset - m_dwWrite;
	}
	else {
		dwRequest = m_uBufSize - m_dwWrite;
		dwRequest += dwOffset;
	}

	// If available bytes less than 1/4 of buffer, do nothing
	if (dwRequest < (m_uBufSize / 4)) {
		return;
	}

	// Round to sample count (L,R pairs of 1 DWORD each)
	ASSERT((dwRequest & 3) == 0);
	dwRequest /= 4;

	// Fill buffer with audio data, or check bRun flag
	if (!bRun) {
		memset(m_lpBuf, 0, m_uBufSize * 2);
		m_pOPMIF->InitBuf(m_uRate);
	}
	else {
		// Process OPM and get ready sample count
		dwReady = m_pOPMIF->ProcessBuf();
		m_pOPMIF->GetBuf(m_lpBuf, (int)dwRequest);
		if (dwReady < dwRequest) {
			dwRequest = dwReady;
		}

		// Process ADPCM data (wait for sync)
		m_pADPCM->GetBuf(m_lpBuf, (int)dwRequest);

		// ADPCM sync processing
		if (dwReady > dwRequest) {
			m_pADPCM->Wait(dwReady - dwRequest);
		}
		else {
			m_pADPCM->Wait(0);
		}

		// Process SCSI data (wait for sync)
		m_pSCSI->GetBuf(m_lpBuf, (int)dwRequest, m_uRate);
	}

	// Lock
	hr = m_lpDSb->Lock(m_dwWrite, (dwRequest * 4),
						(void**)&pBuf1, &dwSize1,
						(void**)&pBuf2, &dwSize2,
						0);
	// If buffer was lost, restore
	if (hr == DSERR_BUFFERLOST) {
		m_lpDSb->Restore();
	}
	// If lock still failed, treat as meaning-less
	if (FAILED(hr)) {
		m_dwWrite = dwOffset;
		return;
	}

	// Rounds to 16-bit = 2 bytes
	ASSERT((dwSize1 & 1) == 0);
	ASSERT((dwSize2 & 1) == 0);

	// MMX conversion routine (dwSize1+dwSize2 bytes, typical 5000~15000 byte range with high frequency)
	SoundMMXPortable(m_lpBuf, pBuf1, dwSize1);
	if (dwSize2 > 0) {
		SoundMMXPortable(&m_lpBuf[dwSize1 / 2], pBuf2, dwSize2);
	}
	SoundEMMSPortable();

	// Unlock
	m_lpDSb->Unlock(pBuf1, dwSize1, pBuf2, dwSize2);

	// Update m_dwWrite
	m_dwWrite += dwSize1;
	m_dwWrite += dwSize2;
	if (m_dwWrite >= m_uBufSize) {
		m_dwWrite -= m_uBufSize;
	}
	ASSERT(m_dwWrite < m_uBufSize);

	// Update WAV if active
	if (bRun && m_pWav) {
		ProcessSaveWav((int*)m_lpBuf, (dwSize1 + dwSize2));
	}
}

//---------------------------------------------------------------------------
//
//	Volume setting
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetVolume(int nVolume)
{
	LONG lVolume;

	ASSERT(this);
	ASSERT((nVolume >= 0) && (nVolume <= 100));

	// If not ready, do nothing
	if (!m_pOPM) {
		return;
	}

	// Convert volume
	lVolume = 100 - nVolume;
	lVolume *= (DSBVOLUME_MAX - DSBVOLUME_MIN);
	lVolume /= -200;

	// Set
	m_lpDSb->SetVolume(lVolume);
}

//---------------------------------------------------------------------------
//
//	Get volume
//
//---------------------------------------------------------------------------
int FASTCALL CSound::GetVolume()
{
	LONG lVolume;

	ASSERT(this);

	// If not ready, return saved value
	if (!m_pOPM) {
		return m_nMaster;
	}

	// Get volume
	ASSERT(m_lpDSb);
	if (FAILED(m_lpDSb->GetVolume(&lVolume))) {
		return 0;
	}

	// Convert volume
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
//	FM volume setting
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetFMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// Store
	m_nFMVol = nVol;

	// Set
	if (m_pOPM) {
		m_pOPM->SetVolume(m_nFMVol);
	}
}

//---------------------------------------------------------------------------
//
//	ADPCM volume setting
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetADPCMVol(int nVol)
{
	ASSERT(this);
	ASSERT((nVol >= 0) && (nVol <= 100));

	// Store
	m_nADPCMVol = nVol;

	// Set
	ASSERT(m_pADPCM);
	m_pADPCM->SetVolume(m_nADPCMVol);
}

//---------------------------------------------------------------------------
//
// YMFM runtime toggle
//
//---------------------------------------------------------------------------
void FASTCALL CSound::SetYmfm(BOOL bEnable)
{
	ASSERT(this);

#if defined(XM6CORE_ENABLE_YMFM)
	m_bYmfm = bEnable ? TRUE : FALSE;
#else
	(void)bEnable;
	m_bYmfm = FALSE;
	m_bYmfmActive = FALSE;
#endif
}

//---------------------------------------------------------------------------
//
//	Device enumeration
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EnumDevice()
{
	// Clear
	m_nDeviceNum = 0;

	// Start enumeration
	DirectSoundEnumerate(EnumCallback, this);
}

//---------------------------------------------------------------------------
//
//	Device enum callback
//
//---------------------------------------------------------------------------
BOOL CALLBACK CSound::EnumCallback(LPGUID lpGuid, LPCTSTR lpDescr, LPCTSTR /*lpModule*/, LPVOID lpContext)
{
	CSound *pSound;
	int index;

	// Receive this pointer
	pSound = (CSound*)lpContext;
	ASSERT(pSound);

	// If device count less than 16, save
	if (pSound->m_nDeviceNum < 16) {
		index = pSound->m_nDeviceNum;

		// Save
		pSound->m_lpGUID[index] = lpGuid;
		pSound->m_DeviceDescr[index] = lpDescr;
		pSound->m_nDeviceNum++;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get master volume
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

	// Only works when selected device is 0 and rate is set
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return -1;
	}

	// Open mixer device
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		// Mixer open error
		return -1;
	}

	// Get destination line
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		// Line not found
		::mixerClose(hMixer);
		return -1;
	}

	// Get control
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
		// Line not found
		::mixerClose(hMixer);
		return -1;
	}

	// Calculate number of controls and allocate
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// Get control value
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
		// Line not found
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// If minimum value is not 0, bail out
	if (mixCtrl.Bounds.lMinimum != 0) {
		// Line not found
		delete[] pData;
		::mixerClose(hMixer);
		return -1;
	}

	// Store value
	nValue = pData[0].dwValue;
	nMaximum = mixCtrl.Bounds.lMaximum;

	// Cleanup
	delete[] pData;
	::mixerClose(hMixer);

	return nValue;
}

//---------------------------------------------------------------------------
//
//	Master volume setting
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

	// Only works when selected device is 0 and rate is set
	if ((m_nSelectDevice != 0) || (m_uRate == 0)) {
		return;
	}

	// Open mixer device
	mmResult = ::mixerOpen(&hMixer, 0, 0, 0,
					MIXER_OBJECTF_MIXER);
	if (mmResult != MMSYSERR_NOERROR) {
		return;
	}

	// Get destination line
	memset(&mixLine, 0, sizeof(mixLine));
	mixLine.cbStruct = sizeof(mixLine);
	mixLine.dwComponentType = MIXERLINE_COMPONENTTYPE_DST_SPEAKERS;
	mmResult = ::mixerGetLineInfo((HMIXEROBJ)hMixer, &mixLine,
					MIXER_OBJECTF_HMIXER | MIXER_GETLINEINFOF_COMPONENTTYPE);
	if (mmResult != MMSYSERR_NOERROR) {
		::mixerClose(hMixer);
		return;
	}

	// Get control
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
		::mixerClose(hMixer);
		return;
	}

	// Calculate number of controls and allocate
	nNum = 1;
	if (mixLine.cChannels > 0) {
		nNum *= mixLine.cChannels;
	}
	if (mixCtrl.cMultipleItems > 0) {
		nNum *= mixCtrl.cMultipleItems;
	}
	pData = new MIXERCONTROLDETAILS_UNSIGNED[nNum];

	// Get control value
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
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// Verify range
	ASSERT(mixCtrl.Bounds.lMinimum <= nVolume);
	ASSERT(nVolume <= mixCtrl.Bounds.lMaximum);
	for (nIndex=0; nIndex<nNum; nIndex++) {
		pData[nIndex].dwValue = (DWORD)nVolume;
	}

	// Set control value
	mmResult = mixerSetControlDetails((HMIXEROBJ)hMixer, &mixDetail,
					MIXER_OBJECTF_HMIXER | MIXER_GETCONTROLDETAILSF_VALUE);
	if (mmResult != MMSYSERR_NOERROR) {
		delete[] pData;
		::mixerClose(hMixer);
		return;
	}

	// Cleanup
	::mixerClose(hMixer);
	delete[] pData;
}

//---------------------------------------------------------------------------
//
//	WAV save start
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::StartSaveWav(LPCTSTR lpszWavFile)
{
	DWORD dwSize;
	WAVEFORMATEX wfex;

	ASSERT(this);
	ASSERT(lpszWavFile);

	// If already saved, error
	if (m_pWav) {
		return FALSE;
	}
	// If not playing, error
	if (!m_bEnable || !m_bPlay) {
		return FALSE;
	}

	// Open file
	if (!m_WavFile.Open(lpszWavFile, Fileio::WriteOnly)) {
		return FALSE;
	}

	// Write RIFF header placeholder
	if (!m_WavFile.Write((BYTE*)"RIFF0123WAVEfmt ", 16)) {
		m_WavFile.Close();
		return FALSE;
	}

	// Write WAVEFORMATEX size
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

	// Write data subchunk header
	if (!m_WavFile.Write((BYTE*)"data0123", 8)) {
		m_WavFile.Close();
		return FALSE;
	}

	// Allocate temp buffer
	try {
		m_pWav = new WORD[0x20000];
	}
	catch (...) {
		return FALSE;
	}
	if (!m_pWav) {
		return FALSE;
	}

	// Clear buffers
	m_nWav = 0;
	m_dwWav = 0;

	// Clear started flags for OPMIF and ADPCM
	m_pOPMIF->ClrStarted();
	m_pADPCM->ClrStarted();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WAV save status
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSound::IsSaveWav() const
{
	ASSERT(this);

	// Check buffer
	if (m_pWav) {
		return TRUE;
	}

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	WAV save process
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

	// Check Started flags and skip if both not started
	if (!m_pOPMIF->IsStarted() && !m_pADPCM->IsStarted()) {
		return;
	}

	// Check if need to flush previous data
	dwPrev = 0;
	if ((dwLength + m_nWav) >= 256 * 1024) {
		dwPrev = 256 * 1024 - m_nWav;
		dwLength -= dwPrev;
	}

	// First
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

	// Then
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
//	WAV save end
//
//---------------------------------------------------------------------------
void FASTCALL CSound::EndSaveWav()
{
	DWORD dwLength;

	// Write remaining data
	if (m_nWav > 0) {
		m_WavFile.Write(m_pWav, m_nWav);
		m_nWav = 0;
	}

	// Update header
	m_WavFile.Seek(4);
	dwLength = m_dwWav + sizeof(WAVEFORMATEX) + 20;
	m_WavFile.Write((BYTE*)&dwLength, sizeof(dwLength));
	m_WavFile.Seek(sizeof(WAVEFORMATEX) + 24);
	m_WavFile.Write((BYTE*)&m_dwWav, sizeof(m_dwWav));

	// Close file
	m_WavFile.Close();

	// Free buffer
	delete[] m_pWav;
	m_pWav = NULL;
}

#endif	// _WIN32
