  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Vista de dibujo MFC ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "render.h"
#include "crtc.h"
#include "config.h"
#include "mfc_sub.h" 
#include "mfc_frm.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_cpu.h"
#include "mfc_cfg.h"
#include "mfc_res.h"
#include "mfc_sch.h"
#include "mfc_inp.h"
#include "mfc_draw.h"

#define WM_XM6_PRESENT (WM_APP + 0x120)

  //===========================================================================
  //
  //	Vista de dibujo
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CDrawView::CDrawView()
{
 	 // Inicializacion de la pieza (basica)
	m_bEnable = FALSE;
	m_pSubWnd = NULL;
	m_pFrmWnd = NULL;
	m_bUseDX9 = TRUE;
	m_lPresentPending = 0;
	m_dwOSDUntil = 0;
	m_szOSDText[0] = _T('\0');

 	 // Componentes
	m_pScheduler = NULL;
	m_pInput = NULL;

 	 // Inicializacion de la pieza (dibujo general)
	m_Info.bPower = FALSE;
	m_Info.pRender = NULL;
	m_Info.pWork = NULL; 
	m_Info.dwDrawCount = 0;

 	 // Inicializacion de la pieza (seccion DIB)
	m_Info.hBitmap = NULL;
	m_Info.pBits = NULL;
	m_Info.nBMPWidth = 0;
	m_Info.nBMPHeight = 0;

 	 // Inicializacion de la pieza (ajuste de tamano)
	m_Info.nRendWidth = 0;
	m_Info.nRendHeight = 0;
	m_Info.nRendHMul = 0;
	m_Info.nRendVMul = 0;
	m_Info.nLeft = 0;
	m_Info.nTop = 0;
	m_Info.nWidth = 0;
	m_Info.nHeight = 0;

 	 // Inicializacion de la pieza (Blt)
	m_Info.nBltTop = 0;
	m_Info.nBltBottom = 0;
	m_Info.nBltLeft = 0;
	m_Info.nBltRight = 0;
	m_Info.bBltAll = TRUE;
	m_Info.bBltStretch = FALSE;
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CDrawView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_ERASEBKGND()
	ON_MESSAGE(WM_DISPLAYCHANGE, OnDisplayChange)
	ON_MESSAGE(WM_XM6_PRESENT, OnPresentFrame)
	ON_WM_DROPFILES()
#if _MFC_VER >= 0x600
	ON_WM_MOUSEWHEEL()
 #endif	 // _MFC_VER
	ON_WM_KEYDOWN()
	ON_WM_SYSKEYDOWN()
	ON_WM_KEYUP()
	ON_WM_SYSKEYUP()
	ON_WM_MOVE()
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Inicializacion
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::Init(CWnd *pParent)
{
	ASSERT(pParent);

 	 // Memoria de la ventana de marco (Frame Window)
	m_pFrmWnd = (CFrmWnd*)pParent;

 	 // Crear como primera vista
	if (!Create(NULL, NULL, WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
				CRect(0, 0, 0, 0), pParent, AFX_IDW_PANE_FIRST, NULL)) {
		return FALSE;
	}

	CRect rect;
	GetClientRect(&rect);
	if (rect.Width() <= 0) {
		rect.right = 640;
	}
	if (rect.Height() <= 0) {
		rect.bottom = 480;
	}
	m_DX9Renderer.Init(m_hWnd, rect.Width(), rect.Height(), TRUE,
		(m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Preparando la creacion de una ventana
  //
  //---------------------------------------------------------------------------
BOOL CDrawView::PreCreateWindow(CREATESTRUCT& cs)
{
 	 // Clase base
	if (!CView::PreCreateWindow(cs)) {
		return FALSE;
	}

 	 // Anadir WS_CLIPCHILDREN
	cs.style |= WS_CLIPCHILDREN;

 	 // Anadir un borde de cliente
	cs.dwExStyle |= WS_EX_CLIENTEDGE;

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Creacion de ventanas
  //
  //---------------------------------------------------------------------------
int CDrawView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
 	 // Clase base
	if (CView::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

 	 // IME desactivado (off)
	::ImmAssociateContext(m_hWnd, (HIMC)NULL);

 	 // Creacion de fuentes de texto
	if (IsJapanese()) {
 		 // Entorno de idioma japones
		m_TextFont.CreateFont(14, 0, 0, 0,
							FW_NORMAL, 0, 0, 0,
							SHIFTJIS_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							FIXED_PITCH, NULL);
	}
	else {
 		 // Entorno de idioma ingles
		m_TextFont.CreateFont(14, 0, 0, 0,
							FW_NORMAL, 0, 0, 0,
							DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
							CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
							FIXED_PITCH, NULL);
	}

 	 // Permiso para arrastrar y soltar (Drag and Drop)
	DragAcceptFiles(TRUE);

	return 0;
}

  //---------------------------------------------------------------------------
  //
  //	Eliminacion de ventanas
  //
  //---------------------------------------------------------------------------
void CDrawView::OnDestroy()
{
 	 // Detener operacion
	Enable(FALSE);

 	 // Borrar mapa de bits
	if (m_Info.hBitmap) {
		::DeleteObject(m_Info.hBitmap);
		m_Info.hBitmap = NULL;
		m_Info.pBits = NULL;
	}

	 // Borrar fuente de texto
	m_TextFont.DeleteObject();
	m_DX9Renderer.Cleanup();

	 // A la clase base
	CView::OnDestroy();
}

  //---------------------------------------------------------------------------
  //
  //	Redimensionar
  //
  //---------------------------------------------------------------------------
void CDrawView::OnSize(UINT nType, int cx, int cy)
{
	 // Actualizacion del mapa de bits
	SetupBitmap();
	if (m_DX9Renderer.IsInitialized()) {
		m_DX9Renderer.ResetDevice(cx, cy, TRUE,
			(m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);
	}

	 // Clase base
	CView::OnSize(nType, cx, cy);
}

  //---------------------------------------------------------------------------
  //
  //	Dibujo
  //
  //---------------------------------------------------------------------------
void CDrawView::OnPaint()
{
	Render *pRender;
	CRTC *pCRTC;
	const CRTC::crtc_t *p;
	CFrmWnd *pFrmWnd;
	PAINTSTRUCT ps;

 	 // Bloqueo VM
	::LockVM();

 	 // Todas las banderas de dibujo activadas (ON)
	m_Info.bBltAll = TRUE;

 	 // Si esta activado y el programador desactivado, crear en buffer Mix (forzado)
	if (m_bEnable) {
		pFrmWnd = (CFrmWnd*)GetParent();
		ASSERT(pFrmWnd);
		if (!pFrmWnd->GetScheduler()->IsEnable()) {
 			 // Render y CRTC presentes si esta activado
			pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
			ASSERT(pRender);
			pCRTC = (CRTC*)::GetVM()->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
			ASSERT(pCRTC);
			p = pCRTC->GetWorkAddr();

 			 // Crear
			m_Info.bPower = ::GetVM()->IsPower();
			if (m_Info.bPower) {
				pRender->Complete();
				pRender->EnableAct(TRUE);
				pRender->StartFrame();
				pRender->HSync(p->v_dots);
				pRender->EndFrame();
			}
			else {
 				 // Borrar todos los mapas de bits
				memset(m_Info.pBits, 0, m_Info.nBMPWidth * m_Info.nBMPHeight * 4);
			}

 			 // Dibujo (conectar con CDrawView::OnDraw)
			CView::OnPaint();

 			 // Desbloqueo VM
			::UnlockVM();
			return;
		}
	}

 	 // Solo obtener DCs
	BeginPaint(&ps);
	EndPaint(&ps);

 	 // Desbloqueo VM
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Dibujo de fondo
  //
  //---------------------------------------------------------------------------
BOOL CDrawView::OnEraseBkgnd(CDC *pDC)
{
	CRect rect;

 	 // Si no esta activado, rellenar con negro
	if (!m_bEnable) {
		GetClientRect(&rect);
		pDC->FillSolidRect(&rect, RGB(0, 0, 0));
	}

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Cambiar el entorno de visualizacion
  //
  //---------------------------------------------------------------------------
LRESULT CDrawView::OnDisplayChange(WPARAM /* wParam */, LPARAM /* lParam */)
{
 	 // Preparacion del mapa de bits
	SetupBitmap();

	return 0;
}

  //---------------------------------------------------------------------------
  //
  //	Soltar archivos (File Drop)
  //
  //---------------------------------------------------------------------------
void CDrawView::OnDropFiles(HDROP hDropInfo)
{
	TCHAR szPath[_MAX_PATH];
	POINT point;
	CRect rect;
	int nFiles;
	int nDrive;

 	 // Obtener numero de archivos soltados
	nFiles = ::DragQueryFile(hDropInfo, 0xffffffff, szPath, _MAX_PATH);
	ASSERT(nFiles > 0);

 	 // Determinar unidad desde posicion de caida
	::DragQueryPoint(hDropInfo, &point);
	GetClientRect(rect);
	if (point.x < (rect.right >> 1)) {
 		 // Mitad izquierda (unidad 0)
		nDrive = 0;
	}
	else {
 		 // Mitad derecha (unidad 1)
		nDrive = 1;
	}

 	 // Dividir por numero de archivos
	if (nFiles == 1) {
 		 // Un solo archivo: mitad izquierda y derecha de la ventana
		::DragQueryFile(hDropInfo, 0, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(nDrive, szPath);
	}
	else {
 		 // Dos archivos: 0 y 1 respectivamente
		::DragQueryFile(hDropInfo, 0, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(0, szPath);
		::DragQueryFile(hDropInfo, 1, szPath, _MAX_PATH);
		m_pFrmWnd->InitCmdSub(1, szPath);
	}

 	 // Fin de procesamiento
	::DragFinish(hDropInfo);

 	 // Dos archivos se restablecen
	if (nFiles > 1) {
		m_pFrmWnd->PostMessage(WM_COMMAND, IDM_RESET, 0);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Rueda del raton
  //
  //---------------------------------------------------------------------------
BOOL CDrawView::OnMouseWheel(UINT /*nFlags*/, short zDelta, CPoint /*pt*/)
{
	CConfig *pConfig;

 	 // Obtener configuracion
	pConfig = m_pFrmWnd->GetConfig();

 	 // Bloquear VM
	::LockVM();

 	 // Cambia segun la orientacion del eje Z
	if (zDelta > 0) {
 		 // Hacia atras: ampliar
		Stretch(TRUE);
		pConfig->SetStretch(TRUE);
	}
	else {
 		 // Hacia adelante: no ampliada
		Stretch(FALSE);
		pConfig->SetStretch(FALSE);
	}

 	 // Desbloqueo VM
	::UnlockVM();

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Tecla pulsada
  //
  //---------------------------------------------------------------------------
void CDrawView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	 // Determinar que teclas excluir
	if (!KeyUpDown(nChar, nFlags, TRUE)) {
		return;
	}

	if (nChar == VK_F8) {
		ToggleRenderer();
		return;
	}

	 // Flujo a clase base
	CView::OnKeyDown(nChar, nRepCnt, nFlags);
}

  //---------------------------------------------------------------------------
  //
  //	Tecla de sistema pulsada
  //
  //---------------------------------------------------------------------------
void CDrawView::OnSysKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
 	 // Determinar que teclas excluir
	if (!KeyUpDown(nChar, nFlags, TRUE)) {
		return;
	}

 	 // Flujo a clase base
	CView::OnSysKeyDown(nChar, nRepCnt, nFlags);
}

  //---------------------------------------------------------------------------
  //
  //	Tecla liberada
  //
  //---------------------------------------------------------------------------
void CDrawView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
 	 // Determinar teclas a excluir
	if (!KeyUpDown(nChar, nFlags, FALSE)) {
		return;
	}

 	 // Flujo a clase base
	CView::OnKeyUp(nChar, nRepCnt, nFlags);
}

  //---------------------------------------------------------------------------
  //
  //	Tecla de sistema liberada
  //
  //---------------------------------------------------------------------------
void CDrawView::OnSysKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
 	 // Determinar que teclas excluir
	if (!KeyUpDown(nChar, nFlags, FALSE)) {
		return;
	}

 	 // Flujo a clase base
	CView::OnSysKeyUp(nChar, nRepCnt, nFlags);
}

  //---------------------------------------------------------------------------
  //
  //	Identificacion de teclas
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::KeyUpDown(UINT nChar, UINT nFlags, BOOL bDown)
{
#if defined(INPUT_MOUSE) && defined(INPUT_KEYBOARD) && defined(INPUT_HARDWARE)
	INPUT input;

	ASSERT(this);
	ASSERT(nChar < 0x100);

 	 // Obtener planificador (Scheduler)
	if (!m_pScheduler) {
		m_pScheduler = m_pFrmWnd->GetScheduler();
		if (!m_pScheduler) {
 			 // No existe planificador, no excluir
			return TRUE;
		}
	}

 	 // Obtener entrada (Input)
	if (!m_pInput) {
		m_pInput = m_pFrmWnd->GetInput();
		if (!m_pScheduler) {
 			 // No existe entrada, no excluir
			return TRUE;
		}
	}

 	 // Si el planificador esta detenido, no excluir
	if (!m_pScheduler->IsEnable()) {
		return TRUE;
	}

 	 // Si la entrada no esta activa o en menu, no excluir
	if (!m_pInput->IsActive()) {
		return TRUE;
	}
	if (m_pInput->IsMenu()) {
		return TRUE;
	}

 	 // Identificacion de tecla
	switch (nChar) {
 		 // F10
		case VK_F10:
			if (m_pInput->IsKeyMapped(DIK_F10)) {
 				 // Mapeada
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // ALT izquierda
		case VK_LMENU:
			if (m_pInput->IsKeyMapped(DIK_LMENU)) {
				if (bDown) {
 					 // Permitir que otras teclas interrumpan
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}

 				 // Mapeada
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // ALT derecha
		case VK_RMENU:
			if (m_pInput->IsKeyMapped(DIK_RMENU)) {
 				 // Mapeada
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // ALT comun
		case VK_MENU:
			if (m_pInput->IsKeyMapped(DIK_LMENU) || m_pInput->IsKeyMapped(DIK_RMENU)) {
 				 // Mapeada
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // Windows izquierda
		case VK_LWIN:
			if (m_pInput->IsKeyMapped(DIK_LWIN)) {
 				 // Mapeada
				if (bDown) {
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // Windows derecha
		case VK_RWIN:
			if (m_pInput->IsKeyMapped(DIK_RWIN)) {
 				 // Mapeada
				if (bDown) {
 					 // No bloquear tecla Windows, permitir interrupcion de otras
					memset(&input, 0, sizeof(input));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					::SendInput(1, &input, sizeof(INPUT));
					input.type = INPUT_KEYBOARD;
					input.ki.wVk = VK_SHIFT;
					input.ki.dwFlags = KEYEVENTF_KEYUP;
					::SendInput(1, &input, sizeof(INPUT));
				}
				return FALSE;
			}
 			 // No mapeada
			return TRUE;

 		 // Otros
		default:
 			 // ?Es tecla con ALT?
			if (nFlags & 0x2000) {
 				 // ?Estan mapeadas ALT izquierda o derecha?
				if (m_pInput->IsKeyMapped(DIK_LMENU) || m_pInput->IsKeyMapped(DIK_RMENU)) {
 					 // Si alguna ALT esta mapeada, desactivar ALT+tecla
					return FALSE;
				}
			}
			break;
	}
 #endif	 // INPUT_MOUSE && INPUT_KEYBOARD && INPUT_HARDWARE

 	 // En otros casos, permitir
	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Movimiento de ventanas
  //
  //---------------------------------------------------------------------------
void CDrawView::OnMove(int x, int y)
{
	ASSERT(m_pFrmWnd);

 	 // Clase base
	CView::OnMove(x, y);

 	 // Solicitar cambio de posicion de la ventana de marco
	m_pFrmWnd->RecalcStatusView();
}

  //---------------------------------------------------------------------------
  //
  //	Preparacion del mapa de bits
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::SetupBitmap()
{
	CClientDC *pDC;
	BITMAPINFOHEADER *p;
	CRect rect;

 	 // Si hay mapa de bits, liberarlo primero
	if (m_Info.hBitmap) {
		if (m_Info.pRender) {
			m_Info.pRender->SetMixBuf(NULL, 0, 0);
		}
		::DeleteObject(m_Info.hBitmap);
		m_Info.hBitmap = NULL;
		m_Info.pBits = NULL;
	}

 	 // Tratamiento especial para minimizacion
	GetClientRect(&rect);
	if ((rect.Width() == 0) || (rect.Height() == 0)) {
		return;
	}
	

 	 // Asignacion de memoria para cabeceras de mapas de bits
	p = (BITMAPINFOHEADER*) new BYTE[sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD)];
	memset(p, 0, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD));

 	 // Creacion de informacion del mapa de bits
	m_Info.nBMPWidth = rect.Width();

	/* ACA SE ESTABLECE LA ALTURA DEL BITMAP A LEER */
	m_Info.nBMPHeight = (rect.Height() < 512) ? 512 : rect.Height();
	p->biSize = sizeof(BITMAPINFOHEADER);
	p->biWidth = m_Info.nBMPWidth;
	p->biHeight = -m_Info.nBMPHeight;
	p->biPlanes = 1;
	p->biBitCount = 32;
	p->biCompression = BI_RGB;
	p->biSizeImage = m_Info.nBMPWidth * m_Info.nBMPHeight * (32 >> 3);

 	 // Obtencion de DC, creacion de seccion DIB
	pDC = new CClientDC(this);
	m_Info.hBitmap = ::CreateDIBSection(pDC->m_hDC, (BITMAPINFO*)p, DIB_RGB_COLORS,
								(void**)&(m_Info.pBits), NULL, 0);
 	 // Si tiene exito, indicar al renderizador
	if (m_Info.hBitmap && m_Info.pRender) {
		m_Info.pRender->SetMixBuf(m_Info.pBits, m_Info.nBMPWidth, m_Info.nBMPHeight);
	}
	delete pDC;
	delete[] p;

 	 // Recalculo
	m_Info.nRendHMul = -1;
	m_Info.nRendVMul = -1;
	ReCalc(rect);
}

  //---------------------------------------------------------------------------
  //
//	Control de funcionamiento
//
//---------------------------------------------------------------------------
void FASTCALL CDrawView::Enable(BOOL bEnable)
{
	CSubWnd* pWnd;

 	 // Memoria de bandera
	m_bEnable = bEnable;

 	 // Memoria del renderizador si esta habilitado
	if (m_bEnable) {
		if (!m_Info.pRender) {
			m_Info.pRender = (Render*)::GetVM()->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
			ASSERT(m_Info.pRender);
			m_Info.pWork = m_Info.pRender->GetWorkAddr();
			ASSERT(m_Info.pWork);
			if (m_Info.pBits) {
				m_Info.pRender->SetMixBuf(m_Info.pBits, m_Info.nBMPWidth, m_Info.nBMPHeight);
			}
		}
	}

 	 // Instrucciones para subventanas
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Enable(bEnable);
		pWnd = pWnd->m_pNextWnd;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtencion de banderas de operacion
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsEnable() const
{
	return m_bEnable;
}

  //---------------------------------------------------------------------------
  //
  //	Refresco de dibujo
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::Refresh()
{
	CSubWnd *pWnd;
	CClientDC dc(this);

 	 // Redibujar vista de dibujo
	OnDraw(&dc);

 	 // Redibujar subventana
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Refresh();

 		 // Siguiente subventana
		pWnd = pWnd->m_pNextWnd;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Dibujo (desde el planificador)
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::Draw(int nChildWnd)
{
	CSubWnd *pSubWnd;
	CClientDC *pDC;

	ASSERT(nChildWnd >= -1);

	 // -1 es la vista Draw
	if (nChildWnd < 0) {
		if (m_bUseDX9) {
			RequestPresent();
		} else {
			pDC = new CClientDC(this);
			OnDraw(pDC);
			delete pDC;
		}
		return;
	}

 	 // Subventana a partir de 0
	pSubWnd = m_pSubWnd;

	while (nChildWnd > 0) {
 		 // Siguiente subventana
		pSubWnd = pSubWnd->m_pNextWnd;
		ASSERT(pSubWnd);
		nChildWnd--;
	}

 	 // Refresco
	pSubWnd->Refresh();
}

  //---------------------------------------------------------------------------
  //
  //	Actualizacion del hilo de mensajes
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::Update()
{
	CSubWnd *pWnd;

 	 // Instrucciones para subventanas
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->Update();
		pWnd = pWnd->m_pNextWnd;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Solicitar presentacion de frame al hilo de UI
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::RequestPresent()
{
	if (!m_hWnd) {
		return;
	}
	if (::InterlockedExchange(&m_lPresentPending, 1) == 0) {
		PostMessage(WM_XM6_PRESENT, 0, 0);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Actualizar VSync en renderer DX
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::SetVSync(BOOL bEnable)
{
	if (!m_DX9Renderer.IsInitialized()) {
		return;
	}
	CRect rect;
	GetClientRect(&rect);
	m_DX9Renderer.ResetDevice((rect.Width() > 0) ? rect.Width() : 1,
		(rect.Height() > 0) ? rect.Height() : 1,
		TRUE,
		bEnable);
}

  //---------------------------------------------------------------------------
  //
  //	Alternar entre DX9 y GDI
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ToggleRenderer()
{
	m_bUseDX9 = !m_bUseDX9;
	if (m_bUseDX9 && !m_DX9Renderer.IsInitialized()) {
		CRect rect;
		GetClientRect(&rect);
		m_DX9Renderer.Init(m_hWnd,
			(rect.Width() > 0) ? rect.Width() : 640,
			(rect.Height() > 0) ? rect.Height() : 480,
			TRUE,
			(m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);
	}
	ShowRenderStatusOSD((m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);
}

  //---------------------------------------------------------------------------
  //
  //	Mostrar OSD de estado de renderer/VSync
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ShowRenderStatusOSD(BOOL bVSync)
{
	CString status;
	status.Format(_T("%s | VSync: %s"),
		m_bUseDX9 ? _T("DirectX 9") : _T("GDI Mode"),
		bVSync ? _T("ON") : _T("OFF"));
	ShowOSD((LPCTSTR)status);
	RequestPresent();
}

  //---------------------------------------------------------------------------
  //
  //	Presentacion asincrona de frame (hilo UI)
  //
  //---------------------------------------------------------------------------
LRESULT CDrawView::OnPresentFrame(WPARAM /*wParam*/, LPARAM /*lParam*/)
{
	BOOL bPresented = FALSE;

	if (m_bEnable && m_Info.hBitmap && m_Info.pWork && m_Info.pBits) {
		if (m_bUseDX9) {
			if (!m_DX9Renderer.IsInitialized()) {
				CRect rect;
				GetClientRect(&rect);
				m_DX9Renderer.Init(m_hWnd,
					(rect.Width() > 0) ? rect.Width() : 640,
					(rect.Height() > 0) ? rect.Height() : 480,
					TRUE,
					(m_pFrmWnd) ? m_pFrmWnd->m_bVSyncEnabled : TRUE);
			}

			if (m_DX9Renderer.IsInitialized()) {
				int srcWidth = 0;
				int srcHeight = 0;
				int srcPitch = 0;
				BOOL bUpdated = FALSE;

				::LockVM();
				CRect rect;
				GetClientRect(&rect);
				ReCalc(rect);
				srcWidth = m_Info.nWidth;
				srcHeight = m_Info.nHeight;
				srcPitch = m_Info.nBMPWidth;

				if ((srcWidth > 0) && (srcHeight > 0) && (srcPitch >= srcWidth)) {
					bUpdated = m_DX9Renderer.UpdateSurface(m_Info.pBits, srcWidth, srcHeight, srcPitch);
				}

				FinishFrame();
				::UnlockVM();

				if (bUpdated && m_DX9Renderer.PresentFrame(srcWidth, srcHeight, TRUE, FALSE)) {
					bPresented = TRUE;
					if (m_szOSDText[0] && (GetTickCount() <= m_dwOSDUntil)) {
						CClientDC dc(this);
						DrawOSD(&dc);
					}
				}
			}

			if (!bPresented) {
				m_bUseDX9 = FALSE;
				ShowOSD(_T("GDI fallback"));
			}
		}

		if (!bPresented) {
			CClientDC dc(this);
			::LockVM();
			OnDraw(&dc);
			::UnlockVM();
		}

	}

	::InterlockedExchange(&m_lPresentPending, 0);
	return 0;
}

  //---------------------------------------------------------------------------
  //
  //	Marcar frame consumido
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::FinishFrame()
{
	if (!m_Info.pWork) {
		return;
	}

	for (int i = 0; i < (m_Info.nHeight * 64); i++) {
		m_Info.pWork->drawflag[i] = FALSE;
	}
	m_Info.dwDrawCount++;
	m_Info.nBltLeft = 0;
	m_Info.nBltTop = 0;
	m_Info.nBltRight = m_Info.nWidth - 1;
	m_Info.nBltBottom = m_Info.nHeight - 1;
	m_Info.bBltAll = FALSE;
}

  //---------------------------------------------------------------------------
  //
  //	OSD simple
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::DrawOSD(CDC *pDC)
{
	if (!pDC || !m_szOSDText[0]) {
		return;
	}
	if (GetTickCount() > m_dwOSDUntil) {
		return;
	}

	CRect rect(8, 8, 220, 28);
	pDC->FillSolidRect(&rect, RGB(0, 0, 0));
	int oldBk = pDC->SetBkMode(TRANSPARENT);
	COLORREF oldColor = pDC->SetTextColor(RGB(255, 255, 255));
	pDC->DrawText(m_szOSDText, -1, &rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
	pDC->SetTextColor(oldColor);
	pDC->SetBkMode(oldBk);
}

  //---------------------------------------------------------------------------
  //
  //	Mostrar OSD
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ShowOSD(LPCTSTR lpszText)
{
	if (!lpszText) {
		m_szOSDText[0] = _T('\0');
		m_dwOSDUntil = 0;
		return;
	}
	_tcsncpy(m_szOSDText, lpszText, (sizeof(m_szOSDText) / sizeof(m_szOSDText[0])) - 1);
	m_szOSDText[(sizeof(m_szOSDText) / sizeof(m_szOSDText[0])) - 1] = _T('\0');
	m_dwOSDUntil = GetTickCount() + 2500;
}

  //---------------------------------------------------------------------------
  //
  //	Aplicar ajustes
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ApplyCfg(const Config *pConfig)
{
	CSubWnd *pWnd;

	ASSERT(pConfig);

 	 // Estiramiento (Stretch)
	Stretch(pConfig->aspect_stretch);

 	 // Instrucciones para subventanas
	pWnd = m_pSubWnd;
	while (pWnd) {
		pWnd->ApplyCfg(pConfig);
		pWnd = pWnd->m_pNextWnd;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtencion de informacion de dibujo
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::GetDrawInfo(LPDRAWINFO pDrawInfo) const
{
	ASSERT(this);
	ASSERT(pDrawInfo);

 	 // Copiar trabajo interno
	*pDrawInfo = m_Info;
}

  //---------------------------------------------------------------------------
  //
  //	Dibujo
  //
  //---------------------------------------------------------------------------
void CDrawView::OnDraw(CDC *pDC)
{
	CRect rect;
	HDC hMemDC;
	HBITMAP hDefBitmap;
	int i;
	int vmul;
	int hmul;

 	 // Rellenar si el mapa de bits no esta listo
	GetClientRect(&rect);
	if (!m_Info.hBitmap || !m_bEnable || !m_Info.pWork) {
		pDC->FillSolidRect(&rect, RGB(0, 0, 0));
		DrawOSD(pDC);
		return;
	}

 	 // Recalculo
	ReCalc(rect);

 	 // Medidas de desconexion
	if (::GetVM()->IsPower() != m_Info.bPower) {
		m_Info.bPower = ::GetVM()->IsPower();
		if (!m_Info.bPower) {
 			 // Borrar todos los mapas de bits
			memset(m_Info.pBits, 0, m_Info.nBMPWidth * m_Info.nBMPHeight * 4);
			m_Info.bBltAll = TRUE;
		}
	}

 	 // Dibujar una esquina
	if (m_Info.bBltAll) {
		DrawRect(pDC);
	}

 	 // Fijar ampliacion de pantalla final
	hmul = 1;
	if (m_Info.nRendHMul == 2) {
 		 // Res 256, etc.
		hmul = 2;
	}


	/* ACA SE ESTABLECE STRETCH HORIZONTAL */
 	if ( m_Info.bBltStretch) {	 // Si se requiere estiramiento
 		 // Distinto de 768x512, mismo modo de aspecto especificado
		int numeroDeMultiplicador = 4;


		/* Mi  Codigo de prueba para calcular stretch  maximo posible con respecto al anfitrion */
		int mihmul = hmul;				
		int anchoCalculado = 0;
		while (anchoCalculado <= rect.Width())
		{						
			numeroDeMultiplicador++;
			mihmul = hmul * numeroDeMultiplicador;			
			anchoCalculado = (m_Info.nWidth * mihmul) >> 2;						
		}
		numeroDeMultiplicador--;
		

		/*CString sz;	
		sz.Format(_T("numeroDeMultiplicador: %d   \r\n"),  numeroDeMultiplicador);	
		OutputDebugStringW(CT2W(sz));		*/
		
		hmul *= numeroDeMultiplicador;

	}
	else {
 		 // Relacion de aspecto no identica. Mismo aumento
		hmul <<= 2;
	}

	/* ACA SE ESTABLECE STRETCH VERTICAL   */
	vmul = 4;
	

	/* Mi  Codigo de prueba para calcular stretch vertical m�ximo posible con respecto al anfitrion */
		if (m_Info.bBltStretch) 
		{
			vmul = 1;							
			int mivmul = vmul;
			int altoCalculado = 0;
 			while (altoCalculado < rect.Height())  // Aumentar multiplicadores hasta alcanzar resolucion de pantalla
			{																	
				mivmul++;
				altoCalculado = (m_Info.nRendHeight * mivmul) >> 2;		
			}
 			if (altoCalculado - rect.Height() > 16 )   // Ajustar 256 para 240p si faltan 16 pix
				mivmul--;
			vmul = mivmul;				
			
		}
		
 		if (m_Info.nWidth == 512 && m_Info.nHeight == 480)  // Caso especial: Code Zero
		{
			if  (m_Info.nRendHeight < m_Info.nHeight)
				vmul = vmul >> 1;
 			 // hmul++;
		}

 		if (m_Info.nWidth == 704 && m_Info.nHeight == 480)  // Caso especial: Carat
		{
			 hmul++;
 			 // vmul++;
		}

		/*
		CString sv;
		sv.Format(_T("nWidth: %d nHeight:%d   nRendwidth: %d nRendheight:%d  vmul: %d  rect.height:%d \r\n"), m_Info.nWidth, m_Info.nHeight, m_Info.nRendWidth, m_Info.nRendHeight, vmul, rect.Height());		
		OutputDebugStringW(CT2W(sv));
        */



 	 // En caso de bBltAll, determinar toda el area
	if (m_Info.bBltAll) {
 		 // Creacion y seleccion de memoria DC
		hMemDC = CreateCompatibleDC(pDC->m_hDC);
		if (!hMemDC) {
			return;
		}
		hDefBitmap = (HBITMAP)SelectObject(hMemDC, m_Info.hBitmap);
		if (!hDefBitmap) {
			DeleteDC(hMemDC);
			return;
		}

 		 // Blt
		if ((hmul == 4) && (vmul == 4)) {
			::BitBlt(pDC->m_hDC,
				m_Info.nLeft, m_Info.nTop,
				m_Info.nWidth, m_Info.nHeight,
				hMemDC, 0, 0,
				SRCCOPY);
		}
		else {
			::StretchBlt(pDC->m_hDC,
				m_Info.nLeft, m_Info.nTop,
				(m_Info.nWidth * hmul) >> 2,
				(m_Info.nHeight * vmul) >> 2,
				hMemDC, 0, 0,
				m_Info.nWidth, m_Info.nHeight,
				SRCCOPY);
		}
		::GdiFlush();
		m_Info.bBltAll = FALSE;

 		 // Volver al mapa de bits
		SelectObject(hMemDC, hDefBitmap);
		DeleteDC(hMemDC);

 		 // ?No olvidar bajar la bandera de dibujo!
		for (i=0; i<m_Info.nHeight * 64; i++) {
			m_Info.pWork->drawflag[i] = FALSE;
		}
		m_Info.dwDrawCount++;
		m_Info.nBltLeft = 0;
		m_Info.nBltTop = 0;
		m_Info.nBltRight = m_Info.nWidth - 1;
		m_Info.nBltBottom = m_Info.nHeight - 1;
		DrawOSD(pDC);
		return;
	}

 	 // Comprobacion de area de dibujo
	if (!CalcRect()) {
		return;
	}
	ASSERT(m_Info.nBltTop <= m_Info.nBltBottom);
	ASSERT(m_Info.nBltLeft <= m_Info.nBltRight);

 	 // Creacion y seleccion de memoria DC
	hMemDC = CreateCompatibleDC(pDC->m_hDC);
	if (!hMemDC) {
		m_Info.bBltAll = TRUE;
		return;
	}

	hDefBitmap = (HBITMAP)SelectObject(hMemDC, m_Info.hBitmap);
	if (!hDefBitmap) {
		DeleteDC(hMemDC);
		m_Info.bBltAll = TRUE;
		return;
	}

 	 // Dibujar solo algunas zonas
	if ((hmul == 4) && (vmul == 4)) {
		::BitBlt(pDC->m_hDC,
			m_Info.nLeft + m_Info.nBltLeft,
			m_Info.nTop + m_Info.nBltTop,
			m_Info.nBltRight - m_Info.nBltLeft + 1,
			m_Info.nBltBottom - m_Info.nBltTop + 1,
			hMemDC,
			m_Info.nBltLeft,
			m_Info.nBltTop,
			SRCCOPY);
	}
	else {
		::StretchBlt(pDC->m_hDC,
			m_Info.nLeft + ((m_Info.nBltLeft * hmul) >> 2),
			m_Info.nTop + ((m_Info.nBltTop * vmul) >> 2),
			((m_Info.nBltRight - m_Info.nBltLeft + 1) * hmul) >> 2,
			((m_Info.nBltBottom - m_Info.nBltTop + 1) * vmul) >> 2,
			hMemDC,
			m_Info.nBltLeft,
			m_Info.nBltTop,
			m_Info.nBltRight - m_Info.nBltLeft + 1,
			m_Info.nBltBottom - m_Info.nBltTop + 1,
			SRCCOPY);
	}
	::GdiFlush();

 	 // Volver al mapa de bits
	SelectObject(hMemDC, hDefBitmap);
	DeleteDC(hMemDC);

	// La bandera es bajada por CalcRect
	m_Info.dwDrawCount++;
	DrawOSD(pDC);
}

  //---------------------------------------------------------------------------
  //
  //	Recalculo
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ReCalc(CRect& rect)
{
	int width;
	int height;
	BOOL flag;

 	 // Trabajo del renderizador, volver si no hay mapa de bits
	if (!m_Info.pWork || !m_Info.hBitmap) {
		return;
	}

 	 // Comparacion
	flag = FALSE;
	if (m_Info.nRendWidth != m_Info.pWork->width) {
		m_Info.nRendWidth = m_Info.pWork->width;
		flag = TRUE;
	}
	if (m_Info.nRendHeight != m_Info.pWork->height) {
		m_Info.nRendHeight = m_Info.pWork->height;
		flag = TRUE;
	}
	if (m_Info.nRendHMul != m_Info.pWork->h_mul) {
		m_Info.nRendHMul = m_Info.pWork->h_mul;
		flag = TRUE;
	}
	if (m_Info.nRendVMul != m_Info.pWork->v_mul) {
		m_Info.nRendVMul = m_Info.pWork->v_mul;
		flag = TRUE;
	}
	if (!flag) {
		return;
	}

 	 // Renderizador: tomar el menor de los dos mapas de bits
	m_Info.nWidth = m_Info.nRendWidth;
	if (m_Info.nBMPWidth < m_Info.nWidth) {
		m_Info.nWidth = m_Info.nBMPWidth;
	}
	m_Info.nHeight = m_Info.nRendHeight;
	if (m_Info.nRendVMul == 0) {
 		 // Procesamiento para el entrelazado de 15k
		m_Info.nHeight <<= 1;
	}
	if (m_Info.nBMPHeight < m_Info.nRendHeight) {
		m_Info.nHeight = m_Info.nBMPHeight;
	}

 	 // Centrado y margenes calculados teniendo en cuenta el aumento
	width = m_Info.nWidth * m_Info.nRendHMul;
 	if ((m_Info.nRendWidth < 768) && m_Info.bBltStretch) {	 // Si necesita estirar
 		width = (width * 5) >> 2;   // Relacion de aspecto 5:4
	}
	height = m_Info.nHeight;
	if (m_Info.nRendVMul == 2) {
		height <<= 1;
	}

	int cx = 0, cy = 0;
	int bordeAncho = 0, bordeAlto = 0;

	if (m_pFrmWnd->m_bFullScreen)
	{
		cx = ::GetSystemMetrics(SM_CXSCREEN);
		cy = ::GetSystemMetrics(SM_CYSCREEN);
	}
	
	if (m_Info.nRendWidth > 256)
	{
		if (cx > m_Info.nRendWidth)
			bordeAncho = cx % m_Info.nRendWidth;
	}
	if (m_Info.nRendHeight < 240) 
	{
		if (cy > m_Info.nRendHeight)
			bordeAlto = cy % m_Info.nRendHeight;
	}


 	 //CString sz, sz2, sz3, sz4;
 	 //sz.Format(_T("nRendHmul: %d   nRendVMul: %d \r\n"),  m_Info.nRendHMul,  m_Info.nRendVMul);
 	 //sz2.Format(_T("width: %d   height: %d   bordeAncho: %d  bordeAlto: %d  \r\n "),  width,  height, bordeAncho, bordeAlto);
 	 //sz3.Format(_T("nWidth: %d   nHeight: %d   nRendWidth: %d   nRendHeight: %d \r\n"),  m_Info.nWidth,  m_Info.nHeight, m_Info.nRendWidth,  m_Info.nRendHeight);
 	 //sz4.Format(_T("rect.Width(): %d   rect.Height(): %d  cx:%d cy:%d\r\n\r\n\r\n"),  rect.Width(),  rect.Height(),cx,cy );
 	 //OutputDebugStringW(CT2W(sz));
 	 //OutputDebugStringW(CT2W(sz2));
 	 //OutputDebugStringW(CT2W(sz3));
 	 //OutputDebugStringW(CT2W(sz4));
	    
	
	/* ACA SE DETERMINAN LAS ESQUINAS SUPERIORES IZQ Y TOP DEL FRAME PRINCIPAL */ 
	
		if (m_Info.bBltStretch) 		
			m_Info.nLeft = (bordeAncho > 0) ? (bordeAncho >> 3) : bordeAncho;
		else 
		    m_Info.nLeft = (rect.Width() - width) >> 1;
	
	
		if (m_Info.bBltStretch) 	
			m_Info.nTop = (bordeAlto > 0) ? (bordeAlto >> 3) : bordeAlto; 
		else
			m_Info.nTop = (rect.Height() - height) >> 1;
	


 	 // Especificar dibujo de la zona
	m_Info.bBltAll = TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Modo de aumento
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::Stretch(BOOL bStretch)
{
	CRect rect;

	ASSERT(this);

	/*char cadena[20];	  
    sprintf(cadena, "%d", bStretch);
	 int msgboxID = MessageBox(
       cadena,"Stretch",
        2 );*/

 	 // Si coincide, no hacer nada
	if (bStretch == m_Info.bBltStretch) {
		return;
	}
	m_Info.bBltStretch = bStretch;

 	 // Recalcular si no es 768x512
 	if ((m_Info.nRendWidth > 0) && (m_Info.nRendWidth < 768)) {		 // Si se requiere mejora de declaracion
		m_Info.nRendWidth = m_Info.pWork->width + 1;
		GetClientRect(&rect);
		ReCalc(rect);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Dibujar rectangulos de bordes en esquinas
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::DrawRect(CDC *pDC)
{
	CRect crect;
	CRect brect;

	ASSERT(m_Info.bBltAll);
	ASSERT(pDC);

 	 // Si se usan todos, no se necesitan
	if ((m_Info.nLeft == 0) && (m_Info.nTop == 0)) {
		return;
	}

 	 // Obtener rectangulo del cliente
	GetClientRect(&crect);


	/* ACA SE ESTABLECEN LOS BORDES EXTERIORES DE VENTANA DE JUEGO Y SUS COLORES */
	if (m_Info.nLeft > 0) {
 		 // Mitad izquierda
		brect.left = 0;
		brect.top = 0;
		brect.right = m_Info.nLeft;
		brect.bottom = crect.bottom;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));

 		 // Mitad derecha
		brect.right = crect.right;
		brect.left = brect.right - m_Info.nLeft - 1;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));
	}

	if (m_Info.nTop > 0) {
 		 // Mitad superior
		brect.left = 0;
		brect.top = 0;
		brect.right = crect.right;
		brect.bottom = m_Info.nTop;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));

 		 // Mitad derecha
		brect.bottom = crect.bottom;
		brect.top = brect.bottom - m_Info.nTop - 1;
		pDC->FillSolidRect(&brect, RGB(0, 0, 0));
	}
}

  //---------------------------------------------------------------------------
  //
  //	Comprobacion de rango de dibujo
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::CalcRect()
{
	int i;
	int j;
	int left;
	int top;
	int right;
	int bottom;
	BOOL *p;
	BOOL flag;

 	 // Inicializacion
	left = 64;
	top = 2048;
	right = -1;
	bottom = -1;
	p = m_Info.pWork->drawflag;

 	 // Bucle y
	for (i=0; i<m_Info.nHeight; i++) {
		flag = FALSE;

 		 // Bucle x
		for(j=0; j<64; j++) {
			if (*p) {
 				 // Borrar
				*p = FALSE;

 				 // Estos 16dot necesitan dibujarse
				if (left > j) {
					left = j;
				}
				if (right < j) {
					right = j;
				}
				flag = TRUE;
			}
			p++;
		}

		if (flag) {
 			 // Necesario trazar esta linea
			if (top > i) {
				top = i;
			}
			if (bottom < i) {
				bottom = i;
			}
		}
	}

 	 // No necesario si y no cambia
	if (bottom < top) {
		return FALSE;
	}

 	 // Correccion (x16)
	left <<= 4;
	right = ((right + 1) << 4) - 1;
	if (right >= m_Info.nWidth) {
		right = m_Info.nWidth - 1;
	}

 	 // Copiar
	m_Info.nBltLeft = left;
	m_Info.nBltTop = top;
	m_Info.nBltRight = right;
	m_Info.nBltBottom = bottom;

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener nuevo indice de subventana
  //
  //---------------------------------------------------------------------------
int FASTCALL CDrawView::GetNewSWnd() const
{
	CSubWnd *pWnd;
	int nSubWnd;

	ASSERT(this);
	ASSERT_VALID(this);

 	 // ?Es la primera subventana?
	if (!m_pSubWnd) {
		return 0;
	}

 	 // Inicializacion
	nSubWnd = 1;
	pWnd = m_pSubWnd;

 	 // Bucle
	while (pWnd->m_pNextWnd) {
		pWnd = pWnd->m_pNextWnd;
		nSubWnd++;
	}

 	 // Devolver indice
	return nSubWnd;
}

  //---------------------------------------------------------------------------
  //
  //	Anadir subventana
  //	Llamada desde CSubWnd que se desea anadir
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::AddSWnd(CSubWnd *pSubWnd)
{
	CSubWnd *pWnd;

	ASSERT(this);
	ASSERT(pSubWnd);
	ASSERT_VALID(this);

 	 // ?Es la primera subventana?
	if (!m_pSubWnd) {
 		 // Esta es la primera. Registrar
		m_pSubWnd = pSubWnd;
		ASSERT(!pSubWnd->m_pNextWnd);
		return;
	}

 	 // Buscar final
	pWnd = m_pSubWnd;
	while (pWnd->m_pNextWnd) {
		pWnd = pWnd->m_pNextWnd;
	}

 	 // Anadir despues de pWnd
	pWnd->m_pNextWnd = pSubWnd;
	ASSERT(!pSubWnd->m_pNextWnd);
}

  //---------------------------------------------------------------------------
  //
  //	Eliminar subventana
  //	Llamada desde CSubWnd que se desea eliminar
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::DelSWnd(CSubWnd *pSubWnd)
{
	CSubWnd *pWnd;

 	 // Assert
	ASSERT(pSubWnd);

 	 // Bloquear VM
	::LockVM();

 	 // ?Es la primera subventana?
	if (m_pSubWnd == pSubWnd) {
 		 // Si hay siguiente, registrar. Si no, NULL
		if (pSubWnd->m_pNextWnd) {
			m_pSubWnd = pSubWnd->m_pNextWnd;
		}
		else {
			m_pSubWnd = NULL;
		}
		::UnlockVM();
		return;
	}

 	 // Buscar subventana que recuerda a pSubWnd
	pWnd = m_pSubWnd;
	while (pWnd->m_pNextWnd != pSubWnd) {
		ASSERT(pWnd->m_pNextWnd);
		pWnd = pWnd->m_pNextWnd;
	}

 	 // Vincular pSubWnd->m_pNextWnd a pWnd y saltar
	pWnd->m_pNextWnd = pSubWnd->m_pNextWnd;

 	 // Desbloquear VM
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Eliminar todas las subventanas
  //
  //---------------------------------------------------------------------------
void FASTCALL CDrawView::ClrSWnd()
{
	CSubWnd *pWnd;
    CSubWnd *pNext;

	ASSERT(this);

 	 // Obtener primera subventana
	pWnd = GetFirstSWnd();

 	 // Bucle
	while (pWnd) {
 		 // Obtener siguiente
		pNext = pWnd->m_pNextWnd;

 		 // Eliminar esta ventana
		pWnd->DestroyWindow();

 		 // Mover
		pWnd = pNext;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtener primera subventana
  //	Si no existe, devuelve NULL
  //
  //---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::GetFirstSWnd() const
{
	return m_pSubWnd;
}

  //---------------------------------------------------------------------------
  //
  //	Buscar subventana
  //	Devuelve NULL si no se encuentra
  //
  //---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::SearchSWnd(DWORD dwID) const
{
	CSubWnd *pWnd;

 	 // Inicializar ventana
	pWnd = m_pSubWnd;

 	 // Bucle de busqueda
	while (pWnd) {
 		 // Comprobar si ID coincide
		if (pWnd->GetID() == dwID) {
			return pWnd;
		}

 		 // Siguiente
		pWnd = pWnd->m_pNextWnd;
	}

 	 // No encontrada
	return NULL;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener fuente de texto
  //
  //---------------------------------------------------------------------------
CFont* FASTCALL CDrawView::GetTextFont()
{
	ASSERT(m_TextFont.m_hObject);

	return &m_TextFont;
}

  //---------------------------------------------------------------------------
  //
  //	Crear nueva ventana
  //
  //---------------------------------------------------------------------------
CSubWnd* FASTCALL CDrawView::NewWindow(BOOL bDis)
{
	DWORD dwID;
	int i;
	CSubWnd *pWnd;
	CDisasmWnd *pDisWnd;
	CMemoryWnd *pMemWnd;

 	 // Crear ID base
	if (bDis) {
		dwID = MAKEID('D', 'I', 'S', 'A');
	}
	else {
		dwID = MAKEID('M', 'E', 'M', 'A');
	}

 	 // Bucle de 8 iteraciones
	for (i=0; i<8; i++) {
	
		pWnd = SearchSWnd(dwID);
		if (!pWnd) {
			if (bDis) {
				pDisWnd = new CDisasmWnd(i);
				VERIFY(pDisWnd->Init(this));
				return pDisWnd;
			}
			else {
				pMemWnd = new CMemoryWnd(i);
				VERIFY(pMemWnd->Init(this));
				return pMemWnd;
			}
		}

 		 // Crear siguiente ID de ventana
		dwID++;
	}

 	 // No se pudo crear
	return NULL;
}

  //---------------------------------------------------------------------------
  //
  //	Comprobar si se puede crear nueva ventana
  //
  //---------------------------------------------------------------------------
BOOL FASTCALL CDrawView::IsNewWindow(BOOL bDis)
{
	DWORD dwID;
	int i;
	CSubWnd *pWnd;

 	 // Crear ID base
	if (bDis) {
		dwID = MAKEID('D', 'I', 'S', 'A');
	}
	else {
		dwID = MAKEID('M', 'E', 'M', 'A');
	}

 	 // Bucle de 8 iteraciones
	for (i=0; i<8; i++) {
 		 // Si no se encuentra ventana, se puede crear nueva
		pWnd = SearchSWnd(dwID);
		if (!pWnd) {
			return TRUE;
		}

 		 // Crear siguiente ID de ventana
		dwID++;
	}

 	 // Todas las ventanas existen -> no se puede crear
	return FALSE;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener numero de subventanas
  //	Devuelve NULL si no se encuentra
  //
  //---------------------------------------------------------------------------
int FASTCALL CDrawView::GetSubWndNum() const
{
	CSubWnd *pWnd;
	int num;

 	 // Inicializacion
	pWnd = m_pSubWnd;
	num = 0;

 	 // Bucle
	while (pWnd) {
 		 // Cantidad++
		num++;

 		 // Siguiente
		pWnd = pWnd->m_pNextWnd;
	}

	return num;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener nombre de clase de ventana
  //
  //---------------------------------------------------------------------------
LPCTSTR FASTCALL CDrawView::GetWndClassName() const
{
	ASSERT(this);
	ASSERT(m_pFrmWnd);
	return m_pFrmWnd->GetWndClassName();
}

 #endif	 // _WIN32
