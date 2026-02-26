//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
//	[ MFC Procesamiento de comandos ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "fdd.h"
#include "fdi.h"
#include "rtc.h"
#include "keyboard.h"
#include "sasi.h"
#include "sram.h"
#include "memory.h"
#include "render.h"
#include "fileio.h"
#include "mfc_frm.h"
#include "mfc_res.h"
#include "mfc_draw.h"
#include "mfc_cpu.h"
#include "mfc_sys.h"
#include "mfc_dev.h"
#include "mfc_w32.h"
#include "mfc_ver.h"
#include "mfc_com.h"
#include "mfc_sch.h"
#include "mfc_snd.h"
#include "mfc_inp.h"
#include "mfc_cfg.h"
#include "mfc_vid.h"
#include "mfc_rend.h"
#include "mfc_stat.h"
#include "mfc_tool.h"

//---------------------------------------------------------------------------
//
//	Abrir
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];

	// Ejecutar dialogo comun
	::GetVM()->GetPath(path);
	_tcscpy(szPath, path.GetPath());
	if (!::FileOpenDlg(this, szPath, IDS_XM6OPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);
			
	// Pre-procesamiento de apertura
	if (!OnOpenPrep(path)) {
		return;
	}

	// Sub-apertura
	OnOpenSub(path);
}


// Apertura r·pida de estados
void CFrmWnd::OnFastOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];		
	TCHAR rutaCargaRapida[_MAX_PATH];		 
	 sprintf(rutaCargaRapida, "%s\\%s.xm6", RutaSaveStates, NombreArchivoXM6);


	/* int msgboxID = MessageBox(
		 rutaCargaRapida,"ruta",
        2 );	*/
	 
	_tcscpy(szPath, rutaCargaRapida);
	path.SetPath(szPath);
			

	// Pre-procesamiento de apertura
	if (!OnOpenPrep(path)) {
		return;
	}

	// Sub-apertura
	OnOpenSub(path);
}

//---------------------------------------------------------------------------
//
//	Abrir UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOpenUI(CCmdUI *pCmdUI)
{
	BOOL bPower;
	BOOL bSW;
	Filepath path;
	CMenu *pMenu;
	CMenu *pSubMenu;
	CString strExit;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	int nEnable;
	int i;

	// Obtener estado de energia y ruta de archivo (bajo bloqueo de VM)
	::LockVM();
	bPower = ::GetVM()->IsPower();
	bSW = ::GetVM()->IsPowerSW();
	::GetVM()->GetPath(path);
	::UnlockVM();

	// Abrir
	pCmdUI->Enable(bPower);

	// Obtener submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	// El menu de archivos es el primero
	pSubMenu = pMenu->GetSubMenu(0);
	ASSERT(pSubMenu);

	// UI de sobrescribir (medida para el timing de ON_UPDATE_COMMAND_UI a continuacion)
	if (bPower && (_tcslen(path.GetPath()) > 0)) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// UI de guardar como
	if (bPower) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// UI de reinicio
	if (bPower) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// UI de interrupcion
	if (bPower) {
		pSubMenu->EnableMenuItem(6, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(6, MF_BYPOSITION | MF_GRAYED);
	}

	// UI de interruptor de encendido
	if (bSW) {
		pSubMenu->EnableMenuItem(7, MF_BYPOSITION | MF_CHECKED);
	}
	else {
		pSubMenu->EnableMenuItem(7, MF_BYPOSITION | MF_UNCHECKED);
	}

	// Abrir separador y eliminar todos los menus posteriores
	while (pSubMenu->GetMenuItemCount() > 9) {
		pSubMenu->RemoveMenu(9, MF_BYPOSITION);
	}

	// Si no hay MRU, anadir menu de salida y terminar
	if (GetConfig()->GetMRUNum(4) == 0) {
		::GetMsg(IDS_EXIT, strExit);
		pSubMenu->AppendMenu(MF_STRING, IDM_EXIT, strExit);
		return;
	}

	// Configuracion de constantes de activado/desactivado
	if (bPower) {
		nEnable = MF_BYCOMMAND | MF_GRAYED;
	}
	else {
		nEnable = MF_BYCOMMAND | MF_ENABLED;
	}

	// Procesamiento MRU - Anadir
	for (i=0; i<9; i++) {
		// Intentar obtener
		GetConfig()->GetMRUFile(4, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Ç†ÇÍÇŒMenuÇ…í«â¡
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_XM6_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_XM6_MRU0 + i, nEnable);
	}

	// Anadir separador
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// Anadir menu de salida
	::GetMsg(IDS_EXIT, strExit);
	pSubMenu->AppendMenu(MF_STRING, IDM_EXIT, strExit);
}

//---------------------------------------------------------------------------
//
//	Verificacion previa a la apertura
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::OnOpenPrep(const Filepath& path, BOOL bWarning)
{
	Fileio fio;
	CString strMsg;
	CString strFmt;
	char cHeader[0x10];
	int nRecVer;
	int nNowVer;
	DWORD dwMajor;
	DWORD dwMinor;

	ASSERT(this);

	// Verificacion de existencia de archivo
	if (!fio.Open(path, Fileio::ReadOnly)) {
		if (bWarning) {
			::GetMsg(IDS_XM6LOADFILE, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
		return FALSE;
	}

	// Lectura de cabecera
	memset(cHeader, 0, sizeof(cHeader));
	fio.Read(cHeader, sizeof(cHeader));
	fio.Close();

	// Obtener version grabada
	cHeader[0x0a] = '\0';
	nRecVer = ::strtoul(&cHeader[0x09], NULL, 16);
	nRecVer <<= 8;
	cHeader[0x0d] = '\0';
	nRecVer |= ::strtoul(&cHeader[0x0b], NULL, 16);

	// Obtener version actual
	::GetVM()->GetVersion(dwMajor, dwMinor);
	nNowVer = (int)((dwMajor << 8) | dwMinor);

	// Verificacion de cabecera
	cHeader[0x09] = '\0';
	if (strcmp(cHeader, "XM6 DATA ") != 0) {
		if (bWarning) {
			::GetMsg(IDS_XM6LOADHDR, strMsg);
			MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
		return FALSE;
	}

	// ÉoÅ[ÉWÉáÉìÉ`ÉFÉbÉN
	if (nNowVer < nRecVer) {
		// La version grabada es mas reciente (formato desconocido)
		::GetMsg(IDS_XM6LOADVER, strMsg);
		strFmt.Format(strMsg,
						nNowVer >> 8, nNowVer & 0xff,
						nRecVer >> 8, nRecVer & 0xff);
		MessageBox(strFmt, NULL, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// Continuar
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Sub-apertura
//
//---------------------------------------------------------------------------
BOOL FASTCALL CFrmWnd::OnOpenSub(const Filepath& path)
{
	BOOL bRun;
	BOOL bSound;
	CString strMsg;
	DWORD dwPos;
	Filepath diskpath;
	int nDrive;

	// Detener planificador y sonido
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);

	// Cargar
	AfxGetApp()->BeginWaitCursor();

	// VM
	dwPos = ::GetVM()->Load(path);	
	//MessageBox("dwPos: " + dwPos, NULL, MB_ICONSTOP | MB_OK);
	if (dwPos == 0) {
		AfxGetApp()->EndWaitCursor();

		// El fallo es peligroso por interrupcion a medias; resetear siempre
		::GetVM()->Reset();
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// CargarError
		::GetMsg(IDS_XM6LOADERR, strMsg);
		CString msg;
		msg.Format(_T("Lectura de archivo (VM): %u"), dwPos);
		MessageBox(msg, NULL, MB_ICONSTOP | MB_OK);

		return FALSE;
	}

	// MFC
	if (!LoadComponent(path, dwPos)) {
		AfxGetApp()->EndWaitCursor();

		// El fallo es peligroso por interrupcion a medias; resetear siempre
		::GetVM()->Reset();
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// CargarError
		::GetMsg(IDS_XM6LOADERR, strMsg);
		MessageBox("Fallo en Lectura de archivo (MFC)", NULL, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// CargarSalir
	AfxGetApp()->EndWaitCursor();

	// Anadir FD, MO, CD a MRU (medida para reanudacion desde v2.04)
	for (nDrive=0; nDrive<2; nDrive++) {
		if (m_pFDD->IsReady(nDrive, FALSE)) {
			m_pFDD->GetPath(nDrive, diskpath);
			GetConfig()->SetMRUFile(nDrive, diskpath.GetPath());
		}
	}
	if (m_pSASI->IsReady()) {
		m_pSASI->GetPath(diskpath);
		GetConfig()->SetMRUFile(2, diskpath.GetPath());
	}
	if (m_pSCSI->IsReady(FALSE)) {
		m_pSCSI->GetPath(diskpath, FALSE);
		GetConfig()->SetMRUFile(3, diskpath.GetPath());
	}

	// Si se guardo con el planificador detenido, permanecer detenido (v2.04)
	if (GetScheduler()->HasSavedEnable()) {
		bRun = GetScheduler()->GetSavedEnable();
	}

	// Limpiar contador de ejecucion
	m_dwExec = 0;

	// Exito
	GetSound()->Enable(bSound);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();

	// Anadir a MRU
	GetConfig()->SetMRUFile(4, path.GetPath());

	// Mostrar mensaje de informacion
	::GetMsg(IDS_XM6LOADOK, strMsg);
	SetInfo(strMsg);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Guardar/Sobrescribir
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSave()
{
	Filepath path;

	// Recibir ruta actual de la VM
	::GetVM()->GetPath(path);

	// Si esta limpio, terminar
	if (path.IsClear()) {
		return;
	}

	// Sub-guardado
	OnSaveSub(path);
}

//---------------------------------------------------------------------------
//
//	UI de sobrescribir
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveUI(CCmdUI *pCmdUI)
{
	Filepath path;

	// Prohibir si la energia esta apagada
	if (!::GetVM()->IsPower()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Recibir ruta actual de la VM
	::GetVM()->GetPath(path);

	// Prohibir uso si esta limpio
	if (path.IsClear()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Permitir uso
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Guardado r·pido
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveAs()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];

	// Ejecutar dialogo comun
	::GetVM()->GetPath(path);
	_tcscpy(szPath, path.GetPath());
	/*if (!::FileSaveDlg(this, szPath, _T("xm6"), IDS_XM6OPEN)) {
		ResetCaption();		 

		return;
	}*/

	 TCHAR lpFile[_MAX_PATH];
	 strcpy(lpFile,RutaSaveStates);
	 TCHAR cadenaArchivo[_MAX_PATH];
	 sprintf(cadenaArchivo, "%s\\%s.xm6", lpFile, NombreArchivoXM6);

	 /* ACA SE GUARDA UN ESTADO RAPIDO */
	

	/* int msgboxID = MessageBox(
		 cadenaArchivo, "GUARDADO",
        4     );*/



	 path.SetPath(cadenaArchivo);
			

	// Sub-guardado
	OnSaveSub(path);
}

//---------------------------------------------------------------------------
//
//	UI de guardar como
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveAsUI(CCmdUI *pCmdUI)
{
	// Solo si la energia esta activada
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	Sub-guardado
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnSaveSub(const Filepath& path)
{
	BOOL bRun;
	BOOL bSound;
	CString strMsg;
	DWORD dwPos;

	// Detener planificador y sonido
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);

	AfxGetApp()->BeginWaitCursor();

	// Notificar al planificador el estado al momento de guardar (v2.04)
	GetScheduler()->SetSavedEnable(bRun);

	// VM
	 // AsegÌVate de incluir un nombre de archivo completo; ajusta segÌR necesites
	dwPos = ::GetVM()->Save(path);

	if (dwPos== 0) {
		AfxGetApp()->EndWaitCursor();

		// Fallo al guardar
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// Error al guardar
		::GetMsg(IDS_XM6SAVEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		return;
	}

	// MFC
	if (!SaveComponent(path, dwPos)) {
		AfxGetApp()->EndWaitCursor();

		// Fallo al guardar
		GetSound()->Enable(bSound);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();

		// Error al guardar
		::GetMsg(IDS_XM6SAVEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		return;
	}

	// Limpiar contador de ejecucion
	m_dwExec = 0;
	AfxGetApp()->EndWaitCursor();

	// Exito
	GetSound()->Enable(bSound);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();

	// Anadir a MRU
	GetConfig()->SetMRUFile(4, path.GetPath());

	// Mostrar mensaje de informacion
	::GetMsg(IDS_XM6SAVEOK, strMsg);
	SetInfo(strMsg);
}

//---------------------------------------------------------------------------
//
//	MRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMRU(UINT uID)
{
	TCHAR szMRU[_MAX_PATH];
	Filepath path;

	ASSERT(uID >= IDM_XM6_MRU0);

	// Conversion de uID
	uID -= IDM_XM6_MRU0;
	ASSERT(uID <= 8);

	// Obtener MRU, crear ruta
	GetConfig()->GetMRUFile(4, (int)uID, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// Pre-procesamiento de apertura
	if (!OnOpenPrep(path)) {
		return;
	}

	// Apertura comun
	if (OnOpenSub(path)) {
		// Actualizar directorio por defecto
		Filepath::SetDefaultDir(szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMRUUI(CCmdUI *pCmdUI)
{
	// Solo si la energia esta activada
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	Reiniciar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnReset()
{
	SRAM *pSRAM;
	DWORD Sw[0x100];
	DWORD dwDevice;
	DWORD dwAddr;
	CString strReset;
	CString strSub;
	BOOL bFlag;
	int i;


	// Operacion no disponible si la energia esta desactivada
	if (!::GetVM()->IsPower()) {
		return;
	}

	::LockVM();

	// Reiniciar y redibujar
	::GetVM()->Reset();
	//OutputDebugString("\n\nSe ejecuto GetVM->Reset\n\n");
	GetView()->Refresh();
	ResetCaption();

	// Obtener interruptores de memoria
	pSRAM = (SRAM*)::GetVM()->SearchDevice(MAKEID('S', 'R', 'A', 'M'));
	ASSERT(pSRAM);
	for (i=0; i<0x100; i++) {
		Sw[i] = pSRAM->ReadOnly(0xed0000 + i);
	}

	::UnlockVM();
	//OutputDebugString("\n\nSe ejecuto UnlockVm\n\n");

	// Cargar mensaje de reinicio
	::GetMsg(IDS_RESET, strReset);

	// Comparar el inicio de los interruptores de memoria
	if (memcmp(Sw, SigTable, sizeof(DWORD) * 7) != 0) {
		SetInfo(strReset);
		return;
	}

	// Obtener dispositivo de arranque
	dwDevice = Sw[0x18];
	dwDevice <<= 8;
	dwDevice |= Sw[0x19];

	// Discriminar dispositivo de arranque
	bFlag = FALSE;
	if (dwDevice == 0x0000) {
		// STD
		strSub = _T("STD)");
		bFlag = TRUE;
	}
	if (dwDevice == 0xa000) {
		// ROM
		dwAddr = Sw[0x0c];
		dwAddr = (dwAddr << 8) | Sw[0x0d];
		dwAddr = (dwAddr << 8) | Sw[0x0e];
		dwAddr = (dwAddr << 8) | Sw[0x0f];

		// FC0000Å`FC001CÇ∆ÅAEA0020Å`EA003CÇÕSCSI#
		strSub.Format(_T("ROM $%06X)"), dwAddr);
		if ((dwAddr >= 0xfc0000) && (dwAddr < 0xfc0020)) {
			strSub.Format(_T("SCSI%1d)"), (dwAddr & 0x001f) >> 2);
		}
		if ((dwAddr >= 0xea0020) && (dwAddr < 0xea0040)) {
			strSub.Format(_T("SCSI%1d)"), (dwAddr & 0x001f) >> 2);
		}
		bFlag = TRUE;
	}
	if (dwDevice == 0xb000) {
		// RAM
		dwAddr = Sw[0x10];
		dwAddr = (dwAddr << 8) | Sw[0x11];
		dwAddr = (dwAddr << 8) | Sw[0x12];
		dwAddr = (dwAddr << 8) | Sw[0x13];
		strSub.Format(_T("RAM $%06X)"), dwAddr);
		bFlag = TRUE;
	}
	if ((dwDevice & 0xf0ff) == 0x9070) {
		strSub.Format(_T("2HD%1d)"), (dwDevice & 0xf00) >> 8);
		bFlag = TRUE;
	}
	if ((dwDevice & 0xf0ff) == 0x8000) {
		strSub.Format(_T("HD%1d)"), (dwDevice & 0xf00) >> 8);
		bFlag = TRUE;
	}
	if (!bFlag) {
		strSub = _T("Unknown)");
	}

	// Mostrar
	strReset += _T(" (");
	strReset += strSub;
	SetInfo(strReset);
	OutputDebugString("\n\nSe ejecuto OnReset viejo...\n\n");
}

//---------------------------------------------------------------------------
//
//	UI de reinicio
//
//---------------------------------------------------------------------------
void CFrmWnd::OnResetUI(CCmdUI *pCmdUI)
{
	// ìdåπONÇ»ÇÁëÄçÏÇ≈Ç´ÇÈ
	pCmdUI->Enable(::GetVM()->IsPower());
}



void CFrmWnd::OnResetNuevo() // Comando para guardar configuracion y luego resetear *-*
{
	char buffer[MAX_PATH];
	GetModuleFileNameA(NULL, buffer, MAX_PATH);
	//MessageBox(buffer, "ConfiguraciÛn", MB_OK);
	ShellExecute(GetSafeHwnd(), "open", buffer, NULL, NULL, 1);

	OutputDebugString("\n\nSe ejecuto OnReset Nuevo...\n\n");
	PostMessage(WM_CLOSE, 0, 0);
}




// Guardar configuracion personalizada por juego 
void CFrmWnd::OnScc()
{
	if (NombreArchivoXM6.GetLength() > 0)
	{
		m_pConfig->CustomInit(FALSE);
		m_pConfig->Cleanup2();

		CString sz;
		sz.Format(_T("\n\nSe ha guardado la configuraciÛn para %s\n\n"), NombreArchivoXM6);
		OutputDebugStringW(CT2W(sz));

		MessageBox(sz, "ConfiguraciÛn", MB_OK);
	}
	else
	{
		MessageBox("No se ha guardado configuraciÛn ya que no se carg? juego", "ConfiguraciÛn", MB_OK);
	}
}



void CFrmWnd::OnSccUI(CCmdUI* pCmdUI)
{
	// Solo si la energia esta activada
	pCmdUI->Enable(::GetVM()->IsPower());
}


// Comando para guardar la configuracion global
void CFrmWnd::OnSgc()
{		
		m_pConfig->CustomInit(TRUE);
		m_pConfig->Cleanup2();

		CString sz;
		sz.Format(_T("\n\nSe ha guardado la configuraciÛn global\n\n"), NombreArchivoXM6);
		OutputDebugStringW(CT2W(sz));

		MessageBox(sz, "ConfiguraciÛn", MB_OK);	
}



void CFrmWnd::OnSgcUI(CCmdUI* pCmdUI)
{
	// Solo si la energia esta activada
	pCmdUI->Enable(::GetVM()->IsPower());
}





void CFrmWnd::OnSgcr() // Guarda config global y reinicia aplicaciÛn *-*
{	
	m_pConfig->CustomInit(TRUE);
	m_pConfig->Cleanup2();
	OnResetNuevo();
}



void CFrmWnd::OnSgcrUI(CCmdUI* pCmdUI)
{
	// Solo si la energia esta activada
	pCmdUI->Enable(::GetVM()->IsPower());
}


//---------------------------------------------------------------------------
//
//	Tabla de firmas SRAM
//
//---------------------------------------------------------------------------
const DWORD CFrmWnd::SigTable[] = {
	0x82, 0x77, 0x36, 0x38, 0x30, 0x30, 0x30
};

//---------------------------------------------------------------------------
//
//	ÉCÉìÉ^ÉâÉvÉg
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInterrupt()
{
	CString strIntr;

	// ìdåπONÇ»ÇÁëÄçÏÇ≈Ç´ÇÈ
	if (::GetVM()->IsPower()) {
		// Interrupcion NMI
		::LockVM();
		::GetVM()->Interrupt();
		::UnlockVM();

		// ÉÅÉbÉZÅ[ÉW
		::GetMsg(IDS_INTERRUPT, strIntr);
		SetInfo(strIntr);
	}
}

//---------------------------------------------------------------------------
//
//	UI de interrupcion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInterruptUI(CCmdUI *pCmdUI)
{
	// ìdåπONÇ»ÇÁëÄçÏÇ≈Ç´ÇÈ
	pCmdUI->Enable(::GetVM()->IsPower());
}

//---------------------------------------------------------------------------
//
//	ìdåπÉXÉCÉbÉ`
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPower()
{
	BOOL bPower;

	::LockVM();

	if (::GetVM()->IsPowerSW()) {
		// Si esta encendido, apagar
		::GetVM()->PowerSW(FALSE);
		::UnlockVM();
		return;
	}

	// åªç›ÇÃìdåπÇÃèÛë‘ÇGuardarÇµÇƒÅAìdåπON
	bPower = ::GetVM()->IsPower();
	::GetVM()->PowerSW(TRUE);

	// ìdåπÇ™êÿÇÍÇƒÇ¢ÇƒÉXÉPÉWÉÖÅ[ÉâÇ™é~Ç‹Ç¡ÇƒÇ¢ÇÍÇŒÅAìÆÇ©Ç∑
	if (!bPower && !GetScheduler()->IsEnable()) {
		GetScheduler()->Enable(TRUE);
	}

	::UnlockVM();

	// Reiniciar(Barra de estadoMostrarÇÃÇΩÇﬂ)
	OnReset();
}

//---------------------------------------------------------------------------
//
//	UI de interruptor de encendido
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPowerUI(CCmdUI *pCmdUI)
{
	// Ç∆ÇËÇ†Ç¶Ç∏ÅASi esta encendido, marcar (check)
	pCmdUI->SetCheck(::GetVM()->IsPowerSW());
}

//---------------------------------------------------------------------------
//
//	Salir
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExit()
{
	PostMessage(WM_CLOSE, 0, 0);
}

//---------------------------------------------------------------------------
//
//	Procesamiento de disco floppy
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFD(UINT uID)
{
	int nDrive;

	// Determinar unidad
	nDrive = 0;
	if (uID >= IDM_D1OPEN) {
		nDrive = 1;
		uID -= (IDM_D1OPEN - IDM_D0OPEN);
	}

	switch (uID) {
		// Abrir
		case IDM_D0OPEN:
			OnFDOpen(nDrive);
			break;

		// Expulsar
		case IDM_D0EJECT:
			OnFDEject(nDrive);
			break;

		// Proteccion contra escritura
		case IDM_D0WRITEP:
			OnFDWriteP(nDrive);
			break;

		// Expulsion forzada
		case IDM_D0FORCE:
			OnFDForce(nDrive);
			break;

		// åÎë}ì¸
		case IDM_D0INVALID:
			OnFDInvalid(nDrive);
			break;

		// ÇªÇÍà»äO
		default:
			if (uID >= IDM_D0_MRU0) {
				// MRU
				uID -= IDM_D0_MRU0;
				ASSERT(uID <= 8);
				OnFDMRU(nDrive, (int)uID);
			}
			else {
				// Media
				uID -= IDM_D0_MEDIA0;
				ASSERT(uID <= 15);
				OnFDMedia(nDrive, (int)uID);
			}
			break;
	}
}

//---------------------------------------------------------------------------
//
//	Abrir rom floppy
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDOpen(int nDrive)
{
	Filepath path;
	CString strMsg;
	TCHAR szPath[_MAX_PATH];
	FDI *pFDI;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT(m_pFDD);

	// Ejecutar dialogo comun
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_FDOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	/* ACA SE ABREN ARCHIVOS DE FLOPPY (ROMS) */


	CString sz;	
	sz.Format(_T("%s"),  szPath);	
    CString fileName= sz.Mid(sz.ReverseFind('\\')+1);

	RutaCompletaArchivoXM6 = szPath;
	NombreArchivoXM6 = fileName;

			

	// VMÉçÉbÉN
	::LockVM();

	// Asignar disco
	if (!m_pFDD->Open(nDrive, path)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// AbrirError
		::GetMsg(IDS_FDERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Obtener FDI antes de reiniciar la VM
	pFDI = m_pFDD->GetFDI(nDrive);

	// Exito
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Anadir a MRU
	GetConfig()->SetMRUFile(nDrive, szPath);

	// ExitoÇ»ÇÁÅABADÉCÉÅÅ[ÉWAdvertencia
	if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
		::GetMsg(IDS_BADFDI_WARNING, strMsg);
		//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
	}
}

//---------------------------------------------------------------------------
//
//	Expulsion de floppy
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDEject(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pFDD->Eject(nDrive, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	ÉtÉçÉbÉsÅ[Proteccion contra escritura
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDWriteP(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// ÉCÉÅÅ[ÉWÇëÄçÏ
	::LockVM();
	m_pFDD->WriteP(nDrive, !m_pFDD->IsWriteP(nDrive));
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	ÉtÉçÉbÉsÅ[Expulsion forzada
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDForce(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pFDD->Eject(nDrive, TRUE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Insercion incorrecta de floppy
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDInvalid(int nDrive)
{
	ASSERT(m_pFDD);
	ASSERT((nDrive == 0) || (nDrive == 1));

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pFDD->Invalid(nDrive);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Medios (discos) floppy
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDMedia(int nDrive, int nMedia)
{
	Filepath path;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT((nMedia >= 0) && (nMedia <= 15));

	// VMÉçÉbÉN
	::LockVM();

	// Confirmar por si acaso
	if (nMedia < m_pFDD->GetDisks(nDrive)) {
		m_pFDD->GetPath(nDrive, path);

		// çƒAbrir
		m_pFDD->Open(nDrive, path, nMedia);
	}

	// VMÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Abrir desde MRU guardada
//
//---------------------------------------------------------------------------
void FASTCALL CFrmWnd::OnFDMRU(int nDrive, int nMRU)
{
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;
	FDI *pFDI;
	CString strMsg;

	ASSERT((nDrive == 0) || (nDrive == 1));
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// Obtener MRU, crear ruta
	GetConfig()->GetMRUFile(nDrive, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	CString sz;	
	sz.Format(_T("%s"),  szMRU);	
    CString fileName= sz.Mid(sz.ReverseFind('\\')+1);


	/* Cuando se carga rom desde lista MRU */
	RutaCompletaArchivoXM6 = szMRU;
	NombreArchivoXM6 = fileName;
	

	// VMÉçÉbÉN
	::LockVM();

	// Intentar asignar disco
	bResult = m_pFDD->Open(nDrive, path);
	pFDI = m_pFDD->GetFDI(nDrive);
	GetScheduler()->Reset();
	ResetCaption();

	// VMÉAÉìÉçÉbÉN
	::UnlockVM();

	// Si tiene exito, actualizar directorio y anadir a MRU
	if (bResult) {
		// Actualizar directorio por defecto
		Filepath::SetDefaultDir(szMRU);

		// Anadir a MRU
		GetConfig()->SetMRUFile(nDrive, szMRU);

		// BADÉCÉÅÅ[ÉWAdvertencia
		if (pFDI->GetID() == MAKEID('B', 'A', 'D', ' ')) {
			::GetMsg(IDS_BADFDI_WARNING, strMsg);
			//MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		}
	}
}

//---------------------------------------------------------------------------
//
//	UI de apertura de floppy
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDOpenUI(CCmdUI *pCmdUI)
{
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;
	int nDrive;
	int nStat;
	int nDisks;
	int nMedia;
	char szShort[_MAX_PATH];
	LPTSTR lpszShort;
	int i;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];

	ASSERT(this);
	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// Expulsarã÷é~Ç≈ÅAÉfÉBÉXÉNÇ†ÇËà»äOÇÕAbrirÇ≈Ç´ÇÈ
	::LockVM();
	nStat = m_pFDD->GetStatus(nDrive);
	m_nFDDStatus[nDrive] = nStat;
	nDisks = m_pFDD->GetDisks(nDrive);
	nMedia = m_pFDD->GetMedia(nDrive);
	::UnlockVM();
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}

	// Obtener submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	// Floppy 0 y Floppy 1 se alinean despues de Archivo (F)
	pSubMenu = pMenu->GetSubMenu(nDrive + 1);

	// UI de expulsion (medida para el timing de ON_UPDATE_COMMAND_UI a continuacion)
	if ((nStat & FDST_INSERT) && (nStat & FDST_EJECT)) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Proteccion contra escrituraUI
	if (m_pFDD->IsReadOnly(nDrive) || !(nStat & FDST_INSERT)) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// Expulsion forzadaUI
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// åÎë}ì¸UI
	if (!(nStat & FDST_INSERT) && !(nStat & FDST_INVALID)) {
		pSubMenu->EnableMenuItem(5, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(5, MF_BYPOSITION | MF_GRAYED);
	}

	// à»ç~ÇÃMenuÇÕÇ∑Ç◊ÇƒEliminar
	while (pSubMenu->GetMenuItemCount() > 6) {
		pSubMenu->RemoveMenu(6, MF_BYPOSITION);
	}

	// Procesamiento multidisco
	if (nDisks > 1) {
		// Configuracion de constantes de activado/desactivado
		if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
		else {
			nEnable = MF_BYCOMMAND | MF_GRAYED;
		}

		// ÉZÉpÉåÅ[É^Çë}ì¸
		pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

		// Bucle de medios (discos)
		ASSERT(nDisks <= 16);
		for (i=0; i<nDisks; i++) {
			// Los nombres de disco se almacenan como char*; convertir a TCHAR
			m_pFDD->GetName(nDrive, szShort, i);
			lpszShort = A2T(szShort);

			// í«â¡
			if (nDrive == 0) {
				pSubMenu->AppendMenu(MF_STRING, IDM_D0_MEDIA0 + i, lpszShort);
				pSubMenu->EnableMenuItem(IDM_D0_MEDIA0 + i, nEnable);
			}
			else {
				pSubMenu->AppendMenu(MF_STRING, IDM_D1_MEDIA0 + i, lpszShort);
				pSubMenu->EnableMenuItem(IDM_D1_MEDIA0 + i, nEnable);
			}
		}

		// Configuracion de botones de radio
		if (nDrive == 0) {
			pSubMenu->CheckMenuRadioItem(IDM_D0_MEDIA0, IDM_D0_MEDIAF,
										IDM_D0_MEDIA0 + nMedia, MF_BYCOMMAND);
		}
		else {
			pSubMenu->CheckMenuRadioItem(IDM_D1_MEDIA0, IDM_D1_MEDIAF,
										IDM_D1_MEDIA0 + nMedia, MF_BYCOMMAND);
		}
	}

	// Procesamiento MRU - Separador
	if (GetConfig()->GetMRUNum(nDrive) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// Configuracion de constantes de activado/desactivado
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		nEnable = MF_BYCOMMAND | MF_GRAYED;
	}
	else {
		nEnable = MF_BYCOMMAND | MF_ENABLED;
	}


	

	// Procesamiento MRU - Anadir
	for (i=0; i<9; i++) {
		// Intentar obtener
		GetConfig()->GetMRUFile(nDrive, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Ç†ÇÍÇŒMenuÇ…í«â¡
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);	
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);
		if (nDrive == 0) {
			pSubMenu->AppendMenu(MF_STRING, IDM_D0_MRU0 + i, szMRU);
			pSubMenu->EnableMenuItem(IDM_D0_MRU0 + i, nEnable);
		}
		else {
			pSubMenu->AppendMenu(MF_STRING, IDM_D1_MRU0 + i, szMRU);
			pSubMenu->EnableMenuItem(IDM_D1_MRU0 + i, nEnable);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Expulsion de floppy UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDEjectUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// Se puede expulsar si esta insertado y la expulsion no esta prohibida
	if ((nStat & FDST_INSERT) && (nStat & FDST_EJECT)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	ÉtÉçÉbÉsÅ[Proteccion contra escritura UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDWritePUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// Marcar segun la proteccion contra escritura
	pCmdUI->SetCheck(m_pFDD->IsWriteP(nDrive));

	// Desactivar si es de solo lectura o no esta insertado
	if (m_pFDD->IsReadOnly(nDrive) || !(nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
		return;
	}
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	ÉtÉçÉbÉsÅ[Expulsion forzada UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDForceUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// Expulsarã÷é~ÇÃéûÇÃÇ›Activado
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Insercion incorrecta de floppy UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDInvalidUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// ë}ì¸Ç≥ÇÍÇƒÇ¢Ç»Ç¢éûÇÃÇ›Activado
	if (!(nStat & FDST_INSERT) && !(nStat & FDST_INVALID)) {
		pCmdUI->Enable(TRUE);
		return;
	}
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Medios (discos) floppy UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDMediaUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// Expulsarã÷é~Ç≈ÅAÉfÉBÉXÉNÇ†ÇËà»äOÇÕAbrirÇ≈Ç´ÇÈ
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	ÉtÉçÉbÉsÅ[MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDMRUUI(CCmdUI *pCmdUI)
{
	int nDrive;
	int nStat;

	ASSERT(m_pFDD);

	// Determinar unidad
	nDrive = 0;
	if (pCmdUI->m_nID >= IDM_D1OPEN) {
		nDrive = 1;
	}

	// ÉXÉeÅ[É^ÉXObtener
	nStat = m_nFDDStatus[nDrive];

	// Expulsarã÷é~Ç≈ÅAÉfÉBÉXÉNÇ†ÇËà»äOÇÕAbrirÇ≈Ç´ÇÈ
	if (!(nStat & FDST_EJECT) && (nStat & FDST_INSERT)) {
		pCmdUI->Enable(FALSE);
	}
	else {
		pCmdUI->Enable(TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Apertura de disco MO
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	CString strMsg;

	ASSERT(this);
	ASSERT(m_pSASI);

	// Ejecutar dialogo comun
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_MOOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	// VMÉçÉbÉN
	::LockVM();

	// MOAsignar disco
	if (!m_pSASI->Open(path)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// AbrirError
		::GetMsg(IDS_MOERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Exito
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Anadir a MRU
	GetConfig()->SetMRUFile(2, szPath);
}

//---------------------------------------------------------------------------
//
//	UI de apertura de disco MO
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOOpenUI(CCmdUI *pCmdUI)
{
	int i;
	BOOL bValid;
	BOOL bReady;
	BOOL bWriteP;
	BOOL bReadOnly;
	BOOL bLocked;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;

	ASSERT(this);
	ASSERT(m_pSASI);

	// ÉhÉâÉCÉuÇÃèÛë‘ÇObtener(ÉçÉbÉNÇµÇƒçsÇ§)
	::LockVM();
	bValid = m_pSASI->IsValid();
	bReady = m_pSASI->IsReady();
	bWriteP = m_pSASI->IsWriteP();
	bReadOnly = m_pSASI->IsReadOnly();
	bLocked = m_pSASI->IsLocked();
	::UnlockVM();

	// Abrir
	if (bValid) {
		if (bReady && bLocked) {
			pCmdUI->Enable(FALSE);
		}
		else {
			pCmdUI->Enable(TRUE);
		}
	}
	else {
		pCmdUI->Enable(FALSE);
	}

	// Obtener submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	// MOMenuÇÕÉtÉ@ÉCÉãÅAÉtÉçÉbÉsÅ[0ÅAÉtÉçÉbÉsÅ[1ÇÃéü
	pSubMenu = pMenu->GetSubMenu(3);
	ASSERT(pSubMenu);

	// UI de expulsion (medida para el timing de ON_UPDATE_COMMAND_UI a continuacion)
	if (bReady && !bLocked) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Proteccion contra escrituraUI
	if (bReady && !bReadOnly) {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_GRAYED);
	}
	else {
		pSubMenu->EnableMenuItem(2, MF_BYPOSITION | MF_ENABLED);
	}

	// Expulsion forzadaUI
	if (bReady && bLocked) {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(4, MF_BYPOSITION | MF_GRAYED);
	}

	// à»ç~ÇÃMenuÇÕÇ∑Ç◊ÇƒEliminar
	while (pSubMenu->GetMenuItemCount() > 5) {
		pSubMenu->RemoveMenu(5, MF_BYPOSITION);
	}

	// Procesamiento MRU - Separador
	if (GetConfig()->GetMRUNum(2) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// Configuracion de constantes de activado/desactivado
	nEnable = MF_BYCOMMAND | MF_GRAYED;
	if (bValid) {
		// ÉhÉâÉCÉuActivadoÇ≈
		if (!bReady || !bLocked) {
			// Si no hay medio o no esta bloqueado, se puede insertar
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
	}

	// Procesamiento MRU - Anadir
	for (i=0; i<9; i++) {
		// Intentar obtener
		GetConfig()->GetMRUFile(2, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Ç†ÇÍÇŒMenuÇ…í«â¡
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_MO_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_MO_MRU0 + i, nEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Expulsion de disco MO
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOEject()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pSASI->Eject(FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Expulsion de disco MO UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOEjectUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// ÉåÉfÉBÇ≈Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSASI->IsReady()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢ÇÍÇŒã÷é~
	if (m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNProteccion contra escritura
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOWriteP()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pSASI->WriteP(!m_pSASI->IsWriteP());
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNProteccion contra escritura UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOWritePUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// ÉåÉfÉBÇ≈Ç»ÇØÇÍÇŒÉ`ÉFÉbÉNÇ»ÇµÅAã÷é~
	if (!m_pSASI->IsReady()) {
		pCmdUI->SetCheck(0);
		pCmdUI->Enable(FALSE);
		return;
	}

	// Read OnlyÇ≈Ç†ÇÍÇŒÉ`ÉFÉbÉNÇ†ÇËÅAã÷é~
	if (m_pSASI->IsReadOnly()) {
		pCmdUI->SetCheck(1);
		pCmdUI->Enable(FALSE);
		return;
	}

	// åªèÛÇ…âûÇ∂ÇƒÉ`ÉFÉbÉNÇµÇƒÅAãñâ¬
	pCmdUI->SetCheck(m_pSASI->IsWriteP());
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNExpulsion forzada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOForce()
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pSASI->Eject(TRUE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNExpulsion forzada UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOForceUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// ÉåÉfÉBÇ≈Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSASI->IsReady()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNMRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOMRU(UINT uID)
{
	int nMRU;
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;

	ASSERT(this);
	ASSERT(m_pSASI);
	ASSERT((uID >= IDM_MO_MRU0) && (uID <= IDM_MO_MRU8));

	// ÉCÉìÉfÉbÉNÉXCrear
	nMRU = uID - IDM_MO_MRU0;
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// MRUObtener
	GetConfig()->GetMRUFile(2, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// VMÉçÉbÉN
	::LockVM();

	// Abrir
	bResult = m_pSASI->Open(path);
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Si tiene exito, actualizar directorio y anadir a MRU
	if (bResult) {
		// ÉCÉjÉVÉÉÉãÉfÉBÉåÉNÉgÉäActualizar
		Filepath::SetDefaultDir(szMRU);

		// Anadir a MRU
		GetConfig()->SetMRUFile(2, szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	MOÉfÉBÉXÉNMRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMOMRUUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSASI);

	// ÉhÉâÉCÉuÇ™ActivadoÇ≈Ç»ÇØÇŒDesactivado
	if (!m_pSASI->IsValid()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢ÇÍÇŒã÷é~
	if (m_pSASI->IsLocked()) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Apertura de CD-ROM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDOpen()
{
	Filepath path;
	TCHAR szPath[_MAX_PATH];
	CString strMsg;

	ASSERT(this);
	ASSERT(m_pSCSI);

	// Ejecutar dialogo comun
	memset(szPath, 0, sizeof(szPath));
	if (!::FileOpenDlg(this, szPath, IDS_CDOPEN)) {
		ResetCaption();
		return;
	}
	path.SetPath(szPath);

	// VMÉçÉbÉN
	::LockVM();

	// CDAsignar disco
	if (!m_pSCSI->Open(path, FALSE)) {
		GetScheduler()->Reset();
		::UnlockVM();

		// AbrirError
		::GetMsg(IDS_CDERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		ResetCaption();
		return;
	}

	// Exito
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Anadir a MRU
	GetConfig()->SetMRUFile(3, szPath);
}

//---------------------------------------------------------------------------
//
//	UI de apertura de CD-ROM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDOpenUI(CCmdUI *pCmdUI)
{
	int i;
	BOOL bValid;
	BOOL bReady;
	BOOL bLocked;
	TCHAR szMRU[_MAX_PATH];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	TCHAR szFile[_MAX_FNAME];
	TCHAR szExt[_MAX_EXT];
	CMenu *pMenu;
	CMenu *pSubMenu;
	UINT nEnable;

	ASSERT(this);
	ASSERT(m_pSCSI);

	// ÉhÉâÉCÉuÇÃèÛë‘ÇObtener(ÉçÉbÉNÇµÇƒçsÇ§)
	::LockVM();
	bValid = m_pSCSI->IsValid(FALSE);
	bReady = m_pSCSI->IsReady(FALSE);
	bLocked = m_pSCSI->IsLocked(FALSE);
	::UnlockVM();

	// Abrir
	if (bValid) {
		if (bReady && bLocked) {
			pCmdUI->Enable(FALSE);
		}
		else {
			pCmdUI->Enable(TRUE);
		}
	}
	else {
		pCmdUI->Enable(FALSE);
	}

	// Obtener submenu
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);

	// El menu CD esta despues de Archivo, Floppy 0, Floppy 1, MO
	pSubMenu = pMenu->GetSubMenu(4);
	ASSERT(pSubMenu);

	// UI de expulsion (medida para el timing de ON_UPDATE_COMMAND_UI a continuacion)
	if (bReady && !bLocked) {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(1, MF_BYPOSITION | MF_GRAYED);
	}

	// Expulsion forzadaUI
	if (bReady && bLocked) {
		pSubMenu->EnableMenuItem(3, MF_BYPOSITION | MF_ENABLED);
	}
	else {
		pSubMenu->EnableMenuItem(3, MF_BYPOSITION | MF_GRAYED);
	}

	// à»ç~ÇÃMenuÇÕÇ∑Ç◊ÇƒEliminar
	while (pSubMenu->GetMenuItemCount() > 4) {
		pSubMenu->RemoveMenu(4, MF_BYPOSITION);
	}

	// Procesamiento MRU - Separador
	if (GetConfig()->GetMRUNum(3) == 0) {
		return;
	}
	pSubMenu->AppendMenu(MF_SEPARATOR, 0, (LPCTSTR)NULL);

	// Configuracion de constantes de activado/desactivado
	nEnable = MF_BYCOMMAND | MF_GRAYED;
	if (bValid) {
		// ÉhÉâÉCÉuActivadoÇ≈
		if (!bReady || !bLocked) {
			// Si no hay medio o no esta bloqueado, se puede insertar
			nEnable = MF_BYCOMMAND | MF_ENABLED;
		}
	}

	// Procesamiento MRU - Anadir
	for (i=0; i<9; i++) {
		// Intentar obtener
		GetConfig()->GetMRUFile(3, i, szMRU);
		if (szMRU[0] == _T('\0')) {
			break;
		}

		// Ç†ÇÍÇŒMenuÇ…í«â¡
		_tsplitpath(szMRU, szDrive, szDir, szFile, szExt);
		if (_tcslen(szDir) > 1) {
			_tcscpy(szDir, _T("\\...\\"));
		}
		_stprintf(szMRU, _T("&%d "), i + 1);
		_tcscat(szMRU, szDrive);
		_tcscat(szMRU, szDir);
		_tcscat(szMRU, szFile);
		_tcscat(szMRU, szExt);

		pSubMenu->AppendMenu(MF_STRING, IDM_CD_MRU0 + i, szMRU);
		pSubMenu->EnableMenuItem(IDM_CD_MRU0 + i, nEnable);
	}
}

//---------------------------------------------------------------------------
//
//	Expulsion de CD-ROM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDEject()
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pSCSI->Eject(FALSE, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Expulsion de CD-ROM UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDEjectUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// ÉåÉfÉBÇ≈Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSCSI->IsReady(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢ÇÍÇŒã÷é~
	if (m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	CD-ROMExpulsion forzada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDForce()
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// VMÇÉçÉbÉNÇµÇƒçsÇ§
	::LockVM();
	m_pSCSI->Eject(TRUE, FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	CD-ROMExpulsion forzada UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDForceUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// ÉåÉfÉBÇ≈Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSCSI->IsReady(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢Ç»ÇØÇÍÇŒã÷é~
	if (!m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	CD-ROM MRU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDMRU(UINT uID)
{
	int nMRU;
	TCHAR szMRU[_MAX_PATH];
	Filepath path;
	BOOL bResult;

	ASSERT(this);
	ASSERT(m_pSCSI);
	ASSERT((uID >= IDM_CD_MRU0) && (uID <= IDM_CD_MRU8));

	// ÉCÉìÉfÉbÉNÉXCrear
	nMRU = uID - IDM_CD_MRU0;
	ASSERT((nMRU >= 0) && (nMRU <= 8));

	// MRUObtener
	GetConfig()->GetMRUFile(3, nMRU, szMRU);
	if (szMRU[0] == _T('\0')) {
		return;
	}
	path.SetPath(szMRU);

	// VMÉçÉbÉN
	::LockVM();

	// Abrir
	bResult = m_pSCSI->Open(path, FALSE);
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();

	// Si tiene exito, actualizar directorio y anadir a MRU
	if (bResult) {
		// ÉCÉjÉVÉÉÉãÉfÉBÉåÉNÉgÉäActualizar
		Filepath::SetDefaultDir(szMRU);

		// Anadir a MRU
		GetConfig()->SetMRUFile(3, szMRU);
	}
}

//---------------------------------------------------------------------------
//
//	CD-ROM MRU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCDMRUUI(CCmdUI *pCmdUI)
{
	ASSERT(this);
	ASSERT(m_pSCSI);

	// ÉhÉâÉCÉuÇ™ActivadoÇ≈Ç»ÇØÇŒDesactivado
	if (!m_pSCSI->IsValid(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ÉçÉbÉNÇ≥ÇÍÇƒÇ¢ÇÍÇŒã÷é~
	if (m_pSCSI->IsLocked(FALSE)) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Macro de comando/UI para subventanas
//
//---------------------------------------------------------------------------
#define ON_SUB_WINDOW(id, wndcls)	do { \
									CSubWnd *pWnd = GetView()->SearchSWnd(id); \
									if (pWnd) { pWnd->DestroyWindow(); return; } \
									wndcls *pNewWnd = new wndcls; \
									pNewWnd->Init(GetView()); \
									} while (0)
#define ON_UPDATE_SUB_WINDOW(id)	if (GetView()->SearchSWnd(id)) pCmdUI->SetCheck(1); \
									else pCmdUI->SetCheck(0);

//---------------------------------------------------------------------------
//
//	Log
//
//---------------------------------------------------------------------------
void CFrmWnd::OnLog()
{
	ON_SUB_WINDOW(MAKEID('L', 'O', 'G', 'L'), CLogWnd);
}

//---------------------------------------------------------------------------
//
//	Log UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnLogUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('L', 'O', 'G', 'L'));
}

//---------------------------------------------------------------------------
//
//	ÉXÉPÉWÉÖÅ[Éâ
//
//---------------------------------------------------------------------------
void CFrmWnd::OnScheduler()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'H', 'E'), CSchedulerWnd);
}

//---------------------------------------------------------------------------
//
//	ÉXÉPÉWÉÖÅ[Éâ UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSchedulerUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'H', 'E'));
}

//---------------------------------------------------------------------------
//
//	Dispositivo
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDevice()
{
	ON_SUB_WINDOW(MAKEID('D', 'E', 'V', 'I'), CDeviceWnd);
}

//---------------------------------------------------------------------------
//
//	Dispositivo UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDeviceUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('D', 'E', 'V', 'I'));
}

//---------------------------------------------------------------------------
//
//	Registros de CPU
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCPUReg()
{
	ON_SUB_WINDOW(MAKEID('M', 'P', 'U', 'R'), CCPURegWnd);
}

//---------------------------------------------------------------------------
//
//	Registros de CPU UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCPURegUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'P', 'U', 'R'));
}

//---------------------------------------------------------------------------
//
//	äÑÇËçûÇ›
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInt()
{
	ON_SUB_WINDOW(MAKEID('I', 'N', 'T', ' '), CIntWnd);
}

//---------------------------------------------------------------------------
//
//	äÑÇËçûÇ› UI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIntUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('I', 'N', 'T', ' '));
}

//---------------------------------------------------------------------------
//
//	Desensamblar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDisasm()
{
	CDisasmWnd *pWnd;
	int i;
	BOOL flag;

	// Comprobar los 8 tipos
	flag = FALSE;
	for (i=0; i<8; i++) {
		pWnd = (CDisasmWnd*)GetView()->SearchSWnd(MAKEID('D', 'I', 'S', 'A' + i));
		if (pWnd) {
			pWnd->PostMessage(WM_CLOSE, 0, 0);
			flag = TRUE;
		}
	}

	// Creacion nueva
	if (!flag) {
		pWnd = new CDisasmWnd(0);
		VERIFY(pWnd->Init(GetView()));
	}
}

//---------------------------------------------------------------------------
//
//	UI de desensamblar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDisasmUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;
	int i;

	// Comprobar los 8 tipos
	for (i=0; i<8; i++) {
		pSubWnd = GetView()->SearchSWnd(MAKEID('D', 'I', 'S', 'A' + i));
		if (pSubWnd) {
			pCmdUI->SetCheck(1);
			return;
		}
	}

	pCmdUI->SetCheck(0);
}

//---------------------------------------------------------------------------
//
//	ÉÅÉÇÉä
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMemory()
{
	CMemoryWnd *pWnd;
	int i;
	BOOL flag;

	// Comprobar los 8 tipos
	flag = FALSE;
	for (i=0; i<8; i++) {
		pWnd = (CMemoryWnd*)GetView()->SearchSWnd(MAKEID('M', 'E', 'M', 'A' + i));
		if (pWnd) {
			pWnd->PostMessage(WM_CLOSE, 0, 0);
			flag = TRUE;
		}
	}

	// Creacion nueva
	if (!flag) {
		pWnd = new CMemoryWnd(0);
		VERIFY(pWnd->Init(GetView()));
	}
}

//---------------------------------------------------------------------------
//
//	UI de memoria
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMemoryUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;
	int i;

	// Comprobar los 8 tipos
	for (i=0; i<8; i++) {
		pSubWnd = GetView()->SearchSWnd(MAKEID('M', 'E', 'M', 'A' + i));
		if (pSubWnd) {
			pCmdUI->SetCheck(1);
			return;
		}
	}

	pCmdUI->SetCheck(0);
}

//---------------------------------------------------------------------------
//
//	Punto de interrupcion (Breakpoint)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakP()
{
	ON_SUB_WINDOW(MAKEID('B', 'R', 'K', 'P'), CBreakPWnd);
}

//---------------------------------------------------------------------------
//
//	UI de puntos de interrupcion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakPUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'R', 'K', 'P'));
}

//---------------------------------------------------------------------------
//
//	MFP
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMFP()
{
	ON_SUB_WINDOW(MAKEID('M', 'F', 'P', ' '), CMFPWnd);
}

//---------------------------------------------------------------------------
//
//	UI de MFP
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMFPUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'F', 'P', ' '));
}

//---------------------------------------------------------------------------
//
//	DMAC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDMAC()
{
	ON_SUB_WINDOW(MAKEID('D', 'M', 'A', 'C'), CDMACWnd);
}

//---------------------------------------------------------------------------
//
//	UI de DMAC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnDMACUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('D', 'M', 'A', 'C'));
}

//---------------------------------------------------------------------------
//
//	CRTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCRTC()
{
	ON_SUB_WINDOW(MAKEID('C', 'R', 'T', 'C'), CCRTCWnd);
}

//---------------------------------------------------------------------------
//
//	UI de CRTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCRTCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'R', 'T', 'C'));
}

//---------------------------------------------------------------------------
//
//	VC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnVC()
{
	ON_SUB_WINDOW(MAKEID('V', 'C', ' ', ' '), CVCWnd);
}

//---------------------------------------------------------------------------
//
//	UI de VC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnVCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('V', 'C', ' ', ' '));
}

//---------------------------------------------------------------------------
//
//	RTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRTC()
{
	ON_SUB_WINDOW(MAKEID('R', 'T', 'C', ' '), CRTCWnd);
}

//---------------------------------------------------------------------------
//
//	UI de RTC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRTCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('R', 'T', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	OPM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOPM()
{
	ON_SUB_WINDOW(MAKEID('O', 'P', 'M', ' '), COPMWnd);
}

//---------------------------------------------------------------------------
//
//	UI de OPM
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOPMUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('O', 'P', 'M', ' '));
}

//---------------------------------------------------------------------------
//
//	ÉLÅ[É{Å[Éh
//
//---------------------------------------------------------------------------
void CFrmWnd::OnKeyboard()
{
	ON_SUB_WINDOW(MAKEID('K', 'E', 'Y', 'B'), CKeyboardWnd);
}

//---------------------------------------------------------------------------
//
//	UI de teclado
//
//---------------------------------------------------------------------------
void CFrmWnd::OnKeyboardUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('K', 'E', 'Y', 'B'));
}

//---------------------------------------------------------------------------
//
//	FDD
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDD()
{
	ON_SUB_WINDOW(MAKEID('F', 'D', 'D', ' '), CFDDWnd);
}

//---------------------------------------------------------------------------
//
//	UI de FDD
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDDUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('F', 'D', 'D', ' '));
}

//---------------------------------------------------------------------------
//
//	FDC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDC()
{
	ON_SUB_WINDOW(MAKEID('F', 'D', 'C', ' '), CFDCWnd);
}

//---------------------------------------------------------------------------
//
//	UI de FDC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFDCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('F', 'D', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	SCC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCC()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'C', ' '), CSCCWnd);
}

//---------------------------------------------------------------------------
//
//	UI de SCC
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCCUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'C', ' '));
}

//---------------------------------------------------------------------------
//
//	CYNTHIA
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCynthia()
{
	ON_SUB_WINDOW(MAKEID('C', 'Y', 'N', 'T'), CCynthiaWnd);
}

//---------------------------------------------------------------------------
//
//	UI de CYNTHIA
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCynthiaUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'Y', 'N', 'T'));
}

//---------------------------------------------------------------------------
//
//	SASI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSASI()
{
	ON_SUB_WINDOW(MAKEID('S', 'A', 'S', 'I'), CSASIWnd);
}

//---------------------------------------------------------------------------
//
//	UI de SASI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSASIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'A', 'S', 'I'));
}

//---------------------------------------------------------------------------
//
//	MIDI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDI()
{
	ON_SUB_WINDOW(MAKEID('M', 'I', 'D', 'I'), CMIDIWnd);
}

//---------------------------------------------------------------------------
//
//	UI de MIDI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'I', 'D', 'I'));
}

//---------------------------------------------------------------------------
//
//	SCSI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCSI()
{
	ON_SUB_WINDOW(MAKEID('S', 'C', 'S', 'I'), CSCSIWnd);
}

//---------------------------------------------------------------------------
//
//	UI de SCSI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSCSIUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'C', 'S', 'I'));
}

//---------------------------------------------------------------------------
//
//	Pantalla de texto
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTVRAM()
{
	ON_SUB_WINDOW(MAKEID('T', 'V', 'R', 'M'), CTVRAMWnd);
}

//---------------------------------------------------------------------------
//
//	UI de pantalla de texto
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTVRAMUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('T', 'V', 'R', 'M'));
}

//---------------------------------------------------------------------------
//
//	Pantalla grafica 1024x1024
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG1024()
{
	ON_SUB_WINDOW(MAKEID('G', '1', '0', '2'), CG1024Wnd);
}

//---------------------------------------------------------------------------
//
//	UI de pantalla grafica 1024x1024
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG1024UI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('G', '1', '0', '2'));
}

//---------------------------------------------------------------------------
//
//	Pantalla grafica 16 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG16(UINT uID)
{
	CG16Wnd *pWnd;
	int index;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(uID - IDM_G16P0);
	ASSERT((index >= 0) || (index <= 3));

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CG16Wnd*)GetView()->SearchSWnd(MAKEID('G', '1', '6', ('A' + index)));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CG16Wnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de pantalla grafica 16 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG16UI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(pCmdUI->m_nID - IDM_G16P0);
	ASSERT((index >= 0) || (index <= 3));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', '1', '6', ('A' + index)));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Pantalla grafica 256 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG256(UINT uID)
{
	CG256Wnd *pWnd;
	int index;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(uID - IDM_G256P0);
	ASSERT((index == 0) || (index == 1));

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CG256Wnd*)GetView()->SearchSWnd(MAKEID('G', '2', '5', ('A' + index)));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CG256Wnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de pantalla grafica 256 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG256UI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(pCmdUI->m_nID - IDM_G256P0);
	ASSERT((index == 0) || (index == 1));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', '2', '5', ('A' + index)));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Pantalla grafica 65536 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG64K()
{
	CG64KWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CG64KWnd*)GetView()->SearchSWnd(MAKEID('G', '6', '4', 'K'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CG64KWnd;
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de pantalla grafica 65536 colores
//
//---------------------------------------------------------------------------
void CFrmWnd::OnG64KUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('G', '6', '4', 'K'));
}

//---------------------------------------------------------------------------
//
//	PCG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCG()
{
	CPCGWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CPCGWnd*)GetView()->SearchSWnd(MAKEID('P', 'C', 'G', ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CPCGWnd;
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de PCG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'C', 'G', ' '));
}

//---------------------------------------------------------------------------
//
//	Pantalla BG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBG(UINT uID)
{
	CBGWnd *pWnd;
	int index;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(uID - IDM_BG0);
	ASSERT((index == 0) || (index == 1));

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CBGWnd*)GetView()->SearchSWnd(MAKEID('B', 'G', ('0' + index), ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CBGWnd(index);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de pantalla BG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGUI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(pCmdUI->m_nID - IDM_BG0);
	ASSERT((index == 0) || (index == 1));

	pSubWnd = GetView()->SearchSWnd(MAKEID('B', 'G', ('0' + index), ' '));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Paleta
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPalet()
{
	CPaletteWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CPaletteWnd*)GetView()->SearchSWnd(MAKEID('P', 'A', 'L', ' '));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CPaletteWnd(FALSE);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de paleta
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'A', 'L', ' '));
}

//---------------------------------------------------------------------------
//
//	Buffer de texto
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTextBuf()
{
	CRendBufWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('T', 'E', 'X', 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CRendBufWnd(0);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de buffer de texto
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTextBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('T', 'E', 'X', 'B'));
}

//---------------------------------------------------------------------------
//
//	Buffer grafico
//
//---------------------------------------------------------------------------
void CFrmWnd::OnGrpBuf(UINT uID)
{
	CRendBufWnd *pWnd;
	int index;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(uID - IDM_REND_GP0);
	ASSERT((index >= 0) || (index <= 4));

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('G', 'P', ('0' + index), 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CRendBufWnd(index + 1);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de buffer grafico
//
//---------------------------------------------------------------------------
void CFrmWnd::OnGrpBufUI(CCmdUI *pCmdUI)
{
	int index;
	CSubWnd *pSubWnd;

	// ÉCÉìÉfÉbÉNÉXAceptar
	index = (int)(pCmdUI->m_nID - IDM_REND_GP0);
	ASSERT((index >= 0) || (index <= 4));

	pSubWnd = GetView()->SearchSWnd(MAKEID('G', 'P', ('0' + index), 'B'));
	if (pSubWnd) {
		pCmdUI->SetCheck(1);
	}
	else {
		pCmdUI->SetCheck(0);
	}
}

//---------------------------------------------------------------------------
//
//	Buffer PCG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGBuf()
{
	ON_SUB_WINDOW(MAKEID('P', 'C', 'G', 'B'), CPCGBufWnd);
}

//---------------------------------------------------------------------------
//
//	UI de buffer PCG
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPCGBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'C', 'G', 'B'));
}

//---------------------------------------------------------------------------
//
//	Buffer BG/Sprite
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGSpBuf()
{
	CRendBufWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CRendBufWnd*)GetView()->SearchSWnd(MAKEID('B', 'G', 'S', 'P'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CRendBufWnd(5);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de buffer BG/Sprite
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBGSpBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'G', 'S', 'P'));
}

//---------------------------------------------------------------------------
//
//	Buffer de paleta
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletBuf()
{
	CPaletteWnd *pWnd;

	// ë∂ç›Ç∑ÇÍÇŒè¡ãé
	pWnd = (CPaletteWnd*)GetView()->SearchSWnd(MAKEID('P', 'A', 'L', 'B'));
	if (pWnd) {
		pWnd->PostMessage(WM_CLOSE, 0, 0);
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉECrear
	pWnd = new CPaletteWnd(TRUE);
	VERIFY(pWnd->Init(GetView()));
}

//---------------------------------------------------------------------------
//
//	UI de buffer de paleta
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPaletBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'A', 'L', 'B'));
}

//---------------------------------------------------------------------------
//
//	Buffer de composicion (Mix)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMixBuf()
{
	ON_SUB_WINDOW(MAKEID('M', 'I', 'X', 'B'), CMixBufWnd);
}

//---------------------------------------------------------------------------
//
//	UI de buffer de composicion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMixBufUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'I', 'X', 'B'));
}

//---------------------------------------------------------------------------
//
//	Componente
//
//---------------------------------------------------------------------------
void CFrmWnd::OnComponent()
{
	ON_SUB_WINDOW(MAKEID('C', 'O', 'M', 'P'), CComponentWnd);
}

//---------------------------------------------------------------------------
//
//	UI de componente
//
//---------------------------------------------------------------------------
void CFrmWnd::OnComponentUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('C', 'O', 'M', 'P'));
}

//---------------------------------------------------------------------------
//
//	Informacion de OS
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOSInfo()
{
	ON_SUB_WINDOW(MAKEID('O', 'S', 'I', 'N'), COSInfoWnd);
}

//---------------------------------------------------------------------------
//
//	UI de informacion de OS
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOSInfoUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('O', 'S', 'I', 'N'));
}

//---------------------------------------------------------------------------
//
//	ÉTÉEÉìÉh
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSound()
{
	ON_SUB_WINDOW(MAKEID('S', 'N', 'D', ' '), CSoundWnd);
}

//---------------------------------------------------------------------------
//
//	UI de sonido
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoundUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'N', 'D', ' '));
}

//---------------------------------------------------------------------------
//
//	Entrada (Input)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInput()
{
	ON_SUB_WINDOW(MAKEID('I', 'N', 'P', ' '), CInputWnd);
}

//---------------------------------------------------------------------------
//
//	UI de entrada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnInputUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('I', 'N', 'P', ' '));
}

//---------------------------------------------------------------------------
//
//	Puertos
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPort()
{
	ON_SUB_WINDOW(MAKEID('P', 'O', 'R', 'T'), CPortWnd);
}

//---------------------------------------------------------------------------
//
//	UI de puertos
//
//---------------------------------------------------------------------------
void CFrmWnd::OnPortUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('P', 'O', 'R', 'T'));
}

//---------------------------------------------------------------------------
//
//	Bitmap
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBitmap()
{
	ON_SUB_WINDOW(MAKEID('B', 'M', 'A', 'P'), CBitmapWnd);
}

//---------------------------------------------------------------------------
//
//	UI de bitmap
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBitmapUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('B', 'M', 'A', 'P'));
}

//---------------------------------------------------------------------------
//
//	Controlador (Driver) MIDI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIDrv()
{
	ON_SUB_WINDOW(MAKEID('M', 'D', 'R', 'V'), CMIDIDrvWnd);
}

//---------------------------------------------------------------------------
//
//	UI de controlador MIDI
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMIDIDrvUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('M', 'D', 'R', 'V'));
}

//---------------------------------------------------------------------------
//
//	Titulo de ventana
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCaption()
{
	// ÉtÉâÉOÇîΩì]
	m_bCaption = !m_bCaption;

	// èàóùÇÕShowCaptionÇ…îCÇπÇÈ
	ShowCaption();
}

//---------------------------------------------------------------------------
//
//	UI de titulo de ventana
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCaptionUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bCaption);
}

//---------------------------------------------------------------------------
//
//	Barra de menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMenu()
{
	// ÉtÉâÉOÇîΩì]
	m_bMenuBar = !m_bMenuBar;

	// èàóùÇÕShowMenuÇ…îCÇπÇÈ
	ShowMenu();
}

//---------------------------------------------------------------------------
//
//	UI de barra de menu
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMenuUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bMenuBar);
}

//---------------------------------------------------------------------------
//
//	Barra de estado
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStatus()
{
	// ÉtÉâÉOÇîΩì]
	m_bStatusBar = !m_bStatusBar;

	// èàóùÇÕShowStatusÇ…îCÇπÇÈ
	ShowStatus();
}

//---------------------------------------------------------------------------
//
//	UI de barra de estado
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStatusUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bStatusBar);
}

//---------------------------------------------------------------------------
//
//	Refrescar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRefresh()
{
	// ÉçÉbÉN
	::LockVM();

	// çƒDibujo
	GetView()->Refresh();

	// ÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Expandir
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStretch()
{
	BOOL bFlag;

	// VMÇÉçÉbÉN
	::LockVM();

	// îΩì]
	bFlag = GetView()->IsStretch();
	GetView()->Stretch(!bFlag);

	// ÉRÉìÉtÉBÉOÇïœÇ¶ÇÈ
	GetConfig()->SetStretch(!bFlag);

	// VMÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	UI de expansion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnStretchUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(GetView()->IsStretch());
}

//---------------------------------------------------------------------------
//
//	Evento de pantalla completa
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFullScreen()
{
	HDC hDC;
	DEVMODE devmode;
	BOOL bEnable;
	BOOL bSound;
	BOOL bMouse;
	BOOL bChanged;
	CRect rectWnd;

	// ïœçXÇFalloÇ…Inicializacion
	bChanged = FALSE;

	// É}ÉEÉXOFFÅAÉXÉPÉWÉÖÅ[ÉâPararÅAÉTÉEÉìÉhOFF
	bEnable = GetScheduler()->IsEnable();
	bMouse = GetInput()->GetMouseMode();
	if (bMouse) {
		GetInput()->SetMouseMode(FALSE);
	}
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	::LockVM();
	bSound = GetSound()->IsEnable();
	GetSound()->Enable(FALSE);
	::UnlockVM();

	if (m_bFullScreen) {
		// Al modo normal

		// ÉEÉBÉìÉhÉEóvëf
		ModifyStyle(0, WS_CAPTION | WS_SYSMENU | WS_BORDER, 0);
		ModifyStyleEx(0, WS_EX_WINDOWEDGE, 0);
		GetView()->ModifyStyleEx(0, WS_EX_CLIENTEDGE, 0);

		// âÊñ ÉÇÅ[ÉhêÿÇËë÷Ç¶(1)
		m_DevMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY;
		if (::ChangeDisplaySettings(&m_DevMode, 0) == DISP_CHANGE_SUCCESSFUL) {
			bChanged = TRUE;
		}

		// FalloÇµÇΩÇÁÅAFREQéwíËÇäOÇµÇƒÅAçƒìxééÇ›ÇÈ
		if (!bChanged) {
			// âÊñ ÉÇÅ[ÉhêÿÇËë÷Ç¶(2)
			m_DevMode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
			if (::ChangeDisplaySettings(&m_DevMode, 0) == DISP_CHANGE_SUCCESSFUL) {
				bChanged = TRUE;
			}
		}

		// ExitoÇµÇΩèÍçá
		if (bChanged) {
			// Si se puso en pantalla completa estando maximizado, restaurarlo
			if (IsZoomed()) {
				ShowWindow(SW_RESTORE);
			}

			// Titulo de ventanaÅAMenuÅABarra de estado
			m_bFullScreen = FALSE;
			ShowCaption();
			ShowMenu();
			ShowStatus();

			// çƒäJèàóù
			HideTaskBar(FALSE, TRUE);

			InitPos(FALSE);
			RecalcLayout();
			GetScheduler()->Enable(bEnable);
			GetSound()->Enable(bSound);
			GetInput()->SetMouseMode(bMouse);
			ResetCaption();
			ResetStatus();

			// é©ìÆÉ}ÉEÉXÉÇÅ[Éh
			if (m_bAutoMouse && bMouse) {
				OnMouseMode();
			}
			return;
		}

		// Se intento cambiar al modo normal, pero no se pudo
		ModifyStyle(WS_CAPTION | WS_SYSMENU | WS_BORDER, 0, 0);
		ModifyStyleEx(WS_EX_WINDOWEDGE, 0, 0);
		GetView()->ModifyStyleEx(WS_EX_CLIENTEDGE, 0, 0);

		// çƒäJèàóù
		GetScheduler()->Enable(bEnable);
		GetSound()->Enable(bSound);
		GetInput()->SetMouseMode(bMouse);
		ResetCaption();
		ResetStatus();
		return;
	}

	// Al pasar a pantalla completa, guardar la posicion actual de la ventana
	GetWindowRect(&rectWnd);
	m_nWndLeft = rectWnd.left;
	m_nWndTop = rectWnd.top;

	


	// ÉEÉBÉìÉhÉEóvëf
	ModifyStyle(WS_CAPTION | WS_SYSMENU | WS_BORDER, 0, 0);
	ModifyStyleEx(WS_EX_WINDOWEDGE, 0, 0);
	GetView()->ModifyStyleEx(WS_EX_CLIENTEDGE, 0, 0);

	// åªç›ÇÃâÊñ ÉÇÅ[ÉhÇObtener
	hDC = ::GetDC(NULL);
	ASSERT(hDC);
	memset(&m_DevMode, 0, sizeof(m_DevMode));
	m_DevMode.dmSize = sizeof(DEVMODE);
	m_DevMode.dmPelsWidth = ::GetDeviceCaps(hDC, HORZRES);
	m_DevMode.dmPelsHeight = ::GetDeviceCaps(hDC, VERTRES);
	m_DevMode.dmBitsPerPel = ::GetDeviceCaps(hDC, BITSPIXEL);
	m_DevMode.dmDisplayFrequency = ::GetDeviceCaps(hDC, VREFRESH);
	::ReleaseDC(NULL, hDC);

	// âÊñ ÉÇÅ[ÉhêÿÇËë÷Ç¶
	memset(&devmode, 0, sizeof(devmode));
	devmode.dmSize = sizeof(DEVMODE);

	/* ACA SE MODIFICA LA RESOLUCION DE PANTALLA COMPLETA*/
/*	int rcx, rcy;
	GetDesktopResolution(rcx, rcy);*/

	//::SetProcessDPIAware();
	int	rcx = ::GetSystemMetrics(SM_CXSCREEN);
    int rcy = ::GetSystemMetrics(SM_CYSCREEN);


	/*char cadena[20],cadena2[20];	  
    sprintf(cadena, "%d", rcx);
	sprintf(cadena2, "%d", rcy);
	 int msgboxID = MessageBox(
       cadena,"Width",
        2 );
	 int msgboxID2 = MessageBox(
       cadena2,"Height",
        2 );*/
		 

	devmode.dmBitsPerPel = 32;
	devmode.dmPelsWidth = rcx;
	devmode.dmPelsHeight = rcy;
	devmode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
	if (::ChangeDisplaySettings(&devmode, CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL) {
		// Titulo de ventanaÅAMenuÅABarra de estado
		m_bFullScreen = TRUE;
		ShowCaption();
		ShowMenu();


		/* No mostrar barra de estado en fullScreen */
		//ShowStatus();

		// çƒäJèàóù
		HideTaskBar(TRUE, TRUE);

		InitPos(FALSE);
		RecalcStatusView();
		GetScheduler()->Enable(bEnable);
		GetSound()->Enable(bSound);
		GetInput()->SetMouseMode(bMouse);
		ResetCaption();
		ResetStatus();

		// é©ìÆÉ}ÉEÉXÉÇÅ[Éh
		if (m_bAutoMouse && bEnable && !bMouse) {
			OnMouseMode();
		}
		//RestoreFrameWnd(FALSE);


/*
		// Mensajes de top y bottom
	char cadena[20],cadena2[20];	  
    sprintf(cadena, "%d", rectWnd.top);
	sprintf(cadena2, "%d", rectWnd.bottom);
	 int msgboxID = MessageBox(
       cadena,"Height",
        2 );
	 int msgboxID2 = MessageBox(
       cadena2,"Width",
        2 );
		*/

		return;
	}

	// Se intento cambiar a pantalla completa, pero no se pudo
	ModifyStyle(0, WS_CAPTION | WS_SYSMENU | WS_BORDER, 0);
	ModifyStyleEx(0, WS_EX_WINDOWEDGE, 0);
	
	GetView()->ModifyStyleEx(0, WS_EX_CLIENTEDGE, 0);
	//GetView()->SetupBitmap();
	// çƒäJèàóù
	GetScheduler()->Enable(bEnable);
	GetSound()->Enable(bSound);
	GetInput()->SetMouseMode(bMouse);
	ResetCaption();
	ResetStatus();	


}


//---------------------------------------------------------------------------
//
//	UI de pantalla completa
//
//---------------------------------------------------------------------------
void CFrmWnd::OnFullScreenUI(CCmdUI *pCmdUI)
{
	pCmdUI->SetCheck(m_bFullScreen);
}

//---------------------------------------------------------------------------
//
//	Ejecutar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExec()
{
	// ÉçÉbÉN
	::LockVM();

	// ÉXÉPÉWÉÖÅ[ÉâÇActivadoâª
	GetScheduler()->Reset();
	GetScheduler()->Enable(TRUE);
	ResetCaption();

	// ÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	UI de ejecucion
//
//---------------------------------------------------------------------------
void CFrmWnd::OnExecUI(CCmdUI *pCmdUI)
{
	// ÉXÉPÉWÉÖÅ[ÉâÇ™PararíÜÇ»ÇÁActivado
	if ((!GetScheduler()->IsEnable()) && ::GetVM()->IsPower()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Parar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreak()
{
	// ÉçÉbÉN
	::LockVM();

	// ÉXÉPÉWÉÖÅ[ÉâÇDesactivadoâª
	GetScheduler()->Enable(FALSE);

	// ÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	UI de parada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnBreakUI(CCmdUI *pCmdUI)
{
	// ÉXÉPÉWÉÖÅ[ÉâÇ™ìÆçÏíÜÇ»ÇÁActivado
	if (GetScheduler()->IsEnable()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Rastro (Trace)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrace()
{
	// ÉçÉbÉN
	::LockVM();

	// Rastro (Trace)
	::GetVM()->Trace();
	GetScheduler()->SyncDisasm();

	// ÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	UI de rastro
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTraceUI(CCmdUI *pCmdUI)
{
	// ÉXÉPÉWÉÖÅ[ÉâÇ™PararíÜÇ»ÇÁActivado
	if ((!GetScheduler()->IsEnable()) && ::GetVM()->IsPower()) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	É}ÉEÉXÉÇÅ[Éh
//
//---------------------------------------------------------------------------
void CFrmWnd::OnMouseMode()
{
	CRect rect;
	BOOL b;
	int cnt;
	CString string;
	LONG cx;
	LONG cy;

	// åªç›ÇÃÉÇÅ[ÉhÇå©ÇÈ
	b = GetInput()->GetMouseMode();

	// Durante la minimizacion y con el planificador apagado, solo funciona el apagado
	if (!b) {
		// No permitir encender si esta minimizado
		if (IsIconic()) {
			return;
		}
		// No permitir encender si el planificador esta apagado
		if (!GetScheduler()->IsEnable()) {
			return;
		}
		// No permitir encender si no esta activo
		if (!GetInput()->IsActive()) {
			return;
		}
	}

	if (b) {
		// Hacia modo raton desactivado
		GetInput()->SetMouseMode(FALSE);

		// Deshacer clip (liberar raton)
		ClipCursor(NULL);

		// Mover cursor del raton al centro de la pantalla
		cx = ::GetSystemMetrics(SM_CXSCREEN);
		cy = ::GetSystemMetrics(SM_CYSCREEN);
		SetCursorPos(cx >> 1, cy >> 1);

		// Cursor del raton activado
		cnt = ::ShowCursor(TRUE);
		while (cnt < 0) {
			cnt = ::ShowCursor(TRUE);
		}

		// ÉÅÉbÉZÅ[ÉW
		::GetMsg(IDS_MOUSE_WIN, string);
	}
	else {
		// É}ÉEÉXÉJÅ[É\ÉãÇÉNÉäÉbÉvÅBViewÉEÉBÉìÉhÉEÇÃ+16ÇÃÇ›
		GetView()->GetWindowRect(&rect);
		rect.right = rect.left + 16;
		rect.bottom = rect.top + 16;
		ClipCursor(&rect);

		// É}ÉEÉXÉJÅ[É\ÉãOFF
		cnt = ::ShowCursor(FALSE);
		while (cnt >= 0) {
			cnt = ::ShowCursor(FALSE);
		}

		// Hacia modo raton activado
		GetInput()->SetMouseMode(TRUE);

		// ÉÅÉbÉZÅ[ÉW
		::GetMsg(IDS_MOUSE_X68K, string);
	}

	// ÉÅÉbÉZÅ[ÉWÉZÉbÉg
	SetInfo(string);
}

//---------------------------------------------------------------------------
//
//	Teclado por software
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoftKey()
{
	ON_SUB_WINDOW(MAKEID('S', 'K', 'E', 'Y'), CSoftKeyWnd);
}

//---------------------------------------------------------------------------
//
//	É\ÉtÉgÉEÉFÉAUI de teclado
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSoftKeyUI(CCmdUI *pCmdUI)
{
	ON_UPDATE_SUB_WINDOW(MAKEID('S', 'K', 'E', 'Y'));

	// ÉLÅ[É{Å[ÉhDispositivoÇ™ë∂ç›ÇµÇ»ÇØÇÍÇŒÅANo hacer nada
	if (!m_pKeyboard) {
		return;
	}

	// ÉTÉuÉEÉBÉìÉhÉEë∂ç›ÇπÇ∏
	if (GetView()->SearchSWnd(MAKEID('S', 'K', 'E', 'Y')) == NULL) {
		// ÉLÅ[É{Å[ÉhÇ™ê⁄ë±Ç≥ÇÍÇƒÇ¢Ç»ÇØÇÍÇŒ
		if (!m_pKeyboard->IsConnect()) {
			// ã÷é~
			pCmdUI->Enable(FALSE);
			return;
		}
	}

	// Activado
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Ajuste de hora
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTimeAdj()
{
	RTC *pRTC;

	// pRTC->Adjust()Ç≈çáÇÌÇπÇÈ
	::LockVM();
	pRTC = (RTC*)::GetVM()->SearchDevice(MAKEID('R', 'T', 'C', ' '));
	ASSERT(pRTC);
	pRTC->Adjust(FALSE);
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Captura WAV
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveWav()
{
	TCHAR szPath[_MAX_PATH];
	CString strMsg;
	BOOL bResult;

	// Detener si se esta guardando WAV
	if (GetSound()->IsSaveWav()) {
		GetSound()->EndSaveWav();
		::GetMsg(IDS_WAVSTOP, strMsg);
		SetInfo(strMsg);
		return;
	}

	// ÉtÉ@ÉCÉãëIë
	szPath[0] = _T('\0');
	if (!FileSaveDlg(this, szPath, _T("wav"), IDS_WAVOPEN)) {
		ResetCaption();
		return;
	}

	// Intentar guardado WAV
	::LockVM();
	bResult = GetSound()->StartSaveWav(szPath);
	::UnlockVM();

	// åãâ ï]âø
	if (bResult) {
		::GetMsg(IDS_WAVSTART, strMsg);
		SetInfo(strMsg);
	}
	else {
		::GetMsg(IDS_CREATEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
	}
}

//---------------------------------------------------------------------------
//
//	UI de captura WAV
//
//---------------------------------------------------------------------------
void CFrmWnd::OnSaveWavUI(CCmdUI *pCmdUI)
{
	pCmdUI->Enable(GetSound()->IsPlay());
	pCmdUI->SetCheck(GetSound()->IsSaveWav());
}

//---------------------------------------------------------------------------
//
//	trap#0
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrap()
{
	CTrapDlg dlg(this);
	DWORD dwVector;
	MFP *pMFP;
	Memory *pMemory;
	DWORD dwAddr;
	DWORD dwCode;

	// DispositivoObtener
	pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(pMFP);
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);

	// ÉçÉbÉN
	::LockVM();

	// MFPÇÊÇËÅAÉxÉNÉ^ÇObtener
	dwVector = (pMFP->GetVR() & 0xf0) + 5;
	dwVector <<= 2;

	// Obtener la direccion actual del vector Timer-C
	dwAddr = pMemory->ReadOnly(dwVector);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 1);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 2);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 3);

	// Invalido si no esta inicializado (24 bits o mas)
	if (dwAddr > 0xffffff) {
		::UnlockVM();
		return;
	}

	// Invalido si ya es 0x6800
	if (dwAddr == 0x6800) {
		::UnlockVM();
		return;
	}

	// ÉRÅ[ÉhëIë
	dlg.m_dwCode = pMemory->ReadOnly(0x6809);
	if (pMemory->ReadOnly(0x6808) == 0xff) {
		// Considerado no inicializado, comenzar desde 0
		dlg.m_dwCode = 1;
	}
	::UnlockVM();
	if (dlg.DoModal() != IDOK) {
		return;
	}
	dwCode = dlg.m_dwCode;

	// ÉRÅ[ÉhèëÇ´çûÇ›
	::LockVM();
	pMemory->WriteWord(0x6800, 0x2f00);
	pMemory->WriteWord(0x6802, 0x2f00);
	pMemory->WriteWord(0x6804, 0x203c);
	pMemory->WriteWord(0x6806, dwCode >> 16);
	pMemory->WriteWord(0x6808, (WORD)dwCode);
	pMemory->WriteWord(0x680a, 0x4e40);
	pMemory->WriteWord(0x680c, 0x203c);
	pMemory->WriteWord(0x680e, dwAddr >> 16);
	pMemory->WriteWord(0x6810, (WORD)dwAddr);
	pMemory->WriteWord(0x6812, 0x21c0);
	pMemory->WriteWord(0x6814, dwVector);
	pMemory->WriteWord(0x6816, 0x2f40);
	pMemory->WriteWord(0x6818, 0x0004);
	pMemory->WriteWord(0x681a, 0x201f);
	pMemory->WriteWord(0x681c, 0x4e75);

	// Timer-CÉxÉNÉ^ïœçX
	pMemory->WriteWord(dwVector, 0x0000);
	pMemory->WriteWord(dwVector + 2, 0x6800);

	// ÉAÉìÉçÉbÉN
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	UI de trap#0
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTrapUI(CCmdUI *pCmdUI)
{
	DWORD dwVector;
	MFP *pMFP;
	Memory *pMemory;
	DWORD dwAddr;

	// DispositivoObtener
	pMFP = (MFP*)::GetVM()->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(pMFP);
	pMemory = (Memory*)::GetVM()->SearchDevice(MAKEID('M', 'E', 'M', ' '));
	ASSERT(pMemory);

	// MFPÇÊÇËÅAÉxÉNÉ^ÇObtener
	dwVector = (pMFP->GetVR() & 0xf0) + 5;
	dwVector <<= 2;

	// Obtener la direccion actual del vector Timer-C
	dwAddr = pMemory->ReadOnly(dwVector);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 1);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 2);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(dwVector + 3);

	// Prohibir si no esta inicializado (24 bits o mas)
	if (dwAddr > 0xffffff) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// Prohibir si ya es 0x6800
	if (dwAddr == 0x6800) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// trap #0ÉxÉNÉ^ÇObtener
	dwAddr = pMemory->ReadOnly(0x0080);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0081);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0082);
	dwAddr <<= 8;
	dwAddr |= pMemory->ReadOnly(0x0083);

	// Prohibir si no esta inicializado (24 bits o mas)
	if (dwAddr > 0xffffff) {
		pCmdUI->Enable(FALSE);
		return;
	}

	// ãñâ¬
	pCmdUI->Enable(TRUE);
}

//---------------------------------------------------------------------------
//
//	Nuevo disco floppy
//
//---------------------------------------------------------------------------
void CFrmWnd::OnNewFD()
{
	CFDIDlg dlg(this);
	FDIDisk *pDisk;
	FDIDisk::option_t opt;
	CString strMsg;
	BOOL bRun;
	Filepath path;

	// Ejecutar dialogo
	if (dlg.DoModal() != IDOK) {
		return;
	}

	// ÉpÉXCrear
	path.SetPath(dlg.m_szFileName);

	// ÉIÉvÉVÉáÉìç\ë¢ëÃCrear
	opt.phyfmt = dlg.m_dwPhysical;
	opt.logfmt = dlg.m_bLogical;
	ASSERT(_tcslen(dlg.m_szDiskName) < 60);
	_tcscpy(opt.name, dlg.m_szDiskName);

	// Crear imagen segun el tipo
	switch (dlg.m_dwType) {
		case 0:
			pDisk = (FDIDisk*)new FDIDisk2HD(0, NULL);
			break;
		case 1:
			pDisk = (FDIDisk*)new FDIDiskDIM(0, NULL);
			break;
		case 2:
			pDisk = (FDIDisk*)new FDIDiskD68(0, NULL);
			break;
		case 3:
			pDisk = (FDIDisk*)new FDIDisk2DD(0, NULL);
			break;
		case 4:
			pDisk = (FDIDisk*)new FDIDisk2HQ(0, NULL);
			break;
		default:
			ASSERT(FALSE);
			return;
	}

	// Formato (incluye fisico, logico y guardado)
	// Detener planificador durante el formato
	AfxGetApp()->BeginWaitCursor();
	bRun = GetScheduler()->IsEnable();
	GetScheduler()->Enable(FALSE);
	::LockVM();
	::UnlockVM();
	if (!pDisk->Create(path, &opt)) {
		AfxGetApp()->EndWaitCursor();
		// àÍìxEliminar
		delete pDisk;

		// ÉÅÉbÉZÅ[ÉWÉ{ÉbÉNÉX
		::GetMsg(IDS_CREATEERR, strMsg);
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();
		return;
	}
	AfxGetApp()->EndWaitCursor();

	// àÍìxEliminar
	delete pDisk;

	// Automontaje (opcional)
	if (dlg.m_nDrive >= 0) {
		InitCmdSub(dlg.m_nDrive, dlg.m_szFileName);
	}

	// Mensaje de estado, reanudar
	::GetMsg(IDS_NEWFD, strMsg);
	SetInfo(strMsg);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();
}

//---------------------------------------------------------------------------
//
//	Nuevo disco de gran capacidad
//
//---------------------------------------------------------------------------
void CFrmWnd::OnNewDisk(UINT uID)
{
	CDiskDlg dlg(this);
	CString strMsg;
	BOOL bRun;
	UINT nMsg;
	LPCTSTR lpszPath;
	Filepath path;

	// ÉpÉâÉÅÅ[É^éÛÇØéÊÇË
	ASSERT(this);
	ASSERT((uID >= IDM_NEWSASI) && (uID <= IDM_NEWMO));
	uID -= IDM_NEWSASI;
	ASSERT(uID <= 2);

	// éÌï ÇìnÇ∑
	dlg.m_nType = (int)uID;

	// Obtener estado del planificador (la parada se realiza dentro del dialogo)
	bRun = GetScheduler()->IsEnable();

	// Ejecutar dialogo
	if (dlg.DoModal() != IDOK) {
		// CancelarÇµÇΩèÍçá
		return;
	}

	// EjecutarÇµÇΩèÍçáÇÕÅAåãâ ï]âø
	if (!dlg.IsSucceeded()) {
		if (dlg.IsCanceled()) {
			// CancelarÇµÇΩ
			::GetMsg(IDS_CANCEL, strMsg);
		}
		else {
			// çÏÇÎÇ§Ç∆ÇµÇΩÇ™FalloÇµÇΩ
			::GetMsg(IDS_CREATEERR, strMsg);
		}
		MessageBox(strMsg, NULL, MB_ICONSTOP | MB_OK);
		GetScheduler()->Reset();
		GetScheduler()->Enable(bRun);
		ResetCaption();
		return;
	}

	// Obtener mensaje de informacion
	switch (uID) {
		// SASI-HD
		case 0:
			nMsg = IDS_NEWSASI;
			break;

		// SCSI-HD
		case 1:
			nMsg = IDS_NEWSCSI;
			break;

		// SCSI-MO
		case 2:
			nMsg = IDS_NEWMO;
			break;

		// ÇªÇÃëº(Ç†ÇËìæÇ»Ç¢)
		default:
			ASSERT(FALSE);
			nMsg = 0;
			break;
	}

	// Cargar mensaje
	::GetMsg(nMsg, strMsg);

	// Montaje de MO
	if (uID == 2) {
		// ÉtÉâÉOÉ`ÉFÉbÉN
		if (dlg.m_bMount) {
			// MOActivadoÇ©
			if (m_pSASI->IsValid()) {
				// Obtener nombre de archivo
				lpszPath = dlg.GetPath();

				// AbrirÇ∆MRU
				path.SetPath(lpszPath);
				if (m_pSASI->Open(path)) {
					GetConfig()->SetMRUFile(2, lpszPath);
				}
			}
		}
	}

	// Mensaje de estado, reanudar
	SetInfo(strMsg);
	GetScheduler()->Reset();
	GetScheduler()->Enable(bRun);
	ResetCaption();
}

//---------------------------------------------------------------------------
//
//	ÉIÉvÉVÉáÉì
//
//---------------------------------------------------------------------------
void CFrmWnd::OnOptions()
{
	Config config;
	CConfigSheet sheet(this);

	// Datos de configuracionÇObtener
	GetConfig()->GetConfig(&config);

	// Ejecutar hoja de propiedades
	sheet.m_pConfig = &config;
	if (sheet.DoModal() != IDOK) {
		return;
	}

	// ÉfÅ[É^ì]ëó
	GetConfig()->SetConfig(&config);

	// Aplicar (bajo bloqueo de VM)
	::LockVM();
	ApplyCfg();
	GetScheduler()->Reset();
	ResetCaption();
	::UnlockVM();
}

//---------------------------------------------------------------------------
//
//	Mostrar en cascada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCascade()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::CascadeWindows(GetView()->m_hWnd, 0, NULL, 0, NULL);
}

//---------------------------------------------------------------------------
//
//	UI de mostrar en cascada
//
//---------------------------------------------------------------------------
void CFrmWnd::OnCascadeUI(CCmdUI *pCmdUI)
{
	CMenu *pMenu;
	CMenu *pSubMenu;
	CSubWnd *pWnd;
	CString string;
	CString temp;
	int i;
	int n;
	UINT uID;
	DWORD dwID;

	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Ç±ÇÃMenué©ëÃÇÃèàóù
	if (IsPopupSWnd()) {
		// É|ÉbÉvÉAÉbÉvÇÃèÍçáÇÕà”ñ°Ç™Ç»Ç¢
		pCmdUI->Enable(FALSE);
	}
	else {
		// En caso de ser hijo, es TRUE si hay una o mas subventanas
		if (GetView()->GetSubWndNum() >= 1) {
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}

	// Buscar menu Window
	if (m_bPopupMenu) {
		pMenu = m_PopupMenu.GetSubMenu(0);
	}
	else {
		pMenu = &m_Menu;
	}
	ASSERT(pMenu);
	n = pMenu->GetMenuItemCount();
	pSubMenu = NULL;
	for (i=0; i<n; i++) {
		pSubMenu = pMenu->GetSubMenu(i);
		ASSERT(pSubMenu);
		if (pSubMenu->GetMenuItemID(0) == IDM_CASCADE) {
			break;
		}
	}
	ASSERT(pSubMenu);
	ASSERT(i < n);

	// 6Ç¬ÇécÇµÇƒEliminar
	while (pSubMenu->GetMenuItemCount() > 6) {
		pSubMenu->DeleteMenu(6, MF_BYPOSITION);
	}

	// ActivadoÇ»ÉTÉuÉEÉBÉìÉhÉEÇ™Ç†ÇÈÇ©
	pWnd = GetView()->GetFirstSWnd();
	if (!pWnd) {
		return;
	}

	// Anadir separador
	pSubMenu->AppendMenu(MF_SEPARATOR, 0);

	// MenuÇèáéüí«â¡
	uID = IDM_SWND_START;
	while (pWnd) {
		// Obtener titulo e ID de ventana
		pWnd->GetWindowText(string);
		dwID = pWnd->GetID();
		temp.Format("%c%c%c%c - ",
			(dwID >> 24) & 0xff,
			(dwID >> 16) & 0xff,
			(dwID >> 8) & 0xff,
			dwID & 0xff);
		string = temp + string;

		// Menuí«â¡
		pSubMenu->AppendMenu(MF_STRING, uID, string);

		// éüÇ÷
		pWnd = pWnd->m_pNextWnd;
		uID++;
	}
}

//---------------------------------------------------------------------------
//
//	Mostrar en mosaico (Tile)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTile()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::TileWindows(GetView()->m_hWnd, MDITILE_VERTICAL, NULL, 0, NULL);
}

//---------------------------------------------------------------------------
//
//	UI de mostrar en mosaico
//
//---------------------------------------------------------------------------
void CFrmWnd::OnTileUI(CCmdUI *pCmdUI)
{
	if (IsPopupSWnd()) {
		// É|ÉbÉvÉAÉbÉvÇÃèÍçáÇÕà”ñ°Ç™Ç»Ç¢
		pCmdUI->Enable(FALSE);
	}
	else {
		// En caso de ser hijo, es TRUE si hay una o mas subventanas
		if (GetView()->GetSubWndNum() >= 1) {
			pCmdUI->Enable(TRUE);
		}
		else {
			pCmdUI->Enable(FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Minimizar todo (Iconificar)
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIconic()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Obtener la primera subventana
	pSubWnd = GetView()->GetFirstSWnd();

	// ÉãÅ[Év
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_MINIMIZE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	UI de minimizar todo
//
//---------------------------------------------------------------------------
void CFrmWnd::OnIconicUI(CCmdUI *pCmdUI)
{
	// ÉTÉuÉEÉBÉìÉhÉEÇ™ë∂ç›Ç∑ÇÍÇŒTRUE
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	ÉAÉCÉRÉìÇÃêÆóÒ
//
//---------------------------------------------------------------------------
void CFrmWnd::OnArrangeIcon()
{
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);
	::ArrangeIconicWindows(GetView()->m_hWnd);
}

//---------------------------------------------------------------------------
//
//	UI de organizar iconos
//
//---------------------------------------------------------------------------
void CFrmWnd::OnArrangeIconUI(CCmdUI *pCmdUI)
{
	CSubWnd *pSubWnd;

	if (IsPopupSWnd()) {
		// É|ÉbÉvÉAÉbÉvÇÃèÍçáÇÕà”ñ°Ç™Ç»Ç¢
		pCmdUI->Enable(FALSE);
		return;
	}

	// Activado si hay alguna subventana en estado de icono
	pSubWnd = GetView()->GetFirstSWnd();
	while (pSubWnd) {
		// Esta minimizado?
		if (pSubWnd->IsIconic()) {
			pCmdUI->Enable(TRUE);
			return;
		}

		// éüÇÃÉTÉuÉEÉBÉìÉhÉE
		pSubWnd = pSubWnd->m_pNextWnd;
	}

	// Desactivado porque ninguna esta minimizada
	pCmdUI->Enable(FALSE);
}

//---------------------------------------------------------------------------
//
//	Ocultar todo
//
//---------------------------------------------------------------------------
void CFrmWnd::OnHide()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Obtener la primera subventana
	pSubWnd = GetView()->GetFirstSWnd();

	// ÉãÅ[Év
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_HIDE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	UI de ocultar todo
//
//---------------------------------------------------------------------------
void CFrmWnd::OnHideUI(CCmdUI *pCmdUI)
{
	// ÉTÉuÉEÉBÉìÉhÉEÇ™ë∂ç›Ç∑ÇÍÇŒTRUE
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Restaurar todo
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRestore()
{
	CSubWnd *pSubWnd;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Obtener la primera subventana
	pSubWnd = GetView()->GetFirstSWnd();

	// ÉãÅ[Év
	while (pSubWnd) {
		pSubWnd->ShowWindow(SW_RESTORE);
		pSubWnd = pSubWnd->m_pNextWnd;
	}
}

//---------------------------------------------------------------------------
//
//	ëSÇƒUI de restaurar
//
//---------------------------------------------------------------------------
void CFrmWnd::OnRestoreUI(CCmdUI *pCmdUI)
{
	// ÉTÉuÉEÉBÉìÉhÉEÇ™ë∂ç›Ç∑ÇÍÇŒTRUE
	if (GetView()->GetSubWndNum() > 0) {
		pCmdUI->Enable(TRUE);
	}
	else {
		pCmdUI->Enable(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	Especificar ventana
//
//---------------------------------------------------------------------------
void CFrmWnd::OnWindow(UINT uID)
{
	CSubWnd *pSubWnd;
	int n;

	ASSERT(this);
	ASSERT(GetView());
	ASSERT(GetView()->m_hWnd);

	// Identificar subventana
	n = (int)(uID - IDM_SWND_START);
	pSubWnd = GetView()->GetFirstSWnd();
	if (!pSubWnd) {
		return;
	}

	// BusquedaÉãÅ[Év
	while (n > 0) {
		if (!pSubWnd) {
			return;
		}

		n--;
		pSubWnd = pSubWnd->m_pNextWnd;
	}

	// pSubWndÇÉZÉåÉNÉg
	pSubWnd->ShowWindow(SW_RESTORE);
	pSubWnd->SetWindowPos(&wndTop, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

//---------------------------------------------------------------------------
//
//	Informacion de version
//
//---------------------------------------------------------------------------
void CFrmWnd::OnAbout()
{
	CAboutDlg dlg(this);

	// Ejecutar dialogo modal
	dlg.DoModal();
}

#endif	// _WIN32
