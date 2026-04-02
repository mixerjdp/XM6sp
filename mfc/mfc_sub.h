//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC sub window ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined (mfc_sub_h)
#define mfc_sub_h

//===========================================================================
//
//	Sub window
//
//===========================================================================
class CSubWnd : public CWnd
{
public:
	CSubWnd();
										// Constructor
	BOOL FASTCALL Init(CDrawView *pDrawView);
										// Initialization
	void FASTCALL Enable(BOOL m_bEnable);
										// Enable control
	DWORD FASTCALL GetID() const;
										// Returns window ID
	virtual void FASTCALL Refresh() = 0;
										// Refresh display
	virtual void FASTCALL Update();
										// Message thread update
	virtual BOOL FASTCALL Save(Fileio *pFio, int nVer);
										// Save
	virtual BOOL FASTCALL Load(Fileio *pFio, int nVer);
										// Load
	virtual void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply configuration
#if !defined(NDEBUG)
	void AssertValid() const;
										// Diagnostics
#endif	// NDEBUG

	CSubWnd *m_pNextWnd;
										// Next window

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window create
	afx_msg void OnDestroy();
										// Window destroy
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Background draw
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// Activate
	void PostNcDestroy();
										// Window destruction
	CString m_strCaption;
										// Window caption
	BOOL m_bEnable;
										// Enable flag
	DWORD m_dwID;
										// Window ID
	CScheduler *m_pSch;
										// Scheduler (Win32)
	CDrawView *m_pDrawView;
										// Draw view
	CFont *m_pTextFont;
										// Text font
	int m_tmWidth;
										// Text width
	int m_tmHeight;
										// Text height
	int m_nWidth;
										// Window width (character units)
	int m_nHeight;
										// Window height (character units)
	BOOL m_bPopup;
										// Popup type

private:
	void FASTCALL SetupTextFont();
										// Text font setup

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Sub text window
//
//===========================================================================
class CSubTextWnd : public CSubWnd
{
public:
	CSubTextWnd();
										// Constructor
	void FASTCALL Refresh();
										// Refresh display

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window create
	afx_msg void OnDestroy();
										// Window destroy
	afx_msg void OnPaint();
										// Draw
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change
	virtual void FASTCALL OnDraw(CDC *pDC);
										// Draw callback
	virtual void FASTCALL Setup() = 0;
										// Setup
	void FASTCALL Clear();
										// Clear text buffer
	void FASTCALL SetChr(int x, int y, TCHAR ch);
										// Set character
	void FASTCALL SetString(int x, int y, LPCTSTR lpszText);
										// Set string (x, y specified)
	void FASTCALL Reverse(BOOL bReverse);
										// Reverse attribute
	void FASTCALL ReSize(int nWidth, int nHeight);
										// Resize
	BOOL m_bReverse;
										// Reverse flag
	BYTE *m_pTextBuf;
										// Text buffer
	BYTE *m_pDrawBuf;
										// Draw buffer

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Sub text size window
//
//===========================================================================
class CSubTextSizeWnd : public CSubTextWnd
{
public:
	CSubTextSizeWnd();
										// Constructor
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation preprocessing

protected:
	virtual void FASTCALL OnDraw(CDC *pDC);
										// Draw callback
};

//===========================================================================
//
//	Sub text scroll window
//
//===========================================================================
class CSubTextScrlWnd : public CSubTextSizeWnd
{
public:
	CSubTextScrlWnd();
										// Constructor
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation preprocessing

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
										// Window create
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Horizontal scroll
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Vertical scroll
	void FASTCALL SetupScrl();
										// Scroll setup
	virtual void FASTCALL SetupScrlH();
										// Scroll setup (horizontal)
	virtual void FASTCALL SetupScrlV();
										// Scroll setup (vertical)
	int m_ScrlWidth;
										// Virtual width (character units)
	int m_ScrlHeight;
										// Virtual height (character units)
	BOOL m_bScrlH;
										// Horizontal scroll flag
	BOOL m_bScrlV;
										// Vertical scroll flag
	int m_ScrlX;
										// Scroll offset (character units)
	int m_ScrlY;
										// Scroll offset (character units)

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Sub list window
//
//===========================================================================
class CSubListWnd : public CSubWnd
{
public:
	CSubListWnd();
										// Constructor
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation preprocessing
	virtual void FASTCALL Refresh() = 0;
										// Refresh

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
										// Window create
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change
	afx_msg void OnDrawItem(int nID, LPDRAWITEMSTRUCT lpDIS);
										// Owner draw
	virtual void FASTCALL InitList() = 0;
										// List control initialization
	CListCtrl m_ListCtrl;
										// List control

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Sub BMP window
//
//===========================================================================
class CSubBMPWnd : public CWnd
{
public:
	CSubBMPWnd();
										// Constructor

	BOOL FASTCALL Init(CDrawView* pDrawView);
	void FASTCALL Refresh();
	void FASTCALL Refresh(int nWidth, int nHeight);
										// Draw
#if !defined(NDEBUG)
	void AssertValid() const;
										// Diagnostics
#endif	// NDEBUG

	// Screen size
	void FASTCALL GetMaximumRect(LPRECT lpRect, BOOL bScroll);
										// Get maximum window rect
	void FASTCALL GetFitRect(LPRECT lpRect);
										// Get fit rect
	void FASTCALL GetDrawRect(LPRECT lpRect);
										// Get draw rect
	BYTE* FASTCALL GetBits() const;
										// Get bit pointer
	void FASTCALL DrawBits(CDC *pDC, CPoint point);
										// Draw

	int m_nScrlWidth;
										// Virtual width
	int m_nScrlHeight;
										// Virtual height
	int m_nScrlX;
										// Scroll offset
	int m_nScrlY;
										// Scroll offset
	int m_nCursorX;
										// Mouse cursor X
	int m_nCursorY;
										// Mouse cursor Y
	CDrawView* mx_pDrawView;

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window create
	afx_msg void OnDestroy();
										// Window destroy
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Background draw
	afx_msg void OnPaint();
										// Paint
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Horizontal scroll
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
										// Vertical scroll
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
										// Mouse move
	void PostNcDestroy();
										// Window destruction

	// Bitmap
	BITMAPINFOHEADER m_bmi;
										// Bitmap info header
	HANDLE m_hBitmap;
										// Bitmap handle
	BYTE *m_pBits;
										// Bitmap bits
	int m_nMul;
										// Multiplier (50% units)

	// Scroll
	void FASTCALL SetupScrlH();
										// Scroll setup (horizontal)
	void FASTCALL SetupScrlV();
										// Scroll setup (vertical)

	DECLARE_MESSAGE_MAP()
										// Message map
};

//===========================================================================
//
//	Sub bitmap window
//
//===========================================================================
class CSubBitmapWnd : public CSubWnd
{
public:
	CSubBitmapWnd();
										// Constructor
	BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation preprocessing
	void FASTCALL Refresh();
										// Refresh
	virtual void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr) = 0;
										// Setup

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window create
	afx_msg void OnSizing(UINT nSide, LPRECT lpRect);
										// Size change start
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Size change
	DWORD FASTCALL ConvPalette(WORD value);
										// Palette conversion
	CSubBMPWnd *m_pBMPWnd;
										// Bitmap window
	CStatusBar m_StatusBar;
										// Status bar
	int m_nScrlWidth;
										// Virtual width (pixel units)
	int m_nScrlHeight;
										// Virtual height (pixel units)

	DECLARE_MESSAGE_MAP()
										// Message map
};

#endif	// mfc_sub_h
#endif	// _WIN32
