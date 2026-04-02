//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC MIDI ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "midi.h"
#include "config.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_cfg.h"
#include "mfc_midi.h"

//===========================================================================
//
//	MIDI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	MIDI message definitions
//
//---------------------------------------------------------------------------
#define MIDI_NOTEOFF		0x80		// Note off
#define MIDI_NOTEON			0x90		// Note on (some note off)
#define MIDI_PKEYPRS		0xa0		// Polyphonic key pressure
#define MIDI_CTRLCHG		0xb0		// Control change
#define MIDI_PROGCHG		0xc0		// Program change
#define MIDI_CHPRS			0xd0		// Channel pressure
#define MIDI_PITCHCHG		0xe0		// Pitch bend change
#define MIDI_EXSTART		0xf0		// System exclusive start
#define MIDI_QFRAME			0xf1		// Quarter frame
#define MIDI_SONGPTR		0xf2		// Song position pointer
#define MIDI_SONGSEL		0xf3		// Song select
#define MIDI_TUNEREQ		0xf6		// Tune request
#define MIDI_EXEND			0xf7		// System exclusive end
#define MIDI_TIMINGCLK		0xf8		// Timing clock
#define MIDI_START			0xfa		// Start
#define MIDI_CONTINUE		0xfb		// Continue
#define MIDI_STOP			0xfc		// Stop
#define MIDI_ACTSENSE		0xfe		// Active sensing
#define MIDI_SYSRESET		0xff		// System reset

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CMIDI::CMIDI(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// Component identifier
	m_dwID = MAKEID('M', 'I', 'D', 'I');
	m_strDesc = _T("MIDI Driver");

	// Objects
	m_pMIDI = NULL;
	m_pScheduler = NULL;
	m_pSch = NULL;

	// Out
	m_dwOutDevice = 0;
	m_hOut = NULL;
	m_pOutThread = NULL;
	m_bOutThread = FALSE;
	m_dwOutDevs = 0;
	m_pOutCaps = NULL;
	m_dwOutDelay = (84 * 2000);
	m_nOutReset = 0;
	m_dwShortSend = 0;
	m_dwExSend = 0;
	m_dwUnSend = 0;

	// In
	m_dwInDevice = 0;
	m_hIn = NULL;
	m_pInThread = NULL;
	m_bInThread = FALSE;
	m_dwInDevs = 0;
	m_pInCaps = NULL;
	m_InState[0] = InNotUsed;
	m_InState[1] = InNotUsed;
	m_dwShortRecv = 0;
	m_dwExRecv = 0;
	m_dwUnRecv = 0;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::Init()
{
	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Get MIDI
	ASSERT(!m_pMIDI);
	m_pMIDI = (MIDI*)::GetVM()->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	ASSERT(m_pMIDI);

	// Get scheduler
	ASSERT(!m_pScheduler);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);

	// Get scheduler (Win32)
	ASSERT(!m_pSch);
	m_pSch = (CScheduler*)SearchComponent(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pSch);

	// Out device check
	m_dwOutDevs = ::midiOutGetNumDevs();
	if (m_dwOutDevs > 0) {
		// Allocate device and CAPS structure
		try {
			m_pOutCaps = new MIDIOUTCAPS[m_dwOutDevs];
		}
		catch (...) {
			return FALSE;
		}
		if (!m_pOutCaps) {
			return FALSE;
		}

		// Get CAPS
		GetOutCaps();
	}

	// In device check
	m_dwInDevs = ::midiInGetNumDevs();
	if (m_dwInDevs > 0) {
		// Allocate device and CAPS structure
		try {
			m_pInCaps = new MIDIINCAPS[m_dwInDevs];
		}
		catch (...) {
			return FALSE;
		}
		if (!m_pInCaps) {
			return FALSE;
		}

		// Get CAPS
		GetInCaps();
	}

	// In queue initialization
	if (!m_InQueue.Init(InBufMax)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::Cleanup()
{
	ASSERT(this);

	// Cleanup
	CloseOut();
	CloseIn();

	// Caps delete
	if (m_pOutCaps) {
		delete[] m_pOutCaps;
		m_pOutCaps = NULL;
	}
	if (m_pInCaps) {
		delete[] m_pInCaps;
		m_pInCaps = NULL;
	}

	// Base class
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::ApplyCfg(const Config* pConfig)
{
	CConfig *pComponent;

	ASSERT(this);
	ASSERT(pConfig);
	ASSERT_VALID(this);

	// Get config component
	pComponent = (CConfig*)SearchComponent(MAKEID('C', 'F', 'G', ' '));
	ASSERT(pComponent);

	// Reset command
	m_nOutReset = pConfig->midi_reset;

	// Out delay
	m_dwOutDelay = (DWORD)(pConfig->midiout_delay * 2000);

	// Out device
	if (pConfig->midiout_device != (int)m_dwOutDevice) {
		// Close first
		CloseOut();

		// Open with specified device
		if (pConfig->midiout_device > 0) {
			OpenOut(pConfig->midiout_device);
		}

		// If successful, set config
		if (m_dwOutDevice != 0) {
			pComponent->SetMIDIDevice((int)m_dwOutDevice, FALSE);
		}
	}

	// In delay
	SetInDelay(pConfig->midiin_delay * 2000);

	// In device
	if (pConfig->midiin_device != (int)m_dwInDevice) {
		// Close first
		CloseIn();

		// Open with specified device
		if (pConfig->midiin_device > 0) {
			OpenIn(pConfig->midiin_device);
		}

		// If successful, set config
		if (m_dwInDevice > 0) {
			pComponent->SetMIDIDevice((int)m_dwInDevice, TRUE);
		}
	}
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void CMIDI::AssertValid() const
{
	// Base class
	CComponent::AssertValid();

	ASSERT(this);
	ASSERT(m_pMIDI);
	ASSERT(m_pMIDI->GetID() == MAKEID('M', 'I', 'D', 'I'));
	ASSERT(m_pScheduler);
	ASSERT(m_pScheduler->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pSch);
	ASSERT(m_pSch->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT((m_dwOutDevs == 0) || m_pOutCaps);
	ASSERT((m_dwInDevs == 0) || m_pInCaps);
	ASSERT((m_nOutReset >= 0) && (m_nOutReset <= 3));
}
#endif	// NDEBUG

//===========================================================================
//
//	OUT procedures
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Out open
//	Note dwDevice=0 is not assigned, dwDevice=1 is MIDI mapper
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::OpenOut(DWORD dwDevice)
{
	MMRESULT mmResult;
	UINT uDeviceID;

	ASSERT(this);
	ASSERT(m_dwOutDevice == 0);
	ASSERT(!m_pOutThread);
	ASSERT_VALID(this);

	// If not assigned, return
	if (dwDevice == 0) {
		return;
	}

	// If no device, return
	if (m_dwOutDevs == 0) {
		return;
	}

	// If device number exceeds range, set to MIDI mapper
	if (dwDevice > (m_dwOutDevs + 1)) {
		dwDevice = 1;
	}

	// Device number
	if (dwDevice == 1) {
		uDeviceID = MIDI_MAPPER;
	}
	else {
		uDeviceID = (UINT)(dwDevice - 2);
	}

	// Open
#if _MFC_VER >= 0x700
	mmResult = ::midiOutOpen(&m_hOut, uDeviceID,
							(DWORD_PTR)OutProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
#else
	mmResult = ::midiOutOpen(&m_hOut, uDeviceID,
							(DWORD)OutProc, (DWORD)this, CALLBACK_FUNCTION);
#endif

	// If failed, exit
	if (mmResult != MMSYSERR_NOERROR) {
		return;
	}

	// Update device number
	m_dwOutDevice = dwDevice;

	// Clear buffer
	m_pMIDI->ClrTransData();

	// Start thread
	m_bOutThread = FALSE;
	m_pOutThread = AfxBeginThread(OutThread, this);
}

//---------------------------------------------------------------------------
//
//	Out close
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::CloseOut()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Stop thread
	if (m_pOutThread) {
		// Set exit flag
		m_bOutThread = TRUE;

		// Wait for exit (infinite)
		::WaitForSingleObject(m_pOutThread->m_hThread, INFINITE);

		// No thread
		m_pOutThread = NULL;
	}

	// Close device
	if (m_dwOutDevice != 0) {
		// Send all notes off and reset, then close
		SendAllNoteOff();
		::midiOutReset(m_hOut);
		::midiOutClose(m_hOut);

		// No open device
		m_dwOutDevice = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Out thread
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::RunOut()
{
	BOOL bEnable;
	BOOL bActive;
	OutState outState;
	DWORD dwTime;
	const MIDI::mididata_t *pData;
	DWORD dwLen;
	DWORD dwCnt;
	DWORD dwCmd;
	DWORD dwShortMsg;

	ASSERT(this);
	ASSERT(m_hOut);
	ASSERT_VALID(this);

	// Get scheduler status
	bEnable = m_pSch->IsEnable();

	// Not sending system exclusive, header not used
	m_bSendEx = FALSE;
	m_bSendExHdr = FALSE;

	// Reset counters
	m_dwShortSend = 0;
	m_dwExSend = 0;
	m_dwUnSend = 0;

	// No data, command reset, state idle
	pData = NULL;
	dwCmd = 0;
	dwLen = 0;
	dwCnt = 0;
	dwShortMsg = 0;
	outState = OutReady;

	// Reset
	SendReset();

	// Not active (sends valid data to MIDI OUT when active)
	bActive = FALSE;

	// Loop
	while (!m_bOutThread) {
		// If sending system exclusive, wait
		if (m_bSendEx) {
			::Sleep(1);
			continue;
		}

		// If system exclusive header is being used, wait
		if (m_bSendExHdr) {
			// Unlock and execute
			m_OutSection.Lock();
			::midiOutUnprepareHeader(m_hOut, &m_ExHdr, sizeof(MIDIHDR));
			m_OutSection.Unlock();

			// System exclusive header prepared flag clear
			m_bSendExHdr = FALSE;
			pData = NULL;
			outState = OutReady;

			// Count up
			m_dwUnSend++;
		}

		// If disabled
		if (!m_bEnable) {
			// If not active, all notes off and reset
			if (bActive) {
				SendAllNoteOff();
				SendReset();
				bActive = FALSE;

				// No data
				outState = OutReady;
				pData = NULL;
			}

			::Sleep(10);
			continue;
		}

		// If scheduler is stopped
		if (!m_pSch->IsEnable()) {
			// If previously active, cannot continue
			if (bEnable) {
				// All notes off
				SendAllNoteOff();

				// Other states are invalid
				bEnable = FALSE;
			}

			// Wait until can continue
			::Sleep(10);
			continue;
		}

		// Scheduler is active
		bEnable = TRUE;

		// If MIDI reset is requested
		if (m_pMIDI->IsReset()) {
			// Clear flag
			m_pMIDI->ClrReset();

			// All notes off and reset
			SendAllNoteOff();
			SendReset();
			pData = NULL;
			outState = OutReady;
		}

		// If MIDI not active, wait
		if (!m_pMIDI->IsActive()) {
			::Sleep(10);
			continue;
		}

		// Get number of MIDI send buffer entries. 0 means Sleep
		if (!pData) {
			if (m_pMIDI->GetTransNum() == 0) {
				// If delay is 15ms or more, 10ms sleep
				if (m_dwOutDelay > 30000) {
					::Sleep(10);
					continue;
				}
				// If delay is 14ms or less, 1ms sleep
				::Sleep(1);
				continue;
			}

			// Get head of send data
			pData = m_pMIDI->GetTransData(0);
		}

		// If 1 byte or more of data exists, set active
		bActive = TRUE;

		// Wait, if system exclusive command is not system exclusive, wait
		if ((outState == OutReady) && (pData->data != MIDI_EXSTART)) {
			dwTime = m_pScheduler->GetTotalTime() - pData->vtime;
			if (dwTime < m_dwOutDelay) {
				if (dwTime > 30000) {
					// If more than 15ms remaining, 1.5ms wait
					::Sleep(10);
					continue;
				}
				if (dwTime > 3000) {
					// If more than 1.5ms remaining, 1.5ms wait
					::Sleep(1);
					continue;
				}
				// Less than 1.5ms remaining, sleep(0) for yield
				::Sleep(0);
				continue;
			}
		}

		// Data processing (first)
		if (outState == OutReady) {
			// If sending system exclusive but not this type, output here (error)
			if ((pData->data & 0x80) && (pData->data != MIDI_EXEND)) {
				if (outState == OutEx) {
					m_ExBuf[dwCnt] = MIDI_EXEND;
					dwCnt++;
					SendEx(m_ExBuf);
					continue;
				}
			}

			// Channel data
			if ((pData->data >= MIDI_NOTEOFF) && (pData->data < MIDI_EXSTART)) {
				// 80-EF
				outState = OutShort;
				dwCmd = pData->data;
				dwCnt = 1;
				dwShortMsg = dwCmd;
				dwLen = 3;

				if ((pData->data >= MIDI_PROGCHG) && (pData->data < MIDI_PITCHCHG)) {
					// C0-DF
					dwLen = 2;
				}
			}

			// Channel messages and real time messages
			switch (pData->data) {
				// System exclusive start
				case MIDI_EXSTART:
					outState = OutEx;
					m_ExBuf[0] = (BYTE)pData->data;
					dwCnt = 1;
					break;
				// Quarter frame
				case MIDI_QFRAME:
				// Song select
				case MIDI_SONGSEL:
					outState = OutShort;
					dwCnt = 1;
					dwShortMsg = pData->data;
					dwLen = 2;
					break;
				// Song position pointer
				case MIDI_SONGPTR:
					outState = OutShort;
					dwCnt = 1;
					dwShortMsg = pData->data;
					dwLen = 3;
					break;
				// Tune request, real time messages
				case MIDI_TUNEREQ:
				case MIDI_TIMINGCLK:
				case MIDI_START:
				case MIDI_CONTINUE:
				case MIDI_STOP:
				case MIDI_ACTSENSE:
				case MIDI_SYSRESET:
					// These real time messages do not output (error here)
					m_pMIDI->DelTransData(1);
					pData = NULL;
					continue;
			}

			// If running status
			if (pData->data < MIDI_NOTEOFF) {
				dwCnt = 2;
				dwShortMsg = (pData->data << 8) | dwCmd;
				outState = OutShort;
			}

#if defined(_DEBUG)
			// Data check (DEBUG only)
			if (outState == OutReady) {
				TRACE("MIDI abnormal data found $%02X\n", pData->data);
			}
#endif	// _DEBUG

			// Delete this data
			m_pMIDI->DelTransData(1);
			pData = NULL;

			// If can send now, send and exit
			if ((outState == OutShort) && (dwCnt == dwLen)) {
				m_OutSection.Lock();
				::midiOutShortMsg(m_hOut, dwShortMsg);
				m_OutSection.Unlock();

				// Count up
				m_dwShortSend++;
				outState = OutReady;
			}
			continue;
		}

		// Data processing (2nd byte and later)
		if (pData->data & 0x80) {
			// If sending system exclusive but not this type, output here (error)
			if (pData->data != MIDI_EXEND) {
				if (outState == OutEx) {
					m_ExBuf[dwCnt] = MIDI_EXEND;
					dwCnt++;
					SendEx(m_ExBuf);
					continue;
				}
			}

			if ((pData->data >= MIDI_NOTEOFF) && (pData->data < MIDI_EXSTART)) {
				// 80-EF
				outState = OutShort;
				dwCmd = pData->data;
				dwCnt = 1;
				dwShortMsg = dwCmd;
				dwLen = 3;

				if ((pData->data >= MIDI_PROGCHG) && (pData->data < MIDI_PITCHCHG)) {
					// C0-DF
					dwLen = 2;
				}
			}

			// Channel messages and real time messages
			switch (pData->data) {
				// System exclusive start
				case MIDI_EXSTART:
					outState = OutEx;
					m_ExBuf[0] = (BYTE)pData->data;
					dwCnt = 1;
					break;
				// Quarter frame
				case MIDI_QFRAME:
				// Song select
				case MIDI_SONGSEL:
					outState = OutShort;
					dwCnt = 1;
					dwShortMsg = pData->data;
					dwLen = 2;
					break;
				// Song position pointer
				case MIDI_SONGPTR:
					outState = OutShort;
					dwCnt = 1;
					dwShortMsg = pData->data;
					dwLen = 3;
					break;
				// Tune request, real time messages
				case MIDI_TUNEREQ:
				case MIDI_TIMINGCLK:
				case MIDI_START:
				case MIDI_CONTINUE:
				case MIDI_STOP:
				case MIDI_ACTSENSE:
				case MIDI_SYSRESET:
					// These real time messages do not output (error here)
					break;
				// System exclusive end
				case MIDI_EXEND:
					if (outState == OutEx) {
						// If sending system exclusive, output here
						m_ExBuf[dwCnt] = (BYTE)pData->data;
						dwCnt++;
						SendEx(m_ExBuf);
						outState = OutReady;

						// Delete this data
						m_pMIDI->DelTransData(1);
						pData = NULL;
						continue;
					}
			}
		}
		else {
			// 00-7F
			if (outState == OutEx) {
				// System exclusive
				m_ExBuf[dwCnt] = (BYTE)pData->data;
				dwCnt++;
				if (dwCnt >= sizeof(m_ExBuf)) {
					outState = OutReady;
				}
			}
			if (outState == OutShort) {
				// Short message
				dwShortMsg |= (pData->data << (dwCnt << 3));
				dwCnt++;
			}
		}

		// Delete this data
		m_pMIDI->DelTransData(1);
		pData = NULL;

		// If can send now, send and exit
		if ((outState == OutShort) && (dwCnt == dwLen)) {
			m_OutSection.Lock();
			::midiOutShortMsg(m_hOut, dwShortMsg);
			m_OutSection.Unlock();

			// Count up
			m_dwShortSend++;
			outState = OutReady;
		}
	}

	// Wait for system exclusive send completion
	SendExWait();

	// All notes off
	SendAllNoteOff();
}

//---------------------------------------------------------------------------
//
//	Out callback
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::CallbackOut(UINT wMsg, DWORD /*dwParam1*/, DWORD /*dwParam2*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Only handle system exclusive send done messages
	if (wMsg == MM_MOM_DONE) {

		// System exclusive send done flag OFF (only BOOL data, so ignore)
		m_bSendEx = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Get Out device caps
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::GetOutCaps()
{
	MMRESULT mmResult;
#if _MFC_VER >= 0x700
	UINT_PTR uDeviceID;
#else
	UINT uDeviceID;
#endif	// MFC_VER

	// Device ID initialization
	uDeviceID = 0;

	// Loop
	while ((DWORD)uDeviceID < m_dwOutDevs) {
		// Get with midiOutGetDevCaps
		mmResult = ::midiOutGetDevCaps( uDeviceID,
										&m_pOutCaps[uDeviceID],
										sizeof(MIDIOUTCAPS));

		// Record result
		if (mmResult == MMSYSERR_NOERROR) {
			m_pOutCaps[uDeviceID].dwSupport = 0;
		}
		else {
			m_pOutCaps[uDeviceID].dwSupport = 1;
		}

		// Device ID increment
		uDeviceID++;
	}
}

//---------------------------------------------------------------------------
//
//	Out thread
//
//---------------------------------------------------------------------------
UINT CMIDI::OutThread(LPVOID pParam)
{
	CMIDI *pMIDI;

	// Receive parameter
	pMIDI = (CMIDI*)pParam;

	// Execute
	pMIDI->RunOut();

	// Exit
	return 0;
}

//---------------------------------------------------------------------------
//
//	Out callback
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CALLBACK CMIDI::OutProc(HMIDIOUT hOut, UINT wMsg, DWORD_PTR dwInstance,
							DWORD dwParam1, DWORD dwParam2)
#else
void CALLBACK CMIDI::OutProc(HMIDIOUT hOut, UINT wMsg, DWORD dwInstance,
							DWORD dwParam1, DWORD dwParam2)
#endif
{
	CMIDI *pMIDI;

	// Receive parameter
	pMIDI = (CMIDI*)dwInstance;

	// Call back
	if (hOut == pMIDI->GetOutHandle()) {
		pMIDI->CallbackOut(wMsg, dwParam1, dwParam2);
	}
}

//---------------------------------------------------------------------------
//
//	System exclusive send
//	Called when previous send completes, header preparation wait is finished
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::SendEx(const BYTE *pExData)
{
	MMRESULT mmResult;
	DWORD dwLength;

	ASSERT(this);
	ASSERT(pExData);
	ASSERT(!m_bSendEx);
	ASSERT(!m_bSendExHdr);
	ASSERT_VALID(this);

	// Count length
	dwLength = 1;
	while (pExData[dwLength - 1] != 0xf7) {
		ASSERT(dwLength <= sizeof(m_ExBuf));
		dwLength++;
	}

	// Validate data range
	ASSERT(pExData[0] == 0xf0);
	ASSERT(pExData[dwLength - 1] == 0xf7);

	// Prepare header
	memset(&m_ExHdr, 0, sizeof(m_ExHdr));
	m_ExHdr.lpData = (LPSTR)pExData;
	m_ExHdr.dwBufferLength = dwLength;
	m_ExHdr.dwBytesRecorded = dwLength;
	m_OutSection.Lock();
	mmResult = ::midiOutPrepareHeader(m_hOut, &m_ExHdr, sizeof(MIDIHDR));
	m_OutSection.Unlock();
	if (mmResult != MMSYSERR_NOERROR) {
		return FALSE;
	}

	// System exclusive header prepared flag ON
	m_bSendExHdr = TRUE;

	// System exclusive send flag set
	m_bSendEx = TRUE;

	// LongMsg send
	m_OutSection.Lock();
	mmResult = ::midiOutLongMsg(m_hOut, &m_ExHdr, sizeof(MIDIHDR));
	m_OutSection.Unlock();
	if (mmResult != MMSYSERR_NOERROR) {
		// Failure, clear flag
		m_bSendEx = FALSE;
		return FALSE;
	}

	// Success
	m_dwExSend++;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Wait for system exclusive send completion
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::SendExWait()
{
	DWORD dwTime;
	DWORD dwDiff;
	BOOL bResult;

	ASSERT(this);
	ASSERT_VALID(this);

	// Initialize flag
	bResult = TRUE;

	// If sending system exclusive
	if (m_bSendEx) {
		// Get current time
		dwTime = ::timeGetTime();

		// Wait until send completes (timeout 2000ms)
		while (m_bSendEx) {
			// Calculate elapsed time
			dwDiff = ::timeGetTime();
			dwDiff -= dwTime;

			// If timeout, treat as failure
			if (dwDiff >= 2000) {
				bResult = FALSE;
				break;
			}

			// Sleep
			::Sleep(1);
		}
	}

	// If system exclusive header is prepared, unlock
	if (m_bSendExHdr) {
		m_OutSection.Lock();
		::midiOutUnprepareHeader(m_hOut, &m_ExHdr, sizeof(MIDIHDR));
		m_OutSection.Unlock();
		m_bSendExHdr = FALSE;
	}

	return bResult;
}

//---------------------------------------------------------------------------
//
//	All notes off
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::SendAllNoteOff()
{
	int nCh;
	DWORD dwMsg;

	ASSERT(this);
	ASSERT_VALID(this);

	// Send All Sound Off for each channel
	m_OutSection.Lock();
	for (nCh=0; nCh<16; nCh++) {
		dwMsg = (DWORD)(0x78b0 + nCh);
		::midiOutShortMsg(m_hOut, dwMsg);
	}
	m_OutSection.Unlock();

	// Note off (MT-32 workaround)
	m_OutSection.Lock();
	for (nCh=0; nCh<16; nCh++) {
		::midiOutShortMsg(m_hOut, (DWORD)(0x7bb0 + nCh));
		::midiOutShortMsg(m_hOut, (DWORD)(0x7db0 + nCh));
		::midiOutShortMsg(m_hOut, (DWORD)(0x7fb0 + nCh));
	}
	m_OutSection.Unlock();
}

//---------------------------------------------------------------------------
//
//	Reset send
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::SendReset()
{
	int nCh;

	ASSERT(this);
	ASSERT_VALID(this);

	// Reset All Controller command
	m_OutSection.Lock();
	for (nCh=0; nCh<16; nCh++) {
		::midiOutShortMsg(m_hOut, (DWORD)(0x79b0 + nCh));
	}
	m_OutSection.Unlock();

	// Reset type
	switch (m_nOutReset) {
		// GM
		case 0:
			SendEx(ResetGM);
			break;

		// GS
		case 1:
			// Maximum output for GM->GS sequence
			SendEx(ResetGM);
			SendExWait();
			SendEx(ResetGS);
			break;

		// XG
		case 2:
			// Maximum output for GM->XG sequence
			SendEx(ResetGM);
			SendExWait();
			SendEx(ResetXG);
			break;

		// LA
		case 3:
			SendEx(ResetLA);
			break;

		// Others (not used)
		default:
			ASSERT(FALSE);
			break;
	};

	// Wait for system exclusive send completion
	SendExWait();

	// Count clear
	m_dwShortSend = 0;
	m_dwExSend = 0;
	m_dwUnSend = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get Out device count
//
//---------------------------------------------------------------------------
DWORD FASTCALL CMIDI::GetOutDevs() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	return m_dwOutDevs;
}

//---------------------------------------------------------------------------
//
//	Get Out device description
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::GetOutDevDesc(int nDevice, CString& strDesc) const
{
	LPCTSTR lpstrDesc;

	ASSERT(this);
	ASSERT(nDevice >= 0);
	ASSERT_VALID(this);

	// Index check
	if (nDevice >= (int)m_dwOutDevs) {
		strDesc.Empty();
		return FALSE;
	}

	// Convert to LPCTSTR
	lpstrDesc = A2T(m_pOutCaps[nDevice].szPname);
	strDesc = lpstrDesc;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set Out delay
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::SetOutDelay(int nDelay)
{
	ASSERT(this);
	ASSERT(nDelay >= 0);
	ASSERT_VALID(this);

	m_dwOutDelay = (DWORD)(nDelay * 2000);
}

//---------------------------------------------------------------------------
//
//	Get Out volume
//
//---------------------------------------------------------------------------
int FASTCALL CMIDI::GetOutVolume()
{
	MMRESULT mmResult;
	DWORD dwVolume;
	int nVolume;

	ASSERT(this);
	ASSERT_VALID(this);

	// If handle is valid and thread is running
	if ((m_dwOutDevice > 0) && m_pOutThread) {
		// Get volume with mixer function, get result
		m_OutSection.Lock();
		mmResult = ::midiOutGetVolume(m_hOut, &dwVolume);
		m_OutSection.Unlock();

		// If successful, LOWORD (left) is used
		if (mmResult == MMSYSERR_NOERROR) {
			// Volume value is 16bit
			nVolume = LOWORD(dwVolume);

			// Return
			return nVolume;
		}
	}

	// MIDI mapper returns -1
	return -1;
}

//---------------------------------------------------------------------------
//
//	Set Out volume
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::SetOutVolume(int nVolume)
{
	DWORD dwVolume;

	ASSERT(this);
	ASSERT((nVolume >= 0) && (nVolume < 0x10000));
	ASSERT_VALID(this);

	// If handle is valid and thread is running
	if ((m_dwOutDevice > 0) && m_pOutThread) {
		// Create value
		dwVolume = (DWORD)nVolume;
		dwVolume <<= 16;
		dwVolume |= (DWORD)nVolume;

		// With mixer function, set volume
		m_OutSection.Lock();
		::midiOutSetVolume(m_hOut, dwVolume);
		m_OutSection.Unlock();
	}
}

//---------------------------------------------------------------------------
//
//	Get Out info
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::GetOutInfo(LPMIDIINFO pInfo) const
{
	ASSERT(this);
	ASSERT(pInfo);
	ASSERT_VALID(this);

	// Device
	pInfo->dwDevices = m_dwOutDevs;
	pInfo->dwDevice = m_dwOutDevice;

	// Counters
	pInfo->dwShort = m_dwShortSend;
	pInfo->dwLong = m_dwExSend;
	pInfo->dwUnprepare = m_dwUnSend;

	// Buffer count from VM device is available, but not set here
}

//---------------------------------------------------------------------------
//
//	GM reset command
//
//---------------------------------------------------------------------------
const BYTE CMIDI::ResetGM[] = {
	0xf0, 0x7e, 0x7f, 0x09, 0x01, 0xf7
};

//---------------------------------------------------------------------------
//
//	GS reset command
//
//---------------------------------------------------------------------------
const BYTE CMIDI::ResetGS[] = {
	0xf0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7f,
	0x00, 0x41, 0xf7
};

//---------------------------------------------------------------------------
//
//	XG reset command
//
//---------------------------------------------------------------------------
const BYTE CMIDI::ResetXG[] = {
	0xf0, 0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00,
	0xf7
};

//---------------------------------------------------------------------------
//
//	LA reset command
//
//---------------------------------------------------------------------------
const BYTE CMIDI::ResetLA[] = {
	0xf0, 0x41, 0x10, 0x16, 0x12, 0x7f, 0x00, 0x00,
	0x00, 0x01, 0xf7
};

//===========================================================================
//
//	IN procedures
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	In open
//	Note dwDevice=0 is not assigned
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::OpenIn(DWORD dwDevice)
{
	MMRESULT mmResult;
	UINT uDeviceID;

	ASSERT(this);
	ASSERT(m_dwInDevice == 0);
	ASSERT(!m_pInThread);
	ASSERT_VALID(this);

	// If not assigned, return
	if (dwDevice == 0) {
		return;
	}

	// If no device, return
	if (m_dwInDevs == 0) {
		return;
	}

	// If device number exceeds range, set to device 0
	if (dwDevice > m_dwInDevs) {
		dwDevice = 1;
	}

	// Device number
	ASSERT(dwDevice >= 1);
	uDeviceID = (UINT)(dwDevice - 1);

	// Open

#if _MFC_VER >= 0x700
	mmResult = ::midiInOpen(&m_hIn, uDeviceID,
							(DWORD_PTR)InProc, (DWORD_PTR)this, CALLBACK_FUNCTION);
#else
	mmResult = ::midiInOpen(&m_hIn, uDeviceID,
							(DWORD)InProc, (DWORD)this, CALLBACK_FUNCTION);
#endif

	// If failed, exit
	if (mmResult != MMSYSERR_NOERROR) {
		return;
	}

	// Update device number
	m_dwInDevice = dwDevice;

	// Start thread
	m_bInThread = FALSE;
	m_pInThread = AfxBeginThread(InThread, this);
}

//---------------------------------------------------------------------------
//
//	In close
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::CloseIn()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Stop thread
	if (m_pInThread) {
		// Set exit flag
		m_bInThread = TRUE;

		// Wait for exit (infinite)
		::WaitForSingleObject(m_pInThread->m_hThread, INFINITE);

		// No thread
		m_pInThread = NULL;
	}

	// Close device
	if (m_dwInDevice != 0) {
		// Reset and close
		::midiInReset(m_hIn);
		::midiInClose(m_hIn);

		// No open device
		m_dwInDevice = 0;
	}
}

//---------------------------------------------------------------------------
//
//	In thread
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::RunIn()
{
	int i;
	BOOL bActive;
	BOOL bValid;
	BOOL bMsg;

	ASSERT(this);
	ASSERT(m_hIn);
	ASSERT_VALID(this);

	// Not active
	bActive = FALSE;

	// System exclusive buffer is used
	m_InState[0] = InNotUsed;
	m_InState[1] = InNotUsed;

	// Reset counters
	m_dwShortRecv = 0;
	m_dwExRecv = 0;
	m_dwUnRecv = 0;

	// Reset
	::midiInReset(m_hIn);

	// Loop
	while (!m_bInThread) {
		// Set valid flag
		bValid = FALSE;
		if (m_pSch->IsEnable()) {
			// Scheduler is running
			if (m_pMIDI->IsActive()) {
				// MIDI active
				bValid = TRUE;
			}
		}

		// If not valid
		if (!bValid) {
			// Sleep
			::Sleep(10);
			continue;
		}

		// If valid, start if not started
		if (!bActive) {
			StartIn();
			bActive = TRUE;
		}

		// If no message
		bMsg = FALSE;

		// Check long message
		for (i=0; i<2; i++) {
			if (m_InState[i] == InDone) {
				// Long message received
				LongIn(i);
				bMsg = TRUE;
			}
		}

		// Check short message
		m_InSection.Lock();
		if (!m_InQueue.IsEmpty()) {
			// Short message received
			ShortIn();
			bMsg = TRUE;
		}
		m_InSection.Unlock();

		// If no message, Sleep
		if (!bMsg) {
			::Sleep(1);
		}
	}

	// Stop
	if (bActive) {
		bActive = FALSE;
		StopIn();
	}
}

//---------------------------------------------------------------------------
//
//	In start
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::StartIn()
{
	int i;
	MMRESULT mmResult;

	ASSERT(this);
	ASSERT(m_hIn);
	ASSERT_VALID(this);

	// Register system exclusive buffer
	for (i=0; i<2; i++) {
		if (m_InState[i] == InNotUsed) {
			// Create MIDI header
			memset(&m_InHdr[i], 0, sizeof(MIDIHDR));
			m_InHdr[i].lpData = (LPSTR)&m_InBuf[i][0];
			m_InHdr[i].dwBufferLength = InBufMax;
			m_InHdr[i].dwUser = (DWORD)i;

			// Prepare header
			mmResult = ::midiInPrepareHeader(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
			if (mmResult != MMSYSERR_NOERROR) {
				// Header preparation failed
				continue;
			}

			// Add header
			mmResult = ::midiInAddBuffer(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
			if (mmResult != MMSYSERR_NOERROR) {
				// Header add failed
				::midiInUnprepareHeader(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
				continue;
			}

			// Set state
			m_InState[i] = InReady;
		}
	}

	// In queue clear
	m_InQueue.Clear();

	// Start MIDI receive
	mmResult = ::midiInStart(m_hIn);
	if (mmResult != MMSYSERR_NOERROR) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	In stop (buffer only)
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::StopIn()
{
	int i;

	ASSERT(this);
	ASSERT(m_hIn);
	ASSERT_VALID(this);

	// Unregister system exclusive buffer
	for (i=0; i<2; i++) {
		// Unprepare header
		if (m_InState[i] != InNotUsed) {
			::midiInUnprepareHeader(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
			m_InState[i] = InNotUsed;
		}
	}
}

//---------------------------------------------------------------------------
//
//	In callback
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::CallbackIn(UINT wMsg, DWORD dwParam1, DWORD /*dwParam2*/)
{
	MIDIHDR *lpMidiHdr;
	DWORD dwLength;
	BYTE msg[3];

	ASSERT(this);
	ASSERT_VALID(this);

	switch (wMsg) {
		// Short message
		case MIM_DATA:
		case MIM_MOREDATA:
			// Create message
			msg[0] = (BYTE)dwParam1;
			msg[1] = (BYTE)(dwParam1 >> 8);
			msg[2] = (BYTE)(dwParam1 >> 16);

			// Respond to status byte to determine length
			if (msg[0] < MIDI_NOTEOFF) {
				// Running status is invalid, so abnormal data
				break;
			}
			dwLength = 3;
			if ((msg[0] >= MIDI_PROGCHG) && (msg[0] < MIDI_PITCHCHG)){
				dwLength = 2;
			}
			if (msg[0] >= MIDI_EXSTART) {
				// F0 or higher messages
				switch (msg[0]) {
					// Song position pointer
					case MIDI_SONGPTR:
						dwLength = 3;
						break;

					// Quarter frame
					case MIDI_QFRAME:
					// Song select
					case MIDI_SONGSEL:
						dwLength = 2;
						break;

					// Tune request
					case MIDI_TUNEREQ:
					// Timing clock
					case MIDI_TIMINGCLK:
					// Start
					case MIDI_START:
					// Continue
					case MIDI_CONTINUE:
					// Stop
					case MIDI_STOP:
					// Active sensing
					case MIDI_ACTSENSE:
					// System reset
					case MIDI_SYSRESET:
						dwLength = 1;

					// Other is not received
					default:
						dwLength = 0;
				}
			}

			// Add data
			if (dwLength > 0) {
				// Lock
				m_InSection.Lock();

				// Insert
				m_InQueue.Insert(msg, dwLength);

				// Unlock
				m_InSection.Unlock();

				// Count up
				m_dwShortRecv++;
			}
			break;

		// Long message
		case MIM_LONGDATA:
		case MIM_LONGERROR:
			// Change flag
			lpMidiHdr = (MIDIHDR*)dwParam1;
			if (lpMidiHdr->dwUser < 2) {
				m_InState[lpMidiHdr->dwUser] = InDone;

				// Count up
				m_dwExRecv++;
			}
			break;

		// Other is not processed
		default:
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Short message receive
//	Note critical section is locked by caller
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::ShortIn()
{
	BYTE buf[InBufMax];
	DWORD dwReceived;

	ASSERT(this);
	ASSERT(m_pMIDI);
	ASSERT_VALID(this);

	// Get everything from queue
	dwReceived = m_InQueue.Get(buf);
	ASSERT(dwReceived > 0);

	// Send to MIDI device
	m_pMIDI->SetRecvData(buf, (int)dwReceived);
}

//---------------------------------------------------------------------------
//
//	Long message receive
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::LongIn(int nHdr)
{
	int i;
	MMRESULT mmResult;

	ASSERT(this);
	ASSERT((nHdr == 0) || (nHdr == 1));
	ASSERT(m_hIn);
	ASSERT_VALID(this);

	// Confirm it was received
	ASSERT(m_InHdr[nHdr].dwFlags & MHDR_DONE);

	// Send to MIDI device
	m_pMIDI->SetRecvData((const BYTE*)m_InHdr[nHdr].lpData,
						 (int)m_InHdr[nHdr].dwBytesRecorded);

	// Unprepare header
	::midiInUnprepareHeader(m_hIn, &m_InHdr[nHdr], sizeof(MIDIHDR));
	m_InState[nHdr] = InNotUsed;
	m_dwUnRecv++;

	// Restart
	for (i=0; i<2; i++) {
		if (m_InState[i] == InNotUsed) {
			// Create MIDI header
			memset(&m_InHdr[i], 0, sizeof(MIDIHDR));
			m_InHdr[i].lpData = (LPSTR)&m_InBuf[i][0];
			m_InHdr[i].dwBufferLength = InBufMax;
			m_InHdr[i].dwUser = (DWORD)i;

			// Prepare header
			mmResult = ::midiInPrepareHeader(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
			if (mmResult != MMSYSERR_NOERROR) {
				// Header preparation failed
				continue;
			}

			// Add header
			mmResult = ::midiInAddBuffer(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
			if (mmResult != MMSYSERR_NOERROR) {
				// Header add failed
				::midiInUnprepareHeader(m_hIn, &m_InHdr[i], sizeof(MIDIHDR));
				continue;
			}

			// Set state
			m_InState[i] = InReady;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Get In device caps
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::GetInCaps()
{
	MMRESULT mmResult;
#if _MFC_VER >= 0x700
	UINT_PTR uDeviceID;
#else
	UINT uDeviceID;
#endif	// MFC_VER

	// Device ID initialization
	uDeviceID = 0;

	// Loop
	while ((DWORD)uDeviceID < m_dwInDevs) {
		// Get with midiInGetDevCaps
		mmResult = ::midiInGetDevCaps(uDeviceID,
									  &m_pInCaps[uDeviceID],
									  sizeof(MIDIINCAPS));

		// Record result
		if (mmResult == MMSYSERR_NOERROR) {
			m_pInCaps[uDeviceID].dwSupport = 0;
		}
		else {
			m_pInCaps[uDeviceID].dwSupport = 1;
		}

		// Device ID increment
		uDeviceID++;
	}
}

//---------------------------------------------------------------------------
//
//	In thread
//
//---------------------------------------------------------------------------
UINT CMIDI::InThread(LPVOID pParam)
{
	CMIDI *pMIDI;

	// Receive parameter
	pMIDI = (CMIDI*)pParam;

	// Execute
	pMIDI->RunIn();

	// Exit
	return 0;
}

//---------------------------------------------------------------------------
//
//	In callback
//
//---------------------------------------------------------------------------
#if _MFC_VER >= 0x700
void CALLBACK CMIDI::InProc(HMIDIIN hIn, UINT wMsg, DWORD_PTR dwInstance,
							DWORD dwParam1, DWORD dwParam2)
#else
void CALLBACK CMIDI::InProc(HMIDIIN hIn, UINT wMsg, DWORD dwInstance,
							DWORD dwParam1, DWORD dwParam2)
#endif
{
	CMIDI *pMIDI;

	// Receive parameter
	pMIDI = (CMIDI*)dwInstance;

	// Call back
	if (hIn == pMIDI->GetInHandle()) {
		pMIDI->CallbackIn(wMsg, dwParam1, dwParam2);
	}
}

//---------------------------------------------------------------------------
//
//	Get In device count
//
//---------------------------------------------------------------------------
DWORD FASTCALL CMIDI::GetInDevs() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	return m_dwInDevs;
}

//---------------------------------------------------------------------------
//
//	Get In device description
//
//---------------------------------------------------------------------------
BOOL FASTCALL CMIDI::GetInDevDesc(int nDevice, CString& strDesc) const
{
	LPCTSTR lpstrDesc;

	ASSERT(this);
	ASSERT(nDevice >= 0);
	ASSERT_VALID(this);

	// Index check
	if (nDevice >= (int)m_dwInDevs) {
		strDesc.Empty();
		return FALSE;
	}

	// Convert to LPCTSTR
	lpstrDesc = A2T(m_pInCaps[nDevice].szPname);
	strDesc = lpstrDesc;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set In delay
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::SetInDelay(int nDelay)
{
	ASSERT(this);
	ASSERT(nDelay >= 0);
	ASSERT_VALID(this);

	// Notify device
	m_pMIDI->SetRecvDelay(nDelay);
}

//---------------------------------------------------------------------------
//
//	Get In info
//
//---------------------------------------------------------------------------
void FASTCALL CMIDI::GetInInfo(LPMIDIINFO pInfo) const
{
	ASSERT(this);
	ASSERT(pInfo);
	ASSERT_VALID(this);

	// Device
	pInfo->dwDevices = m_dwInDevs;
	pInfo->dwDevice = m_dwInDevice;

	// Counters
	pInfo->dwShort = m_dwShortRecv;
	pInfo->dwLong = m_dwExRecv;
	pInfo->dwUnprepare = m_dwUnRecv;

	// Buffer count from VM device is available, but not set here
}

#endif	// _WIN32
