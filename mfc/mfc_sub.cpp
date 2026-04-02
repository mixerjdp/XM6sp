//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC sub window ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "render.h"
#include "mfc_frm.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_sub.h"

//===========================================================================
//
//	Sub window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubWnd::CSubWnd()
{
	// Object
	m_pSch = NULL;
	m_pDrawView = NULL;
	m_pNextWnd = NULL;

	// Property
	m_strCaption.Empty();
	m_bEnable = TRUE;
	m_dwID = 0;
	m_bPopup = FALSE;

	// Window size
	m_nWidth = -1;
	m_nHeight = -1;

	// Text font
	m_pTextFont = NULL;
	m_tmWidth = -1;
	m_tmHeight = -1;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_ERASEBKGND()
	ON_WM_ACTIVATE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Init(CDrawView *pDrawView)
{
	BOOL bRet;
	CFrmWnd *pFrmWnd;

	ASSERT(this);
	ASSERT(pDrawView);
	ASSERT(m_dwID != 0);

	// Draw view member
	ASSERT(!m_pDrawView);
	m_pDrawView = pDrawView;
	ASSERT(m_pDrawView);

	// Get frame window
	pFrmWnd = (CFrmWnd*)AfxGetApp()->m_pMainWnd;
	ASSERT(pFrmWnd);

	// Get scheduler
	ASSERT(!m_pSch);
	m_pSch = pFrmWnd->GetScheduler();
	ASSERT(m_pSch);

	// Window creation
	if (pFrmWnd->IsPopupSWnd()) {
		// Popup window
		m_bPopup = TRUE;
		bRet = CreateEx(0,
					pDrawView->GetWndClassName(),
					m_strCaption,
					WS_POPUP | WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION |
					WS_VISIBLE | WS_MINIMIZEBOX | WS_BORDER,
					0, 0,
					400, 400,
					pDrawView->m_hWnd,
					(HMENU)0,
					0);
	}
	else {
		// Child window
		m_bPopup = FALSE;
		bRet = Create(NULL,
					m_strCaption,
					WS_CHILD | WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION |
					WS_VISIBLE | WS_MINIMIZEBOX | WS_CLIPSIBLINGS,
					CRect(0, 0, 400, 400),
					pDrawView,
					(UINT)m_dwID);
	}

	// If successful
	if (bRet) {
		// Register with main window
		m_pDrawView->AddSWnd(this);
	}

	return bRet;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	int nSWnd;
	CRect rectWnd;
	CRect rectParent;
	CPoint point;

	ASSERT(this);
	ASSERT(lpCreateStruct);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);

	// Base class
	if (CWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Icon configuration
	SetIcon(AfxGetApp()->LoadIcon(IDI_XICON), TRUE);

	// IME Off
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Text font configuration
	m_pTextFont = m_pDrawView->GetTextFont();
	SetupTextFont();

	// Calculate window fit size
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = m_nWidth * m_tmWidth;
	rectWnd.bottom = m_nHeight* m_tmHeight;
	CalcWindowRect(&rectWnd);

	// Get index (expected)
	nSWnd = m_pDrawView->GetNewSWnd();

	// Window position determined from index
	m_pDrawView->GetWindowRect(&rectParent);
	point.x = (nSWnd * 24) % (rectParent.Width() - 24);
	point.y = (nSWnd * 24) % (rectParent.Height() - 24);

	if (m_bPopup) {
		// For popup window types, screen coordinates are the reference
		point.x += rectParent.left;
		point.y += rectParent.top;
	}

	// Set window position and size
	SetWindowPos(&wndTop, point.x, point.y,
		rectWnd.right, rectWnd.bottom, 0);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CSubWnd::OnDestroy()
{
	ASSERT(this);

	// Disable
	Enable(FALSE);

	// Unregister from draw view
	m_pDrawView->DelSWnd(this);

	// Base class
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CSubWnd::PostNcDestroy()
{
	// Delete interface object
	ASSERT(this);
	delete this;
}

//---------------------------------------------------------------------------
//
//	Background draw
//
//---------------------------------------------------------------------------
BOOL CSubWnd::OnEraseBkgnd(CDC *pDC)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// If enabled, suppress
	if (m_bEnable) {
		return TRUE;
	}

	// Base class
	return CWnd::OnEraseBkgnd(pDC);
}

//---------------------------------------------------------------------------
//
//	Activate
//
//---------------------------------------------------------------------------
void CSubWnd::OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Popup window only
	if (m_bPopup) {
		if (nState == WA_INACTIVE) {
			// If popup window loses focus, suspend
			m_pSch->Activate(FALSE);
		}
		else {
			// If popup window gains focus, resume
			m_pSch->Activate(TRUE);
		}
	}

	// Base class
	CWnd::OnActivate(nState, pWnd, bMinimized);
}

//---------------------------------------------------------------------------
//
//	Enable control
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Enable(BOOL bEnable)
{
	ASSERT(this);

	m_bEnable = bEnable;
}

//---------------------------------------------------------------------------
//
//	Get window ID
//
//---------------------------------------------------------------------------
DWORD FASTCALL CSubWnd::GetID() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	return m_dwID;
}

//---------------------------------------------------------------------------
//
//	Text font setup
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::SetupTextFont()
{
	CClientDC dc(this);
	CFont *pFont;
	TEXTMETRIC tm;

	ASSERT(this);

	// Font selection
	ASSERT(m_pTextFont);
	pFont = dc.SelectObject(m_pTextFont);
	ASSERT(pFont);

	// Get text metrics
	VERIFY(dc.GetTextMetrics(&tm));

	// Deselect font
	VERIFY(dc.SelectObject(pFont));

	// Store text width, height
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;
}

//---------------------------------------------------------------------------
//
//	Message thread update
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::Update()
{
	ASSERT(this);
	ASSERT_VALID(this);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Save(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CSubWnd::Load(Fileio* /*pFio*/, int /*nVer*/)
{
	ASSERT(this);
	ASSERT_VALID(this);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CSubWnd::ApplyCfg(const Config* /*pConfig*/)
{
	ASSERT(this);
	ASSERT_VALID(this);
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
//
//---------------------------------------------------------------------------
void CSubWnd::AssertValid() const
{
	ASSERT(this);

	// Member objects
	ASSERT(m_pSch);
	ASSERT(m_pSch->GetID() == MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pDrawView);
	ASSERT(m_pDrawView->m_hWnd);
	ASSERT(::IsWindow(m_pDrawView->m_hWnd));

	// Properties
	ASSERT(m_strCaption.GetLength() > 0);
	ASSERT(m_dwID != 0);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);
	ASSERT(m_pTextFont);
	ASSERT(m_pTextFont->m_hObject);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);
}
#endif	// NDEBUG

//===========================================================================
//
//	Sub text window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubTextWnd::CSubTextWnd()
{
	// Member variable initialization
	m_bReverse = FALSE;
	m_pTextBuf = NULL;
	m_pDrawBuf = NULL;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubTextWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_PAINT()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubTextWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	ASSERT(this);
	ASSERT(!m_pTextBuf);
	ASSERT(m_nWidth > 0);
	ASSERT(m_nHeight > 0);

	// Text buffer allocation
	m_pTextBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pTextBuf);

	// Draw buffer allocation, initialization
	m_pDrawBuf = new BYTE[m_nWidth * m_nHeight];
	ASSERT(m_pDrawBuf);
	memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

	// Base class (delegates to DrawView)
	if (CSubWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnDestroy()
{
	ASSERT(this);

	// Base class (also performs notification to DrawView)
	CSubWnd::OnDestroy();

	// Text buffer delete
	ASSERT(m_pTextBuf);
	if (m_pTextBuf) {
		delete[] m_pTextBuf;
		m_pTextBuf = NULL;
	}

	// Draw buffer delete
	ASSERT(m_pDrawBuf);
	if (m_pDrawBuf) {
		delete[] m_pDrawBuf;
		m_pDrawBuf = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Draw
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// VM lock
	::LockVM();

    BeginPaint(&ps);

	// Text buffer, enable flag check
	if (m_pTextBuf && m_bEnable) {
		// Clear draw buffer with FF
		memset(m_pDrawBuf, 0xff, m_nWidth * m_nHeight);

		// Draw processing (VM thread calls Refresh to delegate)
	}

	EndPaint(&ps);

	// VM unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Size change
//
//---------------------------------------------------------------------------
void CSubTextWnd::OnSize(UINT nType, int cx, int cy)
{
	int nWidth;
	int nHeight;
	CRect rectClient;

	ASSERT(this);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);

	// Calculate client size from character metrics
	GetClientRect(&rectClient);
	nWidth = rectClient.Width() / m_tmWidth;
	nHeight = rectClient.Height() / m_tmHeight;

	// If not matching
	if ((nWidth != m_nWidth) || (nHeight != m_nHeight)) {
		// If not minimized
		if (nType != SIZE_MINIMIZED) {
			// Text buffer reallocation
			if (m_pTextBuf && m_pDrawBuf) {
				// If odd, +1
				if ((nWidth * m_tmWidth) < rectClient.Width()) {
					nWidth++;
				}
				if ((nHeight * m_tmWidth) < rectClient.Height()) {
					nHeight++;
				}

				// Buffer change (VM lock required)
				::LockVM();
				ASSERT(m_pTextBuf);
				if (m_pTextBuf) {
					delete[] m_pTextBuf;
					m_pTextBuf = NULL;
				}
				ASSERT(m_pDrawBuf);
				if (m_pDrawBuf) {
					delete[] m_pDrawBuf;
					m_pDrawBuf = NULL;
				}
				m_pTextBuf = new BYTE[nWidth * nHeight];
				m_pDrawBuf = new BYTE[nWidth * nHeight];
				m_nWidth = nWidth;
				m_nHeight = nHeight;
				::UnlockVM();
			}
		}
	}

	// Redraw
	Invalidate(FALSE);

	// Base class
	CSubWnd::OnSize(nType, cx, cy);
}

//---------------------------------------------------------------------------
//
//	Refresh display
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Refresh()
{
	CClientDC dc(this);

	ASSERT(this);

	// Text buffer, enable flag check
	if (m_pTextBuf && m_bEnable) {
		// Setup
		Setup();

		// Draw
		OnDraw(&dc);
	}
}

//---------------------------------------------------------------------------
//
//	Draw callback
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::OnDraw(CDC *pDC)
{
	CFont *pFont;
	BOOL bReverse;
	BYTE *pText;
	BYTE *pDraw;
	CPoint point;
	int x;
	int y;
	BYTE ch;

	ASSERT(this);
	ASSERT(pDC);
	ASSERT(m_pTextBuf);
	ASSERT(m_pDrawBuf);
	ASSERT(m_pTextFont);
	ASSERT(m_nWidth >= 0);
	ASSERT(m_nHeight >= 0);

	// Font selection
	pFont = pDC->SelectObject(m_pTextFont);
	ASSERT(pFont);

	// Color selection
	pDC->SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
	pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
	bReverse = FALSE;

	// Pointer, coordinate initialization
	pText = m_pTextBuf;
	pDraw = m_pDrawBuf;
	point.y = 0;

	// y loop
	for (y=0; y<m_nHeight; y++) {
		point.x = 0;
		for (x=0; x<m_nWidth; x++) {
			// Match check
			if (*pText == *pDraw) {
				pText++;
				pDraw++;
				point.x += m_tmWidth;
				continue;
			}

			// Copy
			ch = *pText++;
			*pDraw++ = ch;

			// Reverse check
			if (ch >= 0x80) {
				ch &= 0x7f;
				if (!bReverse) {
					pDC->SetTextColor(::GetSysColor(COLOR_WINDOW));
					pDC->SetBkColor(::GetSysColor(COLOR_WINDOWTEXT));
					bReverse = TRUE;
				}
			}
			else {
				if (bReverse) {
					pDC->SetTextColor(::GetSysColor(COLOR_WINDOWTEXT));
					pDC->SetBkColor(::GetSysColor(COLOR_WINDOW));
					bReverse = FALSE;
				}
			}

			// Text out
			pDC->TextOut(point.x, point.y, (LPCTSTR)&ch ,1);
			point.x += m_tmWidth;
		}
		point.y += m_tmHeight;
	}

	// Font selection restore
	VERIFY(pDC->SelectObject(pFont));
}

//---------------------------------------------------------------------------
//
//	Clear
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Clear()
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(m_nWidth >= 0);
	ASSERT(m_nHeight >= 0);

	// Clear
	if (m_pTextBuf) {
		memset(m_pTextBuf, 0x20, m_nWidth * m_nHeight);
	}

	// Reverse off
	Reverse(FALSE);
}

//---------------------------------------------------------------------------
//
//	Set character
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetChr(int x, int y, TCHAR ch)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(_istprint(ch));

	// Valid range check
	if ((x < m_nWidth) && (y < m_nHeight)) {
		// Set character
		if (m_bReverse) {
			m_pTextBuf[(y * m_nWidth) + x] = (BYTE)(ch | 0x80);
		}
		else {
			m_pTextBuf[(y * m_nWidth) + x] = ch;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Set string
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::SetString(int x, int y, LPCTSTR lpszText)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(lpszText);

	// Loop until '\0'
	while (_istprint(*lpszText)) {
		SetChr(x, y, *lpszText);

		lpszText++;
		x++;
	}
}

//---------------------------------------------------------------------------
//
//	Reverse
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::Reverse(BOOL bReverse)
{
	ASSERT(this);
	ASSERT(m_pTextBuf);

	m_bReverse = bReverse;
}

//---------------------------------------------------------------------------
//
//	Resize
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextWnd::ReSize(int nWidth, int nHeight)
{
	CRect rectWnd;
	CRect rectNow;

	ASSERT(this);
	ASSERT(nWidth > 0);
	ASSERT(nHeight > 0);

	// Match check
	if ((nWidth == m_nWidth) && (nHeight == m_nHeight)) {
		return;
	}

	// If no buffer, do nothing
	if (!m_pTextBuf || !m_pDrawBuf) {
		return;
	}

	// Calculate size
	rectWnd.left = 0;
	rectWnd.top = 0;
	rectWnd.right = nWidth * m_tmWidth;
	rectWnd.bottom = nHeight * m_tmHeight;
	CalcWindowRect(&rectWnd);

	// If same, do nothing
	GetWindowRect(&rectNow);
	if ((rectNow.Width() == rectWnd.Width()) && (rectNow.Height() == rectWnd.Height())) {
		return;
	}

	// Size change
	SetWindowPos(&wndTop, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_NOMOVE);
}

//===========================================================================
//
//	Sub text size window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubTextSizeWnd::CSubTextSizeWnd()
{
}

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CSubTextSizeWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// Base class
	if (!CSubTextWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Size, resizable, maximize box
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Draw callback
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextSizeWnd::OnDraw(CDC *pDC)
{
	CRect rect;

	ASSERT(this);
	ASSERT(pDC);

	// Get client rectangle
	GetClientRect(&rect);

	// Clear right
	rect.left = m_nWidth * m_tmWidth;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// Clear bottom
	rect.left = 0;
	rect.top = m_nHeight * m_tmHeight;
	pDC->FillSolidRect(&rect, GetSysColor(COLOR_WINDOW));

	// Base class
	CSubTextWnd::OnDraw(pDC);
}

//===========================================================================
//
//	Sub text scroll window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubTextScrlWnd::CSubTextScrlWnd()
{
	// Member variable initialization
	m_ScrlWidth = -1;
	m_ScrlHeight = -1;
	m_bScrlH = FALSE;
	m_bScrlV = FALSE;
	m_ScrlX = 0;
	m_ScrlY = 0;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubTextScrlWnd, CSubTextSizeWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CSubTextScrlWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// Base class
	if (!CSubTextSizeWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Add scroll bars
	cs.style |= WS_HSCROLL;
	cs.style |= WS_VSCROLL;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubTextScrlWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect wrect;
	CRect crect;

	ASSERT(this);

	// Base class
	if (CSubTextSizeWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// Scroll setup
	ShowScrollBar(SB_BOTH, FALSE);
	SetupScrl();

	// Size adjustment
	GetWindowRect(&wrect);
	GetClientRect(&crect);
	wrect.right -= wrect.left;
	wrect.right -= crect.right;
	wrect.right += m_ScrlWidth * m_tmWidth;
	SetWindowPos(&wndTop, 0, 0, wrect.right, wrect.Height(),
						SWP_NOMOVE | SWP_NOZORDER);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Size change
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnSize(UINT nType, int cx, int cy)
{
	ASSERT(this);

	// Base class first
	CSubTextSizeWnd::OnSize(nType, cx, cy);

	// Scroll setup
	SetupScrl();
}

//---------------------------------------------------------------------------
//
//	Scroll setup
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrl()
{
	CRect rect;
	int width;
	int height;

	ASSERT(this);
	ASSERT(m_ScrlWidth >= 0);
	ASSERT(m_ScrlHeight >= 0);
	ASSERT(m_tmWidth > 0);
	ASSERT(m_tmHeight > 0);

	// Get client size
	GetClientRect(&rect);

	// Do nothing if not yet valid
	if ((rect.right == 0) || (rect.bottom == 0)) {
		return;
	}

	// Evaluate H scroll
	width = rect.right / m_tmWidth;
	if (width < m_ScrlWidth) {
		if (!m_bScrlH) {
			m_bScrlH = TRUE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, TRUE);
		}
		// Scroll bar needed, set position
		SetupScrlH();
	}
	else {
		if (m_bScrlH) {
			m_bScrlH = FALSE;
			m_ScrlX = 0;
			ShowScrollBar(SB_HORZ, FALSE);
		}
	}

	// Evaluate V scroll
	height = rect.bottom / m_tmHeight;
	if (height < m_ScrlHeight) {
		if (!m_bScrlV) {
			m_bScrlV = TRUE;
			m_ScrlY = 0;
			ShowScrollBar(SB_VERT, TRUE);
		}
		// Scroll bar needed, set position
		SetupScrlV();
	}
	else {
		if (m_bScrlV) {
			m_bScrlV = FALSE;
			m_ScrlY = 0;
			ShowScrollBar(SB_VERT, FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Scroll setup (horizontal)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlH()
{
	SCROLLINFO si;
	CRect rect;
	int width;

	ASSERT(this);

	// Get scrollable character count
	GetClientRect(&rect);
	width = rect.right / m_tmWidth;

	// Scroll bar setup
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlWidth - 1;
	si.nPage = width;

	// Position, correct if needed
	si.nPos = m_ScrlX;
	if (si.nPos + width > m_ScrlWidth) {
		si.nPos = m_ScrlWidth - width;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlX = si.nPos;
	}

	SetScrollInfo(SB_HORZ, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Scroll (horizontal)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// Get scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_HORZ, &si, SIF_ALL);

	// Scroll bar code processing
	switch (nSBCode) {
		// Left end
		case SB_LEFT:
			m_ScrlX = si.nMin;
			break;

		// Right end
		case SB_RIGHT:
			m_ScrlX = si.nMax;
			break;

		// 1 line left
		case SB_LINELEFT:
			if (m_ScrlX > 0) {
				m_ScrlX--;
			}
			break;

		// 1 line right
		case SB_LINERIGHT:
			if (m_ScrlX < si.nMax) {
				m_ScrlX++;
			}
			break;

		// 1 page left
		case SB_PAGELEFT:
			if (m_ScrlX >= (int)si.nPage) {
				m_ScrlX -= (int)si.nPage;
			}
			else {
				m_ScrlX = 0;
			}
			break;

		// 1 page right
		case SB_PAGERIGHT:
			if ((m_ScrlX + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlX += si.nPage;
			}
			else {
				m_ScrlX = si.nMax;
			}
			break;

		// Thumb move
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_ScrlX = nPos;
			break;
	}

	// Clamp
	if (m_ScrlX < 0) {
		m_ScrlX = 0;
	}

	// Set
	SetupScrlH();
}

//---------------------------------------------------------------------------
//
//	Scroll setup (vertical)
//
//---------------------------------------------------------------------------
void FASTCALL CSubTextScrlWnd::SetupScrlV()
{
	SCROLLINFO si;
	CRect rect;
	int height;

	ASSERT(this);

	// Get scrollable character count
	GetClientRect(&rect);
	height = rect.bottom / m_tmHeight;

	// Scroll bar setup
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlHeight - 1;
	si.nPage = height;

	// Position, correct if needed
	si.nPos = m_ScrlY;
	if (si.nPos + height > m_ScrlHeight) {
		si.nPos = m_ScrlHeight - height;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlY = si.nPos;
	}

	SetScrollInfo(SB_VERT, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Scroll (vertical)
//
//---------------------------------------------------------------------------
void CSubTextScrlWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar * /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);

	// Get scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// Scroll bar code processing
	switch (nSBCode) {
		// Top
		case SB_TOP:
			m_ScrlY = si.nMin;
			break;

		// Bottom
		case SB_BOTTOM:
			m_ScrlY = si.nMax;
			break;

		// 1 line up
		case SB_LINEUP:
			if (m_ScrlY > 0) {
				m_ScrlY--;
			}
			break;

		// 1 line down
		case SB_LINEDOWN:
			if (m_ScrlY < si.nMax) {
				m_ScrlY++;
			}
			break;

		// 1 page up
		case SB_PAGEUP:
			if (m_ScrlY >= (int)si.nPage) {
				m_ScrlY -= si.nPage;
			}
			else {
				m_ScrlY = 0;
			}
			break;

		// 1 page down
		case SB_PAGEDOWN:
			if ((m_ScrlY + (int)si.nPage) <= (int)si.nMax) {
				m_ScrlY += si.nPage;
			}
			else {
				m_ScrlY = si.nMax;
			}
			break;

		// Thumb move
		case SB_THUMBPOSITION:
			m_ScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_ScrlY = nPos;
			break;
	}

	// Clamp
	if (m_ScrlY < 0) {
		m_ScrlY = 0;
	}

	// Set and draw
	SetupScrlV();
}

//===========================================================================
//
//	Sub list window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubListWnd::CSubListWnd()
{
	// List control initialization ends disabled
	m_bEnable = FALSE;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubListWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_WM_DRAWITEM()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CSubListWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// Base class
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Make window resizable
	cs.style |= WS_THICKFRAME;
	cs.style |= WS_MAXIMIZEBOX;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubListWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	CRect rect;
	CDC *pDC;
	TEXTMETRIC tm;

	// Base class
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// Create list control
	VERIFY(m_ListCtrl.Create(WS_CHILD | WS_VISIBLE |
							LVS_REPORT | LVS_NOSORTHEADER| LVS_SINGLESEL |
						 	LVS_OWNERDRAWFIXED | LVS_OWNERDATA,
							CRect(0, 0, 0, 0), this, 0));

	// Font size calculation
	pDC = m_ListCtrl.GetDC();
	pDC->GetTextMetrics(&tm);
	m_ListCtrl.ReleaseDC(pDC);
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;

	// Initialization
	InitList();

	// Enable and refresh
	m_bEnable = TRUE;
	Refresh();

	return 0;
}

//---------------------------------------------------------------------------
//
//	Size change
//
//---------------------------------------------------------------------------
void CSubListWnd::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;

	ASSERT(this);

	// Base class
	CSubWnd::OnSize(nType, cx, cy);

	// Resize list control to fill client area
	if (m_ListCtrl.GetSafeHwnd()) {
		GetClientRect(&rect);
		m_ListCtrl.SetWindowPos(&wndTop, 0, 0, rect.right, rect.bottom, SWP_NOZORDER);
	}
}

//---------------------------------------------------------------------------
//
//	Owner draw
//
//---------------------------------------------------------------------------
void CSubListWnd::OnDrawItem(int /*nID*/, LPDRAWITEMSTRUCT /*lpDIS*/)
{
	// Must be implemented in derived class
	ASSERT(FALSE);
}

//===========================================================================
//
//	Sub BMP window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubBMPWnd::CSubBMPWnd()
{
	// No bitmap
	memset(&m_bmi, 0, sizeof(m_bmi));
	m_bmi.biSize = sizeof(BITMAPINFOHEADER);
	m_pBits = NULL;
	m_hBitmap = NULL;

	// 50% magnification
	m_nMul = 2;

	// Virtual screen size
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;

	// Scroll offset
	m_nScrlX = 0;
	m_nScrlY = 0;

	// Mouse cursor
	m_nCursorX = -1;
	m_nCursorY = -1;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubBMPWnd, CWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_ERASEBKGND()
	ON_WM_PAINT()
	ON_WM_HSCROLL()
	ON_WM_VSCROLL()
	ON_WM_MOUSEMOVE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubBMPWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	ASSERT(this);

	// Base class
	if (CWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// IME Off
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

	// Show available scroll bars
	ShowScrollBar(SB_HORZ, TRUE);
	ShowScrollBar(SB_VERT, TRUE);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnDestroy()
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Bitmap deletion
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		ASSERT(m_pBits);
		m_pBits = NULL;
	}

	// Base class
	CWnd::OnDestroy();
}

//---------------------------------------------------------------------------
//
//	Window destruction complete
//
//---------------------------------------------------------------------------
void CSubBMPWnd::PostNcDestroy()
{
	ASSERT(this);
	ASSERT(!m_hBitmap);
	ASSERT(!m_pBits);

	// Delete interface elements
	delete this;
}

//---------------------------------------------------------------------------
//
//	Size change
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnSize(UINT nType, int cx, int cy)
{
	CClientDC dc(this);

	ASSERT(this);

	// Base class
	CWnd::OnSize(nType, cx, cy);

	// Initialize mouse
	m_nCursorX = -1;
	m_nCursorY = -1;

	// Lock
	::LockVM();

	// If bitmap exists, release it once
	if (m_hBitmap) {
		::DeleteObject(m_hBitmap);
		m_hBitmap = NULL;
		ASSERT(m_pBits);
		m_pBits = NULL;
	}

	// Return if minimized
	if (nType == SIZE_MINIMIZED) {
		::UnlockVM();
		return;
	}

	// Scale cx,cy to scroll area
	cx = (cx * 2) / m_nMul;
	if (cx >= m_nScrlWidth) {
		cx = m_nScrlWidth;
	}
	cy = (cy * 2) / m_nMul;
	if (cy >= m_nScrlHeight) {
		cy = m_nScrlHeight;
	}

	// Create bitmap (32 bits, equal size)
	m_bmi.biWidth = cx;
	m_bmi.biHeight = -cy;
	m_bmi.biPlanes = 1;
	m_bmi.biBitCount = 32;
	m_bmi.biSizeImage = cx * cy * 4;
	m_hBitmap = ::CreateDIBSection(dc.m_hDC, (BITMAPINFO*)&m_bmi,
						DIB_RGB_COLORS, (void**)&m_pBits, NULL, 0);

	// Initialize (fill black)
	if (m_hBitmap) {
		memset(m_pBits, 0, m_bmi.biSizeImage);
	}

	// Scroll bar adjustment
	SetupScrlH();
	SetupScrlV();

	// Unlock
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Background draw
//
//---------------------------------------------------------------------------
BOOL CSubBMPWnd::OnEraseBkgnd(CDC* /*pDC*/)
{
	// Nothing
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Redraw
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnPaint()
{
	PAINTSTRUCT ps;

	// Just stop for Windows
	BeginPaint(&ps);
	EndPaint(&ps);
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Diagnostics
//
//---------------------------------------------------------------------------
void CSubBMPWnd::AssertValid() const
{
	ASSERT(this);
	ASSERT(m_hWnd);
	ASSERT(::IsWindow(m_hWnd));
	ASSERT(m_nMul >= 1);
	ASSERT(m_nScrlWidth > 0);
	ASSERT(m_nScrlHeight > 0);
	ASSERT(m_nScrlX >= 0);
	ASSERT(m_nScrlX < m_nScrlWidth);
	ASSERT(m_nScrlY >= 0);
	ASSERT(m_nScrlY < m_nScrlHeight);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Scroll setup (horizontal)
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::SetupScrlH()
{
	SCROLLINFO si;
	CRect rect;

	ASSERT(this);
	ASSERT_VALID(this);

	// If no bitmap, skip
	if (!m_hBitmap) {
		return;
	}

	// Set scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_nScrlWidth - 1;
	si.nPage = m_bmi.biWidth;

	// Correct position if needed
	si.nPos = m_nScrlX;
	if (si.nPos + (int)si.nPage >= m_nScrlWidth) {
		si.nPos = m_nScrlWidth - (int)si.nPage;
	}
	if (si.nPos < 0) {
		si.nPos = 0;
	}
	m_nScrlX = si.nPos;
	ASSERT((m_nScrlX >= 0) && (m_nScrlX < m_nScrlWidth));

	// Set
	SetScrollInfo(SB_HORZ, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Scroll (horizontal)
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_HORZ, &si, SIF_ALL);

	// By scroll bar code
	switch (nSBCode) {
		// Left
		case SB_LEFT:
			m_nScrlX = si.nMin;
			break;

		// Right
		case SB_RIGHT:
			m_nScrlX = si.nMax;
			break;

		// One line left
		case SB_LINELEFT:
			if (m_nScrlX > 0) {
				m_nScrlX--;
			}
			break;

		// One line right
		case SB_LINERIGHT:
			if (m_nScrlX < si.nMax) {
				m_nScrlX++;
			}
			break;

		// One page left
		case SB_PAGELEFT:
			if (m_nScrlX >= (int)si.nPage) {
				m_nScrlX -= (int)si.nPage;
			}
			else {
				m_nScrlX = 0;
			}
			break;

		// One page right
		case SB_PAGERIGHT:
			if ((m_nScrlX + (int)si.nPage) <= si.nMax) {
				m_nScrlX += (int)si.nPage;
			}
			else {
				m_nScrlX = si.nMax;
			}
			break;

		// Thumb change
		case SB_THUMBPOSITION:
		case SB_THUMBTRACK:
			m_nScrlX = nPos;
			break;
	}
	ASSERT((m_nScrlX >= 0) && (m_nScrlX < m_nScrlWidth));

	// Set
	SetupScrlH();
}

//---------------------------------------------------------------------------
//
//	Scroll setup (vertical)
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::SetupScrlV()
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// If no bitmap, skip
	if (!m_hBitmap) {
		return;
	}

	// Set scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_nScrlHeight - 1;
	si.nPage = -m_bmi.biHeight;

	// Correct position if needed
	si.nPos = m_nScrlY;
	if (si.nPos + (int)si.nPage >= m_nScrlHeight) {
		si.nPos = m_nScrlHeight - (int)si.nPage;
	}
	if (si.nPos < 0) {
		si.nPos = 0;
	}
	m_nScrlY = si.nPos;
	ASSERT((m_nScrlY >= 0) && (m_nScrlY <= m_nScrlHeight));

	SetScrollInfo(SB_VERT, &si, TRUE);
}

//---------------------------------------------------------------------------
//
//	Scroll (vertical)
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar* /*pBar*/)
{
	SCROLLINFO si;

	ASSERT(this);
	ASSERT_VALID(this);

	// Get scroll info
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

	// By scroll bar code
	switch (nSBCode) {
		// Top
		case SB_TOP:
			m_nScrlY = si.nMin;
			break;

		// Bottom
		case SB_BOTTOM:
			m_nScrlY = si.nMax;
			break;

		// One line
		case SB_LINEUP:
			if (m_nScrlY > 0) {
				m_nScrlY--;
			}
			break;

		// One line less
		case SB_LINEDOWN:
			if (m_nScrlY < si.nMax) {
				m_nScrlY++;
			}
			break;

		// One page up
		case SB_PAGEUP:
			if (m_nScrlY >= (int)si.nPage) {
				m_nScrlY -= (int)si.nPage;
			}
			else {
				m_nScrlY = 0;
			}
			break;

		// One page down
		case SB_PAGEDOWN:
			if ((m_nScrlY + (int)si.nPage) <= si.nMax) {
				m_nScrlY += (int)si.nPage;
			}
			else {
				m_nScrlY = si.nMax;
			}
			break;

		// Thumb change
		case SB_THUMBPOSITION:
			m_nScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_nScrlY = nPos;
			break;
	}

	ASSERT((m_nScrlY >= 0) && (m_nScrlY <= m_nScrlHeight));

	// Set
	SetupScrlV();
}

//---------------------------------------------------------------------------
//
//	Mouse move
//
//---------------------------------------------------------------------------
void CSubBMPWnd::OnMouseMove(UINT nFlags, CPoint point)
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Store mouse position
	m_nCursorX = point.x;
	m_nCursorY = point.y;

	// Consider magnification
	m_nCursorX = (m_nCursorX * 2) / m_nMul;
	m_nCursorY = (m_nCursorY * 2) / m_nMul;

	// Base class
	CWnd::OnMouseMove(nFlags, point);
}

//---------------------------------------------------------------------------
//
//	Draw
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::Refresh(int nWidth, int nHeight)
{
	CClientDC dc(this);
	CDC mDC;
	HBITMAP hBitmap;
	CRect Rect;

	ASSERT(this);
	ASSERT_VALID(this);

	// Only if bitmap is available
	if (m_hBitmap) {
		// Create memory DC
		mDC.CreateCompatibleDC(&dc);

		// Object selection
		hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);

		// BitBlt or StretchBlt
		if (hBitmap) {
			if (m_nMul == 2) {
				// Equal size
				int bmibiwidth =  m_bmi.biWidth;
				int bmibiheight = -m_bmi.biHeight;

				// Stretchblt: nHeight and nWidth are exact source bitmap size parameters at source resolution
				dc.StretchBlt(0, 0,
					bmibiwidth,
					bmibiheight,
					&mDC,
					0, 0,
					nHeight, nWidth,
					SRCCOPY);
			}
			else {
				// n times
				dc.StretchBlt(0, 0,
							(m_bmi.biWidth * m_nMul) >> 1,
							-((m_bmi.biHeight * m_nMul) >> 1),
							&mDC,
							0, 0,
							m_bmi.biWidth, -m_bmi.biHeight,
							SRCCOPY);
			}

			// End of object selection
			::SelectObject(mDC.m_hDC, hBitmap);
		}

		// End of memory DC
		mDC.DeleteDC();
	}
}

void FASTCALL CSubBMPWnd::Refresh()
{
	CClientDC dc(this);
	CDC mDC;
	HBITMAP hBitmap;

	ASSERT(this);
	ASSERT_VALID(this);

	// Return if bitmap available
	if (m_hBitmap) {
		// Memory DC creation
		mDC.CreateCompatibleDC(&dc);

		// Object selection
		hBitmap = (HBITMAP)::SelectObject(mDC.m_hDC, m_hBitmap);

		// BitBlt or StretchBlt
		if (hBitmap) {
			if (m_nMul == 2) {
				// Equal
				dc.BitBlt(0, 0, m_bmi.biWidth, -m_bmi.biHeight,
					&mDC, 0, 0, SRCCOPY);
			}
			else {
				// n times
				dc.StretchBlt(0, 0,
					(m_bmi.biWidth * m_nMul) >> 1,
					-((m_bmi.biHeight * m_nMul) >> 1),
					&mDC,
					0, 0,
					m_bmi.biWidth, -m_bmi.biHeight,
					SRCCOPY);
			}

			// Object deselection
			::SelectObject(mDC.m_hDC, hBitmap);
		}

		// Memory DC deselection
		mDC.DeleteDC();
	}
}

//---------------------------------------------------------------------------
//
//	Get maximum window rectangle
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetMaximumRect(LPRECT lpRect, BOOL bScroll)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// Get max BMP window size
	lpRect->left = 0;
	lpRect->top = 0;
	lpRect->right = (m_nScrlWidth * m_nMul) >> 1;
	lpRect->bottom = (m_nScrlHeight * m_nMul) >> 1;

	// Add scroll bars
	if (bScroll) {
		lpRect->right += ::GetSystemMetrics(SM_CXVSCROLL);
		lpRect->bottom += ::GetSystemMetrics(SM_CYHSCROLL);
	}

	// Convert to window rectangle
	CalcWindowRect(lpRect);
}

BOOL FASTCALL CSubBMPWnd::Init(CDrawView* pDrawView)
{
	pDrawView = NULL;
	return 0;
}

//---------------------------------------------------------------------------
//
//	Get fit rectangle
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetFitRect(LPRECT lpRect)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// Get current client rectangle
	GetClientRect(lpRect);

	// Convert to window rectangle
	CalcWindowRect(lpRect);
}

//---------------------------------------------------------------------------
//
//	Get draw rectangle
//
//---------------------------------------------------------------------------
void FASTCALL CSubBMPWnd::GetDrawRect(LPRECT lpRect)
{
	ASSERT(this);
	ASSERT(lpRect);
	ASSERT_VALID(this);

	// If no bitmap, error
	if (!m_hBitmap) {
		ASSERT(!m_pBits);
		lpRect->top = 0;
		lpRect->left = 0;
		lpRect->right = 0;
		lpRect->bottom = 0;
		return;
	}

	// Bitmap available
	ASSERT(m_pBits);

	// Scroll offset
	lpRect->left = m_nScrlX;
	lpRect->top = m_nScrlY;

	// Range adjustment (m_bmi.biHeight is always negative)
	lpRect->right = lpRect->left + m_bmi.biWidth;
	lpRect->bottom = lpRect->top - m_bmi.biHeight;

	// Check
	ASSERT(lpRect->left <= lpRect->right);
	ASSERT(lpRect->top <= lpRect->bottom);
	ASSERT(lpRect->right <= m_nScrlWidth);
	ASSERT(lpRect->bottom <= m_nScrlHeight);
}

//---------------------------------------------------------------------------
//
//	Get bits
//
//---------------------------------------------------------------------------
BYTE* FASTCALL CSubBMPWnd::GetBits() const
{
	ASSERT(this);
	ASSERT_VALID(this);

	// Bitmap available
	if (m_pBits) {
		ASSERT(m_hBitmap);
		return m_pBits;
	}

	// No bitmap
	ASSERT(!m_hBitmap);
	return NULL;
}

//===========================================================================
//
//	Sub bitmap window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubBitmapWnd::CSubBitmapWnd()
{
	// Member variable initialization
	m_nWidth = 48;
	m_nHeight = 16;
	m_pBMPWnd = NULL;

	// Virtual size (must be defined by derived class)
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubBitmapWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZING()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation preprocessing
//
//---------------------------------------------------------------------------
BOOL CSubBitmapWnd::PreCreateWindow(CREATESTRUCT& cs)
{
	ASSERT(this);

	// Base class
	if (!CSubWnd::PreCreateWindow(cs)) {
		return FALSE;
	}

	// Resizable
	cs.style |= WS_THICKFRAME;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window creation
//
//---------------------------------------------------------------------------
int CSubBitmapWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	UINT id;
	CSize size;
	CRect rect;

	ASSERT(this);

	// Base class
	if (CSubWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

	// Status bar
	id = 0;
	m_StatusBar.Create(this);
	size = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	GetClientRect(&rect);
	m_StatusBar.MoveWindow(0, rect.bottom - size.cy, rect.Width(), size.cy);
	m_StatusBar.SetIndicators(&id, 1);
	m_StatusBar.SetPaneInfo(0, 0, SBPS_STRETCH, 0);

	// BMP window
	rect.bottom -= size.cy;
	m_pBMPWnd = new CSubBMPWnd;
	ASSERT(m_nScrlWidth > 0);
	m_pBMPWnd->m_nScrlWidth = m_nScrlWidth;
	ASSERT(m_nScrlHeight > 0);
	m_pBMPWnd->m_nScrlHeight = m_nScrlHeight;
	m_pBMPWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
					rect, this, 0, NULL);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Size change start
//
//---------------------------------------------------------------------------
void CSubBitmapWnd::OnSizing(UINT nSide, LPRECT lpRect)
{
	CRect rect;
	CSize sizeBar;
	CRect rectSizing;

	// Base class
	CSubWnd::OnSizing(nSide, lpRect);

	// Return if no status bar
	if (!::IsWindow(m_StatusBar.m_hWnd)) {
		return;
	}

	// Get BMP window max size (including scroll bar)
	m_pBMPWnd->GetMaximumRect(&rect, TRUE);

	// Get status bar size and subtract
	sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	rect.bottom += sizeBar.cy;

	// Get max size of this window
	::AdjustWindowRectEx(&rect, GetStyle(), FALSE, GetExStyle());

	// Clamp check
	rectSizing = *lpRect;
	if (rectSizing.Width() >= rect.Width()) {
		lpRect->right = lpRect->left + rect.Width();
	}
	if (rectSizing.Height() >= rect.Height()) {
		lpRect->bottom = lpRect->top + rect.Height();
	}
}

//---------------------------------------------------------------------------
//
//	Size change
//
//---------------------------------------------------------------------------
void CSubBitmapWnd::OnSize(UINT nType, int cx, int cy)
{
	CSize sizeBar;
	CRect rectClient;
	CRect rectWnd;
	CRect rectMax;
	CRect rectFit;

	ASSERT(this);
	ASSERT(cx >= 0);
	ASSERT(cy >= 0);

	// Base class
	CSubWnd::OnSize(nType, cx, cy);

	// Status bar placement (only if window is valid)
	if (::IsWindow(m_StatusBar.m_hWnd)) {
		// Get status bar height, client area height for adjustment
		sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
		GetClientRect(&rectClient);

		// If client area is larger than status bar, resize to fit
		if (rectClient.Height() > sizeBar.cy) {
			m_StatusBar.MoveWindow(0,
								rectClient.Height() - sizeBar.cy,
								rectClient.Width(),
								sizeBar.cy);

			// Resize BMP window accordingly
			rectClient.bottom -= sizeBar.cy;
			m_pBMPWnd->MoveWindow(0, 0, rectClient.Width(), rectClient.Height());
		}

		// Get BMP window size, subtract offset
		m_pBMPWnd->GetWindowRect(&rectWnd);
		m_pBMPWnd->GetMaximumRect(&rectMax, FALSE);
		cx = rectWnd.Width() - rectMax.Width();
		cy = rectWnd.Height() - rectMax.Height();

		// If possible, shrink this window (depending on scroll bar visibility)
		if ((cx > 0) || (cy > 0)) {
			GetWindowRect(&rectWnd);
			SetWindowPos(&wndTop, 0, 0, rectWnd.Width() - cx, rectWnd.Height() - cy,
						SWP_NOMOVE);
			return;
		}



	}
}

//---------------------------------------------------------------------------
//
//	Refresh
//
//---------------------------------------------------------------------------
void FASTCALL CSubBitmapWnd::Refresh()
{
	CRect rect;

	// Enable flag check
	if (!m_bEnable || !m_pBMPWnd) {
		return;
	}

	// Get draw rect
	m_pBMPWnd->GetDrawRect(&rect);
	if ((rect.Width() == 0) && (rect.Height() == 0)) {
		return;
	}

	// Setup
	Setup(rect.left, rect.top, rect.Width(), rect.Height(), m_pBMPWnd->GetBits());

	CDrawView::DRAWINFO info;
	m_pDrawView->GetDrawInfo(&info);
	// Display
	m_pBMPWnd->Refresh();
}

//---------------------------------------------------------------------------
//
//	Palette conversion
//
//---------------------------------------------------------------------------
DWORD FASTCALL CSubBitmapWnd::ConvPalette(WORD value)
{
	DWORD r;
	DWORD g;
	DWORD b;

	// All copy
	r = (DWORD)value;
	g = (DWORD)value;
	b = (DWORD)value;

	// MSB: G:5, R:5, B:5, I:1 -> must become
	// R:8, G:8, B:8 DWORD (B:31-24 unused)
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// Intensity bit round up
	if (value & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	return (DWORD)(r | g | b);
}

#endif	// _WIN32
