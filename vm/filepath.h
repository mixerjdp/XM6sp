//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ ファイルパス ]
//
//---------------------------------------------------------------------------

#if !defined(filepath_h)
#define filepath_h

#include "os.h"
#include "xm6.h"

#if defined(_WIN32)

typedef struct {
	DWORD low;
	DWORD high;
} filepath_time_t;

//---------------------------------------------------------------------------
//
//	定数定義
//
//---------------------------------------------------------------------------
#define FILEPATH_MAX		_MAX_PATH

//===========================================================================
//
//	ファイルパス
//	※代入演算子を用意すること
//
//===========================================================================
class Filepath
{
public:
	// システムファイル種別
	enum SysFileType {
		IPL,							// IPL(version 1.00)
		IPLXVI,							// IPL(version 1.10)
		IPLCompact,						// IPL(version 1.20)
		IPL030,							// IPL(version 1.30)後半
		ROM030,							// IPL(version 1.30)前半
		CG,								// CG
		CGTMP,							// CG(Win合成)
		SCSIInt,						// SCSI(内蔵)
		SCSIExt,						// SCSI(外付)
		SRAM							// SRAM
	};

public:
	Filepath();
										// コンストラクタ
	virtual ~Filepath();
										// デストラクタ
	Filepath& operator=(const Filepath& path);
										// 代入

	void FASTCALL Clear();
										// クリア
	void FASTCALL SysFile(SysFileType sys);
										// ファイル設定(システム)
	void FASTCALL SetPath(const TCHAR* lpszPath);
										// ファイル設定(ユーザ)
	void FASTCALL SetBaseDir();
										// ベースディレクトリ設定
	void FASTCALL SetBaseFile(const TCHAR* lpszName);
										// ベースディレクトリ＋ファイル名設定

	BOOL FASTCALL IsClear() const;
										// クリアされているか
	const TCHAR* FASTCALL GetPath() const	{ return m_szPath; }
										// パス名取得
	const char* FASTCALL GetShort() const;
										// ショート名取得(const char*)
	const TCHAR* FASTCALL GetFileExt() const;
										// ショート名取得(LPCTSTR)
	BOOL FASTCALL CmpPath(const Filepath& path) const;
										// パス比較

	static void FASTCALL ClearDefaultDir();
										// デフォルディレクトリを初期化
	static void FASTCALL SetDefaultDir(const TCHAR* lpszPath);
										// デフォルトディレクトリに設定
	static const TCHAR* FASTCALL GetDefaultDir();
	static void FASTCALL ClearSystemDir();
	static void FASTCALL SetSystemDir(const TCHAR* lpszPath);
	static const TCHAR* FASTCALL GetSystemDir();
										// デフォルトディレクトリ取得

	BOOL FASTCALL Save(Fileio *fio, int ver);
										// セーブ
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ロード

private:
	void FASTCALL Split();
										// パス分割
	void FASTCALL Make();
										// パス合成
	void FASTCALL SetCurDir();
										// カレントディレクトリ設定
	BOOL FASTCALL IsUpdate() const;
										// セーブ後の更新ありか
	void FASTCALL GetUpdateTime(filepath_time_t *pSaved, filepath_time_t *pCurrent ) const;
										// セーブ後の時間情報を取得
	TCHAR m_szPath[_MAX_PATH];
										// ファイルパス
	TCHAR m_szDrive[_MAX_DRIVE];
										// ドライブ
	TCHAR m_szDir[_MAX_DIR];
										// ディレクトリ
	TCHAR m_szFile[_MAX_FNAME];
										// ファイル
	TCHAR m_szExt[_MAX_EXT];
										// 拡張子
	BOOL m_bUpdate;
										// セーブ後の更新あり
	filepath_time_t m_SavedTime;
										// セーブ時の日付
	filepath_time_t m_CurrentTime;
										// 現在の日付
	static const TCHAR* SystemFile[];
										// システムファイル
	static char ShortName[_MAX_FNAME + _MAX_DIR];
										// ショート名(char)
	static TCHAR FileExt[_MAX_FNAME + _MAX_DIR];
										// ショート名(TCHAR)
	static TCHAR DefaultDir[_MAX_PATH];
	static TCHAR SystemDir[_MAX_PATH];
										// デフォルトディレクトリ
};

#endif	// _WIN32
#endif	// filepath_h
