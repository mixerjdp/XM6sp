//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Version Information Dialog ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_ver_h)
#define mfc_ver_h

//===========================================================================
//
//	Version information dialog
//
//===========================================================================
class CAboutDlg : public CDialog
{
public:
	CAboutDlg(CWnd *pParent = NULL);
										// Constructor
	BOOL OnInitDialog();
										// Dialog initialization
	void OnOK();
										// OK
	void OnCancel();
										// Cancel

protected:
	afx_msg void OnPaint();
										// Paint
#if _MFC_VER >= 0x800
	afx_msg LRESULT OnNcHitTest(CPoint point);
										// Hit test
#else
	afx_msg UINT OnNcHitTest(CPoint point);
										// Hit test
#endif	// _MFC_VER
	afx_msg BOOL OnSetCursor(CWnd *pWnd, UINT nHitTest, UINT message);
										// Set cursor
#if _MFC_VER >= 0x700
	afx_msg void OnTimer(UINT_PTR nTimerID);
										// Timer
#else
	afx_msg void OnTimer(UINT nTimerID);
										// Timer
#endif	// _MFC_VER

private:
	void FASTCALL DrawURL(CDC *pDC);
										// URL draw
	void FASTCALL DrawCRT(CDC *pDC);
										// CRT draw
	void FASTCALL DrawX68k(CDC *pDC);
										// X68k draw
	void FASTCALL DrawLED(int x, int y, CDC *pDC);
										// LED draw
	void FASTCALL DrawView(int x, int y, CDC *pDC);
										// View draw
	CString m_URLString;
										// URL string
	CRect m_URLRect;
										// URL rectangle
	BOOL m_bURLHit;
										// URL hit flag
	CRect m_IconRect;
										// Icon rectangle
#if _MFC_VER >= 0x700
	UINT_PTR m_nTimerID;
										// Timer ID
#else
	UINT m_nTimerID;
										// Timer ID
#endif
	RTC *m_pRTC;
										// RTC
	SASI *m_pSASI;
										// SASI
	FDD *m_pFDD;
										// FDD
	CDrawView *m_pDrawView;
										// Draw view
	BOOL m_bFloppyLED;
										// Floppy LED mode
	BOOL m_bPowerLED;
										// Power LED mode

	DECLARE_MESSAGE_MAP()
										// Message map declaration
};

#endif	// mfc_ver_h
#endif	// _WIN32