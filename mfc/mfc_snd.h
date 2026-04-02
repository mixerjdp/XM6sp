//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC sound ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_snd_h)
#define mfc_snd_h

#include "opm.h"
#include "fileio.h"

//===========================================================================
//
//	Sound
//
//===========================================================================
class CSound : public CComponent
{
public:
	CSound(CFrmWnd *pWnd);
										// Constructor
	void FASTCALL Enable(BOOL bEnable);
										// Enable control
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration

	// External API
	void FASTCALL Process(BOOL bRun);
										// Process
	BOOL FASTCALL IsPlay() const		{ return m_bPlay; }
										// Get playback flag
	void FASTCALL SetVolume(int nVolume);
										// Volume setting
	int FASTCALL GetVolume();
										// Get volume
	void FASTCALL SetFMVol(int nVolume);
										// FM volume setting
	void FASTCALL SetADPCMVol(int nVolume);
										// ADPCM volume setting
	void FASTCALL SetYmfm(BOOL bEnable);
										// YMFM runtime toggle
	BOOL FASTCALL IsYmfm() const		{ return m_bYmfm; }
										// YMFM runtime status
	int FASTCALL GetMasterVol(int& nMaximum);
										// Get master volume
	void FASTCALL SetMasterVol(int nVolume);
										// Master volume setting
	BOOL FASTCALL StartSaveWav(LPCTSTR lpszWavFile);
										// WAV save start
	BOOL FASTCALL IsSaveWav() const;
										// WAV save status
	void FASTCALL EndSaveWav();
										// WAV save end

	// Device
	LPGUID m_lpGUID[16];
										// DirectSound device GUID
	CString m_DeviceDescr[16];
										// DirectSound device name
	int m_nDeviceNum;
										// Number of detected devices

private:
	// Initialization
	BOOL FASTCALL InitSub();
										// Initialization sub
	void FASTCALL CleanupSub();
										// Cleanup sub
	void FASTCALL Play();
										// Play start
	void FASTCALL Stop();
										// Play stop
	void FASTCALL EnumDevice();
										// Device enumeration
	static BOOL CALLBACK EnumCallback(LPGUID lpGuid, LPCSTR lpDescr, LPCSTR lpModule,
					void *lpContext);	// Device enum callback
	int m_nSelectDevice;
										// Selected device

	// WAV save
	void FASTCALL ProcessSaveWav(int *pStream, DWORD dwLength);
										// WAV save process
	Fileio m_WavFile;
										// WAV file
	WORD *m_pWav;
										// WAV data buffer (for flagging)
	UINT m_nWav;
										// WAV data buffer offset
	DWORD m_dwWav;
										// WAV header write size

	// Playback
	UINT m_uRate;
										// Sampling rate
	UINT m_uTick;
										// Buffer size (ms)
	UINT m_uPoll;
										// Polling interval (ms)
	UINT m_uCount;
										// Polling count
	UINT m_uBufSize;
										// Buffer size (bytes)
	BOOL m_bPlay;
										// Playback flag
	DWORD m_dwWrite;
										// Write position
	int m_nMaster;
										// Master volume
	int m_nFMVol;
										// FM volume (0~100)
	int m_nADPCMVol;
										// ADPCM volume (0~100)
	BOOL m_bYmfm;
										// YMFM runtime flag
	BOOL m_bYmfmActive;
										// Active YMFM engine flag
	UINT m_uConfiguredRate;
										// Configured sampling rate
	LPDIRECTSOUND m_lpDS;
										// DirectSound
	LPDIRECTSOUNDBUFFER m_lpDSp;
										// DirectSoundBuffer (primary)
	LPDIRECTSOUNDBUFFER m_lpDSb;
										// DirectSoundBuffer (secondary)
	DWORD *m_lpBuf;
										// Sound buffer
	static const UINT RateTable[];
										// Sampling rate table

	// Object
	OPMIF *m_pOPMIF;
										// OPM interface
	ADPCM *m_pADPCM;
										// ADPCM
	SCSI *m_pSCSI;
										// SCSI
	FM::OPM *m_pOPM;
										// OPM device
	Scheduler *m_pScheduler;
										// Scheduler
};

#endif	// mfc_snd_h
#endif	// _WIN32
