  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2005 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Vista de dibujo MFC ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_draw_h)
#define mfc_draw_h

  //===========================================================================
  //
  //	Vista de dibujo
  //
  //===========================================================================
class CDrawView : public CView
{
public:
 	 // Definicion de trabajo interno
	typedef struct _DRAWINFO {
 		BOOL bPower;					 // Energia
 		Render *pRender;				 // Renderizador
 		Render::render_t *pWork;		 // Trabajo del renderizador
         DWORD dwDrawCount;				 // Conteo de dibujo

 		 // Seccion DIB
 		HBITMAP hBitmap;				 // Seccion DIB
 		DWORD *pBits;					 // Datos de bits
 		int nBMPWidth;					 // Ancho BMP
 		int nBMPHeight;					 // Alto BMP

 		 // Comunicacion con renderizador
 		int nRendWidth;					 // Ancho del renderizador
 		int nRendHeight;				 // Alto del renderizador
 		int nRendHMul;					 // Multiplicador horizontal del renderizador
 		int nRendVMul;					 // Multiplicador vertical del renderizador
 		int nLeft;						 // Margen horizontal
 		int nTop;						 // Margen vertical
 		int nWidth;						 // Ancho BitBlt
 		int nHeight;					 // Alto BitBlt

 		 // Relacionado con Blt
 		int nBltTop;					 // Inicio de dibujo Y
 		int nBltBottom;					 // Fin de dibujo Y
 		int nBltLeft;					 // Inicio de dibujo X
 		int nBltRight;					 // Fin de dibujo X
 		BOOL bBltAll;					 // Bandera de visualizacion completa
 		BOOL bBltStretch;				 // Estirar para ajustar relacion de aspecto
	} DRAWINFO, *LPDRAWINFO;

public:
 	 // Funciones basicas
	CDrawView();
 										 // Constructor
	void FASTCALL Enable(BOOL bEnable);
 										 // Control de operacion
	BOOL FASTCALL IsEnable() const;
 										 // Obtencion de bandera de operacion
	BOOL FASTCALL Init(CWnd *pParent);
 										 // Inicializacion
	BOOL PreCreateWindow(CREATESTRUCT& cs);
 										 // Preparacion de creacion de ventana
	void FASTCALL Refresh();
 										 // Dibujo de refresco
	void FASTCALL Draw(int index);
 										 // Dibujo (solo ventana especifica)
	void FASTCALL Update();
 										 // Actualizacion desde hilo de mensajes
	void FASTCALL ApplyCfg(const Config *pConfig);
 										 // Aplicar configuracion
	void FASTCALL GetDrawInfo(LPDRAWINFO pDrawInfo) const;
 										 // Obtencion de informacion de dibujo

 	 // Dibujo de renderizado
	void OnDraw(CDC *pDC);
 										 // Dibujo
	void FASTCALL Stretch(BOOL bStretch);
 										 // Modo de aumento
	BOOL IsStretch() const				{ return m_Info.bBltStretch; }
 										 // Obtencion de modo de aumento

 	 // Gestion de subventanas
	int FASTCALL GetNewSWnd() const;
 										 // Obtencion de nuevo indice de subventana
	void FASTCALL AddSWnd(CSubWnd *pSubWnd);
 										 // Anadir subventana (llamado desde hijo)
	void FASTCALL DelSWnd(CSubWnd *pSubWnd);
 										 // Eliminar subventana (llamado desde hijo)
	void FASTCALL ClrSWnd();
 										 // Eliminar todas las subventanas
	CSubWnd* FASTCALL GetFirstSWnd() const;
 										 // Obtener primera subventana
	CSubWnd* FASTCALL SearchSWnd(DWORD dwID) const;
 										 // Obtener subventana por ID arbitrario
	CFont* FASTCALL GetTextFont();
 										 // Obtener fuente de texto
	CSubWnd* FASTCALL NewWindow(BOOL bDis);
 										 // Crear nueva ventana (Dis, Mem)
	BOOL FASTCALL IsNewWindow(BOOL bDis);
 										 // Consultar si es posible crear nueva ventana
	int FASTCALL GetSubWndNum() const;
 										 // Obtener numero de subventanas
	LPCTSTR FASTCALL GetWndClassName() const;
 										 // Obtener nombre de clase de ventana

protected:
 	 // Mensajes WM
	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
 										 // Creacion de ventana
	afx_msg void OnDestroy();
 										 // Eliminacion de ventana
	afx_msg void OnSize(UINT nType, int cx, int cy);
 										 // Cambio de tamano de ventana
	afx_msg void OnPaint();
 										 // Dibujo
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
 										 // Dibujo de fondo
	afx_msg LRESULT OnDisplayChange(WPARAM wParam, LPARAM lParam);
 										 // Cambio de pantalla
	afx_msg void OnDropFiles(HDROP hDropInfo);
 										 // Soltar archivo (File drop)
	afx_msg BOOL OnMouseWheel(UINT nFlags, short zDelta, CPoint pt);
 										 // Rueda del raton
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
 										 // Tecla pulsada
	afx_msg void OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
 										 // Tecla de sistema pulsada
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
 										 // Tecla liberada
	afx_msg void OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
 										 // Tecla de sistema liberada
	afx_msg void OnMove(int x, int y);
 										 // Movimiento de ventana

	BOOL m_bEnable;
 										 // Bandera de validez
	CFont m_TextFont;
 										 // Fuente de texto

private:
	void FASTCALL SetupBitmap();
 										 // Preparacion de mapa de bits
	inline void FASTCALL ReCalc(CRect& rect);
 										 // Recalculo
	inline void FASTCALL DrawRect(CDC *pDC);
 										 // Dibujar margen circundante
	inline BOOL FASTCALL CalcRect();
 										 // Investigar area necesaria de dibujo
	int FASTCALL MakeBits();
 										 // Creacion de bits
	BOOL FASTCALL KeyUpDown(UINT nChar, UINT nFlags, BOOL bDown);
 										 // Identificacion de teclas
	CSubWnd *m_pSubWnd;
 										 // Primera subventana
	CFrmWnd *m_pFrmWnd;
 										 // Ventana de marco (Frame window)
	CScheduler *m_pScheduler;
 										 // Planificador (Scheduler)
	CInput *m_pInput;
 										 // Entrada (Input)
	DRAWINFO m_Info;
 										 // Trabajo interno

	DECLARE_MESSAGE_MAP()
 										 // Con mapa de mensajes
};

 #endif	 // mfc_draw_h
 #endif	 // _WIN32
