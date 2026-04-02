//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ MFC Scheduler ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "mouse.h"
#include "render.h"
#include "config.h"
#include "mfc_com.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_cpu.h"
#include "mfc_sch.h"

//===========================================================================
//
//	Scheduler
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CScheduler::CScheduler(CFrmWnd *pFrmWnd) : CComponent(pFrmWnd)
{
	// Component identifier
	m_dwID = MAKEID('S', 'C', 'H', 'E');
	m_strDesc = _T("Scheduler");

	// Initialize members
	m_pCPU = NULL;
	m_pRender = NULL;
	m_pThread = NULL;
	m_pSound = NULL;
	m_pInput = NULL;
	m_bExitReq = FALSE;
	m_dwExecTime = 0;
	m_nSubWndNum = 0;
	m_nSubWndDisp = -1;
	m_bMPUFull = FALSE;
	m_bVMFull = FALSE;
	m_dwDrawCount = 0;
	m_dwDrawTime = 0;
	m_bMenu = FALSE;
	m_bActivate = TRUE;
	m_bBackup = TRUE;
	m_bSavedValid = FALSE;
	m_bSavedEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Init()
{
	ASSERT(this);

	// Base class
	if (!CComponent::Init()) {
		return FALSE;
	}

	QueryPerformanceFrequency(&m_liFreq);

	// Get CPU
	ASSERT(!m_pCPU);
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);

	// Get renderer
	ASSERT(!m_pRender);
	m_pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(m_pRender);

	// Search sound component
	ASSERT(!m_pSound);
	m_pSound = (CSound*)SearchComponent(MAKEID('S', 'N', 'D', ' '));
	ASSERT(m_pSound);

	// Search input component
	ASSERT(!m_pInput);
	m_pInput = (CInput*)SearchComponent(MAKEID('I', 'N', 'P', ' '));
	ASSERT(m_pInput);

	// Set multimedia timer to 1ms precision
	::timeBeginPeriod(1);

	// Start thread
	m_pThread = AfxBeginThread(ThreadFunc, this);
	if (!m_pThread) {
		::timeEndPeriod(1);
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Cleanup()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Stop
	Stop();

	// Restore multimedia timer precision
	::timeEndPeriod(1);

	// Base class
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::ApplyCfg(const Config *pConfig)
{
	BOOL bFlag;

	ASSERT(this);
	ASSERT_VALID(this);
	ASSERT(pConfig);

	// Save VM flag
	bFlag = m_bVMFull;

	m_bMPUFull = pConfig->mpu_fullspeed;
	m_bVMFull = pConfig->vm_fullspeed;

	// If changed, reset
	if (bFlag != m_bVMFull) {
		Reset();
	}
}

#if defined(_DEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void CScheduler::AssertValid() const
{
	ASSERT(this);
	ASSERT(GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pCPU);
	ASSERT(m_pCPU->GetID() == MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pRender);
	ASSERT(m_pRender->GetID() == MAKEID('R', 'E', 'N', 'D'));
	ASSERT(m_pSound);
	ASSERT(m_pSound->GetID() == MAKEID('S', 'N', 'D', ' '));
	ASSERT(m_pInput);
	ASSERT(m_pInput->GetID() == MAKEID('I', 'N', 'P', ' '));
}
#endif	// _DEBUG

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Reset()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Reset
	m_dwExecTime = GetTime();
	m_dwDrawTime = m_dwExecTime;
	m_dwDrawBackup = 0;
	m_dwDrawCount = 0;
	m_dwDrawPrev = 0;
}

//---------------------------------------------------------------------------
//
//	Stop
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Stop()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// If thread is running, request termination
	if (m_pThread) {
		// Set exit request
		m_bExitReq = TRUE;

		// Wait for stop
		::WaitForSingleObject(m_pThread->m_hThread, INFINITE);

		// Thread has terminated
		m_pThread = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Save(Fileio *pFio, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Save Enable status at save time (version2.04)
	if (!pFio->Write(&m_bSavedEnable, sizeof(m_bSavedEnable))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CScheduler::Load(Fileio *pFio, int nVer)
{
	ASSERT(this);
	ASSERT(nVer >= 0x0200);
	ASSERT_VALID(this);

	// At save time, Enable status was not saved
	m_bSavedValid = FALSE;

	// version2.04 and later, load
	if (nVer >= 0x0204) {
		if (!pFio->Read(&m_bSavedEnable, sizeof(m_bSavedEnable))) {
			return FALSE;
		}
		m_bSavedValid = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Thread function
//
//---------------------------------------------------------------------------
UINT CScheduler::ThreadFunc(LPVOID pParam)
{
	CScheduler *pSch;

	// Receive parameter
	pSch = (CScheduler*)pParam;
	ASSERT(pSch);
#if defined(_DEBUG)
	pSch->AssertValid();
#endif	// _DEBUG

	// Execute
	pSch->Run();

	// Return code terminates thread
	return 0;
}


//---------------------------------------------------------------------------
//
//	Time (QPC)
//
//---------------------------------------------------------------------------
unsigned __int64 FASTCALL CScheduler::GetTimeMicro()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (li.QuadPart * 1000000ULL) / m_liFreq.QuadPart;
}

unsigned __int64 FASTCALL CScheduler::GetTimeMilli()
{
	LARGE_INTEGER li;
	QueryPerformanceCounter(&li);
	return (li.QuadPart * 1000ULL) / m_liFreq.QuadPart;
}

//---------------------------------------------------------------------------
//
//	Execution
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Run()
{
	VM *pVM;
	Scheduler *pScheduler;
	Render *pRender;
	DWORD dwExecCount;
	DWORD dwCycle;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get VM
	pVM = ::GetVM();
	ASSERT(pVM);
	pScheduler = (Scheduler*)pVM->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(pScheduler);
	pRender = (Render*)pVM->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(pRender);

	// Time counter
	m_dwExecTime = GetTime();
	dwExecCount = 0;
	m_dwDrawTime = m_dwExecTime;
	m_dwDrawBackup = 0;
	m_dwDrawCount = 0;
	m_dwDrawPrev = 0;
	m_bBackup = m_bEnable;

	unsigned __int64 currentTime = GetTimeMicro();
	unsigned __int64 accumulator = 0;

	// Loop until exit request is set
	while (!m_bExitReq) {
		// Permanent diagnostic
		ASSERT_VALID(this);

		// Sleep
		::Sleep(0);

		// Lock
		Lock();

		// If no valid flag is set, machine is stopped
		if (!m_bEnable) {
			// Redraw when activated -> deactivated by breakpoints, power failure, etc.
			if (m_bBackup) {
				m_pFrmWnd->GetView()->Invalidate(FALSE);
				m_bBackup = FALSE;
			}

			// Draw
			Refresh();
			dwExecCount = 0;

			// Processing and time synchronization of other components
			m_pSound->Process(FALSE);
			m_pInput->Process(FALSE);
			m_dwExecTime = GetTime();
			currentTime = GetTimeMicro();
			accumulator = 0;
			Unlock();

			// Sleep
			::Sleep(10);
			continue;
		}

		// Update backup flag (was running just before)
		m_bBackup = TRUE;

		unsigned __int64 newTime = GetTimeMicro();
		unsigned __int64 frameTime = newTime - currentTime;
		currentTime = newTime;

		// Cap the death spiral (for example, if the OS lags by more than 250 ms)
		if (frameTime > 250000) {
			frameTime = 250000;
		}

		// Fast mode processing (ignores the accumulator)
		if (m_bVMFull) {
			if (!pVM->Exec(2000)) {
				SyncDisasm();
				dwExecCount++;
				m_bEnable = FALSE;
				Unlock();
				continue;
			}
			if (!pVM->IsPower()) {
				SyncDisasm();
				dwExecCount++;
				m_bEnable = FALSE;
				Unlock();
				continue;
			}
			m_dwExecTime = GetTime();
			dwExecCount++;
			m_pSound->Process(TRUE);
			m_pInput->Process(TRUE);
			
			pRender->EnableAct(TRUE);
			
			if (dwExecCount > 400) {
				Refresh();
				dwExecCount = 0;
			}
			Unlock();
			continue;
		}
		else if (m_bMPUFull) {
			// MPU full speed mode, keep legacy behavior
			dwCycle = pScheduler->GetCPUCycle();
			m_pCPU->Exec(pScheduler->GetCPUSpeed());
			pScheduler->SetCPUCycle(dwCycle);
			
			// Let accumulator continue normally for video/audio
		}

		accumulator += frameTime;

		// Active wait (spin-lock) for micro-resolution if very close to 1000us
		// and we are not in menu mode
		if (accumulator < 1000 && accumulator > 800 && (!m_bActivate || m_bMenu) == FALSE) {
			Unlock();
			continue;
		}

		while (accumulator >= 1000) {
			// Determine if rendering is possible: if less than 2000 remain,
			// this is the last cycle "up to date"
			if (accumulator < 2000) {
				pRender->EnableAct(TRUE);
			} else {
				pRender->EnableAct(FALSE);
			}

			// Drive the VM
			if (!pVM->Exec(2000)) {
				// Breakpoints
				SyncDisasm();
				dwExecCount++;
				m_bEnable = FALSE;
				break;
			}

			// Check power supply
			if (!pVM->IsPower()) {
				// Power off
				SyncDisasm();
				dwExecCount++;
				m_bEnable = FALSE;
				break;
			}
			
			dwExecCount++;
			m_dwExecTime++;
			accumulator -= 1000;

			// Processing of other components
			m_pSound->Process(TRUE);
			m_pInput->Process(TRUE);

			// Failsafe for slow PCs: if emulator cannot keep up, force draw every 400ms
			if (dwExecCount > 400) {
				Refresh();
				dwExecCount = 0;
			}
		}

		if (!m_bEnable) {
			Unlock();
			continue;
		}

		// We exited the loop, which means the emulator reached real time (accumulator < 1000).
		// This is our "free time", the ideal moment to paint on screen if VM generated a frame.
		if (dwExecCount > 0) {
			Refresh();
			dwExecCount = 0;
		}

		// Sleep once every 8 ms if idle or the menu is open
		if (!m_bActivate || m_bMenu) {
			if ((m_dwExecTime & 0x07) == 0) {
				Unlock();
				::Sleep(1);
				continue;
			}
		}

		// Unlock
		Unlock();
	}
}


//---------------------------------------------------------------------------
//
//	Refresh (e.g., memory)
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::Refresh()
{
	int num;
	CDrawView *pView;

	ASSERT(this);
	ASSERT_VALID(this);
	ASSERT(m_pFrmWnd);

	// Get the view
	pView = m_pFrmWnd->GetView();
	ASSERT(pView);

	// Get the number of subwindows
	num = pView->GetSubWndNum();

	// Reset if the number of pieces differs from the saved value
	if (m_nSubWndNum != num) {
		m_nSubWndNum = num;
		m_nSubWndDisp = -1;
	}

	if (m_bEnable) {
		// Running and it's main screen turn
		if (m_nSubWndDisp < 0) {
			// If renderer not ready, no drawing
			if (!m_pRender->IsReady()) {
				return;
			}
			SyncDisasm();
		}
	}

	// Screen (partial)
	pView->Draw(m_nSubWndDisp);

	// If main screen, complete frame like in legacy flow
	if (m_nSubWndDisp < 0) {
		if (m_pRender) {
			m_pRender->Complete();
		}
		m_dwDrawCount++;
	}

	// Next window
	m_nSubWndDisp++;
	if (m_nSubWndDisp >= m_nSubWndNum) {
		m_nSubWndDisp = -1;
	}
}

//---------------------------------------------------------------------------
//
//	PC sync with disassembler
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::SyncDisasm()
{
	CDisasmWnd *pWnd;
	CDrawView *pView;
	DWORD dwID;
	int i;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get the view
	pView = m_pFrmWnd->GetView();
	ASSERT(pView);

	// Up to 8 pieces maximum
	for (i=0; i<8; i++) {
		dwID = MAKEID('D', 'I', 'S', ('A' + i));
		pWnd = (CDisasmWnd*)pView->SearchSWnd(dwID);
		if (pWnd) {
			// Results
			pWnd->SetPC(m_pCPU->GetPC());
		}
	}
}

//---------------------------------------------------------------------------
//
//	Get frame rate
//
//---------------------------------------------------------------------------
int FASTCALL CScheduler::GetFrameRate()
{
	DWORD dwCount;
	DWORD dwTime;
	DWORD dwDiff;

	ASSERT(this);
	ASSERT_VALID(this);

	// Invalid control
	if (!m_bEnable) {
		return 0;
	}

	// Get time (loop support, initialize if long)
	dwTime = GetTime();
	if (dwTime <= m_dwDrawTime) {
		m_dwDrawTime = dwTime;
		m_dwDrawBackup = 0;
		m_dwDrawCount = 0;
		m_dwDrawPrev = 0;
		return 0;
	}
	dwDiff = dwTime - m_dwDrawTime;
	if (dwDiff > 3500) {
		m_dwDrawTime = dwTime;
		m_dwDrawBackup = 0;
		m_dwDrawCount = 0;
		m_dwDrawPrev = 0;
		return 0;
	}

	// If difference is less than 500 ms, normal processing
	if (dwDiff < 500) {
		m_dwDrawBackup = 0;
		dwDiff /= 10;
		dwCount = m_dwDrawCount * 1000;
		if (dwDiff == 0) {
			return 0;
		}
		return (dwCount / dwDiff);
	}

	// Memory if difference is less than 1000 ms
	if (dwDiff < 1000) {
		if (m_dwDrawBackup == 0) {
			m_dwDrawBackup = dwTime;
			m_dwDrawPrev = m_dwDrawCount;
		}
		dwDiff /= 10;
		dwCount = m_dwDrawCount * 1000;
		return (dwCount / dwDiff);
	}

	// More than that, so switch to Prev
	dwDiff /= 10;
	dwCount = m_dwDrawCount * 1000;
	m_dwDrawTime = (m_dwDrawBackup != 0) ? m_dwDrawBackup : dwTime;
	m_dwDrawBackup = 0;
	if (m_dwDrawTime == dwTime) {
		m_dwDrawCount = 0;
		m_dwDrawPrev = 0;
	} else {
		m_dwDrawCount -= m_dwDrawPrev;
		m_dwDrawPrev = 0;
	}
	return (dwCount / dwDiff);
}

//---------------------------------------------------------------------------
//
//	Async confirmation of frame consumed by UI
//
//---------------------------------------------------------------------------
void FASTCALL CScheduler::OnMainFramePresented()
{
	// Kept for compatibility. Counter and Complete
	// are now updated in Refresh() to preserve original timing
}

#endif	// _WIN32
