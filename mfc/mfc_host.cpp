//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Modified (C) 2006 co (cogood＠gmail.com)
//	[ MFC Host ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "windrv.h"
#include "memory.h"
#include "mfc_com.h"
#include "mfc_cfg.h"
#include "mfc_host.h"

#include <shlobj.h>
#include <winioctl.h>

//===========================================================================
//
//	Windowsファイルドライブ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CWinFileDrv::CWinFileDrv()
{
	// 初期化
	m_bWriteProtect = FALSE;
	m_bSlow = FALSE;
	m_bRemovable = FALSE;
	m_bManual = FALSE;
	m_bEnable = FALSE;
	m_bDevice = FALSE;
	m_hDevice = INVALID_HANDLE_VALUE;
	m_szDevice[0] = _T('\0');
	m_szDrive[0] = _T('\0');
	memset(&m_capCache, 0, sizeof(m_capCache));
	m_bVolumeCache = FALSE;
	m_szVolumeCache[0] = _T('\0');
	m_nUpdate = 0;
	m_bUpdateFile = FALSE;
	m_nUpdateFile = 0;
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	m_nUpdateMedia = 0;
#endif // XM6_HOST_UPDATE_BY_SEQUENCE
#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	m_nUpdateCount = 0;
	memset(m_nUpdateBuffer, 0, sizeof(m_nUpdateBuffer));
#endif // XM6_HOST_UPDATE_BY_FREQUENCY
	m_szBase[0] = _T('\0');
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
CWinFileDrv::~CWinFileDrv()
{
#ifdef XM6_HOST_KEEP_OPEN_ERROR
	// デバイスクローズ
	CloseDevice();
#endif // XM6_HOST_KEEP_OPEN_ERROR
}

//---------------------------------------------------------------------------
//
//	デバイスオープン
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::OpenDevice()
{
	ASSERT(this);
	ASSERT(m_hDevice == INVALID_HANDLE_VALUE);

	m_hDevice = ::CreateFile(
		m_szDevice,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
}

//---------------------------------------------------------------------------
//
//	デバイスクローズ
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::CloseDevice()
{
	ASSERT(this);

	if (m_hDevice != INVALID_HANDLE_VALUE) {
		CloseHandle(m_hDevice);
		m_hDevice = INVALID_HANDLE_VALUE;
	}
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::Init(LPCTSTR szBase, DWORD nFlag)
{
	ASSERT(this);
	ASSERT(szBase);
	ASSERT(_tcslen(szBase) < _MAX_PATH);
	ASSERT(m_bWriteProtect == FALSE);
	ASSERT(m_bSlow == FALSE);
	ASSERT(m_bRemovable == FALSE);
	ASSERT(m_bManual == FALSE);
	ASSERT(m_bEnable == FALSE);
	ASSERT(m_bDevice == FALSE);
	ASSERT(m_hDevice == INVALID_HANDLE_VALUE);
	ASSERT(m_szDevice[0] == _T('\0'));
	ASSERT(m_szDrive[0] == _T('\0'));
	ASSERT(m_capCache.sectors == 0);
	ASSERT(m_bVolumeCache == FALSE);
	ASSERT(m_szVolumeCache[0] == _T('\0'));
	ASSERT(m_nUpdate == 0);
	ASSERT(m_bUpdateFile == FALSE);
	ASSERT(m_nUpdateFile == 0);
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	ASSERT(m_nUpdateMedia == 0);
#endif // XM6_HOST_UPDATE_BY_SEQUENCE
#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	ASSERT(m_nUpdateCount == 0);
	ASSERT(m_szBase[0] == _T('\0'));
#endif // XM6_HOST_UPDATE_BY_FREQUENCY

	// パラメータを受け取る
	if (nFlag & FSFLAG_WRITE_PROTECT) m_bWriteProtect = TRUE;
	if (nFlag & FSFLAG_SLOW) m_bSlow = TRUE;
	if (nFlag & FSFLAG_REMOVABLE) m_bRemovable = TRUE;
	if (nFlag & FSFLAG_MANUAL) m_bManual = TRUE;
	_tcscpy(m_szBase, szBase);

	// ベースパスの最後のパス区切りマークを削除する
	// WARNING: Unicode利用時は修正が必要
	TCHAR* pClear = NULL;
	TCHAR* p = m_szBase;
	for (;;) {
		TCHAR c = *p;
		if (c == _T('\0')) break;
		if (c == '/' || c== '\\') {
			pClear = p;
		} else {
			pClear = NULL;
		}
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
			p++;
			if (*p == _T('\0')) break;
		}
		p++;
	}
	if (pClear) *pClear = _T('\0');

	// ドライブ名獲得
	DWORD n = m_szBase[0] & 0xDF;
	if (n < 'A' || 'Z' < n || m_szBase[1] != ':') {
		// ドライブ名が不正ならネットワークドライブのUNCとして扱う
		m_bSlow = TRUE;
		CheckMedia();
		return;
	}

	// ドライブ名生成
	_stprintf(m_szDrive, _T("%c:\\"), n);

	// ドライブ種類の判定
	DWORD nResult = ::GetDriveType(m_szDrive);
	switch (nResult) {
	case DRIVE_REMOVABLE:
	{
		m_bSlow = TRUE;
		m_bRemovable = TRUE;

		// フロッピーディスク判定
		SHFILEINFO shfi;
		nResult = ::SHGetFileInfo(m_szDrive, 0, &shfi, sizeof(shfi), SHGFI_TYPENAME);
		if (nResult) {
			if (_tcsstr(shfi.szTypeName, _T("フロッピー")) != NULL ||
				_tcsstr(shfi.szTypeName, _T("Floppy")) != NULL)
				m_bManual = TRUE;
		}
		break;
	}

	case DRIVE_CDROM:
		m_bSlow = TRUE;
		m_bRemovable = TRUE;
		break;

	case DRIVE_FIXED:
	case DRIVE_RAMDISK:
		break;

	default:
		m_bSlow = TRUE;
		break;
	}


	// デバイスオープン用UNC生成
	OSVERSIONINFO v;
	memset(&v, 0, sizeof(v));
	v.dwOSVersionInfoSize = sizeof(v);
	::GetVersionEx(&v);
	if (v.dwPlatformId == VER_PLATFORM_WIN32_NT) {
		m_bDevice = TRUE;
		_stprintf(m_szDevice, _T("\\\\.\\%c:"), n);
	}

#ifdef XM6_HOST_KEEP_OPEN_ERROR
	// 毎回オープン/クローズすると遅いので、オープンしたまま使おうと
	// したが使いものにならなかった。
	// ここでオープンした場合、最初からドライブにメディアが入ってい
	// れば何度挿入・排出をしても問題なかったが、最初ドライブにメディ
	// アが入っていなかった場合、このハンドルを閉じるまでWindowsがメ
	// ディア挿入を認識しなくなってしまった。

	// デバイスオープン
	OpenDevice();
#endif // XM6_HOST_KEEP_OPEN_ERROR

	// メディア有効チェック
	if (m_bManual == FALSE) CheckMedia();
}

//---------------------------------------------------------------------------
//
//	ドライブ状態の取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWinFileDrv::GetStatus() const
{
	ASSERT(this);

	return
		(m_bRemovable ? 0 : 0x40) |
		(m_bEnable ? (m_bWriteProtect ? 0x08 : 0) | 0x02 : 0);
}

//---------------------------------------------------------------------------
//
//	メディア状態設定
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::SetEnable(BOOL bEnable)
{
	ASSERT(this);

	m_bEnable = bEnable;

	// メディアが入っていなければキャッシュ消去
	if (bEnable == FALSE) {
		memset(&m_capCache, 0, sizeof(m_capCache));
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
	}
}

//---------------------------------------------------------------------------
//
//	リムーバブルメディアのファイルキャッシュ有効時間を更新
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::SetTimeout()
{
	ASSERT(this);

	// リムーバブルメディアでなければ即終了
	if (m_bRemovable == FALSE) return;

	// 有効時間を更新
	m_bUpdateFile = TRUE;
	m_nUpdateFile = ::GetTickCount();
}

//---------------------------------------------------------------------------
//
//	リムーバブルメディアのファイルキャッシュ有効時間を確認
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::CheckTimeout()
{
	ASSERT(this);

	// リムーバブルメディアでなければ即終了
	if (m_bRemovable == FALSE) return FALSE;

	// タイムアウトを確認
	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nUpdateFile;
	if (nDelta < XM6_HOST_REMOVABLE_CACHE_TIME) return FALSE;	// 更新不要

	if (m_bUpdateFile == FALSE) return FALSE;	// 既に更新済み
	m_bUpdateFile = FALSE;

	return TRUE;	// 更新セヨ
}

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
//---------------------------------------------------------------------------
//
//	リムーバブルメディアの状態更新を有効にする
//
//	コマンド$41(ディレクトリチェック)の直後のコマンド$57(メディア交
//	換チェック)でメディア挿入チェックを実行するため、ステート変数の
//	管理を行なう。
//
//	ただし例外として、コマンド$57(メディア交換チェック)の結果がエラー
//	の場合は、その直後に必ずHuman68kがコマンド$41(ディレクトリチェッ
//	ク)を呼びだすため、この直後のメディア交換チェックでは何も実行さ
//	れないようにする。
//
//	ややこしいけど処理の流れは以下のような感じ。
//	コマンド$57 - メディア交換チェック メディアなし → SetMediaUpdate(TRUE) S2
//	内部動作$41 - ディレクトリチェック S2 SetMediaUpdate(FALSE) Disable状態解除 S0
//	コマンド$57 - メディア交換チェック S0 何もしない メディアなし → SetMediaUpdate(TRUE) S2
//	内部動作$41 - ディレクトリチェック S2 SetMediaUpdate(FALSE) Disable状態解除 S0
//	コマンド$41 - ディレクトリチェック S0 SetMediaUpdate(FALSE) Enable状態設定 S1
//	コマンド$57 - メディア交換チェック S1 交換チェック実行 メディアなし → SetMediaUpdate(TRUE) S2
//	内部動作$41 - ディレクトリチェック S2 SetMediaUpdate(FALSE) Disable状態解除 S0
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::SetMediaUpdate(BOOL bDisable)
{
	ASSERT(this);
	if (bDisable) {
		m_nUpdateMedia = 2;
	} else {
		if (m_nUpdateMedia > 1) {
			m_nUpdateMedia = 0;
		} else {
			m_nUpdateMedia = 1;
		}
	}
}
#endif // XM6_HOST_UPDATE_BY_SEQUENCE

//---------------------------------------------------------------------------
//
//	リムーバブルメディアの状態更新チェック
//
//	手動イジェクトのメディアにおいて、短時間内に連続してCheckMediaが
//	呼び出された場合、アクセス事前チェック処理に移行する。
//
//	mint などのメディア監視パターンは以下の通り。
//	5000ms以内にCheckMediaが1〜2回呼ばれる: リムーバブルメディア監視中
//	100ms以内にCheckMediaが12回以上呼ばれる: リムーバブルメディアを開いた
//
//	概要: 現在のIBM-PC/AT互換機では、FDD内のメディアの存在を、FDD内
//	のデータに直接アクセスすることでしか観測できない。この観測行為に
//	は3つの問題が存在する。
//
//	1. 一般的なAT互換機では、FDDのモーターが停止している状態からのド
//	ライブへのアクセス完了まで、どんなAPIを利用しても1000ms以上かっ
//	てしまう。X680x0では、mint等の代表的なファイラーにおいて、ドライ
//	ブ変更の度に全ドライブのメディア挿入状態を頻繁に確認しているため、
//	毎回観測してしまうと、動作速度が極端に低下し、ユーザの使用感を著
//	しく悪化させる。
//
//	2. FDDのモーターが回転していた場合でも、観測完了までは数10〜
//	100ms程度の処理時間を要する。X680x0では、通常のHuman68kの利用に
//	おいても、メディア状態のチェックが頻繁に行なわれるため、VM側から
//	の指示の度に観測すると動作速度が低下してしまう。
//
//	3. FDDにメディアが存在しない場合に観測が行なわれると、FDDから異
//	音が発生してしまう。これはハードウェアの制約でありソフトウェア側
//	からは解決できない。一方、X680x0では、ポーリングによりメディア存
//	在確認を行なうのが一般的である。VM側からの指示通りに観測行為を行
//	なった場合、一定間隔でFDDが異音を奏でる最悪の事態が発生し、某国
//	民機の悪夢が再来する。
//
//	そこで、手動イジェクト型のリムーバブルメディアについては、以下の
//	条件でメディアの観測を行なう仕様とした。
//
//	1. 実際にホスト側OSによるアクセスが行なわれる直前に観測する。
//	   CHostEntry::CheckMediaAccess()のパターン1,2。
//	2. 短期間に集中してメディア観測要求が発生した場合に観測する。
//	   メディア挿入を検出できないUSBデバイスもあるので、この判定は
//	   リムーバブルメディア全てで行なうよう変更。
//     →mint2.xx系に対応させるため、判定を時間ではなくディレクトリ
//     チェック直後のメディア観測要求のタイミングで行なうよう変更。
//	   CHostEntry::CheckMediaAccess()のパターン3。
//	3. 上記2つだけではメディアが入れ換わったことを検出できない可能性
//	   があるため、キャッシュの有効期間を数秒間に制限して対処する。
//	   キャッシュで利用しているFindFirstChangeNotification()がドライ
//	   ブをロックしてイジェクト不可になるため、短時間で開放すること。
//	   CHostEntry::isMediaOffline()とCHostEntry::CheckTimeout()で処理。
//	4. 連続して観測を行なわないよう、一度アクセスしたら数秒間のガー
//	   ド期間を設け、VM側へはキャッシュ情報を渡す。
//
//	上記仕様のため、手動イジェクト型のリムーバブルメディアについては、
//	メディアが存在しない場合のアクセスは正常系とみなし、白帯エラーは
//	出さないことにする。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::CheckMediaUpdate()
{
	ASSERT(this);

	// リムーバブルメディアでなければ即終了
	if (m_bRemovable == FALSE) return FALSE;

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	// 更新フラグを判定
	if (m_nUpdateMedia == 1) {
		m_nUpdateMedia = 0;
		return TRUE;	// 更新セヨ
	}
	m_nUpdateMedia = 0;	// 最初の1回目だけ更新すればいいのでフラグを降ろす
#endif // XM6_HOST_UPDATE_BY_SEQUENCE

#ifdef XM6_HOST_UPDATE_BY_FREQUENCY
	// チェック時刻を記録
	DWORD n = ::GetTickCount();
	DWORD nOld = m_nUpdateBuffer[m_nUpdateCount];
	m_nUpdateBuffer[m_nUpdateCount] = n;
	m_nUpdateCount++;
	m_nUpdateCount %= XM6_HOST_REMOVABLE_RELOAD_COUNT;

	// 時間あたりの呼び出し回数が少なければキャッシュを使用する
	if (n - nOld <= XM6_HOST_REMOVABLE_RELOAD_TIME) return TRUE;	// 更新セヨ
#endif // XM6_HOST_UPDATE_BY_FREQUENCY

#ifdef XM6_HOST_UPDATE_ALWAYS
	return TRUE;	// 更新セヨ
#endif //XM6_HOST_UPDATE_ALWAYS

	return FALSE;	// 更新不要
}

//---------------------------------------------------------------------------
//
//	リムーバブルメディアのアクセス事前チェック
//
//	手動イジェクトのメディアは、アクセスされる直前に状態を確認する。
//	連続して呼び出された場合はキャッシュを使用する。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::CheckMediaAccess(BOOL bManual)
{
	ASSERT(this);

	if (bManual) {
		// 手動イジェクトでなければ即終了
		if (m_bManual == FALSE) return FALSE;
	} else {
		// リムーバブルメディアでなければ即終了
		if (m_bRemovable == FALSE) return FALSE;
	}

	// 連続して呼び出された場合はキャッシュを使用する
	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nUpdate;
	if (nDelta < XM6_HOST_REMOVABLE_GUARD_TIME) return FALSE;	// 更新不要
	m_nUpdate = nCount;

	return TRUE;	// 更新セヨ
}

//---------------------------------------------------------------------------
//
//	メディア有効チェック
//
//	リムーバブルメディア挿入直後に実行すると、Openもしくは
//	DeviceIoControl内部で停止して十数秒間帰ってこないことがあるので
//	呼び出しタイミングに注意すること。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::CheckMedia()
{
	ASSERT(this);

	// メディア挿入とみなす
	BOOL bEnable = TRUE;

	if (m_bDevice) {
		bEnable = FALSE;

		// デバイスオープン
		OpenDevice();

		if (m_hDevice != INVALID_HANDLE_VALUE) {
			// 状態獲得
			DWORD nSize;
			bEnable = ::DeviceIoControl(
				m_hDevice,
				IOCTL_STORAGE_CHECK_VERIFY,
				NULL,
				0,
				NULL,
				0,
				&nSize,
				NULL);
		}

		// デバイスクローズ
		CloseDevice();
	}

	// メディア状態反映
	SetEnable(bEnable);

	// 最終更新時刻
	m_nUpdate = ::GetTickCount();

	return bEnable;
}

//---------------------------------------------------------------------------
//
//	イジェクト
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::Eject()
{
	ASSERT(this);

	// リムーバブルメディアでなければ即終了
	if (m_bRemovable == FALSE) return FALSE;

	// 既にイジェクト済みなら即終了
	if (m_bEnable == FALSE) return FALSE;

	// デバイス制御ができなければ即終了
	if (m_bDevice == FALSE) return FALSE;

	// デバイスオープン
	OpenDevice();
	if (m_hDevice == INVALID_HANDLE_VALUE) return FALSE;

	// マウント解除
	DWORD nSize;
	BOOL bResult = DeviceIoControl(
		m_hDevice,
		FSCTL_DISMOUNT_VOLUME,
		NULL,
		0,
		NULL,
		0,
		&nSize,
		NULL);

	// イジェクト
	bResult = ::DeviceIoControl(
		m_hDevice,
		IOCTL_STORAGE_EJECT_MEDIA,
		NULL,
		0,
		NULL,
		0,
		&nSize,
		NULL);

	// デバイスクローズ
	CloseDevice();

	return bResult;
}

//---------------------------------------------------------------------------
//
//	ボリュームラベルの取得
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileDrv::GetVolume(TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(szLabel);

#if 0	// 使われないロジック
	// メディア状態チェック
	if (m_bEnable == FALSE) {
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
		szLabel[0] = _T('\0');
		return;
	}
#endif

	// ボリュームラベルの取得
	DWORD dwMaximumComponentLength;
	DWORD dwFileSystemFlags;
	BOOL bResult = ::GetVolumeInformation(
		m_szDrive,
		m_szVolumeCache,
		sizeof(m_szVolumeCache) / sizeof(TCHAR),
		NULL,
		&dwMaximumComponentLength,
		&dwFileSystemFlags,
		NULL,
		0);
	if (bResult == 0) {
		m_bVolumeCache = FALSE;
		m_szVolumeCache[0] = _T('\0');
		szLabel[0] = _T('\0');
		return;
	}

	// キャッシュ更新
	m_bVolumeCache = TRUE;
	_tcscpy(szLabel, m_szVolumeCache);
}

//---------------------------------------------------------------------------
//
//	キャッシュからボリュームラベルを取得
//
//	キャッシュされているボリュームラベル情報を転送する。
//	キャッシュ内容が有効なら TRUE を、無効なら FALSE を返す。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::GetVolumeCache(TCHAR* szLabel) const
{
	ASSERT(this);
	ASSERT(szLabel);

	// 内容を転送
	_tcscpy(szLabel, m_szVolumeCache);

	// メディア未挿入なら常にキャッシュ内容を使う
	if (m_bEnable == FALSE) return TRUE;

	return m_bVolumeCache;
}

//---------------------------------------------------------------------------
//
//	容量の取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWinFileDrv::GetCapacity(Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);

#if 0	// 使われないロジック
	// メディア状態チェック
	if (m_bEnable == FALSE) {
		// キャッシュ消去
		memset(&m_capCache, 0, sizeof(m_capCache));
		memset(pCapacity, 0, sizeof(*pCapacity));
		return 0;
	}
#endif

	// ULARGE_INTEGER形式で取得(Win95OSR2以降、WinNTでサポートされたAPI)
	ULARGE_INTEGER ulFree;
	ULARGE_INTEGER ulTotal;
	ULARGE_INTEGER ulTotalFree;
	BOOL bResult = ::GetDiskFreeSpaceEx(m_szDrive, &ulFree, &ulTotal, &ulTotalFree);
	if (bResult == FALSE) {
		// キャッシュ消去
		memset(&m_capCache, 0, sizeof(m_capCache));
		memset(pCapacity, 0, sizeof(*pCapacity));
		return 0;
	}

	// 使用可能バイト数(2GBでクリップ)
	DWORD nFree = 0x7FFFFFFF;
	if (ulFree.HighPart == 0 && ulFree.LowPart < 0x80000000) {
		nFree = ulFree.LowPart;
	}

	// クラスタ構成の計算
	DWORD free;
	DWORD clusters;
	DWORD sectors;
	DWORD bytes;
	if (ulTotal.HighPart == 0 && ulTotal.LowPart < 0x80000000) {
		// 2GB未満ならGetDiskFreeSpaceを使って値を取得
		bResult = ::GetDiskFreeSpace(m_szDrive, &sectors, &bytes, &free, &clusters);
		if (bResult == FALSE) {	// 念のため
			free = nFree;
			clusters = ulTotal.LowPart;
			sectors = 1;
			bytes = 512;
		}

		// セクタサイズが512より大きいパターンを補正 (見たことないが念のため)
		while (bytes > 512) {
			bytes >>= 1;
			sectors <<= 1;

			free <<= 1;
		}

		// セクタサイズが256以下のパターンを補正 (見たことないが念のため)
		while (1 <= bytes && bytes <= 256) {
			bytes <<= 1;
			if (sectors > 1) {
				sectors >>= 1;
			} else {
				clusters >>= 1;
			}

			free >>= 1;
		}

		// 総クラスタ数が16ビットの範囲に収まらないパターンを補正
		while (clusters >= 0x10000) {
			clusters >>= 1;
			sectors <<= 1;

			free >>= 1;
		}
	} else {
		// 2GB以上なら、512(セクタ)×128(クラスタ)×32768に固定
		sectors = 0x80;
		clusters = 0x8000;
		bytes = 512;

		// 空き容量が2GBを超えることは無い
		free = (WORD)(nFree >> 16);
		if (nFree & 0xFFFF) free++;
	}

	// パラメータ範囲チェック (念のため)
	ASSERT(free < 0x10000);
	ASSERT(clusters < 0x10000);
	ASSERT(sectors < 0x10000);
	ASSERT(bytes == 512);

	// 全パラメータセット
	m_capCache.free = (WORD)free;
	m_capCache.clusters = (WORD)clusters;
	m_capCache.sectors = (WORD)sectors;
	m_capCache.bytes = 512;

	// 内容を転送
	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	return nFree;
}

//---------------------------------------------------------------------------
//
//	キャッシュからクラスタサイズを取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileDrv::GetCapacityCache(Human68k::capacity_t* pCapacity) const
{
	ASSERT(this);
	ASSERT(pCapacity);

	// 内容を転送
	memcpy(pCapacity, &m_capCache, sizeof(m_capCache));

	// メディア未挿入なら常にキャッシュ内容を使う
	if (m_bEnable == FALSE) return TRUE;

	return (m_capCache.sectors != 0);
}

//===========================================================================
//
//	ディレクトリエントリ ファイル名
//
//===========================================================================

DWORD CHostFilename::g_nOption;			// 動作フラグ (ファイル名変換関連)

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CHostFilename::CHostFilename()
{
	m_pszWin32 = NULL;
	m_bCorrect = FALSE;
	m_szHuman[0] = '\0';
	// memset(&m_dirEntry, 0, sizeof(m_dirEntry));		// 出前迅速初期化は不要
	m_pszHumanLast = m_szHuman;
	m_pszHumanExt = m_szHuman;
	m_pEntry = NULL;
	m_pChild = NULL;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
CHostFilename::~CHostFilename()
{
	FreeWin32();
	DeleteCache();
}

//---------------------------------------------------------------------------
//
//	Win32側の名称の設定
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilename::SetWin32(const TCHAR* szWin32)
{
	ASSERT(this);
	ASSERT(szWin32);
	ASSERT(_tcslen(szWin32) < _MAX_PATH);
	ASSERT(m_pszWin32 == NULL);

	// Win32名称コピー
	m_pszWin32 = (TCHAR*)malloc(_tcslen(szWin32) + 1);
	ASSERT(m_pszWin32);
	_tcscpy(m_pszWin32, szWin32);
}

//---------------------------------------------------------------------------
//
//	Human68k側のファイル名要素をコピー
//
//---------------------------------------------------------------------------
BYTE* FASTCALL CHostFilename::CopyName(BYTE* pWrite, const BYTE* pFirst, const BYTE* pLast)
{
	ASSERT(this);
	ASSERT(pWrite);
	ASSERT(pFirst);
	ASSERT(pLast);

	for (const BYTE* p = pFirst; p < pLast; p++) {
		*pWrite++ = *p;
	}

	return pWrite;
}

//---------------------------------------------------------------------------
//
//	Human68k側の名称を変換
//
//	あらかじめ SetWin32() を実行しておくこと。
//	18+3の命名規則に従った名前変換を行なう。
//	ファイル名先頭および末尾の空白は、Human68kで扱えないため自動的に削除される。
//	ディレクトリエントリの名前部分を、ファイル名変換時の拡張子の位置情報を使って生成する。
//	その後、ファイル名の異常判定を行なう。(スペース8文字だけのファイル名など)
//	ファイル名の重複判定は行なわないので注意。これらの判定は上位クラスで行なう。
//	TwentyOne version 1.36c modified +14 patchlevel9 以降の拡張子規則に対応させる。
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilename::SetHuman(int nCount)
{
	ASSERT(this);
	ASSERT(m_pszWin32);
	ASSERT(_tcslen(m_pszWin32) < _MAX_PATH);

	// サブディレクトリ名の場合は変換しない
	if (m_pszWin32[0] == '.') {
		if (m_pszWin32[1] == '\0' || (m_pszWin32[1] == '.' && m_pszWin32[2] == '\0')) {
			strcpy((char*)m_szHuman, (char*)m_pszWin32);	// WARNING: Unicode時要修正

			m_bCorrect = TRUE;
			m_pszHumanLast = m_szHuman + strlen((char*)m_szHuman);
			m_pszHumanExt = m_pszHumanLast;
			return;
		}
	}

	DWORD nMax = 18;	// ベース部分(ベース名と拡張子名)のバイト数
	if (g_nOption & WINDRV_OPT_CONVERT_LENGTH) nMax = 8;

	// ベース名部分の補正準備
	BYTE szNumber[8];
	BYTE* pNumber = NULL;
	if (nCount >= 0) {
		pNumber = &szNumber[8];
		for (int i = 0; i < 5; i++) {	// 最大5+1桁まで (ベース名先頭2バイトは必ず残す)
			int n = nCount % 36;
			nMax--;
			pNumber--;
			*pNumber = (BYTE)n + (n < 10 ? '0' : 'A' - 10);
			nCount /= 36;
			if (nCount == 0) break;
		}
		nMax--;
		pNumber--;
		BYTE c = (BYTE)(g_nOption >> 24) & 0x7F;
		if (c == 0) c = XM6_HOST_FILENAME_MARK;
		*pNumber = c;
	}

	// 文字変換
	// WARNING: Unicode未対応。いずれUnicodeの世界に飮まれた時はここで変換を行なう
	BYTE szHuman[_MAX_PATH];
	const BYTE* pFirst = szHuman;
	const BYTE* pLast;
	const BYTE* pExt = NULL;

	{
		const BYTE* pRead = (const BYTE*)m_pszWin32;
		BYTE* pWrite = szHuman;
		const BYTE* pPeriod = SeparateExt(pRead);

		for (bool bFirst = true; ; bFirst = false) {
			BYTE c = *pRead++;
			switch (c) {
			case ' ':
				if (g_nOption & WINDRV_OPT_REDUCED_SPACE) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_SPACE) c = '_';
				else if (pWrite == szHuman) continue;	// 先頭の空白は無視
				break;
			case '=':
			case '+':
				if (g_nOption & WINDRV_OPT_REDUCED_BADCHAR) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_BADCHAR) c = '_';
				break;
			case '-':
				if (bFirst) {
					if (g_nOption & WINDRV_OPT_REDUCED_HYPHEN) continue;
					if (g_nOption & WINDRV_OPT_CONVERT_HYPHEN) c = '_';
					break;
				}
				if (g_nOption & WINDRV_OPT_REDUCED_HYPHENS) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_HYPHENS) c = '_';
				break;
			case '.':
				if (pRead - 1 == pPeriod) {		// Human68k拡張子は例外とする
					pExt = pWrite;
					break;
				}
				if (bFirst) {
					if (g_nOption & WINDRV_OPT_REDUCED_PERIOD) continue;
					if (g_nOption & WINDRV_OPT_CONVERT_PERIOD) c = '_';
					break;
				}
				if (g_nOption & WINDRV_OPT_REDUCED_PERIODS) continue;
				if (g_nOption & WINDRV_OPT_CONVERT_PERIODS) c = '_';
				break;
			}
			*pWrite++ = c;
			if (c == '\0') break;
		}

		pLast = pWrite - 1;
	}

	// 拡張子補正
	if (pExt) {
		// 末尾の空白を削除する
		while(pExt < pLast - 1 && *(pLast - 1) == ' ') {
			pLast--;
			BYTE* p = (BYTE*)pLast;
			*p = '\0';
		}

		// 変換後に実体がなくなった場合は削除
		if (pExt + 1 >= pLast) {
			pLast = pExt;
			BYTE* p = (BYTE*)pLast;
			*p = '\0';		// 念のため
		}
	} else {
		pExt = pLast;
	}

	// 登場人物紹介
	//
	// pFirst: 俺はリーダー。ファイル名先頭
	// pCut: 通称フェイス。最初のピリオドの出現位置 その後ベース名終端位置となる
	// pSecond: よぉおまちどう。俺様こそマードック。拡張子名の開始位置。だから何。
	// pExt: B・A・バラカス。Human68k拡張子の天才だ。でも、3文字より長い名前は勘弁な。
	// 最後のピリオドの出現位置 該当しなければpLastと同じ値
	//
	// ↓pFirst            ↓pStop ↓pSecond ←                ↓pExt
	// T h i s _ i s _ a . V e r y . L o n g . F i l e n a m e . t x t \0
	//         ↑pCut ← ↑pCut初期位置                                ↑pLast
	//
	// 上記の場合、変換後は This.Long.Filename.txt となる

	// 1文字目判定
	const BYTE* pCut = pFirst;
	const BYTE* pStop = pExt - nMax;	// 拡張子名は最大17バイトとする(ベース名を残す)
	if (pFirst < pExt) {
		pCut++;		// 必ず1バイトはベース名を使う
		BYTE c = *pFirst;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
			pCut++;		// ベース名 最小2バイト
			pStop++;	// 拡張子名 最大16バイト
		}
	}
	if (pStop < pFirst) pStop = pFirst;

	// ベース名判定
	pCut = (BYTE*)strchr((char*)pCut, '.');	// SJIS2バイト目は必ず0x40以上なので問題ない
	if (pCut == NULL) pCut = pLast;
	if ((DWORD)(pCut - pFirst) > nMax) {
		pCut = pFirst + nMax;	// 後ほどSJIS2バイト判定/補正を行なう ここで判定してはいけない
	}

	// 拡張子名判定
	const BYTE* pSecond = pExt;
	for (const BYTE* p = pExt - 1; pStop < p; p--) {
		if (*p == '.') pSecond = p;	// SJIS2バイト目は必ず0x40以上なので問題ない
	}

	// ベース名を短縮
	DWORD nExt = pExt - pSecond;	// 拡張子名部分の長さ
	if ((DWORD)(pCut - pFirst) + nExt > nMax) pCut = pFirst + nMax - nExt;
	
	for (const BYTE* p = pFirst; p < pCut; p++) {
		BYTE c = *p;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
			p++;
			if (p >= pCut) {
				pCut--;
				break;
			}
		}
	}

	// 名前の結合
	BYTE* pWrite = m_szHuman;
	pWrite = CopyName(pWrite, pFirst, pCut);	// ベース名を転送
	if (pNumber) pWrite = CopyName(pWrite, pNumber, &szNumber[8]);	// 補正文字を転送
	pWrite = CopyName(pWrite, pSecond, pExt);	// 拡張子名を転送
	m_pszHumanExt = pWrite;						// 拡張子位置保存
	pWrite = CopyName(pWrite, pExt, pLast);		// Human68k拡張子を転送
	m_pszHumanLast = pWrite;					// 終端位置保存
	*pWrite = '\0';

	// 変換結果の確認
	m_bCorrect = TRUE;
	int nSize = m_pszHumanExt - m_szHuman;				// 拡張子の位置を保存

	// ファイル名本体が存在しなければ不合格
	if (nSize <= 0) m_bCorrect = FALSE;

	// ファイル名本体が1文字以上でかつ空白で終了していれば不合格
	// ファイル名本体が8文字以上の場合、理論上は空白での終了が表現可
	// 能だが、Human68kでは正しく扱えないため、これも不合格とする
	else if (m_szHuman[nSize - 1] == ' ') m_bCorrect = FALSE;

	// 変換結果がディレクトリ名と同じなら不合格
	if (m_szHuman[0] == '.') {
		if (m_szHuman[1] == '\0' || (m_szHuman[1] == '.' && m_szHuman[2] == '\0')) {
			m_bCorrect = FALSE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Human68k側の名称を複製
//
//	ファイル名部分の情報を複製し、SetHuman() 相当の初期化動作を行なう。
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilename::CopyHuman(const BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < 23);

	strcpy((char*)m_szHuman, (char*)szHuman);
	m_bCorrect = TRUE;
	m_pszHumanLast = m_szHuman + strlen((char*)m_szHuman);
	m_pszHumanExt = (BYTE*)SeparateExt(m_szHuman);
}

//---------------------------------------------------------------------------
//
//	ディレクトリエントリ情報の設定
//
//	ホスト側Findの結果から、属性・サイズ・日付情報をディレクトリエン
//	トリに反映する。ファイル名情報はFindの結果ではなくクラス変数に設
//	定済みのデータを反映するため、あらかじめ SetHuman() を実行してお
//	くこと(実行していなくても特に不具合は起きないが)。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostFilename::SetEntry(const WIN32_FIND_DATA* pWin32Find)
{
	ASSERT(this);
	ASSERT(pWin32Find);

	// ファイル名設定
	BYTE* p = m_szHuman;
	for (int i = 0; i < 8; i++) {
		if (p < m_pszHumanExt)
			m_dirEntry.name[i] = *p++;
		else
			m_dirEntry.name[i] = ' ';
	}

	for (int i = 0; i < 10; i++) {
		if (p < m_pszHumanExt)
			m_dirEntry.add[i] = *p++;
		else
			m_dirEntry.add[i] = '\0';
	}

	if (*p == '.') p++;
	for (int i = 0; i < 3; i++) {
		BYTE c = *p;
		if (c) p++;
		m_dirEntry.ext[i] = c;
	}

	// 属性設定
	DWORD n = pWin32Find->dwFileAttributes;
	BYTE nHumanAttribute = Human68k::AT_ARCHIVE;
	if ((n & FILE_ATTRIBUTE_DIRECTORY) != 0) {
		nHumanAttribute = Human68k::AT_DIRECTORY;
	}
	if ((n & FILE_ATTRIBUTE_SYSTEM) != 0) nHumanAttribute |= Human68k::AT_SYSTEM;
	if ((n & FILE_ATTRIBUTE_HIDDEN) != 0) nHumanAttribute |= Human68k::AT_HIDDEN;
	if ((n & FILE_ATTRIBUTE_READONLY) != 0) nHumanAttribute |= Human68k::AT_READONLY;
	m_dirEntry.attr = nHumanAttribute;

	// サイズ設定
	m_dirEntry.size = pWin32Find->nFileSizeLow;
	if (pWin32Find->nFileSizeHigh > 0 || pWin32Find->nFileSizeLow > XM6_HOST_FILESIZE_MAX)
		m_dirEntry.size = XM6_HOST_FILESIZE_MAX;

	// 日付・時刻設定
	m_dirEntry.date = 0;
	m_dirEntry.time = 0;
	FILETIME ft = pWin32Find->ftLastWriteTime;
	if (ft.dwLowDateTime == 0 && ft.dwHighDateTime == 0) {
		ft = pWin32Find->ftCreationTime;
	}
	FILETIME lt;
	if (::FileTimeToLocalFileTime(&ft, &lt) == 0) return FALSE;
	if (::FileTimeToDosDateTime(&lt, &m_dirEntry.date, &m_dirEntry.time) == 0) return FALSE;

	// クラスタ番号設定
	m_dirEntry.cluster = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Human68k側の名称が加工されたか調査
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostFilename::isReduce() const
{
	ASSERT(this);
	ASSERT(m_pszWin32);

	return strcmp((char*)m_pszWin32, (char*)m_szHuman) != 0; // WARNING: Unicode時要修正
}

//---------------------------------------------------------------------------
//
//	該当エントリのキャッシュを削除
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilename::DeleteCache()
{
	ASSERT(this);

	if (m_pEntry) m_pEntry->DeleteCache(m_pChild);
}

//---------------------------------------------------------------------------
//
//	Human68kファイル名から拡張子を分離
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL CHostFilename::SeparateExt(const BYTE* szHuman)
{
	// ファイル名の長さを獲得
	int nLength = strlen((char*)szHuman);
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + nLength;

	// Human68k拡張子の位置を確認
	const BYTE* pExt = (BYTE*)strrchr((char*)pFirst, '.');	// SJIS2バイト目は必ず0x40以上なので問題ない
	if (pExt == NULL) pExt = pLast;
	// ファイル名が20〜22文字かつ19文字目が'.'かつ'.'で終了というパターンを特別扱いする
	if (20 <= nLength && nLength <= 22 && pFirst[18] == '.' && pFirst[nLength-1] == '.')
		pExt = pFirst + 18;
	// 拡張子の文字数を計算	(-1:なし 0:ピリオドだけ 1〜3:Human68k拡張子 4以上:拡張子名)
	int nExt = pLast - pExt - 1;
	// '.' が文字列先頭以外に存在して、かつ1〜3文字の場合のみ拡張子とみなす
	if (pExt == pFirst || nExt < 1 || nExt > 3) pExt = pLast;

	return pExt;
}

//===========================================================================
//
//	ディレクトリエントリ パス名
//
//	Human68k側のパス名は、必ず先頭が / で始まり、末尾に / がつかない。
//	(末尾が / で終わる場合は空のファイル名指定と判断される)
//	また、ユニット番号を持ち、常にペアで扱う。
//	従来の mfc_host での扱いと異なるので注意。
//	高速化のため、Win32パス名はベースパス部分も含む。
//	ベースパス部分なしでも動作可能とする。
//
//===========================================================================

DWORD CHostPath::g_nId;				// 識別ID生成用カウンタ
DWORD CHostPath::g_nOption;			// 動作フラグ (パス名比較関連)

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CHostPath::CHostPath()
{
	m_nHumanUnit = 0;
	m_pszHuman = NULL;
	m_pszWin32 = NULL;
	m_hChange = INVALID_HANDLE_VALUE;
	m_nId = g_nId++;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
CHostPath::~CHostPath()
{
#if 0
	OutputDebugString("Cache Delete ");
	OutputDebugString(m_pszWin32);
	OutputDebugString("\r\n");
#endif

	Clean();
}

//---------------------------------------------------------------------------
//
//	Human68k側の名称を直接指定する
//
//---------------------------------------------------------------------------
void FASTCALL CHostPath::SetHuman(DWORD nUnit, BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);
	ASSERT(m_pszHuman == NULL);

	m_pszHuman = (BYTE*)malloc(strlen((char*)szHuman) + 1);
	ASSERT(m_pszHuman);
	strcpy((char*)m_pszHuman, (char*)szHuman);
	m_nHumanUnit = nUnit;
}

//---------------------------------------------------------------------------
//
//	Win32側の名称を直接指定する
//
//---------------------------------------------------------------------------
void FASTCALL CHostPath::SetWin32(TCHAR* szWin32)
{
	ASSERT(this);
	ASSERT(szWin32);
	ASSERT(_tcslen(szWin32) < _MAX_PATH);
	ASSERT(m_pszWin32 == NULL);

	m_pszWin32 = (TCHAR*)malloc(_tcslen(szWin32) + 1);
	ASSERT(m_pszWin32);
	_tcscpy(m_pszWin32, szWin32);
}

//---------------------------------------------------------------------------
//
//	文字列比較 (ワイルドカード対応)
//
//---------------------------------------------------------------------------
int FASTCALL CHostPath::Compare(const BYTE* pFirst, const BYTE* pLast, const BYTE* pBufFirst, const BYTE* pBufLast)
{
	ASSERT(pFirst);
	ASSERT(pLast);
	ASSERT(pBufFirst);
	ASSERT(pBufLast);

	// 文字比較
	BOOL bSkip0 = FALSE;
	BOOL bSkip1 = FALSE;
	for (const BYTE* p = pFirst; p < pLast; p++) {
		// 1文字読み込み
		BYTE c = *p;
		BYTE d = '\0';
		if (pBufFirst < pBufLast) d = *pBufFirst++;

		// 比較のための文字補正
		if (bSkip0 == FALSE) {
			if (bSkip1 == FALSE) {	// cもdも1バイト目
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
					bSkip0 = TRUE;
				}
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
					bSkip1 = TRUE;
				}
				if (c == d) continue;	// 高確率で判定完了する
				if ((g_nOption & WINDRV_OPT_ALPHABET) == 0) {
					if ('A' <= c && c <= 'Z') c += 'a' - 'A';	// 小文字化
					if ('A' <= d && d <= 'Z') d += 'a' - 'A';	// 小文字化
				}
			} else {		// cだけが1バイト目
				if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
					bSkip0 = TRUE;
				}
				bSkip1 = FALSE;
			}
		} else {
			if (bSkip1 == FALSE) {	// dだけが1バイト目
				bSkip0 = FALSE;
				if ((0x80 <= d && d <= 0x9F) || 0xE0 <= d) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
					bSkip1 = TRUE;
				}
			} else {		// cもdも2バイト目
				bSkip0 = FALSE;
				bSkip1 = FALSE;
			}
		}

		// 比較
		if (c == d) continue;
		if (c == '?') continue;
		//if (c == '*') return 0;
		return 1;
	}
	if (pBufFirst < pBufLast) return 2;

	return 0;
}

//---------------------------------------------------------------------------
//
//	Human68k側の名称を比較する
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostPath::isSameHuman(DWORD nUnit, const BYTE* szHuman) const
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(m_pszHuman);

	// ユニット番号の比較
	if (m_nHumanUnit != nUnit) return FALSE;

	// 文字数計算
	const BYTE* pFirst = m_pszHuman;
	DWORD nLength = strlen((char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	const BYTE* pBufFirst = szHuman;
	DWORD nBufLength = strlen((char*)pBufFirst);
	const BYTE* pBufLast = pBufFirst + nBufLength;

	// 文字数チェック
	if (nLength != nBufLength) return FALSE;

	// Human68kパス名の比較
	return Compare(pFirst, pLast, pBufFirst, pBufLast) == 0;
}

//---------------------------------------------------------------------------
//
//	ファイル名を検索
//
//	所有するキャシュバッファの中から検索し、見つかればその名称を返す。
//	パス名を除外しておくこと。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
CHostFilename* FASTCALL CHostPath::FindFilename(BYTE* szHuman, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(szHuman);

	// 文字数計算
	const BYTE* pFirst = szHuman;
	DWORD nLength = strlen((char*)pFirst);
	const BYTE* pLast = pFirst + nLength;

	// 所持している全てのファイル名の中から完全一致するものを検索
	for (CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
		 p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
		// 属性チェック
		if (p->CheckAttribute(nHumanAttribute) == 0) continue;
		// 文字数計算
		const BYTE* pBufFirst = p->GetHuman();
		const BYTE* pBufLast = p->GetHumanLast();
		DWORD nBufLength = pBufLast - pBufFirst;
		// 文字数チェック
		if (nLength != nBufLength) continue;
		// ファイル名チェック
		if (Compare(pFirst, pLast, pBufFirst, pBufLast) == 0) return p;
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	ファイル名を検索 (ワイルドカード対応)
//
//	所有するバッファの中から検索し、見つかればその名称を返す。
//	パス名を除外しておくこと。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
CHostFilename* FASTCALL CHostPath::FindFilenameWildcard(BYTE* szHuman, find_t* pFind, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(pFind);

	// 検索ファイル名を本体とHuman68k拡張子に分ける
	const BYTE* pFirst = szHuman;
	const BYTE* pLast = pFirst + strlen((char*)pFirst);
	const BYTE* pExt = CHostFilename::SeparateExt(pFirst);

	// 開始地点へ移動
	CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
	if (pFind->count > 0) {
		if (pFind->id == m_nId) {
			// キャッシュ内容に変化がない場合は、前回の位置から即検索
			p = pFind->pos;
		} else {
			// 開始地点をエントリ内容から検索する
			DWORD n = 0;
			for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
				if (p->isSameEntry(&pFind->entry)) {
					pFind->count = n;
					break;
				}
				n++;
			}

			// 該当エントリが見つからなかった場合、回数指定を使う
			if (p == &m_cRingFilename) {
				CHostFilename* p = (CHostFilename*)m_cRingFilename.Next();
				n = 0;
				for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
					if (n >= pFind->count) break;
					n++;
				}
			}
		}
	}

	// ファイル検索
	for (; p != &m_cRingFilename; p = (CHostFilename*)p->Next()) {
		pFind->count++;

		// 属性チェック
		if (p->CheckAttribute(nHumanAttribute) == 0) continue;

		// ファイル名を本体とHuman68k拡張子に分ける
		const BYTE* pBufFirst = p->GetHuman();
		const BYTE* pBufLast = p->GetHumanLast();
		const BYTE* pBufExt = p->GetHumanExt();

		// 本体比較
		if (Compare(pFirst, pExt, pBufFirst, pBufExt) != 0) continue;

		// Human68k拡張子比較
		// 拡張子 .??? (.*) の場合は、Human68k拡張子のピリオドなしにもマッチさせる
		if (strcmp((char*)pExt, ".???") == 0 ||	// strncmp((char*)pExt, ".*", 2) == 0 ||
			Compare(pExt, pLast, pBufExt, pBufLast) == 0) {
			// 次の候補のエントリ内容を記録
			CHostFilename* pNext = (CHostFilename*)p->Next();
			pFind->id = m_nId;
			pFind->pos = pNext;
			if (pNext != &m_cRingFilename) {
				memcpy(&pFind->entry, pNext->GetEntry(), sizeof(pFind->entry));
			} else {
				memset(&pFind->entry, 0, sizeof(pFind->entry));
			}
			return p;
		}
	}

	pFind->id = m_nId;
	pFind->pos = p;
	memset(&pFind->entry, 0, sizeof(pFind->entry));
	return NULL;
}

//---------------------------------------------------------------------------
//
//	再利用のための初期化
//
//---------------------------------------------------------------------------
void FASTCALL CHostPath::Clean()
{
	ASSERT(this);

	if (m_hChange != INVALID_HANDLE_VALUE) {
		::FindCloseChangeNotification(m_hChange);
		m_hChange = INVALID_HANDLE_VALUE;
	}

	CleanFilename();

	FreeHuman();
	FreeWin32();
}

//---------------------------------------------------------------------------
//
//	全ファイル名を開放
//
//---------------------------------------------------------------------------
void FASTCALL CHostPath::CleanFilename()
{
	// 実体を開放
	CHostFilename* p;
	while ((p = (CHostFilename*)m_cRingFilename.Next()) != &m_cRingFilename) {
		delete p;
	}
}

//---------------------------------------------------------------------------
//
//	ファイル変更が行なわれたか確認
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostPath::isRefresh()
{
	ASSERT(this);

	if (m_hChange == INVALID_HANDLE_VALUE) return TRUE;		// 初期化されていなければ要更新

	DWORD nResult = WaitForSingleObject(m_hChange, 0);
	if (nResult == WAIT_TIMEOUT) return FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ファイル再構成
//
//	ここで初めて、Win32ファイルシステムの観測が行なわれる。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostPath::Refresh()
{
	ASSERT(this);
	ASSERT(m_pszWin32);
	ASSERT(_tcslen(m_pszWin32) + 22 < _MAX_PATH);
#if 0
	OutputDebugString("Cache Refresh ");
	OutputDebugString(m_pszWin32);
	OutputDebugString("\r\n");
#endif

	// パス名生成
	TCHAR szPath[_MAX_PATH];
	_tcscpy(szPath, m_pszWin32);
	_tcscat(szPath, _T("\\"));

	// 更新チェック
	if (m_hChange == INVALID_HANDLE_VALUE) {
		// 初回
		DWORD nFlag =
			FILE_NOTIFY_CHANGE_FILE_NAME |
			FILE_NOTIFY_CHANGE_DIR_NAME |
			FILE_NOTIFY_CHANGE_ATTRIBUTES |
			FILE_NOTIFY_CHANGE_SIZE |
			FILE_NOTIFY_CHANGE_LAST_WRITE;
		m_hChange = ::FindFirstChangeNotification(szPath, FALSE, nFlag);
		// 外部アプリの影響でエラーが出る可能性あり。このまま続行しても問題ない
	} else {
		// 二回目以降
		for (int i = 0; i < 3; i++) {	// 1回実行しただけではフラッシュされない可能性がある
			DWORD nResult = WaitForSingleObject(m_hChange, 0);
			if (nResult == WAIT_TIMEOUT) break;

			BOOL bResult = ::FindNextChangeNotification(m_hChange);
			if (bResult == FALSE) {
				::FindCloseChangeNotification(m_hChange);
				m_hChange = INVALID_HANDLE_VALUE;
				break;
			}
		}
	}

	// 以前のキャッシュ内容を移動
	CRing cRingBackup;
	m_cRingFilename.InsertRing(&cRingBackup);

	// 検索用ファイル名生成
	_tcscat(szPath, _T("*.*"));

	// ファイル名登録
	BOOL bUpdate = FALSE;
	HANDLE hFind = INVALID_HANDLE_VALUE;
	for (int i = 0; i < XM6_HOST_FILENAME_CACHE_MAX; i++) {
		WIN32_FIND_DATA w32Find;
		if (hFind == INVALID_HANDLE_VALUE) {
			// 最初のファイル名を獲得
			hFind = ::FindFirstFile(szPath, &w32Find);
			if (hFind == INVALID_HANDLE_VALUE) break;
		} else {
			// 次のファイル名を獲得
			BOOL bResult = ::FindNextFile(hFind, &w32Find);
			if (bResult == FALSE) break;
		}

		// 新規ファイル名登録
		CHostFilename* pFilename = new CHostFilename;
		ASSERT(pFilename);
		pFilename->SetWin32((TCHAR*)w32Find.cFileName);

		// 以前のキャッシュ内容に該当するファイル名があればそのHuman68k名称を優先する
		CHostFilename* pCache = (CHostFilename*)cRingBackup.Next();
		for (;;) {
			if (pCache == &cRingBackup) {
				pCache = NULL;			// 該当するエントリなし(この時点で新規エントリと確定)
				pFilename->SetHuman();
				break;
			}
			if (_tcscmp(pFilename->GetWin32(), pCache->GetWin32()) == 0) {
				pFilename->CopyHuman(pCache->GetHuman());	// Human68k名称のコピー
				break;
			}
			pCache = (CHostFilename*)pCache->Next();
		}

		// 新規エントリの場合はファイル名重複をチェックする
		// ファイル名に変更があった場合は、以下のチェックを全てパスしたものを使用する
		// ・正しいファイル名であること
		// ・過去のエントリに同名のものが存在しないこと
		// ・同名の実ファイル名が存在しないこと
		if (pFilename->isReduce()) {	// ファイル名の変更が行なわれた場合のみチェック
			for (int n = 0; n < 36 * 36 * 36 * 36 * 36; n++) {	// 約6千万パターン(36の5乗)
				// 正しいファイル名かどうか確認
				if (pFilename->isCorrect()) {
					// 過去のエントリと一致するか確認
					CHostFilename* pCheck = FindFilename(pFilename->GetHuman());
					if (pCheck == NULL) {
						// 一致するものがなければ、実ファイルが存在するか確認
						TCHAR szBuf[_MAX_PATH];
						_tcscpy(szBuf, m_pszWin32);
						_tcscat(szBuf, _T("\\"));
						_tcscat(szBuf, (TCHAR*)pFilename->GetHuman());	// WARNING: Unicode時要修正
						WIN32_FIND_DATA w32Check;
						HANDLE hCheck = ::FindFirstFile(szBuf, &w32Check);
						if (hCheck == INVALID_HANDLE_VALUE) break;	// 利用可能パターンを発見
						FindClose(hCheck);
					}
				}
				// 新しい名前を生成
				pFilename->SetHuman(n);
			}
		}

		// ディレクトリエントリ生成
		pFilename->SetEntry(&w32Find);

		// 以前のキャッシュ内容と比較
		if (pCache) {
			if (pCache->isSameEntry(pFilename->GetEntry())) {
				delete pFilename;		// 今回作成したエントリは破棄し
				pFilename = pCache;		// 以前のキャッシュ内容を使う
			} else {
				bUpdate = TRUE;			// 一致しなければ更新あり
				delete pCache;			// 次回の検索対象から除外
			}
		} else {
			bUpdate = TRUE;				// 新規エントリのため更新あり
		}

		// リング末尾へ追加
		pFilename->InsertTail(&m_cRingFilename);
	}

	if (hFind != INVALID_HANDLE_VALUE) FindClose(hFind);

	// 残存するキャッシュ内容を削除
	CHostFilename* p;
	while ((p = (CHostFilename*)cRingBackup.Next()) != &cRingBackup) {
		bUpdate = TRUE;			// 削除を行なうので更新あり
		delete p;
	}

	// 更新が行なわれたら識別IDを変更
	if (bUpdate) m_nId = g_nId++;

	// 最後にエラー発生を通知
	if (m_hChange == INVALID_HANDLE_VALUE) return FALSE;

	return TRUE;
}

//===========================================================================
//
//	ディレクトリエントリ管理
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	初期化 (起動直後)
//
//---------------------------------------------------------------------------
CHostEntry::CHostEntry()
{
#ifdef XM6_HOST_STRICT_TIMEOUT
	m_hEvent = NULL;
	m_hThread = NULL;
#else // XM6_HOST_STRICT_TIMEOUT
	m_nTimeout = 0;
#endif // XM6_HOST_STRICT_TIMEOUT
	m_nRingPath = 0;

	InitializeCriticalSection(&m_csAccess);
}

//---------------------------------------------------------------------------
//
//	開放 (終了時)
//
//---------------------------------------------------------------------------
CHostEntry::~CHostEntry()
{
	Clean();

	DeleteCriticalSection(&m_csAccess);
}

//---------------------------------------------------------------------------
//
//	初期化 (ドライバ組込み時)
//
//---------------------------------------------------------------------------
void CHostEntry::Init(CWinFileDrv** ppBase)
{
	ASSERT(this);
	ASSERT(ppBase);

	m_ppBase = ppBase;

#ifdef XM6_HOST_STRICT_TIMEOUT
	ASSERT(m_hEvent == NULL);
	ASSERT(m_hThread == NULL);

	// イベント確保
	m_hEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);		// 自動リセット
	ASSERT(m_hEvent);

	// 監視スレッド開始
	DWORD nThread;
	m_hThread = ::CreateThread(NULL, 0, Run, this, 0, &nThread);
	ASSERT(m_hThread);
#endif // XM6_HOST_STRICT_TIMEOUT
}

//---------------------------------------------------------------------------
//
//	開放 (起動・リセット時)
//
//---------------------------------------------------------------------------
void CHostEntry::Clean()
{
	ASSERT(this);

#ifdef XM6_HOST_STRICT_TIMEOUT
	// 監視スレッド終了
	if (m_hThread) {
		ASSERT(m_hEvent);

		// スレッドへ停止要求
		::SetEvent(m_hEvent);

		// スレッド終了待ち
		DWORD nResult;
		nResult = ::WaitForSingleObject(m_hThread, 30 * 1000);	// 猶予期間 30秒
		if (nResult != WAIT_OBJECT_0) {
			// 強制停止
			ASSERT(FALSE);	// 念のため
			::TerminateThread(m_hThread, (DWORD)-1);
			nResult = ::WaitForSingleObject(m_hThread, 100);
		}

		// スレッドハンドル開放
		::CloseHandle(m_hThread);
		m_hThread = NULL;
	}

	// イベント開放
	if (m_hEvent) {
		::CloseHandle(m_hEvent);
		m_hEvent = NULL;
	}
#endif // XM6_HOST_STRICT_TIMEOUT

	LockCache();

	CleanCache();

	UnlockCache();
}

#ifdef XM6_HOST_STRICT_TIMEOUT
//---------------------------------------------------------------------------
//
//	スレッド実行開始ポイント
//
//---------------------------------------------------------------------------
DWORD WINAPI CHostEntry::Run(VOID* pThis)
{
	ASSERT(pThis);

	((CHostEntry*)pThis)->Runner();

	::ExitThread(0);
	return 0;
}

//---------------------------------------------------------------------------
//
//	スレッド実体
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::Runner()
{
	ASSERT(this);
	ASSERT(m_hEvent);

	for (;;) {
		DWORD nResult = ::WaitForSingleObject(m_hEvent, XM6_HOST_REMOVABLE_CHECK_TIME);
		if (nResult == WAIT_OBJECT_0) break;

		// 全ドライブのタイムアウトチェック
		CheckTimeout();
	}
}
#endif // XM6_HOST_STRICT_TIMEOUT

//---------------------------------------------------------------------------
//
//	指定されたユニットのキャッシュを全て削除する
//
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::CleanCache()
{
	ASSERT(this);

	// 実体を開放
	CHostPath* p;
	while ((p = (CHostPath*)m_cRingPath.Next()) != &m_cRingPath) {
		delete p;
		m_nRingPath--;
	}
	ASSERT(m_nRingPath == 0);

	CHostPath::InitId();
}

//---------------------------------------------------------------------------
//
//	指定されたユニットのキャッシュを全て削除する
//
//	リムーバブルメディアの排出に伴う削除処理。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::EraseCache(DWORD nUnit)
{
	ASSERT(this);

	// 所持している全てのファイル名の中から一致するものを検索
	for (CHostPath* p = (CHostPath*)m_cRingPath.Next(); p != &m_cRingPath;) {
		if (p->isSameUnit(nUnit)) {
			delete p;
			m_nRingPath--;

			// 連鎖反応で次のオブジェクトが消える可能性があるので最初から調べなおす
			p = (CHostPath*)m_cRingPath.Next();
			continue;
		}
		p = (CHostPath*)p->Next();
	}
}

//---------------------------------------------------------------------------
//
//	該当するキャッシュを削除する
//
//	キャッシュは既に削除済みのこともあるため、確認を行なってから削除する。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::DeleteCache(CHostPath* pPath)
{
	ASSERT(this);
	ASSERT(pPath);

	// 所持している全てのファイル名の中から一致するものを検索
	for (CHostPath* p = (CHostPath*)m_cRingPath.Next();
		 p != &m_cRingPath; p = (CHostPath*)p->Next()) {
		if (p == pPath) {
			delete p;
			m_nRingPath--;
			break;
		}
	}
}

//---------------------------------------------------------------------------
//
//	該当するパス名がキャッシュされているか検索する
//
//	所有するキャシュバッファの中から完全一致で検索し、見つかればその名称を返す。
//	ファイル名を除外しておくこと。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
CHostPath* FASTCALL CHostEntry::FindCache(DWORD nUnit, const BYTE* szHuman)
{
	ASSERT(this);
	ASSERT(szHuman);

	// 所持している全てのファイル名の中から完全一致するものを検索
	for (CHostPath* p = (CHostPath*)m_cRingPath.Next();
		 p != &m_cRingPath; p = (CHostPath*)p->Next()) {
		if (p->isSameHuman(nUnit, szHuman)) {
			return p;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	キャッシュ情報を元に、Win32名を獲得する
//
//	パス・ファイル名に1段階だけ分離し、キャッシュにあるか確認。なければエラー。
//	見つかったキャッシュの更新チェック。更新が必要ならエラー。
//	必ず上位で排他制御を行なうこと。
//
//---------------------------------------------------------------------------
CHostPath* FASTCALL CHostEntry::CopyCache(DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);

	// パス名とファイル名に分離
	const BYTE* pSeparate = SeparatePath(szHuman);
	if (pSeparate == NULL) return NULL;	// エラー: パス分離失敗

	BYTE szHumanPath[HUMAN68K_MAX_PATH];
	int nPath = pSeparate - szHuman;
	strncpy((char*)szHumanPath, (char*)szHuman, nPath);
	szHumanPath[nPath] = '\0';

	BYTE szHumanFilename[_MAX_PATH];
	strcpy((char*)szHumanFilename, (char*)pSeparate + 1);

	// パス名のキャッシュ残存チェック
	CHostPath* pPath = FindCache(nUnit, szHumanPath);
	if (pPath == NULL) return NULL;	// エラー: パス名がキャッシュされていないため解析不能

	// リング先頭へ移動
	pPath->Insert(&m_cRingPath);

	// キャッシュ更新チェック
	if (pPath->isRefresh()) return NULL;	// エラー: キャッシュ更新が必要

	// Win32パス名をコピー
	if (szWin32Buffer) {
		_tcscpy(szWin32Buffer, pPath->GetWin32());
		_tcscat(szWin32Buffer, _T("\\"));
	}

	// ファイル名がなければ終了
	if (szHumanFilename[0] == '\0') {
		return pPath;	// 普通はここで終了
	}

	// ファイル名検索
	CHostFilename* pFilename = pPath->FindFilename(szHumanFilename);
	if (pFilename == NULL) return NULL;	// エラー: 途中のパス名/ファイル名が見つからない

	// Win32ファイル名をコピー
	if (szWin32Buffer) {
		_tcscat(szWin32Buffer, pFilename->GetWin32());
	}

	return pPath;
}

//---------------------------------------------------------------------------
//
//	Win32名の構築に必要な情報をすべて取得する
//
//	ファイル名は省略可能。(普通は指定しない)
//	必ず上位で排他制御を行なうこと。
//	ベースパス末尾にパス区切り文字をつけないよう注意。
//	ファイルアクセスが多発する可能性があるときは、VMスレッドの動作を開始させる。
//
//	使いかた:
//	CopyCache()してエラーの場合はMakeCache()する。必ず正しいWin32パスが取得できる。
//
//	ファイル名とパス名をすべて分離する。
//	上位フォルダから順に、キャッシュされているかどうか確認。
//	キャッシュされていれば破棄チェック。破棄した場合未キャッシュ扱いとなる。
//	キャッシュされていなければキャッシュを構築。
//	順番にすべてのフォルダ・ファイル名に対して行ない終了。
//	エラーが発生した場合はNULLとなる。
//
//---------------------------------------------------------------------------
CHostPath* FASTCALL CHostEntry::MakeCache(CWindrv* ps, DWORD nUnit, const BYTE* szHuman, TCHAR* szWin32Buffer)
{
	ASSERT(this);
	ASSERT(szHuman);
	ASSERT(strlen((char*)szHuman) < HUMAN68K_MAX_PATH);

	const TCHAR* szWin32Base = GetBase(nUnit);
	ASSERT(szWin32Base);
	ASSERT(_tcslen(szWin32Base) < _MAX_PATH);

	BYTE szHumanPath[HUMAN68K_MAX_PATH];	// 上位フォルダから順にパス名が入ってゆく
	szHumanPath[0] = '\0';
	int nHumanPath = 0;

	TCHAR szWin32Path[_MAX_PATH];
	_tcscpy(szWin32Path, szWin32Base);
	int nWin32Path = _tcslen(szWin32Path);

	CHostFilename* pFilename = NULL;	// 親フォルダの実体が入る
	CHostPath* pPath;
	const BYTE* p = szHuman;
	for (;;) {
		// ファイルいっこいれる
		BYTE szHumanFilename[24];			// ファイル名部分
		p = SeparateCopyFilename(p, szHumanFilename);
		if (p == NULL) return NULL;		// エラー: ファイル名読み込み失敗

		// 該当パスがキャッシュされているか？
		pPath = FindCache(nUnit, szHumanPath);
		if (pPath == NULL) {
			// キャッシュ最大数チェック
			if (m_nRingPath >= XM6_HOST_DIRENTRY_CACHE_MAX) {
				// 既存のオブジェクトのうち、最も古いものを取得
				pPath = (CHostPath*)m_cRingPath.Prev();
				pPath->Clean();		// 全ファイル開放 更新チェック用ハンドルも開放
			} else {
				// 新規登録
				pPath = new CHostPath;
				ASSERT(pPath);
				m_nRingPath++;
			}
			pPath->SetHuman(nUnit, szHumanPath);
			pPath->SetWin32(szWin32Path);
		}

		// キャッシュ更新チェック
		if (pPath->isRefresh()) {
			// 低速メディアでなくてもVMスレッドを実行開始
			ps->Ready();
			BOOL bResult = CheckMediaAccess(nUnit, FALSE);
			if (bResult) bResult = pPath->Refresh();	// 高コスト処理
			if (bResult == FALSE) {
				// 更新失敗時は、同一ユニットのキャッシュを全て消去する
				delete pPath;
				m_nRingPath--;
				EraseCache(nUnit);
				return NULL;		// エラー: パスが取得できない
			}
		}

		// リング先頭へ
		pPath->Insert(&m_cRingPath);

		// 親フォルダがあれば記録
		if (pFilename) pFilename->SetChild(this, pPath);

		// ファイル名がなければここで終了(パス名だけだった場合)
		// ファイル名なしで帰ってくるのは文字列終端の場合のみなので終了判定として使える
		if (szHumanFilename[0] == '\0') {
			// パス名を連結
			if (nHumanPath + 1 + 1 > HUMAN68K_MAX_PATH) return NULL;	// エラー: Human68kパスが長すぎる
			szHumanPath[nHumanPath++] = '/';
			szHumanPath[nHumanPath] = '\0';
			if (nWin32Path + 1 + 1 > _MAX_PATH) return NULL;	// エラー: Win32パスが長すぎる
			szWin32Path[nWin32Path++] = '\\';
			szWin32Path[nWin32Path] = '\0';
			break;	// 普通はここで終了
		}

		// 次のパスを検索
		// パスの途中ならディレクトリかどうか確認
		if (*p != '\0') {
			pFilename = pPath->FindFilename(szHumanFilename, Human68k::AT_DIRECTORY);
		} else {
			pFilename = pPath->FindFilename(szHumanFilename);
		}
		if (pFilename == NULL) return NULL;	// エラー: 途中のパス名/ファイル名が見つからない

		// パス名を連結
		int n = strlen((char*)szHumanFilename);
		if (nHumanPath + n + 1 > HUMAN68K_MAX_PATH) return NULL;	// エラー: Human68kパスが長すぎる
		szHumanPath[nHumanPath++] = '/';
		strcpy((char*)szHumanPath + nHumanPath, (char*)szHumanFilename);
		nHumanPath += n;

		n = _tcslen(pFilename->GetWin32());
		if (nWin32Path + n + 1 > _MAX_PATH) return NULL;	// エラー: Win32パスが長すぎる
		szWin32Path[nWin32Path++] = '\\';
		_tcscpy(szWin32Path + nWin32Path, pFilename->GetWin32());
		nWin32Path += n;

		// PLEASE CONTINUE
		if (*p == '\0') break;
	}

	// Win32名をコピー
	if (szWin32Buffer) {
		_tcscpy(szWin32Buffer, szWin32Path);
	}

	return pPath;
}

//---------------------------------------------------------------------------
//
//	ベースパス名を取得する
//
//---------------------------------------------------------------------------
TCHAR* FASTCALL CHostEntry::GetBase(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetBase();
}

//---------------------------------------------------------------------------
//
//	書き込み禁止かどうか確認する
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::isWriteProtect(CWindrv* ps) const
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->isWriteProtect();
}

//---------------------------------------------------------------------------
//
//	低速メディアチェックとオフライン状態チェック
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::isMediaOffline(CWindrv* ps, BOOL bMedia)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	// 低速デバイスならVMスレッドの動作を開始する
	if (m_ppBase[nUnit]->isSlow()) ps->Ready();

	// 手動イジェクトデバイスのアクセス事前チェック
	if (bMedia) CheckMediaAccess(nUnit, TRUE);

	// リムーバブルメディアのキャッシュ有効時間更新
	m_ppBase[nUnit]->SetTimeout();

	// 手動イジェクトデバイスは「ディスクが入っていません」にしない
	if (m_ppBase[nUnit]->isManual()) return FALSE;

	// オフライン状態チェック
	return m_ppBase[nUnit]->isEnable() == FALSE;
}

//---------------------------------------------------------------------------
//
//	メディアバイトの取得
//
//---------------------------------------------------------------------------
BYTE FASTCALL CHostEntry::GetMediaByte(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	if (m_ppBase[nUnit]->isRemovable()) {
		if (m_ppBase[nUnit]->isManual()) {
			return 0xF1;
		}
		return 0xF2;
	}
	return 0xF3;
}

//---------------------------------------------------------------------------
//
//	ドライブ状態の取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostEntry::GetStatus(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetStatus();
}

//---------------------------------------------------------------------------
//
//	全ドライブのタイムアウトチェック
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::CheckTimeout()
{
	ASSERT(this);
	ASSERT(m_ppBase);

#ifndef XM6_HOST_STRICT_TIMEOUT
	// タイムアウトを確認
	DWORD nCount = ::GetTickCount();
	DWORD nDelta = nCount - m_nTimeout;
	if (nDelta < XM6_HOST_REMOVABLE_CHECK_TIME) return;
	m_nTimeout = nCount;
#endif // XM6_HOST_STRICT_TIMEOUT

	for (DWORD n = 0; n < CWinFileSys::DrvMax; n++) {
		if (m_ppBase[n]) {
			BOOL bResult = m_ppBase[n]->CheckTimeout();
			if (bResult) {
				LockCache();
				EraseCache(n);
				UnlockCache();
			}
		}
	}
}

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
//---------------------------------------------------------------------------
//
//	リムーバブルメディアの状態更新を有効にする
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::SetMediaUpdate(CWindrv* ps, BOOL bDisable)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	m_ppBase[nUnit]->SetMediaUpdate(bDisable);
}
#endif // XM6_HOST_UPDATE_BY_SEQUENCE

//---------------------------------------------------------------------------
//
//	リムーバブルメディアの状態更新チェック
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::CheckMediaUpdate(CWindrv* ps)
{
	ASSERT(this);
	ASSERT(ps);
	ASSERT(m_ppBase);

	DWORD nUnit = ps->GetUnit();
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	// 更新判定 レベル1
	BOOL bResult = m_ppBase[nUnit]->CheckMediaUpdate();
	if (bResult) {
		// 更新判定 レベル2
		bResult = m_ppBase[nUnit]->CheckMediaAccess(FALSE);
		if (bResult) {
			// 低速メディアチェック
			if (m_ppBase[nUnit]->isSlow()) ps->Ready();

			// メディア有効チェック
			bResult = m_ppBase[nUnit]->CheckMedia();
			if (bResult == FALSE) {
				// メディア使用不能時はキャッシュクリア
				LockCache();
				EraseCache(nUnit);
				UnlockCache();
			}
		}
	}

	return m_ppBase[nUnit]->isEnable();
}

//---------------------------------------------------------------------------
//
//	リムーバブルメディアのアクセス事前チェック
//
//	呼び出し元により、それぞれ以下のような処理を行なう
//	1: MakeCache()			低速判定不要/クリア不要/手動イジェクトメディアのみ
//	2: isMediaOffline()		低速判定不要/クリア必要/手動イジェクトメディアのみ
//	3: CheckMediaUpdate()に統合済	低速判定必要/クリア必要/リムーバブルメディア全て
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::CheckMediaAccess(DWORD nUnit, BOOL bErase)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	// 更新判定
	BOOL bResult = m_ppBase[nUnit]->CheckMediaAccess(TRUE);
	if (bResult) {
		// メディア有効チェック
		bResult = m_ppBase[nUnit]->CheckMedia();
		if (bResult == FALSE) {
			// メディア使用不能時はキャッシュクリア
			if (bErase) {
				LockCache();
				EraseCache(nUnit);
				UnlockCache();
			}
		}
	}

	return m_ppBase[nUnit]->isEnable();
}

//---------------------------------------------------------------------------
//
//	イジェクト
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::Eject(DWORD nUnit)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	BOOL bResult = m_ppBase[nUnit]->Eject();
	if (bResult) {
		m_ppBase[nUnit]->CheckMedia();
	}

	return bResult;
}

//---------------------------------------------------------------------------
//
//	ボリュームラベルの取得
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::GetVolume(DWORD nUnit, TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	m_ppBase[nUnit]->GetVolume(szLabel);
}

//---------------------------------------------------------------------------
//
//	キャッシュからボリュームラベルを取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::GetVolumeCache(DWORD nUnit, TCHAR* szLabel)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetVolumeCache(szLabel);
}

//---------------------------------------------------------------------------
//
//	容量の取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostEntry::GetCapacity(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetCapacity(pCapacity);
}

//---------------------------------------------------------------------------
//
//	キャッシュからクラスタサイズを取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostEntry::GetCapacityCache(DWORD nUnit, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(m_ppBase);
	ASSERT(nUnit < CWinFileSys::DrvMax);
	ASSERT(m_ppBase[nUnit]);

	return m_ppBase[nUnit]->GetCapacityCache(pCapacity);
}

//---------------------------------------------------------------------------
//
//	ファイルシステム状態通知
//
//---------------------------------------------------------------------------
void FASTCALL CHostEntry::ShellNotify(DWORD nEvent, const TCHAR* szPath)
{
	ASSERT(this);
	ASSERT(m_ppBase);

	// 該当するユニットを検索 (重複指定も考慮する)
	for (int n = 0; n < CWinFileSys::DrvMax; n++) {
		if (m_ppBase[n]) {
			if (m_ppBase[n]->isSameDrive(szPath)) {
				// 排出
				if (nEvent & (SHCNE_MEDIAREMOVED | SHCNE_DRIVEREMOVED)) {
					m_ppBase[n]->SetEnable(FALSE);
				}

				// 挿入
				if (nEvent & (SHCNE_MEDIAINSERTED | SHCNE_DRIVEADD)) {
					m_ppBase[n]->SetEnable(TRUE);
				}

				// 該当するキャッシュを消去
				LockCache();
				EraseCache(n);
				UnlockCache();
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	Human68kフルパス名から最後の要素を分離
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL CHostEntry::SeparatePath(const BYTE* szHuman)
{
	ASSERT(szHuman);

	DWORD nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p;
	if (c != '/' && c != '\\') return NULL;	// エラー: 不正なパス名

	// 文字列中の最後の / もしくは \ を検索する。
	const BYTE* pCut = NULL;
	for (;;) {
		c = *p;
		if (c == '\0') break;
		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
			p++;
			if (*p == '\0') break;
		}
		if (c == '/' || c == '\\') {
			if (pCut == p - 1) return NULL;	// 2文字連続して区切り文字があったらエラー
			pCut = p;
		}
		p++;
	}

	// 分離可能かどうか確認
	if (pCut == NULL) return NULL;			// そもそも文字が見つからない場合はエラー
	//if (pCut == szHuman) return NULL;		// 先頭(ルート)だった場合 特別扱いしない
	//if (pCut[1] == '\0') return NULL;		// ファイル名部分がない場合 特別扱いしない
	if (strlen((char*)pCut + 1) > nMax) return NULL;	// ファイル名部分が長すぎる場合はエラー

	return pCut;
}

//---------------------------------------------------------------------------
//
//	Human68kフルパス名から先頭の要素を分離・コピー
//
//	Human68kパスは必ず / で開始すること。
//	途中 / が2つ以上連続して出現した場合はエラーとする。
//	文字列終端が / だけの場合、空の文字列として処理する。エラーにはしない。
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL CHostEntry::SeparateCopyFilename(const BYTE* szHuman, BYTE* szBuffer)
{
	ASSERT(szHuman);
	ASSERT(szBuffer);

	DWORD nMax = 22;
	const BYTE* p = szHuman;

	BYTE c = *p;
	if (c != '/' && c != '\\') return NULL;	// エラー: 不正なパス名
	p++;

	// ファイルいっこいれる
	BYTE* pWrite = szBuffer;
	DWORD i = 0;
	for (;;) {
		c = *p;								// 一文字読み込み 進める
		if (c == '\0') break;				// 文字列終端なら終了
		if (c == '/' || c == '\\') {
			if (pWrite == szBuffer) return NULL;	// エラー: パス区切り文字が連続している
			break;	// パスの区切りを読んだら終了
		}
		p++;

		if (i >= nMax) return NULL;	// エラー: 1バイト目がバッファ終端にかかる
		i++;								// 書き込む前に終端チェック
		*pWrite++ = c;						// 一文字書き込み

		if ((0x80 <= c && c <= 0x9F) || 0xE0 <= c) {	// 厳密には 0x81〜0x9F 0xE0〜0xEF
			c = *p;							// 一文字読み込み 進める
			if (c < 0x40) return NULL;		// エラー: 不正なSJIS2バイト目
			p++;

			if (i >= nMax) return NULL;	// エラー: 2バイト目がバッファ終端にかかる
			i++;							// 書き込む前に終端チェック
			*pWrite++ = c;					// 一文字書き込み
		}
	}
	*pWrite = '\0';

	return p;
}

//===========================================================================
//
//	ファイル検索処理
//
//	Human68k側のファイル名を内部Unicodeで処理するのは正直キツい。と
//	いうわけで、全てBYTEに変換して処理する。変換処理はディレクトリエ
//	ントリキャッシュが一手に担い、WINDRV側はすべてシフトJISのみで扱
//	えるようにする。
//	また、Human68k側名称は、完全にベースパス指定から独立させる。
//
//	ファイルを扱う直前に、ディレクトリエントリのキャッシュを生成する。
//	ディレクトリエントリの生成処理は高コストのため、キャッシュ構築の
//	直前でVMスレッドの動作を開始させる。
//
//	ファイル検索は3方式。すべてCHostFiles::Find()で処理する。
//	1. パス名のみ検索 属性はディレクトリのみ	_CHKDIR _CREATE
//	2. パス名+ファイル名+属性の検索 _OPEN
//	3. パス名+ワイルドカード+属性の検索 _FILES _NFILES
//	検索結果は、ディレクトリエントリ情報として保持しておく。
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	クリア
//
//---------------------------------------------------------------------------
void FASTCALL CHostFiles::Clear()
{
	ASSERT(this);

	m_nKey = 0;
#if 0
	// 検索条件: SetPath()で設定されるため不要
	m_nHumanUnit = 0;
	m_szHumanPath[0] = '\0';
	m_szHumanFilename[0] = '\0';
	m_nHumanWildcard = 0;
	m_nHumanAttribute = 0;
	m_findNext.Clear();
#endif
	memset(&m_dirEntry, 0, sizeof(m_dirEntry));
	m_szHumanResult[0] = '\0';
	m_szWin32Result[0] = _T('\0');
}

//---------------------------------------------------------------------------
//
//	パス名・ファイル名を内部で生成
//
//---------------------------------------------------------------------------
void FASTCALL CHostFiles::SetPath(DWORD nUnit, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	m_nHumanUnit = nUnit;
	pNamests->GetCopyPath(m_szHumanPath);
	pNamests->GetCopyFilename(m_szHumanFilename);
	m_nHumanWildcard = 0;
	m_nHumanAttribute = Human68k::AT_ARCHIVE;
	m_findNext.Clear();
}

//---------------------------------------------------------------------------
//
//	Win32名を検索 (パス名 + ファイル名(省略可) + 属性)
//
//	あらかじめ全てのHuman68k用パラメータを設定しておくこと。
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostFiles::Find(CWindrv* ps, CHostEntry* pEntry, BOOL bRemove)
{
	ASSERT(this);
	ASSERT(pEntry);

	// 排他制御 開始
	pEntry->LockCache();

	// パス名獲得およびキャッシュ構築
	CHostPath* pPath;
#ifdef XM6_HOST_STRICT_CACHE_CHECK
	pPath = pEntry->MakeCache(ps, m_nHumanUnit, m_szHumanPath, m_szWin32Result);
#else // XM6_HOST_STRICT_CACHE_CHECK
	pPath = pEntry->CopyCache(m_nHumanUnit, m_szHumanPath, m_szWin32Result);
	if (pPath == NULL) {
		pPath = pEntry->MakeCache(ps, m_nHumanUnit, m_szHumanPath, m_szWin32Result);
	}
#endif // XM6_HOST_STRICT_CACHE_CHECK
	if (pPath == NULL) {
		pEntry->UnlockCache();
		return FALSE;	// エラー: キャッシュ構築失敗
	}

	// ファイル名がなければ終了
	if (m_nHumanWildcard == 0xFF) {
		_tcscpy(m_szWin32Result, pPath->GetWin32());
		_tcscat(m_szWin32Result, _T("\\"));
		pEntry->UnlockCache();
		return TRUE;	// 正常終了: パス名のみ
	}

	// ファイル名獲得
	CHostFilename* pFilename;
	if (m_nHumanWildcard == 0) {
		pFilename = pPath->FindFilename(m_szHumanFilename, m_nHumanAttribute);
	} else {
		pFilename = pPath->FindFilenameWildcard(m_szHumanFilename, &m_findNext, m_nHumanAttribute);
	}
	if (pFilename == NULL) {
		pEntry->UnlockCache();
		return FALSE;	// エラー: ファイル名が獲得できません
	}

	// ディレクトリ削除の場合はここでハンドルを開放させる
	if (bRemove) pFilename->DeleteCache();

	// Human68kファイル情報保存
	memcpy(&m_dirEntry, pFilename->GetEntry(), sizeof(m_dirEntry));

	// Human68kファイル名保存
	strcpy((char*)m_szHumanResult, (char*)pFilename->GetHuman());

	// Win32フルパス名保存
	_tcscpy(m_szWin32Result, pPath->GetWin32());
	_tcscat(m_szWin32Result, _T("\\"));
	_tcscat(m_szWin32Result, pFilename->GetWin32());

	// 排他制御 終了
	pEntry->UnlockCache();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Win32名にファイル名を追加
//
//---------------------------------------------------------------------------
void FASTCALL CHostFiles::AddFilename()
{
	ASSERT(this);
	ASSERT(_tcslen(m_szWin32Result) + strlen((char*)m_szHumanFilename) < _MAX_PATH);

	// WARNING: Unicode未対応。いずれUnicodeの世界に飮まれた時はここで変換を行なう
	_tcscat(m_szWin32Result, (TCHAR*)m_szHumanFilename);
}

//===========================================================================
//
//	ファイル検索領域 マネージャ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ファイル検索領域 初期化 (起動直後)
//
//---------------------------------------------------------------------------
CHostFilesManager::CHostFilesManager()
{
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 確認 (終了時)
//
//---------------------------------------------------------------------------
CHostFilesManager::~CHostFilesManager()
{
#ifdef _DEBUG
	// 実体が存在しないことを確認
	ASSERT(m_cRingFiles.Next() == &m_cRingFiles);
	ASSERT(m_cRingFiles.Prev() == &m_cRingFiles);

	// 念のため(実際には使われない)
	Clean();
#endif // _DEBUG
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 初期化 (ドライバ組込み時)
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilesManager::Init()
{
	ASSERT(this);

	// 実体が存在しないことを確認 (念のため)
	ASSERT(m_cRingFiles.Next() == &m_cRingFiles);
	ASSERT(m_cRingFiles.Prev() == &m_cRingFiles);

	// 実体を確保
	for (int i = 0; i < XM6_HOST_FILES_MAX; i++) {
		CHostFiles* p = new CHostFiles;
		ASSERT(p);
		p->InsertTail(&m_cRingFiles);
	}
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 開放 (起動・リセット時)
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilesManager::Clean()
{
	ASSERT(this);

	// 実体を開放
	CHostFiles* p;
	while ((p = (CHostFiles*)m_cRingFiles.Next()) != &m_cRingFiles) {
		delete p;
	}
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 確保
//
//---------------------------------------------------------------------------
CHostFiles* FASTCALL CHostFilesManager::Alloc(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	// 末尾の1つを選択
	CHostFiles* p = (CHostFiles*)m_cRingFiles.Prev();

	// 該当オブジェクトをリング先頭へ移動
	p->Insert(&m_cRingFiles);

	// キーを設定
	p->SetKey(nKey);

	return p;
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 検索
//
//---------------------------------------------------------------------------
CHostFiles* FASTCALL CHostFilesManager::Search(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	// 該当するオブジェクトを検索
	for (CHostFiles* p = (CHostFiles*)m_cRingFiles.Next();
		 p != &m_cRingFiles; p = (CHostFiles*)p->Next()) {
		if (p->isSameKey(nKey)) {
			// 該当オブジェクトをリング先頭へ移動
			p->Insert(&m_cRingFiles);
			return p;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	ファイル検索領域 開放
//
//---------------------------------------------------------------------------
void FASTCALL CHostFilesManager::Free(CHostFiles* p)
{
	ASSERT(this);
	ASSERT(p);

	// 実体の初期化 (再利用するため領域開放は行なわない)
	p->Free();

	// 該当オブジェクトをリング末尾へ移動
	p->InsertTail(&m_cRingFiles);
}

//===========================================================================
//
//	FCB処理
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	クリア
//
//---------------------------------------------------------------------------
void FASTCALL CHostFcb::Clear()
{
	ASSERT(this);

	m_nKey = 0;
	m_nMode = 0;
	m_hFile = INVALID_HANDLE_VALUE;
	m_szFilename[0] = _T('\0');
}

//---------------------------------------------------------------------------
//
//	ファイルオープンモードの設定
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostFcb::SetOpenMode(DWORD nHumanMode)
{
	ASSERT(this);

	switch (nHumanMode & 0x0F) {
	case Human68k::OP_READ:
		m_nMode = GENERIC_READ;
		break;
	case Human68k::OP_WRITE:
		m_nMode = GENERIC_WRITE;
		break;
	case Human68k::OP_READWRITE:
		m_nMode = GENERIC_READ | GENERIC_WRITE;
		break;
	default:
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ファイル名の設定
//
//---------------------------------------------------------------------------
void FASTCALL CHostFcb::SetFilename(const TCHAR* szFilename)
{
	ASSERT(this);
	ASSERT(szFilename);
	ASSERT(_tcslen(szFilename) < _MAX_PATH);

	_tcscpy(m_szFilename, szFilename);
}

//---------------------------------------------------------------------------
//
//	ファイル作成
//
//---------------------------------------------------------------------------
HANDLE FASTCALL CHostFcb::Create(DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(this);
	ASSERT(_tcslen(m_szFilename) > 0);
	ASSERT(m_hFile == INVALID_HANDLE_VALUE);

	// 属性生成
	DWORD nAttribute = 0;
	if ((nHumanAttribute & Human68k::AT_DIRECTORY) != 0) return INVALID_HANDLE_VALUE;
	if ((nHumanAttribute & Human68k::AT_SYSTEM) != 0) nAttribute |= FILE_ATTRIBUTE_SYSTEM;
	if ((nHumanAttribute & Human68k::AT_HIDDEN) != 0) nAttribute |= FILE_ATTRIBUTE_HIDDEN;
	if ((nHumanAttribute & Human68k::AT_READONLY) != 0) nAttribute |= FILE_ATTRIBUTE_READONLY;
	if (nAttribute == 0) nAttribute = FILE_ATTRIBUTE_NORMAL;

	DWORD nShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
	DWORD nCreation = bForce ? CREATE_ALWAYS : CREATE_NEW;

	// ファイルオープン
	m_hFile = ::CreateFile(m_szFilename, m_nMode, nShare, NULL, nCreation, nAttribute, NULL);
	return m_hFile;
}

//---------------------------------------------------------------------------
//
//	ファイルオープン/ハンドル獲得
//
//---------------------------------------------------------------------------
HANDLE FASTCALL CHostFcb::Open()
{
	ASSERT(this);
	ASSERT(_tcslen(m_szFilename) > 0);

	// ファイルオープン
	if (m_hFile == INVALID_HANDLE_VALUE) {
		DWORD nShare = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
		DWORD nCreation = OPEN_EXISTING;
		DWORD nAttribute = FILE_ATTRIBUTE_NORMAL;
		m_hFile = ::CreateFile(m_szFilename, m_nMode, nShare, NULL, nCreation, nAttribute, NULL);
	}
	return m_hFile;
}

//---------------------------------------------------------------------------
//
//	ファイル読み込み
//
//	0バイト読み込みでも正常動作とする。
//	エラーの時は -1 を返す。
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostFcb::ReadFile(void* pBuffer, DWORD nSize)
{
	ASSERT(this);
	ASSERT(pBuffer);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	DWORD nResult;
	BOOL bResult = ::ReadFile(m_hFile, pBuffer, nSize, &nResult, NULL);
	if (bResult == FALSE) nResult = (DWORD)-1;

	return nResult;
}

//---------------------------------------------------------------------------
//
//	ファイル書き込み
//
//	0バイト書き込みでも正常動作とする。
//	エラーの時は -1 を返す。
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostFcb::WriteFile(void* pBuffer, DWORD nSize)
{
	ASSERT(this);
	ASSERT(pBuffer);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	DWORD nResult;
	BOOL bResult = ::WriteFile(m_hFile, pBuffer, nSize, &nResult, NULL);
	if (bResult == FALSE) nResult = (DWORD)-1;

	return nResult;
}

//---------------------------------------------------------------------------
//
//	ファイルポインタ設定
//
//	エラーの時は -1 を返す。
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostFcb::SetFilePointer(DWORD nOffset, DWORD nMode)
{
	ASSERT(this);
	ASSERT(nMode == FILE_BEGIN || nMode == FILE_CURRENT || nMode == FILE_END);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	return ::SetFilePointer(m_hFile, nOffset, NULL, nMode);
}

//---------------------------------------------------------------------------
//
//	ファイル時刻設定
//
//	エラーの時は -1 を返す。
//
//---------------------------------------------------------------------------
DWORD FASTCALL CHostFcb::SetFileTime(WORD nFatDate, WORD nFatTime)
{
	ASSERT(this);
	ASSERT(m_hFile != INVALID_HANDLE_VALUE);

	FILETIME lt;
	if (::DosDateTimeToFileTime(nFatDate, nFatTime, &lt) == 0) return (DWORD)-1;
	FILETIME ft;
	if (::LocalFileTimeToFileTime(&lt, &ft) == 0) return (DWORD)-1;

	if (::SetFileTime(m_hFile, NULL, &ft, &ft) == 0) return (DWORD)-1;
	return 0;
}

//---------------------------------------------------------------------------
//
//	ファイルクローズ
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHostFcb::Close()
{
	ASSERT(this);

	BOOL bResult = TRUE;

	// ファイルクローズ
	if (m_hFile != INVALID_HANDLE_VALUE) {
		bResult = ::CloseHandle(m_hFile);
		// エラー発生→Close→Free(内部で再度Close) という流れもあるので
		// 二重にCloseしないようにきちんと設定しておく
		m_hFile = INVALID_HANDLE_VALUE;
	}

	return bResult;
}

//===========================================================================
//
//	FCB処理 マネージャ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	FCB操作領域 初期化 (起動直後)
//
//---------------------------------------------------------------------------
CHostFcbManager::CHostFcbManager()
{
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 確認 (終了時)
//
//---------------------------------------------------------------------------
CHostFcbManager::~CHostFcbManager()
{
#ifdef _DEBUG
	// 実体が存在しないことを確認
	ASSERT(m_cRingFcb.Next() == &m_cRingFcb);
	ASSERT(m_cRingFcb.Prev() == &m_cRingFcb);

	// 念のため(実際には使われない)
	Clean();
#endif // _DEBUG
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 初期化 (ドライバ組込み時)
//
//---------------------------------------------------------------------------
void FASTCALL CHostFcbManager::Init()
{
	ASSERT(this);

	// 実体が存在しないことを確認
	ASSERT(m_cRingFcb.Next() == &m_cRingFcb);
	ASSERT(m_cRingFcb.Prev() == &m_cRingFcb);

	for (int i = 0; i < XM6_HOST_FCB_MAX; i++) {
		CHostFcb* p = new CHostFcb;
		ASSERT(p);
		p->InsertTail(&m_cRingFcb);
	}
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 開放 (起動・リセット時)
//
//---------------------------------------------------------------------------
void CHostFcbManager::Clean()
{
	ASSERT(this);

	// 実体を開放
	CHostFcb* p;
	while ((p = (CHostFcb*)m_cRingFcb.Next()) != &m_cRingFcb) {
		delete p;
	}
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 確保
//
//---------------------------------------------------------------------------
CHostFcb* FASTCALL CHostFcbManager::Alloc(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	// 末尾の1つを選択
	CHostFcb* p = (CHostFcb*)m_cRingFcb.Prev();

	// 使用中ならエラー
	if (p->isSameKey(0) == FALSE) return NULL;

	// 該当オブジェクトをリング先頭へ移動
	p->Insert(&m_cRingFcb);

	// キーを設定
	p->SetKey(nKey);

	return p;
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 検索
//
//---------------------------------------------------------------------------
CHostFcb* FASTCALL CHostFcbManager::Search(DWORD nKey)
{
	ASSERT(this);
	ASSERT(nKey != 0);

	// 該当するオブジェクトを検索
	for (CHostFcb* p = (CHostFcb*)m_cRingFcb.Next();
		 p != &m_cRingFcb; p = (CHostFcb*)p->Next()) {
		if (p->isSameKey(nKey)) {
			// 該当オブジェクトをリング先頭へ移動
			p->Insert(&m_cRingFcb);
			return p;
		}
	}

	return NULL;
}

//---------------------------------------------------------------------------
//
//	FCB操作領域 開放
//
//---------------------------------------------------------------------------
void FASTCALL CHostFcbManager::Free(CHostFcb* p)
{
	ASSERT(this);
	ASSERT(p);

	// 実体の初期化 (再利用するため領域開放は行なわない)
	p->Free();

	// 該当オブジェクトをリング末尾へ移動
	p->InsertTail(&m_cRingFcb);
}

//===========================================================================
//
//	Windowsファイルシステム
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CWinFileSys::CWinFileSys()
{
	// ドライブオブジェクト初期化
	for (int n = 0; n < DrvMax; n++) {
		m_pDrv[n] = NULL;
	}

	// コンフィグデータ初期化
	m_bResume = FALSE;
	m_nDrives = 0;

	for (int n = 0; n < DrvMax; n++) {
		m_nFlag[n] = 0;
		m_szBase[n][0] = _T('\0');
	}

	// TwentyOneオプション監視初期化
	m_nKernel = 0;
	m_nKernelSearch = 0;

	// 動作フラグ初期化
	m_nOptionDefault = 0;
	m_nOption = 0;
	CHostFilename::SetOption(0);
	CHostPath::SetOption(0);
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
CWinFileSys::~CWinFileSys()
{
#ifdef _DEBUG
	int nDrv;

	// オブジェクト確認
	for (nDrv=0; nDrv<DrvMax; nDrv++) {
		// 存在しないことを確認
		ASSERT(!m_pDrv[nDrv]);

		// 念のため(実際には使われない)
		if (m_pDrv[nDrv]) {
			delete m_pDrv[nDrv];
			m_pDrv[nDrv] = NULL;
		}
	}
#endif // _DEBUG
}

//---------------------------------------------------------------------------
//
//	設定適用
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileSys::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);

	// コンフィグデータ適用
	m_bResume = pConfig->host_resume;
	m_nDrives = pConfig->host_drives;

	for (int n = 0; n < DrvMax; n++) {
		m_nFlag[n] = pConfig->host_flag[n];
		ASSERT(_tcslen(pConfig->host_path[n]) < _MAX_PATH);
		_tcscpy(m_szBase[n], pConfig->host_path[n]);
	}

	// 動作フラグ設定
	m_nOptionDefault = pConfig->host_option;
}

//---------------------------------------------------------------------------
//
//	$40 - 初期化
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWinFileSys::Init(CWindrv* ps, DWORD nDriveMax, const BYTE* pOption)
{
	ASSERT(this);
	ASSERT(nDriveMax < 26);

	// VMスレッドの動作を開始
	ps->Ready();

	// エラーモード設定
	::SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

	// ファイル検索領域 初期化 (ドライバ組込み時)
	m_cFiles.Init();

	// FCB操作領域 初期化 (ドライバ組込み時)
	m_cFcb.Init();

	// ディレクトリエントリ 初期化 (ドライバ組込み時)
	m_cEntry.Init(m_pDrv);

	// オプション初期化
	InitOption(pOption);

	// 全ドライブスキャンの有無を判定
	DWORD nConfigDrives = m_nDrives;
	if (m_bResume == FALSE) {
		// 全コンフィグ消去
		for (DWORD i = 0; i < DrvMax; i++) {
			m_nFlag[i] = 0;
			m_szBase[i][0] = _T('\0');
		}

		// 全ドライブスキャン
		nConfigDrives = 0;
		DWORD nBits = ::GetLogicalDrives();
		for (DWORD n = 0; n < 26; n++) {
			// ビットチェック
			if (nBits & 1) {
				// ベースパス設定
				_stprintf(m_szBase[nConfigDrives], _T("%c:\\"), 'A' + n);

				// 動作フラグ設定
				m_nFlag[nConfigDrives] = 0;

				// 次のドライブへ
				nConfigDrives++;

				// 最大数に達していれば終了
				if (nConfigDrives >= DrvMax) break;
			}

			// 次のビットへ
			nBits >>= 1;
		}
	}

	// ファイルシステムを登録
	DWORD nDrives = 0;
	for (DWORD n = 0; n < nConfigDrives; n++) {	// 全ての有効なコンフィグデータを調査
		// ベースパスが存在しない場合は無効なデバイスとみなす
		if (m_szBase[n][0] == _T('\0')) continue;

		// これ以上登録できなければ終了 (nDriveMaxが0の場合は何も登録しない)
		if (nDrives >= nDriveMax) break;

		// ホストファイルシステムを1つ生成
		ASSERT(m_pDrv[nDrives] == NULL);
		m_pDrv[nDrives] = new CWinFileDrv;
		ASSERT(m_pDrv[nDrives]);

		// 初期化
		m_pDrv[nDrives]->Init(m_szBase[n], m_nFlag[n]);

		// 次のドライブへ
		nDrives++;
	}

	// 登録したドライブ数を返す
	return nDrives;
}

//---------------------------------------------------------------------------
//
//	リセット(全クローズ)
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileSys::Reset()
{
	int nDrv;

	ASSERT(this);

	// 仮想セクタ領域初期化
	m_nHostSectorCount = 0;
	memset(m_nHostSectorBuffer, 0, sizeof(m_nHostSectorBuffer));

	// ファイル検索領域 開放 (起動・リセット時)
	m_cFiles.Clean();

	// FCB操作領域 開放 (起動・リセット時)
	m_cFcb.Clean();

	// ディレクトリエントリ 開放 (起動・リセット時)
	m_cEntry.Clean();

	// オブジェクト削除
	for (nDrv=0; nDrv<DrvMax; nDrv++) {
		if (m_pDrv[nDrv]) {
			delete m_pDrv[nDrv];
			m_pDrv[nDrv] = NULL;
		}
	}

	// TwentyOneオプション監視初期化
	m_nKernel = 0;
	m_nKernelSearch = 0;
}

//---------------------------------------------------------------------------
//
//	$41 - ディレクトリチェック
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::CheckDir(CWindrv* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	// 直後に必ず他のコマンドが実行されるため、メディアチェックは不要
	// ここでチェックするとmintのドライブ変更前に無駄なディスクアクセスが発生してしまう
	//if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
	// 直後のメディア交換チェックコマンドを有効にするためフラグを立てる
	m_cEntry.SetMediaUpdate(ps, FALSE);
#endif // XM6_HOST_UPDATE_BY_SEQUENCE

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	if (f.isRootPath()) return 0;
	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_DIRNOTFND;

	return 0;
}

//---------------------------------------------------------------------------
//
//	$42 - ディレクトリ作成
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::MakeDir(CWindrv* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	f.AddFilename();

	// ディレクトリ作成
	BOOL bResult = ::CreateDirectory(f.GetPath(), NULL);
	if (bResult == FALSE) return FS_INVALIDPATH;

	return 0;
}

//---------------------------------------------------------------------------
//
//	$43 - ディレクトリ削除
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::RemoveDir(CWindrv* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	// ディレクトリ確認
	f.SetAttribute(Human68k::AT_DIRECTORY);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_DIRNOTFND;

	// ディレクトリ削除
	if ((m_nOption & WINDRV_OPT_REMOVE) == 0) {
		BOOL bResult = ::RemoveDirectory(f.GetPath());
		if (bResult == FALSE) return FS_CANTDELETE;
	} else {
		// ディレクトリ内部が空かどうか確認
		TCHAR szBuf[_MAX_PATH];
		_tcscpy(szBuf, f.GetPath());
		_tcscat(szBuf, "\\*.*");
		WIN32_FIND_DATA w32Find;
		HANDLE hFind = ::FindFirstFile(szBuf, &w32Find);
		if (hFind != INVALID_HANDLE_VALUE) {
			for (;;) {
				if (strcmp(w32Find.cFileName, ".") != 0 &&
					strcmp(w32Find.cFileName, "..") != 0) {
					FindClose(hFind);
					return FS_CANTDELETE;
				}
				BOOL bResult = ::FindNextFile(hFind, &w32Find);
				if (bResult == FALSE) break;
			}
			FindClose(hFind);
		}

		// WARNING: Unicode対応時要修正
		char szBuffer[_MAX_PATH + 1];
		strcpy(szBuffer, f.GetPath());
		szBuffer[strlen(szBuffer) + 1] = '\0';

		SHFILEOPSTRUCT sop;
		sop.hwnd = NULL;
		sop.wFunc = FO_DELETE;
		sop.pFrom = szBuffer;
		sop.pTo = NULL;
		sop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		sop.fAnyOperationsAborted = TRUE;
		sop.hNameMappings = NULL;
		sop.lpszProgressTitle = NULL;

		int nResult = ::SHFileOperation(&sop);
		if (nResult != 0) return FS_CANTDELETE;
	}

	// キャッシュ削除
	f.Find(ps, &m_cEntry, TRUE);

	return 0;
}

//---------------------------------------------------------------------------
//
//	$44 - ファイル名変更
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Rename(CWindrv* ps, const Human68k::namests_t* pNamests, const Human68k::namests_t* pNamestsNew)
{
	ASSERT(this);
	ASSERT(pNamests);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	CHostFiles fNew;
	fNew.SetPath(nUnit, pNamestsNew);
	fNew.SetPathOnly();
	if (fNew.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	fNew.AddFilename();

	// ファイル名変更
	BOOL bResult = ::MoveFile(f.GetPath(), fNew.GetPath());
	if (bResult == FALSE) return FS_FILENOTFND;

	return 0;
}

//---------------------------------------------------------------------------
//
//	$45 - ファイル削除
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Delete(CWindrv* ps, const Human68k::namests_t* pNamests)
{
	ASSERT(this);
	ASSERT(pNamests);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	// ファイル確認
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	// ファイル削除
	if ((m_nOption & WINDRV_OPT_REMOVE) == 0) {
		BOOL bResult = ::DeleteFile(f.GetPath());
		if (bResult == FALSE) return FS_CANTDELETE;
	} else {
		// WARNING: Unicode対応時要修正
		char szBuffer[_MAX_PATH + 1];
		strcpy(szBuffer, f.GetPath());
		szBuffer[strlen(szBuffer) + 1] = '\0';

		SHFILEOPSTRUCT sop;
		sop.hwnd = NULL;
		sop.wFunc = FO_DELETE;
		sop.pFrom = szBuffer;
		sop.pTo = NULL;
		sop.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
		sop.fAnyOperationsAborted = TRUE;
		sop.hNameMappings = NULL;
		sop.lpszProgressTitle = NULL;

		int nResult = ::SHFileOperation(&sop);
		if (nResult != 0) return FS_CANTDELETE;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	$46 - ファイル属性取得/設定
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Attribute(CWindrv* ps, const Human68k::namests_t* pNamests, DWORD nHumanAttribute)
{
	ASSERT(this);
	ASSERT(pNamests);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);
	f.SetAttribute(Human68k::AT_ALL);
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	// 属性取得なら終了
	if (nHumanAttribute == 0xFF) {
		return f.GetAttribute();
	}

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

#if 0
	if (f.GetAttribute() & Human68k::AT_DIRECTORY) {
		if ((nHumanAttribute & Human68k::AT_DIRECTORY) == 0) {
			OutputDebugString("Warning: ディレクトリの属性をファイルに変更できません\r\n");
		}
	}

	if ((f.GetAttribute() & Human68k::AT_DIRECTORY) == 0) {
		if (nHumanAttribute & Human68k::AT_DIRECTORY) {
			OutputDebugString("Warning: ファイルの属性をディレクトリに変更できません\r\n");
		}
	}
#endif

	// 属性生成
	DWORD nAttribute = 0;
	if ((nHumanAttribute & Human68k::AT_SYSTEM) != 0) nAttribute |= FILE_ATTRIBUTE_SYSTEM;
	if ((nHumanAttribute & Human68k::AT_HIDDEN) != 0) nAttribute |= FILE_ATTRIBUTE_HIDDEN;
	if ((nHumanAttribute & Human68k::AT_READONLY) != 0) nAttribute |= FILE_ATTRIBUTE_READONLY;
	if (nAttribute == 0) nAttribute = FILE_ATTRIBUTE_NORMAL;

	// 属性設定
	BOOL bResult = ::SetFileAttributes(f.GetPath(), nAttribute);
	if (bResult == FALSE) return FS_FILENOTFND;

	// 変更後の属性取得
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;
	return f.GetAttribute();
}

//---------------------------------------------------------------------------
//
//	$47 - ファイル検索(First)
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Files(CWindrv* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFiles);

	// 既に同じキーを持つ領域があれば開放しておく
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles != NULL) {
		m_cFiles.Free(pHostFiles);
	}

	// ボリュームラベルの場合
	if (pFiles->fatr == Human68k::AT_VOLUME) {
		// バッファを確保せず、いきなり結果を返す
		if (FilesVolume(ps, pFiles) == FALSE) {
			return FS_FILENOTFND;
		}
		return 0;
	}

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// バッファ確保
	pHostFiles = m_cFiles.Alloc(nKey);
	if (pHostFiles == NULL) {
		return FS_OUTOFMEM;
	}

	// ディレクトリチェック
	DWORD nUnit = ps->GetUnit();
	pHostFiles->SetPath(nUnit, pNamests);
	if (pHostFiles->isRootPath() == FALSE) {
		pHostFiles->SetPathOnly();
		if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
			m_cFiles.Free(pHostFiles);
			return FS_DIRNOTFND;
		}
	}

	// ワイルドカード使用可能に設定
	pHostFiles->SetPathWildcard();
	pHostFiles->SetAttribute(pFiles->fatr);

	// ファイル検索
	if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	// 検索結果を格納
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (char*)pHostFiles->GetHumanResult());

	// 擬似ディレクトリエントリを指定
	pFiles->sector = nKey;
	pFiles->offset = 0;

	// ファイル名にワイルドカードがなければ、この時点でバッファを開放可�
	if (pNamests->wildcard == 0) {
		// しかし、仮想セクタのエミュレーションで使う可能性があるため、すぐには開放しない
		//m_cFiles.Free(pHostFiles);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	$48 - ファイル検索(Next)
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::NFiles(CWindrv* ps, DWORD nKey, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFiles);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// バッファ検索
	CHostFiles* pHostFiles = m_cFiles.Search(nKey);
	if (pHostFiles == NULL) {
		return FS_FILENOTFND;
	}

	// ファイル検索
	if (pHostFiles->Find(ps, &m_cEntry) == FALSE) {
		m_cFiles.Free(pHostFiles);
		return FS_FILENOTFND;
	}

	// 検索結果を格納
	pFiles->attr = (BYTE)pHostFiles->GetAttribute();
	pFiles->date = pHostFiles->GetDate();
	pFiles->time = pHostFiles->GetTime();
	pFiles->size = pHostFiles->GetSize();
	strcpy((char*)pFiles->full, (char*)pHostFiles->GetHumanResult());

	return 0;
}

//---------------------------------------------------------------------------
//
//	$49 - ファイル新規作成
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Create(CWindrv* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nHumanAttribute, BOOL bForce)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// 既に同じキーを持つ領域があればエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb != NULL) return FS_FILEEXIST;

	// ファイル上書きチェック
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	// パス名生成
	f.SetPathOnly();
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_INVALIDPATH;
	f.AddFilename();

	// パス名保存
	pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL) return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());

	// オープンモード設定
	pFcb->mode = (pFcb->mode & ~0x0F) | Human68k::OP_READWRITE;
	if (pHostFcb->SetOpenMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// ファイル作成
	HANDLE hFile = pHostFcb->Create(nHumanAttribute, bForce);
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_FILEEXIST;
	}

#ifdef XM6_HOST_STRICT_CLOSE
	// すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return 0;
}

//---------------------------------------------------------------------------
//
//	$4A - ファイルオープン
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Open(CWindrv* ps, const Human68k::namests_t* pNamests, DWORD nKey, Human68k::fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(pNamests);
	ASSERT(nKey);
	ASSERT(pFcb);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	switch (pFcb->mode) {
	case Human68k::OP_WRITE:
	case Human68k::OP_READWRITE:
		if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;
	}

	// 既に同じキーを持つ領域があればエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb != NULL) return FS_FILEEXIST;

	// パス名生成
	CHostFiles f;
	DWORD nUnit = ps->GetUnit();
	f.SetPath(nUnit, pNamests);

	// ファイル情報取得
	if (f.Find(ps, &m_cEntry) == FALSE) return FS_FILENOTFND;

	// タイムスタンプ
	pFcb->date = f.GetDate();
	pFcb->time = f.GetTime();

	// ファイルサイズ
	pFcb->size = f.GetSize();

	// パス名保存
	pHostFcb = m_cFcb.Alloc(nKey);
	if (pHostFcb == NULL) return FS_OUTOFMEM;
	pHostFcb->SetFilename(f.GetPath());

	// オープンモード設定
	if (pHostFcb->SetOpenMode(pFcb->mode) == FALSE) {
		m_cFcb.Free(pHostFcb);
		return FS_ILLEGALMOD;
	}

	// ファイルオープン
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_INVALIDPATH;
	}

#ifdef XM6_HOST_STRICT_CLOSE
	// すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return 0;
}

//---------------------------------------------------------------------------
//
//	$4B - ファイルクローズ
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Close(CWindrv* ps, DWORD nKey, Human68k::fcb_t* pFcb)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_INVALIDPRM;

	// ファイルクローズと領域開放
	//pHostFcb->Close();	// Free時に自動実行されるので不要
	m_cFcb.Free(pHostFcb);

	pFcb = NULL;

	return 0;
}

//---------------------------------------------------------------------------
//
//	$4C - ファイル読み込み
//
//	0バイト読み込みでも正常終了する。
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Read(CWindrv* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	// ファイルオープン/ハンドル獲得
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	DWORD nResult;
#ifdef XM6_HOST_STRICT_CLOSE
	// ファイルポインタ復元
	nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}
#endif // XM6_HOST_STRICT_CLOSE

	// ファイル読み込み
	DWORD nTotal = 0;
	BYTE chBuffer[XM6_HOST_FILE_BUFFER_READ];
	for (DWORD nOffset = 0; nOffset < nSize; nOffset += XM6_HOST_FILE_BUFFER_READ) {
		// 扱うサイズが大きい場合はVMスレッドの動作を開始させる
		if (nOffset == XM6_HOST_FILE_BUFFER_READ) ps->Ready();

		// サイズ決定
		DWORD n = nSize - nOffset;
		if (n > XM6_HOST_FILE_BUFFER_READ) n = XM6_HOST_FILE_BUFFER_READ;

		// 部分読み込み
		nResult = pHostFcb->ReadFile(chBuffer, n);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_INVALIDFUNC;
		}

		ps->LockXM();

#if 0
		// 8ビット単位でデータ転送
		for (DWORD i = 0; i < nResult; i++) pMemory->WriteByte(nAddress++, chBuffer[i]);
#else
		// 先頭が奇数アドレスなら最初の1バイト転送
		BYTE* pBuffer = chBuffer;
		DWORD nEnd = nAddress + nResult;
		if (nAddress < nEnd && (nAddress & 1) != 0)
			pMemory->WriteByte(nAddress++, *pBuffer++);

		// 16ビット単位でデータ転送
		DWORD nEndWord = nEnd & ~1;
		while (nAddress < nEndWord) {
			DWORD nData = (*pBuffer++)<<8;
			nData |= *pBuffer++;
			pMemory->WriteWord(nAddress, nData);
			nAddress += 2;
		}

		// データが残っていれば(末尾が偶数アドレスなので)最後の1バイト転送
		if (nAddress < nEnd) pMemory->WriteByte(nAddress++, *pBuffer);
#endif

		ps->UnlockXM();

		// サイズ集計
		nTotal += nResult;

		// ファイル終端なら終了
		if (nResult != n) break;
	}

	// ファイルポインタ保存
	pFcb->fileptr += nTotal;

#ifdef XM6_HOST_STRICT_CLOSE
	// すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return nTotal;
}

//---------------------------------------------------------------------------
//
//	$4D - ファイル書き込み
//
//	0バイト書き込みでも正常終了する。
//	WARNING: バスエラーが発生するアドレスを指定した場合、実機と動作が異なる可能性あり
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Write(CWindrv* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nAddress, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	// ファイルオープン/ハンドル獲得
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	DWORD nResult;
#ifdef XM6_HOST_STRICT_CLOSE
	// ファイルポインタ復元
	nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}
#endif // XM6_HOST_STRICT_CLOSE

	// ファイル書き込み
	DWORD nTotal = 0;
	BYTE chBuffer[XM6_HOST_FILE_BUFFER_WRITE];
	for (DWORD nOffset = 0; nOffset < nSize; nOffset += XM6_HOST_FILE_BUFFER_WRITE) {
		// 扱うサイズが大きい場合はVMスレッドの動作を開始させる
		if (nOffset == XM6_HOST_FILE_BUFFER_WRITE) ps->Ready();

		// サイズ決定
		DWORD n = nSize - nOffset;
		if (n > XM6_HOST_FILE_BUFFER_WRITE) n = XM6_HOST_FILE_BUFFER_WRITE;

		ps->LockXM();

#if 0
		// データ転送
		for (DWORD i = 0; i < n; i++) chBuffer[i] = (BYTE)pMemory->ReadOnly(nAddress++);
#else
		// 先頭が奇数アドレスなら最初の1バイト転送
		BYTE* pBuffer = chBuffer;
		DWORD nEnd = nAddress + n;
		if (nAddress < nEnd && (nAddress & 1) != 0)
			*pBuffer++ = (BYTE)pMemory->ReadOnly(nAddress++);

		// 16ビット単位でデータ転送
		DWORD nEndWord = nEnd & ~1;
		while (nAddress < nEndWord) {
			DWORD nData = pMemory->ReadWord(nAddress);
			*pBuffer++ = (BYTE)(nData>>8);
			*pBuffer++ = (BYTE)nData;
			nAddress += 2;
		}

		// データが残っていれば(末尾が偶数アドレスなので)最後の1バイト転送
		if (nAddress < nEnd) *pBuffer = (BYTE)pMemory->ReadOnly(nAddress++);
#endif

		ps->UnlockXM();

		// 書き込み
		nResult = pHostFcb->WriteFile(chBuffer, n);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTWRITE;
		}

		// サイズ集計
		nTotal += nResult;

		// ファイル終端なら終了
		if (nResult != n) break;
	}

	// ファイルポインタ保存
	pFcb->fileptr += nTotal;

	// ファイルサイズ更新
	if (pFcb->size < pFcb->fileptr) pFcb->size = pFcb->fileptr;

#ifdef XM6_HOST_STRICT_CLOSE
	// すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return nTotal;
}

//---------------------------------------------------------------------------
//
//	$4E - ファイルシーク
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Seek(CWindrv* ps, DWORD nKey, Human68k::fcb_t* pFcb, DWORD nMode, int nOffset)
{
	ASSERT(this);
	ASSERT(pFcb);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	// 制御パラメータ
	DWORD nSeek;
	switch (nMode) {
	case Human68k::SK_BEGIN:
		nSeek = FILE_BEGIN;
		break;
	case Human68k::SK_CURRENT:
#ifdef XM6_HOST_STRICT_CLOSE
		nSeek = FILE_BEGIN;
		nOffset += pFcb->fileptr;
#else // XM6_HOST_STRICT_CLOSE
		nSeek = FILE_CURRENT;
#endif // XM6_HOST_STRICT_CLOSE
		break;
	case Human68k::SK_END:
		nSeek = FILE_END;
		break;
	default:
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	// ファイルオープン/ハンドル獲得
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	// ファイルシーク
	DWORD nResult = pHostFcb->SetFilePointer(nOffset, nSeek);
	if (nResult == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTSEEK;
	}

	// ファイルポインタ保存
	pFcb->fileptr = nResult;

#ifdef XM6_HOST_STRICT_CLOSE
	//すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return nResult;
}

//---------------------------------------------------------------------------
//
//	$4F - ファイル時刻取得/設定
//
//	結果の上位16Bitが$FFFFだとエラー。
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWinFileSys::TimeStamp(CWindrv* ps, DWORD nKey, Human68k::fcb_t* pFcb, WORD nFatDate, WORD nFatTime)
{
	ASSERT(this);
	ASSERT(nKey);
	ASSERT(pFcb);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 取得のみ
	if (nFatDate == 0 && nFatTime == 0) {
		return ((DWORD)pFcb->date << 16) | pFcb->time;
	}

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	// 既に同じキーを持つ領域がなければエラーとする
	CHostFcb* pHostFcb = m_cFcb.Search(nKey);
	if (pHostFcb == NULL) return FS_NOTOPENED;

	// Human68kでは、読み込みオープン時でもタイムスタンプ操作が可能になっているため
	// 読み込みオンリーモードで開いていたら、一時的に書き込みモードで開きなおす
	BOOL bReopen = FALSE;
	if ((pFcb->mode & 0x0F) == Human68k::OP_READ) {
		bReopen = TRUE;
#ifndef XM6_HOST_STRICT_CLOSE
		pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE
		pHostFcb->SetOpenMode(Human68k::OP_READWRITE);
	}

	// ファイルオープン/ハンドル獲得
	HANDLE hFile = pHostFcb->Open();
	if (hFile == INVALID_HANDLE_VALUE) {
		m_cFcb.Free(pHostFcb);
		return FS_NOTOPENED;
	}

	// 時刻設定
	if (pHostFcb->SetFileTime(nFatDate, nFatTime) == (DWORD)-1) {
		m_cFcb.Free(pHostFcb);
		return FS_CANTWRITE;
	}

	// 一時的に書き込みモードで開きなおした場合は元に戻す
	if (bReopen) {
		pHostFcb->SetOpenMode(pFcb->mode);
#ifndef XM6_HOST_STRICT_CLOSE
		pHostFcb->Close();
		hFile = pHostFcb->Open();
		if (hFile == INVALID_HANDLE_VALUE) {
			m_cFcb.Free(pHostFcb);
			return FS_NOTOPENED;
		}
		DWORD nResult = pHostFcb->SetFilePointer(pFcb->fileptr);
		if (nResult == (DWORD)-1) {
			m_cFcb.Free(pHostFcb);
			return FS_CANTSEEK;
		}
#endif // XM6_HOST_STRICT_CLOSE
	}

#ifdef XM6_HOST_STRICT_CLOSE
	// すぐ閉じる
	pHostFcb->Close();
#endif // XM6_HOST_STRICT_CLOSE

	return 0;
}

//---------------------------------------------------------------------------
//
//	$50 - 容量取得
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::GetCapacity(CWindrv* ps, Human68k::capacity_t* pCapacity)
{
	ASSERT(this);
	ASSERT(pCapacity);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 容量取得
	DWORD nUnit = ps->GetUnit();
	return m_cEntry.GetCapacity(nUnit, pCapacity);
}

//---------------------------------------------------------------------------
//
//	$51 - ドライブ状態検査/制御
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::CtrlDrive(CWindrv* ps, Human68k::ctrldrive_t* pCtrlDrive)
{
	ASSERT(this);
	ASSERT(pCtrlDrive);

	DWORD nUnit = ps->GetUnit();

	switch (pCtrlDrive->status) {
	case 0:		// 状態検査
	case 9:		// 状態検査2
		pCtrlDrive->status = (BYTE)m_cEntry.GetStatus(nUnit);
		return pCtrlDrive->status;

	case 1:		// イジェクト
		m_cEntry.isMediaOffline(ps, FALSE);	// イジェクト後にチェックするので事前チェック不要
		m_cEntry.Eject(nUnit);
		return 0;

	case 2:		// イジェクト禁止1 (未実装)
	case 3:		// イジェクト許可1 (未実装)
	case 4:		// メディア未挿入時にLED点滅 (未実装)
	case 5:		// メディア未挿入時にLED消灯 (未実装)
	case 6:		// イジェクト禁止2 (未実装)
	case 7:		// イジェクト許可2 (未実装)
		return 0;

	case 8:		// イジェクト検査
		return 1;
	}

	return FS_INVALIDFUNC;
}

//---------------------------------------------------------------------------
//
//	$52 - DPB取得
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::GetDPB(CWindrv* ps, Human68k::dpb_t* pDpb)
{
	ASSERT(this);
	ASSERT(pDpb);

	DWORD nUnit = ps->GetUnit();

	// セクタ情報獲得
	Human68k::capacity_t cap;
	BOOL bResult = m_cEntry.GetCapacityCache(nUnit, &cap);
	if (bResult == FALSE) {
		// メディアチェック
		m_cEntry.isMediaOffline(ps);

		// ドライブ状態取得
		m_cEntry.GetCapacity(nUnit, &cap);
	}

	// シフト数計算
	DWORD nSize = 1;
	DWORD nShift = 0;
	for (;;) {
		if (nSize >= cap.sectors) break;
		nSize <<= 1;
		nShift++;
	}

	// セクタ番号計算
	//
	// 以下の順に並べる。
	//	クラスタ0:未使用
	//	クラスタ1:FAT
	//	クラスタ2:ルートディレクトリ
	//	クラスタ3:データ領域(擬似セクタ)
	DWORD nFat = 1 * cap.sectors;
	DWORD nRoot = 2 * cap.sectors;
	DWORD nData = 3 * cap.sectors;

	// DPB設定
	pDpb->sector_size = (WORD)cap.bytes;		// + 0	1セクタ当りのバイト数
	pDpb->cluster_size = (BYTE)cap.sectors - 1;	// + 2	1クラスタ当りのセクタ数 - 1
	pDpb->shift = (BYTE)nShift;					// + 3	クラスタ→セクタのシフト数
	pDpb->fat_sector = (WORD)nFat;				// + 4	FAT の先頭セクタ番号
	pDpb->fat_max = 1;							// + 6	FAT 領域の個数
	pDpb->fat_size = (BYTE)cap.sectors;			// + 7	FAT の占めるセクタ数(複写分を除く)
	pDpb->file_max =							// + 8	ルートディレクトリに入るファイルの個数
		(WORD)(cap.sectors * cap.bytes / 0x20);
	pDpb->data_sector = (WORD)nData;		   	// +10	データ領域の先頭セクタ番号
	pDpb->cluster_max =	(WORD)cap.clusters;		// +12	総クラスタ数 + 1
	pDpb->root_sector = (WORD)nRoot;			// +14	ルートディレクトリの先頭セクタ番号
	pDpb->media = 0xF3;							// +20	メディアバイト

	// メディアバイト変更
	if (m_nOption & WINDRV_OPT_MEDIABYTE) {
		pDpb->media = m_cEntry.GetMediaByte(nUnit);
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	$53 - セクタ読み込み
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::DiskRead(CWindrv* ps, DWORD nAddress, DWORD nSector, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nAddress);

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	DWORD nUnit = ps->GetUnit();

	// セクタ数1以外の場合はエラー
	if (nSize != 1) return FS_NOTIOCTRL;

	// セクタ情報獲得
	Human68k::capacity_t cap;
	BOOL bResult = m_cEntry.GetCapacityCache(nUnit, &cap);
	if (bResult == FALSE) {
		// メディアチェック
		m_cEntry.isMediaOffline(ps);

		// ドライブ状態取得
		m_cEntry.GetCapacity(nUnit, &cap);
	}

	// 擬似ディレクトリエントリへのアクセス
	CHostFiles* pHostFiles = m_cFiles.Search(nSector);
	if (pHostFiles) {
		// 擬似ディレクトリエントリを生成
		// WARNING: リトルエンディアン専用
		Human68k::dirent_t dir;
		memcpy(&dir, pHostFiles->GetEntry(), sizeof(dir));

		// 擬似ディレクトリエントリ内にファイル実体を指す擬似セクタ番号を記録
		// なお、lzdsys では以下の式で読み込みセクタ番号を算出している。
		// (dirent.cluster - 2) * (dpb.cluster_size + 1) + dpb.data_sector
		dir.cluster = (WORD)(m_nHostSectorCount + 2);	// 擬似セクタ番号
		m_nHostSectorBuffer[m_nHostSectorCount] = nSector;	// 擬似セクタの指す実体
		m_nHostSectorCount++;
		m_nHostSectorCount %= XM6_HOST_PSEUDO_CLUSTER_MAX;

		ps->LockXM();

		// 擬似ディレクトリエントリを転送
		BYTE* p = (BYTE*)&dir;
		for (int i = 0; i < 0x20; i++) pMemory->WriteByte(nAddress++, *p++);
		for (int i = 0x20; i < 0x200; i++) pMemory->WriteByte(nAddress++, 0xFF);

		ps->UnlockXM();

		return 0;
	}

	// クラスタ番号からセクタ番号を算出
	DWORD n = nSector - (3 * cap.sectors);
	DWORD nMod = 1;
	if (cap.sectors) {
		// メディアが存在しない場合は cap.sectors が0になるので注意
		nMod = n % cap.sectors;
		n /= cap.sectors;
	}

	// ファイル実体へのアクセス
	if (nMod == 0 && n < XM6_HOST_PSEUDO_CLUSTER_MAX) {
		pHostFiles = m_cFiles.Search(m_nHostSectorBuffer[n]);	// 実体を検索
		if (pHostFiles) {
			// メディアチェック
			if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

			// 擬似セクタを生成
			CHostFcb f;
			f.SetFilename(pHostFiles->GetPath());
			f.SetOpenMode(Human68k::OP_READ);
			HANDLE hFile = f.Open();
			if (hFile == INVALID_HANDLE_VALUE) return FS_NOTIOCTRL;
			BYTE chBuffer[512];
			memset(chBuffer, 0, sizeof(chBuffer));
			DWORD nResult = f.ReadFile(chBuffer, 512);
			f.Close();
			if (nResult == (DWORD)-1) return FS_NOTIOCTRL;

			ps->LockXM();

			// 擬似セクタを転送
			for (int i = 0; i < 0x200; i++) pMemory->WriteByte(nAddress++, chBuffer[i]);

			ps->UnlockXM();

			return 0;
		}
	}

	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
//	$54 - セクタ書き込み
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::DiskWrite(CWindrv* ps, DWORD nAddress, DWORD nSector, DWORD nSize)
{
	ASSERT(this);
	ASSERT(nAddress);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 書き込み禁止チェック
	if (m_cEntry.isWriteProtect(ps)) return FS_FATAL_WRITEPROTECT;

	printf("%d %d %d", nAddress, nSector, nSize);
	// 現実を突きつける
	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
//	$55 - IOCTRL
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::IoControl(CWindrv* ps, Human68k::ioctrl_t* pIoctrl, DWORD nFunction)
{
	ASSERT(this);

	switch (nFunction) {
	case 0:
		// メディアIDの獲得
		pIoctrl->media = 0xF3;
		if (m_nOption & WINDRV_OPT_MEDIABYTE) {
			DWORD nUnit = ps->GetUnit();
			pIoctrl->media = m_cEntry.GetMediaByte(nUnit);
		}
		return 0;

	case 1:
		// Human68k互換のためのダミー
		pIoctrl->param = (DWORD)-1;
		return 0;

	case 2:
		switch (pIoctrl->param) {
		case -1:
			// メディア再認識
			m_cEntry.isMediaOffline(ps);
			return 0;

		case 0:
		case 1:
			// Human68k互換のためのダミー
			return 0;
		}
		break;

	case -1:
		// 常駐判定
		memcpy(pIoctrl->buffer, "WindrvXM", 8);
		return 0;

	case -2:
		// オプション設定
		SetOption(pIoctrl->param);
		return 0;

	case -3:
		// オプション獲得
		pIoctrl->param = GetOption();
		return 0;
	}

	return FS_NOTIOCTRL;
}

//---------------------------------------------------------------------------
//
//	$56 - フラッシュ
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Flush(CWindrv* ps)
{
	ASSERT(this);

	DWORD nUnit = ps->GetUnit();

	// フラッシュ
	m_cEntry.LockCache();
	m_cEntry.EraseCache(nUnit);
	m_cEntry.UnlockCache();

	// 常に成功
	return 0;
}

//---------------------------------------------------------------------------
//
//	$57 - メディア交換チェック
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::CheckMedia(CWindrv* ps)
{
	ASSERT(this);

	// TwentyOneオプション監視
	CheckKernel(ps);

#ifndef XM6_HOST_STRICT_TIMEOUT
	// タイムアウトチェック
	m_cEntry.CheckTimeout();
#endif // XM6_HOST_STRICT_TIMEOUT

	// 手動イジェクトメディアの状態更新チェック
	BOOL bResult = m_cEntry.CheckMediaUpdate(ps);

	// メディア未挿入ならエラーとする
	if (bResult == FALSE) {
#ifdef XM6_HOST_UPDATE_BY_SEQUENCE
		// 直後のディレクトリチェックコマンドを無効にするためフラグを立てる
		m_cEntry.SetMediaUpdate(ps, TRUE);
#endif // XM6_HOST_UPDATE_BY_SEQUENCE

		return FS_INVALIDFUNC;
	}

	return 0;
}

//---------------------------------------------------------------------------
//
//	$58 - 排他制御
//
//---------------------------------------------------------------------------
int FASTCALL CWinFileSys::Lock(CWindrv* ps)
{
	ASSERT(this);

	// メディアチェック
	if (m_cEntry.isMediaOffline(ps)) return FS_FATAL_MEDIAOFFLINE;

	// 常に成功
	return 0;
}

#if 0
//---------------------------------------------------------------------------
//
//	Win32最終エラー取得 Human68kエラーに変換
//
//---------------------------------------------------------------------------
DWORD FASTCALL CWinFileSys::GetLastError(DWORD nUnit) const
{
	ASSERT(this);
	ASSERT(m_cEntry.GetBase(nUnit));

	return FS_INVALIDFUNC;
}
#endif

//---------------------------------------------------------------------------
//
//	オプション設定
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileSys::SetOption(DWORD nOption)
{
	ASSERT(this);

	// ディレクトリエントリの再構成が必要ならキャッシュクリア
	DWORD nDiff = m_nOption ^ nOption;
	if (nDiff & 0x7F3F1F3F) {
		m_cEntry.LockCache();
		m_cEntry.CleanCache();
		m_cEntry.UnlockCache();
	}

	m_nOption = nOption;
	CHostFilename::SetOption(nOption);
	CHostPath::SetOption(nOption);
}

//---------------------------------------------------------------------------
//
//	オプション初期化
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileSys::InitOption(const BYTE* pOption)
{
	pOption += strlen((char*)pOption) + 1;

	DWORD nOption = m_nOptionDefault;
	for (;;) {
		const BYTE* p = pOption;
		BYTE c = *p++;
		if (c == '\0') break;

		DWORD nMode;
		if (c == '+') {
			nMode = 1;
		}
		else if (c == '-') {
			nMode = 0;
		} else {
			// オプション指定ではないので次へ
			pOption += strlen((char*)pOption) + 1;
			continue;
		}

		for (;;) {
			c = *p++;
			if (c == '\0') break;

			DWORD nBit = 0;
			switch (c) {
			case 'D': case 'd': nBit = WINDRV_OPT_REMOVE; break;
			case 'K': case 'k': nBit = WINDRV_OPT_TWENTYONE; break;
			case 'M': case 'm': nBit = WINDRV_OPT_MEDIABYTE; break;
			case 'A': case 'a': nBit = WINDRV_OPT_CONVERT_LENGTH; break;
			case 'T': case 't': nBit = WINDRV_OPT_COMPARE_LENGTH; nMode ^= 1; break;
			case 'C': case 'c': nBit = WINDRV_OPT_ALPHABET; break;

			case 'E': nBit = WINDRV_OPT_CONVERT_PERIOD; break;
			case 'P': nBit = WINDRV_OPT_CONVERT_PERIODS; break;
			case 'N': nBit = WINDRV_OPT_CONVERT_HYPHEN; break;
			case 'H': nBit = WINDRV_OPT_CONVERT_HYPHENS; break;
			case 'X': nBit = WINDRV_OPT_CONVERT_BADCHAR; break;
			case 'S': nBit = WINDRV_OPT_CONVERT_SPACE; break;

			case 'e': nBit = WINDRV_OPT_REDUCED_PERIOD; break;
			case 'p': nBit = WINDRV_OPT_REDUCED_PERIODS; break;
			case 'n': nBit = WINDRV_OPT_REDUCED_HYPHEN; break;
			case 'h': nBit = WINDRV_OPT_REDUCED_HYPHENS; break;
			case 'x': nBit = WINDRV_OPT_REDUCED_BADCHAR; break;
			case 's': nBit = WINDRV_OPT_REDUCED_SPACE; break;
			}

			if (nMode) {
				nOption |= nBit;
			} else {
				nOption &= ~nBit;
			}
		}

		pOption = p;
	}

	// オプション設定
	if (nOption != m_nOption) {
		SetOption(nOption);
	}
}

//---------------------------------------------------------------------------
//
//	ボリュームラベル取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL CWinFileSys::FilesVolume(CWindrv* ps, Human68k::files_t* pFiles)
{
	ASSERT(this);
	ASSERT(pFiles);

	DWORD nUnit = ps->GetUnit();

	// ボリュームラベル取得
	TCHAR szVolume[32];
	BOOL bResult = m_cEntry.GetVolumeCache(nUnit, szVolume);
	if (bResult == FALSE) {
		// メディアチェック
		m_cEntry.isMediaOffline(ps);

		// ボリュームラベル取得
		m_cEntry.GetVolume(nUnit, szVolume);
	}
	if (szVolume[0] == _T('\0')) return FALSE;

	pFiles->attr = Human68k::AT_VOLUME;
	pFiles->time = 0;
	pFiles->date = 0;
	pFiles->size = 0;

	CHostFilename fname;
	fname.SetWin32(szVolume);
	fname.SetHuman();
	strcpy((char*)pFiles->full, (char*)fname.GetHuman());

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	TwentyOneオプション監視
//
//	カーネル不備のため、やむを得ずリモートドライブ側で対処しなければ
//	ならないパラメータを自動反映する。
//
//	TODO: いつかカーネル・ドライバを修正してこの処理そのものをなくしたい
//
//---------------------------------------------------------------------------
void FASTCALL CWinFileSys::CheckKernel(CWindrv* ps)
{
	ASSERT(this);

	if ((m_nOption & WINDRV_OPT_TWENTYONE) == 0) return;

	ps->LockXM();

	Memory* pMemory = ps->GetMemory();
	ASSERT(pMemory);

	// まだ検索完了していなければHuman68kカーネル内ワークを調査する
	if (m_nKernel < 1024) {
		// 再挑戦へのカウントダウン
		if (m_nKernel > 0) {
			m_nKernel--;		// いずれ再挑戦
			goto CheckKernelExit;
		}

		// Step1: NULデバイス検索
		if (m_nKernelSearch == 0) {
			DWORD n = 0x6800;
			for (;;) {
				DWORD nData = pMemory->ReadWord(n);
				if (nData == 'NU') {
					if (pMemory->ReadWord(n + 2) == 'L ') break;
				}
				n += 2;
				if (n >= 0x20000 - 2) {
					// NULデバイス発見できず
					m_nKernel = 0xFFFFFFFF;		// カーネル異常: 二度と検索しない
					goto CheckKernelExit;
				}
			}
			n -= 14;
			m_nKernelSearch = n;
		}

		// Step2: NULデバイスを起点として全デバイス検索
		DWORD n = m_nKernelSearch;
		for (;;) {
			// 次のデバイスへ
			n = (pMemory->ReadWord(n) << 16) | pMemory->ReadWord(n + 2);
			if (n == 0xFFFFFFFF) {
				// 該当デバイスなし
				m_nKernel = XM6_HOST_TWENTYONE_CHECK_COUNT;	// 検索失敗: いずれ再挑戦
				goto CheckKernelExit;
			}

			DWORD x1 = (pMemory->ReadWord(n + 14) << 16) | pMemory->ReadWord(n + 16);

			if (x1 == '*Twe') {
				DWORD x2 = (pMemory->ReadWord(n + 18) << 16) | pMemory->ReadWord(n + 20);
				if (x2 == 'nty*') {
					// 旧バージョンを発見
					m_nKernel = 0xFFFFFFFF;		// TwentyOne旧バージョン: 二度と検索しない
					goto CheckKernelExit;
				}
				continue;
			}

			if (x1 == '?Twe') {
				DWORD x2 = (pMemory->ReadWord(n + 18) << 16) | pMemory->ReadWord(n + 20);
				if (x2 == 'nty?' || x2 == 'ntyE') {
					break;
				}
				continue;
			}
		}

		// 発見
		m_nKernel = n + 22;
	} else {
		if (m_nKernel == 0xFFFFFFFF) {
			goto CheckKernelExit;
		}
	}

	{
		// カーネル側オプション獲得
		DWORD nKernelOption =
			(pMemory->ReadWord(m_nKernel) << 16) | pMemory->ReadWord(m_nKernel + 2);

		// リモートドライブ側オプション獲得
		DWORD nOption = m_nOption &
			~(WINDRV_OPT_ALPHABET | WINDRV_OPT_COMPARE_LENGTH | WINDRV_OPT_CONVERT_LENGTH);

		// オプション反映
		if (nKernelOption & 0x40000000) {	// _TWON_C_BIT: Bit30
			nOption |= WINDRV_OPT_ALPHABET;
		}
		if (nKernelOption & 0x08000000) {	// _TWON_T_BIT: Bit27
			nOption |= WINDRV_OPT_COMPARE_LENGTH;
		}
		if (nKernelOption & 0x00400000) {	// _TWON_A_BIT: Bit22
			nOption |= WINDRV_OPT_CONVERT_LENGTH;
		}

		// オプション設定
		if (nOption != m_nOption) {
			SetOption(nOption);
		}
	}

CheckKernelExit:
	ps->UnlockXM();
}

//===========================================================================
//
//	Host
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
CHost::CHost(CFrmWnd *pWnd) : CComponent(pWnd)
{
	// コンポーネントパラメータ
	m_dwID = MAKEID('H', 'O', 'S', 'T');
	m_strDesc = _T("Host FileSystem");

	// オブジェクト
	m_pWindrv = NULL;
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
BOOL FASTCALL CHost::Init()
{
	ASSERT(this);

	// 基本クラス
	if (!CComponent::Init()) {
		return FALSE;
	}

	// Windrv取得
	ASSERT(!m_pWindrv);
	m_pWindrv = (Windrv*)::GetVM()->SearchDevice(MAKEID('W', 'D', 'R', 'V'));
	ASSERT(m_pWindrv);

	// ファイルシステム設定
	m_pWindrv->SetFileSys(&m_WinFileSys);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void FASTCALL CHost::Cleanup()
{
	ASSERT(this);

	// ファイルシステムを切り離す
	if (m_pWindrv) {
		m_pWindrv->SetFileSys(NULL);
	}

	// リセット(全クローズ)
	m_WinFileSys.Reset();

	// 基本クラス
	CComponent::Cleanup();
}

//---------------------------------------------------------------------------
//
//	設定適用
//
//---------------------------------------------------------------------------
void FASTCALL CHost::ApplyCfg(const Config* pConfig)
{
	ASSERT(this);
	ASSERT(pConfig);

	// ファイルシステムを呼び出す
	m_WinFileSys.ApplyCfg(pConfig);
}

#endif // _WIN32
