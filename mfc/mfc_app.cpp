//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
//	[ Aplicacion MFC ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "mfc_frm.h"
#include "mfc_asm.h"
#include "mfc_com.h"
#include "mfc_info.h"
#include "mfc_res.h"
#include "mfc_app.h"

//---------------------------------------------------------------------------
//
//	Instancia de la aplicacion
//
//---------------------------------------------------------------------------
CApp theApp;

//---------------------------------------------------------------------------
//
//	Definicion de punteros a funciones
//
//---------------------------------------------------------------------------
extern "C" {
typedef int (WINAPI *DRAWTEXTWIDE)(HDC, LPCWSTR, int, LPRECT, UINT);
}

//---------------------------------------------------------------------------
//
//	Estructuras globales
//
//---------------------------------------------------------------------------
VM *pVM;								// Virtual Machine

//---------------------------------------------------------------------------
//
//	Estructuras estaticas
//
//---------------------------------------------------------------------------
static CCriticalSection csect;			// Seccion critica para el bloqueo de la VM
static BOOL bJapanese;					// Flag de discriminacion Japones/Ingles
static BOOL bWinNT;						// Flag de discriminacion Windows NT/9x
static BOOL bSupport932;				// Flag de soporte para CP932 (SHIFT-JIS)
static BOOL bMMX;						// Flag de discriminacion MMX
static BOOL bCMOV;						// Flag de discriminacion CMOV
static LPSTR lpszInfoMsg;				// Buffer de mensaje de informacion
static DRAWTEXTWIDE pDrawTextW;			// DrawTextW

//---------------------------------------------------------------------------
//
//	Determinacion del entorno japones
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsJapanese(void)
{
	return bJapanese;
}

//---------------------------------------------------------------------------
//
//	Determinacion de Windows NT
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsWinNT(void)
{
	return bWinNT;
}

//---------------------------------------------------------------------------
//
//	Determinacion del soporte de CP932
//
//---------------------------------------------------------------------------
BOOL FASTCALL Support932(void)
{
	return bSupport932;
}

//---------------------------------------------------------------------------
//
//	Determinacion de MMX
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsMMX(void)
{
	return bMMX;
}

//---------------------------------------------------------------------------
//
//	Determinacion de CMOV
//
//---------------------------------------------------------------------------
BOOL FASTCALL IsCMOV(void)
{
	return bCMOV;
}

//---------------------------------------------------------------------------
//
//	Obtencion de mensajes
//
//---------------------------------------------------------------------------
void FASTCALL GetMsg(UINT uID, CString& string)
{
	// uID puede llegar como 0 en algunos casos
	if (uID == 0) {
		string.Empty();
		return;
	}

	// ?Es japones?
	if (IsJapanese()) {
		if (!string.LoadString(uID)) {
#if defined(_DEBUG)
			TRACE(_T("GetMsg: Error al cargar cadena ID:%d
"), uID);
#endif	// _DEBUG
			string.Empty();
		}
		return;
	}

	// Ingles. Probar con +5000
	if (string.LoadString(uID + 5000)) {
		return;
	}

	// Intentar de nuevo con +0
	if (!string.LoadString(uID)) {
#if defined(_DEBUG)
		TRACE(_T("GetMsg: Error al cargar cadena ID:%d
"), uID);
#endif	// _DEBUG
		string.Empty();
	}
}

//---------------------------------------------------------------------------
//
//	Obtener maquina virtual
//
//---------------------------------------------------------------------------
VM* FASTCALL GetVM(void)
{
	ASSERT(pVM);
	return pVM;
}

//---------------------------------------------------------------------------
//
//	Bloquear maquina virtual
//
//---------------------------------------------------------------------------
void FASTCALL LockVM(void)
{
	csect.Lock();
}

//---------------------------------------------------------------------------
//
//	Desbloquear maquina virtual
//
//---------------------------------------------------------------------------
void FASTCALL UnlockVM(void)
{
	csect.Unlock();
}

//---------------------------------------------------------------------------
//
//	Dialogo de apertura de archivo
//	* lpszPath siempre debe inicializarse antes de llamar
//
//---------------------------------------------------------------------------
BOOL FASTCALL FileOpenDlg(CWnd *pParent, LPSTR lpszPath, UINT nFilterID)
{
	OPENFILENAME ofn;
	TCHAR szFilter[0x200];
	TCHAR szDrive[_MAX_DRIVE];
	TCHAR szDir[_MAX_DIR];
	CString strFilter;
	int i;
	int nLen;
	WIN32_FIND_DATA wfd;
	HANDLE hFind;

	ASSERT(pParent);
	ASSERT(lpszPath);
	ASSERT(nFilterID);

	// Configurar estructura
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_FILEMUSTEXIST;

	// Configurar filtro
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// Ejecutar dialogo comun
	if (!GetOpenFileName(&ofn)) {
		return FALSE;
	}

	// Obtener el nombre de archivo formal (FindFirstFile solo devuelve el nombre + extension)
	hFind = FindFirstFile(lpszPath, &wfd);
	FindClose(hFind);
	_tsplitpath(lpszPath, szDrive, szDir, NULL, NULL);
	_tcscpy(lpszPath, szDrive);
	_tcscat(lpszPath, szDir);
	_tcscat(lpszPath, wfd.cFileName);

	// Guardar directorio predeterminado
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Dialogo de guardado de archivo
//	* lpszPath siempre debe inicializarse antes de llamarÅBlpszExtÇÕêÊì™3ï∂éöÇÃÇ›óLå¯
//
//---------------------------------------------------------------------------
BOOL FASTCALL FileSaveDlg(CWnd *pParent, LPSTR lpszPath, LPCTSTR lpszExt, UINT nFilterID)
{
	OPENFILENAME ofn;
	TCHAR szFilter[0x200];
	CString strFilter;
	int i;
	int nLen;

	ASSERT(pParent);
	ASSERT(lpszPath);
	ASSERT(nFilterID);

	// Configurar estructura
	memset(&ofn, 0, sizeof(OPENFILENAME));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = pParent->m_hWnd;
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = lpszPath;
	ofn.nMaxFile = _MAX_PATH;
	ofn.lpstrInitialDir = Filepath::GetDefaultDir();
	ofn.Flags = OFN_EXPLORER | OFN_HIDEREADONLY | OFN_OVERWRITEPROMPT;
	ofn.lpstrDefExt = lpszExt;

	// Configurar filtro
	GetMsg(nFilterID, strFilter);
	_tcscpy(szFilter, (LPCTSTR)strFilter);
	nLen = (int)_tcslen(szFilter);
	for (i=0; i<nLen; i++) {
		if (szFilter[i] == _T('|')) {
			szFilter[i] = _T('\0');
		}
	}

	// Ejecutar dialogo comun
	if (!GetSaveFileName(&ofn)) {
		return FALSE;
	}

	// Guardar directorio predeterminado
	Filepath::SetDefaultDir(lpszPath);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Configuracion del mensaje de informacion
//
//---------------------------------------------------------------------------
void FASTCALL SetInfoMsg(LPCTSTR lpszMsg, BOOL bRec)
{
	// Revisar flag de almacenamiento
	if (bRec) {
		// Almacenar direccion del buffer
		lpszInfoMsg = (LPSTR)lpszMsg;
		return;
	}

	// Si no se proporciona direccion de buffer, ignorar
	if (!lpszInfoMsg) {
		return;
	}

	// Verificacion del tama?o del buffer
	if (_tcslen(lpszInfoMsg) < CInfo::InfoBufMax) {
		// Copiar la cadena proporcionada
		_tcscpy(lpszInfoMsg, lpszMsg);
	}
}

//---------------------------------------------------------------------------
//
//	DrawTextW
//	* En sistemas operativos no compatibles, no hace nada
//
//---------------------------------------------------------------------------
int FASTCALL DrawTextWide(HDC hDC, LPCWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat)
{
	// ?Es compatible?
	if (!pDrawTextW) {
		// No hacer nada
		return 1;
	}

	// Dibujar con caracteres anchos
	return pDrawTextW(hDC, lpString, nCount, lpRect, uFormat);
}

//===========================================================================
//
//	Aplicacion
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CApp::CApp() : CWinApp(_T("XM6"))
{
	m_hMutex = NULL;
	m_hUser32 = NULL;
}

// Quitar comillas dobles
void CApp::RemoveDoubleQuotes(LPTSTR str) {
	LPTSTR source = str;
	LPTSTR destination = str;

	while (*source != _T('\0')) {
		if (*source != _T('"')) {
			*destination = *source;
			++destination;
		}
		++source;
	}

	*destination = _T('\0');
}

// Reemplazar las diagonales con barras invertidas
void CApp::ReplaceForwardSlashWithBackslash(LPTSTR str) {
	while (*str != _T('\0')) {
		if (*str == _T('/')) {
			*str = _T('\\');
		}		
		++str;
	}
}


//---------------------------------------------------------------------------
//
//	Iniciar linea de comandos
//
//---------------------------------------------------------------------------
BOOL CApp::InitInstance()
{
	CFrmWnd *pFrmWnd;

	// En este apartado se verifica la linea de comando inicial para cargar un posible HDF *-*
	// Get the string from the command line ('Run' in PocketPC)	
	ReplaceForwardSlashWithBackslash(m_lpCmdLine);
	RemoveDoubleQuotes(m_lpCmdLine);
	//int msgboxIDx = MessageBox(NULL, testString, "BBC", MB_OKCANCEL | MB_DEFBUTTON2);

	// Borrar el directorio por defecto
	Filepath::ClearDefaultDir();

	// Evaluacion de ambiente
	if (!CheckEnvironment()) {
		return FALSE;
	}

	// Comprobacion de la doble activacion
	if (!CheckMutex()) {
		// Si hay una linea de comandos, pase
		if (m_lpCmdLine[0] != _T('\0')) {
			SendCmd();
		}
		return FALSE;
	}

	// Crear ventana principal (asignar a m_pMainWnd inmediatamente)
	pFrmWnd = new CFrmWnd();
	m_pMainWnd = (CWnd*)pFrmWnd;

	pFrmWnd->RutaCompletaArchivoXM6 = m_lpCmdLine;
	
	//Borrar archivo SRAM.DAT	
	try {
		CFile::Remove("SRAM.dat");
	}
	catch (CFileException* e) {
		// Manejo de excepciones especÌficas de CFile
		// Puedes acceder a la informaciÛn detallada de la excepciÛn utilizando mÈtodos como GetErrorMessage
		TRACE(_T("ExcepciÛn de CFile\n"));
		e->Delete();  // Importante liberar la memoria de la excepciÛn
	}

	// Inicializacion
	if (!pFrmWnd->Init()) {
		return FALSE;
	}
		
	// Mostrar
	pFrmWnd->ShowWindow(m_nCmdShow);
	pFrmWnd->UpdateWindow();

	return TRUE;
}
//---------------------------------------------------------------------------
//
//	Fin de la instancia
//
//---------------------------------------------------------------------------
BOOL CApp::ExitInstance()
{
	// Eliminar Mutex
	if (m_hMutex) {
		::CloseHandle(m_hMutex);
		m_hMutex = NULL;
	}

	// Liberar USER32.DLL
	if (m_hUser32) {
		::FreeLibrary(m_hUser32);
		m_hUser32 = NULL;
	}

	// Clase base
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Verificacion de Mutex
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckMutex()
{
	HANDLE hMutex;

	ASSERT(this);

	// Crear independientemente de si existe o no
	hMutex = ::CreateMutex(NULL, TRUE, _T("XM6"));
	if (hMutex) {
		// ?Ya esta iniciado?
		if (::GetLastError() == ERROR_ALREADY_EXISTS) {
			return TRUE;
		}

		// OK
		m_hMutex = hMutex;
		return TRUE;
	}

	// Fallo por alguna razon
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Determinacion del entorno
//
//---------------------------------------------------------------------------
BOOL FASTCALL CApp::CheckEnvironment()
{
	OSVERSIONINFO ovi;
	CString strError;

	ASSERT(this);

	//
	//	Determinacion del OS
	//

	// Determinacion del entorno japones
	::bJapanese = FALSE;
	if (PRIMARYLANGID(GetSystemDefaultLangID()) == LANG_JAPANESE) {
		if (PRIMARYLANGID(GetUserDefaultLangID()) == LANG_JAPANESE) {
			// Determinado tanto por el sistema por defecto como por el usuario por defecto
			::bJapanese = TRUE;
		}
	}

	// Determinacion de Windows NT
	memset(&ovi, 0, sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	VERIFY(::GetVersionEx(&ovi));
	if (ovi.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		::bWinNT = TRUE;
	}
	else {
		::bWinNT = FALSE;
	}

	// Determinacion del soporte de la pagina de codigos 932 (requiere soporte UNICODE)
	::bSupport932 = FALSE;
	::pDrawTextW = NULL;
	if (::bWinNT) {
		// OS con soporte UNICODE
		if (::IsValidCodePage(932)) {
			// Cargar USER32.DLL
			m_hUser32 = ::LoadLibrary(_T("USER32.DLL"));
			if (m_hUser32) {
				// Obtener direccion de DrawTextW
				pDrawTextW = (DRAWTEXTWIDE)::GetProcAddress(m_hUser32, _T("DrawTextW"));
				if (pDrawTextW) {
					// CP932Ç÷ÇÃïœä∑Ç∆ïé¶Ç™â¬?
					::bSupport932 = TRUE;
				}
			}
		}
	}

	//
	//	Determinacion del procesador
	//

	// Determinacion de CMOV
	::bCMOV = FALSE;
	if (::IsCMOVSupport()) {
		::bCMOV = TRUE;
	}

	// Determinacion de MMX(Windows98à»ç~ÇÃÇ›)
	::bMMX = FALSE;
	if (ovi.dwMajorVersion >= 4) {
		// Windows 95 o Windows NT 4 o posterior
		if ((ovi.dwMajorVersion == 4) && (ovi.dwMinorVersion == 0)) {
			// Windows 95 o Windows NT 4
			::bMMX = FALSE;
		}
		else {
			// Depende del procesador
			::bMMX = ::IsMMXSupport();
		}
	}

	// Desde la version 2.05, tanto CMOV como MMX son obligatorios
	if (!::bCMOV || !::bMMX) {
		::GetMsg(IDS_PROCESSOR ,strError);
		AfxMessageBox(strError, MB_ICONSTOP | MB_OK);
		return FALSE;
	}

	// Todo OK
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Envio de linea de comandos
//
//---------------------------------------------------------------------------
void FASTCALL CApp::SendCmd()
{
	HWND hWnd;
	COPYDATASTRUCT cds;

	ASSERT(this);

	// Busqueda de ventana
	hWnd = SearchXM6Wnd();
	if (!hWnd) {
		return;
	}

	// Enviar via WM_COPYDATA
	memset(&cds, 0, sizeof(cds));
	cds.dwData = WM_COPYDATA;
	cds.cbData = ((DWORD)_tcslen(m_lpCmdLine) + 1) * sizeof(TCHAR);
	cds.lpData = m_lpCmdLine;
	::SendMessage(hWnd, WM_COPYDATA, NULL, (LPARAM)&cds);
}

//---------------------------------------------------------------------------
//
//	XM6Busqueda de ventana
//
//---------------------------------------------------------------------------
HWND FASTCALL CApp::SearchXM6Wnd()
{
	HWND hWnd;

	// La ventana es NULL
	hWnd = NULL;

	// Busqueda
	::EnumWindows(EnumXM6Proc, (LPARAM)&hWnd);

	// Poner el resultado en la funcion de callback
	return hWnd;
}

//---------------------------------------------------------------------------
//
//	XM6Busqueda de ventanaÉRÅ[ÉãÉoÉbÉN
//
//---------------------------------------------------------------------------
BOOL CALLBACK CApp::EnumXM6Proc(HWND hWnd, LPARAM lParam)
{
	HWND *phWnd;
	LONG lUser;

	// Recibir parametros
	phWnd = (HWND*)lParam;
	ASSERT(phWnd);
	ASSERT(*phWnd == NULL);

	// Obtener datos de usuario de la ventana correspondiente
	lUser = ::GetWindowLong(hWnd, GWL_USERDATA);

	// Realizar verificacion de XM6
	if (lUser == (LONG)MAKEID('X', 'M', '6', ' ')) {
		// Determinado como ventana de marco de XM6, terminar
		*phWnd = hWnd;
		return FALSE;
	}

	// Es diferente, continuar
	return TRUE;
}

#endif	// _WIN32
