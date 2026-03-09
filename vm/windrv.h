//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Modified (C) 2006 co (cogood＠gmail.com)
//	[ Windrv ]
//
//---------------------------------------------------------------------------

#if !defined(windrv_h)
#define windrv_h

#include "os.h"
#include "device.h"
#include "host_fs.h"
#include "host_services.h"

//■最大スレッド数
#define WINDRV_THREAD_MAX	3

//■WINDRV互換動作のサポート
#define WINDRV_SUPPORT_COMPATIBLE

//■ログ出力有効
#ifdef _DEBUG
#define WINDRV_LOG
#endif // _DEBUG


class CWindrv : public WindrvContext {
public:
	// 基本ファンクション
	CWindrv(Windrv* pWindrv, Memory* pMemory, DWORD nHandle = 0);
										// コンストラクタ
	virtual ~CWindrv();
										// デストラクタ

	// スレッド処理
	BOOL FASTCALL Start();
										// スレッド起動
	BOOL FASTCALL Terminate();
										// スレッド停止

	// Windrvが利用する外部API
	void FASTCALL Execute(DWORD nA5);
										// コマンド実行
#ifdef WINDRV_SUPPORT_COMPATIBLE
	DWORD FASTCALL ExecuteCompatible(DWORD nA5);
										// コマンド実行 (WINDRV互換)
#endif // WINDRV_SUPPORT_COMPATIBLE
	DWORD FASTCALL GetHandle() const { ASSERT(this); return m_nHandle; }
										// 自身のハンドル(配列番号)を獲得
	BOOL FASTCALL isExecute() const { ASSERT(this); return m_bExecute; }
										// コマンド実行中かどうか確認

	// CWindrvManagerが利用する外部API
	void FASTCALL SetAlloc(BOOL bAlloc) { ASSERT(this); m_bAlloc = bAlloc; }
	BOOL FASTCALL isAlloc() const { ASSERT(this); return m_bAlloc; }
										// ハンドル使用中かどうか確認

	// FileSysが利用する外部API
	DWORD FASTCALL GetUnit() const { ASSERT(this); return unit; }
										// ユニット番号取得
	Memory* FASTCALL GetMemory() const { ASSERT(this); return memory; }
										// ユニット番号取得
	void FASTCALL LockXM();
										// VMへのアクセス開始
	void FASTCALL UnlockXM();
										// VMへのアクセス終了
	void FASTCALL Ready();
										// コマンド完了を待たずにVMスレッド実行再開

	// スレッド実体
	static DWORD __stdcall Run(void* pThis);
										// スレッド実行開始ポイント
	void FASTCALL Runner();
										// スレッド実体

private:
	// コマンドハンドラ
	void FASTCALL ExecuteCommand();
										// コマンドハンドラ

	void FASTCALL InitDrive();
										// $40 - 初期化
	void FASTCALL CheckDir();
										// $41 - ディレクトリチェック
	void FASTCALL MakeDir();
										// $42 - ディレクトリ作成
	void FASTCALL RemoveDir();
										// $43 - ディレクトリ削除
	void FASTCALL Rename();
										// $44 - ファイル名変更
	void FASTCALL Delete();
										// $45 - ファイル削除
	void FASTCALL Attribute();
										// $46 - ファイル属性取得/設定
	void FASTCALL Files();
										// $47 - ファイル検索(First)
	void FASTCALL NFiles();
										// $48 - ファイル検索(NFiles)
	void FASTCALL Create();
										// $49 - ファイル作成
	void FASTCALL Open();
										// $4A - ファイルオープン
	void FASTCALL Close();
										// $4B - ファイルクローズ
	void FASTCALL Read();
										// $4C - ファイル読み込み
	void FASTCALL Write();
										// $4D - ファイル書き込み
	void FASTCALL Seek();
										// $4E - ファイルシーク
	void FASTCALL TimeStamp();
										// $4F - ファイル時刻取得/設定
	void FASTCALL GetCapacity();
										// $50 - 容量取得
	void FASTCALL CtrlDrive();
										// $51 - ドライブ状態検査/制御
	void FASTCALL GetDPB();
										// $52 - DPB取得
	void FASTCALL DiskRead();
										// $53 - セクタ読み込み
	void FASTCALL DiskWrite();
										// $54 - セクタ書き込み
	void FASTCALL IoControl();
										// $55 - IOCTRL
	void FASTCALL Flush();
										// $56 - フラッシュ
	void FASTCALL CheckMedia();
										// $57 - メディア交換チェック
	void FASTCALL Lock();
										// $58 - 排他制御

	// 終了値
	void FASTCALL SetResult(DWORD result);
										// 終了値書き込み

	// メモリアクセス
	DWORD FASTCALL GetByte(DWORD addr) const;
										// バイト読み込み
	void FASTCALL SetByte(DWORD addr, DWORD data);
										// バイト書き込み
	DWORD FASTCALL GetWord(DWORD addr) const;
										// ワード読み込み
	void FASTCALL SetWord(DWORD addr, DWORD data);
										// ワード書き込み
	DWORD FASTCALL GetLong(DWORD addr) const;
										// ロング読み込み
	void FASTCALL SetLong(DWORD addr, DWORD data);
										// ロング書き込み
	DWORD FASTCALL GetAddr(DWORD addr) const;
										// アドレス読み込み
	void FASTCALL SetAddr(DWORD addr, DWORD data);
										// アドレス書き込み

	// 構造体変換
	void FASTCALL GetNameStsPath(DWORD addr, Human68k::namests_t* pNamests) const;
										// NAMESTSパス名読み込み
	void FASTCALL GetNameSts(DWORD addr, Human68k::namests_t* pNamests) const;
										// NAMESTS読み込み
	void FASTCALL GetFiles(DWORD addr, Human68k::files_t* pFiles) const;
										// FILES読み込み
	void FASTCALL SetFiles(DWORD addr, const Human68k::files_t* pFiles);
										// FILES書き込み
	void FASTCALL GetFcb(DWORD addr, Human68k::fcb_t* pFcb) const;
										// FCB読み込み
	void FASTCALL SetFcb(DWORD addr, const Human68k::fcb_t* pFcb);
										// FCB書き込み
	void FASTCALL SetCapacity(DWORD addr, const Human68k::capacity_t* pCapacity);
										// CAPACITY書き込み
	void FASTCALL SetDpb(DWORD addr, const Human68k::dpb_t* pDpb);
										// DPB書き込み
	void FASTCALL GetIoctrl(DWORD param, DWORD func, Human68k::ioctrl_t* pIoctrl);
										// IOCTRL読み込み
	void FASTCALL SetIoctrl(DWORD param, DWORD func, const Human68k::ioctrl_t* pIoctrl);
										// IOCTRL書き込み
	void FASTCALL GetParameter(DWORD addr, BYTE* pOption, DWORD nSize);
										// パラメータ読み込み

#ifdef WINDRV_LOG
	// ログ
	void Log(DWORD level, char* format, ...) const;
										// ログ出力
#endif // WINDRV_LOG

	// コマンドハンドラ用
	Windrv* windrv;
										// 設定内容
	Memory* memory;
										// メモリ
	DWORD a5;
										// リクエストヘッダ
	DWORD unit;
										// ユニット番号
	DWORD command;
										// コマンド番号

	// スレッド管理用
	DWORD m_nHandle;
										// バッファ番号をハンドルとして使う(0～THREAD_MAX - 1)
	BOOL m_bAlloc;
										// ハンドル使用中のときTRUE
	BOOL m_bExecute;
										// コマンド実行中のときTRUE
	BOOL m_bReady;
										// VMスレッド実行再開のときTRUE
	BOOL m_bTerminate;
										// スレッド利用終了のときTRUE
	void* m_hThread;
										// スレッドハンドル
	void* m_hEventStart;
										// コマンド実行開始通知
	void* m_hEventReady;
										// VMスレッド実行再開通知
};

//===========================================================================
//
//	コマンドハンドラ管理
//
//===========================================================================
class CWindrvManager {
public:
	void FASTCALL Init(Windrv* pWindrv, Memory* pMemory);
										// 初期化
	void FASTCALL Clean();
										// 終了
	void FASTCALL Reset();
										// リセット

	CWindrv* FASTCALL Alloc();
										// 空きスレッド確保
	CWindrv* FASTCALL Search(DWORD nHandle);
										// スレッド検索
	void FASTCALL Free(CWindrv* p);
										// スレッド開放

private:
	CWindrv* m_pThread[WINDRV_THREAD_MAX];
										// コマンドハンドラ実体
};

//===========================================================================
//
//	Windrv
//
//===========================================================================
class Windrv : public MemDevice
{
public:
	// Windrv動作
	enum {
		WINDRV_MODE_NONE = 0,
		WINDRV_MODE_ENABLE = 1,
		WINDRV_MODE_COMPATIBLE = 2,
		WINDRV_MODE_DUAL = 3,
		WINDRV_MODE_DISABLE = 255,
	};

	// 内部データ定義
	typedef struct {
		// コンフィグ
		DWORD enable;					// Windrvサポート 0:無効 1:WindrvXM (2:Windrv互換)

		// ドライブ・ファイル管理
		DWORD drives;					// 確保されたドライブ数 (FileSysアクセス境界チェック用)

		// プロセス
		FileSys *fs;					// ファイルシステム

		// host callback
		host_sync_callback_t lock_vm_cb;
		host_sync_callback_t unlock_vm_cb;
		void *sync_user;
	} windrv_t;

public:
	// 基本ファンクション
	Windrv(VM *p);
										// コンストラクタ
	BOOL FASTCALL Init();
										// 初期化
	void FASTCALL Cleanup();
										// クリーンアップ
	void FASTCALL Reset();
										// リセット
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// セーブ
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ロード
	void FASTCALL ApplyCfg(const Config *config);
										// 設定適用

	// メモリデバイス
	DWORD FASTCALL ReadByte(DWORD addr);
										// バイト読み込み
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// バイト書き込み
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// 読み込みのみ

	// 外部API
	void FASTCALL SetFileSys(FileSys *fs);
										// ファイルシステム設定
	void FASTCALL SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user);
	void FASTCALL HostLockVM() const;
	void FASTCALL HostUnlockVM() const;

	// コマンドハンドラ用 外部API
	void FASTCALL SetUnitMax(DWORD nUnitMax) { ASSERT(this); windrv.drives = nUnitMax; }
										// 現在のユニット番号上限設定
	BOOL FASTCALL isInvalidUnit(DWORD unit) const;
										// ユニット番号チェック
	BOOL FASTCALL isValidUnit(DWORD unit) const { ASSERT(this); return unit < windrv.drives; }
										// ユニット番号チェック (ASSERT専用)
	FileSys* FASTCALL GetFilesystem() const { ASSERT(this); return windrv.fs; }
										// ファイルシステムの獲得

#ifdef WINDRV_LOG
	// ログ
	void FASTCALL Log(DWORD level, char* message) const;
										// ログ出力
#endif // WINDRV_LOG

private:
#ifdef WINDRV_SUPPORT_COMPATIBLE
	void FASTCALL Execute();
										// コマンド実行
#endif // WINDRV_SUPPORT_COMPATIBLE
	void FASTCALL ExecuteAsynchronous();
										// コマンド実行開始 非同期
	DWORD FASTCALL StatusAsynchronous();
										// ステータス獲得 非同期
	void FASTCALL ReleaseAsynchronous();
										// ハンドル開放 非同期

	windrv_t windrv;
										// 内部データ
	CWindrvManager m_cThread;
										// コマンドハンドラ管理
};


#endif // windrv_h






