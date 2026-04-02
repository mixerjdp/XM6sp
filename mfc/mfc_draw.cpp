//---------------------------------------------------------------------------
//
// X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
// [ MFC draw view ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "render.h"
#include "crtc.h"
#include "config.h"
#include "mfc_sub.h"
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_cpu.h"
#include "mfc_cfg.h"
#include "mfc_res.h"
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#include "mfc_inp.h"
#include "mfc_draw.h"

#define WM_XM6_PRESENT (WM_APP + 0x120)
#define RENDERCMD_INIT 1
#define RENDERCMD_RESET 2
#define RENDERCMD_CLEANUP 3
#define RENDERCMD_SHADER_APPLY_STATE 4

static void WaitRenderAck(HANDLE hEvent, DWORD dwTimeout = 2000)
{
	DWORD dwStart = GetTickCount();
	while (true) {
		DWORD dwElapsed = GetTickCount() - dwStart;
		DWORD dwTimeLeft = dwTimeout;
		if (dwTimeout != INFINITE) {
			if (dwElapsed >= dwTimeout) break;
			dwTimeLeft = dwTimeout - dwElapsed;
		}
		DWORD dwWait = MsgWaitForMultipleObjects(1, &hEvent, FALSE, dwTimeLeft, QS_ALLINPUT);
		if (dwWait == WAIT_OBJECT_0) break;
		if (dwWait == WAIT_OBJECT_0 + 1) {
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
				if (msg.message == WM_QUIT) {
					PostQuitMessage((int)msg.wParam);
					return;
				}
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		} else {
			break;
		}
	}
}

//===========================================================================
//
// Draw view
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDrawView::CDrawView()
{
	// Basic component initialization
	m_bEnable = FALSE;
	m_pSubWnd = NULL;
	m_pFrmWnd = NULL;
	m_bUseDX9 = TRUE;
	m_lPresentPending = 0;
	m_hRenderEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRenderExitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_hRenderAckEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	m_lRenderCmd = 0;
	m_lPendingShaderEnable=-1;	// -1: no change
	m_lScaleIndex = 0;
	m_pRenderThread = AfxBeginThread(RenderThreadFunc, this, THREAD_PRIORITY_ABOVE_NORMAL, 0, CREATE_SUSPENDED);
	if (m_pRenderThread) {
		m_pRenderThread->m_bAutoDelete = FALSE;
		m_pRenderThread->ResumeThread();
	}
	m_pStagingBuffer = NULL;
	m_nStagingWidth = 0;
	m_nStagingHeight = 0;
	m_bShowOSD = FALSE;
	m_lShaderEnabled=0;	// 0: disabled, 1: enabled (use InterlockedExchange)
	m_dwOSDUntil = 0;
	m_dwPerfOSDLastTick = 0;
	m_nPerfFPS = 0;
	m_szPerfLine[0] = _T('\0');
	m_szOSDText[0] = _T('\0');

	// Components
	m_pScheduler = NULL;
	m_pInput = NULL;

	// General drawing component initialization
	m_Info.bPower = FALSE;
	m_Info.pRender = NULL;
	m_Info.pWork = NULL;
	m_Info.dwDrawCount = 0;

	// DIB section initialization
	m_Info.hBitmap = NULL;
	m_Info.pBits = NULL;
	m_Info.nBMPWidth = 0;
	m_Info.nBMPHeight = 0;

	// Size adjustment initialization
	m_Info.nRendWidth = 0;
	m_Info.nRendHeight = 0;
	m_Info.nRendHMul = 0;
	m_Info.nRendVMul = 0;
	m_Info.nLeft = 0;
	m_Info.nTop = 0;
	m_Info.nWidth = 0;
	m_Info.nHeight = 0;

	// Blt initialization
	m_Info.nBltTop = 0;
	m_Info.nBltBottom = 0;
	m_Info.nBltLeft = 0;
	m_Info.nBltRight = 0;
	m_Info.nScaleIndex = 0;
	m_Info.bBltAll = TRUE;
	m_Info.bBltStretch = TRUE;
}

//---------------------------------------------------------------------------
//
// Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CDrawView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
	ON_MESSAGE(WM_XM6_PRESENT, OnPresentFrame)
	ON_WM_DROPFILES()
#if _MFC_VER >= 0x600
	ON_WM_MOUSEWHEEL()
#endif	// _MFC_VER
	ON_WM_KEYDOWN()
	ON_WM_SYSKEYDOWN()
	ON_WM_KEYUP()
	ON_WM_SYSKEYUP()
	ON_WM_MOVE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::Init(CWnd *pParent, BOOL bShaderEnabled)
{
	ASSERT(pParent);

	// Frame window pointer
	m_pFrmWnd = (CFrmWnd*)pParent;

	// Initialize shader state
	InterlockedExchange(&m_lShaderEnabled, bShaderEnabled ? 1 : 0);
	InterlockedExchange(&m_lPendingShaderEnable, bShaderEnabled ? 1 : 0);

	// Create as the first view
	if (!Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
				CRect(0, 0, 0, 0), pParent, AFX_IDW_PANE_FIRST, NULL)) {
		return FALSE;
	}

	CRect rect;
	GetClientRect(&rect);
	if (rect.Width() <= 0) {
		rect.right = 640;
	}
	if (rect.Height() <= 0) {
		rect.bottom = 480;
	}

	if (m_bUseDX9 && m_pRenderThread) {
		m_nRenderWidth = rect.Width();
		m_nRenderHeight = rect.Height();
		m_bRenderVSync = (m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE;
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_INIT);
		SetEvent(m_hRenderEvent);
		WaitRenderAck(m_hRenderAckEvent);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Preparing window creation
//
//---------------------------------------------------------------------------
BOOL CDrawView::PreCreateWindow(CREATESTRUCT& cs)
{
	// Base class
	if (!CView::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Add WS_CLIPCHILDREN
	cs.style |= WS_CLIPCHILDREN;

	// Add a client edge
	cs.dwExStyle |= WS_EX_CLIENTEDGE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Window creation
//
//---------------------------------------------------------------------------
int CDrawView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	// Base class
	if (CView::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Disable IME
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Create text fonts
	if (IsJapanese()) {
		// Japanese language environment
		m_TextFont.CreateFont(14, 0, 0, 0,
							FW_NORMAL, 0, 0, 0,
							SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							FIXED_PITCH, NULL);
	}
	else {
		// English language environment
		m_TextFont.CreateFont(14, 0, 0, 0,
							FW_NORMAL, 0, 0, 0,
							DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							FIXED_PITCH, NULL);
	}

	// Enable drag and drop
	DragAcceptFiles(TRUE);

	return 0;
}

//---------------------------------------------------------------------------
//
// Window destruction
//
//---------------------------------------------------------------------------
void CDrawView::OnDestroy()
{
	BOOL bHadRenderThread;
	// Stop operation
	Enable(FALSE);
	bHadRenderThread = (m_pRenderThread != NULL);

	// Delete bitmap
	if (m_Info.hBitmap) {
		::DeleteObject(m_Info.hBitmap);
		m_Info.hBitmap = NULL;
		m_Info.pBits = NULL;
	}

	if (m_pRenderThread) {
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_CLEANUP);
		SetEvent(m_hRenderEvent);
		WaitRenderAck(m_hRenderAckEvent);
		SetEvent(m_hRenderExitEvent);
		WaitRenderAck(m_pRenderThread->m_hThread, INFINITE);
		delete m_pRenderThread;
		m_pRenderThread = NULL;
	}
	if (m_hRenderEvent) { CloseHandle(m_hRenderEvent); m_hRenderEvent = NULL; }
	if (m_hRenderExitEvent) { CloseHandle(m_hRenderExitEvent); m_hRenderExitEvent = NULL; }
	if (m_hRenderAckEvent) { CloseHandle(m_hRenderAckEvent); m_hRenderAckEvent = NULL; }
	if (!bHadRenderThread) {
		m_DX9Renderer.Cleanup();
	}

	// Delete text font
	m_TextFont.DeleteObject();
	if (m_pStagingBuffer) {
		free(m_pStagingBuffer);
		m_pStagingBuffer = NULL;
		m_nStagingWidth = 0;
		m_nStagingHeight = 0;
	}

	// Call the base class
	CView::OnDestroy();
}

//---------------------------------------------------------------------------
//
// Resize
//
//---------------------------------------------------------------------------
void CDrawView::OnSize(UINT nType, int cx, int cy)
{
	// Update bitmap
	SetupBitmap();
	if (m_bUseDX9 && m_pRenderThread) {
		m_nRenderWidth = cx;
		m_nRenderHeight = cy;
		m_bRenderVSync = (m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE;
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_RESET);
		SetEvent(m_hRenderEvent);
		WaitRenderAck(m_hRenderAckEvent);
	}

	// Base class
	CView::OnSize(nType, cx, cy);
}

//---------------------------------------------------------------------------
//
// Draw
//
//---------------------------------------------------------------------------
void CDrawView::OnPaint()
{
	Render *pRender;
	CRTC *pCRTC;
	const CRTC::crtc_t *p;
	CFrmWnd *pFrmWnd;
	PAINTSTRUCT ps;

	// Lock the VM
	::LockVM();

	// Force all drawing flags on
	m_Info.bBltAll = TRUE;

	// If enabled and the scheduler is disabled, render into the mix buffer (forced)
	if (m_bEnable) {
		pFrmWnd = (CFrmWnd*)GetParent();
		ASSERT(pFrmWnd);
		if (!pFrmWnd->GetScheduler()->IsEnable()) {
			// If enabled, fetch the renderer and CRTC
			pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
			ASSERT(pRender);
			pCRTC = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
			ASSERT(pCRTC);
			p = pCRTC->GetWorkAddr();

			// Render the frame
			m_Info.bPower = ::GetVM()->IsPower();
			if (m_Info.bPower) {
				pRender->Complete();
				pRender->EnableAct(TRUE);
				pRender->StartFrame();
				pRender->HSync(p->v_dots);
				pRender->EndFrame();
			}
			else {
				// Clear all bitmaps
				memset(m_Info.pBits, 0, m_Info.nBMPWidth * m_Info.nBMPHeight * 4);
			}

			// Draw (via CDrawView::OnDraw)
			CView::OnPaint();

			// Unlock the VM
			::UnlockVM();
			return;
		}
	}

	// Acquire the DCs only
	BeginPaint(&ps);
	EndPaint(&ps);

	// Unlock the VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
// Background drawing
//
//---------------------------------------------------------------------------
BOOL CDrawView::OnEraseBkgnd(CDC *pDC)
{
	CRect rect;

	// If disabled, fill with black
	if (!m_bEnable) {
		GetClientRect(&rect);
		pDC->FillSolidRect(&rect, RGB(0, 0, 0));
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Handle display environment changes
//
//---------------------------------------------------------------------------
LRESULT CDrawView::OnDisplayChange(WPARAM /* wParam */, LPARAM /* lParam */)
{
	// Prepare the bitmap
	SetupBitmap();

	return 0;
}

//---------------------------------------------------------------------------
//
// File drop
//
//---------------------------------------------------------------------------
void CDrawView::OnDropFiles(HDROP hDropInfo)
{
	TCHAR szPath[_MAX_PATH];
	POINT point;
	CRect rect;
	int nFiles;
	int nDrive;

	// Get the number of dropped files
	nFiles = ::DragQueryFile(hDropInfo, 0xffffffff, szPath, _MAX_PATH);
	ASSERT(nFiles > 0);

	// Determine the drive from the drop position
	::DragQueryPoint(hDropInfo, &point);
	GetClientRect(rect);
	if (point.x < (rect.right >> 1)) {
		// Left half (drive 0)
		nDrive = 0;
	}
	else {
		// Right half (drive 1)
		nDrive = 1;
	}

	// Dispatch based on the number of files
	if (nFiles == 1) {
		// One file: use the left and right halves of the window
		::DragQueryFile(hDropInfo, 0, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(nDrive, szPath);
	}
	else {
		// Two files: assign them to 0 and 1 respectively
		::DragQueryFile(hDropInfo, 0, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(0, szPath);
		::DragQueryFile(hDropInfo, 1, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(1, szPath);
	}

	// Finish processing
	::DragFinish(hDropInfo);

	// Reset when two files are dropped
	if (nFiles > 1) {
		m_pFrmWnd->PostMessage(WM_COMMAND, IDM_RESET, 0);
	}
}

//---------------------------------------------------------------------------
//
// Mouse wheel
//
//---------------------------------------------------------------------------
BOOL CDrawView::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
	CConfig *pConfig;

	// Get settings
	pConfig = m_pFrmWnd->GetConfig();

	// Lock the VM
	::LockVM();

	// Depends on the Z-axis direction
	if (zDelta > 0) {
		// Backward: zoom in
		int nScale = GetScaleIndex();
		if (nScale < 4) {
			nScale++;
		}
		SetScaleIndex(nScale);
		pConfig->SetWindowScale(nScale);
	}
	else {
		// Forward: zoom out
		int nScale = GetScaleIndex();
		if (nScale > 0) {
			nScale--;
		}
		SetScaleIndex(nScale);
		pConfig->SetWindowScale(nScale);
	}

	// Unlock the VM
	::UnlockVM();

	// Resize outside the VM lock to avoid short audio stalls.
	m_pFrmWnd->ApplyWindowScale();

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Key pressed
//
//---------------------------------------------------------------------------
void CDrawView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Determine which keys to filter out
	if (!KeyUpDown(nChar, nFlags, TRUE)) {
		return;
	}

	if (nChar == VK_F8) {
		ToggleRenderer();
		return;
	}

	// Pass to the base class
	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

//---------------------------------------------------------------------------
//
// System key pressed
//
//---------------------------------------------------------------------------
void CDrawView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Determine which keys to filter out
	if (!KeyUpDown(nChar, nFlags, TRUE)) {
		return;
	}

	// Pass to the base class
	CView::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

//---------------------------------------------------------------------------
//
// Key released
//
//---------------------------------------------------------------------------
void CDrawView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Determine which keys to filter out
	if (!KeyUpDown(nChar, nFlags, FALSE)) {
		return;
	}

	// Pass to the base class
	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

//---------------------------------------------------------------------------
//
// System key released
//
//---------------------------------------------------------------------------
void CDrawView::OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	// Determine which keys to filter out
	if (!KeyUpDown(nChar, nFlags, FALSE)) {
		return;
	}

	// Pass to the base class
	CView::OnSysKeyUp(nChar, nRepCnt, nFlags);
}

//---------------------------------------------------------------------------
//
// Key handling
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::KeyUpDown(UINT nChar, UINT nFlags, BOOL bDown)
{
#if defined(INPUT_MOUSE) && defined(INPUT_KEYBOARD) && defined(INPUT_HARDWARE)
	INPUT input;

	ASSERT(this);
	ASSERT(nChar < 0x100);

	// Get the scheduler
	if (!m_pScheduler) {
		m_pScheduler = m_pFrmWnd->GetScheduler();
		if (!m_pScheduler) {
			// No scheduler, nothing to filter
			return TRUE;
		}
	}

	// Get the input handler
	if (!m_pInput) {
		m_pInput = m_pFrmWnd->GetInput();
		if (!m_pScheduler) {
			// No input handler, nothing to filter
			return TRUE;
		}
	}

	// If the scheduler is stopped, do not filter
	if (!m_pScheduler->IsEnable()) {
		return TRUE;
	}

	// If input is inactive or in menu mode, do not filter
	if (!m_pInput->IsActive()) {
		return TRUE;
	}
	if (m_pInput->IsMenu()) {
		return TRUE;
	}

	// Key lookup
	switch (nChar) {
		// F10
		case VK_F10:
			if (m_pInput->IsKeyMapped(DIK_F10)) {
				// Mapped
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Left Alt
		case VK_LMENU:
			if (m_pInput->IsKeyMapped(DIK_LMENU)) {
				if (bDown) {
					// Allow other keys to interrupt
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}

				// Mapped
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Right Alt
		case VK_RMENU:
			if (m_pInput->IsKeyMapped(DIK_RMENU)) {
				// Mapped
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Common Alt
		case VK_MENU:
			if (m_pInput->IsKeyMapped(DIK_LMENU) || m_pInput->IsKeyMapped(DIK_RMENU)) {
				// Mapped
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Left Windows
		case VK_LWIN:
			if (m_pInput->IsKeyMapped(DIK_LWIN)) {
				// Mapped
				if (bDown) {
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Right Windows
		case VK_RWIN:
			if (m_pInput->IsKeyMapped(DIK_RWIN)) {
				// Mapped
				if (bDown) {
					// Do not block the Windows key; allow other keys to interrupt
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}
				return FALSE;
			}
			// Not mapped
			return TRUE;

		// Other keys
		default:
			// Is this an Alt key?
			if (nFlags & 0x2000) {
				// Are Left/Right Alt mapped?
				if (m_pInput->IsKeyMapped(DIK_LMENU) || m_pInput->IsKeyMapped(DIK_RMENU)) {
					// If any Alt key is mapped, disable Alt+key
					return FALSE;
				}
			}
			break;
	}
#endif	// INPUT_MOUSE && INPUT_KEYBOARD && INPUT_HARDWARE

	// Allow in other cases
	return TRUE;
}

//---------------------------------------------------------------------------
//
// Window move
//
//---------------------------------------------------------------------------
void CDrawView::OnMove(int x, int y)
{
	ASSERT(m_pFrmWnd);

	// Base class
	CView::OnMove(x, y);

	// Request a frame window position change
	m_pFrmWnd->RecalcStatusView();
}

//---------------------------------------------------------------------------
//
// Prepare the bitmap
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::SetupBitmap()
{
	CClientDC *pDC;
	BITMAPINFOHEADER *p;
	CRect rect;

	// If a bitmap exists, release it first
	if (m_Info.hBitmap) {
		if (m_Info.pRender) {
			m_Info.pRender->SetMixBuf(NULL, 0, 0);
		}
		::DeleteObject(m_Info.hBitmap);
		m_Info.hBitmap = NULL;
		m_Info.pBits = NULL;
	}

	// Special handling for minimization
	GetClientRect(&rect);
	if ((rect.Width() == 0) || (rect.Height() == 0)) {
		return;
	}


	// Allocate memory for bitmap headers
	p = (BITMAPINFOHEADER*) new BYTE[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)];
	memset(p, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD));

	// Create bitmap information
	m_Info.nBMPWidth = rect.Width();

	/* Bitmap height is set here */
	m_Info.nBMPHeight = (rect.Height() < 512) ? 512 : rect.Height();
	p->biSize = sizeof(BITMAPINFOHEADER);
	p->biWidth = m_Info.nBMPWidth;
	p->biHeight = -m_Info.nBMPHeight;
	p->biPlanes = 1;
	p->biBitCount = 32;
	p->biCompression = BI_RGB;
	p->biSizeImage = m_Info.nBMPWidth * m_Info.nBMPHeight * (32 >> 3);

	// Acquire the DC and create the DIB section
	pDC = new CClientDC(this);
	m_Info.hBitmap = ::CreateDIBSection(pDC->m_hDC, (BITMAPINFO*)p, DIB_RGB_COLORS,
								(void**)&(m_Info.pBits), NULL, 0);
	// If successful, notify the renderer
	if (m_Info.hBitmap && m_Info.pRender) {
		m_Info.pRender->SetMixBuf(m_Info.pBits, m_Info.nBMPWidth, m_Info.nBMPHeight);
	}
	delete pDC;
	delete[] p;

	// Recalculate
	m_Info.nRendHMul = -1;
	m_Info.nRendVMul = -1;
	ReCalc(rect);
}

//---------------------------------------------------------------------------
//
// Operation control
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::Enable(BOOL bEnable)
{
	CSubWnd* pWnd;

	// Flag storage
	m_bEnable = bEnable;

	// Renderer storage when enabled
	if (m_bEnable) {
		if (!m_Info.pRender) {
			m_Info.pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
			ASSERT(m_Info.pRender);
			m_Info.pWork = m_Info.pRender->GetWorkAddr();
			ASSERT(m_Info.pWork);
			if (m_Info.pBits) {
				m_Info.pRender->SetMixBuf(m_Info.pBits, m_Info.nBMPWidth, m_Info.nBMPHeight);
			}
		}
	}

	// Subwindow handling
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Enable(bEnable);
		pWnd = pWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
// Get operation flags
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsEnable() const
{
	return m_bEnable;
}

//---------------------------------------------------------------------------
//
// Refresh drawing
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::Refresh()
{
	CSubWnd *pWnd;
	CClientDC dc(this);

	// Redraw the draw view
	OnDraw(&dc);

	// Redraw the subwindow
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Refresh();

		// Next subwindow
		pWnd = pWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
// Draw (from the scheduler)
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::Draw(int nChildWnd)
{
	CSubWnd *pSubWnd;
	CClientDC *pDC;

	ASSERT(nChildWnd >= -1);

	// -1 selects the Draw view
	if (nChildWnd < 0) {
		if (m_bUseDX9) {
			RequestPresent();
		} else {
			pDC = new CClientDC(this);
			OnDraw(pDC);
			delete pDC;
		}
		return;
	}

	// Subwindows start at 0
	pSubWnd = m_pSubWnd;

	while (nChildWnd > 0) {
		// Next subwindow
		pSubWnd = pSubWnd->m_pNextWnd;
		ASSERT(pSubWnd);
		nChildWnd--;
	}

	// Refresh
	pSubWnd->Refresh();
}

//---------------------------------------------------------------------------
//
// Update from the message thread
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::Update()
{
	CSubWnd *pWnd;

	// Subwindow handling
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Update();
		pWnd = pWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
// Request frame presentation on the UI thread
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::RequestPresent()
{
	if (!m_hWnd) {
		return;
	}
	if (::InterlockedExchange(&m_lPresentPending, 1) == 0) {
		if (m_bUseDX9 && m_pRenderThread) {
			if (!SetEvent(m_hRenderEvent)) {
				::InterlockedExchange(&m_lPresentPending, 0);
			}
		} else {
			if (!PostMessage(WM_XM6_PRESENT, 0, 0)) {
				::InterlockedExchange(&m_lPresentPending, 0);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// Update VSync in the DX renderer
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::SetVSync(BOOL bEnable)
{
	if (m_bUseDX9 && m_pRenderThread) {
		CRect rect;
		GetClientRect(&rect);
		m_nRenderWidth = (rect.Width() > 0) ? rect.Width() : 1;
		m_nRenderHeight = (rect.Height() > 0) ? rect.Height() : 1;
		m_bRenderVSync = bEnable;
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_RESET);
		SetEvent(m_hRenderEvent);
		WaitRenderAck(m_hRenderAckEvent);
	}
}

//---------------------------------------------------------------------------
//
// Apply renderer settings
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ApplyRendererConfig(int nRenderMode)
{
	BOOL bWantDX9 = (nRenderMode == 1);

	if (m_bUseDX9 != bWantDX9) {
		ToggleRenderer();
	}
}

//---------------------------------------------------------------------------
//
// Toggle between DX9 and GDI
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ToggleRenderer()
{
	m_bUseDX9 = !m_bUseDX9;
	if (m_bUseDX9 && m_pRenderThread) {
		CRect rect;
		GetClientRect(&rect);
		m_nRenderWidth = (rect.Width() > 0) ? rect.Width() : 640;
		m_nRenderHeight = (rect.Height() > 0) ? rect.Height() : 480;
		m_bRenderVSync = (m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE;
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_INIT);
		SetEvent(m_hRenderEvent);
		WaitRenderAck(m_hRenderAckEvent);
	}
	ShowRenderStatusOSD((m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);
}

//---------------------------------------------------------------------------
//
// Show the renderer/VSync status OSD
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ShowRenderStatusOSD(BOOL bVSync)
{
	CString status;
	status.Format(_T("%s | VSync: %s"),
		m_bUseDX9 ? _T("DirectX 9") : _T("GDI Mode"),
		bVSync ? _T("ON") : _T("OFF"));
	m_dwPerfOSDLastTick = 0;
	ShowOSD((LPCTSTR)status);
	RequestPresent();
}

//---------------------------------------------------------------------------
//
// Toggle CRT shader
//
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//
// Toggle CRT shader (thread-safe)
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ToggleShader()
{
	// Thread-safe toggle using InterlockedCompareExchange
	LONG oldValue, newValue;
	do {
		oldValue = m_lShaderEnabled;
		newValue = (oldValue == 0) ? 1 : 0;
	} while (InterlockedCompareExchange(&m_lShaderEnabled, newValue, oldValue) != oldValue);

	if (m_bUseDX9 && m_pRenderThread) {
		// Notify the render thread of the state change
		InterlockedExchange(&m_lPendingShaderEnable, newValue);
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_SHADER_APPLY_STATE);
		SetEvent(m_hRenderEvent);
	}
	// Show OSD
	CString status;
	status.Format(_T("CRT Shader: %s"), newValue ? _T("ON") : _T("OFF"));
	m_dwPerfOSDLastTick = 0;
	ShowOSD((LPCTSTR)status);
	RequestPresent();
}

//---------------------------------------------------------------------------
//
// Enable/disable shader (thread-safe)
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::SetShaderEnabled(BOOL bEnable)
{
	LONG newValue = bEnable ? 1 : 0;
	InterlockedExchange(&m_lShaderEnabled, newValue);

	// Always keep the pending value so it can be applied when the render thread is ready
	InterlockedExchange(&m_lPendingShaderEnable, newValue);

	if (m_bUseDX9 && m_pRenderThread) {
		// Notify the render thread of the state change
		InterlockedExchange(&m_lRenderCmd, RENDERCMD_SHADER_APPLY_STATE);
		SetEvent(m_hRenderEvent);
	}
}

//---------------------------------------------------------------------------
//
// Get shader state (thread-safe)
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsShaderEnabled() const
{
	return (m_lShaderEnabled != 0) ? TRUE : FALSE;
}

//---------------------------------------------------------------------------
//
// Check whether DX9 mode is active
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsDX9Active() const
{
	return m_bUseDX9 && m_pRenderThread != NULL;
}

//---------------------------------------------------------------------------
//
// Rendering thread
//
//---------------------------------------------------------------------------
UINT CDrawView::RenderThreadFunc(LPVOID pParam)
{
	CDrawView* pView = (CDrawView*)pParam;
	pView->RenderLoop();
	return 0;
}

void FASTCALL CDrawView::RenderLoop()
{
	HANDLE hEvents[2] = { m_hRenderExitEvent, m_hRenderEvent };

	while (true) {
		DWORD dwWait = WaitForMultipleObjects(2, hEvents, FALSE, INFINITE);
		if (dwWait == WAIT_OBJECT_0) {
			if (m_DX9Renderer.IsInitialized()) {
				m_DX9Renderer.Cleanup();
			}
			::InterlockedExchange(&m_lPresentPending, 0);
			break;
		}

		if (dwWait == WAIT_OBJECT_0 + 1) {
			int cmd = InterlockedExchange(&m_lRenderCmd, 0);

			if (cmd == RENDERCMD_INIT) {
				m_DX9Renderer.Init(m_hWnd, m_nRenderWidth, m_nRenderHeight, TRUE, m_bRenderVSync);
				SetEvent(m_hRenderAckEvent);
			} else if (cmd == RENDERCMD_RESET) {
				m_DX9Renderer.ResetDevice(m_nRenderWidth, m_nRenderHeight, TRUE, m_bRenderVSync);
				SetEvent(m_hRenderAckEvent);
			} else if (cmd == RENDERCMD_CLEANUP) {
				if (m_DX9Renderer.IsInitialized()) {
					m_DX9Renderer.Cleanup();
				}
				SetEvent(m_hRenderAckEvent);
				continue;
			}

			// Robust shader-state handling outside specific commands
			// This ensures that INIT, RESET, or direct commands apply the shader as soon as possible.
			LONG pendingEnable = InterlockedExchange(&m_lPendingShaderEnable, -1);
			if (pendingEnable != -1) {
				if (pendingEnable == 1) {
					TCHAR szShaderPath[MAX_PATH];
					GetModuleFileName(NULL, szShaderPath, MAX_PATH);
					PathRemoveFileSpec(szShaderPath);
					PathAppend(szShaderPath, _T("shaders\\crt.hlsl"));

					if (m_DX9Renderer.CreateCRTShader(szShaderPath)) {
						m_DX9Renderer.SetShaderEnabled(TRUE);
						InterlockedExchange(&m_lShaderEnabled, 1);
						ShowOSD(_T("CRT Shader: Enabled"));
						m_dwPerfOSDLastTick = 0;
					} else {
						InterlockedExchange(&m_lShaderEnabled, 0);
						m_DX9Renderer.SetShaderEnabled(FALSE);
						ShowOSD(_T("CRT Shader: Failed to load"));
						m_dwPerfOSDLastTick = 0;
					}
				} else if (pendingEnable == 0) {
					m_DX9Renderer.SetShaderEnabled(FALSE);
					InterlockedExchange(&m_lShaderEnabled, 0);
					ShowOSD(_T("CRT Shader: Disabled"));
					m_dwPerfOSDLastTick = 0;
				}
			}

			if (InterlockedExchange(&m_lPresentPending, 0) == 1) {
				BOOL bPresented = FALSE;
				if (m_bEnable && m_Info.hBitmap && m_Info.pWork && m_Info.pBits) {
					if (m_DX9Renderer.IsInitialized()) {
						int srcWidth = 0;
						int srcHeight = 0;
						int srcPitch = 0;
						BOOL bCopied = FALSE;
						BOOL bUpdated = FALSE;

						::LockVM();
						CRect rect;
						GetClientRect(&rect);
						ReCalc(rect);
						srcWidth = m_Info.nWidth;
						srcHeight = m_Info.nHeight;
						srcPitch = m_Info.nBMPWidth;

						if ((srcWidth > 0) && (srcHeight > 0) && (srcPitch >= srcWidth)) {
							if (!m_pStagingBuffer || (m_nStagingWidth < srcWidth) || (m_nStagingHeight < srcHeight)) {
								int newWidth = (m_nStagingWidth > srcWidth) ? m_nStagingWidth : srcWidth;
								int newHeight = (m_nStagingHeight > srcHeight) ? m_nStagingHeight : srcHeight;
								size_t newSize = (size_t)newWidth * (size_t)newHeight * sizeof(DWORD);
								DWORD *pNew = (DWORD*)malloc(newSize);
								if (pNew) {
									if (m_pStagingBuffer) {
										free(m_pStagingBuffer);
									}
									m_pStagingBuffer = pNew;
									m_nStagingWidth = newWidth;
									m_nStagingHeight = newHeight;
								}
							}

							if (m_pStagingBuffer) {
								for (int y = 0; y < srcHeight; y++) {
									memcpy(m_pStagingBuffer + (y * srcWidth),
										m_Info.pBits + (y * srcPitch),
										srcWidth * sizeof(DWORD));
								}
								bCopied = TRUE;
							}
						}

						FinishFrame();
						::UnlockVM();

						if (bCopied) {
							bUpdated = m_DX9Renderer.UpdateSurface(m_pStagingBuffer, srcWidth, srcHeight, srcWidth);
						}

						if (bUpdated) {
							DrawOSD(NULL);
							m_DX9Renderer.SetOverlayText(m_bShowOSD ? m_szPerfLine : NULL,
								(m_szOSDText[0] && (GetTickCount() <= m_dwOSDUntil)) ? m_szOSDText : NULL);
							if (m_DX9Renderer.PresentFrame(srcWidth, srcHeight, GetScalePercent())) {
								bPresented = TRUE;
							}
						}
					}

					if (!bPresented) {
						m_bUseDX9 = FALSE;
						if (m_DX9Renderer.IsInitialized()) {
							m_DX9Renderer.Cleanup();
						}
						PostMessage(WM_XM6_PRESENT, 0, 0);
					}
				}
			}
		}
	}
}

//---------------------------------------------------------------------------
//
// Asynchronous frame presentation (UI thread)
//
//---------------------------------------------------------------------------
LRESULT CDrawView::OnPresentFrame(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	::InterlockedExchange(&m_lPresentPending, 0);

	if (m_bEnable && m_Info.hBitmap && m_Info.pWork && m_Info.pBits) {
		CClientDC dc(this);
		::LockVM();
		OnDraw(&dc);
		::UnlockVM();
	}

	return 0;
}

//---------------------------------------------------------------------------
//
// Mark frame as consumed
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::FinishFrame()
{
	if (!m_Info.pWork) {
		return;
	}

	for (int i = 0; i < (m_Info.nHeight * 64); i++) {
		m_Info.pWork->drawflag[i] = FALSE;
	}
	m_Info.dwDrawCount++;
	m_Info.nBltLeft = 0;
	m_Info.nBltTop = 0;
	m_Info.nBltRight = m_Info.nWidth - 1;
	m_Info.nBltBottom = m_Info.nHeight - 1;
	m_Info.bBltAll = FALSE;
}

//---------------------------------------------------------------------------
//
// Simple OSD
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::DrawOSD(CDC *pDC)
{
	if (!m_pScheduler && m_pFrmWnd) {
		m_pScheduler = m_pFrmWnd->GetScheduler();
	}

	DWORD now = GetTickCount();
	if ((m_dwPerfOSDLastTick == 0) || ((now - m_dwPerfOSDLastTick) >= 1500)) {
		m_dwPerfOSDLastTick = now;
		m_nPerfFPS = (m_pScheduler) ? m_pScheduler->GetFrameRate() : 0;
		if (m_nPerfFPS < 0) {
			m_nPerfFPS = 0;
		}

		BOOL bVSync = (m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : FALSE;
		_stprintf(m_szPerfLine, _T("%s | VSync:%s | FPS:%d.%d"),
			m_bUseDX9 ? _T("DirectX 9") : _T("GDI"),
			bVSync ? _T("ON") : _T("OFF"),
			m_nPerfFPS / 10,
			m_nPerfFPS % 10);
	}

	if (!pDC) {
		return;
	}

	if (!m_bShowOSD && !m_szOSDText[0]) {
		return;
	}

	BOOL bShowMsg = (m_szOSDText[0] && (now <= m_dwOSDUntil));
	CRect rect(8, 8, 320, bShowMsg ? 46 : 28);

	pDC->FillSolidRect(&rect, RGB(0, 0, 0));
	int oldBk = pDC->SetBkMode(TRANSPARENT);
	COLORREF oldColor = pDC->SetTextColor(RGB(255, 255, 255));

	CRect lineRect = rect;
	lineRect.left += 4;
	lineRect.top += 2;
	lineRect.bottom = lineRect.top + 16;
	pDC->DrawText(m_szPerfLine, -1, &lineRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

	if (bShowMsg) {
		CRect msgRect = rect;
		msgRect.left += 4;
		msgRect.top = lineRect.bottom + 2;
		msgRect.bottom = msgRect.top + 16;
		pDC->SetTextColor(RGB(255, 220, 96));
		pDC->DrawText(m_szOSDText, -1, &msgRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	}

	pDC->SetTextColor(oldColor);
	pDC->SetBkMode(oldBk);
}

//---------------------------------------------------------------------------
//
// Show OSD
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ShowOSD(LPCTSTR lpszText)
{
	if (!lpszText) {
		m_szOSDText[0] = _T('\0');
		m_dwOSDUntil = 0;
		return;
	}
	_tcsncpy(m_szOSDText, lpszText, (sizeof(m_szOSDText) / sizeof(m_szOSDText[0])) - 1);
	m_szOSDText[(sizeof(m_szOSDText) / sizeof(m_szOSDText[0])) - 1] = _T('\0');
	m_dwOSDUntil = GetTickCount() + 2500;
}

//---------------------------------------------------------------------------
//
// Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ApplyCfg(const Config *pConfig)
{
	CSubWnd *pWnd;

	ASSERT(pConfig);

	// Window scale
	SetScaleIndex(pConfig->window_scale);

	// Shader (CRT)
	SetShaderEnabled(pConfig->render_shader);

	// Subwindow handling
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->ApplyCfg(pConfig);
		pWnd = pWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
// Get drawing information
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::GetDrawInfo(LPDRAWINFO pDrawInfo) const
{
	ASSERT(this);
	ASSERT(pDrawInfo);

	// Copy internal state
	*pDrawInfo = m_Info;
}

//---------------------------------------------------------------------------
//
// Draw
//
//---------------------------------------------------------------------------
void CDrawView::OnDraw(CDC *pDC)
{
	CRect rect;
	HDC hMemDC;
	HBITMAP hDefBitmap;
	int i;
	int vmul;
	int hmul;

	// Fill if the bitmap is not ready
	GetClientRect(&rect);
	if (!m_Info.hBitmap || !m_bEnable || !m_Info.pWork) {
		pDC->FillSolidRect(&rect, RGB(0, 0, 0));
		DrawOSD(pDC);
		return;
	}

	// Recalculate
	ReCalc(rect);

	// Disconnect handling
	if (::GetVM()->IsPower() != m_Info.bPower) {
		m_Info.bPower = ::GetVM()->IsPower();
		if (!m_Info.bPower) {
			// Clear all bitmaps
			memset(m_Info.pBits, 0, m_Info.nBMPWidth * m_Info.nBMPHeight * 4);
			m_Info.bBltAll = TRUE;
		}
	}

	// Draw a corner
	if (m_Info.bBltAll) {
		DrawRect(pDC);
	}

	// Set final screen scaling
	hmul = 1;
	if (m_Info.nRendHMul == 2) {
		// 256-resolution cases, etc.
		hmul = 2;
	}


	/* Horizontal stretch is set here */
	if(m_Info.bBltStretch){	// If stretching is required
		// Stretch to cover the client area instead of stopping at the last fit.
		int multiplierCount = 4;


		/* Test code for calculating the minimum stretch that reaches the host area */
		int myHmul = hmul;
		int calculatedWidth = 0;
		while (calculatedWidth < rect.Width())
		{
			multiplierCount++;
			myHmul = hmul * multiplierCount;
			calculatedWidth = (m_Info.nWidth * myHmul) >> 2;
		}

		hmul *= multiplierCount;

	}
	else {
		// Different aspect ratio. Keep the same scaling
		hmul <<= 2;
	}

	/* Vertical stretch is set here */
	vmul = 4;


	/* Test code for calculating maximum possible vertical stretch relative to the host */
	if (m_Info.bBltStretch)
	{
			vmul = 1;
			int myVmul = vmul;
			int calculatedHeight = 0;
			while(calculatedHeight<rect.Height())// Increase the multipliers until the screen resolution is reached
			{
				myVmul++;
				calculatedHeight = (m_Info.nRendHeight * myVmul) >> 2;
			}
			vmul = myVmul;

		}

		if(m_Info.nWidth==512&&m_Info.nHeight==480)// Special case: Code Zero
		{
			if  (m_Info.nRendHeight < m_Info.nHeight)
				vmul = vmul >> 1;
			// hmul++;
		}

		if(m_Info.nWidth==704&&m_Info.nHeight==480)// Special case: Carat
		{
			 hmul++;
			// vmul++;
		}

		/*
		CString sv;
		sv.Format(_T("nWidth: %d nHeight:%d   nRendwidth: %d nRendheight:%d  vmul: %d  rect.height:%d \r\n"), m_Info.nWidth, m_Info.nHeight, m_Info.nRendWidth, m_Info.nRendHeight, vmul, rect.Height());
		OutputDebugStringW(CT2W(sv));
        */



	// If bBltAll is set, process the entire area
	if (m_Info.bBltAll) {
		// Create and select a memory DC
		hMemDC = CreateCompatibleDC(pDC->m_hDC);
		if (!hMemDC) {
			return;
		}
		hDefBitmap = (HBITMAP)SelectObject(hMemDC, m_Info.hBitmap);
		if (!hDefBitmap) {
			DeleteDC(hMemDC);
			return;
		}

		// BitBlt
		if ((hmul == 4) && (vmul == 4)) {
			::BitBlt(pDC->m_hDC,
				m_Info.nLeft, m_Info.nTop,
				m_Info.nWidth, m_Info.nHeight,
				hMemDC, 0, 0,
				SRCCOPY);
		}
		else {
			if (m_Info.bBltStretch) {
				::StretchBlt(pDC->m_hDC,
					0, 0,
					rect.Width(), rect.Height(),
					hMemDC, 0, 0,
					m_Info.nWidth, m_Info.nHeight,
					SRCCOPY);
			}
			else {
				::StretchBlt(pDC->m_hDC,
					m_Info.nLeft, m_Info.nTop,
					(m_Info.nWidth * hmul) >> 2,
					(m_Info.nHeight * vmul) >> 2,
					hMemDC, 0, 0,
					m_Info.nWidth, m_Info.nHeight,
					SRCCOPY);
			}
		}
		::GdiFlush();
		m_Info.bBltAll = FALSE;

		// Switch back to the bitmap
		SelectObject(hMemDC, hDefBitmap);
		DeleteDC(hMemDC);

		// Don't forget to clear the draw flag!
		for (i=0; i<m_Info.nHeight * 64; i++) {
			m_Info.pWork->drawflag[i] = FALSE;
		}
		m_Info.dwDrawCount++;
		m_Info.nBltLeft = 0;
		m_Info.nBltTop = 0;
		m_Info.nBltRight = m_Info.nWidth - 1;
		m_Info.nBltBottom = m_Info.nHeight - 1;
		DrawOSD(pDC);
		return;
	}

	// Check the draw area
	if (!CalcRect()) {
		return;
	}
	ASSERT(m_Info.nBltTop <= m_Info.nBltBottom);
	ASSERT(m_Info.nBltLeft <= m_Info.nBltRight);

	// Create and select a memory DC
	hMemDC = CreateCompatibleDC(pDC->m_hDC);
	if (!hMemDC) {
		m_Info.bBltAll = TRUE;
		return;
	}

	hDefBitmap = (HBITMAP)SelectObject(hMemDC, m_Info.hBitmap);
	if (!hDefBitmap) {
		DeleteDC(hMemDC);
		m_Info.bBltAll = TRUE;
		return;
	}

	// Draw only selected areas
	if ((hmul == 4) && (vmul == 4)) {
		::BitBlt(pDC->m_hDC,
			m_Info.nLeft + m_Info.nBltLeft,
			m_Info.nTop + m_Info.nBltTop,
			m_Info.nBltRight - m_Info.nBltLeft + 1,
			m_Info.nBltBottom - m_Info.nBltTop + 1,
			hMemDC,
			m_Info.nBltLeft,
			m_Info.nBltTop,
			SRCCOPY);
	}
	else {
		if (m_Info.bBltStretch) {
			const int destX = MulDiv(m_Info.nBltLeft, rect.Width(), m_Info.nWidth);
			const int destY = MulDiv(m_Info.nBltTop, rect.Height(), m_Info.nHeight);
			const int destW = MulDiv(m_Info.nBltRight - m_Info.nBltLeft + 1, rect.Width(), m_Info.nWidth);
			const int destH = MulDiv(m_Info.nBltBottom - m_Info.nBltTop + 1, rect.Height(), m_Info.nHeight);

			::StretchBlt(pDC->m_hDC,
				m_Info.nLeft + destX,
				m_Info.nTop + destY,
				destW,
				destH,
				hMemDC,
				m_Info.nBltLeft,
				m_Info.nBltTop,
				m_Info.nBltRight - m_Info.nBltLeft + 1,
				m_Info.nBltBottom - m_Info.nBltTop + 1,
				SRCCOPY);
		}
		else {
			::StretchBlt(pDC->m_hDC,
				m_Info.nLeft + ((m_Info.nBltLeft * hmul) >> 2),
				m_Info.nTop + ((m_Info.nBltTop * vmul) >> 2),
				((m_Info.nBltRight - m_Info.nBltLeft + 1) * hmul) >> 2,
				((m_Info.nBltBottom - m_Info.nBltTop + 1) * vmul) >> 2,
				hMemDC,
				m_Info.nBltLeft,
				m_Info.nBltTop,
				m_Info.nBltRight - m_Info.nBltLeft + 1,
				m_Info.nBltBottom - m_Info.nBltTop + 1,
				SRCCOPY);
		}
	}
	::GdiFlush();

	// Switch back to the bitmap
	SelectObject(hMemDC, hDefBitmap);
	DeleteDC(hMemDC);

	// CalcRect clears the flag
	m_Info.dwDrawCount++;
	DrawOSD(pDC);
}

//---------------------------------------------------------------------------
//
// Recalculate
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ReCalc(CRect& rect)
{
	int width;
	int height;
	BOOL flag;

	// Renderer work; return if no bitmap is available
	if (!m_Info.pWork || !m_Info.hBitmap) {
		return;
	}

	// Compare
	flag = FALSE;
	if (m_Info.nRendWidth != m_Info.pWork->width) {
		m_Info.nRendWidth = m_Info.pWork->width;
		flag = TRUE;
	}
	if (m_Info.nRendHeight != m_Info.pWork->height) {
		m_Info.nRendHeight = m_Info.pWork->height;
		flag = TRUE;
	}
	if (m_Info.nRendHMul != m_Info.pWork->h_mul) {
		m_Info.nRendHMul = m_Info.pWork->h_mul;
		flag = TRUE;
	}
	if (m_Info.nRendVMul != m_Info.pWork->v_mul) {
		m_Info.nRendVMul = m_Info.pWork->v_mul;
		flag = TRUE;
	}
	if (!flag) {
		return;
	}

	// Renderer: use the smaller of the two bitmaps
	m_Info.nWidth = m_Info.nRendWidth;
	if (m_Info.nBMPWidth < m_Info.nWidth) {
		m_Info.nWidth = m_Info.nBMPWidth;
	}
	m_Info.nHeight = m_Info.nRendHeight;
	if (m_Info.nRendVMul == 0) {
		// 15 kHz interlacing handling
		m_Info.nHeight <<= 1;
	}
	if (m_Info.nBMPHeight < m_Info.nRendHeight) {
		m_Info.nHeight = m_Info.nBMPHeight;
	}

	// Centering and margins computed with scaling in mind
	width = m_Info.nWidth * m_Info.nRendHMul;
	if((m_Info.nRendWidth<768)&&m_Info.bBltStretch){	// If stretching is needed
		width=(width*5)>>2;// 5:4 aspect ratio
	}
	height = m_Info.nHeight;
	if (m_Info.nRendVMul == 2) {
		height <<= 1;
	}

	int cx = 0, cy = 0;
	int borderWidth = 0, borderHeight = 0;

	if (m_pFrmWnd->m_bFullScreen)
	{
		cx = ::GetSystemMetrics(SM_CXSCREEN);
		cy = ::GetSystemMetrics(SM_CYSCREEN);
	}

	if (m_Info.nRendWidth > 256)
	{
		if (cx > m_Info.nRendWidth)
			borderWidth = cx % m_Info.nRendWidth;
	}
	if (m_Info.nRendHeight < 240)
	{
		if (cy > m_Info.nRendHeight)
			borderHeight = cy % m_Info.nRendHeight;
	}


	//CString sz, sz2, sz3, sz4;
	//sz.Format(_T("nRendHmul: %d   nRendVMul: %d \r\n"),  m_Info.nRendHMul,  m_Info.nRendVMul);
	//sz2.Format(_T("width: %d   height: %d   bordeAncho: %d  bordeAlto: %d  \r\n "),  width,  height, bordeAncho, bordeAlto);
	//sz3.Format(_T("nWidth: %d   nHeight: %d   nRendWidth: %d   nRendHeight: %d \r\n"),  m_Info.nWidth,  m_Info.nHeight, m_Info.nRendWidth,  m_Info.nRendHeight);
	//sz4.Format(_T("rect.Width(): %d   rect.Height(): %d  cx:%d cy:%d\r\n\r\n\r\n"),  rect.Width(),  rect.Height(),cx,cy );
	//OutputDebugStringW(CT2W(sz));
	//OutputDebugStringW(CT2W(sz2));
	//OutputDebugStringW(CT2W(sz3));
	//OutputDebugStringW(CT2W(sz4));


	/* The upper-left corner positions and TOP of the main frame are determined here */

		if (m_Info.bBltStretch)
			m_Info.nLeft = 0;
		else
		    m_Info.nLeft = (rect.Width() - width) >> 1;


		if (m_Info.bBltStretch)
			m_Info.nTop = 0;
		else
			m_Info.nTop = (rect.Height() - height) >> 1;

	// Specify area drawing
	m_Info.bBltAll = TRUE;
}

//---------------------------------------------------------------------------
//
// Window scale
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::SetScaleIndex(int nScaleIndex)
{
	CRect rect;

	ASSERT(this);

	if (nScaleIndex < 0) {
		nScaleIndex = 0;
	}
	if (nScaleIndex > 4) {
		nScaleIndex = 4;
	}

	if (nScaleIndex == m_Info.nScaleIndex) {
		return;
	}

	m_Info.nScaleIndex = nScaleIndex;
	// Window scale always renders stretched to the client area.
	m_Info.bBltStretch = TRUE;
	InterlockedExchange(&m_lScaleIndex, nScaleIndex);

	if (m_Info.pWork) {
		GetClientRect(&rect);
		ReCalc(rect);
	}

	m_Info.bBltAll = TRUE;
	if (m_hWnd) {
		Invalidate(FALSE);
	}
	RequestPresent();
}

int FASTCALL CDrawView::GetScaleIndex() const
{
	return (int)m_lScaleIndex;
}

int FASTCALL CDrawView::GetScalePercent() const
{
	return 100 + (GetScaleIndex() * 50);
}

//---------------------------------------------------------------------------
//
// Draw border rectangles at the corners
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::DrawRect(CDC *pDC)
{
	CRect crect;
	CRect brect;

	ASSERT(m_Info.bBltAll);
	ASSERT(pDC);

	// If all of them are used, none are needed
	if ((m_Info.nLeft == 0) && (m_Info.nTop == 0)) {
		return;
	}

	// Get the client rectangle
	GetClientRect(&crect);


	/* The outer borders of the game window and their colors are set here */
	if (m_Info.nLeft > 0) {
		// Left half
		brect.left = 0;
		brect.top = 0;
		brect.right = m_Info.nLeft;
		brect.bottom = crect.bottom;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));

		// Right half
		brect.right = crect.right;
		brect.left = brect.right - m_Info.nLeft - 1;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));
	}

	if (m_Info.nTop > 0) {
		// Top half
		brect.left = 0;
		brect.top = 0;
		brect.right = crect.right;
		brect.bottom = m_Info.nTop;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));

		// Right half
		brect.bottom = crect.bottom;
		brect.top = brect.bottom - m_Info.nTop - 1;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));
	}
}

//---------------------------------------------------------------------------
//
// Draw-range check
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::CalcRect()
{
	int i;
	int j;
	int left;
	int top;
	int right;
	int bottom;
	BOOL *p;
	BOOL flag;

	// Initialization
	left = 64;
	top = 2048;
	right = -1;
	bottom = -1;
	p = m_Info.pWork->drawflag;

	// Y loop
	for (i=0; i<m_Info.nHeight; i++) {
		flag = FALSE;

		// X loop
		for(j=0; j<64; j++) {
			if (*p) {
				// Clear
				*p = FALSE;

				// These 16 dots need to be drawn
				if (left > j) {
					left = j;
				}
				if (right < j) {
					right = j;
				}
				flag = TRUE;
			}
			p++;
		}

		if (flag) {
			// This line must be drawn
			if (top > i) {
				top = i;
			}
			if (bottom < i) {
				bottom = i;
			}
		}
	}

	// Not needed if Y does not change
	if (bottom < top) {
		return FALSE;
	}

	// Correction (x16)
	left <<= 4;
	right = ((right + 1) << 4) - 1;
	if (right >= m_Info.nWidth) {
		right = m_Info.nWidth - 1;
	}

	// Copy
	m_Info.nBltLeft = left;
	m_Info.nBltTop = top;
	m_Info.nBltRight = right;
	m_Info.nBltBottom = bottom;

	return TRUE;
}

//---------------------------------------------------------------------------
//
// Get the next subwindow index
//
//---------------------------------------------------------------------------
int FASTCALL CDrawView::GetNewSWnd() const
{
	CSubWnd *pWnd;
	int nSubWnd;

	ASSERT(this);
	ASSERT_VALID(this);

	// Is this the first subwindow?
	if (!m_pSubWnd) {
		return 0;
	}

	// Initialization
	nSubWnd = 1;
	pWnd = m_pSubWnd;

	// Loop
	while (pWnd->m_pNextWnd) {
		pWnd = pWnd->m_pNextWnd;
		nSubWnd++;
	}

	// Return index
	return nSubWnd;
}

//---------------------------------------------------------------------------
//
// Add subwindow
// Called from the CSubWnd being added
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::AddSWnd(CSubWnd *pSubWnd)
{
	CSubWnd *pWnd;

	ASSERT(this);
	ASSERT(pSubWnd);
	ASSERT_VALID(this);

	// Is this the first subwindow?
	if (!m_pSubWnd) {
		// This is the first one. Register it
		m_pSubWnd = pSubWnd;
		ASSERT(!pSubWnd->m_pNextWnd);
		return;
	}

	// Find the tail
	pWnd = m_pSubWnd;
	while (pWnd->m_pNextWnd) {
		pWnd = pWnd->m_pNextWnd;
	}

	// Insert after pWnd
	pWnd->m_pNextWnd = pSubWnd;
	ASSERT(!pSubWnd->m_pNextWnd);
}

//---------------------------------------------------------------------------
//
// Delete subwindow
// Called from the CSubWnd being removed
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::DelSWnd(CSubWnd *pSubWnd)
{
	CSubWnd *pWnd;

	// Assert
	ASSERT(pSubWnd);

	// Lock the VM
	::LockVM();

	// Is this the first subwindow?
	if (m_pSubWnd == pSubWnd) {
		// If there is a next one, link it; otherwise NULL
		if (pSubWnd->m_pNextWnd) {
			m_pSubWnd = pSubWnd->m_pNextWnd;
		}
		else {
			m_pSubWnd = NULL;
		}
		::UnlockVM();
		return;
	}

	// Find the subwindow that precedes pSubWnd
	pWnd = m_pSubWnd;
	while (pWnd->m_pNextWnd != pSubWnd) {
		ASSERT(pWnd->m_pNextWnd);
		pWnd = pWnd->m_pNextWnd;
	}

	// Link pSubWnd->m_pNextWnd to pWnd and skip it
	pWnd->m_pNextWnd = pSubWnd->m_pNextWnd;

	// Unlock the VM
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
// Delete all subwindows
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::ClrSWnd()
{
	CSubWnd *pWnd;
    CSubWnd *pNext;

	ASSERT(this);

	// Get first subwindow
	pWnd = GetFirstSWnd();

	// Loop
	while (pWnd) {
		// Get next
		pNext = pWnd->m_pNextWnd;

		// Delete this window
		pWnd->DestroyWindow();

		// Advance
		pWnd = pNext;
	}
}

//---------------------------------------------------------------------------
//
// Get first subwindow
// If none exists, return NULL
//
//---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::GetFirstSWnd() const
{
	return m_pSubWnd;
}

//---------------------------------------------------------------------------
//
// Search subwindow
// Return NULL if not found
//
//---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::SearchSWnd(DWORD dwID) const
{
	CSubWnd *pWnd;

	// Initialize window
	pWnd = m_pSubWnd;

	// Search loop
	while (pWnd) {
		// Check whether the ID matches
		if (pWnd->GetID() == dwID) {
			return pWnd;
		}

		// Next
		pWnd = pWnd->m_pNextWnd;
	}

	// Not found
	return NULL;
}

//---------------------------------------------------------------------------
//
// Get text font
//
//---------------------------------------------------------------------------
CFont* FASTCALL CDrawView::GetTextFont()
{
	ASSERT(m_TextFont.m_hObject);

	return &m_TextFont;
}

//---------------------------------------------------------------------------
//
// Create new window
//
//---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::NewWindow(BOOL bDis)
{
	DWORD dwID;
	int i;
	CSubWnd *pWnd;
	CDisasmWnd *pDisWnd;
	CMemoryWnd *pMemWnd;

	// Create base ID
	if (bDis) {
		dwID = MAKEID('D', 'I', 'S', 'A');
	}
	else {
		dwID = MAKEID('M', 'E', 'M', 'A');
	}

	// Loop for eight iterations
	for (i=0; i<8; i++) {

		pWnd = SearchSWnd(dwID);
		if (!pWnd) {
			if (bDis) {
				pDisWnd = new CDisasmWnd(i);
				VERIFY(pDisWnd->Init(this));
				return pDisWnd;
			}
			else {
				pMemWnd = new CMemoryWnd(i);
				VERIFY(pMemWnd->Init(this));
				return pMemWnd;
			}
		}

		// Create next window ID
		dwID++;
	}

	// Could not create
	return NULL;
}

//---------------------------------------------------------------------------
//
// Check whether a new window can be created
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsNewWindow(BOOL bDis)
{
	DWORD dwID;
	int i;
	CSubWnd *pWnd;

	// Create base ID
	if (bDis) {
		dwID = MAKEID('D', 'I', 'S', 'A');
	}
	else {
		dwID = MAKEID('M', 'E', 'M', 'A');
	}

	// Loop for eight iterations
	for (i=0; i<8; i++) {
		// If no window is found, a new one can be created
		pWnd = SearchSWnd(dwID);
		if (!pWnd) {
			return TRUE;
		}

		// Create next window ID
		dwID++;
	}

	// All windows already exist -> cannot create
	return FALSE;
}

//---------------------------------------------------------------------------
//
// Get number of subwindows
// Return NULL if not found
//
//---------------------------------------------------------------------------
int FASTCALL CDrawView::GetSubWndNum() const
{
	CSubWnd *pWnd;
	int num;

	// Initialization
	pWnd = m_pSubWnd;
	num = 0;

	// Loop
	while (pWnd) {
		// Count++
		num++;

		// Next
		pWnd = pWnd->m_pNextWnd;
	}

	return num;
}

//---------------------------------------------------------------------------
//
// Get window class name
//
//---------------------------------------------------------------------------
LPCTSTR FASTCALL CDrawView::GetWndClassName() const
{
	ASSERT(this);
	ASSERT(m_pFrmWnd);
	return m_pFrmWnd->GetWndClassName();
}

#endif	// _WIN32
