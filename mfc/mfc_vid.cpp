//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC サブウィンドウ(ビデオ) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "vc.h"
#include "tvram.h"
#include "gvram.h"
#include "sprite.h"
#include "render.h"
#include "mfc_sub.h"
#include "mfc_vid.h"
#include "mfc_asm.h"
#include "mfc_res.h"

//===========================================================================
//
//	Sub-video window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CSubVideoWnd::CSubVideoWnd()
{
	// Initial size setting
	m_nWidth = 48;
	m_nHeight = 16;

	// Child
	m_pBMPWnd = NULL;
	m_nPane = 1;

	// Maximum size
	m_nScrlWidth = -1;
	m_nScrlHeight = -1;

	// Object
	m_pCRTC = NULL;
	m_pVC = NULL;

	// Flag
	m_bScroll = FALSE;
	m_bPalette = FALSE;
	m_bContrast = FALSE;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CSubVideoWnd, CSubWnd)
	ON_WM_CREATE()
	ON_WM_SIZING()
	ON_WM_SIZE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Window creation preparation
//
//---------------------------------------------------------------------------
BOOL CSubVideoWnd::PreCreateWindow(CREATESTRUCT& cs)
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
int CSubVideoWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	UINT uID[16];
	CSize size;
	CRect rect;
	int i;

	ASSERT(this);
	ASSERT(lpCreateStruct);
	ASSERT(m_nScrlWidth > 0);
	ASSERT(m_nScrlHeight > 0);

	// Base class
	if (CSubWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

	// Get CRTC
	ASSERT(!m_pCRTC);
	m_pCRTC = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(m_pCRTC);

	// Get VC
	ASSERT(!m_pVC);
	m_pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(m_pVC);

	// Create status bar
	m_StatusBar.Create(this);
	size = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	GetClientRect(&rect);
	m_StatusBar.MoveWindow(0, rect.bottom - size.cy, rect.Width(), size.cy);

	// Status bar setting
	ASSERT((m_nPane >= 1) && (m_nPane <= 16));
	for (i=0; i<m_nPane; i++) {
		if (i == 0) {
			uID[0] = ID_SEPARATOR;
			continue;
		}
		uID[i] = i;
	}
	m_StatusBar.SetIndicators(uID, m_nPane);
	m_StatusBar.SetPaneInfo(0, 0, SBPS_NOBORDERS | SBPS_STRETCH, 0);

	// BMPウィンドウ
	rect.bottom -= size.cy;
	m_pBMPWnd = new CSubBMPWnd;
	m_pBMPWnd->m_nScrlWidth = m_nScrlWidth;
	m_pBMPWnd->m_nScrlHeight = m_nScrlHeight;
	m_pBMPWnd->Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL,
					rect, this, 0, NULL);

	return 0;
}

//---------------------------------------------------------------------------
//
//	Sizing
//
//---------------------------------------------------------------------------
void CSubVideoWnd::OnSizing(UINT nSide, LPRECT lpRect)
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

	// Get maximum size of BMP window (including scroll bar)
	m_pBMPWnd->GetMaximumRect(&rect, TRUE);

	// Get status bar size and total
	sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
	rect.bottom += sizeBar.cy;

	// Get maximum size for this window
	CalcWindowRect(&rect);

	// Overflow check
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
void CSubVideoWnd::OnSize(UINT nType, int cx, int cy)
{
	CSize sizeBar;
	CRect rectClient;
	CRect rectBmp;
	CRect rectMax;
	CRect rectWnd;
	CSize sizeWnd;
	BOOL bReSize;

	ASSERT(this);
	ASSERT(cx >= 0);
	ASSERT(cy >= 0);

	// Base class
	CSubWnd::OnSize(nType, cx, cy);

	// Relocate status bar (only if window is valid)
	if (::IsWindow(m_StatusBar.m_hWnd)) {
		// Get status bar height and client area size
		sizeBar = m_StatusBar.CalcFixedLayout(TRUE, TRUE);
		GetClientRect(&rectClient);

		// Change position if client area is sufficient for status bar
		if (rectClient.Height() > sizeBar.cy) {
			m_StatusBar.MoveWindow(0,
								rectClient.Height() - sizeBar.cy,
								rectClient.Width(),
								sizeBar.cy);

			// BMPウィンドウをあわせて再配置
			rectClient.bottom -= sizeBar.cy;
			m_pBMPWnd->MoveWindow(0, 0, rectClient.Width(), rectClient.Height());
		}

		// Get BMP window size and maximum allowable size
		m_pBMPWnd->GetWindowRect(&rectBmp);
		m_pBMPWnd->GetMaximumRect(&rectMax, FALSE);

		// Shrink if exceeding maximum allowable size (handles automatic scroll bar ON/OFF)
		GetWindowRect(&rectWnd);
		sizeWnd.cx = rectWnd.Width();
		sizeWnd.cy = rectWnd.Height();
		bReSize = FALSE;
		if (rectBmp.Width() > rectMax.Width()) {
			sizeWnd.cx -= (rectBmp.Width() - rectMax.Width());
			bReSize = TRUE;
		}
		if (rectBmp.Height() > rectMax.Height()) {
			sizeWnd.cy -= (rectBmp.Height() - rectMax.Height());
			bReSize = TRUE;
		}
		if (bReSize) {
			SetWindowPos(&wndTop, 0, 0, sizeWnd.cx, sizeWnd.cy, SWP_NOMOVE);
			return;
		}

		// Shrink if either size is exceeded (countermeasure for tiling/cascading)
		m_pBMPWnd->GetMaximumRect(&rectMax, TRUE);
		rectMax.bottom += sizeBar.cy;
		CalcWindowRect(&rectMax);
		GetWindowRect(&rectWnd);
		bReSize = FALSE;
		if (rectWnd.Width() >= rectMax.Width()) {
			rectWnd.right = rectWnd.left + rectMax.Width();
			bReSize = TRUE;
		}
		if (rectWnd.Height() >= rectMax.Height()) {
			rectWnd.bottom = rectWnd.top + rectMax.Height();
			bReSize = TRUE;
		}
		if (bReSize) {
			SetWindowPos(&wndTop, 0, 0, rectWnd.Width(), rectWnd.Height(), SWP_NOMOVE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Update
//
//---------------------------------------------------------------------------
void FASTCALL CSubVideoWnd::Refresh()
{
	CRect rect;

	ASSERT(this);
	ASSERT_VALID(this);

	// Check enable flag
	if (!m_bEnable || !m_pBMPWnd) {
		return;
	}

	// Get drawing rectangle
	m_pBMPWnd->GetDrawRect(&rect);
	if ((rect.Width() == 0) || (rect.Height() == 0)) {
		return;
	}

	// Setup
	Setup(rect, m_pBMPWnd->GetBits());
		

	// Display
	m_pBMPWnd->Refresh();
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CSubVideoWnd::Update()
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
void CSubVideoWnd::AssertValid() const
{
	ASSERT(this);

	// Base class
	CSubWnd::AssertValid();

	// Virtual screen size
	ASSERT(m_nScrlWidth > 0);
	ASSERT(m_nScrlHeight > 0);
}
#endif	// NDEBUG

//===========================================================================
//
//	Text screen window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CTVRAMWnd::CTVRAMWnd()
{
	// Window parameter definition
	m_dwID = MAKEID('T', 'V', 'R', 'M');
	::GetMsg(IDS_SWND_TVRAM, m_strCaption);

	// Virtual screen size setting
	m_nScrlWidth = 0x400;
	m_nScrlHeight = 0x400;

	// Get TVRAM
	m_pTVRAM = (TVRAM*)::GetVM()->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(m_pTVRAM);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CTVRAMWnd::Setup(CRect& rect, BYTE *pBits)
{
//	int x
//	int y;
//	DWORD bufBit[0x400];
	memset(pBits, 0, sizeof(pBits));
	memset(rect, 0, sizeof(rect));

#if 0
	int i;
	const BYTE *p;
	DWORD buf[1024];
	int len;
	int below;
	int delta;

	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	for (i=0; i<16; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	// x, y check
	if (x >= 1024) {
		// No display area. All black
		for (i=0; i<height; i++) {
			memset(ptr, 0, (width << 2));
			ptr += (width << 2);
		}
		return;
	}
	if (y >= 1024) {
		// No display area. All black
		for (i=0; i<height; i++) {
			memset(ptr, 0, (width << 2));
			ptr += (width << 2);
		}
		return;
	}

	// Initialize pointer
	p = m_pTVRAM;
	i = (x >> 5);
	p += (y << 7);
	p += (i << 2);

	// X-direction offset, rendering length
	x &= 0x1f;
	len = x + width + 31;
	len >>= 5;

	// Overflow countermeasure
	below = 0;
	if ((y + height) > 1024) {
		below = height - 1024 + y;
		height = 1024 - y;
	}
	delta = 0;
	if ((len + i) > 32) {
		len = 32 - i;
		delta = width - (len << 5);
		width = (len << 5);
	}

	// Rendering loop
	for (i=0; i<height; i++) {
		::VideoText(p, buf, len, m_WndColor);
		p += 128;

		// Copy considering x, width
		memcpy(ptr, &buf[x], (width << 2));
		ptr += (width << 2);
		memset(ptr, 0, (delta << 2));
		ptr += (delta << 2);
	}

	// Clear extra downward area
	for (i=0; i<below; i++) {
		memset(ptr, 0, (width << 2));
		ptr += (width << 2);
		memset(ptr, 0, (delta << 2));
		ptr += (delta << 2);
	}
#endif
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CTVRAMWnd::Update()
{
	ASSERT(this);
	ASSERT_VALID(this);

	m_StatusBar.SetPaneText(0, "ABC", TRUE);
#if 0
	int x;
	int y;
	DWORD addr;
	CString string;
	BYTE p[4];
	int value;
	int bit;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Get data
	addr = ((y << 7) + (x >> 3)) ^ 1;
	p[0] = m_pTVRAM[addr + 0x00000];
	p[1] = m_pTVRAM[addr + 0x20000];
	p[2] = m_pTVRAM[addr + 0x40000];
	p[3] = m_pTVRAM[addr + 0x60000];
	addr = (addr ^ 1) + 0xe00000;

	// Get RGB values
	value = (x & 0x07);
	bit = (0x80 >> value);
	value = 0;
	if (p[0] & bit) {
		value |= 0x01;
	}
	if (p[1] & bit) {
		value |= 0x02;
	}
	if (p[2] & bit) {
		value |= 0x04;
	}
	if (p[3] & bit) {
		value |= 0x08;
	}
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X [%02X %02X %02X %02X] Color%d R%d G%d B%d",
				x, y, addr,
				p[0], p[1], p[2], p[3],
				value, r, g, b);
	m_StatusBar.SetPaneText(0, string);
#endif
}

//===========================================================================
//
//	Graphic screen (1024x1024) window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CG1024Wnd::CG1024Wnd()
{
	VC *pVC;
	GVRAM *pGVRAM;
	int i;

	// Window parameter definition
	m_dwID = MAKEID('G', '1', '0', '2');
	::GetMsg(IDS_SWND_G1024, m_strCaption);

	// Virtual screen size
	m_nScrlWidth = 1024;
	m_nScrlHeight = 1024;

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();

	// Get Graphic VRAM memory
	pGVRAM = (GVRAM*)::GetVM()->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(pGVRAM);
	m_pGVRAM = pGVRAM->GetGVRAM();

	// Palette data initialization
	for (i=0; i<16; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CG1024Wnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	DWORD buf[1024];

	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	for (i=0; i<16; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	for (i=0; i<height; i++) {
		if (y < 512) {
			::VideoG1024A(&m_pGVRAM[y << 10], buf, m_WndColor);
		}
		else {
			::VideoG1024B(&m_pGVRAM[(y & 0x1ff) << 10], buf, m_WndColor);
		}
		memcpy(ptr, &buf[x], (width << 2));
		ptr += (width << 2);
		y++;
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CG1024Wnd::Update()
{
	int x;
	int y;
	DWORD addr;
	CString string;
	int value;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Get data
	addr = (y & 0x1ff);
	addr <<= 10;
	addr += ((x & 0x1ff) << 1);
	if (y < 512) {
		if (x < 512) {
			value = m_pGVRAM[addr] & 0x0f;
			addr += 0xc00000;
		}
		else {
			value = m_pGVRAM[addr] >> 4;
			addr += 0xc80000;
		}
	}
	else {
		if (x < 512) {
			value = m_pGVRAM[addr ^ 1] & 0x0f;
			addr += 0xd00000;
		}
		else {
			value = m_pGVRAM[addr ^ 1] >> 4;
			addr += 0xd80000;
		}
	}

	// Get RGB values
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X [%02X] Color%d R%d G%d B%d",
				x, y, addr,
				value, value, r, g, b);
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	Graphic screen (16 colors) window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CG16Wnd::CG16Wnd(int nPage)
{
	VC *pVC;
	GVRAM *pGVRAM;
	int i;

	// Page memory
	ASSERT((nPage >= 0) || (nPage <= 3));
	m_nPage = nPage;

	// Window parameter definition
	m_dwID = MAKEID('G', '1', '6', ('A' + nPage));
	::GetMsg((IDS_SWND_G16P0 + nPage), m_strCaption);

	// Virtual screen size
	m_nScrlWidth = 512;
	m_nScrlHeight = 512;

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();

	// Get Graphic VRAM memory
	pGVRAM = (GVRAM*)::GetVM()->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(pGVRAM);
	m_pGVRAM = pGVRAM->GetGVRAM();

	// Palette data initialization
	for (i=0; i<16; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CG16Wnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	const BYTE *p;
	DWORD buf[1024];
	int delta;

	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	for (i=0; i<16; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	// Initialize pointer
	p = m_pGVRAM;
	p += (x << 1);
	p += (y << 10);

	// 512x512, overflow is possible
	if ((y + height) > m_nScrlHeight) {
		height = m_nScrlHeight - y;
	}
	delta = 0;
	if ((x + width) > m_nScrlWidth) {
		delta = width - m_nScrlWidth + x;
		width = m_nScrlWidth - x;
	}

	// Rendering loop
	switch (m_nPage) {
		// Page0
		case 0:
			for (i=0; i<height; i++) {
				::VideoG16A(p, buf, width, m_WndColor);
				p += 1024;
				// Copy considering x, width
				memcpy(ptr, buf, (width << 2));
				ptr += (width << 2);
				ptr += (delta << 2);
			}
			break;
		// Page1
		case 1:
			for (i=0; i<height; i++) {
				::VideoG16B(p, buf, width, m_WndColor);
				p += 1024;
				// Copy considering x, width
				memcpy(ptr, buf, (width << 2));
				ptr += (width << 2);
				ptr += (delta << 2);
			}
			break;
		// Page2
		case 2:
			for (i=0; i<height; i++) {
				::VideoG16C(p, buf, width, m_WndColor);
				p += 1024;
				// Copy considering x, width
				memcpy(ptr, buf, (width << 2));
				ptr += (width << 2);
				ptr += (delta << 2);
			}
			break;
		// Page3
		case 3:
			for (i=0; i<height; i++) {
				::VideoG16D(p, buf, width, m_WndColor);
				p += 1024;
				// Copy considering x, width
				memcpy(ptr, buf, (width << 2));
				ptr += (width << 2);
				ptr += (delta << 2);
			}
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CG16Wnd::Update()
{
	int x;
	int y;
	DWORD addr;
	CString string;
	int value = 0;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Get data
	addr = ((y << 10) + (x << 1));
	switch (m_nPage) {
		case 0:
			value = m_pGVRAM[addr] & 0x0f;
			addr += 0xc00000;
			break;
		case 1:
			value = m_pGVRAM[addr] >> 4;
			addr += 0xc80000;
			break;
		case 2:
			value = m_pGVRAM[addr ^ 1] & 0x0f;
			addr += 0xd00000;
			break;
		case 3:
			value = m_pGVRAM[addr ^ 1] >> 4;
			addr += 0xd80000;
			break;
	}

	// Get RGB values
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X [%02X] Color%d R%d G%d B%d",
				x, y, addr,
				value, value, r, g, b);
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	Graphic screen (256 colors) window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CG256Wnd::CG256Wnd(int nPage)
{
	VC *pVC;
	GVRAM *pGVRAM;
	int i;

	// Page memory
	ASSERT((nPage == 0) || (nPage == 1));
	m_nPage = nPage;

	// Window parameter definition
	m_dwID = MAKEID('G', '2', '5', ('A' + nPage));
	::GetMsg((IDS_SWND_G256P0 + nPage), m_strCaption);

	// Virtual screen size
	m_nScrlWidth = 512;
	m_nScrlHeight = 512;

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();

	// Get Graphic VRAM memory
	pGVRAM = (GVRAM*)::GetVM()->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(pGVRAM);
	m_pGVRAM = pGVRAM->GetGVRAM();

	// Palette data initialization
	for (i=0; i<256; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CG256Wnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	const BYTE *p;
	DWORD buf[512];
	int delta;

	// assert
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	for (i=0; i<256; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	// Initialize pointer
	p = m_pGVRAM;
	p += (y << 10);
	p += (x << 1);

	// 512x512, overflow is possible
	if ((y + height) > 512) {
		height = 512 - y;
	}
	delta = 0;
	if ((x + width) > 512) {
		delta = width - 512 + x;
		width = 512 - x;
	}

	// Rendering loop
	if (m_nPage == 0) {
		for (i=0; i<height; i++) {
			::VideoG256A(p, buf, width, m_WndColor);
			p += 1024;

			// Copy considering x, width
			memcpy(ptr, buf, (width << 2));
			ptr += (width << 2);
			ptr += (delta << 2);
		}
	}
	else {
		for (i=0; i<height; i++) {
			::VideoG256B(p, buf, width, m_WndColor);
			p += 1024;

			// Copy considering x, width
			memcpy(ptr, buf, (width << 2));
			ptr += (width << 2);
			ptr += (delta << 2);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CG256Wnd::Update()
{
	int x;
	int y;
	DWORD addr;
	CString string;
	int value;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Get data (assuming 256-color GRAM layout)
	addr = (y << 9) + x;
	addr <<= 1;
	if (m_nPage > 0) {
		value = m_pGVRAM[addr ^ 1];
		addr += 0xc80000;
	}
	else {
		value = m_pGVRAM[addr];
		addr += 0xc00000;
	}

	// Get RGB values
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X [%02X] Color%d R%d G%d B%d",
				x, y, addr,
				value, value, r, g, b);
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	Graphic screen (65536 colors) window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CG64KWnd::CG64KWnd()
{
	VC *pVC;
	GVRAM *pGVRAM;
	int i;

	// Window parameter definition
	m_dwID = MAKEID('G', '6', '4', 'K');
	::GetMsg(IDS_SWND_G64K, m_strCaption);

	// Virtual screen size
	m_nScrlWidth = 512;
	m_nScrlHeight = 512;

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();

	// Get Graphic VRAM memory
	pGVRAM = (GVRAM*)::GetVM()->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(pGVRAM);
	m_pGVRAM = pGVRAM->GetGVRAM();

	// Palette data initialization
	for (i=0; i<256; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CG64KWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	BOOL flag;
	int i;
	const BYTE *p;
	DWORD buf[512];
	int delta;

	// assert
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	flag = FALSE;
	for (i=0; i<256; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			flag = TRUE;
		}
	}

	// Palette reconstruction
	if (flag) {
		Palette();
	}

	// Initialize pointer
	p = m_pGVRAM;
	p += (y << 10);
	p += (x << 1);

	// 512x512, overflow is possible
	if ((y + height) > 512) {
		height = 512 - y;
	}
	delta = 0;
	if ((x + width) > 512) {
		delta = width - 512 + x;
		width = 512 - x;
	}

	// Rendering loop
	for (i=0; i<height; i++) {
		// レンダリング
		::VideoG64K(p, buf, width, m_WndColor);
		p += 1024;

		// Copy considering x, width
		memcpy(ptr, buf, (width << 2));
		ptr += (width << 2);
		ptr += (delta << 2);
	}
}

//---------------------------------------------------------------------------
//
//	Palette reconstruction
//
//---------------------------------------------------------------------------
void FASTCALL CG64KWnd::Palette()
{
	const WORD *p;
	const WORD *q;
	DWORD *r;
	WORD hi;
	WORD lo;
	int i;
	int j;

	// Initialize store destination
	r = m_WndColor;

	// Loop1
	p = m_pPalette + 1;
	for (i=0; i<256; i++) {
		// hiGet data
		hi = *p;
		if (i & 1) {
			hi <<= 8;
			p += 2;
		}
		else {
			hi &= 0xff00;
		}

		// Loop2
		q = m_pPalette;
		for (j=0; j<256; j++) {
			// loGet data
			lo = *q;
			if (j & 1) {
				lo &= 0x00ff;
				q += 2;
			}
			else {
				lo >>= 8;
			}

			// Convert & store
			lo |= hi;
			*r++ = ConvPalette(lo);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CG64KWnd::Update()
{
	int x;
	int y;
	DWORD addr;
	CString string;
	int value;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Get data (assuming 64K-color GRAM layout)
	addr = ((y << 10) + (x << 1));
	value = m_pGVRAM[addr + 1];
	value <<= 8;
	value |= m_pGVRAM[addr];
	addr += 0xc00000;

	// Get RGB values
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X [%04X] Color%d R%d G%d B%d",
				x, y, addr,
				value, value, r, g, b);
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	PCG window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPCGWnd::CPCGWnd()
{
	VC *pVC;
	Sprite *pSprite;
    int i;

	// Window parameter definition
	m_dwID = MAKEID('P', 'C', 'G', ' ');
	::GetMsg(IDS_SWND_PCG, m_strCaption);

	// ウィンドウ初期サイズ
	m_nWidth = 28;
	m_nHeight = 16;

	// Virtual screen size
	m_nScrlWidth = 256;
	m_nScrlHeight = 256;

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();
	m_pPalette += 256;

	// PCGメモリ取得
	pSprite = (Sprite*)::GetVM()->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(pSprite);
	m_pPCG = pSprite->GetPCG();

	// Palette data initialization
	for (i=0; i<256; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}

	// Color block 1
	m_nColor = 1;
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CPCGWnd, CSubBitmapWnd)
	ON_WM_PARENTNOTIFY()
	ON_WM_CONTEXTMENU()
	ON_COMMAND_RANGE(IDM_PCG_0, IDM_PCG_F, OnPalette)
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CPCGWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	int delta;
	int addr;
	DWORD buf[256];

	// assert
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Palette check
	for (i=0; i<256; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	// 256×256なので、オーバーする可能性がある
	if ((y + height) > 256) {
		height = 256 - y;
	}
	delta = 0;
	if ((x + width) > 256) {
		delta = width - 256 + x;
		width = 256 - x;
	}

	// Rendering loop
	for (i=0; i<height; i++) {
		// yから、アドレスを決める
		addr = y >> 4;
		addr <<= 11;
		if (y & 8) {
			addr += 0x0020;
		}
		addr += ((y & 7) << 2);

		// レンダリング
		::VideoPCG((BYTE*)&m_pPCG[addr], buf, &m_WndColor[m_nColor << 4]);

		// Copy considering x, width
		memcpy(ptr, &buf[x], (width << 2));
		ptr += (width << 2);
		ptr += (delta << 2);

		// 次のyへ
		y++;
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CPCGWnd::Update()
{
	int x;
	int y;
	CString string;
	int addr;
	int base;
	int color;
	int value;
	int r;
	int g;
	int b;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// アドレス計算
	addr = y >> 4;
	addr <<= 11;
	if (y & 8) {
		addr += 0x0020;
	}
	addr += ((y & 7) << 2);
	addr += ((x >> 3) << 6);
	addr += ((x & 7) >> 1);
	base = addr;

	// アドレス計算
	addr += 0xeb8000;

	// カラー取得
	color = m_pPCG[base ^ 1];
	if ((x & 1) == 0) {
		color >>= 4;
	}
	color &= 0x0f;

	// Get RGB
	value = (m_nColor << 4) + color;
	r = m_WndColor[value] >> 16;
	g = (m_WndColor[value] >> 8) & 0xff;
	b = m_WndColor[value] & 0xff;

	// Format and set
	string.Format("( %d, %d) $%06X Pal%1X [$%02X +%d +%d] Color%d R%d G%d B%d",
				x, y, addr, m_nColor,
				(y & 0xf0) + (x >> 4), (x & 0x0f), (y & 0x0f),
				color, r, g, b);
	m_StatusBar.SetPaneText(0, string);
}

//---------------------------------------------------------------------------
//
//	親ウィンドウへ通知
//
//---------------------------------------------------------------------------
void CPCGWnd::OnParentNotify(UINT message, LPARAM lParam)
{
	POINT point;
	CRect rect;

	if (message == WM_LBUTTONDOWN) {
		// クライアント内なら、Palette change
		::GetCursorPos(&point);
		m_pBMPWnd->GetClientRect(&rect);
		m_pBMPWnd->ClientToScreen(&rect);
		if (rect.PtInRect(point)) {
			m_nColor = (m_nColor + 1) & 0x0f;
		}
	}

	// Base class
	CSubBitmapWnd::OnParentNotify(message, lParam);
}

//---------------------------------------------------------------------------
//
//	Context menu
//
//---------------------------------------------------------------------------
void CPCGWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

	// クライアント領域内で押されたか判定する
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubBitmapWnd::OnContextMenu(pWnd, point);
		return;
	}

	// メニュー実行
	menu.LoadMenu(IDR_PCGMENU);
	pMenu = menu.GetSubMenu(0);
	pMenu->CheckMenuItem(m_nColor, MF_CHECKED | MF_BYPOSITION);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

//---------------------------------------------------------------------------
//
//	Palette change
//
//---------------------------------------------------------------------------
void CPCGWnd::OnPalette(UINT uID)
{
	uID -= IDM_PCG_0;
	ASSERT(uID < 0x10);

	m_nColor = (int)uID;
}

//===========================================================================
//
//	BG window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CBGWnd::CBGWnd(int nPage)
{
	VC *pVC;
	int i;

	ASSERT((nPage == 0) || (nPage == 1));

	// Page memory
	m_nPage = nPage;

	// Window parameter definition
	m_dwID = MAKEID('B', 'G', ('0' + nPage), ' ');
	::GetMsg(IDS_SWND_BG0 + nPage, m_strCaption);

	// Virtual screen size
	if (nPage == 0) {
		m_nScrlWidth = 1024;
		m_nScrlHeight = 1024;
	}
	else {
		m_nScrlWidth = 512;
		m_nScrlHeight = 512;
	}

	// Get palette memory
	pVC = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(pVC);
	m_pPalette = (WORD*)pVC->GetPalette();
	m_pPalette += 256;

	// Palette data initialization
	for (i=0; i<256; i++) {
		m_WndPalette[i] = 0;
		m_WndColor[i] = 0;
	}

	// Get sprite
	m_pSprite = (Sprite*)::GetVM()->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(m_pSprite);
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CBGWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	DWORD buf[1024];
	DWORD *q;
	BYTE *pcg;
	Sprite::sprite_t spr;
	int delta;
	int size;
	int offset;
	int i;
	int j;

	// assert
	ASSERT(x >= 0);
	ASSERT(y >= 0);
	ASSERT(width > 0);
	ASSERT(height > 0);
	ASSERT(ptr);

	// Get sprite data, calculate PCG
	m_pSprite->GetSprite(&spr);
	pcg = &spr.mem[0x8000];

	// In case of BG1
	if (m_nPage != 0) {
		// Do not display if BG size is 16x16
		if (spr.bg_size) {
			memset(ptr, 0, (width * height) << 2);
			return;
		}
	}

	// 最初にクリア
	memset(buf, 0, sizeof(buf));
	memset(ptr, 0, (width * height) << 2);

	// サイズ確定
	if (spr.bg_size) {
		ASSERT(m_nPage == 0);
		size = 1024;
	}
	else {
		size = 512;
	}

	// Palette check
	for (i=0; i<256; i++) {
		if (m_WndPalette[i] != m_pPalette[i]) {
			m_WndPalette[i] = m_pPalette[i];
			m_WndColor[i] = ConvPalette(m_pPalette[i]);
		}
	}

	// Advance overflow check
	if (y > size) {
		return;
	}
	if (x > size) {
		return;
	}

	// Overflow check
	if ((y + height) > size) {
		height = size - y;
	}
	delta = 0;
	if ((x + width) > size) {
		delta = width - size + x;
		width = size - x;
	}

	// Rendering loop
	for (i=0; i<height; i++) {
		// Get BG data offset from y
		if (spr.bg_size) {
			// 16x16
			offset = (y >> 4);
		}
		else {
			// 8x8
			offset = (y >> 3);
		}
		// 横方向に64BG、つまり128バイト
		offset <<= 7;
		// BG data area 0 is +C000, area 1 is +E000
		offset += ((spr.bg_area[m_nPage] & 0x01) << 13);
		offset += 0xc000;

		// X方向Loop
		q = buf;
		for (j=0; j<64; j++) {
			// Assembler sub
			if (spr.bg_size) {
				::VideoBG16(pcg, q, *(WORD*)&spr.mem[offset],
											y & 0x0f, m_WndColor);
				q += 16;
			}
			else {
				::VideoBG8(pcg, q, *(WORD*)&spr.mem[offset],
											y & 0x07, m_WndColor);
				q += 8;
			}
			offset += 2;
		}

		// Copy considering x, width
		memcpy(ptr, &buf[x], (width << 2));
		ptr += (width << 2);
		ptr += (delta << 2);

		// 次のyへ
		y++;
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CBGWnd::Update()
{
	CString string;
	Sprite::sprite_t spr;
	DWORD addr;
	WORD data;
	int x;
	int y;

	// Get sprite data, calculate PCG
	m_pSprite->GetSprite(&spr);

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Page 0 is 1024x1024 window, sometimes only 512x512 is used
	if ((m_nPage == 0) && (!spr.bg_size)) {
		if (x >= 512) {
			m_StatusBar.SetPaneText(0, "");
			return;
		}
		if (y >= 512) {
			m_StatusBar.SetPaneText(0, "");
			return;
		}
	}

	// Calculate address
	if (spr.bg_size) {
		// 16×16
		addr = y >> 4;
		addr <<= 7;
		addr += ((x >> 4) << 1);
	}
	else {
		addr = y >> 3;
		addr <<= 7;
		addr += ((x >> 3) << 1);
	}
	// BG data area 0 is +C000, area 1 is +E000
	addr += ((spr.bg_area[m_nPage] & 0x01) << 13);
	addr += 0xc000;

	// Get data
	data = *(WORD*)&spr.mem[addr];
	addr += 0xeb0000;

	// Output
	string.Format("( %d, %d) $%06X [%04X] PCG%d Pal%1X",
				x, y, addr, data, data & 0xff, ((data >> 8) & 0x0f));
	if (data & 0x4000) {
		string += " X-Reverse";
	}
	if (data & 0x8000) {
		string += " Y-Reverse";
	}
	m_StatusBar.SetPaneText(0, string);
}

//===========================================================================
//
//	Palette window
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CPaletteWnd::CPaletteWnd(BOOL bRend)
{
	VC *vc;

	// Window size setting (32x16 becomes 16x32)
	m_nScrlWidth = 512;
	m_nScrlHeight = 512;

	// Type setting
	m_bRend = bRend;

	// Set according to type
	if (m_bRend) {
		m_dwID = MAKEID('P', 'A', 'L', 'B');
		::GetMsg(IDS_SWND_REND_PALET, m_strCaption);
		m_pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
		ASSERT(m_pRender);
	}
	else {
		m_dwID = MAKEID('P', 'A', 'L', ' ');
		::GetMsg(IDS_SWND_PALET, m_strCaption);
		vc = (VC*)::GetVM()->SearchDevice(MAKEID('V', 'C', ' ', ' '));
		ASSERT(vc);
		m_pVCPal = (WORD*)vc->GetPalette();
		ASSERT(m_pVCPal);
	}
}

//---------------------------------------------------------------------------
//
//	Setup
//
//---------------------------------------------------------------------------
void FASTCALL CPaletteWnd::Setup(int x, int y, int width, int height, BYTE *ptr)
{
	int i;
	int n;
	int m;
	int delta;
	DWORD buf[512];

	// Initialize data
	m = -2;

	// 512x512, overflow is possible
	if ((y + height) > 512) {
		height = 512 - y;
	}
	delta = 0;
	if ((x + width) > 512) {
		delta = width - 512 + x;
		width = 512 - x;
	}

	// Loop
	for (i=0; i<height; i++) {
		// Determine data to display
		n = y + i;
		if ((n & 0x0f) == 0x0f) {
			// y is +15: gap space
			n = -1;
		}
		else {
			// y is +0~+14: normal data
			n >>= 4;
		}

		// Displayデータを作成
		if (m != n) {
			m = n;
			if (n < 0) {
				// 間の空白
				memset(buf, 0, sizeof(buf));
			}
			else {
				// Normal data
				if (m_bRend) {
					SetupRend(buf, n);
				}
				else {
					SetupVC(buf, n);
				}
			}
		}

		// コピー
		memcpy(ptr, &buf[x], width << 2);
		ptr += (width << 2);
		ptr += (delta << 2);
	}
}

//---------------------------------------------------------------------------
//
//	Setup (Renderer)
//
//---------------------------------------------------------------------------
void FASTCALL CPaletteWnd::SetupRend(DWORD *buf, int n)
{
	const DWORD *p;
	DWORD rgb;
	int i;
	int j;

	ASSERT(buf);
	ASSERT((n >= 0) && (n <= 0x1f));

	// ポインタDetermine
	p = m_pRender->GetPalette();
	p += (n << 4);

	// 16-color loop
	for (i=0; i<16; i++) {
		// カラー取得
		rgb =*p++;

		// Draw 31 consecutive dots
		for (j=0; j<31; j++) {
			*buf++ = rgb;
		}

		// Next is blank
		*buf++ = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Setup (VC)
//
//---------------------------------------------------------------------------
void FASTCALL CPaletteWnd::SetupVC(DWORD *buf, int n)
{
	const WORD *p;
	DWORD rgb;
	int i;
	int j;

	ASSERT(buf);
	ASSERT((n >= 0) && (n <= 0x1f));

	// ポインタDetermine
	ASSERT(m_pVCPal);
	p = m_pVCPal;
	p += (n << 4);

	// 16-color loop
	for (i=0; i<16; i++) {
		// カラー取得
		rgb = ConvPalette(*p++);

		// Draw 31 consecutive dots
		for (j=0; j<31; j++) {
			*buf++ = rgb;
		}

		// Next is blank
		*buf++ = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Update from message thread
//
//---------------------------------------------------------------------------
void FASTCALL CPaletteWnd::Update()
{
	int x;
	int y;
	int index;
	DWORD rgb;
	const DWORD *p;
	CString string;

	// BMP window check
	if (!m_pBMPWnd) {
		return;
	}

	// Mouse cursor check
	if ((m_pBMPWnd->m_nCursorX < 0) || (m_pBMPWnd->m_nCursorY < 0)) {
		m_StatusBar.SetPaneText(0, "");
		return;
	}

	// Coordinate calculation, overflow check
	x = m_pBMPWnd->m_nCursorX + m_pBMPWnd->m_nScrlX;
	y = m_pBMPWnd->m_nCursorY + m_pBMPWnd->m_nScrlY;
	if (x >= m_nScrlWidth) {
		return;
	}
	if (y >= m_nScrlHeight) {
		return;
	}

	// Produce display data
	index = y & ~0xf;
	index += (x >> 5);

	if (m_bRend) {
		p = m_pRender->GetPalette();
		rgb = p[index];
		string.Format("($%03X) Contrast%d R%d G%d B%d",
						index,
						m_pRender->GetContrast(),
						(rgb >> 16) & 0xff,
						(rgb >> 8) & 0xff,
						(rgb & 0xff));
	}
	else {
		rgb = ConvPalette(m_pVCPal[index]);
		string.Format("($%03X) $%06X [$%04X] R%d G%d B%d",
						index,
						0xe82000 + (index << 1),
						m_pVCPal[index],
						(rgb >> 16) & 0xff,
						(rgb >> 8) & 0xff,
						(rgb & 0xff));
	}
	m_StatusBar.SetPaneText(0, string);
}

#endif	// _WIN32