//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
//	[MFC subwindow (Devices)]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_dev_h)
#define mfc_dev_h

#include "mfp.h"
#include "dmac.h"
#include "scc.h"
#include "fdc.h"
#include "midi.h"
#include "sasi.h"
#include "scsi.h"
#include "mfc_sub.h"

//===========================================================================
//
//	MFP window
//
//===========================================================================
class CMFPWnd : public CSubTextWnd
{
public:
	CMFPWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupInt(int x, int y);
										// Configuration (Interrupt)
	void FASTCALL SetupGPIP(int x, int y);
										// Configuration (GPIP)
	void FASTCALL SetupTimer(int x, int y);
										// Configuration (Timer)
	static LPCTSTR DescInt[];
										// Interrupt table
	static LPCTSTR DescGPIP[];
										// GPIP table
	static LPCTSTR DescTimer[];
										// Timer table
	MFP *m_pMFP;
										// MFP
	MFP::mfp_t m_mfp;
										// MFP internal data
};

//===========================================================================
//
//	DMAC window
//
//===========================================================================
class CDMACWnd : public CSubTextWnd
{
public:
	CDMACWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupCh(int nCh, DMAC::dma_t *pDMA, LPCTSTR lpszTitle);
										// Configuration (Channel)
	DMAC *m_pDMAC;
										// DMAC
};

//===========================================================================
//
//	CRTC window
//
//===========================================================================
class CCRTCWnd : public CSubTextWnd
{
public:
	CCRTCWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	CRTC *m_pCRTC;
										// CRTC
};

//===========================================================================
//
//	VC window
//
//===========================================================================
class CVCWnd : public CSubTextWnd
{
public:
	CVCWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	VC *m_pVC;
										// VC
};

//===========================================================================
//
//	RTC window
//
//===========================================================================
class CRTCWnd : public CSubTextWnd
{
public:
	CRTCWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	RTC* m_pRTC;
										// FDC
};

//===========================================================================
//
//	OPM window
//
//===========================================================================
class COPMWnd : public CSubTextWnd
{
public:
	COPMWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	OPMIF *m_pOPM;
										// OPM
};

//===========================================================================
//
//	Keyboard window
//
//===========================================================================
class CKeyboardWnd : public CSubTextWnd
{
public:
	CKeyboardWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	static LPCTSTR DescLED[];
										// LED table
	Keyboard *m_pKeyboard;
										// Keyboard
};

//===========================================================================
//
//	FDD window
//
//===========================================================================
class CFDDWnd : public CSubTextWnd
{
public:
	CFDDWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupFDD(int nDrive, int x);
										// Sub configuration
	BOOL FASTCALL SetupTrack();
										// Configuration of pista
	static LPCTSTR DescTable[];
										// Description table
	FDD *m_pFDD;
										// FDD
	FDC *m_pFDC;
										// FDC
	DWORD m_dwDrive;
										// Access unit
	DWORD m_dwHD;
										// Access head
	DWORD m_CHRN[4];
										// Access CHRN
};

//===========================================================================
//
//	FDC window
//
//===========================================================================
class CFDCWnd : public CSubTextWnd
{
public:
	CFDCWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupGeneral(int x, int y);
										// Configuration (General)
	void FASTCALL SetupParam(int x, int y);
										// Configuration (Parameter)
	void FASTCALL SetupSR(int x, int y);
										// Configuration (Status register)
	static LPCTSTR SRDesc[8];
										// String (status register)
	void FASTCALL SetupST0(int x, int y);
										// Configuration (ST0)
	static LPCTSTR ST0Desc[8];
										// String (ST0)
	void FASTCALL SetupST1(int x, int y);
										// Configuration (ST1)
	static LPCTSTR ST1Desc[8];
										// String (ST1)
	void FASTCALL SetupST2(int x, int y);
										// Configuration (ST2)
	static LPCTSTR ST2Desc[8];
										// String (ST2)
	void FASTCALL SetupSub(int x, int y, LPCTSTR lpszTitle, LPCTSTR *lpszDesc,
					DWORD data);		// Configuration (Sub)
	FDC *m_pFDC;
										// FDC
	const FDC::fdc_t *m_pWork;
										// FDC internal data
};

//===========================================================================
//
//	SCC window
//
//===========================================================================
class CSCCWnd : public CSubTextWnd
{
public:
	CSCCWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupSCC(SCC::ch_t *pCh, int x, int y);
										// Configuration (Channel)
	SCC *m_pSCC;
										// SCC
	static LPCTSTR DescTable[];
										// String table
};

//===========================================================================
//
//	Cynthia window
//
//===========================================================================
class CCynthiaWnd : public CSubTextWnd
{
public:
	CCynthiaWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	Sprite *m_pSprite;
										// CYNTHIA
};

//===========================================================================
//
//	SASI window
//
//===========================================================================
class CSASIWnd : public CSubTextWnd
{
public:
	CSASIWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupCmd(int x, int y);
										// Configuration (Command)
	void FASTCALL SetupCtrl(int x, int y);
										// Configuration (Controller)
	void FASTCALL SetupDrive(int x, int y);
										// Configuration (Unit)
	void FASTCALL SetupCache(int x, int y);
										// Configuration (Cache)
	SASI *m_pSASI;
										// SASI
	SASI::sasi_t m_sasi;
										// Data internos SASI
};

//===========================================================================
//
//	MIDI window
//
//===========================================================================
class CMIDIWnd : public CSubTextWnd
{
public:
	CMIDIWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupCtrl(int x, int y);
										// Configuration (Control)
	static LPCTSTR DescCtrl[];
										// String table (Control)
	void FASTCALL SetupInt(int x, int y);
										// Configuration (Interrupt)
	static LPCTSTR DescInt[];
										// String table (Interrupt)
	void FASTCALL SetupTrans(int x, int y);
										// Configuration (Transmission)
	static LPCTSTR DescTrans[];
										// String table (Transmission)
	void FASTCALL SetupRecv(int x, int y);
										// Configuration (Reception)
	static LPCTSTR DescRecv[];
										// String table (Reception)
	void FASTCALL SetupRT(int x, int y);
										// Configuration (Real-time transmission)
	static LPCTSTR DescRT[];
										// String table (Real-time transmission)
	void FASTCALL SetupRR(int x, int y);
										// Configuration (Real-time reception)
	static LPCTSTR DescRR[];
										// String table (Real-time reception)
	void FASTCALL SetupCount(int x, int y);
										// Configuration (Counter)
	static LPCTSTR DescCount[];
										// String table (Counter)
	void FASTCALL SetupHunter(int x, int y);
										// Configuration (Address Hunter)
	static LPCTSTR DescHunter[];
										// String table (Address Hunter)
	void FASTCALL SetupFSK(int x, int y);
										// Configuration (FSK)
	static LPCTSTR DescFSK[];
										// String table (FSK)
	void FASTCALL SetupGPIO(int x, int y);
										// Configuration (GPIO)
	static LPCTSTR DescGPIO[];
										// String table (GPIO)

	MIDI *m_pMIDI;
										// MIDI
	MIDI::midi_t m_midi;
										// MIDI internal data
};

//===========================================================================
//
//	SCSI window
//
//===========================================================================
class CSCSIWnd : public CSubTextWnd
{
public:
	CSCSIWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuration

private:
	void FASTCALL SetupCmd(int x, int y);
										// Configuration (Command)
	void FASTCALL SetupCtrl(int x, int y);
										// Configuration (Controller)
	void FASTCALL SetupDrive(int x, int y);
										// Configuration (Unit)
	void FASTCALL SetupReg(int x, int y);
										// Configuration (Register)
	void FASTCALL SetupCDB(int x, int y);
										// Configuration (CDB)
	SCSI *m_pSCSI;
										// SCSI
	SCSI::scsi_t m_scsi;
										// SCSI internal data
};

#endif	// mfc_dev_h
#endif	// _WIN32
