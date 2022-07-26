//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ フロッピーディスクイメージ ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "fdd.h"
#include "fdi.h"

//===========================================================================
//
//	FDIセクタ
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDISector::FDISector(BOOL mfm, const DWORD *chrn)
{
	int i;

	ASSERT(chrn);

	// CHRN,MFM記憶
	for (i=0; i<4; i++) {
		sec.chrn[i] = chrn[i];
	}
	sec.mfm = mfm;

	// その他初期化
	sec.error = FDD_NODATA;
	sec.length = 0;
	sec.gap3 = 0;
	sec.buffer = NULL;
	sec.pos = 0;
	sec.changed = FALSE;
	sec.next = NULL;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDISector::~FDISector()
{
	// メモリ解放
	if (sec.buffer) {
		delete[] sec.buffer;
		sec.buffer = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	初期ロード
//
//---------------------------------------------------------------------------
void FASTCALL FDISector::Load(const BYTE *buf, int len, int gap, int err)
{
	ASSERT(this);
	ASSERT(!sec.buffer);
	ASSERT(sec.length == 0);
	ASSERT(buf);
	ASSERT(len > 0);
	ASSERT(gap > 0);

	// レングスだけ先に設定
	sec.length = len;

	// バッファ確保
	try {
		sec.buffer = new BYTE[len];
	}
	catch (...) {
		sec.buffer = NULL;
		sec.length = 0;
	}
	if (!sec.buffer) {
		sec.length = 0;
	}

	// 転送
	memcpy(sec.buffer, buf, sec.length);

	// ワーク設定
	sec.gap3 = gap;
	sec.error = err;
	sec.changed = FALSE;
}

//---------------------------------------------------------------------------
//
//	セクタマッチするか
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDISector::IsMatch(BOOL mfm, const DWORD *chrn) const
{
	int i;

	ASSERT(this);
	ASSERT(chrn);

	// MFMを比較
	if (sec.mfm != mfm) {
		return FALSE;
	}

	// CHRを比較
	for (i=0; i<3; i++) {
		if (chrn[i] != sec.chrn[i]) {
			return FALSE;
		}
	}

	// 引数のNが!=0の場合のみ、N比較
	if (chrn[3] != 0) {
		if (chrn[3] != sec.chrn[3]) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	CHRNを取得
//
//---------------------------------------------------------------------------
void FASTCALL FDISector::GetCHRN(DWORD *chrn) const
{
	int i;

	ASSERT(this);
	ASSERT(chrn);

	for (i=0; i<4; i++) {
		chrn[i] = sec.chrn[i];
	}
}

//---------------------------------------------------------------------------
//
//	リード
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Read(BYTE *buf) const
{
	ASSERT(this);
	ASSERT(buf);

	// セクタバッファがなければ何もしない
	if (!sec.buffer) {
		return sec.error;
	}

	// 転送＋エラーを返す
	memcpy(buf, sec.buffer, sec.length);
	return sec.error;
}

//---------------------------------------------------------------------------
//
//	ライト
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Write(const BYTE *buf, BOOL deleted)
{
	ASSERT(this);
	ASSERT(buf);

	// セクタバッファがなければ何もしない
	if (!sec.buffer) {
		return sec.error;
	}

	// エラー処理を先に行う
	sec.error &= ~FDD_DATACRC;
	sec.error &= ~FDD_DDAM;
	if (deleted) {
		sec.error |= FDD_DDAM;
	}

	// 一致すれば何もしない
	if (memcmp(sec.buffer, buf, sec.length) == 0) {
		return sec.error;
	}

	// 転送
	memcpy(sec.buffer, buf, sec.length);

	// 更新フラグをたて、エラーを返す
	sec.changed = TRUE;
	return sec.error;
}

//---------------------------------------------------------------------------
//
//	フィル
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Fill(DWORD d)
{
	int i;
	BOOL changed;

	ASSERT(this);

	// セクタバッファがなければ何もしない
	if (!sec.buffer) {
		return sec.error;
	}

	// 比較しながら書き込み
	changed = FALSE;
	for (i=0; i<sec.length; i++) {
		if (sec.buffer[i] != (BYTE)d) {
			// 1回でも違ったら、フィルしてbreak
			memset(sec.buffer, d, sec.length);
			changed = TRUE;
			break;
		}
	}

	// 書き込みではデータCRCは発生しないものとする
	sec.error &= ~FDD_DATACRC;

	// 更新フラグをたて、エラーを返す
	if (changed) {
		sec.changed = TRUE;
	}
	return sec.error;
}

//===========================================================================
//
//	FDIトラック
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrack::FDITrack(FDIDisk *disk, int track, BOOL hd)
{
	ASSERT(disk);
	ASSERT((track >= 0) && (track <= 163));

	// ディスク、トラック記憶
	trk.disk = disk;
	trk.track = track;

	// その他ワークエリア
	trk.init = FALSE;
	trk.sectors[0] = 0;
	trk.sectors[1] = 0;
	trk.sectors[2] = 0;
	trk.hd = hd;
	trk.mfm = TRUE;
	trk.first = NULL;
	trk.next = NULL;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDITrack::~FDITrack()
{
	// クリア
	ClrSector();
}

//---------------------------------------------------------------------------
//
//	セーブ
//	※2HD,DIMなどのセクタ連続書き込みタイプ向け
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::Save(const Filepath& path, DWORD offset)
{
	Fileio fio;
	FDISector *sector;
	BOOL changed;

	ASSERT(this);

	// 初期化されていなければ書き込む必要なし
	if (!IsInit()) {
		return TRUE;
	}

	// セクタをまわって、書き込まれているセクタがあるか
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// どれも書き込まれていなければ何もしない
	if (!changed) {
		return TRUE;
	}

	// ファイルオープン
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// ループ
	sector = GetFirst();
	while (sector) {
		// 変更されていなければ、次へ
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// シーク
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}

		// 書き込み
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// フラグを落とす
		sector->ClrChanged();

		// 次へ
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// 終了
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//	※2HD,DIMなどのセクタ連続書き込みタイプ向け
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::Save(Fileio *fio, DWORD offset)
{
	FDISector *sector;
	BOOL changed;

	ASSERT(this);
	ASSERT(fio);

	// 初期化されていなければ書き込む必要なし
	if (!IsInit()) {
		return TRUE;
	}

	// セクタをまわって、書き込まれているセクタがあるか
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// どれも書き込まれていなければ何もしない
	if (!changed) {
		return TRUE;
	}

	// ループ
	sector = GetFirst();
	while (sector) {
		// 変更されていなければ、次へ
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// シーク
		if (!fio->Seek(offset)) {
			return FALSE;
		}

		// 書き込み
		if (!fio->Write(sector->GetSector(), sector->GetLength())) {
			return FALSE;
		}

		// フラグを落とす
		sector->ClrChanged();

		// 次へ
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// 終了
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	物理フォーマット
//	※セーブは別に行うこと
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create(DWORD phyfmt)
{
	ASSERT(this);

	// すべてのセクタを削除
	ClrSector();

	// 物理フォーマット別
	switch (phyfmt) {
		// 標準2HD
		case FDI_2HD:
			Create2HD(153);
			break;

		// オーバトラック2HD
		case FDI_2HDA:
			Create2HD(159);
			break;

		// 2HS
		case FDI_2HS:
			Create2HS();
			break;

		// 2HC
		case FDI_2HC:
			Create2HC();
			break;

		// 2HDE
		case FDI_2HDE:
			Create2HDE();
			break;

		// 2HQ
		case FDI_2HQ:
			Create2HQ();
			break;

		// N88-BASIC
		case FDI_N88B:
			CreateN88B();
			break;

		// OS-9/68000
		case FDI_OS9:
			CreateOS9();
			break;

		// 2DD
		case FDI_2DD:
			Create2DD();
			break;

		// その他
		default:
			ASSERT(FALSE);
			return;
	}

	// セクタがあるなら、後で必ずセーブさせるために、初期化＆全変更状態とする
	if (GetAllSectors() > 0) {
		trk.init = TRUE;
		ForceChanged();
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2HD, 2HDA)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HD(int lim)
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは指定数まで
	if (trk.track > lim) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x03;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス3×8セクタ(MFM)
	for (i=0; i<8; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x400, 0x74, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2HS)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HS()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは159まで
	if (trk.track > 159) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x03;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス3×9セクタ(MFM)
	for (i=0; i<9; i++) {
		// R作成(特例あり)
		if ((trk.track == 0) && (i == 0)) {
			chrn[2] = 0x01;
		}
		else {
			chrn[2] = 10 + i;
		}

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x400, 0x39, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2HC)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HC()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは159まで
	if (trk.track > 159) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス2×15セクタ(MFM)
	for (i=0; i<15; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2HDE)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HDE()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは159まで
	if (trk.track > 159) {
		return;
	}

	// C,N作成
	chrn[0] = trk.track >> 1;
	chrn[3] = 0x03;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス3×9セクタ(MFM)
	for (i=0; i<9; i++) {
		// H作成(特例あり)
		chrn[1] = 0x80 + (trk.track & 1);
		if ((trk.track == 0) && (i == 0)) {
			chrn[1] = 0x00;
		}

		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x400, 0x39, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2HQ)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HQ()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは159まで
	if (trk.track > 159) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス2×18セクタ(MFM)
	for (i=0; i<18; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(N88-BASIC)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CreateN88B()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x100];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは153まで
	if (trk.track > 153) {
		return;
	}

	// C,H作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// トラック0は例外
	if (trk.track == 0) {
		// レングス0×26セクタ(FM)
		chrn[3] = 0;
		for (i=0; i<26; i++) {
			// R作成
			chrn[2] = i + 1;

			// セクタ作成
			sector = new FDISector(FALSE, chrn);

			// データロード
			sector->Load(buf, 0x80, 0x1a, FDD_NOERROR);

			// 追加
			AddSector(sector);
		}
		return;
	}

	// レングス1×26セクタ(MFM)
	chrn[3] = 1;
	for (i=0; i<26; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x100, 0x33, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(OS-9/68000)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CreateOS9()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x100];

	ASSERT(this);
	ASSERT(trk.hd);

	// トラックは153まで
	if (trk.track > 153) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 1;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス1×26セクタ(MFM)
	for (i=0; i<26; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x100, 0x33, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	物理フォーマット(2DD)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2DD()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(!trk.hd);

	// トラックは159まで
	if (trk.track > 159) {
		return;
	}

	// C,H,N作成
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// バッファ初期化
	memset(buf, 0xe5, sizeof(buf));

	// レングス2×9セクタ(MFM)
	for (i=0; i<9; i++) {
		// R作成
		chrn[2] = i + 1;

		// セクタ作成
		sector = new FDISector(TRUE, chrn);

		// データロード
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// 追加
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	リードID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタすべてID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadID(DWORD *buf, BOOL mfm)
{
	FDISector *sector;
	DWORD pos;
	int status;
	int num;
	int match;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(trk.disk);

	// ステータス初期化
	status = FDD_NOERROR;

	// HDフラグが合わなければ、エラー処理する
	if (GetDisk()->IsHD() != trk.hd) {
		status |= FDD_MAM;
		status |= FDD_NODATA;
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// 密度が合い、ID CRCのないセクタの個数を数える
	num = 0;
	match = 0;
	sector = GetFirst();
	while (sector) {
		// 密度がマッチするか
		if (sector->IsMFM() == mfm) {
			match++;

			// ID CRCエラーがないか
			if (!(sector->GetError() & FDD_IDCRC)) {
				num++;
			}
		}
		sector = sector->GetNext();
	}

	// 密度がマッチするデータがない
	if (match == 0) {
		status |= FDD_MAM;
	}

	// ID CRCのないセクタがない
	if (num == 0) {
		status |= FDD_NODATA;
	}

	// どちらでも失敗。検索にかかる時間はインデックスホール２回検出
	if (status != FDD_NOERROR) {
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// カレントポジション以降の最初のセクタを取得。ただし密度が合うこと
	sector = GetCurSector();
	ASSERT(sector);
	for (;;) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// 密度が一致しなければスキップ
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// ID CRCならスキップ
		if (sector->GetError() & FDD_IDCRC) {
			sector = sector->GetNext();
			continue;
		}

		// 終了
		break;
	}

	// CHRNを取得
	sector->GetCHRN(buf);

	// 検索にかかる時間を設定
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// エラー無し
	return status;
}

//---------------------------------------------------------------------------
//
//	リードセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_NOCYL		検索中にIDのCが一致せず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		DATAフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;
	DWORD work[4];
	DWORD pos;
	int status;
	int i;
	int num;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// 密度が合うセクタ数を取得
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// HDフラグが合わなければ、強制的にセクタ数0とする
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// 0ならMAM,NODATA(セクタは１つもない)
	if (num == 0) {
		// 検索にかかる時間はインデックスホール２回検出
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// カレントポジション以降の最初のセクタを取得
	sector = GetCurSector();

	// Numberだけループ、セクタ検索(Rしか見ない)
	status = FDD_NOERROR;
	num = GetAllSectors();
	for (i=0; i<num; i++) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// 密度がマッチしなければ繰り返す
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// CHRNを取得、Cをチェック
		sector->GetCHRN(work);
		if (work[0] != chrn[0]) {
			if (work[0] == 0xff) {
				status |= FDD_BADCYL;
			}
			else {
				status |= FDD_NOCYL;
			}
		}

		// R一致で抜ける
		if (work[2] == chrn[2]) {
			break;
		}

		// 次のセクタへ
		sector = sector->GetNext();
	}

	// R一致しなければ、NODATAで返す
	if (work[2] != chrn[2]) {
		status |= FDD_NODATA;

		// 検索にかかる時間はインデックスホール２回検出
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// 検索にかかる時間を設定
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// bufが指定されている場合のみ、バッファへデータを入れる。NULLならステータスのみ
	*len = sector->GetLength();
	if (buf) {
		status = sector->Read(buf);
	}
	else {
		status = sector->GetError();
	}

	// ステータスをマスク
	status &= (FDD_IDCRC | FDD_DATACRC | FDD_DDAM);
	return status;
}

//---------------------------------------------------------------------------
//
//	ライトセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してCHRNが一致しない
//		FDD_NOCYL		検索中にIDのCが一致せず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, BOOL deleted)
{
	FDISector *sector;
	DWORD work[4];
	DWORD pos;
	int status;
	int i;
	int num;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// 密度が合うセクタ数を取得
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// HDフラグが合わなければ、強制的にセクタ数0とする
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// 0ならMAM,NODATA(セクタは１つもない)
	if (num == 0) {
		// 検索にかかる時間はインデックスホール２回検出
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// カレントポジション以降の最初のセクタを取得
	sector = GetCurSector();

	// Numberだけループ、セクタ検索(CHRNチェック)
	status = FDD_NOERROR;
	num = GetAllSectors();
	for (i=0; i<num; i++) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// 密度がマッチしなければ繰り返す
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// CHRNを取得、Cをチェック
		sector->GetCHRN(work);
		if (work[0] != chrn[0]) {
			if (work[0] == 0xff) {
				status |= FDD_BADCYL;
			}
			else {
				status |= FDD_NOCYL;
			}
		}

		// CHRN一致で抜ける
		if (sector->IsMatch(mfm, chrn)) {
			break;
		}

		// 次のセクタへ
		sector = sector->GetNext();
	}

	// セクタが見つからなければ、NODATA
	if (i >= num) {
		status |= FDD_NODATA;

		// 検索にかかる時間はインデックスホール２回検出
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// 検索にかかる時間を設定
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// bufが指定されている場合のみ、書き込む。NULLならステータスのみ
	*len = sector->GetLength();
	if (buf) {
		status = sector->Write(buf, deleted);
	}
	else {
		status = sector->GetError();
	}

	// ステータスをマスク
	status &= (FDD_IDCRC | FDD_DDAM);
	return status;
}

//---------------------------------------------------------------------------
//
//	リードダイアグ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		データフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;
	DWORD work[4];
	int num;
	int total;
	int start;
	int length;
	int status;
	int error;
	BYTE *ptr;
	DWORD pos;
	BOOL found;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// 密度が合うセクタ数を取得
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// HDフラグが合わなければ、強制的にセクタ数0とする
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// 0ならMAM,NODATA(セクタは１つもない)
	if (num == 0) {
		// 検索にかかる時間はインデックスホール2回検出
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// ワーク確保
	try {
		ptr = new BYTE[0x4000];
	}
	catch (...) {
		return FDD_NODATA | FDD_MAM;
	}
	if (!ptr) {
		return FDD_NODATA | FDD_MAM;
	}

	// レングスを決める。最大N=7(4000h)
	length = chrn[3];
	if (length > 7) {
		length = 7;
	}
	length = 1 << (length + 7);

	// 検索にかかる時間は先頭セクタ取得時間
	sector = GetFirst();
	pos = sector->GetPos();
	GetDisk()->SetSearch(pos);

	// ステータス初期化
	status = FDD_NOERROR;

	// GAP1作成
	total = MakeGAP1(ptr, 0);

	// ループ
	found = FALSE;
	while (sector) {
		// セクタのMFMが一致しなければ、continue
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// セクタデータを作成
		total = MakeSector(ptr, total, sector);

		// CHRN取得
		sector->GetCHRN(work);

		// R, Nともに一致した場合のみ、found
		if (work[2] == chrn[2]) {
			if (work[3] == chrn[3]) {
				found = TRUE;
			}
		}

		// IDCRC, DATACRC, DDAM
		error = sector->GetError();
		error &= (FDD_IDCRC | FDD_DATACRC | FDD_DDAM);
		status |= error;

		// 次へ
		sector = sector->GetNext();
	}

	// 結果を見る
	if (!found) {
		// (グローディア系)
		status |= (FDD_NODATA | FDD_DATAERR);
	}

	// GAP4作成
	total = MakeGAP4(ptr, total);

	// バッファが与えられていなければ、ここで終了
	if (!buf) {
		*len = 0;
		delete[] ptr;
		return status;
	}

	// スタート位置を決める(最初のセクタのデータ直前までスキップ)
	if (mfm) {
		start = 60 + GetGAP1();
	}
	else {
		start = 31 + GetGAP1();
	}

	// １回で終わる場合
	if (length <= (total - start)) {
		memcpy(buf, &ptr[start], length);
		*len = length;
		delete[] ptr;
		return status;
	}

	// 最初の1回を処理
	memcpy(buf, &ptr[start], (total - start));
	*len = (total - start);
	length -= (total - start);
	buf += (total - start);

	// 次のループ
	while (length > 0) {
		if (length <= total) {
			// 収まる
			memcpy(buf, ptr, length);
			*len += length;
			break;
		}
		// 全て入れる
		memcpy(buf, ptr, total);
		*len += total;
		length -= total;
		buf += total;
	}

	// ptr解放
	delete[] ptr;

	// 終了
	return status;
}

//---------------------------------------------------------------------------
//
//	ライトID
//	※2HD,DIMなどのセクタ固定タイプ向け
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTWRITE	書き込み禁止
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int /*gpl*/)
{
	FDISector *sector;
	DWORD chrn[4];
	int num;
	int i;
	DWORD pos;

	ASSERT(this);
	ASSERT(sc > 0);

	// 現在のセクタを取得
	if (IsMFM()) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// HDフラグが合わなければ、強制的にセクタ数0とする
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// セクタ数が一致していることが必要
	if (num != sc) {
		return FDD_NOTWRITE;
	}

	// 単倍混在は不可
	if (GetAllSectors() != num) {
		return FDD_NOTWRITE;
	}

	// 時間を設定(indexまで)
	pos = GetDisk()->GetRotationTime();
	pos -= GetDisk()->GetRotationPos();
	GetDisk()->SetSearch(pos);

	// bufが与えられていなければここまで
	if (!buf) {
		return FDD_NOERROR;
	}

	// CHRNが重複なく、一致していること
	sector = GetFirst();
	while (sector) {
		// bufの中から調べる
		for (i=0; i<sc; i++) {
			chrn[0] = (DWORD)buf[i * 4 + 0];
			chrn[1] = (DWORD)buf[i * 4 + 1];
			chrn[2] = (DWORD)buf[i * 4 + 2];
			chrn[3] = (DWORD)buf[i * 4 + 3];
			if (sector->IsMatch(mfm, chrn)) {
				break;
			}
		}

		// 一致するものがなかった
		if (i >= sc) {
			ASSERT(i == sc);
			return FDD_NOTWRITE;
		}

		// 次へ
		sector = sector->GetNext();
	}

	// 書き込みループ(全セクタを埋める)
	sector = GetFirst();
	while (sector) {
		sector->Fill(d);
		sector = sector->GetNext();
	}

	return FDD_NOERROR;
}

//---------------------------------------------------------------------------
//
//	変更チェック
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::IsChanged() const
{
	BOOL changed;
	FDISector *sector;

	ASSERT(this);

	// 初期化
	changed = FALSE;
	sector = GetFirst();

	// ORで
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	return changed;
}

//---------------------------------------------------------------------------
//
//	セクタレングス累計算出
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrack::GetTotalLength() const
{
	DWORD total;
	FDISector *sector;

	ASSERT(this);

	// 初期化
	total = 0;
	sector = GetFirst();

	// ループ
	while (sector) {
		total += sector->GetLength();
		sector = sector->GetNext();
	}

	return total;
}

//---------------------------------------------------------------------------
//
//	強制変更
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::ForceChanged()
{
	FDISector *sector;

	ASSERT(this);

	// 初期化
	sector = GetFirst();

	// ループ
	while (sector) {
		sector->ForceChanged();
		sector = sector->GetNext();
	}
}

//---------------------------------------------------------------------------
//
//	セクタ追加
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::AddSector(FDISector *sector)
{
	FDISector *ptr;

	ASSERT(this);
	ASSERT(sector);

	// セクタを持っていなければ、そのまま追加
	if (!trk.first) {
		trk.first = sector;
		sector->SetNext(NULL);

		// MFM決定
		trk.mfm = sector->IsMFM();
	}
	else {
		// 最終セクタを得る
		ptr = trk.first;
			while (ptr->GetNext()) {
			ptr = ptr->GetNext();
		}

		// 最終セクタに追加
		ptr->SetNext(sector);
		sector->SetNext(NULL);
	}

	// 個数加算
	trk.sectors[0]++;
	if (sector->IsMFM()) {
		trk.sectors[1]++;
	}
	else {
		trk.sectors[2]++;
	}
}

//---------------------------------------------------------------------------
//
//	セクタ全削除
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::ClrSector()
{
	FDISector *sector;

	ASSERT(this);

	// セクタをすべて削除
	while (trk.first) {
		sector = trk.first->GetNext();
		delete trk.first;
		trk.first = sector;
	}

	// 個数0
	trk.sectors[0] = 0;
	trk.sectors[1] = 0;
	trk.sectors[2] = 0;
}

//---------------------------------------------------------------------------
//
//	セクタ検索
//
//---------------------------------------------------------------------------
FDISector* FASTCALL FDITrack::Search(BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;

	ASSERT(this);
	ASSERT(chrn);

	// 最初のセクタを取得
	sector = GetFirst();

	// ループ
	while (sector) {
		// マッチすればそのセクタを返す
		if (sector->IsMatch(mfm, chrn)) {
			return sector;
		}

		sector = sector->GetNext();
	}

	// マッチしない
	return NULL;
}

//---------------------------------------------------------------------------
//
//	GAP1の長さを取得
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::GetGAP1() const
{
	ASSERT(this);

	if (IsMFM()) {
		// GAP4a 80バイト、SYNC12バイト、IAM4バイト、GAP1 50バイト
		return 146;
	}
	else {
		// GAP4a 40バイト、SYNC6バイト、IAM1バイト、GAP1 26バイト
		return 73;
	}
}

//---------------------------------------------------------------------------
//
//	トータルの長さを取得
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::GetTotal() const
{
	ASSERT(this);

	// 2DDは別扱い
	if (!trk.hd) {
		// PC-9801BX4での実測値
		if (IsMFM()) {
			return 6034 + GetGAP1() + 60;
		}
		else {
			return 3016 + GetGAP1() + 31;
		}
	}

	// XVIでの実測値
	if (IsMFM()) {
		return 10193 + GetGAP1() + 60;
	}
	else {
		return 5095 + GetGAP1() + 31;
	}
}

//---------------------------------------------------------------------------
//
//	各セクタ先頭の位置を算出
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CalcPos()
{
	FDISector *sector;
	DWORD total;
	DWORD prev;
	DWORD hus;
	FDISector *p;

	ASSERT(this);

	// 最初のセクタをセット
	sector = GetFirst();

	// ループ
	while (sector) {
		// GAP1
		prev = GetGAP1();

		// 全てのセクタをまわって、サイズを得る
		p = GetFirst();
		while (p) {
			if (p == sector) {
				break;
			}

			// まだ現れていなければ、prevを加算
			prev += GetSize(p);
			p = p->GetNext();
		}

		// IDフィールドとSYNCの部分を加える
		if (sector->IsMFM()) {
			prev += 60;
		}
		else {
			prev += 31;
		}

		// GAP4を加えたトータルの値を得る
		total = GetTotal();

		// prevとtotalの比を算出。１周でGetDisk()->GetRotationTime()になるように
		if (prev >= total) {
			prev = total;
		}
		ASSERT(total <= 0x5000);
		hus = GetDisk()->GetRotationTime();
		prev >>= 1;
		total >>= 1;
		prev *= hus;
		prev /= total;
		if (prev >= hus) {
			prev = hus - 1;
		}

		// 格納
		sector->SetPos(prev);

		// 次へ
		sector = sector->GetNext();
	}
}

//---------------------------------------------------------------------------
//
//	セクタのサイズを得る
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrack::GetSize(FDISector *sector) const
{
	DWORD len;

	ASSERT(this);
	ASSERT(sector);

	// まずセクタの実データ領域+CRC+GAP3
	len = sector->GetLength();
	len += 2;
	len += sector->GetGAP3();

	if (sector->IsMFM()) {
		// SYNC12バイト
		len += 12;

		// IDアドレスマーク、CHRN、CRC2バイト
		len += 10;

		// GAP2、SYNC、データマーク
		len += 38;
	}
	else {
		// SYNC6バイト
		len += 6;

		// IDアドレスマーク、CHRN、CRC2バイト
		len += 7;

		// GAP2、SYNC、データマーク
		len += 18;
	}

	return len;
}

//---------------------------------------------------------------------------
//
//	カレントポジション以降のセクタを取得
//
//---------------------------------------------------------------------------
FDISector* FASTCALL FDITrack::GetCurSector() const
{
	DWORD cur;
	DWORD pos;
	FDISector *sector;

	ASSERT(this);

	// カレントポジションを得る
	cur = GetDisk()->GetRotationPos();

	// 先頭セクタを得る
	sector = GetFirst();
	if (!sector) {
		return NULL;
	}

	// セクタを頭から見て、以上であればok
	while (sector) {
		pos = sector->GetPos();
		if (pos >= cur) {
			return sector;
		}
		sector = sector->GetNext();
	}

	// 最終セクタの位置を超えるところが指定されているので、先頭に戻す
	return GetFirst();
}

//---------------------------------------------------------------------------
//
//	GAP1作成
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeGAP1(BYTE *buf, int offset) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);

	// MFMか
	if (IsMFM()) {
		ASSERT(GetMFMSectors() > 0);

		// GAP1作成
		offset = MakeData(buf, offset, 0x4e, 80);
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xc2, 3);
		offset = MakeData(buf, offset, 0xfc, 1);
		offset = MakeData(buf, offset, 0x4e, 50);
		return offset;
	}

	// FM
	ASSERT(GetFMSectors() > 0);

	// GAP1作成
	offset = MakeData(buf, offset, 0xff, 40);
	offset = MakeData(buf, offset, 0x00, 6);
	offset = MakeData(buf, offset, 0xfc, 1);
	offset = MakeData(buf, offset, 0xff, 26);
	return offset;
}

//---------------------------------------------------------------------------
//
//	セクタデータ作成
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeSector(BYTE *buf, int offset, FDISector *sector) const
{
	DWORD chrn[4];
	int i;
	WORD crc;
	const BYTE *ptr;
	int len;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);
	ASSERT(sector);

	// CHRN、セクタデータ、レングス取得
	sector->GetCHRN(chrn);
	ptr = sector->GetSector();
	len = sector->GetLength();

	// MFMか
	if (sector->IsMFM()) {
		// MFM(ID部)
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xa1, 3);
		offset = MakeData(buf, offset, 0xfe, 1);
		for (i=0; i<4; i++) {
			buf[offset + i] = (BYTE)chrn[i];
		}
		offset += 4;
		crc = CalcCRC(&buf[offset - 8], 8);
		buf[offset + 0] = (BYTE)(crc >> 8);
		buf[offset + 1] = (BYTE)crc;
		offset += 2;
		offset = MakeData(buf, offset, 0x4e, 22);

		// MFM(データ部)
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xa1, 3);
		if (sector->GetError() & FDD_DDAM) {
			offset = MakeData(buf, offset, 0xf8, 1);
		}
		else {
			offset = MakeData(buf, offset, 0xfb, 1);
		}
		memcpy(&buf[offset], ptr, len);
		crc = CalcCRC(&buf[offset - 4], len + 4);
		offset += len;
		buf[offset + 0] = (BYTE)(crc >> 8);
		buf[offset + 1] = (BYTE)crc;
		offset += 2;
		offset = MakeData(buf, offset, 0x4e, sector->GetGAP3());
		return offset;
	}

	// FM(ID部)
	offset = MakeData(buf, offset, 0x00, 6);
	offset = MakeData(buf, offset, 0xfe, 1);
	for (i=0; i<4; i++) {
		buf[offset + i] = (BYTE)chrn[i];
	}
	offset += 4;
	crc = CalcCRC(&buf[offset - 5], 5);
	buf[offset + 0] = (BYTE)(crc >> 8);
	buf[offset + 1] = (BYTE)crc;
	offset += 2;
	offset = MakeData(buf, offset, 0xff, 11);

	// FM(データ部)
	offset = MakeData(buf, offset, 0x00, 6);
	if (sector->GetError() & FDD_DDAM) {
		offset = MakeData(buf, offset, 0xf8, 1);
	}
	else {
		offset = MakeData(buf, offset, 0xfb, 1);
	}
	memcpy(&buf[offset], ptr, len);
	crc = CalcCRC(&buf[offset - 1], len + 1);
	offset += len;
	buf[offset + 0] = (BYTE)(crc >> 8);
	buf[offset + 1] = (BYTE)crc;
	offset += 2;
	offset = MakeData(buf, offset, 0xff, sector->GetGAP3());

	return offset;
}

//---------------------------------------------------------------------------
//
//	GAP4作成
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeGAP4(BYTE *buf, int offset) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);

	if (IsMFM()) {
		return MakeData(buf, offset, 0x4e, GetTotal() - offset);
	}
	else {
		return MakeData(buf, offset, 0xff, GetTotal() - offset);
	}
}

//---------------------------------------------------------------------------
//
//	CRC算出
//
//---------------------------------------------------------------------------
WORD FASTCALL FDITrack::CalcCRC(BYTE *ptr, int len) const
{
	WORD crc;
	int i;

	ASSERT(this);
	ASSERT(ptr);
	ASSERT(len >= 0);

	// 初期化
	crc = 0xffff;

	// ループ
	for (i=0; i<len; i++) {
		crc = (WORD)((crc << 8) ^ CRCTable[ (BYTE)(crc >> 8) ^ (BYTE)*ptr++ ]);
	}

	return crc;
}

//---------------------------------------------------------------------------
//
//	CRC算出テーブル
//
//---------------------------------------------------------------------------
const WORD FDITrack::CRCTable[0x100] = {
	0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
	0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
	0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
	0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
	0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
	0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
	0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
	0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
	0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
	0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
	0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
	0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
	0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
	0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
	0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
	0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
	0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
	0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
	0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
	0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
	0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
	0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
	0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
	0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
	0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
	0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
	0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
	0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
	0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
	0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
	0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
	0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};

//---------------------------------------------------------------------------
//
//	Diagデータ作成
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeData(BYTE *buf, int offset, BYTE data, int length) const
{
	int i;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);
	ASSERT((length > 0) && (length < 0x400));

	for (i=0; i<length; i++) {
		buf[offset + i] = data;
	}

	return (offset + length);
}

//===========================================================================
//
//	FDIディスク
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDisk::FDIDisk(int index, FDI *fdi)
{
	ASSERT((index >= 0) && (index < 0x10));

	// インデックス、ID指定
	disk.index = index;
	disk.fdi = fdi;
	disk.id = MAKEID('I', 'N', 'I', 'T');

	// 状態
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// 名称なし
	disk.name[0] = '\0';
	disk.offset = 0;

	// 保持トラックなし
	disk.first = NULL;
	disk.head[0] = NULL;
	disk.head[1] = NULL;

	// ポジション
	disk.search = 0;

	// リンクなし
	disk.next = NULL;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//	※派生クラスの注意点：
//		現在のhead[]をファイルに書き戻す
//
//---------------------------------------------------------------------------
FDIDisk::~FDIDisk()
{
	// クリア
	ClrTrack();
}

//---------------------------------------------------------------------------
//
//	作成
//	※派生クラスの注意点：
//		物理フォーマットを行う(仮想関数の最後で、ここを呼ぶこと)
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Create(const Filepath& /*path*/, const option_t *opt)
{
	ASSERT(this);
	ASSERT(opt);

	// 論理フォーマットが必要なければ終了
	if (!opt->logfmt) {
		return TRUE;
	}

	// 物理フォーマット別に、論理フォーマットを行う
	switch (opt->phyfmt) {
		// 2HD
		case FDI_2HD:
			Create2HD(TRUE);
			break;

		// 2HDA
		case FDI_2HDA:
			Create2HD(FALSE);
			break;

		// 2HS
		case FDI_2HS:
			Create2HS();
			break;

		// 2HC
		case FDI_2HC:
			Create2HC();
			break;

		// 2HDE
		case FDI_2HDE:
			Create2HDE();
			break;

		// 2HQ
		case FDI_2HQ:
			Create2HQ();
			break;

		// 2DD
		case FDI_2DD:
			Create2DD();
			break;

		// その他
		default:
			return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	論理フォーマット(2HD,2HDA)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HD(BOOL flag2hd)
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// トラック0セクタ1〜8へすべて書き込む
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 3;
	for (i=1; i<=8; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック1セクタ1〜3へすべて書き込む
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 3;
	for (i=1; i<=3; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL書き込み
	memcpy(buf, IPL2HD, 0x200);
	if (!flag2hd) {
		// 2HDAは論理セクタ数=1280セクタ
		buf[0x13] = 0x00;
		buf[0x14] = 0x05;
	}
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// FAT先頭セクタ初期化
	memset(buf, 0, sizeof(buf));
	buf[0] = 0xfe;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// 第1FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// 第2FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 4;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL(2HD,2HDA)
//	※FORMAT.x v2.31より取得したもの
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HD[0x200] = {
	0x60,0x3c,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x04,0x01,0x01,0x00,
	0x02,0xc0,0x00,0xd0,0x04,0xfe,0x02,0x00,
	0x08,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x04,0x00,0x03,0x00,
	0x00,0x06,0x00,0x08,0x00,0x1f,0x00,0x09,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	論理フォーマット(2HS)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HS()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// 合計10セクタへ書き込む(1トラックあたり9セクタ)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 3;
	for (i=11; i<=18; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[2] = 10;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL書き込み(1)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HS[0x000], FALSE);

	// IPL書き込み(2)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 13;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HS[0x400], FALSE);

	// FAT先頭セクタ初期化
	buf[0] = 0xfb;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 11;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL(2HS)
//	※9scdrv.x v3.00より取得したもの
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HS[0x800] = {
	0x60,0x1e,0x39,0x53,0x43,0x46,0x4d,0x54,
	0x20,0x49,0x50,0x4c,0x20,0x76,0x31,0x2e,
	0x30,0x32,0x04,0x00,0x01,0x03,0x00,0x01,
	0x00,0xc0,0x05,0xa0,0xfb,0x01,0x90,0x70,
	0x60,0x00,0x03,0x5a,0x08,0x01,0x00,0x0c,
	0x66,0x08,0x4d,0xfa,0xff,0xd4,0x2c,0x56,
	0x4e,0xd6,0x61,0x00,0x00,0xba,0x48,0xe7,
	0x4f,0x00,0x61,0x00,0x02,0xf0,0x61,0x00,
	0x00,0xc4,0x08,0x00,0x00,0x1b,0x66,0x4e,
	0xc2,0x3c,0x00,0xc0,0x82,0x3c,0x00,0x06,
	0x61,0x00,0x00,0xd0,0xe1,0x9a,0x54,0x88,
	0x20,0xc2,0xe0,0x9a,0x10,0xc2,0x10,0xc7,
	0x10,0x86,0x61,0x00,0x00,0xf0,0x41,0xf8,
	0x09,0xee,0x70,0x08,0x61,0x00,0x01,0x0c,
	0x61,0x00,0x01,0x42,0x61,0x00,0x01,0x60,
	0x61,0x00,0x01,0x7a,0x08,0x00,0x00,0x0e,
	0x66,0x0c,0x08,0x00,0x00,0x1e,0x67,0x26,
	0x08,0x00,0x00,0x1b,0x66,0x08,0x61,0x00,
	0x01,0x7a,0x51,0xcc,0xff,0xbc,0x4c,0xdf,
	0x00,0xf2,0x4a,0x38,0x09,0xe1,0x67,0x0c,
	0x31,0xf8,0x09,0xc2,0x09,0xc4,0x11,0xfc,
	0x00,0x40,0x09,0xe1,0x4e,0x75,0x08,0x00,
	0x00,0x1f,0x66,0xe2,0xd3,0xc5,0x96,0x85,
	0x63,0xdc,0x20,0x04,0x48,0x40,0x38,0x00,
	0x30,0x3c,0x00,0x12,0x52,0x02,0xb0,0x02,
	0x64,0x86,0x14,0x3c,0x00,0x0a,0x0a,0x42,
	0x01,0x00,0x08,0x02,0x00,0x08,0x66,0x00,
	0xff,0x78,0xd4,0xbc,0x00,0x01,0x00,0x00,
	0x61,0x00,0x01,0xb8,0x08,0x00,0x00,0x1b,
	0x66,0xac,0x60,0x00,0xff,0x64,0x08,0x38,
	0x00,0x07,0x09,0xe1,0x66,0x0c,0x48,0xe7,
	0xc0,0x00,0x61,0x00,0x01,0x46,0x4c,0xdf,
	0x00,0x03,0x4e,0x75,0x70,0x00,0x78,0x00,
	0x08,0x01,0x00,0x05,0x67,0x08,0x78,0x09,
	0x48,0x44,0x38,0x3c,0x00,0x09,0x08,0x01,
	0x00,0x04,0x67,0x04,0x61,0x00,0x01,0x7c,
	0x4e,0x75,0x2f,0x01,0x41,0xf8,0x09,0xee,
	0x10,0x81,0xe0,0x99,0xc2,0x3c,0x00,0x03,
	0x08,0x02,0x00,0x08,0x67,0x04,0x08,0xc1,
	0x00,0x02,0x11,0x41,0x00,0x01,0x22,0x1f,
	0x4e,0x75,0x13,0xfc,0x00,0xff,0x00,0xe8,
	0x40,0x00,0x13,0xfc,0x00,0x32,0x00,0xe8,
	0x40,0x05,0x60,0x10,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x13,0xfc,0x00,0xb2,
	0x00,0xe8,0x40,0x05,0x23,0xc9,0x00,0xe8,
	0x40,0x0c,0x33,0xc5,0x00,0xe8,0x40,0x0a,
	0x13,0xfc,0x00,0x80,0x00,0xe8,0x40,0x07,
	0x4e,0x75,0x48,0xe7,0x40,0x60,0x43,0xf9,
	0x00,0xe9,0x40,0x01,0x45,0xf9,0x00,0xe9,
	0x40,0x03,0x40,0xe7,0x00,0x7c,0x07,0x00,
	0x12,0x11,0x08,0x01,0x00,0x04,0x66,0xf8,
	0x12,0x11,0x08,0x01,0x00,0x07,0x67,0xf8,
	0x08,0x01,0x00,0x06,0x66,0xf2,0x14,0x98,
	0x51,0xc8,0xff,0xee,0x46,0xdf,0x4c,0xdf,
	0x06,0x02,0x4e,0x75,0x10,0x39,0x00,0xe8,
	0x40,0x00,0x08,0x00,0x00,0x04,0x66,0x0e,
	0x10,0x39,0x00,0xe9,0x40,0x01,0xc0,0x3c,
	0x00,0x1f,0x66,0xf4,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x01,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x00,0x08,0x00,0x00,0x07,
	0x66,0x08,0x13,0xfc,0x00,0x10,0x00,0xe8,
	0x40,0x07,0x13,0xfc,0x00,0xff,0x00,0xe8,
	0x40,0x00,0x4e,0x75,0x30,0x01,0xe0,0x48,
	0xc0,0xbc,0x00,0x00,0x00,0x03,0xe7,0x40,
	0x41,0xf8,0x0c,0x90,0xd1,0xc0,0x20,0x10,
	0x4e,0x75,0x2f,0x00,0xc0,0xbc,0x00,0x35,
	0xff,0x00,0x67,0x2a,0xb8,0x3c,0x00,0x05,
	0x64,0x24,0x2f,0x38,0x09,0xee,0x2f,0x38,
	0x09,0xf2,0x3f,0x38,0x09,0xf6,0x61,0x00,
	0x00,0xc4,0x70,0x64,0x51,0xc8,0xff,0xfe,
	0x61,0x68,0x31,0xdf,0x09,0xf6,0x21,0xdf,
	0x09,0xf2,0x21,0xdf,0x09,0xee,0x20,0x1f,
	0x4e,0x75,0x30,0x01,0xe0,0x48,0x4a,0x00,
	0x67,0x3c,0xc0,0x3c,0x00,0x03,0x80,0x3c,
	0x00,0x80,0x08,0xf8,0x00,0x07,0x09,0xe1,
	0x13,0xc0,0x00,0xe9,0x40,0x07,0x08,0xf8,
	0x00,0x06,0x09,0xe1,0x66,0x18,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x61,0x00,0x00,0x90,
	0x08,0x00,0x00,0x1d,0x66,0x08,0x0c,0x78,
	0x00,0x64,0x09,0xc4,0x64,0xee,0x08,0xb8,
	0x00,0x06,0x09,0xe1,0x4e,0x75,0x4a,0x38,
	0x09,0xe1,0x67,0x0c,0x31,0xf8,0x09,0xc2,
	0x09,0xc4,0x11,0xfc,0x00,0x40,0x09,0xe1,
	0x4e,0x75,0x61,0x12,0x08,0x00,0x00,0x1b,
	0x66,0x26,0x48,0x40,0x48,0x42,0xb4,0x00,
	0x67,0x1a,0x48,0x42,0x61,0x3e,0x2f,0x01,
	0x12,0x3c,0x00,0x0f,0x61,0x00,0xfe,0x6c,
	0x48,0x42,0x11,0x42,0x00,0x02,0x48,0x42,
	0x70,0x02,0x60,0x08,0x48,0x42,0x48,0x40,
	0x4e,0x75,0x2f,0x01,0x61,0x00,0xfe,0xac,
	0x61,0x00,0xfe,0xee,0x22,0x1f,0x30,0x01,
	0xe0,0x48,0xc0,0xbc,0x00,0x00,0x00,0x03,
	0xe7,0x40,0x41,0xf8,0x0c,0x90,0xd1,0xc0,
	0x20,0x10,0x4e,0x75,0x2f,0x01,0x12,0x3c,
	0x00,0x07,0x61,0x00,0xfe,0x2e,0x70,0x01,
	0x61,0xd0,0x22,0x1f,0x4e,0x75,0x2f,0x01,
	0x12,0x3c,0x00,0x04,0x61,0x00,0xfe,0x1c,
	0x22,0x1f,0x70,0x01,0x61,0x00,0xfe,0x6c,
	0x10,0x39,0x00,0xe9,0x40,0x01,0xc0,0x3c,
	0x00,0xd0,0xb0,0x3c,0x00,0xd0,0x66,0xf0,
	0x70,0x00,0x10,0x39,0x00,0xe9,0x40,0x03,
	0xe0,0x98,0x4e,0x75,0x53,0x02,0x7e,0x00,
	0x3a,0x02,0xe0,0x5d,0x4a,0x05,0x67,0x04,
	0x06,0x45,0x08,0x00,0xe0,0x4d,0x48,0x42,
	0x02,0x82,0x00,0x00,0x00,0xff,0xe9,0x8a,
	0xd4,0x45,0x0c,0x42,0x00,0x04,0x65,0x02,
	0x53,0x42,0x84,0xfc,0x00,0x12,0x48,0x42,
	0x3e,0x02,0x8e,0xfc,0x00,0x09,0x48,0x47,
	0xe1,0x4f,0xe0,0x8f,0x34,0x07,0x06,0x82,
	0x03,0x00,0x00,0x0a,0x2a,0x3c,0x00,0x00,
	0x04,0x00,0x3c,0x3c,0x00,0xff,0x3e,0x3c,
	0x09,0x28,0x4e,0x75,0x4f,0xfa,0xfc,0x82,
	0x43,0xfa,0xfc,0xa2,0x4d,0xfa,0xfc,0x7a,
	0x2c,0xb9,0x00,0x00,0x05,0x18,0x23,0xc9,
	0x00,0x00,0x05,0x18,0x43,0xfa,0x00,0xda,
	0x4d,0xfa,0xfc,0x6a,0x2c,0xb9,0x00,0x00,
	0x05,0x14,0x23,0xc9,0x00,0x00,0x05,0x14,
	0x43,0xfa,0x01,0x6e,0x4d,0xfa,0xfc,0x5a,
	0x2c,0xb9,0x00,0x00,0x05,0x04,0x23,0xc9,
	0x00,0x00,0x05,0x04,0x24,0x3c,0x03,0x00,
	0x00,0x04,0x20,0x3c,0x00,0x00,0x00,0x8e,
	0x4e,0x4f,0x12,0x00,0xe1,0x41,0x12,0x3c,
	0x00,0x70,0x33,0xc1,0x00,0x00,0x00,0x64,
	0x26,0x3c,0x00,0x00,0x04,0x00,0x43,0xfa,
	0x00,0x20,0x61,0x04,0x60,0x00,0x01,0xf0,
	0x48,0xe7,0x78,0x40,0x70,0x46,0x4e,0x4f,
	0x08,0x00,0x00,0x1e,0x66,0x02,0x70,0x00,
	0x4c,0xdf,0x02,0x1e,0x4e,0x75,0x4e,0x75,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfb,0x8c,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfc,0x6e,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfe,0xa4,0x61,0x00,0xfc,0x78,0x08,0x00,
	0x00,0x1b,0x66,0x30,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x05,0x60,0x08,0x30,0x3c,
	0x01,0xac,0x51,0xc8,0xff,0xfe,0x61,0x00,
	0x01,0x00,0x08,0x00,0x00,0x1e,0x67,0x2c,
	0x08,0x00,0x00,0x1b,0x66,0x0e,0x08,0x00,
	0x00,0x11,0x66,0x08,0x61,0x00,0xfd,0x4c,
	0x51,0xcc,0xff,0xe4,0x4c,0xdf,0x00,0xf2,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x08,0x00,0x00,0x1f,
	0x66,0xe2,0xd3,0xc5,0x96,0x85,0x63,0xdc,
	0x20,0x04,0x48,0x40,0x38,0x00,0x30,0x3c,
	0x00,0x12,0x52,0x02,0xb0,0x02,0x64,0xae,
	0x14,0x3c,0x00,0x0a,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0x98,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfd,0x8c,
	0x08,0x00,0x00,0x1b,0x66,0xae,0x60,0x8e,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfa,0xe0,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfb,0xc6,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfd,0xfc,0x61,0x00,0xfb,0xd0,0x08,0x00,
	0x00,0x1b,0x66,0x24,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x11,0x61,0x62,0x08,0x00,
	0x00,0x0a,0x66,0x14,0x08,0x00,0x00,0x1e,
	0x67,0x16,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0xfc,0xb0,0x51,0xcc,0xff,0xe6,
	0x4c,0xdf,0x00,0xf2,0x60,0x00,0xfb,0x34,
	0x08,0x00,0x00,0x1f,0x66,0xf2,0xd3,0xc5,
	0x96,0x85,0x63,0xec,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x12,0x30,0x3c,
	0x00,0x12,0x52,0x02,0xb0,0x02,0x64,0xbc,
	0x14,0x3c,0x00,0x0a,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0xae,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfc,0xfc,
	0x08,0x00,0x00,0x1b,0x66,0xba,0x60,0x9c,
	0x61,0x00,0xfb,0x78,0xe1,0x9a,0x54,0x88,
	0x20,0xc2,0xe0,0x9a,0x10,0xc2,0x10,0xc7,
	0x10,0x86,0x61,0x00,0xfb,0x86,0x41,0xf8,
	0x09,0xee,0x70,0x08,0x61,0x00,0xfb,0xb4,
	0x61,0x00,0xfb,0xea,0x61,0x00,0xfc,0x08,
	0x61,0x00,0xfc,0x22,0x4e,0x75,0x43,0xfa,
	0x01,0x8c,0x61,0x00,0x01,0x76,0x24,0x3c,
	0x03,0x00,0x00,0x06,0x32,0x39,0x00,0x00,
	0x00,0x64,0x26,0x3c,0x00,0x00,0x04,0x00,
	0x43,0xf8,0x28,0x00,0x61,0x00,0xfd,0xf2,
	0x4a,0x80,0x66,0x00,0x01,0x20,0x43,0xf8,
	0x28,0x00,0x49,0xfa,0x01,0x54,0x78,0x1f,
	0x24,0x49,0x26,0x4c,0x7a,0x0a,0x10,0x1a,
	0x80,0x3c,0x00,0x20,0xb0,0x1b,0x66,0x06,
	0x51,0xcd,0xff,0xf4,0x60,0x0c,0x43,0xe9,
	0x00,0x20,0x51,0xcc,0xff,0xe4,0x66,0x00,
	0x00,0xf4,0x30,0x29,0x00,0x1a,0xe1,0x58,
	0x55,0x40,0xd0,0x7c,0x00,0x0b,0x34,0x00,
	0xc4,0x7c,0x00,0x07,0x52,0x02,0xe8,0x48,
	0x64,0x04,0x84,0x7c,0x01,0x00,0x48,0x42,
	0x34,0x3c,0x03,0x00,0x14,0x00,0x48,0x42,
	0x26,0x29,0x00,0x1c,0xe1,0x5b,0x48,0x43,
	0xe1,0x5b,0x43,0xf8,0x67,0xc0,0x61,0x00,
	0xfd,0x88,0x0c,0x51,0x48,0x55,0x66,0x00,
	0x00,0xb4,0x4b,0xf8,0x68,0x00,0x49,0xfa,
	0x00,0x4c,0x22,0x4d,0x43,0xf1,0x38,0xc0,
	0x2c,0x3c,0x00,0x04,0x00,0x00,0x0c,0x69,
	0x4e,0xd4,0xff,0xd2,0x66,0x36,0x0c,0xad,
	0x4c,0x5a,0x58,0x20,0x00,0x04,0x66,0x16,
	0x2b,0x46,0x00,0x04,0x2b,0x4d,0x00,0x08,
	0x42,0xad,0x00,0x20,0x51,0xf9,0x00,0x00,
	0x07,0x9e,0x4e,0xed,0x00,0x02,0x0c,0x6d,
	0x4e,0xec,0x00,0x1a,0x66,0x0e,0x0c,0x6d,
	0x4e,0xea,0x00,0x2a,0x66,0x06,0x43,0xfa,
	0x01,0x1f,0x60,0x64,0x10,0x3c,0x00,0xc0,
	0x41,0xf8,0x68,0x00,0x36,0x3c,0xff,0xff,
	0xb0,0x18,0x67,0x26,0x51,0xcb,0xff,0xfa,
	0x43,0xf8,0x68,0x00,0x4a,0x39,0x00,0x00,
	0x07,0x9e,0x67,0x14,0x41,0xf8,0x67,0xcc,
	0x24,0x18,0xd4,0x98,0x22,0x10,0xd1,0xc2,
	0x53,0x81,0x65,0x04,0x42,0x18,0x60,0xf8,
	0x4e,0xd1,0x0c,0x10,0x00,0x04,0x66,0xd0,
	0x52,0x88,0x0c,0x10,0x00,0xd0,0x66,0xc8,
	0x52,0x88,0x0c,0x10,0x00,0xfe,0x66,0xc0,
	0x52,0x88,0x0c,0x10,0x00,0x02,0x66,0xb8,
	0x57,0x88,0x30,0xfc,0x05,0xa1,0x10,0xbc,
	0x00,0xfb,0x60,0xac,0x43,0xfa,0x00,0x92,
	0x2f,0x09,0x43,0xfa,0x00,0x47,0x61,0x2a,
	0x43,0xfa,0x00,0x46,0x61,0x24,0x43,0xfa,
	0x00,0x52,0x61,0x1e,0x43,0xfa,0x00,0x43,
	0x61,0x18,0x43,0xfa,0x00,0x46,0x61,0x12,
	0x22,0x5f,0x61,0x0e,0x32,0x39,0x00,0x00,
	0x00,0x64,0x70,0x4f,0x4e,0x4f,0x70,0xfe,
	0x4e,0x4f,0x70,0x21,0x4e,0x4f,0x4e,0x75,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x39,0x53,0x43,0x49,
	0x50,0x4c,0x00,0x1b,0x5b,0x34,0x37,0x6d,
	0x1b,0x5b,0x31,0x33,0x3b,0x32,0x36,0x48,
	0x00,0x1b,0x5b,0x31,0x34,0x3b,0x32,0x36,
	0x48,0x00,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,
	0x1b,0x5b,0x31,0x34,0x3b,0x33,0x34,0x48,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x93,0xc7,0x82,0xdd,
	0x8d,0x9e,0x82,0xdd,0x83,0x47,0x83,0x89,
	0x81,0x5b,0x82,0xc5,0x82,0xb7,0x00,0x1b,
	0x5b,0x31,0x34,0x3b,0x33,0x34,0x48,0x4c,
	0x5a,0x58,0x2e,0x58,0x20,0x82,0xcc,0x83,
	0x6f,0x81,0x5b,0x83,0x57,0x83,0x87,0x83,
	0x93,0x82,0xaa,0x8c,0xc3,0x82,0xb7,0x82,
	0xac,0x82,0xdc,0x82,0xb7,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	論理フォーマット(2HC)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HC()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// 合計29セクタへ書き込む(1トラックあたり15セクタ)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=15; i++) {
		chrn[2] = (BYTE)i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=14; i++) {
		chrn[2] = (BYTE)i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(IPL2HC, FALSE);

	// FAT先頭セクタ初期化
	buf[0] = 0xf9;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// 第1FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// 第2FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 9;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL(2HC)
//	※FORMAT.x v2.31より取得したもの
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HC[0x200] = {
	0x60,0x3c,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x02,0x01,0x01,0x00,
	0x02,0xe0,0x00,0x60,0x09,0xf9,0x07,0x00,
	0x0f,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x02,0x00,0x02,0x00,
	0x01,0x01,0x00,0x0f,0x00,0x0f,0x00,0x1b,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	論理フォーマット(2HDE)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HDE()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// 合計13セクタへ書き込む(1トラックあたり9セクタ)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[3] = 3;
	for (i=2; i<=9; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0x81;
	chrn[3] = 3;
	for (i=1; i<=4; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL書き込み(1)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HDE[0x000], FALSE);

	// IPL書き込み(2)
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 4;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HDE[0x400], FALSE);

	// FAT先頭セクタ初期化
	buf[0] = 0xf8;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// 第1FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 2;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// 第2FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 5;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL(2HDE)
//	※9scdrv.x v3.00より取得したもの
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HDE[0x800] = {
	0x60,0x20,0x32,0x48,0x44,0x45,0x20,0x76,
	0x31,0x2e,0x31,0x00,0x00,0x04,0x01,0x01,
	0x00,0x02,0xc0,0x00,0xa0,0x05,0x03,0x03,
	0x00,0x09,0x00,0x02,0x00,0x00,0x00,0x00,
	0x90,0x70,0x60,0x00,0x03,0x5a,0x08,0x01,
	0x00,0x0c,0x66,0x08,0x4d,0xfa,0xff,0xd2,
	0x2c,0x56,0x4e,0xd6,0x61,0x00,0x00,0xba,
	0x48,0xe7,0x4f,0x00,0x61,0x00,0x02,0xf0,
	0x61,0x00,0x00,0xc4,0x08,0x00,0x00,0x1b,
	0x66,0x4e,0xc2,0x3c,0x00,0xc0,0x82,0x3c,
	0x00,0x06,0x61,0x00,0x00,0xd0,0xe1,0x9a,
	0x54,0x88,0x20,0xc2,0xe0,0x9a,0x10,0xc2,
	0x10,0xc7,0x10,0x86,0x61,0x00,0x00,0xf0,
	0x41,0xf8,0x09,0xee,0x70,0x08,0x61,0x00,
	0x01,0x0c,0x61,0x00,0x01,0x42,0x61,0x00,
	0x01,0x60,0x61,0x00,0x01,0x7a,0x08,0x00,
	0x00,0x0e,0x66,0x0c,0x08,0x00,0x00,0x1e,
	0x67,0x26,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0x01,0x7a,0x51,0xcc,0xff,0xbc,
	0x4c,0xdf,0x00,0xf2,0x4a,0x38,0x09,0xe1,
	0x67,0x0c,0x31,0xf8,0x09,0xc2,0x09,0xc4,
	0x11,0xfc,0x00,0x40,0x09,0xe1,0x4e,0x75,
	0x08,0x00,0x00,0x1f,0x66,0xe2,0xd3,0xc5,
	0x96,0x85,0x63,0xdc,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x09,0x52,0x02,
	0xb0,0x02,0x64,0x86,0x14,0x3c,0x00,0x01,
	0x0a,0x42,0x01,0x00,0x08,0x02,0x00,0x08,
	0x66,0x00,0xff,0x78,0xd4,0xbc,0x00,0x01,
	0x00,0x00,0x61,0x00,0x01,0xb8,0x08,0x00,
	0x00,0x1b,0x66,0xac,0x60,0x00,0xff,0x64,
	0x08,0x38,0x00,0x07,0x09,0xe1,0x66,0x0c,
	0x48,0xe7,0xc0,0x00,0x61,0x00,0x01,0x46,
	0x4c,0xdf,0x00,0x03,0x4e,0x75,0x70,0x00,
	0x78,0x00,0x08,0x01,0x00,0x05,0x67,0x08,
	0x78,0x09,0x48,0x44,0x38,0x3c,0x00,0x09,
	0x08,0x01,0x00,0x04,0x67,0x04,0x61,0x00,
	0x01,0x7c,0x4e,0x75,0x2f,0x01,0x41,0xf8,
	0x09,0xee,0x10,0x81,0xe0,0x99,0xc2,0x3c,
	0x00,0x03,0x08,0x02,0x00,0x08,0x67,0x04,
	0x08,0xc1,0x00,0x02,0x11,0x41,0x00,0x01,
	0x22,0x1f,0x4e,0x75,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x13,0xfc,0x00,0x32,
	0x00,0xe8,0x40,0x05,0x60,0x10,0x13,0xfc,
	0x00,0xff,0x00,0xe8,0x40,0x00,0x13,0xfc,
	0x00,0xb2,0x00,0xe8,0x40,0x05,0x23,0xc9,
	0x00,0xe8,0x40,0x0c,0x33,0xc5,0x00,0xe8,
	0x40,0x0a,0x13,0xfc,0x00,0x80,0x00,0xe8,
	0x40,0x07,0x4e,0x75,0x48,0xe7,0x40,0x60,
	0x43,0xf9,0x00,0xe9,0x40,0x01,0x45,0xf9,
	0x00,0xe9,0x40,0x03,0x40,0xe7,0x00,0x7c,
	0x07,0x00,0x12,0x11,0x08,0x01,0x00,0x04,
	0x66,0xf8,0x12,0x11,0x08,0x01,0x00,0x07,
	0x67,0xf8,0x08,0x01,0x00,0x06,0x66,0xf2,
	0x14,0x98,0x51,0xc8,0xff,0xee,0x46,0xdf,
	0x4c,0xdf,0x06,0x02,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x00,0x08,0x00,0x00,0x04,
	0x66,0x0e,0x10,0x39,0x00,0xe9,0x40,0x01,
	0xc0,0x3c,0x00,0x1f,0x66,0xf4,0x4e,0x75,
	0x10,0x39,0x00,0xe8,0x40,0x01,0x4e,0x75,
	0x10,0x39,0x00,0xe8,0x40,0x00,0x08,0x00,
	0x00,0x07,0x66,0x08,0x13,0xfc,0x00,0x10,
	0x00,0xe8,0x40,0x07,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x4e,0x75,0x30,0x01,
	0xe0,0x48,0xc0,0xbc,0x00,0x00,0x00,0x03,
	0xe7,0x40,0x41,0xf8,0x0c,0x90,0xd1,0xc0,
	0x20,0x10,0x4e,0x75,0x2f,0x00,0xc0,0xbc,
	0x00,0x35,0xff,0x00,0x67,0x2a,0xb8,0x3c,
	0x00,0x05,0x64,0x24,0x2f,0x38,0x09,0xee,
	0x2f,0x38,0x09,0xf2,0x3f,0x38,0x09,0xf6,
	0x61,0x00,0x00,0xc4,0x70,0x64,0x51,0xc8,
	0xff,0xfe,0x61,0x68,0x31,0xdf,0x09,0xf6,
	0x21,0xdf,0x09,0xf2,0x21,0xdf,0x09,0xee,
	0x20,0x1f,0x4e,0x75,0x30,0x01,0xe0,0x48,
	0x4a,0x00,0x67,0x3c,0xc0,0x3c,0x00,0x03,
	0x80,0x3c,0x00,0x80,0x08,0xf8,0x00,0x07,
	0x09,0xe1,0x13,0xc0,0x00,0xe9,0x40,0x07,
	0x08,0xf8,0x00,0x06,0x09,0xe1,0x66,0x18,
	0x31,0xf8,0x09,0xc2,0x09,0xc4,0x61,0x00,
	0x00,0x90,0x08,0x00,0x00,0x1d,0x66,0x08,
	0x0c,0x78,0x00,0x64,0x09,0xc4,0x64,0xee,
	0x08,0xb8,0x00,0x06,0x09,0xe1,0x4e,0x75,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x61,0x12,0x08,0x00,
	0x00,0x1b,0x66,0x26,0x48,0x40,0x48,0x42,
	0xb4,0x00,0x67,0x1a,0x48,0x42,0x61,0x3e,
	0x2f,0x01,0x12,0x3c,0x00,0x0f,0x61,0x00,
	0xfe,0x6c,0x48,0x42,0x11,0x42,0x00,0x02,
	0x48,0x42,0x70,0x02,0x60,0x08,0x48,0x42,
	0x48,0x40,0x4e,0x75,0x2f,0x01,0x61,0x00,
	0xfe,0xac,0x61,0x00,0xfe,0xee,0x22,0x1f,
	0x30,0x01,0xe0,0x48,0xc0,0xbc,0x00,0x00,
	0x00,0x03,0xe7,0x40,0x41,0xf8,0x0c,0x90,
	0xd1,0xc0,0x20,0x10,0x4e,0x75,0x2f,0x01,
	0x12,0x3c,0x00,0x07,0x61,0x00,0xfe,0x2e,
	0x70,0x01,0x61,0xd0,0x22,0x1f,0x4e,0x75,
	0x2f,0x01,0x12,0x3c,0x00,0x04,0x61,0x00,
	0xfe,0x1c,0x22,0x1f,0x70,0x01,0x61,0x00,
	0xfe,0x6c,0x10,0x39,0x00,0xe9,0x40,0x01,
	0xc0,0x3c,0x00,0xd0,0xb0,0x3c,0x00,0xd0,
	0x66,0xf0,0x70,0x00,0x10,0x39,0x00,0xe9,
	0x40,0x03,0xe0,0x98,0x4e,0x75,0x53,0x02,
	0x7e,0x00,0x3a,0x02,0xe0,0x5d,0x4a,0x05,
	0x67,0x04,0x06,0x45,0x08,0x00,0xe0,0x4d,
	0x48,0x42,0x02,0x82,0x00,0x00,0x00,0xff,
	0xe9,0x8a,0xd4,0x45,0x0c,0x42,0x00,0x04,
	0x65,0x02,0x54,0x42,0x84,0xfc,0x00,0x12,
	0x48,0x42,0x3e,0x02,0x8e,0xfc,0x00,0x09,
	0x48,0x47,0xe1,0x4f,0xe0,0x8f,0x34,0x07,
	0x06,0x82,0x03,0x00,0x80,0x01,0x2a,0x3c,
	0x00,0x00,0x04,0x00,0x3c,0x3c,0x00,0xff,
	0x3e,0x3c,0x09,0x28,0x4e,0x75,0x4f,0xfa,
	0xfc,0x80,0x43,0xfa,0xfc,0xa2,0x4d,0xfa,
	0xfc,0x78,0x2c,0xb9,0x00,0x00,0x05,0x18,
	0x23,0xc9,0x00,0x00,0x05,0x18,0x43,0xfa,
	0x00,0xda,0x4d,0xfa,0xfc,0x68,0x2c,0xb9,
	0x00,0x00,0x05,0x14,0x23,0xc9,0x00,0x00,
	0x05,0x14,0x43,0xfa,0x01,0x6e,0x4d,0xfa,
	0xfc,0x58,0x2c,0xb9,0x00,0x00,0x05,0x04,
	0x23,0xc9,0x00,0x00,0x05,0x04,0x24,0x3c,
	0x03,0x00,0x00,0x04,0x20,0x3c,0x00,0x00,
	0x00,0x8e,0x4e,0x4f,0x12,0x00,0xe1,0x41,
	0x12,0x3c,0x00,0x70,0x33,0xc1,0x00,0x00,
	0x00,0x66,0x26,0x3c,0x00,0x00,0x04,0x00,
	0x43,0xfa,0x00,0x20,0x61,0x04,0x60,0x00,
	0x01,0xec,0x48,0xe7,0x78,0x40,0x70,0x46,
	0x4e,0x4f,0x08,0x00,0x00,0x1e,0x66,0x02,
	0x70,0x00,0x4c,0xdf,0x02,0x1e,0x4e,0x75,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfb,0x8a,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfc,0x6e,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfe,0xa4,0x61,0x00,0xfc,0x78,0x08,0x00,
	0x00,0x1b,0x66,0x30,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x05,0x60,0x08,0x30,0x3c,
	0x01,0xac,0x51,0xc8,0xff,0xfe,0x61,0x00,
	0x00,0xfc,0x08,0x00,0x00,0x1e,0x67,0x2c,
	0x08,0x00,0x00,0x1b,0x66,0x0e,0x08,0x00,
	0x00,0x11,0x66,0x08,0x61,0x00,0xfd,0x4c,
	0x51,0xcc,0xff,0xe4,0x4c,0xdf,0x00,0xf2,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x08,0x00,0x00,0x1f,
	0x66,0xe2,0xd3,0xc5,0x96,0x85,0x63,0xdc,
	0x20,0x04,0x48,0x40,0x38,0x00,0x30,0x3c,
	0x00,0x09,0x52,0x02,0xb0,0x02,0x64,0xae,
	0x14,0x3c,0x00,0x01,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0x98,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfd,0x8c,
	0x08,0x00,0x00,0x1b,0x66,0xae,0x60,0x8e,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfa,0xde,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfb,0xc6,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfd,0xfc,0x61,0x00,0xfb,0xd0,0x08,0x00,
	0x00,0x1b,0x66,0x24,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x11,0x61,0x5e,0x08,0x00,
	0x00,0x0a,0x66,0x14,0x08,0x00,0x00,0x1e,
	0x67,0x16,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0xfc,0xb0,0x51,0xcc,0xff,0xe6,
	0x4c,0xdf,0x00,0xf2,0x60,0x00,0xfb,0x34,
	0x08,0x00,0x00,0x1f,0x66,0xf2,0xd3,0xc5,
	0x96,0x85,0x63,0xec,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x09,0x52,0x02,
	0xb0,0x02,0x64,0xc0,0x14,0x3c,0x00,0x01,
	0x0a,0x42,0x01,0x00,0x08,0x02,0x00,0x08,
	0x66,0xb2,0xd4,0xbc,0x00,0x01,0x00,0x00,
	0x61,0x00,0xfd,0x00,0x08,0x00,0x00,0x1b,
	0x66,0xbe,0x60,0xa0,0x61,0x00,0xfb,0x7c,
	0xe1,0x9a,0x54,0x88,0x20,0xc2,0xe0,0x9a,
	0x10,0xc2,0x10,0xc7,0x10,0x86,0x61,0x00,
	0xfb,0x8a,0x41,0xf8,0x09,0xee,0x70,0x08,
	0x61,0x00,0xfb,0xb8,0x61,0x00,0xfb,0xee,
	0x61,0x00,0xfc,0x0c,0x61,0x00,0xfc,0x26,
	0x4e,0x75,0x43,0xfa,0x01,0x8c,0x61,0x00,
	0x01,0x76,0x24,0x3c,0x03,0x00,0x00,0x06,
	0x32,0x39,0x00,0x00,0x00,0x66,0x26,0x3c,
	0x00,0x00,0x04,0x00,0x43,0xf8,0x28,0x00,
	0x61,0x00,0xfd,0xf6,0x4a,0x80,0x66,0x00,
	0x01,0x20,0x43,0xf8,0x28,0x00,0x49,0xfa,
	0x01,0x54,0x78,0x1f,0x24,0x49,0x26,0x4c,
	0x7a,0x0a,0x10,0x1a,0x80,0x3c,0x00,0x20,
	0xb0,0x1b,0x66,0x06,0x51,0xcd,0xff,0xf4,
	0x60,0x0c,0x43,0xe9,0x00,0x20,0x51,0xcc,
	0xff,0xe4,0x66,0x00,0x00,0xf4,0x30,0x29,
	0x00,0x1a,0xe1,0x58,0x55,0x40,0xd0,0x7c,
	0x00,0x0b,0x34,0x00,0xc4,0x7c,0x00,0x07,
	0x52,0x02,0xe8,0x48,0x64,0x04,0x84,0x7c,
	0x01,0x00,0x48,0x42,0x34,0x3c,0x03,0x00,
	0x14,0x00,0x48,0x42,0x26,0x29,0x00,0x1c,
	0xe1,0x5b,0x48,0x43,0xe1,0x5b,0x43,0xf8,
	0x67,0xc0,0x61,0x00,0xfd,0x8c,0x0c,0x51,
	0x48,0x55,0x66,0x00,0x00,0xb4,0x4b,0xf8,
	0x68,0x00,0x49,0xfa,0x00,0x4c,0x22,0x4d,
	0x43,0xf1,0x38,0xc0,0x2c,0x3c,0x00,0x04,
	0x00,0x00,0x0c,0x69,0x4e,0xd4,0xff,0xd2,
	0x66,0x36,0x0c,0xad,0x4c,0x5a,0x58,0x20,
	0x00,0x04,0x66,0x16,0x2b,0x46,0x00,0x04,
	0x2b,0x4d,0x00,0x08,0x42,0xad,0x00,0x20,
	0x51,0xf9,0x00,0x00,0x07,0x9c,0x4e,0xed,
	0x00,0x02,0x0c,0x6d,0x4e,0xec,0x00,0x1a,
	0x66,0x0e,0x0c,0x6d,0x4e,0xea,0x00,0x2a,
	0x66,0x06,0x43,0xfa,0x01,0x20,0x60,0x64,
	0x10,0x3c,0x00,0xc0,0x41,0xf8,0x68,0x00,
	0x36,0x3c,0xff,0xff,0xb0,0x18,0x67,0x26,
	0x51,0xcb,0xff,0xfa,0x43,0xf8,0x68,0x00,
	0x4a,0x39,0x00,0x00,0x07,0x9c,0x67,0x14,
	0x41,0xf8,0x67,0xcc,0x24,0x18,0xd4,0x98,
	0x22,0x10,0xd1,0xc2,0x53,0x81,0x65,0x04,
	0x42,0x18,0x60,0xf8,0x4e,0xd1,0x0c,0x10,
	0x00,0x04,0x66,0xd0,0x52,0x88,0x0c,0x10,
	0x00,0xd0,0x66,0xc8,0x52,0x88,0x0c,0x10,
	0x00,0xfe,0x66,0xc0,0x52,0x88,0x0c,0x10,
	0x00,0x02,0x66,0xb8,0x57,0x88,0x30,0xfc,
	0x05,0x9e,0x10,0xbc,0x00,0xfb,0x60,0xac,
	0x43,0xfa,0x00,0x93,0x2f,0x09,0x43,0xfa,
	0x00,0x48,0x61,0x2a,0x43,0xfa,0x00,0x47,
	0x61,0x24,0x43,0xfa,0x00,0x53,0x61,0x1e,
	0x43,0xfa,0x00,0x44,0x61,0x18,0x43,0xfa,
	0x00,0x47,0x61,0x12,0x22,0x5f,0x61,0x0e,
	0x32,0x39,0x00,0x00,0x00,0x66,0x70,0x4f,
	0x4e,0x4f,0x70,0xfe,0x4e,0x4f,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x68,0x75,0x6d,0x61,
	0x6e,0x20,0x20,0x20,0x73,0x79,0x73,0x00,
	0x32,0x48,0x44,0x45,0x49,0x50,0x4c,0x00,
	0x1b,0x5b,0x34,0x37,0x6d,0x1b,0x5b,0x31,
	0x33,0x3b,0x32,0x36,0x48,0x00,0x1b,0x5b,
	0x31,0x34,0x3b,0x32,0x36,0x48,0x00,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x00,0x1b,0x5b,0x31,
	0x34,0x3b,0x33,0x34,0x48,0x48,0x75,0x6d,
	0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,0x82,
	0xcc,0x93,0xc7,0x82,0xdd,0x8d,0x9e,0x82,
	0xdd,0x83,0x47,0x83,0x89,0x81,0x5b,0x82,
	0xc5,0x82,0xb7,0x00,0x1b,0x5b,0x31,0x34,
	0x3b,0x33,0x34,0x48,0x4c,0x5a,0x58,0x2e,
	0x58,0x20,0x82,0xcc,0x83,0x6f,0x81,0x5b,
	0x83,0x57,0x83,0x87,0x83,0x93,0x82,0xaa,
	0x8c,0xc3,0x82,0xb7,0x82,0xac,0x82,0xdc,
	0x82,0xb7,0x00,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	論理フォーマット(2HQ)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HQ()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// 合計33セクタへ書き込む(1トラックあたり18セクタ)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=18; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=15; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(IPL2HQ, FALSE);

	// FAT先頭セクタ初期化
	buf[0] = 0xf0;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// 第1FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// 第2FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 11;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL(2HQ)
//	※FORMAT.x v2.31より取得したもの
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HQ[0x200] = {
	0xeb,0xfe,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x02,0x01,0x01,0x00,
	0x02,0xe0,0x00,0x40,0x0b,0xf0,0x09,0x00,
	0x12,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x02,0x00,0x02,0x00,
	0x01,0x02,0x00,0x12,0x00,0x0f,0x00,0x1f,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	論理フォーマット(2DD)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2DD()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// 通常初期化
	memset(buf, 0, sizeof(buf));

	// 合計14セクタへ書き込む(1トラックあたり18セクタ)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=9; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=5; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// トラック0へシーク
	track = Search(0);
	ASSERT(track);

	// IPL加工(2HQ用をベースに作成)
	memcpy(buf, IPL2HQ, sizeof(buf));
	buf[0] = 0x60;
	buf[1] = 0x3c;
	buf[13] = 0x02;
	buf[17] = 0x70;
	buf[19] = 0xa0;
	buf[20] = 0x05;
	buf[21] = 0xf9;
	buf[22] = 0x03;
	buf[24] = 0x09;
	buf[0x168] = 0x00;
	buf[0x169] = 0x08;
	buf[0x16b] = 0x09;
	buf[0x16f] = 0x0c;

	// IPL書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
	memset(buf, 0, sizeof(buf));

	// FAT先頭セクタ初期化
	buf[0] = 0xf9;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// 第1FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// 第2FAT書き込み
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 5;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	オープン
//	※派生クラスの注意点：
//		writep, readonlyを適切に設定する
//		path, nameを適切に設定する
//	※上位クラスの注意点：
//		呼び出した後で、現在のシリンダへシークする
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Open(const Filepath& /*path*/, DWORD /*offset*/)
{
	// 純粋仮想クラス的
	ASSERT(FALSE);
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	書き込み禁止設定
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::WriteP(BOOL flag)
{
	ASSERT(this);

	// ReadOnlyなら、常に書き込み禁止
	if (IsReadOnly()) {
		disk.writep = TRUE;
		return;
	}

	// 設定
	disk.writep = flag;
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Flush()
{
	ASSERT(this);

	// 何もしない
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ディスク名取得
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::GetName(char *buf) const
{
	ASSERT(this);
	ASSERT(buf);

	strcpy(buf, disk.name);
}

//---------------------------------------------------------------------------
//
//	パス名取得
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::GetPath(Filepath& path) const
{
	ASSERT(this);

	// 代入
	path = disk.path;
}

//---------------------------------------------------------------------------
//
//	シーク
//	※派生クラスの注意点：
//		現在のhead[]をファイルに書き戻し、新しいトラックをロード、head[]へセット
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Seek(int /*c*/)
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	リードID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタすべてID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadID(DWORD *buf, BOOL mfm, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(buf);
	ASSERT((hd == 0) || (hd == 4));

	// トラック取得
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// NULLならNODATA
	if (!track) {
		// 検索にかかる時間を設定
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// トラックに任せる
	return track->ReadID(buf, mfm);
}

//---------------------------------------------------------------------------
//
//	リードセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_NOCYL		検索中にIDのCが一致でず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		DATAフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// トラック取得
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// NULLならNODATA
	if (!track) {
		// 検索にかかる時間を設定
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// トラックに任せる
	return track->ReadSector(buf, len, mfm, chrn);
}

//---------------------------------------------------------------------------
//
//	ライトセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTWRITE	メディアは書き込み禁止
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_NOCYL		検索中にIDのCが一致でず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// 書き込みチェック
	if (IsWriteP()) {
		return FDD_NOTWRITE;
	}

	// トラック取得
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// NULLならNODATA
	if (!track) {
		// 検索にかかる時間を設定
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// トラックに任せる
	return track->WriteSector(buf, len, mfm, chrn, deleted);
}

//---------------------------------------------------------------------------
//
//	リードダイアグ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		データフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// トラック取得
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// NULLならNODATA
	if (!track) {
		// 検索にかかる時間を設定
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// トラックに任せる
	return track->ReadDiag(buf, len, mfm, chrn);
}

//---------------------------------------------------------------------------
//
//	ライトID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTWRITE	メディアは書き込み禁止
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl)
{
	FDITrack *track;

	ASSERT(this);
	ASSERT(sc > 0);

	// 書き込みチェック
	if (IsWriteP()) {
		return FDD_NOTWRITE;
	}

	// トラック取得
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// NULLならNo Errorとする(format.x v2.20で154トラックにWrite ID)
	if (!track) {
		return FDD_NOERROR;
	}

	// トラックに任せる
	return track->WriteID(buf, d, sc, mfm, gpl);
}

//---------------------------------------------------------------------------
//
//	回転位置取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDisk::GetRotationPos() const
{
	ASSERT(this);
	ASSERT(GetFDI());

	// 親に聞く
	return GetFDI()->GetRotationPos();
}

//---------------------------------------------------------------------------
//
//	回転時間取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDisk::GetRotationTime() const
{
	ASSERT(this);

	ASSERT(GetFDI());

	// 親に聞く
	return GetFDI()->GetRotationTime();
}

//---------------------------------------------------------------------------
//
//	検索長さ算出
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::CalcSearch(DWORD pos)
{
	DWORD cur;
	DWORD hus;

	ASSERT(this);

	// 取得
	cur = GetRotationPos();
	hus = GetRotationTime();

	// カレント<ポジションなら、差を出すのみ
	if (cur < pos) {
		SetSearch(pos - cur);
		return;
	}

	// ポジション < カレントは、１周を超えている
	ASSERT(cur <= hus);
	pos += (hus - cur);
	SetSearch(pos);
}

//---------------------------------------------------------------------------
//
//	HDフラグ取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::IsHD() const
{
	ASSERT(this);
	ASSERT(GetFDI());

	// 親に聞く
	return GetFDI()->IsHD();
}

//---------------------------------------------------------------------------
//
//	トラック追加
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::AddTrack(FDITrack *track)
{
	FDITrack *ptr;

	ASSERT(this);
	ASSERT(track);

	// トラックを持っていなければ、そのまま追加
	if (!disk.first) {
		disk.first = track;
		disk.first->SetNext(NULL);
		return;
	}

	// 最終トラックを得る
	ptr = disk.first;
	while (ptr->GetNext()) {
		ptr = ptr->GetNext();
	}

	// 最終トラックに追加
	ptr->SetNext(track);
	track->SetNext(NULL);
}

//---------------------------------------------------------------------------
//
//	トラック全削除
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::ClrTrack()
{
	FDITrack *track;

	ASSERT(this);

	// トラックをすべて削除
	while (disk.first) {
		track = disk.first->GetNext();
		delete disk.first;
		disk.first = track;
	}
}

//---------------------------------------------------------------------------
//
//	トラックサーチ
//
//---------------------------------------------------------------------------
FDITrack* FASTCALL FDIDisk::Search(int track) const
{
	FDITrack *p;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));

	// 最初のトラックを取得
	p = GetFirst();

	// ループ
	while (p) {
		if (p->GetTrack() == track) {
			return p;
		}
		p = p->GetNext();
	}

	// 見つからない
	return NULL;
}

//===========================================================================
//
//	FDI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDI::FDI(FDD *fdd)
{
	ASSERT(fdd);

	// ワーク初期化
	fdi.fdd = fdd;
	fdi.disks = 0;
	fdi.first = NULL;
	fdi.disk = NULL;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDI::~FDI()
{
	ClrDisk();
}

//---------------------------------------------------------------------------
//
//	オープン
//	※上位クラスの注意点：
//		呼び出した後で、現在のシリンダへシークする
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Open(const Filepath& path, int media = 0)
{
	FDIDisk2HD *disk2hd;
	FDIDiskDIM *diskdim;
	FDIDiskD68 *diskd68;
	FDIDisk2DD *disk2dd;
	FDIDisk2HQ *disk2hq;
	FDIDiskBAD *diskbad;
	int i;
	int num;
	DWORD offset[0x10];

	ASSERT(this);
	ASSERT((media >= 0) && (media < 0x10));

	// 既にオープンされているならおかしい
	ASSERT(!GetDisk());
	ASSERT(!GetFirst());
	ASSERT(fdi.disks == 0);

	// DIMファイルとしてトライ
	diskdim = new FDIDiskDIM(0, this);
	if (diskdim->Open(path, 0)) {
		AddDisk(diskdim);
		fdi.disk = diskdim;
		return TRUE;
	}
	// 失敗
	delete diskdim;

	// D68ファイルとしてトライ(枚数を数える)
	num = FDIDiskD68::CheckDisks(path, offset);
	if (num > 0) {
		// D68ディスク作成ループ(枚数分だけ追加)
		for (i=0; i<num; i++) {
			diskd68 = new FDIDiskD68(i, this);
			if (!diskd68->Open(path, offset[i])) {
				// 失敗
				delete diskd68;
				ClrDisk();
				return FALSE;
			}
			AddDisk(diskd68);
		}
		// メディアセレクト
		fdi.disk = Search(media);
		if (!fdi.disk) {
			ClrDisk();
			return FALSE;
		}
		return TRUE;
	}

	// 2HDファイルとしてトライ
	disk2hd = new FDIDisk2HD(0, this);
	if (disk2hd->Open(path, 0)) {
		AddDisk(disk2hd);
		fdi.disk = disk2hd;
		return TRUE;
	}
	// 失敗
	delete disk2hd;

	// 2DDファイルとしてトライ
	disk2dd = new FDIDisk2DD(0, this);
	if (disk2dd->Open(path, 0)) {
		AddDisk(disk2dd);
		fdi.disk = disk2dd;
		return TRUE;
	}
	// 失敗
	delete disk2dd;

	// 2HQファイルとしてトライ
	disk2hq = new FDIDisk2HQ(0, this);
	if (disk2hq->Open(path, 0)) {
		AddDisk(disk2hq);
		fdi.disk = disk2hq;
		return TRUE;
	}
	// 失敗
	delete disk2hq;

	// BADファイルとしてトライ
	diskbad = new FDIDiskBAD(0, this);
	if (diskbad->Open(path, 0)) {
		AddDisk(diskbad);
		fdi.disk = diskbad;
		return TRUE;
	}
	// 失敗
	delete diskbad;

	// エラー
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	ID取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetID() const
{
	ASSERT(this);

	// ノットレディならNULL
	if (!IsReady()) {
		return MAKEID('N', 'U', 'L', 'L');
	}

	// ディスクに聞く
	return GetDisk()->GetID();
}

//---------------------------------------------------------------------------
//
//	マルチディスクか
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsMulti() const
{
	ASSERT(this);

	// ディスクが2枚以上ならTRUE
	if (GetDisks() >= 2) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	メディア番号を取得
//
//---------------------------------------------------------------------------
int FASTCALL FDI::GetMedia() const
{
	ASSERT(this);

	// ディスクがなければ0
	if (!GetDisk()) {
		return 0;
	}

	// ディスクに聞くだけ
	return GetDisk()->GetIndex();
}

//---------------------------------------------------------------------------
//
//	ディスク名取得
//
//---------------------------------------------------------------------------
void FASTCALL FDI::GetName(char *buf, int index) const
{
	FDIDisk *disk;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(index >= -1);
	ASSERT(index < GetDisks());

	// -1の時は、カレントを意味する
	if (index < 0) {
		// ノットレディなら、空文字列
		if (!IsReady()) {
			buf[0] = '\0';
			return;
		}

		// カレントに聞く
		GetDisk()->GetName(buf);
		return;
	}

	// インデックスつきなので、検索
	disk = Search(index);
	if (!disk) {
		buf[0] = '\0';
		return;
	}
	disk->GetName(buf);
}

//---------------------------------------------------------------------------
//
//	パス取得
//
//---------------------------------------------------------------------------
void FASTCALL FDI::GetPath(Filepath& path) const
{
	ASSERT(this);

	// ノットレディなら、空文字列
	if (!IsReady()) {
		path.Clear();
		return;
	}

	// ディスクに聞く
	GetDisk()->GetPath(path);
}

//---------------------------------------------------------------------------
//
//	レディチェック
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsReady() const
{
	ASSERT(this);

	// カレントメディアがあればTRUE、なければFALSE
	if (GetDisk()) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	書き込み禁止か
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsWriteP() const
{
	// ノットレディならFALSE
	if (!IsReady()) {
		return FALSE;
	}

	// ディスクに聞く
	return GetDisk()->IsWriteP();
}

//---------------------------------------------------------------------------
//
//	Read Onlyディスクイメージか
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsReadOnly() const
{
	// ノットレディならFALSE
	if (!IsReady()) {
		return FALSE;
	}

	// ディスクに聞く
	return GetDisk()->IsReadOnly();
}

//---------------------------------------------------------------------------
//
//	書き込み禁止セット
//
//---------------------------------------------------------------------------
void FASTCALL FDI::WriteP(BOOL flag)
{
	ASSERT(this);

	// レディなら、指示
	if (IsReady()) {
		GetDisk()->WriteP(flag);
	}
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDI::Seek(int c)
{
	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// ノットレディなら何もしない
	if (!IsReady()) {
		return;
	}

	// ディスクに通知
	GetDisk()->Seek(c);
}

//---------------------------------------------------------------------------
//
//	リードID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTREADY	ノットレディ
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタすべてID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadID(DWORD *buf, BOOL mfm, int hd)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((hd == 0) || (hd == 4));

	// ノットレディ判定
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// ディスクに任せる
	return GetDisk()->ReadID(buf, mfm, hd);
}

//---------------------------------------------------------------------------
//
//	リードセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTREADY	ノットレディ
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_NOCYL		検索中にIDのCが一致でず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		DATAフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// ノットレディ判定
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// ディスクに任せる
	return GetDisk()->ReadSector(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	ライトセクタ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTREADY	ノットレディ
//		FDD_NOTWRITE	メディアは書き込み禁止
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_NOCYL		検索中にIDのCが一致でず、FFでないセクタを見つけた
//		FDD_BADCYL		検索中にIDのCが一致せず、FFとなっているセクタを見つけた
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDI::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// ノットレディ判定
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// ディスクに任せる
	return GetDisk()->WriteSector(buf, len, mfm, chrn, hd, deleted);
}

//---------------------------------------------------------------------------
//
//	リードダイアグ
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_MAM			指定密度ではアンフォーマット
//		FDD_NODATA		指定密度ではアンフォーマットか、
//						または有効なセクタを全検索してRが一致しない
//		FDD_IDCRC		IDフィールドにCRCエラーがある
//		FDD_DATACRC		データフィールドにCRCエラーがある
//		FDD_DDAM		デリーテッドセクタである
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// ノットレディ判定
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// ディスクに任せる
	return GetDisk()->ReadDiag(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	ライトID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTREADY	ノットレディ
//		FDD_NOTWRITE	メディアは書き込み禁止
//
//---------------------------------------------------------------------------
int FASTCALL FDI::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl)
{
	ASSERT(this);
	ASSERT(sc > 0);
	ASSERT((hd == 0) || (hd == 4));

	// ノットレディ判定
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// ディスクに任せる
	return GetDisk()->WriteID(buf, d, sc, mfm, hd, gpl);
}

//---------------------------------------------------------------------------
//
//	回転位置取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetRotationPos() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// 親に聞く
	return GetFDD()->GetRotationPos();
}

//---------------------------------------------------------------------------
//
//	回転時間取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetRotationTime() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// 親に聞く
	return GetFDD()->GetRotationTime();
}

//---------------------------------------------------------------------------
//
//	検索時間取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetSearch() const
{
	FDIDisk *disk;

	ASSERT(this);

	// ノットレディなら常に0
	disk = GetDisk();
	if (!disk) {
		return 0;
	}

	// ディスクに聞く
	return disk->GetSearch();
}

//---------------------------------------------------------------------------
//
//	HDフラグ取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsHD() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// 親に聞く
	return GetFDD()->IsHD();
}

//---------------------------------------------------------------------------
//
//	ディスク追加
//
//---------------------------------------------------------------------------
void FASTCALL FDI::AddDisk(FDIDisk *disk)
{
	FDIDisk *ptr;

	ASSERT(this);
	ASSERT(disk);

	// ディスクを持っていなければ、そのまま追加
	if (!fdi.first) {
		fdi.first = disk;
		fdi.first->SetNext(NULL);

		// カウンタUp
		fdi.disks++;
		return;
	}

	// 最終ディスクを得る
	ptr = fdi.first;
	while (ptr->GetNext()) {
		ptr = ptr->GetNext();
	}

	// 最終ディスクに追加
	ptr->SetNext(disk);
	disk->SetNext(NULL);

	// カウンタUp
	fdi.disks++;
}

//---------------------------------------------------------------------------
//
//	ディスク全削除
//
//---------------------------------------------------------------------------
void FASTCALL FDI::ClrDisk()
{
	FDIDisk *disk;

	ASSERT(this);

	// ディスクをすべて削除
	while (fdi.first) {
		disk = fdi.first->GetNext();
		delete fdi.first;
		fdi.first = disk;
	}

	// カウンタ0
	fdi.disks = 0;
}

//---------------------------------------------------------------------------
//
//	ディスク検索
//
//---------------------------------------------------------------------------
FDIDisk* FASTCALL FDI::Search(int index) const
{
	FDIDisk *disk;

	ASSERT(this);
	ASSERT(index >= 0);
	ASSERT(index < GetDisks());

	// 最初のディスクを取得
	disk = GetFirst();

	// 比較ループ
	while (disk) {
		if (disk->GetIndex() == index) {
			return disk;
		}

		// 次へ
		disk = disk->GetNext();
	}

	// 見つからない
	return NULL;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Save(Fileio *fio, int ver)
{
	BOOL ready;
	FDIDisk *disk;
	Filepath path;
	int i;
	int disks;
	int media;

	ASSERT(this);
	ASSERT(fio);

	// レディフラグを書き込む
	ready = IsReady();
	if (!fio->Write(&ready, sizeof(ready))) {
		return FALSE;
	}

	// レディでなければ終了
	if (!ready) {
		return TRUE;
	}

	// 全メディアをフラッシュ
	disks = GetDisks();
	for (i=0; i<disks; i++) {
		disk = Search(i);
		ASSERT(disk);
		if (!disk->Flush()) {
			return FALSE;
		}
	}

	// メディアを書き込む
	media = GetMedia();
	if (!fio->Write(&media, sizeof(media))) {
		return FALSE;
	}

	// パスを書き込む
	GetPath(path);
	if (!path.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Load(Fileio *fio, int ver, BOOL *ready, int *media, Filepath& path)
{
	ASSERT(this);
	ASSERT(fio);
	ASSERT(ready);
	ASSERT(media);
	ASSERT(!IsReady());
	ASSERT(GetDisks() == 0);

	// レディフラグを読み込む
	if (!fio->Read(ready, sizeof(BOOL))) {
		return FALSE;
	}

	// レディでなければ終了
	if (!(*ready)) {
		return TRUE;
	}

	// メディアを読み込む
	if (!fio->Read(media, sizeof(int))) {
		return FALSE;
	}

	// パスを読み込む
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	調整(特殊)
//
//---------------------------------------------------------------------------
void FASTCALL FDI::Adjust()
{
	FDIDisk *disk;
	FDIDiskD68 *disk68;

	ASSERT(this);

	// 複数イメージの場合のみ
	if (!IsMulti()) {
		return;
	}

	disk = GetFirst();
	while (disk) {
		// D68の場合のみ
		if (disk->GetID() == MAKEID('D', '6', '8', ' ')) {
			disk68 = (FDIDiskD68*)disk;
			disk68->AdjustOffset();
		}

		disk = disk->GetNext();
	}
}

//===========================================================================
//
//	FDIトラック(2HD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrack2HD::FDITrack2HD(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2HD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset & 0x1fff) == 0);
	ASSERT(offset < 0x134000);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// C・H・N決定
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 3;

	// 読み込みオープン
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// シーク
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// ループ
	for (i=0; i<8; i++) {
		// データ読み込み
		if (!fio.Read(buf, sizeof(buf))) {
			// 途中まで追加した分を削除する
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// セクタ作成
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);

		// セクタ追加
		AddSector(sector);
	}

	// クローズ
	fio.Close();

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDIディスク(2HD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2HD::FDIDisk2HD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('2', 'H', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2HD::~FDIDisk2HD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2HD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ファイルサイズが1261568であることを確かめる
	size = fio.GetFileSize();
	if (size != 1261568) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス、オフセットを記憶
	disk.path = path;
	disk.offset = offset;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// トラックを作成(0〜76シリンダまで、77*2トラック)
	for (i=0; i<154; i++) {
		track = new FDITrack2HD(this, i);
		AddTrack(track);
	}

	// 終了
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2HD::Seek(int c)
{
	int i;
	FDITrack2HD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// トラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = (FDITrack2HD*)GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		track->Save(disk.path, offset);
	}

	// cは75まで許可。範囲外であればhead[i]=NULLとする
	if ((c < 0) || (c > 76)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrack2HD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// ロード
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	新規ディスク作成
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2HD *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// 物理フォーマットは2HDのみ許可
	if (opt->phyfmt != FDI_2HD) {
		return FALSE;
	}

	// ファイル作成を試みる
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// パス名、オフセットを記録
	disk.path = path;
	disk.offset = 0;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// 0〜153に限り、トラックを作成して物理フォーマット
	for (i=0; i<154; i++) {
		track = new FDITrack2HD(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// 論理フォーマット
	FDIDisk::Create(path, opt);

	// 書き込みループ
	offset = 0;
	for (i=0; i<154; i++) {
		// トラック取得
		track = (FDITrack2HD*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// 書き込み
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// 次へ
		offset += (0x400 * 8);
	}

	// 成功
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Flush()
{
	int i;
	DWORD offset;
	FDITrack *track;

	ASSERT(this);

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDIトラック(DIM)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrackDIM::FDITrackDIM(FDIDisk *disk, int track, int type) : FDITrack(disk, track)
{
	// タイプに応じてdim_mfm, dim_secs, dim_nを決める
	switch (type) {
		// 2HD (N=3,8セクタ)
		case 0:
			dim_mfm = TRUE;
			dim_secs = 8;
			dim_n = 3;
			break;

		// 2HS (N=3,9セクタ)
		case 1:
			dim_mfm = TRUE;
			dim_secs = 9;
			dim_n = 3;
			break;

		// 2HC (N=2,15セクタ)
		case 2:
			dim_mfm = TRUE;
			dim_secs = 15;
			dim_n = 2;
			break;

		// 2HDE (N=3,9セクタ)
		case 3:
			dim_mfm = TRUE;
			dim_secs = 9;
			dim_n = 3;
			break;

		// 2HQ (N=2,18セクタ)
		case 9:
			dim_mfm = TRUE;
			dim_secs = 18;
			dim_n = 2;
			break;

		// N88-BASIC (26セクタ、トラック0のみ単密)
		case 17:
			dim_secs = 26;
			if (track == 0) {
				dim_mfm = FALSE;
				dim_n = 0;
			}
			else {
				dim_mfm = TRUE;
				dim_n = 1;
			}
			break;

		// その他
		default:
			ASSERT(FALSE);
			break;
	}

	dim_type = type;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackDIM::Load(const Filepath& path, DWORD offset, BOOL load)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	int num;
	int len;
	int gap;
	FDISector *sector;

	ASSERT(this);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// C・H・N決定
	chrn[0] = GetTrack() >> 1;
	chrn[3] = GetDIMN();

	// 読み込みオープン
	if (load) {
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// シーク
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
	}

	// ロード準備
	num = GetDIMSectors();
	len = 1 << (GetDIMN() + 7);
	ASSERT(len <= sizeof(buf));
	switch (GetDIMN()) {
		case 0:
			gap = 0x14;
			break;
		case 1:
			gap = 0x33;
			break;
		case 2:
			gap = 0x54;
			break;
		case 3:
			if (GetDIMSectors() == 8) {
				gap = 0x74;
			}
			else {
				gap = 0x39;
			}
			break;
		default:
			ASSERT(FALSE);
			fio.Close();
			return FALSE;
	}

	// 初期データ作成(load==FALSEの場合)
	memset(buf, 0xe5, len);

	// ループ
	for (i=0; i<num; i++) {
		// データ読み込み
		if (load) {
			if (!fio.Read(buf, len)) {
				// 途中まで追加した分を削除する
				ClrSector();
				fio.Close();
				return FALSE;
			}
		}

		// HとRを決める (2HS, 2HDEで特例あり)
		chrn[1] = GetTrack() & 1;
		chrn[2] = i + 1;
		if (dim_type == 1) {
			chrn[2] = i + 10;
			if ((GetTrack() == 0) && (i == 0)) {
				chrn[2] = 1;
			}
		}
		if (dim_type == 3) {
			chrn[1] = chrn[1] + 0x80;
			if ((GetTrack() == 0) && (i == 0)) {
				chrn[1] = 0;
			}
		}

		// セクタ作成
		sector = new FDISector(IsDIMMFM(), chrn);
		sector->Load(buf, len, gap, FDD_NOERROR);

		// セクタ追加
		AddSector(sector);
	}

	// クローズ
	if (load) {
		fio.Close();
	}

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDIディスク(DIM)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDiskDIM::FDIDiskDIM(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('D', 'I', 'M', ' ');

	// ヘッダをクリア
	memset(dim_hdr, 0, sizeof(dim_hdr));

	// ロードなし
	dim_load = FALSE;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDiskDIM::~FDIDiskDIM()
{
	// ロードされていれば書き込み
	if (dim_load) {
		Save();
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrackDIM *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ファイルサイズが256以上あることを確かめる
	size = fio.GetFileSize();
	if (size < 0x100) {
		fio.Close();
		return FALSE;
	}

	// ヘッダ読み込み、認識文字列チェック
	fio.Read(dim_hdr, sizeof(dim_hdr));
	fio.Close();
	if (strcmp((char*)&dim_hdr[171], "DIFC HEADER  ") != 0) {
		return FALSE;
	}

	// パス名＋オフセットを記録
	disk.path = path;
	disk.offset = offset;

	// コメントがあるか
	if (dim_hdr[0xc2] != '\0') {
		// ディスク名はコメントとする(必ず60文字で切る)
		dim_hdr[0xc2 + 60] = '\0';
		strcpy(disk.name, (char*)&dim_hdr[0xc2]);
	}
	else {
		// ディスク名はファイル名＋拡張子とする
		strcpy(disk.name, path.GetShort());
	}

	// トラックを作成(0〜81シリンダまで、82*2トラック)
	for (i=0; i<164; i++) {
		track = new FDITrackDIM(this, i, dim_hdr[0]);
		AddTrack(track);
	}

	// フラグUp、終了
	dim_load = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskDIM::Seek(int c)
{
	int i;
	FDITrackDIM *track;
	DWORD offset;
	BOOL flag;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));
	ASSERT(dim_load);

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrackDIM*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// マップを見て、有効トラックならロード
		flag = FALSE;
		offset = 0;
		if (GetDIMMap(c * 2 + i)) {
			// オフセット計算
			offset = GetDIMOffset(c * 2 + i);
			flag = TRUE;
		}

		// ロードまたは作成
		track->Load(disk.path, offset, flag);
	}
}

//---------------------------------------------------------------------------
//
//	DIMトラックマップを取得
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::GetDIMMap(int track) const
{
	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(dim_load);

	if (dim_hdr[track + 1] != 0) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	DIMトラックオフセットを取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDiskDIM::GetDIMOffset(int track) const
{
	int i;
	DWORD offset;
	int length;
	FDITrackDIM *dim;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(dim_load);

	// ベースは256
	offset = 0x100;

	// 前のセクタまでの合算とする
	for (i=0; i<track; i++) {
		// 有効トラックなら合算
		if (GetDIMMap(i)) {
			dim = (FDITrackDIM*)Search(track);
			ASSERT(dim);
			length = 1 << (dim->GetDIMN() + 7);
			length *= dim->GetDIMSectors();
			offset += length;
		}
	}

	return offset;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Save()
{
	BOOL changed;
	int i;
	FDITrackDIM *track;
	DWORD offset;
	Fileio fio;
	DWORD total;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(dim_load);

	// マップ以外のトラックで変更されたものがあるか、まず調べる
	changed = FALSE;
	for (i=0; i<164; i++) {
		if (!GetDIMMap(i)) {
			// マップにないトラック。変更あるか
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			if (track->IsChanged()) {
				changed = TRUE;
			}
		}
	}

	// 変更がマップのみなら、個別対応
	if (!changed) {
		for (i=0; i<164; i++) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			if (track->IsChanged()) {
				// マップ済みのトラック
				ASSERT(GetDIMMap(i));
				offset = GetDIMOffset(i);
				if (!track->Save(disk.path, offset)) {
					return FALSE;
				}
			}
		}
		// 全トラック終了
		return TRUE;
	}

	// マップ済みトラックをすべてロード
	for (i=0; i<164; i++) {
		if (GetDIMMap(i)) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			offset = GetDIMOffset(i);
			if (!track->Load(disk.path, offset, TRUE)) {
				return FALSE;
			}
		}
	}

	// マップされていなくて今回追加されたところをマップ。同時にトータルサイズを得る
	total = 0;
	for (i=0; i<164; i++) {
		track = (FDITrackDIM*)Search(i);
		ASSERT(track);
		if (!GetDIMMap(i)) {
			if (track->IsChanged()) {
				// 新しくマップに追加
				dim_hdr[i + 1] = 0x01;
				total += track->GetTotalLength();
			}
		}
		else {
			// 既にマップされている
			total += track->GetTotalLength();
		}
	}

	// 2HDで154トラック以降が使われていれば、OverTrackフラグを立てる
	dim_hdr[0xff] = 0x00;
	if (dim_hdr[0] == 0x00) {
		for (i=154; i<164; i++) {
			if (GetDIMMap(i)) {
				dim_hdr[0xff] = 0x01;
			}
		}
	}

	// 新規セーブ(サイズをつくる)
	if (!fio.Open(disk.path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(dim_hdr, sizeof(dim_hdr))) {
		fio.Close();
		return FALSE;
	}

	// 256バイトのヘッダ以降は、E5データを最初にセーブ
	try {
		ptr = new BYTE[total];
	}
	catch (...) {
		fio.Close();
		return FALSE;
	}
	if (!ptr) {
		fio.Close();
		return FALSE;
	}
	memset(ptr, 0xe5, total);
	if (!fio.Write(ptr, total)) {
		fio.Close();
		delete[] ptr;
		return FALSE;
	}
	delete[] ptr;

	// 全て書き込む(ファイルは開いたまま)
	for (i=0; i<164; i++) {
		if (GetDIMMap(i)) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			offset = GetDIMOffset(i);
			track->ForceChanged();
			if (!track->Save(&fio, offset)) {
				return FALSE;
			}
		}
	}

	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	新規ディスク作成
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrackDIM *track;
	Fileio fio;
	static BYTE iocsdata[] = {
		0x04, 0x21, 0x03, 0x22, 0x01, 0x00, 0x00, 0x00
	};

	ASSERT(this);
	ASSERT(opt);

	// ヘッダをクリア
	memset(dim_hdr, 0, sizeof(dim_hdr));

	// フォーマットのチェックとタイプ書き込み
	switch (opt->phyfmt) {
		// 2HD(オーバートラック使用を含む)
		case FDI_2HD:
		case FDI_2HDA:
			dim_hdr[0] = 0x00;
			break;

		// 2HS
		case FDI_2HS:
			dim_hdr[0] = 0x01;
			break;

		// 2HC
		case FDI_2HC:
			dim_hdr[0] = 0x02;
			break;

		// 2HDE(68)
		case FDI_2HDE:
			dim_hdr[0] = 0x03;
			break;

		// 2HQ
		case FDI_2HQ:
			dim_hdr[0] = 0x09;
			break;

		// N88-BASIC
		case FDI_N88B:
			dim_hdr[0] = 0x11;
			break;

		// サポートしていない物理フォーマット
		default:
			return FALSE;
	}

	// ヘッダ残り(日付は2001-03-22 00:00:00とする; XM6開発開始日)
	strcpy((char*)&dim_hdr[0xab], "DIFC HEADER  ");
	dim_hdr[0xfe] = 0x19;
	if (opt->phyfmt == FDI_2HDA) {
		dim_hdr[0xff] = 0x01;
	}
	memcpy(&dim_hdr[0xba], iocsdata, 8);
	ASSERT(strlen(opt->name) < 60);
	strcpy((char*)&dim_hdr[0xc2], opt->name);

	// ヘッダ書き込み
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(&dim_hdr[0], sizeof(dim_hdr))) {
		return FALSE;
	}
	fio.Close();

	// フラグ設定
	disk.writep = FALSE;
	disk.readonly = FALSE;
	dim_load = TRUE;

	// パス名＋オフセットを記録
	disk.path = path;
	disk.offset = 0;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// トラックを作成して物理フォーマット
	for (i=0; i<164; i++) {
		track = new FDITrackDIM(this, i, dim_hdr[0x00]);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// 論理フォーマット
	FDIDisk::Create(path, opt);

	// 保存
	if (!Save()) {
		return FALSE;
	}

	// 成功
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Flush()
{
	ASSERT(this);

	// ロードされていれば書き込み
	if (dim_load) {
		return Save();
	}

	// ロードされていない
	return TRUE;
}

//===========================================================================
//
//	FDIトラック(D68)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrackD68::FDITrackD68(FDIDisk *disk, int track, BOOL hd) : FDITrack(disk, track, hd)
{
	// フォーマット変更なし
	d68_format = FALSE;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE header[0x10];
	DWORD chrn[4];
	BYTE buf[0x2000];
	BOOL mfm;
	int len;
	int i;
	int num;
	int gap;
	int stat;
	FDISector *sector;
	BYTE *ptr;
	const int *table;

	ASSERT(this);
	ASSERT(offset > 0);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// オープンとシーク
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	if (!fio.Seek(offset)) {
		return FALSE;
	}

	// 最低1つはセクタがあると仮定(オフセット!=0のため)
	i = 0;
	num = 1;
	while (i < num) {
		// ヘッダ読み
		if (!fio.Read(header, sizeof(header))) {
			break;
		}

		// 初回はセクタ数を取得
		if (i == 0) {
			ptr = &header[0x04];
			num = (int)ptr[1];
			num <<= 8;
			num |= (int)ptr[0];
		}

		// MFM
		mfm = TRUE;
		if (header[0x06] != 0) {
			mfm = FALSE;
		}

		// レングス
		ptr = &header[0x0e];
		len = (int)ptr[1];
		len <<= 8;
		len |= (int)ptr[0];

		// GAP3(D68ファイルには情報がないので、よくあるパターンを用意)
		gap = 0x12;
		table = &Gap3Table[0];
		while (table[0] != 0) {
			// GAPテーブル検索
			if ((table[0] == num) && (table[1] == (int)header[3])) {
				gap = table[2];
				break;
			}
			table += 3;
		}

		// DELETED SECTORを含むステータス
		stat = FDD_NOERROR;
		if (header[0x07] != 0) {
			stat |= FDD_DDAM;
		}
		if ((header[0x08] & 0xf0) == 0xa0) {
			stat |= FDD_DATACRC;
		}

		// バッファへデータをロード
		if (sizeof(buf) < len) {
			break;
		}
		if (!fio.Read(buf, len)) {
			break;
		}

		// セクタ作成
		chrn[0] = (DWORD)header[0];
		chrn[1] = (DWORD)header[1];
		chrn[2] = (DWORD)header[2];
		chrn[3] = (DWORD)header[3];
		sector = new FDISector(mfm, chrn);
		sector->Load(buf, len, gap, stat);

		// 次へ
		AddSector(sector);
		i++;
	}

	fio.Close();

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Save(const Filepath& path, DWORD offset)
{
	FDISector *sector;
	BYTE header[0x10];
	DWORD chrn[4];
	Fileio fio;
	int secs;
	int len;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(offset > 0);

	// セクタ初期化
	sector = GetFirst();

	while (sector) {
		if (!sector->IsChanged()) {
			// 変更されていないのでスキップ
			offset += 0x10;
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// 変更されている。ヘッダを作る
		memset(header, 0, sizeof(header));
		sector->GetCHRN(chrn);
		header[0] = (BYTE)chrn[0];
		header[1] = (BYTE)chrn[1];
		header[2] = (BYTE)chrn[2];
		header[3] = (BYTE)chrn[3];
		secs = GetAllSectors();
		ptr = &header[0x04];
		ptr[1] = (BYTE)(secs >> 8);
		ptr[0] = (BYTE)secs;
		if (!sector->IsMFM()) {
			header[0x06] = 0x40;
		}
		if (sector->GetError() & FDD_DDAM) {
			header[0x07] = 0x10;
		}
		if (sector->GetError() & FDD_IDCRC) {
			header[0x08] = 0xa0;
		}
		if (sector->GetError() & FDD_DATACRC) {
			header[0x08] = 0xa0;
		}
		ptr = &header[0x0e];
		len = sector->GetLength();
		ptr[1] = (BYTE)(len >> 8);
		ptr[0] = (BYTE)len;

		// 書き込み
		if (!fio.IsValid()) {
			// 初回ならファイルオープン
			if (!fio.Open(path, Fileio::ReadWrite)) {
				return FALSE;
			}
		}
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Write(header, sizeof(header))) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// 書き込み完了
		sector->ClrChanged();

		// 次へ
		offset += 0x10;
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// 有効ならファイルクローズ
	if (fio.IsValid()) {
		fio.Close();
	}

	// フォーマットフラグも降ろしておく
	d68_format = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Save(Fileio *fio, DWORD offset)
{
	FDISector *sector;
	BYTE header[0x10];
	DWORD chrn[4];
	int secs;
	int len;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(offset > 0);

	// セクタ初期化
	sector = GetFirst();

	while (sector) {
		if (!sector->IsChanged()) {
			// 変更されていないのでスキップ
			offset += 0x10;
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// 変更されている。ヘッダを作る
		memset(header, 0, sizeof(header));
		sector->GetCHRN(chrn);
		header[0] = (BYTE)chrn[0];
		header[1] = (BYTE)chrn[1];
		header[2] = (BYTE)chrn[2];
		header[3] = (BYTE)chrn[3];
		secs = GetAllSectors();
		ptr = &header[0x04];
		ptr[1] = (BYTE)(secs >> 8);
		ptr[0] = (BYTE)secs;
		if (!sector->IsMFM()) {
			header[0x06] = 0x40;
		}
		if (sector->GetError() & FDD_DDAM) {
			header[0x07] = 0x10;
		}
		if (sector->GetError() & FDD_IDCRC) {
			header[0x08] = 0xa0;
		}
		if (sector->GetError() & FDD_DATACRC) {
			header[0x08] = 0xa0;
		}
		ptr = &header[0x0e];
		len = sector->GetLength();
		ptr[1] = (BYTE)(len >> 8);
		ptr[0] = (BYTE)len;

		// 書き込み
		fio->Seek(offset);
		if (!fio->Write(header, sizeof(header))) {
			return FALSE;
		}
		if (!fio->Write(sector->GetSector(), sector->GetLength())) {
			return FALSE;
		}

		// 書き込み完了
		sector->ClrChanged();

		// 次へ
		offset += 0x10;
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// フォーマットフラグも降ろしておく
	d68_format = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ライトID
//	※次のステータスを返す(エラーはORされる)
//		FDD_NOERROR		エラーなし
//		FDD_NOTWRITE	書き込み禁止
//
//---------------------------------------------------------------------------
int FASTCALL FDITrackD68::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int gpl)
{
	int stat;
	FDISector *sector;
	DWORD pos;
	int i;
	BYTE fillbuf[0x2000];
	DWORD chrn[4];

	ASSERT(this);
	ASSERT(sc > 0);

	// オリジナルを呼ぶ(ライトプロテクトのチェックはFDIDiskで既に行われている)
	stat = FDITrack::WriteID(buf, d, sc, mfm, gpl);
	if (stat == FDD_NOERROR) {
		// フォーマット成功(以前と同一の物理フォーマット)
		return stat;
	}

	// 異なるフォーマット
	d68_format = TRUE;

	// 時間を設定(indexまで)
	pos = GetDisk()->GetRotationTime();
	pos -= GetDisk()->GetRotationPos();
	GetDisk()->SetSearch(pos);

	// bufが与えられていなければここまで
	if (!buf) {
		return FDD_NOERROR;
	}

	// セクタをクリア
	ClrSector();
	memset(fillbuf, d, sizeof(fillbuf));

	// 順にセクタを作成
	for (i=0; i<sc; i++) {
		// レングス>=7はアンフォーマット
		if (buf[i * 4 + 3] >= 0x07) {
			ClrSector();
			return FDD_NOERROR;
		}

		// セクタを作成
		chrn[0] = (DWORD)buf[i * 4 + 0];
		chrn[1] = (DWORD)buf[i * 4 + 1];
		chrn[2] = (DWORD)buf[i * 4 + 2];
		chrn[3] = (DWORD)buf[i * 4 + 3];
		sector = new FDISector(mfm, chrn);
		sector->Load(fillbuf, 1 << (buf[i * 4 + 3] + 7), gpl, FDD_NOERROR);

		// セクタを追加
		AddSector(sector);
	}

	// ポジション計算
	CalcPos();

	return FDD_NOERROR;
}

//---------------------------------------------------------------------------
//
//	D68での長さ取得
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrackD68::GetD68Length() const
{
	DWORD length;
	FDISector *sector;

	ASSERT(this);

	// 初期化
	length = 0;
	sector = GetFirst();

	// ループ
	while (sector) {
		length += 0x10;
		length += sector->GetLength();
		sector = sector->GetNext();
	}

	return length;
}

//---------------------------------------------------------------------------
//
//	GAP3テーブル
//
//---------------------------------------------------------------------------
const int FDITrackD68::Gap3Table[] = {
	// SEC, N, GAP3
	 8, 3, 0x74,						// 2HD
	 9, 3, 0x39,						// 2HS, 2HDE
	15, 2, 0x54,						// 2HC
	18, 2, 0x54,						// 2HQ
	26, 1, 0x33,						// OS-9/68000, N88-BASIC
	26, 0, 0x1a,						// N88-BASIC
	 9, 2, 0x54,						// 2DD(720KB)
	 8, 2, 0x54,						// 2DD(640KB)
	 5, 3, 0x74,						// 2D(Falcom)
	16, 1, 0x33,						// 2D(BASIC)
	 0, 0, 0
};

//===========================================================================
//
//	FDIディスク(D68)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDiskD68::FDIDiskD68(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('D', '6', '8', ' ');

	// ヘッダをクリア
	memset(d68_hdr, 0, sizeof(d68_hdr));

	// ロードなし
	d68_load = FALSE;
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDiskD68::~FDIDiskD68()
{
	// ロードされていれば書き込み
	if (d68_load) {
		Save();
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	int i;
	FDITrackD68 *track;
	BOOL hd;

	ASSERT(this);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// シーク、ヘッダ読み込み
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}
	if (!fio.Read(d68_hdr, sizeof(d68_hdr))) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス名＋オフセットを記録
	disk.path = path;
	disk.offset = offset;

	// ディスク名(必ず16文字で切る)
	d68_hdr[0x10] = 0;
	strcpy(disk.name, (char*)d68_hdr);
	// ただしシングルディスクで、NULLかDefaultならファイル名+拡張子
	if (!GetFDI()->IsMulti()) {
		if (strcmp(disk.name, "Default") == 0) {
			strcpy(disk.name, path.GetShort());
		}
		if (strlen(disk.name) == 0) {
			strcpy(disk.name, path.GetShort());
		}
	}

	// ライトプロテクト
	if (d68_hdr[0x1a] != 0) {
		disk.writep = TRUE;
	}

	// HDフラグ
	switch (d68_hdr[0x1b]) {
		// 2D,2DD
		case 0x00:
		case 0x10:
			hd = FALSE;
			break;
		// 2HD
		case 0x20:
			hd = TRUE;
			break;
		default:
			return FALSE;
	}

	// トラックを作成(0〜81シリンダまで、82*2トラック)
	for (i=0; i<164; i++) {
		track = new FDITrackD68(this, i, hd);
		AddTrack(track);
	}

	// フラグUp、終了
	d68_load = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskD68::Seek(int c)
{
	int i;
	FDITrackD68 *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));
	ASSERT(d68_load);

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrackD68*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// オフセット取得、有効トラックならロード
		if (d68_hdr[0x1b] == 0x00) {
			// 2D
			if (c == 0) {
				offset = GetD68Offset(i);
			}
			else {
				if (c & 1) {
					// 1,3,5...奇数シリンダはそれぞれ1,2,3シリンダ
					offset = GetD68Offset(c + 1 + i);
				}
				else {
					// 2,4,6...偶数シリンダはUnformat
					offset = 0;
				}
			}
		}
		else {
			// 2DD,2HD
			offset = GetD68Offset(c * 2 + i);
		}
		if (offset > 0) {
			track->Load(disk.path, disk.offset + offset);
		}
	}
}

//---------------------------------------------------------------------------
//
//	D68トラックオフセットを取得
//	※無効トラックは0
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDiskD68::GetD68Offset(int track) const
{
	DWORD offset;
	const BYTE *ptr;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(d68_load);

	// ポインタ取得
	ptr = &d68_hdr[0x20 + (track << 2)];

	// オフセット取得(リトルエンディアン)
	offset = (DWORD)ptr[2];
	offset <<= 8;
	offset |= (DWORD)ptr[1];
	offset <<= 8;
	offset |= (DWORD)ptr[0];

	return offset;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Save()
{
	DWORD diskoff[16 + 1];
	BOOL format;
	int i;
	FDITrackD68 *track;
	DWORD offset;
	DWORD length;
	BYTE *fileptr;
	DWORD filelen;
	Fileio fio;
	BYTE *ptr;

	ASSERT(this);

	// ロードされていなければ何もしない
	if (!d68_load) {
		return TRUE;
	}

	// オフセットを再取得する
	memset(diskoff, 0, sizeof(diskoff));
	CheckDisks(disk.path, diskoff);
	disk.offset = diskoff[disk.index];

	// フォーマットの変更が生じているかどうか調べる
	format = FALSE;
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		if (track->IsFormated()) {
			format = TRUE;
		}
	}

	// 生じていなければ、トラック単位で保存
	if (!format) {
		for (i=0; i<164; i++) {
			track = (FDITrackD68*)Search(i);
			ASSERT(track);
			offset = GetD68Offset(i);
			if (offset > 0) {
				if (!track->Save(disk.path, disk.offset + offset)) {
					return FALSE;
				}
			}
		}

		// ライトプロテクトの食い違いがあれば、そこだけ保存
		i = 0;
		if (IsWriteP()) {
			i = 0x10;
		}
		if (d68_hdr[0x1a] != (BYTE)i) {
			d68_hdr[0x1a] = (BYTE)i;
			if (!fio.Open(disk.path, Fileio::ReadWrite)) {
				return FALSE;
			}
			if (!fio.Seek(diskoff[disk.index])) {
				fio.Close();
				return FALSE;
			}
			if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
				fio.Close();
				return FALSE;
			}
			fio.Close();
		}
		return TRUE;
	}

	// ファイルを全てメモリに落とす
	if (!fio.Open(disk.path, Fileio::ReadOnly)) {
		return FALSE;
	}
	filelen = fio.GetFileSize();
	try {
		fileptr = new BYTE[filelen];
	}
	catch (...) {
		fio.Close();
		return FALSE;
	}
	if (!fileptr) {
		fio.Close();
		return FALSE;
	}
	if (!fio.Read(fileptr, filelen)) {
		delete[] fileptr;
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// ヘッダを再構築
	offset = sizeof(d68_hdr);
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		length = track->GetD68Length();
		ptr = &d68_hdr[0x20 + (i << 2)];
		if (length == 0) {
			memset(ptr, 0, 4);
		}
		else {
			ptr[3] = (BYTE)(offset >> 24);
			ptr[2] = (BYTE)(offset >> 16);
			ptr[1] = (BYTE)(offset >> 8);
			ptr[0] = (BYTE)offset;
			offset += length;
		}
	}
	d68_hdr[0x1a] = 0;
	if (IsWriteP()) {
		d68_hdr[0x1a] = 0x10;
	}
	ptr = &d68_hdr[0x1c];
	ptr[3] = (BYTE)(offset >> 24);
	ptr[2] = (BYTE)(offset >> 16);
	ptr[1] = (BYTE)(offset >> 8);
	ptr[0] = (BYTE)offset;

	// ファイルの前を保存
	if (!fio.Open(disk.path, Fileio::WriteOnly)) {
		delete[] fileptr;
		return FALSE;
	}
	if (diskoff[disk.index] != 0) {
		if (!fio.Write(fileptr, diskoff[disk.index])) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
	}

	// ヘッダを保存
	if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
		delete[] fileptr;
		fio.Close();
		return FALSE;
	}
	offset -= sizeof(d68_hdr);

	// サイズ分をつくる
	while (offset > 0) {
		// 1回で書き込みが済む場合
		if (offset < filelen) {
			if (!fio.Write(fileptr, offset)) {
				delete[] fileptr;
				fio.Close();
				return FALSE;
			}
			break;
		}

		// 複数回にわたる場合
		if (!fio.Write(fileptr, filelen)) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
		offset -= filelen;
	}

	// ファイルの後を保存
	if (diskoff[disk.index + 1] != 0) {
		ASSERT(filelen >= diskoff[disk.index + 1]);
		if (!fio.Write(&fileptr[ diskoff[disk.index + 1] ],
				filelen - diskoff[disk.index + 1])) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
	}
	delete[] fileptr;

	// レングス!=0について全てセーブ(強制変更、ファイルは開いたまま)
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		length = track->GetD68Length();
		if (length != 0) {
			offset = GetD68Offset(i);
			ASSERT(offset != 0);
			track->ForceChanged();
			if (!track->Save(&fio, disk.offset + offset)) {
				return FALSE;
			}
		}
	}

	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	新規ディスク作成
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Create(const Filepath& path, const option_t *opt)
{
	Fileio fio;
	int i;
	FDITrackD68 *track;
	BOOL hd;

	ASSERT(this);
	ASSERT(opt);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// ヘッダを作る
	memset(&d68_hdr, 0, sizeof(d68_hdr));
	ASSERT(strlen(opt->name) <= 16);
	strcpy((char*)d68_hdr, opt->name);
	if (opt->phyfmt == FDI_2DD) {
		hd = FALSE;
		d68_hdr[0x1b] = 0x10;
	}
	else {
		hd = TRUE;
		d68_hdr[0x1b] = 0x20;
	}

	// ヘッダを書き込む
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス、ディスク名、オフセット
	disk.path = path;
	strcpy(disk.name, opt->name);
	disk.offset = 0;

	// トラックを作成(0〜81シリンダまで、82*2トラック)
	for (i=0; i<164; i++) {
		track = new FDITrackD68(this, i, hd);
		track->Create(opt->phyfmt);
		track->ForceFormat();
		AddTrack(track);
	}

	// フラグ設定
	disk.writep = FALSE;
	disk.readonly = FALSE;
	d68_load = TRUE;

	// 論理フォーマット
	FDIDisk::Create(path, opt);

	// 保存
	if (!Save()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	D68ファイルの検査
//
//---------------------------------------------------------------------------
int FASTCALL FDIDiskD68::CheckDisks(const Filepath& path, DWORD *offbuf)
{
	Fileio fio;
	DWORD fsize;
	DWORD dsize;
	DWORD base;
	DWORD prev;
	DWORD offset;
	int disks;
	BYTE header[0x2b0];
	BYTE *ptr;
	int i;

	ASSERT(offbuf);

	// 初期化
	disks = 0;
	base = 0;

	// ファイルサイズ取得
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return 0;
	}
	fsize = fio.GetFileSize();
	fio.Close();
	if (fsize < sizeof(header)) {
		return 0;
	}

	// ディスクループ
	while (disks < 16) {
		// サイズオーバなら終了
		if (base >= fsize) {
			break;
		}

		// ヘッダを読む
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return 0;
		}
		if (!fio.Seek(base)) {
			fio.Close();
			break;
		}
		if (!fio.Read(header, sizeof(header))) {
			fio.Close();
			break;
		}
		fio.Close();

		// 密度をチェック
		switch (header[0x1b]) {
			case 0x00:
			case 0x10:
			case 0x20:
				break;
			default:
				return 0;
		}

		// このディスクサイズを取得(0x200以上、1.92MB以下と限定)
		ptr = &header[0x1c];
		dsize = (DWORD)ptr[3];
		dsize <<= 8;
		dsize |= (DWORD)ptr[2];
		dsize <<= 8;
		dsize |= (DWORD)ptr[1];
		dsize <<= 8;
		dsize |= (DWORD)ptr[0];

		if ((dsize + base) > fsize) {
			return 0;
		}
		if (dsize < 0x200) {
			return 0;
		}
		if (dsize > 1920 * 1024) {
			return 0;
		}

		// オフセットを検査。dsizeを超えていなくて単調増加であること
		prev = 0;
		for (i=0; i<164; i++) {
			// 2Dイメージであれば84トラック以上は検査しない(変な値が書き込まれている場合がある)
			if (header[0x1b] == 0x00) {
				if (i >= 84) {
					break;
				}
			}

			// このトラックのオフセットを得る
			ptr = &header[0x20 + (i << 2)];
			offset = (DWORD)ptr[3];
			offset <<= 8;
			offset |= (DWORD)ptr[2];
			offset <<= 8;
			offset |= (DWORD)ptr[1];
			offset <<= 8;
			offset |= (DWORD)ptr[0];

			// オフセットが0x10で割り切れなければエラー
			if (offset & 0x0f) {
				return 0;
			}

			// 0は、そのトラックがアンフォーマットであることを示す
			if (offset != 0) {
				// 0でなければ
				if (prev == 0) {
					// 最初のトラックは2X0から始まること
					if ((offset & 0xffffff0f) != 0x200) {
						return 0;
					}
				}
				else {
					// 単調増加で
					if (offset <= prev) {
						return 0;
					}
					// ディスクサイズを超えていないこと
					if (offset > dsize) {
						return 0;
					}
				}
				prev = offset;
			}
		}

		// 枚数Up、バッファに登録、次へ
		offbuf[disks] = base;
		disks++;
		base += dsize;
	}

	return disks;
}

//---------------------------------------------------------------------------
//
//	オフセット更新
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskD68::AdjustOffset()
{
	DWORD offset[0x10];

	ASSERT(this);

	memset(offset, 0, sizeof(offset));
	CheckDisks(disk.path, offset);
	disk.offset = offset[disk.index];
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Flush()
{
	ASSERT(this);

	// ロードされていれば書き込み
	if (d68_load) {
		return Save();
	}

	// ロードされていない
	return TRUE;
}

//===========================================================================
//
//	FDIトラック(BAD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrackBAD::FDITrackBAD(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);

	// ファイル有効セクタ数0
	bad_secs = 0;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackBAD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// ファイル有効セクタ数0
	bad_secs = 0;

	// C・H・N決定
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 3;

	// 読み込みオープン
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// シーク(失敗でもよい)
	if (!fio.Seek(offset)) {
		// シーク失敗なので、E5で埋める
		memset(buf, 0xe5, sizeof(buf));

		// セクタループ
		for (i=0; i<8; i++) {
			chrn[2] = i + 1;
			sector = new FDISector(TRUE, chrn);
			sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);
			AddSector(sector);
		}

		// クローズ、初期化OK
		fio.Close();
		trk.init = TRUE;
		return TRUE;
	}

	// ループ
	for (i=0; i<8; i++) {
		// バッファを毎回E5で埋める
		memset(buf, 0xe5, sizeof(buf));

		// データ読み込み(失敗してもよい)
		if (fio.Read(buf, sizeof(buf))) {
			// ファイル有効セクタを増やす(0〜8)
			bad_secs++;
		}

		// セクタ作成
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);

		// セクタ追加
		AddSector(sector);
	}

	// クローズ
	fio.Close();

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackBAD::Save(const Filepath& path, DWORD offset)
{
	Fileio fio;
	FDISector *sector;
	BOOL changed;
	int index;

	ASSERT(this);

	// 初期化されていなければ書き込む必要なし
	if (!IsInit()) {
		return TRUE;
	}

	// セクタをまわって、書き込まれているセクタがあるか
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// どれも書き込まれていなければ何もしない
	if (!changed) {
		return TRUE;
	}

	// ファイルオープン
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// ループ
	sector = GetFirst();
	index = 1;
	while (sector) {
		// 変更されていなければ、次へ
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			index++;
			continue;
		}

		// 有効範囲内か
		if (index > bad_secs) {
			// ファイルを超えているので、ダミー処理
			sector->ClrChanged();
			offset += sector->GetLength();
			sector = sector->GetNext();
			index++;
			continue;
		}

		// シーク
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}

		// 書き込み
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// フラグを落とす
		sector->ClrChanged();

		// 次へ
		offset += sector->GetLength();
		sector = sector->GetNext();
		index++;
	}

	// 終了
	fio.Close();
	return TRUE;
}

//===========================================================================
//
//	FDIディスク(BAD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDiskBAD::FDIDiskBAD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('B', 'A', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDiskBAD::~FDIDiskBAD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskBAD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrackBAD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ファイルサイズが1024で割り切れること、1280KB以下であることを確かめる
	size = fio.GetFileSize();
	if (size & 0x3ff) {
		fio.Close();
		return FALSE;
	}
	if (size > 1310720) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス、オフセットを記憶
	disk.path = path;
	disk.offset = offset;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// トラックを作成(0〜76シリンダまで、77*2トラック)
	for (i=0; i<154; i++) {
		track = new FDITrackBAD(this, i);
		AddTrack(track);
	}

	// 終了
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskBAD::Seek(int c)
{
	int i;
	FDITrackBAD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// トラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = (FDITrackBAD*)GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		track->Save(disk.path, offset);
	}

	// cは76まで許可。範囲外であればhead[i]=NULLとする
	if ((c < 0) || (c > 76)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrackBAD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// ロード
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskBAD::Flush()
{
	ASSERT(this);

	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// 書き込み
		if(!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDIトラック(2DD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrack2DD::FDITrack2DD(FDIDisk *disk, int track) : FDITrack(disk, track, FALSE)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2DD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset % 0x1200) == 0);
	ASSERT(offset < 0xb4000);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// C・H・N決定
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 2;

	// 読み込みオープン
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// シーク
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// ループ
	for (i=0; i<9; i++) {
		// データ読み込み
		if (!fio.Read(buf, sizeof(buf))) {
			// 途中まで追加した分を削除する
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// セクタ作成
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x54, FDD_NOERROR);

		// セクタ追加
		AddSector(sector);
	}

	// クローズ
	fio.Close();

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDIディスク(2DD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2DD::FDIDisk2DD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('2', 'D', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2DD::~FDIDisk2DD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// 書き込み
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2DD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ファイルサイズが737280であることを確かめる
	size = fio.GetFileSize();
	if (size != 0xb4000) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス、オフセットを記憶
	disk.path = path;
	disk.offset = offset;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// トラックを作成(0〜79シリンダまで、80*2トラック)
	for (i=0; i<160; i++) {
		track = new FDITrack2DD(this, i);
		AddTrack(track);
	}

	// 終了
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2DD::Seek(int c)
{
	int i;
	FDITrack2DD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// トラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = (FDITrack2DD*)GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// 書き込み
		track->Save(disk.path, offset);
	}

	// cは79まで許可。範囲外であればhead[i]=NULLとする
	if ((c < 0) || (c > 79)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrack2DD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// トラックナンバから、オフセットを算出(x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// ロード
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	新規ディスク作成
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2DD *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// 物理フォーマットは2DDのみ許可
	if (opt->phyfmt != FDI_2DD) {
		return FALSE;
	}

	// ファイル作成を試みる
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// パス名、オフセットを記録
	disk.path = path;
	disk.offset = 0;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// 0〜159に限り、トラックを作成して物理フォーマット
	for (i=0; i<160; i++) {
		track = new FDITrack2DD(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// 論理フォーマット
	FDIDisk::Create(path, opt);

	// 書き込みループ
	offset = 0;
	for (i=0; i<160; i++) {
		// トラック取得
		track = (FDITrack2DD*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// 書き込み
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// 次へ
		offset += (0x200 * 9);
	}

	// 成功
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Flush()
{
	int i;
	DWORD offset;
	FDITrack *track;

	ASSERT(this);

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// 書き込み
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDIトラック(2HQ)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDITrack2HQ::FDITrack2HQ(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2HQ::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset % 0x2400) == 0);
	ASSERT(offset < 0x168000);

	// 初期化済みなら不要(シーク毎に呼ばれるので、１度だけ読んでキャッシュする)
	if (IsInit()) {
		return TRUE;
	}

	// セクタが存在しないこと
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// C・H・N決定
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 2;

	// 読み込みオープン
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// シーク
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// ループ
	for (i=0; i<18; i++) {
		// データ読み込み
		if (!fio.Read(buf, sizeof(buf))) {
			// 途中まで追加した分を削除する
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// セクタ作成
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x54, FDD_NOERROR);

		// セクタ追加
		AddSector(sector);
	}

	// クローズ
	fio.Close();

	// ポジション計算
	CalcPos();

	// 初期化ok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDIディスク(2HQ)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2HQ::FDIDisk2HQ(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// ID設定
	disk.id = MAKEID('2', 'H', 'Q', ' ');
}

//---------------------------------------------------------------------------
//
//	デストラクタ
//
//---------------------------------------------------------------------------
FDIDisk2HQ::~FDIDisk2HQ()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// 書き込み
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	オープン
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2HQ *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// オープンできることを確かめる
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// 読み込みオープンを試みる
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// 読み込みは可�
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// ファイルサイズが1474560であることを確かめる
	size = fio.GetFileSize();
	if (size != 0x168000) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// パス、オフセットを記憶
	disk.path = path;
	disk.offset = offset;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// トラックを作成(0〜79シリンダまで、80*2トラック)
	for (i=0; i<160; i++) {
		track = new FDITrack2HQ(this, i);
		AddTrack(track);
	}

	// 終了
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	シーク
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2HQ::Seek(int c)
{
	int i;
	FDITrack2HQ *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// トラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = (FDITrack2HQ*)GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// 書き込み
		track->Save(disk.path, offset);
	}

	// cは79まで許可。範囲外であればhead[i]=NULLとする
	if ((c < 0) || (c > 79)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// 該当するトラックを検索し、ロード
	for (i=0; i<2; i++) {
		// トラックを検索
		track = (FDITrack2HQ*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// トラックナンバから、オフセットを算出(x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// ロード
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	新規ディスク作成
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2HQ *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// 物理フォーマットは2HQのみ許可
	if (opt->phyfmt != FDI_2HQ) {
		return FALSE;
	}

	// ファイル作成を試みる
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// 書き込み可能として初期化
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// パス名、オフセットを記録
	disk.path = path;
	disk.offset = 0;

	// ディスク名はファイル名＋拡張子とする
	strcpy(disk.name, path.GetShort());

	// 0〜159に限り、トラックを作成して物理フォーマット
	for (i=0; i<160; i++) {
		track = new FDITrack2HQ(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// 論理フォーマット
	FDIDisk::Create(path, opt);

	// 書き込みループ
	offset = 0;
	for (i=0; i<160; i++) {
		// トラック取得
		track = (FDITrack2HQ*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// 書き込み
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// 次へ
		offset += (0x200 * 18);
	}

	// 成功
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	フラッシュ
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Flush()
{
	ASSERT(this);

	int i;
	DWORD offset;
	FDITrack *track;

	// 最後のトラックデータを書き込む
	for (i=0; i<2; i++) {
		// トラックがあるか
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// トラックナンバから、オフセットを算出(x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// 書き込み
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}
