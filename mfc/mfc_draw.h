//---------------------------------------------------------------------------
//
// X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
// [ MFC draw view ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_draw_h)
#define mfc_draw_h

#include "mfc_dx9.h"

//===========================================================================
//
// Draw view
//
//===========================================================================
class CDrawView : public CView
{
public:
	// Internal data definition
	typedef struct _DRAWINFO {
		BOOL bPower;					// Power state
		Render *pRender;				// Renderer
		Render::render_t *pWork;		// Renderer work buffer
		DWORD dwDrawCount;				// Draw count

		// DIB section
		HBITMAP hBitmap;				// DIB section
		DWORD *pBits;					// Bitmap data
		int nBMPWidth;					// BMP width
		int nBMPHeight;					// BMP height

		// Renderer communication
		int nRendWidth;					// Renderer width
		int nRendHeight;				// Renderer height
		int nRendHMul;					// Renderer horizontal scale
		int nRendVMul;					// Renderer vertical scale
		int nLeft;						// Left margin
		int nTop;						// Top margin
		int nWidth;						// BitBlt width
		int nHeight;					// BitBlt height

		// Blit-related
		int nBltTop;					// Draw start Y
		int nBltBottom;				// Draw end Y
		int nBltLeft;					// Draw start X
		int nBltRight;				// Draw end X
		int nScaleIndex;				// Selected scale index
		BOOL bBltAll;					// Full display flag
		BOOL bBltStretch;				// Stretch to match aspect ratio
	} DRAWINFO, *LPDRAWINFO;

public:
	// Basic methods
	CDrawView();
										// Constructor

	BOOL m_bShowOSD;					// OSD visibility
	volatile LONG m_lShaderEnabled;		// Shader state (volatile + atomic-safe)
	void FASTCALL Enable(BOOL bEnable);
										// Enable control
	BOOL FASTCALL IsEnable() const;
										// Get enable state
	BOOL FASTCALL Init(CWnd *pParent, BOOL bShaderEnabled);
										// Initialization
	BOOL PreCreateWindow(CREATESTRUCT& cs);
										// Window creation setup
	void FASTCALL Refresh();
										// Refresh drawing
	void FASTCALL Draw(int index);
										// Draw (specific window only)
	void FASTCALL Update();
										// Update from the message thread
	void FASTCALL RequestPresent();
										// Request a frame from the UI thread
	void FASTCALL SetVSync(BOOL bEnable);
										// Update VSync in the DX renderer
	void FASTCALL ApplyRendererConfig(int nRenderMode);
										// Apply renderer settings
	void FASTCALL ToggleRenderer();
										// Toggle between DX9 and GDI
	void FASTCALL ToggleShader();
										// Toggle CRT shader effect
	void FASTCALL SetShaderEnabled(BOOL bEnable);
										// Enable/disable shader
	BOOL FASTCALL IsShaderEnabled() const;
										// Get shader state
	BOOL FASTCALL IsDX9Active() const;
										// Check whether DX9 mode is active
	void FASTCALL ShowRenderStatusOSD(BOOL bVSync);
										// Show active renderer and VSync state
	void FASTCALL ApplyCfg(const Config *pConfig);
										// Apply settings
	void FASTCALL GetDrawInfo(LPDRAWINFO pDrawInfo) const;
										// Get drawing information

	// Rendering
	void OnDraw(CDC *pDC);
										// Draw
	void FASTCALL SetScaleIndex(int nScaleIndex);
										// Set window scale
	int FASTCALL GetScaleIndex() const;
										// Get window scale index
	int FASTCALL GetScalePercent() const;
										// Get window scale percent

	// Subwindow management
	int FASTCALL GetNewSWnd() const;
										// Get next subwindow index
	void FASTCALL AddSWnd(CSubWnd *pSubWnd);
										// Add subwindow (called from child)
	void FASTCALL DelSWnd(CSubWnd *pSubWnd);
										// Delete subwindow (called from child)
	void FASTCALL ClrSWnd();
										// Delete all subwindows
	CSubWnd* FASTCALL GetFirstSWnd() const;
										// Get first subwindow
	CSubWnd* FASTCALL SearchSWnd(DWORD dwID) const;
										// Get subwindow by arbitrary ID
	CFont* FASTCALL GetTextFont();
										// Get text font
	CSubWnd* FASTCALL NewWindow(BOOL bDis);
										// Create new window (Dis, Mem)
	BOOL FASTCALL IsNewWindow(BOOL bDis);
										// Check whether a new window can be created
	int FASTCALL GetSubWndNum() const;
										// Get number of subwindows
	LPCTSTR FASTCALL GetWndClassName() const;
										// Get window class name

protected:
	// WM messages
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Window creation
	afx_msg void OnDestroy();
										// Window destruction
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Window size change
	afx_msg void OnPaint();
										// Draw
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Background drawing
	afx_msg LRESULT OnDisplayChange(WPARAM wParam, LPARAM lParam);
										// Display change
	afx_msg LRESULT OnPresentFrame(WPARAM wParam, LPARAM lParam);
										// Asynchronous frame presentation
	afx_msg void OnDropFiles(HDROP hDropInfo);
										// File drop
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
										// Mouse wheel
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
										// Key pressed
	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
										// System key pressed
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
										// Key released
	afx_msg void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
										// System key released
	afx_msg void OnMove(int x, int y);
										// Window move

	BOOL m_bEnable;
										// Validity flag
	CFont m_TextFont;
										// Text font

private:
	void FASTCALL SetupBitmap();
	void FASTCALL FinishFrame();
	void FASTCALL DrawOSD(CDC *pDC);
	void FASTCALL ShowOSD(LPCTSTR lpszText);
										// Bitmap setup
	inline void FASTCALL ReCalc(CRect& rect);
										// Recalculate
	inline void FASTCALL DrawRect(CDC *pDC);
										// Draw surrounding border
	inline BOOL FASTCALL CalcRect();
										// Determine the required drawing area
	int FASTCALL MakeBits();
										// Bitmap creation
	BOOL FASTCALL KeyUpDown(UINT nChar, UINT nFlags, BOOL bDown);
										// Key handling
	CDX9Renderer m_DX9Renderer;
	BOOL m_bUseDX9;
	volatile LONG m_lPresentPending;

	// Variables for the dedicated rendering thread
	HANDLE m_hRenderEvent;
	HANDLE m_hRenderExitEvent;
	HANDLE m_hRenderAckEvent;
	CWinThread *m_pRenderThread;
	volatile LONG m_lRenderCmd;
	volatile LONG m_lPendingShaderEnable;	// -1: no change, 0: disable, 1: enable
	volatile LONG m_lScaleIndex;
	int m_nRenderWidth;
	int m_nRenderHeight;
	BOOL m_bRenderVSync;

	static UINT RenderThreadFunc(LPVOID pParam);
	void FASTCALL RenderLoop();

	DWORD *m_pStagingBuffer;
	int m_nStagingWidth;
	int m_nStagingHeight;
	DWORD m_dwOSDUntil;

	DWORD m_dwPerfOSDLastTick;
	int m_nPerfFPS;
	TCHAR m_szPerfLine[96];
	TCHAR m_szOSDText[64];
	CSubWnd *m_pSubWnd;
										// First subwindow
	CFrmWnd *m_pFrmWnd;
										// Frame window
	CScheduler *m_pScheduler;
										// Scheduler
	CInput *m_pInput;
										// Input
	DRAWINFO m_Info;
										// Internal data

	DECLARE_MESSAGE_MAP()
										// With message map
};

#endif	// mfc_draw_h
#endif	// _WIN32
