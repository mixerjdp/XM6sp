//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC サブウィンドウ(ビデオ) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_vid_h)
#define mfc_vid_h

#include "mfc_sub.h"

//===========================================================================
//
//	Sub-video window
//
//===========================================================================
class CSubVideoWnd : public CSubWnd
{
public:
	CSubVideoWnd();
										// Constructor
	BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation preparation
	void FASTCALL Refresh();
										// Refresh
	void FASTCALL Update();
										// Update from message thread
#if !defined(NDEBUG)
	void AssertValid() const;
										// Diagnostics
#endif	// NDEBUG

protected:
	// Message handler
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window creation
	afx_msg void OnSizing(UINT nSide, LPRECT lpRect);
										// Sizing
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change

	// Update
	virtual void FASTCALL Setup(CRect& rect, BYTE *pBits) = 0;
										// Setup

	// Child window
	CSubBMPWnd *m_pBMPWnd;
										// Bitmap window
	CStatusBar m_StatusBar;
										// Status bar
	int m_nScrlWidth;
										// Virtual screen width (dots)
	int m_nScrlHeight;
										// Virtual screen height (dots)
	int m_nPane;
										// Status bar pane count

	// Common
	DWORD FASTCALL GetColor(DWORD dwColor) const;
										// Create color (from 16-bit)
	DWORD FASTCALL GetPalette(DWORD dwPalette) const;
										// Create color (from palette index)
	CRTC *m_pCRTC;
										// CRTC
	VC *m_pVC;
										// VC
	BOOL m_bScroll;
										// Consider scroll
	BOOL m_bPalette;
										// Consider palette
	BOOL m_bContrast;
										// Consider contrast

	DECLARE_MESSAGE_MAP()
										// Message map exists
};


//===========================================================================
//
//	Text screen window
//
//===========================================================================
class CTVRAMWnd : public CSubVideoWnd
{
public:
	CTVRAMWnd();
										// Constructor
	void FASTCALL Setup(CRect& rect, BYTE* pBits);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	TVRAM *m_pTVRAM;
										// TVRAM
};

//===========================================================================
//
//	Graphic screen (1024x1024) window
//
//===========================================================================
class CG1024Wnd : public CSubBitmapWnd
{
public:
	CG1024Wnd();
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[16];
										// Window-held palette
	DWORD m_WndColor[16];
										// Window color table
	const BYTE *m_pGVRAM;
										// GVRAM address
};

//===========================================================================
//
//	Graphic screen (16 colors) window
//
//===========================================================================
class CG16Wnd : public CSubBitmapWnd
{
public:
	CG16Wnd(int nPage);
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	int m_nPage;
										// Page number
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[16];
										// Window-held palette
	DWORD m_WndColor[16];
										// Window color table
	const BYTE *m_pGVRAM;
										// GVRAM address
};

//===========================================================================
//
//	Graphic screen (256 colors) window
//
//===========================================================================
class CG256Wnd : public CSubBitmapWnd
{
public:
	CG256Wnd(int nPage);
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	int m_nPage;
										// Page number
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[256];
										// Window-held palette
	DWORD m_WndColor[256];
										// Window color table
	const BYTE *m_pGVRAM;
										// GVRAM address
};

//===========================================================================
//
//	Graphic screen (65536 colors) window
//
//===========================================================================
class CG64KWnd : public CSubBitmapWnd
{
public:
	CG64KWnd();
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	void FASTCALL Palette();
										// Palette reset
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[256];
										// Window-held palette
	DWORD m_WndColor[0x10000];
										// Window color table
	const BYTE *m_pGVRAM;
										// GVRAM address
};

//===========================================================================
//
//	PCG window
//
//===========================================================================
class CPCGWnd : public CSubBitmapWnd
{
public:
	CPCGWnd();
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread
	afx_msg void OnParentNotify(UINT message, LPARAM lParam);
										// Notify parent window
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
										// Context menu
	afx_msg void OnPalette(UINT uID);
										// Palette change

private:
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[256];
										// Window-held palette
	DWORD m_WndColor[256];
										// Window color table
	const BYTE *m_pPCG;
										// PCG address
	int m_nColor;
										// Color block

	DECLARE_MESSAGE_MAP()
										// Message map exists
};

//===========================================================================
//
//	BG window
//
//===========================================================================
class CBGWnd : public CSubBitmapWnd
{
public:
	CBGWnd(int nPage);
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	const WORD *m_pPalette;
										// VC palette address
	WORD m_WndPalette[256];
										// Window-held palette
	DWORD m_WndColor[256];
										// Window color table
	int m_nPage;
										// Page
	Sprite *m_pSprite;
										// Sprite controller
};

//===========================================================================
//
//	Palette window
//
//===========================================================================
class CPaletteWnd : public CSubBitmapWnd
{
public:
	CPaletteWnd(BOOL bRend);
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	void FASTCALL SetupRend(DWORD *buf, int no);
										// Setup (Renderer)
	void FASTCALL SetupVC(DWORD *buf, int no);
										// Setup (VC)
	BOOL m_bRend;
										// Renderer flag
	Render *m_pRender;
										// Renderer
	const WORD *m_pVCPal;
										// VC palette
};

#endif	// mfc_vid_h
#endif	// _WIN32