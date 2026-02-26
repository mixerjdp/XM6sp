  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Subventana MFC (CPU) ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_cpu_h)
#define mfc_cpu_h

#include "mfc_sub.h"

  //===========================================================================
  //
  //	Dialogo con historial
  //
  //===========================================================================
class CHistoryDlg : public CDialog
{
public:
	CHistoryDlg(UINT nID, CWnd *pParentWnd);
 										 // Constructor
	BOOL OnInitDialog();
 										 // Inicializacion de dialogo
	void OnOK();
 										 // OK
	DWORD m_dwValue;
 										 // Valor de edicion

protected:
	virtual UINT* GetNumPtr() = 0;
 										 // Obtener puntero de conteo de historial
	virtual DWORD* GetDataPtr() = 0;
 										 // Obtener puntero de datos de historial
	UINT m_nBit;
 										 // Bits validos
	DWORD m_dwMask;
 										 // Mascara (generada internamente a partir de bits)
};

  //===========================================================================
  //
  //	Dialogo de entrada de direccion
  //
  //===========================================================================
class CAddrDlg : public CHistoryDlg
{
public:
	CAddrDlg(CWnd *pParent = NULL);
 										 // Constructor
	static void SetupHisMenu(CMenu *pMenu);
 										 // Configuracion de menu
	static DWORD GetAddr(UINT nID);
 										 // Obtener resultado de menu

protected:
	UINT* GetNumPtr();
 										 // Obtener puntero de conteo de historial
	DWORD* GetDataPtr();
 										 // Obtener puntero de datos de historial
	static UINT m_Num;
 										 // Conteo de historial
	static DWORD m_Data[10];
 										 // Datos de historial
};

  //===========================================================================
  //
  //	Dialogo de entrada de registros
  //
  //===========================================================================
class CRegDlg : public CHistoryDlg
{
public:
	CRegDlg(CWnd *pParent = NULL);
 										 // Constructor
	BOOL OnInitDialog();
 										 // Inicializacion de dialogo
	void OnOK();
 										 // OK
	UINT m_nIndex;
 										 // Indice de registro

protected:
	UINT* GetNumPtr();
 										 // Obtener puntero de conteo de historial
	DWORD* GetDataPtr();
 										 // Obtener puntero de datos de historial
	static UINT m_Num;
 										 // Conteo de historial
	static DWORD m_Data[10];
 										 // Datos de historial
};

  //===========================================================================
  //
  //	Dialogo de entrada de datos
  //
  //===========================================================================
class CDataDlg : public CHistoryDlg
{
public:
	CDataDlg(CWnd *pParent = NULL);
 										 // Constructor
	BOOL OnInitDialog();
 										 // Inicializacion de dialogo
	UINT m_nSize;
 										 // Tamano
	DWORD m_dwAddr;
 										 // Direccion

protected:
	UINT* GetNumPtr();
 										 // Obtener puntero de conteo de historial
	DWORD* GetDataPtr();
 										 // Obtener puntero de datos de historial
	static UINT m_Num;
 										 // Conteo de historial
	static DWORD m_Data[10];
 										 // Datos de historial
};

  //===========================================================================
  //
  //	Ventana de registros de MPU
  //
  //===========================================================================
class CCPURegWnd : public CSubTextWnd
{
public:
	CCPURegWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion
	static void SetupRegMenu(CMenu *pMenu, CPU *pCPU, BOOL bSR);
 										 // Configuracion de menu
	static DWORD GetRegValue(CPU *pCPU, UINT uID);
 										 // Obtener valor de registro

protected:
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
 										 // Menu de contexto
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
 										 // Doble clic izquierdo
	afx_msg void OnReg(UINT nID);
 										 // Seleccion de registro

private:
	CPU *m_pCPU;
 										 // CPU

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

  //===========================================================================
  //
  //	Ventana de interrupcion
  //
  //===========================================================================
class CIntWnd : public CSubTextWnd
{
public:
	CIntWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
 										 // Doble clic izquierdo
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
 										 // Menu de contexto

private:
	CPU* m_pCPU;
 										 // CPU

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

  //===========================================================================
  //
  //	Ventana de desensamblado
  //
  //===========================================================================
class CDisasmWnd : public CSubTextScrlWnd
{
public:
	CDisasmWnd(int index);
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion
	void FASTCALL SetAddr(DWORD dwAddr);
 										 // Especificacion de direccion
	void FASTCALL SetPC(DWORD pc);
 										 // Especificacion de PC
	void FASTCALL Update();
 										 // Actualizacion desde hilo de mensajes
	static void FASTCALL SetupBreakMenu(CMenu *pMenu, Scheduler *pScheduler);
 										 // Configuracion de menu de puntos de interrupcion
	static DWORD FASTCALL GetBreak(UINT nID, Scheduler *pScheduler);
 										 // Obtener punto de interrupcion

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpcs);
 										 // Creacion de ventana
	afx_msg void OnDestroy();
 										 // Eliminacion de ventana
	afx_msg void OnSize(UINT nType, int cx, int cy);
 										 // Cambio de tamano
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
 										 // Clic izquierdo
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
 										 // Doble clic izquierdo
	afx_msg void OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar);
 										 // Desplazamiento vertical
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
 										 // Menu de contexto
	afx_msg void OnNewWin();
 										 // Nueva ventana
	afx_msg void OnPC();
 										 // Mover al PC
	afx_msg void OnSync();
 										 // Sincronizar con PC
	afx_msg void OnAddr();
 										 // Entrada de direccion
	afx_msg void OnReg(UINT nID);
 										 // Registro
	afx_msg void OnStack(UINT nID);
 										 // Stack
	afx_msg void OnBreak(UINT nID);
 										 // Punto de interrupcion (Breakpoint)
	afx_msg void OnHistory(UINT nID);
 										 // Historial de direcciones
	afx_msg void OnCPUExcept(UINT nID);
 										 // Vector de excepcion de CPU
	afx_msg void OnTrap(UINT nID);
 										 // vector trap
	afx_msg void OnMFP(UINT nID);
 										 // Vector MFP
	afx_msg void OnSCC(UINT nID);
 										 // Vector SCC
	afx_msg void OnDMAC(UINT uID);
 										 // Vector DMAC
	afx_msg void OnIOSC(UINT uID);
 										 // Vector IOSC

private:
	DWORD FASTCALL GetPrevAddr(DWORD dwAddr);
 										 // Obtener direccion anterior
	void FASTCALL SetupContext(CMenu *pMenu);
 										 // Configuracion de menu de contexto
	void FASTCALL SetupVector(CMenu *pMenu, UINT index, DWORD vector, int num);
 										 // Configuracion de vectores de interrupcion
	void FASTCALL SetupAddress(CMenu *pMenu, UINT index, DWORD addr);
 										 // Configuracion de direccion
	void FASTCALL OnVector(UINT vector);
 										 // Especificacion de vector
	CPU *m_pCPU;
 										 // CPU
	Scheduler *m_pScheduler;
 										 // Planificador (Scheduler)
	MFP *m_pMFP;
 										 // MFP
	Memory *m_pMemory;
 										 // Memoria
	SCC * m_pSCC;
 										 // SCC
	DMAC *m_pDMAC;
 										 // DMAC
	IOSC *m_pIOSC;
 										 // IOSC
	BOOL m_bSync;
 										 // Flag de sincronizacion de PC
	DWORD m_dwPC;
 										 // PC
	DWORD m_dwAddr;
 										 // Direccion de inicio de visualizacion
	DWORD m_dwSetAddr;
 										 // Direccion establecida
	DWORD *m_pAddrBuf;
 										 // Buffer de direcciones
	CString m_Caption;
 										 // Cadena de caption
	CString m_CaptionSet;
 										 // Cadena de configuracion de caption

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

  //===========================================================================
  //
  //	Ventana de memoria
  //
  //===========================================================================
class CMemoryWnd : public CSubTextScrlWnd
{
public:
	CMemoryWnd(int nWnd);
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion
	void FASTCALL SetAddr(DWORD dwAddr);
 										 // Especificacion de direccion
	void FASTCALL SetUnit(int nUnit);
 										 // Especificacion de unidad de visualizacion
	void FASTCALL Update();
 										 // Actualizacion desde hilo de mensajes
	static void SetupStackMenu(CMenu *pMenu, Memory *pMemory, CPU *pCPU);
 										 // Configuracion de menu de stack
	static DWORD GetStackAddr(UINT nID, Memory *pMemory, CPU *pCPU);
 										 // Obtener stack

protected:
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
 										 // Creacion de ventana
	afx_msg void OnPaint();
 										 // Dibujo
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
 										 // Doble clic izquierdo
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
 										 // Menu de contexto
	afx_msg void OnAddr();
 										 // Entrada de direccion
	afx_msg void OnNewWin();
 										 // Nueva ventana
	afx_msg void OnUnit(UINT uID);
 										 // Especificacion de unidad de visualizacion
	afx_msg void OnRange(UINT uID);
 										 // Especificacion de rango de direcciones
	afx_msg void OnReg(UINT uID);
 										 // Especificar valor de registro
	afx_msg void OnArea(UINT uID);
 										 // Especificacion de area
	afx_msg void OnHistory(UINT uID);
 										 // Historial de direcciones
	afx_msg void OnStack(UINT uID);
 										 // Stack
	void FASTCALL SetupScrlV();
 										 // Preparacion de desplazamiento (vertical)

private:
	void FASTCALL SetupContext(CMenu *pMenu);
 										 // Configuracion de menu de contexto
	Memory *m_pMemory;
 										 // Memoria
	CPU *m_pCPU;
 										 // CPU
	DWORD m_dwAddr;
 										 // Direccion de inicio de visualizacion
	CString m_strCaptionReq;
 										 // Cadena de caption (solicitud)
	CString m_strCaptionSet;
 										 // Cadena de caption (configuracion)
	CCriticalSection m_CSection;
 										 // Seccion critica
	UINT m_nUnit;
 										 // Tamano de visualizaci?n 0/1/2=Byte/Word/Long

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

  //===========================================================================
  //
  //	Ventana de puntos de interrupcion (breakpoints)
  //
  //===========================================================================
class CBreakPWnd : public CSubTextWnd
{
public:
	CBreakPWnd();
 										 // Constructor
	void FASTCALL Setup();
 										 // Configuracion

protected:
	afx_msg void OnLButtonDblClk(UINT nFlags, CPoint point);
 										 // Doble clic izquierdo
	afx_msg void OnContextMenu(CWnd *pWnd, CPoint point);
 										 // Menu de contexto
	afx_msg void OnEnable();
 										 // Activar/Desactivar
	afx_msg void OnClear();
 										 // Limpiar conteo
	afx_msg void OnDel();
 										 // Eliminar
	afx_msg void OnAddr();
 										 // Especificacion de direccion
	afx_msg void OnAll();
 										 // Eliminar todos
	afx_msg void OnHistory(UINT nID);
 										 // Historial de direcciones

private:
	void FASTCALL SetupContext(CMenu *pMenu);
 										 // Configuracion de menu de contexto
	void FASTCALL SetAddr(DWORD dwAddr);
 										 // Establecer direccion
	Scheduler* m_pScheduler;
 										 // Planificador (Scheduler)
	CPoint m_Point;
 										 // Punto del menu de contexto

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

 #endif	 // mfc_cpu_h
 #endif	 // _WIN32
