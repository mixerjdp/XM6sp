//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
//	[ MFC status view ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined (mfc_stat_h)
#define mfc_stat_h

//===========================================================================
//
//	Status view
//
//===========================================================================
class CStatusView : public CWnd
{
public:
	CStatusView();
										// Constructor
	BOOL FASTCALL Init(CFrmWnd *pFrmWnd);
										// Initialization
	void FASTCALL SetCaptionString(CString& strCap);
										// Caption string specification
	void FASTCALL SetMenuString(CString& strMenu);
										// Menu string specification
	void FASTCALL SetInfoString(CString& strInfo);
										// Info string specification
	void FASTCALL DrawStatus(int nPane);
										// Status draw

protected:
	void PostNcDestroy();
										// Window destruction
	afx_msg void OnPaint();
										// Window paint
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Window background paint
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Window size change
	afx_msg void OnMove(int x, int y);
										// Window position change

private:
	// General
	CFrmWnd *m_pFrmWnd;
										// Frame window
	CRect m_rectClient;
										// Client area
	LONG m_tmWidth;
										// Text metrics width
	LONG m_tmHeight;
										// Text metrics height

	// Messages
	void FASTCALL DrawMessage(CDC *pDC) const;
										// Message draw
	CString m_strIdle;
										// Idle string
	CString m_strCaption;
										// Caption string
	CString m_strMenu;
										// Menu string
	CString m_strInfo;
										// Info string

	// Status
	CRect m_rectFDD[2];
										// FDD rect
	CRect m_rectLED[3];
										// LED rect

	DECLARE_MESSAGE_MAP()
										// Message map
};

#endif	// mfc_stat_h
#endif	// _WIN32
