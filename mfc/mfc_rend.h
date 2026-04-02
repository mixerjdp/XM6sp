//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFC Subwindow (render) ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_rend_h)
#define mfc_rend_h

#include "mfc_sub.h"

//===========================================================================
//
//	Render buffer window
//
//===========================================================================
class CRendBufWnd : public CSubBitmapWnd
{
public:
	CRendBufWnd(int nType);
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	int m_nType;
										// Type
	const DWORD *m_pRendBuf;
										// Text buffer
};

//===========================================================================
//
//	Composite buffer window
//
//===========================================================================
class CMixBufWnd : public CSubBitmapWnd
{
public:
	CMixBufWnd();
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	const Render::render_t *m_pRendWork;
										// Renderer work
};

//===========================================================================
//
//	PCG buffer window
//
//===========================================================================
class CPCGBufWnd : public CSubBitmapWnd
{
public:
	CPCGBufWnd();
										// Constructor
	void FASTCALL Setup(int x, int y, int width, int height, BYTE *ptr);
										// Setup
	void FASTCALL Update();
										// Update from message thread

private:
	const DWORD *m_pPCGBuf;
										// PCG buffer address
	const DWORD *m_dwPCGBuf;
										// PCG buffer usage counter
};

#endif	// mfc_rend_h
#endif	// _WIN32
