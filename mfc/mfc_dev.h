  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Subventana MFC (Dispositivo) ]
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
  //	Ventana MFP
  //
  //===========================================================================
class CMFPWnd : public CSubTextWnd
{
public:
	CMFPWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupInt(int x, int y);
 										 // Configuracion (Interrupcion)
	void FASTCALL SetupGPIP(int x, int y);
 										 // Configuracion (GPIP)
	void FASTCALL SetupTimer(int x, int y);
 										 // Configuracion (Temporizador)
	static LPCTSTR DescInt[];
 										 // Tabla de interrupciones
	static LPCTSTR DescGPIP[];
 										 // Tabla GPIP
	static LPCTSTR DescTimer[];
 										 // Tabla de temporizador
	MFP *m_pMFP;
 										 // MFP
	MFP::mfp_t m_mfp;
 										 // Datos internos MFP
};

  //===========================================================================
  //
  //	Ventana DMAC
  //
  //===========================================================================
class CDMACWnd : public CSubTextWnd
{
public:
	CDMACWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupCh(int nCh, DMAC::dma_t *pDMA, LPCTSTR lpszTitle);
 										 // Configuracion (Canal)
	DMAC *m_pDMAC;
 										 // DMAC
};

  //===========================================================================
  //
  //	Ventana CRTC
  //
  //===========================================================================
class CCRTCWnd : public CSubTextWnd
{
public:
	CCRTCWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	CRTC *m_pCRTC;
 										 // CRTC
};

  //===========================================================================
  //
  //	Ventana VC
  //
  //===========================================================================
class CVCWnd : public CSubTextWnd
{
public:
	CVCWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	VC *m_pVC;
 										 // VC
};

  //===========================================================================
  //
  //	Ventana RTC
  //
  //===========================================================================
class CRTCWnd : public CSubTextWnd
{
public:
	CRTCWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	RTC* m_pRTC;
 										 // FDC
};

  //===========================================================================
  //
  //	Ventana OPM
  //
  //===========================================================================
class COPMWnd : public CSubTextWnd
{
public:
	COPMWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	OPMIF *m_pOPM;
 										 // OPM
};

  //===========================================================================
  //
  //	Ventana de teclado
  //
  //===========================================================================
class CKeyboardWnd : public CSubTextWnd
{
public:
	CKeyboardWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	static LPCTSTR DescLED[];
 										 // Tabla de LED
	Keyboard *m_pKeyboard;
 										 // Teclado
};

  //===========================================================================
  //
  //	Ventana FDD
  //
  //===========================================================================
class CFDDWnd : public CSubTextWnd
{
public:
	CFDDWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupFDD(int nDrive, int x);
 										 // Configuracion sub
	BOOL FASTCALL SetupTrack();
 										 // Configuracion de pista
	static LPCTSTR DescTable[];
 										 // Tabla de descripcion
	FDD *m_pFDD;
 										 // FDD
	FDC *m_pFDC;
 										 // FDC
	DWORD m_dwDrive;
 										 // Unidad de acceso
	DWORD m_dwHD;
 										 // Cabezal de acceso
	DWORD m_CHRN[4];
 										 // CHRN de acceso
};

  //===========================================================================
  //
  //	Ventana FDC
  //
  //===========================================================================
class CFDCWnd : public CSubTextWnd
{
public:
	CFDCWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupGeneral(int x, int y);
 										 // Configuracion (General)
	void FASTCALL SetupParam(int x, int y);
 										 // Configuracion (Parametro)
	void FASTCALL SetupSR(int x, int y);
 										 // Configuracion (Registro de estado)
	static LPCTSTR SRDesc[8];
 										 // Cadena (Registro de estado)
	void FASTCALL SetupST0(int x, int y);
 										 // Configuracion (ST0)
	static LPCTSTR ST0Desc[8];
 										 // Cadena (ST0)
	void FASTCALL SetupST1(int x, int y);
 										 // Configuracion (ST1)
	static LPCTSTR ST1Desc[8];
 										 // Cadena (ST1)
	void FASTCALL SetupST2(int x, int y);
 										 // Configuracion (ST2)
	static LPCTSTR ST2Desc[8];
 										 // Cadena (ST2)
	void FASTCALL SetupSub(int x, int y, LPCTSTR lpszTitle, LPCTSTR *lpszDesc,
 					DWORD data);		 // Configuracion (Sub)
	FDC *m_pFDC;
 										 // FDC
	const FDC::fdc_t *m_pWork;
 										 // Datos internos FDC
};

  //===========================================================================
  //
  //	Ventana SCC
  //
  //===========================================================================
class CSCCWnd : public CSubTextWnd
{
public:
	CSCCWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupSCC(SCC::ch_t *pCh, int x, int y);
 										 // Configuracion (Canal)
	SCC *m_pSCC;
 										 // SCC
	static LPCTSTR DescTable[];
 										 // Tabla de cadenas
};

  //===========================================================================
  //
  //	Ventana Cynthia
  //
  //===========================================================================
class CCynthiaWnd : public CSubTextWnd
{
public:
	CCynthiaWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	Sprite *m_pSprite;
 										 // CYNTHIA
};

  //===========================================================================
  //
  //	Ventana SASI
  //
  //===========================================================================
class CSASIWnd : public CSubTextWnd
{
public:
	CSASIWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupCmd(int x, int y);
 										 // Configuracion (Comando)
	void FASTCALL SetupCtrl(int x, int y);
 										 // Configuracion (Controlador)
	void FASTCALL SetupDrive(int x, int y);
 										 // Configuracion (Unidad)
	void FASTCALL SetupCache(int x, int y);
 										 // Configuracion (Cache)
	SASI *m_pSASI;
 										 // SASI
	SASI::sasi_t m_sasi;
 										 // Datos internos SASI
};

  //===========================================================================
  //
  //	Ventana MIDI
  //
  //===========================================================================
class CMIDIWnd : public CSubTextWnd
{
public:
	CMIDIWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupCtrl(int x, int y);
 										 // Configuracion (Control)
	static LPCTSTR DescCtrl[];
 										 // Tabla de cadenas (Control)
	void FASTCALL SetupInt(int x, int y);
 										 // Configuracion (Interrupcion)
	static LPCTSTR DescInt[];
 										 // Tabla de cadenas (Interrupcion)
	void FASTCALL SetupTrans(int x, int y);
 										 // Configuracion (Transmision)
	static LPCTSTR DescTrans[];
 										 // Tabla de cadenas (Transmision)
	void FASTCALL SetupRecv(int x, int y);
 										 // Configuracion (Recepcion)
	static LPCTSTR DescRecv[];
 										 // Tabla de cadenas (Recepcion)
	void FASTCALL SetupRT(int x, int y);
 										 // Configuracion (Transmision en tiempo real)
	static LPCTSTR DescRT[];
 										 // Tabla de cadenas (Transmision en tiempo real)
	void FASTCALL SetupRR(int x, int y);
 										 // Configuracion (Recepcion en tiempo real)
	static LPCTSTR DescRR[];
 										 // Tabla de cadenas (Recepcion en tiempo real)
	void FASTCALL SetupCount(int x, int y);
 										 // Configuracion (Contador)
	static LPCTSTR DescCount[];
 										 // Tabla de cadenas (Contador)
	void FASTCALL SetupHunter(int x, int y);
 										 // Configuracion (Address Hunter)
	static LPCTSTR DescHunter[];
 										 // Tabla de cadenas (Address Hunter)
	void FASTCALL SetupFSK(int x, int y);
 										 // Configuracion (FSK)
	static LPCTSTR DescFSK[];
 										 // Tabla de cadenas (FSK)
	void FASTCALL SetupGPIO(int x, int y);
 										 // Configuracion (GPIO)
	static LPCTSTR DescGPIO[];
 										 // Tabla de cadenas (GPIO)

	MIDI *m_pMIDI;
 										 // MIDI
	MIDI::midi_t m_midi;
 										 // Datos internos MIDI
};

  //===========================================================================
  //
  //	Ventana SCSI
  //
  //===========================================================================
class CSCSIWnd : public CSubTextWnd
{
public:
	CSCSIWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

private:
	void FASTCALL SetupCmd(int x, int y);
 										 // Configuracion (Comando)
	void FASTCALL SetupCtrl(int x, int y);
 										 // Configuracion (Controlador)
	void FASTCALL SetupDrive(int x, int y);
 										 // Configuracion (Unidad)
	void FASTCALL SetupReg(int x, int y);
 										 // Configuracion (Registro)
	void FASTCALL SetupCDB(int x, int y);
 										 // Configuracion (CDB)
	SCSI *m_pSCSI;
 										 // SCSI
	SCSI::scsi_t m_scsi;
 										 // Datos internos SCSI
};

 #endif	 // mfc_dev_h
 #endif	 // _WIN32
