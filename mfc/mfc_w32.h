//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI. (ytanaka@ipc-tokai.or.jp)
//	[ MFC Sub-window (Win32) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_w32_h)
#define mfc_w32_h

#include "keyboard.h"
#include "mfc_sub.h"
#include "mfc_midi.h"

//===========================================================================
//
//	Components Window
//
//===========================================================================
class CComponentWnd : public CSubTextWnd
{
public:
	CComponentWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CComponent *m_pComponent;
										// First component
};

//===========================================================================
//
//	OS Information Window
//
//===========================================================================
class COSInfoWnd : public CSubTextWnd
{
public:
	COSInfoWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration
};

//===========================================================================
//
//	Sound Window
//
//===========================================================================
class CSoundWnd : public CSubTextWnd
{
public:
	CSoundWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	Scheduler *m_pScheduler;
										// Scheduler
	OPMIF *m_pOPMIF;
										// OPM
	ADPCM *m_pADPCM;
										// ADPCM
	CSound *m_pSound;
										// Sound component
};

//===========================================================================
//
//	Input Window
//
//===========================================================================
class CInputWnd : public CSubTextWnd
{
public:
	CInputWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	void FASTCALL SetupInput(int x, int y);
                                        // Configuration (full input system)
	void FASTCALL SetupMouse(int x, int y);
                                        // Configuration (mouse)
	void FASTCALL SetupKey(int x, int y);
                                        // Configuration (keyboard)
	void FASTCALL SetupJoy(int x, int y, int nJoy);
                                        // Configuration (joystick)
	CInput *m_pInput;
										// Input component
};

//===========================================================================
//
//	Ports Window
//
//===========================================================================
class CPortWnd : public CSubTextWnd
{
public:
	CPortWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CPort *m_pPort;
										// Port component
};

//===========================================================================
//
//	Bitmap Window
//
//===========================================================================
class CBitmapWnd : public CSubTextWnd
{
public:
	CBitmapWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	CDrawView *m_pView;
										// Drawing window
};

//===========================================================================
//
//	MIDI controller window
//
//===========================================================================
class CMIDIDrvWnd : public CSubTextWnd
{
public:
	CMIDIDrvWnd();
										// Constructor
	void FASTCALL Setup();
                                        // Configuration

private:
	void FASTCALL SetupInfo(int x, int y, CMIDI::LPMIDIINFO pInfo);
                                        // Configuration (sub)
	void FASTCALL SetupExCnt(int x, int y, DWORD dwStart, DWORD dwEnd);
                                        // Configuration (exclusive counter)
	static LPCTSTR DescTable[];
										// String table
	MIDI *m_pMIDI;
										// MIDI
	CMIDI *m_pMIDIDrv;
										// Controller MIDI
};

//===========================================================================
//
//  Keyboard display window
//
//===========================================================================
class CKeyDispWnd : public CWnd
{
public:
	CKeyDispWnd();
										// Constructor
	void PostNcDestroy();
                                        // Window destruction completed
	void FASTCALL SetShiftMode(UINT nMode);
                                        // Shift mode configuration
	void FASTCALL Refresh(const BOOL *m_pKeyBuf);
                                        // Key update
	void FASTCALL SetKey(const BOOL *m_pKeyBuf);
                                        // Batch key configuration

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
                                        // Window creation
	afx_msg void OnDestroy(void);
                                        // Window destruction
	afx_msg void OnSize(UINT nType, int cx, int cy);
                                        // Window resize
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Draw background
	afx_msg void OnPaint();
                                        // Redraw window
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
										// Left button pressed
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
										// Left button released
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Right button pressed
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
										// Right button released
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
                                        // Mouse movement
	afx_msg UINT OnGetDlgCode();
										// Get dialog code

private:
	void FASTCALL SetupBitmap();
										// Prepare bitmap
	void FASTCALL OnDraw(CDC *pDC);
                                        // Window drawing
	LPCTSTR FASTCALL GetKeyString(int nKey);
                                        // Get key string
	int FASTCALL PtInKey(CPoint& point);
                                        // Get key code in rectangle
	void FASTCALL DrawKey(int nKey, BOOL bDown);
                                        // Key display
	void FASTCALL DrawBox(int nColorOut, int nColorIn, RECT& rect);
                                        // Key box drawing
	void FASTCALL DrawCRBox(int nColorOut, int nColorIn, RECT& rect);
                                        // Key box drawing CR
	void FASTCALL DrawChar(int x, int y, int nColor, DWORD dwChar);
										// Draw character
	void FASTCALL DrawCRChar(int x, int y, int nColor);
										// Draw character CR
	int FASTCALL CalcCGAddr(DWORD dwChar);
										// Calculate full-width CGROM address
	UINT m_nMode;
										// SHIFT mode
	UINT m_nKey[0x80];
                                        // Key state (display)
	BOOL m_bKey[0x80];
                                        // Key state (final)
	int m_nPoint;
                                        // Mouse cursor point
	const BYTE* m_pCG;
										// CGROM
	HBITMAP m_hBitmap;
										// Bitmap handle
	BYTE *m_pBits;
										// Bitmap bits
	UINT m_nBMPWidth;
										// Bitmap width
	UINT m_nBMPHeight;
										// Bitmap height
	UINT m_nBMPMul;
										// Bitmap multiplication width
	static RGBQUAD PalTable[0x10];
										// Palette table
	static const RECT RectTable[0x75];
										// Rectangle table
	static LPCTSTR NormalTable[];
										// String table
	static LPCTSTR KanaTable[];
										// String table
	static LPCTSTR KanaShiftTable[];
										// String table
	static LPCTSTR MarkTable[];
										// String table
	static LPCTSTR AnotherTable[];
										// String table

	DECLARE_MESSAGE_MAP()
										// With message map
};

//===========================================================================
//
//  Software keyboard window
//
//===========================================================================
class CSoftKeyWnd : public CSubWnd
{
public:
	CSoftKeyWnd();
										// Constructor
	void FASTCALL Refresh();
										// Update

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
                                        // Window creation
	afx_msg void OnDestroy();
                                        // Window destruction
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// Activate
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
                                        // User (lower window notification)

private:
	void FASTCALL Analyze(Keyboard::keyboard_t *pKbd);
                                        // Keyboard data analysis
	Keyboard *m_pKeyboard;
										// Keyboard
	CInput *m_pInput;
										// Input
	CStatusBar m_StatusBar;
                                        // Status bar
	CKeyDispWnd *m_pDispWnd;
//  Key display window
	UINT m_nSoftKey;
										// Software key pressed

	DECLARE_MESSAGE_MAP()
										// With message map
};

#endif	// mfc_w32_h
#endif	// _WIN32