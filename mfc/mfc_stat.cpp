//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC status view ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "mfc.h"
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "render.h"
#include "mfc_frm.h"
#include "mfc_res.h"
#include "mfc_draw.h"
#include "mfc_info.h"
#include "mfc_stat.h"

//===========================================================================
//
//	Status view
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CStatusView::CStatusView()
{
	int i;

	// Frame window
	m_pFrmWnd = NULL;

	// Messages
	m_strCaption.Empty();
	::GetMsg(AFX_IDS_IDLEMESSAGE, m_strIdle);
	m_strMenu = m_strIdle;
	m_strInfo.Empty();

	// FDD rect
	for (i=0; i<2; i++) {
		m_rectFDD[i].left = 0;
		m_rectFDD[i].top = 0;
		m_rectFDD[i].right = 0;
		m_rectFDD[i].bottom = 0;
	}

	// LED rect
	for (i=0; i<3; i++) {
		m_rectLED[i].left = 0;
		m_rectLED[i].top = 0;
		m_rectLED[i].right = 0;
		m_rectLED[i].bottom = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Message map
//
//---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CStatusView, CWnd)
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_WM_SIZE()
	ON_WM_MOVE()
END_MESSAGE_MAP()

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CStatusView::Init(CFrmWnd *pFrmWnd)
{
	CRect rectParent;

	ASSERT(this);
	ASSERT(pFrmWnd);

	// Frame window member
	m_pFrmWnd = pFrmWnd;

	// Get frame window rectangle
	pFrmWnd->GetClientRect(&rectParent);

	// Frame window rectangle, 20px height
	m_rectClient.left = 0;
	m_rectClient.top = 0;
	m_rectClient.right = rectParent.Width();
	m_rectClient.bottom = 20;

	// Create
	if (!Create(NULL, NULL, WS_CHILD | WS_VISIBLE,
				m_rectClient, pFrmWnd, IDM_FULLSCREEN, NULL)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window paint
//
//---------------------------------------------------------------------------
void CStatusView::OnPaint()
{
	CPaintDC dc(this);
	CInfo *pInfo;
	int i;

	// Do nothing if minimized
	if (IsIconic()) {
		return;
	}

	// Message
	DrawMessage(&dc);

	// Status
	pInfo = m_pFrmWnd->GetInfo();
	if (pInfo) {
		// Delegate to Info
		for (i=0; i<2; i++) {
			// FDD
			if (m_rectFDD[i].Width() > 0) {
				pInfo->DrawStatus(i, dc.m_hDC, m_rectFDD[i]);
			}
		}
		for (i=0; i<3; i++) {
			// LED
			if (m_rectLED[i].Width() > 0) {
				pInfo->DrawStatus(i + 2, dc.m_hDC, m_rectLED[i]);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Window background paint
//
//---------------------------------------------------------------------------
BOOL CStatusView::OnEraseBkgnd(CDC *pDC)
{
	// Fill black
	pDC->FillSolidRect(&m_rectClient, RGB(0, 0, 0));

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Window size change
//
//---------------------------------------------------------------------------
void CStatusView::OnSize(UINT nType, int cx, int cy)
{
	CClientDC *pDC;
	TEXTMETRIC tm;
	HFONT hFont;
	HFONT hDefFont;
	int i;
	int nRest;

	// Do nothing if no client area yet
	if ((cx == 0) || (cy == 0)) {
		m_rectClient.right = 0;
		m_rectClient.bottom = 0;
		return;
	}

	// Set client area
	m_rectClient.right = cx;
	m_rectClient.bottom = cy;

	// Get text metrics
	pDC = new CClientDC(this);
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(pDC->m_hDC, hFont);
	ASSERT(hDefFont);
	pDC->GetTextMetrics(&tm);
	::SelectObject(pDC->m_hDC, hDefFont);
	delete pDC;

	// Set text metrics
	m_tmWidth = tm.tmAveCharWidth;
	m_tmHeight = tm.tmHeight + tm.tmExternalLeading;

	// Calculate top/bottom ratio for text
	nRest = cy - m_tmHeight;

	// Calculate LED rectangles
	for (i=0; i<3; i++) {
		m_rectLED[i].top = nRest >> 2;
		m_rectLED[i].bottom = cy - (nRest >> 2);

		// Set individual size
		switch (i) {
			// HD BUSY
			case 0:
				m_rectLED[i].left = cx - m_tmWidth * 31;
				m_rectLED[i].right = m_rectLED[i].left;
				m_rectLED[i].right += m_tmWidth * 11;
				break;
			// TIMER
			case 1:
				m_rectLED[i].left = cx - m_tmWidth * 19;
				m_rectLED[i].right = m_rectLED[i].left;
				m_rectLED[i].right += m_tmWidth * 9;
				break;
			// POWER
			case 2:
				m_rectLED[i].left = cx - m_tmWidth * 9;
				m_rectLED[i].right = m_rectLED[i].left;
				m_rectLED[i].right += m_tmWidth * 9;
				break;
			// Others (ignore)
			default:
				ASSERT(FALSE);
				break;
		}
	}

	// Calculate FDD rectangles
	m_rectFDD[0].left = cx * 4 / 10;
	m_rectFDD[0].right = m_rectLED[0].left - m_tmWidth;
	m_rectFDD[1].left = m_rectFDD[0].left + m_rectFDD[0].Width() / 2;
	m_rectFDD[1].right = m_rectFDD[0].right;
	m_rectFDD[0].right = m_rectFDD[1].left - m_tmWidth / 2;
	m_rectFDD[1].left += m_tmWidth / 2;
	for (i=0; i<2; i++) {
		m_rectFDD[i].top = nRest >> 2;
		m_rectFDD[i].bottom = cy - (nRest >> 2);
	}

	// Redraw
	Invalidate(FALSE);

	// Base class
	CWnd::OnSize(nType, cx, cy);
}

//---------------------------------------------------------------------------
//
//	Window move
//
//---------------------------------------------------------------------------
void CStatusView::OnMove(int x, int y)
{
	ASSERT(m_pFrmWnd);

	// Base class
	CWnd::OnMove(x, y);

	// Re-layout frame window status view
	m_pFrmWnd->RecalcStatusView();
}

//---------------------------------------------------------------------------
//
//	Window destruction
//
//---------------------------------------------------------------------------
void CStatusView::PostNcDestroy()
{
	// Delete interface object
	delete this;
}

//---------------------------------------------------------------------------
//
//	Caption string
//
//---------------------------------------------------------------------------
void FASTCALL CStatusView::SetCaptionString(CString& strCap)
{
	CClientDC dc(this);

	ASSERT(this);

	// Store and draw
	m_strCaption = strCap;
	DrawMessage(&dc);
}

//---------------------------------------------------------------------------
//
//	Menu string
//
//---------------------------------------------------------------------------
void FASTCALL CStatusView::SetMenuString(CString& strMenu)
{
	CClientDC dc(this);

	ASSERT(this);

	// Store and draw
	m_strMenu = strMenu;
	DrawMessage(&dc);
}

//---------------------------------------------------------------------------
//
//	Info string
//
//---------------------------------------------------------------------------
void FASTCALL CStatusView::SetInfoString(CString& strInfo)
{
	CClientDC dc(this);

	ASSERT(this);

	// Store and draw
	m_strInfo = strInfo;
	DrawMessage(&dc);
}

//---------------------------------------------------------------------------
//
//	Message draw
//
//---------------------------------------------------------------------------
void FASTCALL CStatusView::DrawMessage(CDC *pDC) const
{
	CString strMsg;
	CBitmap Bitmap;
	CBitmap *pBitmap;
	CDC mDC;
	HFONT hFont;
	HFONT hDefFont;
	CRect rectDraw;

	ASSERT(pDC);

	// Do nothing if no size
	if ((m_rectClient.Width() == 0) || (m_rectClient.Height() == 0)) {
		return;
	}

	// Select string (Menu first, Info second, Cap third)
	strMsg = m_strMenu;
	if (strMsg == m_strIdle) {
		if (m_strInfo.GetLength() > 0) {
			strMsg = m_strInfo;
		}
		else {
			strMsg = m_strCaption;
		}
	}

	// Left rect (40% width)
	rectDraw.left = 0;
	rectDraw.top = 0;
	rectDraw.right = m_rectClient.right * 4 / 10;
	rectDraw.bottom = m_rectClient.bottom;

	// Create compatible DC and memory bitmap
	VERIFY(mDC.CreateCompatibleDC(pDC));
	VERIFY(Bitmap.CreateCompatibleBitmap(pDC, rectDraw.right, rectDraw.bottom));
	pBitmap = mDC.SelectObject(&Bitmap);
	ASSERT(pBitmap);

	// Select font
	hFont = (HFONT)::GetStockObject(DEFAULT_GUI_FONT);
	hDefFont = (HFONT)::SelectObject(mDC.m_hDC, hFont);
	ASSERT(hDefFont);

	// Clear and set color
	mDC.SetTextColor(RGB(255, 255, 255));
	mDC.FillSolidRect(&rectDraw, RGB(0, 0, 0));

	// DrawText
	rectDraw.left = 16;
	mDC.DrawText(strMsg, &rectDraw, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

	// BitBlt
	pDC->BitBlt(0, 0, rectDraw.right, rectDraw.bottom, &mDC, 0, 0, SRCCOPY);

	// Cleanup
	::SelectObject(mDC.m_hDC, hDefFont);
	mDC.SelectObject(pBitmap);
	VERIFY(Bitmap.DeleteObject());
	VERIFY(mDC.DeleteDC());
}

//---------------------------------------------------------------------------
//
//	Status draw
//
//---------------------------------------------------------------------------
void FASTCALL CStatusView::DrawStatus(int nPane)
{
	CClientDC dc(this);
	CRect rect;
	CInfo *pInfo;

	ASSERT(this);
	ASSERT(nPane >= 0);
	ASSERT(m_pFrmWnd);

	// Get Info
	pInfo = m_pFrmWnd->GetInfo();
	if (!pInfo) {
		return;
	}

	// Get rect
	if (nPane > 2) {
		rect = m_rectLED[nPane - 2];
	}
	else {
		rect = m_rectFDD[nPane];
	}

	// Delegate to Info
	pInfo->DrawStatus(nPane, dc.m_hDC, rect);
}

#endif	// _WIN32
