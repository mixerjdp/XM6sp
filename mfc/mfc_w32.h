//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ Subventana MFC (Win32) ]
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
//	Ventana de Componentes
//
//===========================================================================
class CComponentWnd : public CSubTextWnd
{
public:
	CComponentWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	CComponent *m_pComponent;
										// Primer componente
};

//===========================================================================
//
//	Ventana de información del SO
//
//===========================================================================
class COSInfoWnd : public CSubTextWnd
{
public:
	COSInfoWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración
};

//===========================================================================
//
//	Ventana de sonido
//
//===========================================================================
class CSoundWnd : public CSubTextWnd
{
public:
	CSoundWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	Scheduler *m_pScheduler;
										// Planificador
	OPMIF *m_pOPMIF;
										// OPM
	ADPCM *m_pADPCM;
										// ADPCM
	CSound *m_pSound;
										// Componente de sonido
};

//===========================================================================
//
//	Ventana de entrada
//
//===========================================================================
class CInputWnd : public CSubTextWnd
{
public:
	CInputWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	void FASTCALL SetupInput(int x, int y);
										// Configuración (sistema de entrada completo)
	void FASTCALL SetupMouse(int x, int y);
										// Configuración (ratón)
	void FASTCALL SetupKey(int x, int y);
										// Configuración (teclado)
	void FASTCALL SetupJoy(int x, int y, int nJoy);
										// Configuración (joystick)
	CInput *m_pInput;
										// Componente de entrada
};

//===========================================================================
//
//	Ventana de puertos
//
//===========================================================================
class CPortWnd : public CSubTextWnd
{
public:
	CPortWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	CPort *m_pPort;
										// Componente de puerto
};

//===========================================================================
//
//	Ventana de mapa de bits
//
//===========================================================================
class CBitmapWnd : public CSubTextWnd
{
public:
	CBitmapWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	CDrawView *m_pView;
										// Ventana de dibujo
};

//===========================================================================
//
//	Ventana del controlador MIDI
//
//===========================================================================
class CMIDIDrvWnd : public CSubTextWnd
{
public:
	CMIDIDrvWnd();
										// Constructor
	void FASTCALL Setup();
										// Configuración

private:
	void FASTCALL SetupInfo(int x, int y, CMIDI::LPMIDIINFO pInfo);
										// Configuración (sub)
	void FASTCALL SetupExCnt(int x, int y, DWORD dwStart, DWORD dwEnd);
										// Configuración (contador exclusivo)
	static LPCTSTR DescTable[];
										// Tabla de cadenas
	MIDI *m_pMIDI;
										// MIDI
	CMIDI *m_pMIDIDrv;
										// Controlador MIDI
};

//===========================================================================
//
//	Ventana de visualización de teclado
//
//===========================================================================
class CKeyDispWnd : public CWnd
{
public:
	CKeyDispWnd();
										// Constructor
	void PostNcDestroy();
										// Eliminación de ventana completada
	void FASTCALL SetShiftMode(UINT nMode);
										// Configuración del modo shift
	void FASTCALL Refresh(const BOOL *m_pKeyBuf);
										// Actualización de teclas
	void FASTCALL SetKey(const BOOL *m_pKeyBuf);
										// Configuración de teclas por lotes

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Creación de ventana
	afx_msg void OnDestroy(void);
										// Eliminación de ventana
	afx_msg void OnSize(UINT nType, int cx, int cy);
										// Cambio de tamaño de ventana
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
										// Dibujo de fondo
	afx_msg void OnPaint();
										// Redibujado de ventana
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
										// Botón izquierdo presionado
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
										// Botón izquierdo soltado
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
										// Botón derecho presionado
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
										// Botón derecho soltado
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
										// Movimiento del ratón
	afx_msg UINT OnGetDlgCode();
										// Obtención de código de diálogo

private:
	void FASTCALL SetupBitmap();
										// Preparación de mapa de bits
	void FASTCALL OnDraw(CDC *pDC);
										// Dibujo de ventana
	LPCTSTR FASTCALL GetKeyString(int nKey);
										// Obtención de cadena de tecla
	int FASTCALL PtInKey(CPoint& point);
										// Obtención de código de tecla en rectángulo
	void FASTCALL DrawKey(int nKey, BOOL bDown);
										// Visualización de tecla
	void FASTCALL DrawBox(int nColorOut, int nColorIn, RECT& rect);
										// Dibujo de caja de tecla
	void FASTCALL DrawCRBox(int nColorOut, int nColorIn, RECT& rect);
										// Dibujo de caja de tecla CR
	void FASTCALL DrawChar(int x, int y, int nColor, DWORD dwChar);
										// Dibujo de carácter
	void FASTCALL DrawCRChar(int x, int y, int nColor);
										// Dibujo de carácter CR
	int FASTCALL CalcCGAddr(DWORD dwChar);
										// Cálculo de dirección CGROM de ancho completo
	UINT m_nMode;
										// Modo SHIFT
	UINT m_nKey[0x80];
										// Estado de tecla (visualización)
	BOOL m_bKey[0x80];
										// Estado de tecla (final)
	int m_nPoint;
										// Punto de movimiento del ratón
	const BYTE* m_pCG;
										// CGROM
	HBITMAP m_hBitmap;
										// Manejador de mapa de bits
	BYTE *m_pBits;
										// Bits de mapa de bits
	UINT m_nBMPWidth;
										// Ancho de mapa de bits
	UINT m_nBMPHeight;
										// Altura de mapa de bits
	UINT m_nBMPMul;
										// Ancho de multiplicación de mapa de bits
	static RGBQUAD PalTable[0x10];
										// Tabla de paleta
	static const RECT RectTable[0x75];
										// Tabla de rectángulos
	static LPCTSTR NormalTable[];
										// Tabla de cadenas
	static LPCTSTR KanaTable[];
										// Tabla de cadenas
	static LPCTSTR KanaShiftTable[];
										// Tabla de cadenas
	static LPCTSTR MarkTable[];
										// Tabla de cadenas
	static LPCTSTR AnotherTable[];
										// Tabla de cadenas

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

//===========================================================================
//
//	Ventana de teclado de software
//
//===========================================================================
class CSoftKeyWnd : public CSubWnd
{
public:
	CSoftKeyWnd();
										// Constructor
	void FASTCALL Refresh();
										// Actualizar

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
										// Creación de ventana
	afx_msg void OnDestroy();
										// Eliminación de ventana
	afx_msg void OnActivate(UINT nState, CWnd *pWnd, BOOL bMinimized);
										// Activar
	afx_msg LONG OnApp(UINT uParam, LONG lParam);
										// Usuario (notificación de ventana inferior)

private:
	void FASTCALL Analyze(Keyboard::keyboard_t *pKbd);
										// Análisis de datos de teclado
	Keyboard *m_pKeyboard;
										// Teclado
	CInput *m_pInput;
										// Entrada
	CStatusBar m_StatusBar;
										// Barra de estado
	CKeyDispWnd *m_pDispWnd;
										// Ventana de visualización de teclas
	UINT m_nSoftKey;
										// Tecla de software presionada

	DECLARE_MESSAGE_MAP()
										// Con mapa de mensajes
};

#endif	// mfc_w32_h
#endif	// _WIN32
