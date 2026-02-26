  //---------------------------------------------------------------------------
  //
  //	EMULADOR X68000 "XM6"
  //
  //	Copyright (C) 2001-2006 PI.(ytanaka@ipc-tokai.or.jp)
  //	[ Subventana MFC (CPU) ]
  //
  //---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "cpu.h"
#include "memory.h"
#include "schedule.h"
#include "mfp.h"
#include "scc.h"
#include "dmac.h"
#include "iosc.h"
#include "render.h"
#include "mfc_sub.h"
#include "mfc_draw.h"
#include "mfc_res.h"
#include "mfc_cpu.h"

  //---------------------------------------------------------------------------
  //
  //	Comunicacion con cpudebug.c
  //
  //---------------------------------------------------------------------------
#if defined(__cplusplus)
extern "C" {
 #endif	 // __cplusplus

void cpudebug_disassemble(int n);
 										 // Desensamblado de 1 linea
extern void (*cpudebug_put)(const char*);
 										 // Salida de 1 linea
extern DWORD debugpc;
 										 // PC de desensamblado

#if defined(__cplusplus)
}
 #endif	 // __cplusplus

static char debugbuf[0x200];
 										 // Buffer de salida

  //===========================================================================
  //
  //	Dialogo con historial
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CHistoryDlg::CHistoryDlg(UINT nID, CWnd *pParentWnd) : CDialog(nID, pParentWnd)
{
 	 // Inicializacion
	m_dwValue = 0;
	m_nBit = 32;
}

  //---------------------------------------------------------------------------
  //
  //	Inicializacion de dialogo
  //
  //---------------------------------------------------------------------------
BOOL CHistoryDlg::OnInitDialog()
{
	int i;
	int nNum;
	DWORD *pData;
	CString strText;
	CComboBox *pComboBox;

 	 // Clase base
	if (!CDialog::OnInitDialog()) {
		return FALSE;
	}

 	 // Generacion de mascara
	m_dwMask = 0;
	for (i=0; i<(int)m_nBit; i++) {
		m_dwMask <<= 1;
		m_dwMask |= 0x01;
	}

 	 // Limpiar combo box
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->ResetContent();

 	 // Anadir combo box
	nNum = *(int *)GetNumPtr();
	pData = GetDataPtr();
	for (i=0; i<nNum; i++) {
		if (pData[i] > m_dwMask) {
 			 // Al ser mayor que la mascara, 32 bits esta bien por defecto
			strText.Format(_T("%08X"), pData[i]);
		}
		else {
 			 // Menor o igual que la mascara
			switch (m_nBit) {
				case 8:
					strText.Format(_T("%02X"), pData[i]);
					break;
				case 16:
					strText.Format(_T("%04X"), pData[i]);
					break;
				case 24:
					strText.Format(_T("%06X"), pData[i]);
					break;
				default:
					strText.Format(_T("%08X"), pData[i]);
					break;
			}
		}
 		 // Anadir
		pComboBox->AddString(strText);
	}

 	 // dwValue siempre es mascara
	m_dwValue &= m_dwMask;
	switch (m_nBit) {
		case 8:
			strText.Format(_T("%02X"), m_dwValue);
			break;
		case 16:
			strText.Format(_T("%04X"), m_dwValue);
			break;
		case 24:
			strText.Format(_T("%06X"), m_dwValue);
			break;
		default:
			strText.Format(_T("%08X"), m_dwValue);
			break;
	}
	pComboBox->SetWindowText(strText);

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Dialogo OK
  //
  //---------------------------------------------------------------------------
void CHistoryDlg::OnOK()
{
	CComboBox *pComboBox;
	CString strText;
	int i;
	int nHit;
	int nNum;
	DWORD *pData;

 	 // Obtener valor numerico de entrada
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->GetWindowText(strText);
	m_dwValue = _tcstoul((LPCTSTR)strText, NULL, 16);

 	 // Comprobar si es igual a lo ya ingresado
	nNum = *(int *)GetNumPtr();
	pData = GetDataPtr();
	nHit = -1;
	for (i=0; i<nNum; i++) {
		if (pData[i] == m_dwValue) {
			nHit = i;
			break;
		}
	}

 	 // Nuevo o adoptado
	if (nHit >= 0) {
 		 // Igual al existente. Intercambiar lugar
		for (i=(nHit - 1); i>=0; i--) {
			pData[i + 1] = pData[i];
		}
		pData[0] = m_dwValue;
	}
	else {
 		 // Nuevo. Bajar el existente un nivel
		for (i=9; i>=1; i--) {
			pData[i] = pData[i - 1];
		}

 		 // Poner el mas reciente en [0]
		pData[0] = m_dwValue;

 		 // Se pueden anadir hasta 10
		if (nNum < 10) {
			*(int *)GetNumPtr() = (nNum + 1);
		}
	}

 	 // Enmascarar dwValue y OK
	m_dwValue &= m_dwMask;

 	 // Clase base
	CDialog::OnOK();
}

  //===========================================================================
  //
  //	Dialogo de entrada de direccion
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CAddrDlg::CAddrDlg(CWnd *pParent) : CHistoryDlg(IDD_ADDRDLG, pParent)
{
 	 // Soporte para entorno en ingles
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_ADDRDLG);
		m_nIDHelp = IDD_US_ADDRDLG;
	}

	m_nBit = 24;
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu
  //
  //---------------------------------------------------------------------------
void CAddrDlg::SetupHisMenu(CMenu *pMenu)
{
	int i;
	CString string;

	ASSERT(pMenu);

 	 // Cadena de menu
	for (i=0; i<(int)m_Num; i++) {
		string.Format("$%06X", m_Data[i]);
		pMenu->ModifyMenu(IDM_HISTORY_0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_HISTORY_0 + i, (LPCTSTR)string);
	}

 	 // Eliminar menu
	for (i=m_Num; i<10; i++) {
		pMenu->DeleteMenu(IDM_HISTORY_0 + i, MF_BYCOMMAND);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtener resultado de menu
  //
  //---------------------------------------------------------------------------
DWORD CAddrDlg::GetAddr(UINT nID)
{
	DWORD dwAddr;
    int i;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

 	 // Obtener direccion
	nID -= IDM_HISTORY_0;
	ASSERT(nID < 10);
	dwAddr = m_Data[nID];

 	 // Intercambiar lugar
	for (i=(int)(nID - 1); i>=0; i--) {
		m_Data[i + 1] = m_Data[i];
	}
	m_Data[0] = dwAddr;

	return dwAddr;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de conteo de historial
  //
  //---------------------------------------------------------------------------
UINT* CAddrDlg::GetNumPtr()
{
	return &m_Num;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de datos de historial
  //
  //---------------------------------------------------------------------------
DWORD* CAddrDlg::GetDataPtr()
{
	return m_Data;
}

  //---------------------------------------------------------------------------
  //
  //	Conteo de historial
  //
  //---------------------------------------------------------------------------
UINT CAddrDlg::m_Num = 0;

  //---------------------------------------------------------------------------
  //
  //	Datos de historial
  //
  //---------------------------------------------------------------------------
DWORD CAddrDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

  //===========================================================================
  //
  //	Dialogo de entrada de registros
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CRegDlg::CRegDlg(CWnd *pParent) : CHistoryDlg(IDD_REGDLG, pParent)
{
 	 // Soporte para entorno en ingles
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_REGDLG);
		m_nIDHelp = IDD_US_REGDLG;
	}

	m_nIndex = 0;
	m_nBit = 32;
}

  //---------------------------------------------------------------------------
  //
  //	Inicializacion de dialogo
  //
  //---------------------------------------------------------------------------
BOOL CRegDlg::OnInitDialog()
{
	CWnd *pWnd;
	CString strRegister;
	CPU *pCPU;
	CPU::cpu_t reg;

	ASSERT(this);
	ASSERT(m_nIndex < 20);

 	 // Obtener registros de CPU
	::LockVM();
	pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(pCPU);
	pCPU->GetCPU(&reg);
	::UnlockVM();

 	 // Crear cadena
	if (m_nIndex <= 7) {
		strRegister.Format("D%d", m_nIndex);
		m_dwValue = reg.dreg[m_nIndex];
	}
	if ((m_nIndex >= 8) && (m_nIndex <= 15)) {
		strRegister.Format("A%d", m_nIndex & 7);
		m_dwValue = reg.areg[m_nIndex & 7];
	}
	switch (m_nIndex) {
 		 // USP
		case 16:
			strRegister = "USP";
			if (reg.sr & 0x2000) {
				m_dwValue = reg.sp;
			}
			else {
				m_dwValue = reg.areg[7];
			}
			break;
 		 // SSP
		case 17:
			strRegister = "SSP";
			if (reg.sr & 0x2000) {
				m_dwValue = reg.areg[7];
			}
			else {
				m_dwValue = reg.sp;
			}
			break;
 		 // PC
		case 18:
			strRegister = "PC";
			m_dwValue = reg.pc;
			break;
 		 // SR
		case 19:
			strRegister = "SR";
			m_dwValue = reg.sr;
			break;
	}

 	 // Llamar a la clase base aqui
	if (!CHistoryDlg::OnInitDialog()) {
		return FALSE;
	}

 	 // Establecer valor
	pWnd = GetDlgItem(IDC_ADDR_ADDRL);
	ASSERT(pWnd);
	pWnd->SetWindowText(strRegister);

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	OK
  //
  //---------------------------------------------------------------------------
void CRegDlg::OnOK()
{
	CComboBox *pComboBox;
	CString string;
	DWORD dwValue;
	CPU *pCPU;
	CPU::cpu_t reg;

	ASSERT(this);

 	 // Obtener valor
	pComboBox = (CComboBox*)GetDlgItem(IDC_ADDR_ADDRE);
	ASSERT(pComboBox);
	pComboBox->GetWindowText(string);
	dwValue = ::strtoul((LPCTSTR)string, NULL, 16);

 	 // Bloqueo de VM, obtener registros
	::LockVM();
	pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(pCPU);
	pCPU->GetCPU(&reg);

 	 // Por indice
	if (m_nIndex <= 7) {
		reg.dreg[m_nIndex] = dwValue;
	}
	if ((m_nIndex >= 8) && (m_nIndex <= 15)) {
		reg.areg[m_nIndex & 7] = dwValue;
	}
	switch (m_nIndex) {
 		 // USP
		case 16:
			if (reg.sr & 0x2000) {
				reg.sp = dwValue;
			}
			else {
				reg.areg[7] = dwValue;
			}
			break;
 		 // SSP
		case 17:
			if (reg.sr & 0x2000) {
				reg.areg[7] = dwValue;
			}
			else {
				reg.sp = dwValue;
			}
			break;
 		 // PC
		case 18:
			reg.pc = dwValue & 0xfffffe;
			break;
 		 // SR
		case 19:
			reg.sr = (WORD)dwValue;
			break;
	}

 	 // Establecer registros, desbloqueo de VM
	pCPU->SetCPU(&reg);
	::UnlockVM();

 	 // Clase base
	CHistoryDlg::OnOK();
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de conteo de historial
  //
  //---------------------------------------------------------------------------
UINT* CRegDlg::GetNumPtr()
{
	return &m_Num;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de datos de historial
  //
  //---------------------------------------------------------------------------
DWORD* CRegDlg::GetDataPtr()
{
	return m_Data;
}

  //---------------------------------------------------------------------------
  //
  //	Conteo de historial
  //
  //---------------------------------------------------------------------------
UINT CRegDlg::m_Num = 0;

  //---------------------------------------------------------------------------
  //
  //	Datos de historial
  //
  //---------------------------------------------------------------------------
DWORD CRegDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

  //===========================================================================
  //
  //	Dialogo de entrada de datos
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CDataDlg::CDataDlg(CWnd *pParent) : CHistoryDlg(IDD_DATADLG, pParent)
{
 	 // Soporte para entorno en ingles
	if (!::IsJapanese()) {
		m_lpszTemplateName = MAKEINTRESOURCE(IDD_US_DATADLG);
		m_nIDHelp = IDD_US_DATADLG;
	}

 	 // Inicializacion por si acaso
	m_dwAddr = 0;
	m_nSize = 0;
}

  //---------------------------------------------------------------------------
  //
  //	Inicializacion de dialogo
  //
  //---------------------------------------------------------------------------
BOOL CDataDlg::OnInitDialog()
{
	CWnd *pWnd;
	CString string;

	ASSERT(this);
	ASSERT(m_dwAddr < 0x1000000);

 	 // Determinar direccion y numero de bits
	switch (m_nSize) {
 		 // Byte
		case 0:
			string.Format("$%06X (B)", m_dwAddr);
			m_nBit = 8;
			break;
 		 // Word
		case 1:
			string.Format("$%06X (W)", m_dwAddr);
			m_nBit = 16;
			break;
 		 // Long
		case 2:
			string.Format("$%06X (L)", m_dwAddr);
			m_nBit = 32;
			break;
 		 // Otros
		default:
			ASSERT(FALSE);
			break;
	}

 	 // Clase base
	if (!CHistoryDlg::OnInitDialog()) {
		return FALSE;
	}

 	 // Configurar tras establecer la inicializacion
	pWnd = GetDlgItem(IDC_ADDR_ADDRL);
	ASSERT(pWnd);
	pWnd->SetWindowText(string);

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de conteo de historial
  //
  //---------------------------------------------------------------------------
UINT* CDataDlg::GetNumPtr()
{
	return &m_Num;
}

  //---------------------------------------------------------------------------
  //
  //	Obtener puntero de datos de historial
  //
  //---------------------------------------------------------------------------
DWORD* CDataDlg::GetDataPtr()
{
	return m_Data;
}

  //---------------------------------------------------------------------------
  //
  //	Conteo de historial
  //
  //---------------------------------------------------------------------------
UINT CDataDlg::m_Num = 0;

  //---------------------------------------------------------------------------
  //
  //	Datos de historial
  //
  //---------------------------------------------------------------------------
DWORD CDataDlg::m_Data[10] = {
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

  //===========================================================================
  //
  //	Ventana de registros de CPU
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CCPURegWnd::CCPURegWnd()
{
 	 // Definicion de parametros de ventana
	m_dwID = MAKEID('M', 'P', 'U', 'R');
	::GetMsg(IDS_SWND_CPUREG, m_strCaption);
	m_nWidth = 27;
	m_nHeight = 10;

 	 // Obtener CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CCPURegWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_SR, OnReg)
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Configuracion
  //
  //---------------------------------------------------------------------------
void FASTCALL CCPURegWnd::Setup()
{
	CPU::cpu_t buf;
	int i;
	CString string;

	ASSERT(this);

 	 // Limpiar
	Clear();

 	 // Obtener registros de CPU
	m_pCPU->GetCPU(&buf);

 	 // Set (D, A)
	for (i=0; i<8; i++) {
		string.Format("D%1d  %08X", i, buf.dreg[i]);
		SetString(0, i, string);

		string.Format("A%1d  %08X", i, buf.areg[i]);
		SetString(15, i, string);
	}

 	 // Set (Stack)
	if (buf.sr & 0x2000) {
		string.Format("USP %08X", buf.sp);
		SetString(0, 8, string);
		string.Format("SSP %08X", buf.areg[7]);
		SetString(15, 8, string);
	}
	else {
		string.Format("USP %08X", buf.areg[7]);
		SetString(0, 8, string);
		string.Format("SSP %08X", buf.sp);
		SetString(15, 8, string);
	}

 	 // Set (Otros)
	string.Format("PC    %06X", buf.pc);
	SetString(0, 9, string);
	string.Format("SR      %04X", buf.sr);
	SetString(15, 9, string);
}

  //---------------------------------------------------------------------------
  //
  //	Doble clic con boton izquierdo
  //
  //---------------------------------------------------------------------------
void CCPURegWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int x;
	int y;
	int index;
	CRegDlg dlg(this);

 	 // Calcular x, y
	x = point.x / m_tmWidth;
	y = point.y / m_tmHeight;

 	 // Obtener indice
	if (y < 8) {
		if (x < 15) {
 			 // D0-D7
			index = y;
		}
		else {
 			 // A0-A7
			index = y + 8;
		}
	}
	else {
		index = y - 8;
		index <<= 1;
		if (x >= 15) {
			index++;
		}
		index += 16;
	}

	printf("%d", nFlags);

 	 // Ejecutar dialogo
	dlg.m_nIndex = index;
	dlg.DoModal();
}

  //---------------------------------------------------------------------------
  //
  //	Menu de contexto
  //
  //---------------------------------------------------------------------------
void CCPURegWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

 	 // Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextWnd::OnContextMenu(pWnd, point);
		return;
	}

 	 // Cargar menu
	menu.LoadMenu(IDR_REGMENU);
	pMenu = menu.GetSubMenu(0);

 	 // Configuracion de menu
	SetupRegMenu(pMenu, m_pCPU, TRUE);

 	 // Ejecutar menu
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu
  //
  //---------------------------------------------------------------------------
void CCPURegWnd::SetupRegMenu(CMenu *pMenu, CPU *pCPU, BOOL bSR)
{
	int i;
	CString string;
	CPU::cpu_t reg;

	ASSERT(pMenu);
	ASSERT(pCPU);

 	 // Obtener registros de CPU
	::LockVM();
	pCPU->GetCPU(&reg);
	::UnlockVM();

 	 // Configuracion (D)
	for (i=0; i<8; i++) {
		string.Format("D%1d ($%08X)", i, reg.dreg[i]);
		pMenu->ModifyMenu(IDM_REG_D0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_REG_D0 + i, (LPCTSTR)string);
	}
 	 // Configuracion (A)
	for (i=0; i<8; i++) {
		string.Format("A%1d ($%08X)", i, reg.areg[i]);
		pMenu->ModifyMenu(IDM_REG_A0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_REG_A0 + i, (LPCTSTR)string);
	}
 	 // Configuracion (USP, SSP)
	if (reg.sr & 0x2000) {
		string.Format("USP ($%08X)", reg.sp);
		pMenu->ModifyMenu(IDM_REG_USP, MF_BYCOMMAND | MF_STRING, IDM_REG_USP, (LPCTSTR)string);
		string.Format("SSP ($%08X)", reg.areg[7]);
		pMenu->ModifyMenu(IDM_REG_SSP, MF_BYCOMMAND | MF_STRING, IDM_REG_SSP, (LPCTSTR)string);
	}
	else {
		string.Format("USP ($%08X)", reg.areg[7]);
		pMenu->ModifyMenu(IDM_REG_USP, MF_BYCOMMAND | MF_STRING, IDM_REG_USP, (LPCTSTR)string);
		string.Format("SSP ($%08X)", reg.sp);
		pMenu->ModifyMenu(IDM_REG_SSP, MF_BYCOMMAND | MF_STRING, IDM_REG_SSP, (LPCTSTR)string);
	}

 	 // Configuracion (PC)
	string.Format("PC ($%06X)", reg.pc);
	pMenu->ModifyMenu(IDM_REG_PC, MF_BYCOMMAND | MF_STRING, IDM_REG_PC, (LPCTSTR)string);

 	 // Configuracion (SR)
	if (bSR) {
		string.Format("SR ($%04X)", reg.sr);
		pMenu->ModifyMenu(IDM_REG_SR, MF_BYCOMMAND | MF_STRING, IDM_REG_SR, (LPCTSTR)string);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtener valor de registro
  //
  //---------------------------------------------------------------------------
DWORD CCPURegWnd::GetRegValue(CPU *pCPU, UINT uID)
{
	CPU::cpu_t reg;

	ASSERT(pCPU);
	ASSERT((uID >= IDM_REG_D0) && (uID <= IDM_REG_SR));

 	 // Obtener registros de CPU
	::LockVM();
	pCPU->GetCPU(&reg);
	::UnlockVM();

 	 // D0 a D7
	if (uID <= IDM_REG_D7) {
		return reg.dreg[uID - IDM_REG_D0];
	}

 	 // A0 a A7
	if (uID <= IDM_REG_A7) {
		return reg.areg[uID - IDM_REG_A0];
	}

 	 // USP
	if (uID == IDM_REG_USP) {
		if (reg.sr & 0x2000) {
			return reg.sp;
		}
		else {
			return reg.areg[7];
		}
	}

 	 // SSP
	if (uID == IDM_REG_SSP) {
		if (reg.sr & 0x2000) {
			return reg.areg[7];
		}
		else {
			return reg.sp;
		}
	}

 	 // PC
	if (uID == IDM_REG_PC) {
		return reg.pc;
	}

 	 // SR
	ASSERT(uID == IDM_REG_SR);
	return reg.sr;
}

  //---------------------------------------------------------------------------
  //
  //	Comando de registro
  //
  //---------------------------------------------------------------------------
void CCPURegWnd::OnReg(UINT nID)
{
	CRegDlg dlg(this);

	ASSERT((nID >= IDM_REG_D0) && (nID <= IDM_REG_SR));

 	 // Convertir
	nID -= IDM_REG_D0;

 	 // Dejar a cargo del dialogo
	dlg.m_nIndex = nID;
	dlg.DoModal();
}

  //===========================================================================
  //
  //	Ventana de interrupcion
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CIntWnd::CIntWnd()
{
 	 // Definicion de parametros de ventana
	m_dwID = MAKEID('I', 'N', 'T', ' ');
	::GetMsg(IDS_SWND_INT, m_strCaption);
	m_nWidth = 47;
	m_nHeight = 9;

 	 // Obtener CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CIntWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Configuracion
  //
  //---------------------------------------------------------------------------
void FASTCALL CIntWnd::Setup()
{
	static const char *desc_table[] = {
		"INTSW",
		"MFP",
		"SCC",
		"(Ext.)",
		"DMAC",
		"(Ext.)",
		"IOSC"
	};
	int y;
	int i;
	int level;
	CString string;
	CPU::cpu_t cpu;

 	 // Obtener datos de CPU
	m_pCPU->GetCPU(&cpu);
	level = (cpu.sr >> 8);
	level &= 0x07;

 	 // Limpiar
	Clear();
	y = 0;

 	 // Guia
	SetString(0, y, "(High)  Device  Mask  Vector     Req        Ack");
	y++;

 	 // Procesamiento de 7 niveles
	for (i=7; i>=1; i--) {
 		 // Establecer nombre de interrupcion
		string.Format("Level%1d  ", i);
		string += desc_table[7 - i];
		SetString(0, y, string);

 		 // Mascara
		if (i < 7) {
			if (i <= level) {
				SetString(16, y, "Mask");
			}
		}

 		 // Esta en peticion?
		if (cpu.intr[0] & 0x80) {
 			 // Hay peticion. Mostrar vector
			string.Format("$%02X", cpu.intr[i]);
			SetString(22, y, string);
		}

 		 // Contador de peticiones
		string.Format("%10d", cpu.intreq[i]);
		SetString(26, y, string);
		
 		 // Contador de respuestas
		string.Format("%10d", cpu.intack[i]);
		SetString(37, y, string);

 		 // Siguiente
		y++;
		cpu.intr[0] <<= 1;
	}

 	 // Guia
	SetString(0, y, "(Low)");
}

  //---------------------------------------------------------------------------
  //
  //	Doble clic con boton izquierdo
  //
  //---------------------------------------------------------------------------
void CIntWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int y;
	int level;
	CPU::cpu_t cpu;

 	 // Calcular y
	y = point.y / m_tmHeight;

 	 // y=1,2,3,4,5,6,7 corresponden respectivamente a int7,6,5,4,3,2,1
	level = 8 - y;
	if ((level < 1) || (level > 7)) {
		return;
	}

 	 // Bloqueo, obtener datos
	::LockVM();
	m_pCPU->GetCPU(&cpu);

 	 // Limpiar
	ASSERT((level >= 1) && (level <= 7));
	cpu.intreq[level] = 0;
	cpu.intack[level] = 0;

 	 // Establecer datos, desbloqueo
	m_pCPU->SetCPU(&cpu);
	printf("%d", nFlags);
	::UnlockVM();
}

  //===========================================================================
  //
  //	Ventana de desensamblado
  //
  //===========================================================================

#if defined(__cplusplus)
extern "C" {
#endif
  //---------------------------------------------------------------------------
  //
  //	cpudebug.c Salida de cadena
  //
  //---------------------------------------------------------------------------
void disasm_put(const char *s)
{
	strcpy(debugbuf, s);
}

  //---------------------------------------------------------------------------
  //
  //	Dispositivo de memoria
  //
  //---------------------------------------------------------------------------
static Memory* cpudebug_memory;

  //---------------------------------------------------------------------------
  //
  //	cpudebug.c Lectura de palabra
  //
  //---------------------------------------------------------------------------
WORD cpudebug_fetch(DWORD addr)
{
	WORD w;

	ASSERT(cpudebug_memory);

	addr &= 0xfffffe;
	w = (WORD)(cpudebug_memory->ReadOnly(addr));
	w <<= 8;
	w |= cpudebug_memory->ReadOnly(addr + 1);

	return w;
}
#if defined(__cplusplus)
};
#endif

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CDisasmWnd::CDisasmWnd(int index)
{
 	 // Hasta 8 tipos de ventanas de desensamblado
	ASSERT((index >= 0) && (index <= 0x07));

 	 // Definicion de parametros de ventana
	m_dwID = MAKEID('D', 'I', 'S', (index + 'A'));
	::GetMsg(IDS_SWND_DISASM, m_strCaption);
	m_nWidth = 70;
	m_nHeight = 16;

 	 // Definicion de parametros de ventana (scroll)
	m_ScrlWidth = 70;
	m_ScrlHeight = 0x8000;

 	 // La primera ventana tiene sincronizacion con PC, las demas no
	if (index == 0) {
		m_bSync = TRUE;
	}
	else {
		m_bSync = FALSE;
	}

 	 // Otros
	m_pAddrBuf = NULL;
	m_Caption = m_strCaption;
	m_CaptionSet = "";

 	 // Obtener dispositivo
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);
	cpudebug_memory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(cpudebug_memory);
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
	m_pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(m_pMFP);
	m_pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(m_pMemory);
	m_pSCC = (SCC*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'C', ' '));
	ASSERT(m_pSCC);
	m_pDMAC = (DMAC*)::GetVM()->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(m_pDMAC);
	m_pIOSC = (IOSC*)::GetVM()->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(m_pIOSC);

 	 // Inicializar direccion al PC
	m_dwSetAddr = m_pCPU->GetPC();
	m_dwAddr = m_dwSetAddr;
	m_dwAddr = m_dwAddr & 0xff0000;
	m_dwPC = 0xffffffff;

 	 // Conexion de buffer de desensamblado
	::cpudebug_put = ::disasm_put;
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CDisasmWnd, CSubTextScrlWnd)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_VSCROLL()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_DIS_NEWWIN, OnNewWin)
	ON_COMMAND(IDM_DIS_PC, OnPC)
	ON_COMMAND(IDM_DIS_SYNC, OnSync)
	ON_COMMAND(IDM_DIS_ADDR, OnAddr)
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_PC, OnReg)
	ON_COMMAND_RANGE(IDM_STACK_0, IDM_STACK_F, OnStack)
	ON_COMMAND_RANGE(IDM_DIS_BREAKP0, IDM_DIS_BREAKP7, OnBreak)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND_RANGE(IDM_DIS_RESET, IDM_DIS_FLINE, OnCPUExcept)
	ON_COMMAND_RANGE(IDM_DIS_TRAP0, IDM_DIS_TRAPF, OnTrap)
	ON_COMMAND_RANGE(IDM_DIS_MFP0, IDM_DIS_MFPF, OnMFP)
	ON_COMMAND_RANGE(IDM_DIS_SCC0, IDM_DIS_SCC7, OnSCC)
	ON_COMMAND_RANGE(IDM_DIS_DMAC0, IDM_DIS_DMAC7, OnDMAC)
	ON_COMMAND_RANGE(IDM_DIS_IOSC0, IDM_DIS_IOSC3, OnIOSC)
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Creacion de ventana
  //
  //---------------------------------------------------------------------------
int CDisasmWnd::OnCreate(LPCREATESTRUCT lpcs)
{
	int i;

 	 // Asegurar el buffer de direcciones primero
	m_pAddrBuf = new DWORD[ m_nHeight ];
	for (i=0; i<m_nHeight; i++) {
		m_pAddrBuf[i] = 0xffffffff;
	}

 	 // Clase base
	if (CSubTextScrlWnd::OnCreate(lpcs) != 0) {
		return -1;
	}

 	 // Inicializacion de direccion
	SetAddr(m_dwSetAddr);

	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Eliminacion de ventana
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnDestroy()
{
	m_bEnable = FALSE;

 	 // Liberar buffer de direcciones
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}

 	 // A la clase base
	CSubTextScrlWnd::OnDestroy();
}

  //---------------------------------------------------------------------------
  //
  //	Cambio de tamano
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnSize(UINT nType, int cx, int cy)
{
	CRect rect;
	int i;

	ASSERT(this);
	ASSERT(cx >= 0);
	ASSERT(cy >= 0);

 	 // Liberar buffer de direcciones si existe
	::LockVM();
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}
	::UnlockVM();

 	 // Clase base (dentro de esto, se puede llamar de nuevo a CDisasmWnd::OnSize)
	CSubTextScrlWnd::OnSize(nType, cx, cy);

 	 // Volver a asegurar el buffer de direcciones. Tambien comprobar liberacion
	::LockVM();
	if (m_pAddrBuf) {
		delete[] m_pAddrBuf;
		m_pAddrBuf = NULL;
	}
	m_pAddrBuf = new DWORD[ m_nHeight ];
	for (i=0; i<m_nHeight; i++) {
		m_pAddrBuf[i] = 0xffffffff;
	}
	::UnlockVM();

 	 // Reestablecer direccion
	SetAddr(m_dwSetAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Clic izquierdo
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnLButtonDown(UINT nFlags, CPoint point)
{
	OnLButtonDblClk(nFlags, point);
}

  //---------------------------------------------------------------------------
  //
  //	Doble clic con boton izquierdo
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnLButtonDblClk(UINT nFlags, CPoint point)
{
	int y;
	DWORD dwAddr;

 	 // Terminar si no hay buffer de direcciones
	if (!m_pAddrBuf) {
		return;
	}

 	 // Obtener direccion, comprobar
	y = point.y / m_tmHeight;
	dwAddr = m_pAddrBuf[y];
	if (dwAddr >= 0x01000000) {
		return;
	}

 	 // Bloqueo de VM
	::LockVM();

 	 // Si hay direccion, eliminar; si no hay, establecer
	if (m_pScheduler->IsBreak(dwAddr) >= 0) {
		m_pScheduler->DelBreak(dwAddr);
	}
	else {
		m_pScheduler->SetBreak(dwAddr);
	}
	printf("%d", nFlags);
 	 // Desbloqueo de VM
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Desplazamiento (vertical)
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnVScroll(UINT nSBCode, UINT nPos, CScrollBar *pBar)
{
	SCROLLINFO si;
	DWORD dwDiff;
	DWORD dwAddr;
	int i;

 	 // Obtener informacion de desplazamiento
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	GetScrollInfo(SB_VERT, &si, SIF_ALL);

 	 // Por codigo de barra de desplazamiento
	switch (nSBCode) {
 		 // Hacia arriba
		case SB_TOP:
			m_ScrlY = si.nMin;
			break;

 		 // Hacia abajo
		case SB_BOTTOM:
			m_ScrlY = si.nMax;
			break;

 		 // 1 linea hacia arriba
		case SB_LINEUP:
 			 // Comprobar buffer de direcciones
			if (m_pAddrBuf) {
 				 // Obtener direccion anterior y calcular diferencia
				dwDiff = GetPrevAddr(m_pAddrBuf[0]);
				dwDiff = m_pAddrBuf[0] - dwDiff;

 				 // Si hay diferencia, disminuir; si no, -1
				if (dwDiff > 0) {
					dwDiff >>= 1;
					m_ScrlY -= dwDiff;
				}
				else {
					m_ScrlY--;
				}
			}
			break;

 		 // 1 linea hacia abajo
		case SB_LINEDOWN:
 			 // Mirar buffer de direcciones y avanzar 1 instruccion
			if (m_nHeight >= 2) {
				if (m_pAddrBuf) {
					dwDiff = m_pAddrBuf[1] - m_pAddrBuf[0];
					dwDiff >>= 1;
					m_ScrlY += dwDiff;
				}
			}
			
			break;

 		 // 1 pagina hacia arriba
		case SB_PAGEUP:
 			 // Comprobar buffer de direcciones
			if (m_pAddrBuf) {
				dwAddr = m_pAddrBuf[0];
				for (i=0; i<m_nHeight-1; i++) {
 					 // Obtener direccion anterior y calcular diferencia
					dwDiff = GetPrevAddr(dwAddr);
					dwDiff = dwAddr - dwDiff;
					dwAddr -= dwDiff;

 					 // Si hay diferencia, disminuir; si no, -1
					if (dwDiff > 0) {
						dwDiff >>= 1;
						m_ScrlY -= dwDiff;
					}
					else {
						m_ScrlY--;
					}

 					 // Comprobar desbordamiento
					if (m_ScrlY < 0) {
						m_ScrlY = 0;
					}
				}

 				 // Considerar el caso en el que no se pudo volver atras en absoluto
				if (dwAddr == m_pAddrBuf[0]) {
					m_ScrlY -= si.nPage;
				}
			}
			break;

 		 // 1 pagina hacia abajo
		case SB_PAGEDOWN:
 			 // Mirar buffer de direcciones y avanzar m_nHeight instrucciones
			if (m_pAddrBuf) {
				dwDiff = m_pAddrBuf[m_nHeight - 1] - m_pAddrBuf[0];
				dwDiff >>= 1;
				m_ScrlY += dwDiff;
			}
			break;

 		 // Mover thumb
		case SB_THUMBPOSITION:
			m_ScrlY = nPos;
			break;
		case SB_THUMBTRACK:
			m_ScrlY = nPos;
			break;
	}

 	 // Comprobar desbordamiento
	if (m_ScrlY < 0) {
		m_ScrlY = 0;
	}
	if (m_ScrlY > si.nMax) {
		m_ScrlY = si.nMax;
	}

	pBar = NULL;
 	 // Set
	SetupScrlV();
}

  //---------------------------------------------------------------------------
  //
  //	Menu de contexto
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

 	 // Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextScrlWnd::OnContextMenu(pWnd, point);
		return;
	}

 	 // Cargar menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_DISMENU);
	}
	else {
		menu.LoadMenu(IDR_US_DISMENU);
	}
	pMenu = menu.GetSubMenu(0);

 	 // Configuracion
	SetupContext(pMenu);

 	 // Ejecutar menu
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu de contexto
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupContext(CMenu *pMenu)
{
	CMenu *pSubMenu;
	int i;

	ASSERT(pMenu);

 	 // Sincronizar con PC
	if (m_bSync) {
		pMenu->CheckMenuItem(IDM_DIS_SYNC, MF_BYCOMMAND | MF_CHECKED);
	}

 	 // Nueva ventana
	if (!m_pDrawView->IsNewWindow(TRUE)) {
		pMenu->EnableMenuItem(IDM_DIS_NEWWIN, MF_BYCOMMAND | MF_GRAYED);
	}

 	 // Registros de MPU, Stack, Historial de direcciones
	CCPURegWnd::SetupRegMenu(pMenu, m_pCPU, FALSE);
	CMemoryWnd::SetupStackMenu(pMenu, m_pMemory, m_pCPU);
	CAddrDlg::SetupHisMenu(pMenu);

 	 // Punto de interrupcion (Breakpoint)
	SetupBreakMenu(pMenu, m_pScheduler);

 	 // Bloquear VM
	::LockVM();

 	 // Preparar vectores de interrupcion
	pSubMenu = pMenu->GetSubMenu(9);

 	 // MPU estandar
	SetupVector(pSubMenu, 0, 1, 11);

 	 // trap #x
	SetupVector(pSubMenu, 12, 0x20, 16);

 	 // MFP
	SetupVector(pSubMenu, 29, (m_pMFP->GetVR() & 0xf0), 16);

 	 // SCC
	for (i=0; i<8; i++) {
		SetupVector(pSubMenu, 46 + i, m_pSCC->GetVector(i), 1);
	}

 	 // DMAC
	for (i=0; i<8; i++) {
		SetupVector(pSubMenu, 55 + i, m_pDMAC->GetVector(i), 1);
	}

 	 // IOSC
	SetupVector(pSubMenu, 64, m_pIOSC->GetVector(), 4);

 	 // Desbloquear VM
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de vectores de interrupcion
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupVector(CMenu *pMenu, UINT index, DWORD vector, int num)
{
	int i;
	DWORD handler;

	ASSERT(pMenu);
	ASSERT(num > 0);

 	 // Inicializacion de direcciones de vectores de interrupcion
	vector <<= 2;

 	 // Bucle
	for (i=0; i<num; i++) {
 		 // Obtener direccion del controlador de interrupcion
		handler = (DWORD)m_pMemory->ReadOnly(vector + 1);
		handler <<= 8;
		handler |= (DWORD)m_pMemory->ReadOnly(vector + 2);
		handler <<= 8;
		handler |= (DWORD)m_pMemory->ReadOnly(vector + 3);
		vector += 4;

 		 // Establecer direccion
		SetupAddress(pMenu, index, handler);
		index++;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de direccion
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupAddress(CMenu *pMenu, UINT index, DWORD addr)
{
	CString string;
	CString menustr;
	int ext;
	UINT id;

	ASSERT(pMenu);
	ASSERT(addr <= 0xffffff);

 	 // Obtener cadena actual
	pMenu->GetMenuString(index, string, MF_BYPOSITION);

 	 // Buscar el inicio de los parentesis, y si existe, solo lo posterior
	ext = string.Find(" : ");
	if (ext >= 0) {
		menustr = string.Mid(ext + 3);
	}
	else {
		menustr = string;
	}

 	 // Crear cadena ($)
	string.Format("$%06X : ", addr);
	string += menustr;

 	 // Establecer cadena
	id = pMenu->GetMenuItemID(index);
	pMenu->ModifyMenu(index, MF_BYPOSITION | MF_STRING, id, string);
}

  //---------------------------------------------------------------------------
  //
  //	Nueva ventana
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnNewWin()
{
	CDisasmWnd *pDisasmWnd;
	DWORD dwAddr;

 	 // Solicitar a la ventana padre la creacion de una nueva ventana
	pDisasmWnd = (CDisasmWnd*)m_pDrawView->NewWindow(TRUE);

 	 // Si tiene exito, pasar mi propia direccion
	if (pDisasmWnd) {
		dwAddr = m_ScrlY * 2;
		dwAddr += m_dwAddr;
		pDisasmWnd->SetAddr(dwAddr);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Mover al PC
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnPC()
{
 	 // Establecer direccion al PC actual (realiza Refresh internamente)
	SetAddr(m_pCPU->GetPC());
}

  //---------------------------------------------------------------------------
  //
  //	Sincronizar con PC
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnSync()
{
	m_bSync = (!m_bSync);
}

  //---------------------------------------------------------------------------
  //
  //	Entrada de direccion
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnAddr()
{
	CAddrDlg dlg(this);

 	 // Ejecutar dialogo
	dlg.m_dwValue = m_dwSetAddr;
	if (dlg.DoModal() != IDOK) {
		return;
	}

 	 // Establecer direccion
	SetAddr(dlg.m_dwValue);
}

  //---------------------------------------------------------------------------
  //
  //	Registros MPU
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnReg(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_REG_D0) && (nID <= IDM_REG_PC));

	dwAddr = CCPURegWnd::GetRegValue(m_pCPU, nID);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Stack
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnStack(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_STACK_0) && (nID <= IDM_STACK_F));

	dwAddr = CMemoryWnd::GetStackAddr(nID, m_pMemory, m_pCPU);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Breakpoint
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnBreak(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_DIS_BREAKP0) && (nID <= IDM_DIS_BREAKP7));

	dwAddr = GetBreak(nID, m_pScheduler);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Historial de direcciones
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnHistory(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

	dwAddr = CAddrDlg::GetAddr(nID);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Vectores de excepcion de CPU
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnCPUExcept(UINT nID)
{
	nID -= IDM_DIS_RESET;

	OnVector(nID + 1);
}

  //---------------------------------------------------------------------------
  //
  //	vectores trap
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnTrap(UINT nID)
{
	nID -= IDM_DIS_TRAP0;

	OnVector(nID + 0x20);
}

  //---------------------------------------------------------------------------
  //
  //	Vectores MFP
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnMFP(UINT nID)
{
	nID -= IDM_DIS_MFP0;

	OnVector(nID + (m_pMFP->GetVR() & 0xf0));
}

  //---------------------------------------------------------------------------
  //
  //	Vectores SCC
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnSCC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_SCC0;
	ASSERT(nID <= 7);

 	 // Obtener numero de vector
	::LockVM();
	vector = m_pSCC->GetVector(nID);
	::UnlockVM();

	OnVector(vector);
}

  //---------------------------------------------------------------------------
  //
  //	Vectores DMAC
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnDMAC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_DMAC0;
	ASSERT(nID <= 7);

 	 // Obtener numero de vector
	::LockVM();
	vector = m_pDMAC->GetVector(nID);
	::UnlockVM();

	OnVector(vector);
}

  //---------------------------------------------------------------------------
  //
  //	Vectores IOSC
  //
  //---------------------------------------------------------------------------
void CDisasmWnd::OnIOSC(UINT nID)
{
	DWORD vector;

	nID -= IDM_DIS_IOSC0;
	ASSERT(nID <= 3);

 	 // Obtener numero de vector
	::LockVM();
	vector = m_pIOSC->GetVector() + nID;
	::UnlockVM();

	OnVector(vector);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de vector
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::OnVector(UINT vector)
{
	DWORD addr;

 	 // Lectura de vector
	::LockVM();
	vector <<= 2;
	addr = (DWORD)m_pMemory->ReadOnly(vector + 1);
	addr <<= 8;
	addr |= (DWORD)m_pMemory->ReadOnly(vector + 2);
	addr <<= 8;
	addr |= (DWORD)m_pMemory->ReadOnly(vector + 3);
	::UnlockVM();

 	 // Especificacion de direccion
	SetAddr(addr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de direccion
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetAddr(DWORD dwAddr)
{
	int offset;
	CString string;

	::LockVM();

 	 // Memorizar direccion
	dwAddr &= 0xffffff;
	m_dwSetAddr = dwAddr;
	m_dwAddr = dwAddr & 0xff0000;

 	 // Extraer solo la parte baja
	offset = dwAddr & 0x00ffff;
	offset >>= 1;

 	 // Desplazamiento
	m_ScrlY = offset;
	::UnlockVM();
	SetScrollPos(SB_VERT, offset, TRUE);

 	 // Actualizar cadena de caption
	string.Format(" [%d] ($%06X - $%06X)", (m_dwID & 0xff) - 'A' + 1, m_dwAddr, m_dwAddr + 0xffff);
	string = m_strCaption + string;
	if (m_Caption != string) {
		m_Caption = string;
		SetWindowText(string);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de PC
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetPC(DWORD pc)
{
	ASSERT(pc <= 0xffffff);

 	 // Si el flag de sincronizacion esta activo, establecer direccion
	if (m_bSync) {
		m_dwPC = pc;
	}
	else {
		m_dwPC = 0xffffffff;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Actualizacion desde el hilo de mensajes
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::Update()
{
 	 // Especificacion de PC
	if (m_dwPC < 0x1000000) {
		SetAddr(m_dwPC);
		m_dwPC = 0xffffffff;
	}
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::Setup()
{
	DWORD dwAddr;
	DWORD dwPC;
	int i;
	int j;
	int k;

 	 // Especificacion de direccion
	dwAddr = (m_dwAddr & 0xff0000);
	dwAddr |= (DWORD)(m_ScrlY * 2);
	::debugpc = dwAddr;

 	 // Obtener PC
	dwPC = m_pCPU->GetPC();

 	 // Bucle
	for (i=0; i<m_nHeight; i++) {
		dwAddr = ::debugpc;

 		 // Almacenar direccion
		if (m_pAddrBuf) {
			m_pAddrBuf[i] = dwAddr;
		}

 		 // Comprobar bucle de direccion (considerar bucle FFFFFFF)
		if (dwAddr > 0xffffff) {
 			 // Hubo bucle. Mantener direccion
			ASSERT(i > 0);
			if (m_pAddrBuf) {
				m_pAddrBuf[i] = m_pAddrBuf[i - 1];
			}

 			 // Eliminar
			Reverse(FALSE);
			for (j=0; j<m_nWidth; j++) {
				SetChr(j, i, ' ');
			}
			continue;
		}

 		 // Determinacion de atributos
		k = m_pScheduler->IsBreak(dwAddr);
		if (k >= 0) {
			Reverse(TRUE);
		}
		else {
			Reverse(FALSE);
		}
 		 // Relleno
		for (j=0; j<m_nWidth; j++) {
			SetChr(j, i, ' ');
		}

 		 // Desensamblar
		::cpudebug_disassemble(1);

 		 // Marca de PC, marca de break
		if (k >= 0) {
			::debugbuf[0] = (char)(k + '1');
		}
		else {
			::debugbuf[0] = ' ';
		}
		if (dwAddr == dwPC) {
			::debugbuf[1] = '>';
		}
		else {
			::debugbuf[1] = ' ';
		}

 		 // Mostrar
		if (m_ScrlX < (int)strlen(::debugbuf)) {
			SetString(0, i, &debugbuf[m_ScrlX]);
		}

 		 // Soportar caso en el que no es valido por breakpoint
		k = m_pScheduler->IsBreak(dwAddr, TRUE);
		if (k >= 0) {
			Reverse(TRUE);
			SetChr(0, i, (char)(k + '1'));
		}
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtener direccion anterior
  //
  //---------------------------------------------------------------------------
DWORD FASTCALL CDisasmWnd::GetPrevAddr(DWORD dwAddr)
{
	int i;
	DWORD dwTest;

	ASSERT(dwAddr <= 0xffffff);

 	 // Inicializacion de direccion
	dwTest = dwAddr;

	for (i=0; i<16; i++) {
 		 // Disminuir dwTest, comprobar desbordamiento
		dwTest -= 2;
		if (dwTest >= 0x01000000) {
			return dwAddr;
		}

 		 // Desensamblar desde ahi y ver el incremento de direccion
		::debugpc = dwTest;
		::cpudebug_disassemble(1);

 		 // Si coincide, comprobar "UNRECOG"
		if (::debugpc == dwAddr) {
			if ((::debugbuf[35] == 'U') || (::debugbuf[36] == 'N') || (::debugbuf[37] == 'R')) {
				continue;
			}
 			 // ok, retorno
			return dwTest;
		}
	}

 	 // No coincide. Devolver dwAddr
	return dwAddr;
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu de puntos de interrupcion (breakpoints)
  //
  //---------------------------------------------------------------------------
void FASTCALL CDisasmWnd::SetupBreakMenu(CMenu *pMenu, Scheduler *pScheduler)
{
	int num;
	int i;
	Scheduler::breakpoint_t buf;
	CString string;

	ASSERT(pMenu);
	ASSERT(pScheduler);

 	 // Limpiar cantidad
	num = 0;

 	 // Configuracion
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		pScheduler->GetBreak(i, &buf);
		if (buf.use) {
 			 // Esta en uso, asi que configurar
			string.Format("%1d : $%06X", num + 1, buf.addr);
			pMenu->ModifyMenu(IDM_DIS_BREAKP0 + num, MF_BYCOMMAND | MF_STRING,
				IDM_DIS_BREAKP0 + num, (LPCTSTR)string);

 			 // +1
			num++;
		}
	}
	::UnlockVM();

 	 // Limpiar el resto
	for (i=num; i<Scheduler::BreakMax; i++) {
		pMenu->DeleteMenu(IDM_DIS_BREAKP0 + i, MF_BYCOMMAND);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Obtener menu de puntos de interrupcion (breakpoints)
  //
  //---------------------------------------------------------------------------
DWORD FASTCALL CDisasmWnd::GetBreak(UINT nID, Scheduler *pScheduler)
{
	int i;
	Scheduler::breakpoint_t buf;

	ASSERT((nID >= IDM_DIS_BREAKP0) && (nID <= IDM_DIS_BREAKP7));
	ASSERT(pScheduler);
	nID -= IDM_DIS_BREAKP0;

 	 // Bucle de busqueda
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		pScheduler->GetBreak(i, &buf);
		if (buf.use) {
 			 // Esta en uso, ?es este?
			if (nID == 0) {
				::UnlockVM();
				return buf.addr;
			}
			nID--;
		}
	}
	::UnlockVM();

	return 0;
}

  //===========================================================================
  //
  //	Ventana de memoria
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CMemoryWnd::CMemoryWnd(int nWnd)
{
 	 // Hasta 8 tipos de ventanas de memoria
	ASSERT((nWnd >= 0) && (nWnd <= 7));

 	 // Definicion de parametros de ventana
	m_dwID = MAKEID('M', 'E', 'M', (nWnd + 'A'));
	::GetMsg(IDS_SWND_MEMORY, m_strCaption);
	m_nWidth = 73;
	m_nHeight = 16;

 	 // Definicion de parametros de ventana (scroll)
	m_ScrlWidth = 73;
	m_ScrlHeight = 0x8000;

 	 // Obtener CPU
	m_pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
	ASSERT(m_pCPU);

 	 // Obtener memoria
	m_pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(m_pMemory);

 	 // Otros
	m_dwAddr = 0;
	m_nUnit = 0;
	m_strCaptionReq.Empty();
	m_strCaptionSet.Empty();
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CMemoryWnd, CSubTextScrlWnd)
	ON_WM_CREATE()
	ON_WM_PAINT()
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_MEMORY_ADDR, OnAddr)
	ON_COMMAND(IDM_MEMORY_NEWWIN, OnNewWin)
	ON_COMMAND_RANGE(IDM_MEMORY_BYTE, IDM_MEMORY_LONG, OnUnit)
	ON_COMMAND_RANGE(IDM_MEMORY_0, IDM_MEMORY_F, OnRange)
	ON_COMMAND_RANGE(IDM_REG_D0, IDM_REG_PC, OnReg)
	ON_COMMAND_RANGE(IDM_AREA_MPU, IDM_AREA_IPLROM, OnArea)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND_RANGE(IDM_STACK_0, IDM_STACK_F, OnStack)
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Creacion de ventana
  //
  //---------------------------------------------------------------------------
int CMemoryWnd::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
 	 // Clase base
	if (CSubTextScrlWnd::OnCreate(lpCreateStruct) != 0) {
		return -1;
	}

 	 // Inicializacion de direccion
	SetAddr(0);
	return TRUE;
}

  //---------------------------------------------------------------------------
  //
  //	Dibujo
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnPaint()
{
 	 // Clase base
	CSubTextScrlWnd::OnPaint();

 	 // Configuracion de caption
	SetWindowText(m_strCaption);
}

  //---------------------------------------------------------------------------
  //
  //	Doble clic con boton izquierdo
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int x;
	int y;
	DWORD dwAddr;
	DWORD dwData;
	CDataDlg dlg(this);

 	 // Calcular x, y
	x = point.x / m_tmWidth;
	y = point.y / m_tmHeight;

 	 // Comprobar x
	if (x < 8) {
		return;
	}
	x -= 8;

 	 // Obtener direccion de y
	dwAddr = m_dwAddr | (m_ScrlY << 5);
	dwAddr += (y << 4);
	if ((dwAddr - m_dwAddr) >= 0x100000) {
		return;
	}

 	 // Obtener direccion de x
	switch (m_nUnit) {
 		 // Byte
		case 0:
			x /= 3;
			break;
 		 // Word
		case 1:
			x /= 5;
			x <<= 1;
			break;
 		 // Long
		case 2:
			x /= 9;
			x <<= 2;
			break;
 		 // Otros
		default:
			ASSERT(FALSE);
			break;
	}
	if (x >= 16) {
		return;
	}
	dwAddr += x;

 	 // Lectura de datos
	::LockVM();
	switch (m_nUnit) {
 		 // Byte
		case 0:
			dwData = m_pMemory->ReadOnly(dwAddr);
			break;
 		 // Word
		case 1:
			dwData = m_pMemory->ReadOnly(dwAddr);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 1);
			break;
 		 // Long
		case 2:
			dwData = m_pMemory->ReadOnly(dwAddr);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 1);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 2);
			dwData <<= 8;
			dwData |= m_pMemory->ReadOnly(dwAddr + 3);
			break;
 		 // Otros
		default:
			dwData = 0;
			ASSERT(FALSE);
			break;
	}
	::UnlockVM();

 	 // Ejecutar dialogo
	dlg.m_dwAddr = dwAddr;
	dlg.m_dwValue = dwData;
	dlg.m_nSize = m_nUnit;
	if (dlg.DoModal() != IDOK) {
		return;
	}

 	 // Escritura
	dwData = dlg.m_dwValue;
	::LockVM();
	switch (m_nUnit) {
 		 // Byte
		case 0:
			m_pMemory->WriteByte(dwAddr, dwData);
			break;
 		 // Word
		case 1:
			m_pMemory->WriteWord(dwAddr, dwData);
			break;
 		 // Long
		case 2:
			m_pMemory->WriteWord(dwAddr, (WORD)(dwData >> 16));
			m_pMemory->WriteWord(dwAddr + 2, (WORD)dwData);
			break;
 		 // Otros
		default:
			ASSERT(FALSE);
			break;
	}
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Menu de contexto
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

 	 // Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextScrlWnd::OnContextMenu(pWnd, point);
		return;
	}

 	 // Ejecutar menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_MEMORYMENU);
	}
	else {
		menu.LoadMenu(IDR_US_MEMORYMENU);
	}
	pMenu = menu.GetSubMenu(0);
	SetupContext(pMenu);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu de contexto
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetupContext(CMenu *pMenu)
{
	ASSERT(pMenu);

 	 // Nueva ventana
	if (!m_pDrawView->IsNewWindow(FALSE)) {
		pMenu->EnableMenuItem(IDM_MEMORY_NEWWIN, MF_BYCOMMAND | MF_GRAYED);
	}

 	 // Comprobar tamano
	pMenu->CheckMenuRadioItem(IDM_MEMORY_BYTE, IDM_MEMORY_LONG,
			IDM_MEMORY_BYTE + m_nUnit, MF_BYCOMMAND);

 	 // Comprobar direccion
	pMenu->CheckMenuRadioItem(IDM_MEMORY_0, IDM_MEMORY_F,
			IDM_MEMORY_0 + (m_dwAddr >> 20), MF_BYCOMMAND);

 	 // Registros MPU
	CCPURegWnd::SetupRegMenu(pMenu, m_pCPU, FALSE);

 	 // Historial de direcciones
	CAddrDlg::SetupHisMenu(pMenu);

 	 // Stack
	SetupStackMenu(pMenu, m_pMemory, m_pCPU);
}

  //---------------------------------------------------------------------------
  //
  //	Entrada de direccion
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnAddr()
{
	CAddrDlg dlg(this);

 	 // Ejecutar dialogo
	dlg.m_dwValue = m_dwAddr | (m_ScrlY * 0x20);
	if (dlg.DoModal() != IDOK) {
		return;
	}

 	 // Establecer direccion
	SetAddr(dlg.m_dwValue);
}

  //---------------------------------------------------------------------------
  //
  //	Nueva ventana
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnNewWin()
{
	CMemoryWnd *pWnd;

 	 // Solicitar a la ventana padre la creacion de una nueva ventana
	pWnd = (CMemoryWnd*)m_pDrawView->NewWindow(FALSE);

 	 // Si se crea, especificar mi misma direccion y tamano
	if (pWnd) {
		pWnd->SetAddr(m_dwAddr | (m_ScrlY * 0x20));
		pWnd->SetUnit(m_nUnit);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de unidad de visualizacion
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnUnit(UINT uID)
{
	int unit;

	unit = (int)(uID - IDM_MEMORY_BYTE);
	ASSERT((unit >= 0) && (unit <= 2));

	SetUnit(unit);
}

  //---------------------------------------------------------------------------
  //
  //	Set de unidad de visualizacion
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetUnit(int nUnit)
{
	ASSERT(this);
	ASSERT((nUnit >= 0) && (nUnit <= 2));

 	 // Bloqueo, cambio
	::LockVM();
	m_nUnit = nUnit;
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de rango de direcciones
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnRange(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_MEMORY_0) && (uID <= IDM_MEMORY_F));

	dwAddr = (DWORD)(uID - IDM_MEMORY_0);
	dwAddr *= 0x100000;
	dwAddr |= (DWORD)(m_ScrlY * 0x20);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de registro
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnReg(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_REG_D0) && (uID <= IDM_REG_PC));

	dwAddr = CCPURegWnd::GetRegValue(m_pCPU, uID);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de area
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnArea(UINT uID)
{
	CMenu menu;
	TCHAR buf[0x100];
	DWORD dwAddr;

	ASSERT((uID >= IDM_AREA_MPU) && (uID <= IDM_AREA_IPLROM));

 	 // Cargar menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_MEMORYMENU);
	}
	else {
		menu.LoadMenu(IDR_US_MEMORYMENU);
	}

 	 // Obtener cadena de menu del ID especificado
	menu.GetMenuString(uID, buf, 0x100, MF_BYCOMMAND);

 	 // Asumir el formato "$000000 : "
	buf[0] = _T('0');
	buf[7] = _T('\0');
	dwAddr = ::_tcstoul(buf, NULL, 16);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de historial
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnHistory(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_HISTORY_0) && (uID <= IDM_HISTORY_9));
	dwAddr = CAddrDlg::GetAddr(uID);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de stack
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::OnStack(UINT uID)
{
	DWORD dwAddr;

	ASSERT((uID >= IDM_STACK_0) && (uID <= IDM_STACK_F));
	dwAddr = GetStackAddr(uID, m_pMemory, m_pCPU);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de direccion
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetAddr(DWORD dwAddr)
{
	int offset;
	CString strCap;

	ASSERT(this);
	ASSERT(dwAddr <= 0x1000000);

 	 // Extraer solo parte superior
	m_dwAddr = dwAddr & 0xf00000;

 	 // Extraer solo parte inferior
	offset = dwAddr & 0x0fffff;
	offset /= 0x20;

 	 // Desplazamiento
	m_ScrlY = offset;
	SetScrollPos(SB_VERT, offset, TRUE);

 	 // Crear cadena de caption
	strCap.Format(_T(" [%d] ($%06X - $%06X)"), (m_dwID & 0xff) - 'A' + 1,
									m_dwAddr, m_dwAddr + 0x0fffff);
	m_CSection.Lock();
	m_strCaptionReq = m_strCaption + strCap;
	m_CSection.Unlock();
}

  //---------------------------------------------------------------------------
  //
  //	Actualizacion desde el hilo de mensajes
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::Update()
{
	CString strCap;

 	 // Obtener cadena de caption
	m_CSection.Lock();
	strCap = m_strCaptionReq;
	m_CSection.Unlock();

 	 // Comparacion
	if (m_strCaptionSet != strCap) {
		m_strCaptionSet = strCap;
		SetWindowText(m_strCaptionSet);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::Setup()
{
	int x;
	int y;
	CString strText;
	CString strHex;
	DWORD dwAddr;
	DWORD dwOffset;
	TCHAR szAscii[2];

 	 // Limpiar, inicializar direccion
	Clear();
	dwAddr = (m_dwAddr & 0xf00000);
	dwOffset = (DWORD)(m_ScrlY << 5);
	dwAddr |= dwOffset;

 	 // Inicializar cadena
	szAscii[1] = _T('\0');

 	 // Bucle y
	for (y=0; y<m_nHeight; y++) {
 		 // Comprobar desbordamiento
		if (dwOffset >= 0x100000) {
			break;
		}

 		 // Mostrar direccion
		strText.Format(_T("%06X:"), dwAddr);

 		 // Bucle x
		switch (m_nUnit) {
 			 // Byte
			case 0:
				for (x=0; x<16; x++) {
					strHex.Format(_T(" %02X"), m_pMemory->ReadOnly(dwAddr));
					strText += strHex;
					dwAddr++;
				}
				break;
 			 // Word
			case 1:
				for (x=0; x<8; x++) {
					strHex.Format(_T(" %02X%02X"),  m_pMemory->ReadOnly(dwAddr),
													m_pMemory->ReadOnly(dwAddr + 1));
					strText += strHex;
					dwAddr += 2;
				}
				break;
 			 // Long
			case 2:
				for (x=0; x<4; x++) {
					strHex.Format(" %02X%02X%02X%02X",  m_pMemory->ReadOnly(dwAddr),
														m_pMemory->ReadOnly(dwAddr + 1),
														m_pMemory->ReadOnly(dwAddr + 2),
														m_pMemory->ReadOnly(dwAddr + 3));
					strText += strHex;
					dwAddr += 4;
				}
				break;
 			 // Otros (imposible)
			default:
				ASSERT(FALSE);
				break;
		}

 		 // Volver una vez
		dwAddr -= 0x10;
		dwAddr &= 0xffffff;

 		 // Anadir caracter ASCII
		strText += _T("  ");
		for (x=0; x<16; x++) {
			szAscii[0] = (TCHAR)m_pMemory->ReadOnly(dwAddr + x);
			if ((szAscii[0] >= 0) && (szAscii[0] < 0x20)) {
				szAscii[0] = TCHAR('.');
			}
			if ((szAscii[0] < 0) || (szAscii[0] >= 0x7f)) {
				szAscii[0] = TCHAR('.');
			}
			strText += szAscii;
		}
		dwAddr += 0x10;
		dwAddr &= 0xffffff;

 		 // Avanzar offset
		dwOffset += 0x10;

 		 // Mostrar
		if (m_ScrlX < strText.GetLength()) {
			SetString(0, y, (LPCTSTR)(strText) + m_ScrlX * sizeof(TCHAR));
		}
	}
}

  //---------------------------------------------------------------------------
  //
  //	Preparacion de desplazamiento (vertical)
  //
  //---------------------------------------------------------------------------
void FASTCALL CMemoryWnd::SetupScrlV()
{
	SCROLLINFO si;
	CRect rect;
	int height;

 	 // Obtener numero de caracteres de visualizacion vertical
	GetClientRect(&rect);
	height = rect.bottom / m_tmHeight;

 	 // Correccion (debido a 2 lineas por unidad de scroll)
	height >>= 1;

 	 // Establecer informacion de desplazamiento
	memset(&si, 0, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask = SIF_ALL;
	si.nMin = 0;
	si.nMax = m_ScrlHeight - 1;
	si.nPage = height;

 	 // Corregir posicion si es necesario
	si.nPos = m_ScrlY;
	if (si.nPos + height > m_ScrlHeight) {
		si.nPos = m_ScrlHeight - height;
		if (si.nPos < 0) {
			si.nPos = 0;
		}
		m_ScrlY = si.nPos;
	}

	SetScrollInfo(SB_VERT, &si, TRUE);
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu de stack
  //
  //---------------------------------------------------------------------------
void CMemoryWnd::SetupStackMenu(CMenu *pMenu, Memory *pMemory, CPU *pCPU)
{
	int i;
	CPU::cpu_t reg;
	DWORD dwAddr;
	DWORD dwValue;
	CString strMenu;

	ASSERT(pMenu);
	ASSERT(pMemory);
	ASSERT(pCPU);

 	 // Bloqueo de VM, obtener registros
	::LockVM();
	pCPU->GetCPU(&reg);

 	 // 16 niveles
	for (i=0; i<16; i++) {
 		 // Calcular direccion
		dwAddr = reg.areg[7];
		dwAddr += (i << 1);
		dwAddr &= 0xfffffe;

 		 // Obtener datos
		dwValue = pMemory->ReadOnly(dwAddr + 1);
		dwAddr = (dwAddr + 2) & 0xfffffe;
		dwValue <<= 8;
		dwValue |= pMemory->ReadOnly(dwAddr);
		dwValue <<= 8;
		dwValue |= pMemory->ReadOnly(dwAddr + 1);

 		 // Actualizar menu
		strMenu.Format(_T("(A7+%1X) : $%06X"), (i << 1), dwValue);
		pMenu->ModifyMenu(IDM_STACK_0 + i, MF_BYCOMMAND | MF_STRING,
							IDM_STACK_0 + i, (LPCTSTR)strMenu);
	}

 	 // Desbloqueo de VM
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Obtener direccion de stack
  //
  //---------------------------------------------------------------------------
DWORD CMemoryWnd::GetStackAddr(UINT nID, Memory *pMemory, CPU *pCPU)
{
	CPU::cpu_t reg;
	DWORD dwAddr;
	DWORD dwValue;

	ASSERT((nID >= IDM_STACK_0) && (nID <= IDM_STACK_F));
	ASSERT(pMemory);
	ASSERT(pCPU);

 	 // Calcular offset
	nID -= IDM_STACK_0;
	ASSERT(nID <= 15);
	nID <<= 1;

 	 // Calcular direccion y memoria desde registros de CPU
	::LockVM();
	pCPU->GetCPU(&reg);
	dwAddr = reg.areg[7];
	dwAddr += nID;
	dwAddr &= 0xfffffe;

 	 // Obtener datos
	dwValue = pMemory->ReadOnly(dwAddr + 1);
	dwAddr = (dwAddr + 2) & 0xfffffe;
	dwValue <<= 8;
	dwValue |= pMemory->ReadOnly(dwAddr);
	dwValue <<= 8;
	dwValue |= pMemory->ReadOnly(dwAddr + 1);
	::UnlockVM();

	return dwValue;
}

  //===========================================================================
  //
  //	Ventana de puntos de interrupcion (breakpoints)
  //
  //===========================================================================

  //---------------------------------------------------------------------------
  //
  //	Constructor
  //
  //---------------------------------------------------------------------------
CBreakPWnd::CBreakPWnd()
{
 	 // Definicion de parametros de ventana
	m_dwID = MAKEID('B', 'R', 'K', 'P');
	::GetMsg(IDS_SWND_BREAKP, m_strCaption);
	m_nWidth = 43;
	m_nHeight = Scheduler::BreakMax + 1;

 	 // Obtener planificador
	m_pScheduler = (Scheduler*)::GetVM()->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(m_pScheduler);
}

  //---------------------------------------------------------------------------
  //
  //	Mapa de mensajes
  //
  //---------------------------------------------------------------------------
BEGIN_MESSAGE_MAP(CBreakPWnd, CSubTextWnd)
	ON_WM_LBUTTONDBLCLK()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(IDM_BREAKP_ENABLE, OnEnable)
	ON_COMMAND(IDM_BREAKP_CLEAR, OnClear)
	ON_COMMAND(IDM_BREAKP_DEL, OnDel)
	ON_COMMAND(IDM_BREAKP_ADDR, OnAddr)
	ON_COMMAND_RANGE(IDM_HISTORY_0, IDM_HISTORY_9, OnHistory)
	ON_COMMAND(IDM_BREAKP_ALL, OnAll)
END_MESSAGE_MAP()

  //---------------------------------------------------------------------------
  //
  //	Doble clic con boton izquierdo
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnLButtonDblClk(UINT /*nFlags*/, CPoint point)
{
	int y;
	Scheduler::breakpoint_t buf;

 	 // Obtener y, correccion (-1), comprobar
	y = point.y / m_tmHeight;
	y--;
	if ((y < 0) || (y >= Scheduler::BreakMax)) {
		return;
	}

 	 // Bloqueo, obtener breakpoint
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);

 	 // Invertir si esta en uso
	if (buf.use) {
		m_pScheduler->EnableBreak(y, !(buf.enable));
	}

 	 // Desbloqueo
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Menu de contexto
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnContextMenu(CWnd *pWnd, CPoint point)
{
	CRect rect;
	CMenu menu;
	CMenu *pMenu;

 	 // Determinar si se presiono dentro del area del cliente
	GetClientRect(&rect);
	ClientToScreen(&rect);
	if (!rect.PtInRect(point)) {
		CSubTextWnd::OnContextMenu(pWnd, point);
		return;
	}

 	 // Memorizar posicion
	m_Point = point;

 	 // Ejecutar menu
	if (::IsJapanese()) {
		menu.LoadMenu(IDR_BREAKPMENU);
	}
	else {
		menu.LoadMenu(IDR_US_BREAKPMENU);
	}
	pMenu = menu.GetSubMenu(0);
	SetupContext(pMenu);
	pMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
							point.x, point.y, this);
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion de menu de contexto
  //
  //---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::SetupContext(CMenu *pMenu)
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;
	int nCount;
	int nBreak;

	ASSERT(pMenu);

 	 // Inicializacion
	buf.enable = FALSE;
	buf.use = FALSE;

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

 	 // Obtener cantidad de breakpoints en uso y obtener el actual
	nCount = 0;
	::LockVM();
	for (nBreak=0; nBreak<Scheduler::BreakMax; nBreak++) {
		m_pScheduler->GetBreak(nBreak, &buf);
		if (buf.use) {
			nCount++;
		}
	}
	buf.use = FALSE;
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
	}
	::UnlockVM();

 	 // Eliminar todos
	if (nCount > 0) {
		pMenu->EnableMenuItem(IDM_BREAKP_ALL, MF_BYCOMMAND | MF_ENABLED);
	}
	else {
		pMenu->EnableMenuItem(IDM_BREAKP_ALL, MF_BYCOMMAND | MF_GRAYED);
	}

 	 // Historial de direcciones
	CAddrDlg::SetupHisMenu(pMenu);

 	 // Si el actual no esta en uso, deshabilitar los relacionados con cambios
	if (!buf.use) {
		pMenu->EnableMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_GRAYED);
		pMenu->EnableMenuItem(IDM_BREAKP_CLEAR, MF_BYCOMMAND | MF_GRAYED);
		pMenu->EnableMenuItem(IDM_BREAKP_DEL, MF_BYCOMMAND | MF_GRAYED);
		return;
	}

 	 // Si esta en uso, comprobar Enable
	if (buf.enable) {
		pMenu->CheckMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_CHECKED);
	}
	else {
		pMenu->CheckMenuItem(IDM_BREAKP_ENABLE, MF_BYCOMMAND | MF_UNCHECKED);
	}
}

  //---------------------------------------------------------------------------
  //
  //	Activar/Desactivar
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnEnable()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

 	 // Invertir breakpoint
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);
	ASSERT(buf.use);
	m_pScheduler->EnableBreak(y, !(buf.enable));
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Limpiar conteo
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnClear()
{
	int y;
	CPoint point;

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

 	 // Limpiar conteo
	::LockVM();
	m_pScheduler->ClearBreak(y);
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Eliminar punto de interrupcion (breakpoint)
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnDel()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;
	ASSERT((y >= 0) && (y < Scheduler::BreakMax));

 	 // Limpiar conteo
	::LockVM();
	m_pScheduler->GetBreak(y, &buf);
	ASSERT(buf.use);
	m_pScheduler->DelBreak(buf.addr);
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Especificacion de direccion
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnAddr()
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;
	CPU *pCPU;
	CPU::cpu_t reg;
	DWORD dwAddr;
	CAddrDlg dlg(this);

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

	::LockVM();
 	 // Si apunta a un breakpoint valido, esa direccion
	dwAddr = 0xffffffff;
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			dwAddr = buf.addr & 0xffffff;
		}
	}
 	 // Si no, el PC
	if (dwAddr == 0xffffffff) {
		pCPU = (CPU*)::GetVM()->SearchDevice(MAKEID('C', 'P', 'U', ' '));
		ASSERT(pCPU);
		pCPU->GetCPU(&reg);
		dwAddr = reg.pc & 0xffffff;
	}
	::UnlockVM();
	ASSERT(dwAddr <= 0xffffff);

 	 // Dialogo de entrada
	dlg.m_dwValue = dwAddr;
	if (dlg.DoModal() != IDOK) {
		return;
	}

 	 // Dejar a cargo de la rutina comun
	SetAddr(dlg.m_dwValue);
}

  //---------------------------------------------------------------------------
  //
  //	Historial de direcciones
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnHistory(UINT nID)
{
	DWORD dwAddr;

	ASSERT((nID >= IDM_HISTORY_0) && (nID <= IDM_HISTORY_9));

 	 // Dejar a cargo de la rutina comun
	dwAddr = CAddrDlg::GetAddr(nID);
	SetAddr(dwAddr);
}

  //---------------------------------------------------------------------------
  //
  //	Eliminar todos
  //
  //---------------------------------------------------------------------------
void CBreakPWnd::OnAll()
{
	Scheduler::breakpoint_t buf;
	int i;

 	 // Limpiar todos
	::LockVM();
	for (i=0; i<Scheduler::BreakMax; i++) {
		m_pScheduler->GetBreak(i, &buf);
		if (buf.use) {
			m_pScheduler->DelBreak(buf.addr);
		}
	}
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Establecer direccion
  //
  //---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::SetAddr(DWORD dwAddr)
{
	int y;
	CPoint point;
	Scheduler::breakpoint_t buf;

	ASSERT(dwAddr <= 0xffffff);

 	 // Si es una direccion ya registrada, invalido
	::LockVM();
	for (y=0; y<Scheduler::BreakMax; y++) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			if (buf.addr == dwAddr) {
				::UnlockVM();
				return;
			}
		}
	}
	::UnlockVM();

 	 // Obtener y
	point = m_Point;
	ScreenToClient(&point);
	y = point.y / m_tmHeight;
	y--;

 	 // Si apunta a un breakpoint en uso, reemplazarlo
	::LockVM();
	if ((y >= 0) && (y < Scheduler::BreakMax)) {
		m_pScheduler->GetBreak(y, &buf);
		if (buf.use) {
			m_pScheduler->AddrBreak(y, dwAddr);
			::UnlockVM();
			return;
		}
	}

 	 // Si no, configurar como nuevo
	m_pScheduler->SetBreak(dwAddr);
	::UnlockVM();
}

  //---------------------------------------------------------------------------
  //
  //	Configuracion
  //
  //---------------------------------------------------------------------------
void FASTCALL CBreakPWnd::Setup()
{
	int i;
	CString strText;
	CString strFmt;
	Scheduler::breakpoint_t buf;

 	 // Limpiar
	Clear();

 	 // Mostrar guia
	SetString(0, 0, _T("No."));
	SetString(5, 0, _T("Address"));
	SetString(14, 0, _T("Flag"));
	SetString(28, 0, _T("Time"));
	SetString(38, 0, _T("Count"));

 	 // Bucle
	for (i=0; i<Scheduler::BreakMax; i++) {
 		 // Numero
		strText.Format(_T("%2d "), i + 1);

 		 // Obtener, comprobar validez
		m_pScheduler->GetBreak(i, &buf);
		if (buf.use) {
 			 // Direccion
			strFmt.Format(_T("  $%06X "), buf.addr);
			strText += strFmt;

 			 // Flag
			if (buf.enable) {
				strText += _T(" Enable");
			}
			else {
				strText += _T("Disable");
			}

 			 // Tiempo
			if (buf.count > 0) {
				strFmt.Format(_T(" %7d.%05dms"), (buf.time / 2000), (buf.time % 2000) * 5);
				strText += strFmt;

 				 // Conteo
				strFmt.Format(_T("   %4d"), buf.count);
				strText += strFmt;
			}
		}

 		 // Set de cadena
		SetString(0, i + 1, strText);
	}
}

 #endif	 // _WIN32
