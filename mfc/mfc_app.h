//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
//	[ Aplicacion MFC ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_app_h)
#define mfc_app_h

//===========================================================================
//
//	Aplicacion
//
//===========================================================================
class CApp : public CWinApp
{
public:
	CApp();
										// Constructor
	BOOL InitInstance();
										// Inicializacion de instancia
	BOOL ExitInstance();
										// Fin de la instancia

private:
	void ReplaceForwardSlashWithBackslash(LPTSTR str);
	void RemoveDoubleQuotes(LPTSTR str);

	BOOL FASTCALL CheckMutex();
										// Verificacion de Mutex
	BOOL FASTCALL CheckEnvironment();
										// Verificacion de entorno
	void FASTCALL SendCmd();
										// Envio de comando
	HWND FASTCALL SearchXM6Wnd();
										// XM6Busqueda de ventana
	static BOOL CALLBACK EnumXM6Proc(HWND hWnd, LPARAM lParam);
										// Callback de enumeracion de ventanas
	HANDLE m_hMutex;
										// Manejador (Handle) de Mutex
	HMODULE m_hUser32;
										// Manejador (Handle) de USER32.DLL
};

#endif	// mfc_app_h
#endif	// _WIN32
