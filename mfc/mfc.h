//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ MFC ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined(mfc_h)
#define mfc_h

//---------------------------------------------------------------------------
//
//	#include
//
//---------------------------------------------------------------------------
#define WINVER					0x500	// Windows98,Me,2000,XP以降
#define _WIN32_WINNT			0x410	// Windows98,Me,2000,XP以降
#define VC_EXTRALEAN

// MFC
#include <afxwin.h>
#include <afxext.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <afxmt.h>
#include <afxconv.h>

// Win32API
#include <imm.h>
#include <mmsystem.h>
#include <shlobj.h>

// DirectX
#define DIRECTSOUND_VERSION		0x500	// DirectX5を指定
#include <dsound.h>
#define DIRECTINPUT_VERSION		0x500	// DirectX5を指定
#include <dinput.h>

// C Runtime
#include <math.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

//---------------------------------------------------------------------------
//
//	#define
//
//---------------------------------------------------------------------------
#if defined(_DEBUG)
#define new		DEBUG_NEW
#endif	// _DEBUG

#if defined(_MSC_VER) && defined(_M_IX86)
#define FASTCALL	__fastcall
#else
#define FASTCALL
#endif	// _MSC_VER

//---------------------------------------------------------------------------
//
//	クラス宣言
//
//---------------------------------------------------------------------------
class VM;
										// 仮想マシン

class CApp;
										// アプリケーション
class CFrmWnd;
										// フレームウィンドウ
class CDrawView;
										// 描画ビュー
class CStatusView;
										// ステータスビュー

class CSubWnd;
										// サブウィンドウ
class CSubTextWnd;
										// サブウィンドウ(テキスト)
class CSubListWnd;
										// サブウィンドウ(リストコントロール)

class CComponent;
										// コンポーネント共通
class CConfig;
										// コンフィグコンポーネント
class CScheduler;
										// スケジュールコンポーネント
class CSound;
										// サウンドコンポーネント
class CInput;
										// インプットコンポーネント
class CPort;
										// ポートコンポーネント
class CMIDI;
										// MIDIコンポーネント
class CTKey;
										// TrueKeyコンポーネント
class CHost;
										// ホストコンポーネント
class CInfo;
										// 情報コンポーネント

//---------------------------------------------------------------------------
//
//	グローバル
//
//---------------------------------------------------------------------------
extern VM *pVM;
										// 仮想マシン
BOOL FASTCALL IsJapanese(void);
										// 日本語環境の判定
BOOL FASTCALL IsWinNT(void);
										// WindowsNT環境の判定
BOOL FASTCALL Support932(void);
										// CP932サポート有無の判定
BOOL FASTCALL IsMMX(void);
										// MMX環境の判定
BOOL FASTCALL IsCMOV(void);
										// CMOVサポートの判定
void FASTCALL GetMsg(UINT uID, CString& string);
										// メッセージ取得
VM* FASTCALL GetVM(void);
										// 仮想マシンを取得
void FASTCALL LockVM(void);
										// 仮想マシンをロック
void FASTCALL UnlockVM(void);
										// 仮想マシンをアンロック
BOOL FASTCALL FileOpenDlg(CWnd *pParent, LPTSTR lpszPath, UINT nFilterID);
										// ファイルオープンダイアログ
BOOL FASTCALL FileSaveDlg(CWnd *pParent, LPTSTR lpszPath, LPCTSTR lpszExt, UINT nFilterID);
										// ファイルセーブダイアログ
void FASTCALL SetInfoMsg(LPCTSTR lpszBuf, BOOL bRec);
										// 情報メッセージ保持サブ
int FASTCALL DrawTextWide(HDC hDC, LPCWSTR lpString, int nCount, LPRECT lpRect, UINT uFormat);
										// DrawTextWラッパ

#endif	// mfc_h
#endif	// _WIN32
