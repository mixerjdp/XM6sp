//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC アプリケーション ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_app_h)
#define mfc_app_h

//===========================================================================
//
//	アプリケーション
//
//===========================================================================
class CApp : public CWinApp
{
public:
	CApp();
										// コンストラクタ
	BOOL InitInstance();
										// インスタンス初期化
	BOOL ExitInstance();
										// インスタンス終了

private:
	BOOL FASTCALL CheckMutex();
										// Mutexチェック
	BOOL FASTCALL CheckEnvironment();
										// 環境チェック
	void FASTCALL SendCmd();
										// コマンド送信
	HWND FASTCALL SearchXM6Wnd();
										// XM6ウィンドウ検索
	static BOOL CALLBACK EnumXM6Proc(HWND hWnd, LPARAM lParam);
										// ウィンドウ列挙コールバック
	HANDLE m_hMutex;
										// Mutexハンドル
	HMODULE m_hUser32;
										// USER32.DLLハンドル
};

#endif	// mfc_app_h
#endif	// _WIN32
