//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ スプライト(CYNTHIA) ]
//
//---------------------------------------------------------------------------

/*

ポートの状態調査結果
---------------------------------------------------------------------------

  画面モードが512×512 若しくは 256×256 の時以外のバスエラー状況

　　$eb0000 - $eb03ff バスエラー
　　$eb0400 - $eb07ff バスエラー
　　$eb0800 - $eb0811 アクセス可能
　　$eb0812 - $eb7fff 常に$ff
　　$eb8000 - $ebffff バスエラー

  画面モードが512×512 若しくは 256×256 の時

　　$eb0000 - $eb03ff アクセス可能
　　$eb0400 - $eb07ff 常に$ff
　　$eb0800 - $eb0811 アクセス可能
　　$eb0812 - $eb7fff 常に$ff
　　$eb8000 - $ebffff アクセス可能

  ワードアクセス、バイトアクセス状況

　　$eb0000 - $eb03ff R：ワード/バイト　W:ワード
　　$eb0400 - $eb07ff 常に$ff
　　$eb0800 - $eb0811 R：ワード/バイト　W:ワード/バイト
　　$eb0812 - $eb7fff 常に$ff
　　$eb8000 - $ebffff R：ワード/バイト　W:ワード

  　＊バイト不可の場合に書き込むと・・・
   　 上位バイトなら下位にも同じ値が書き込まれ、
	  下位バイトなら上位にも同じ値が書き込まれる。


レジスタの有効ビット
---------------------------------------------------------------------------
  スプライトスクロールレジスタのPRW(優先度)はbit2も保持
  BG コントロール($eb0808)のbit10も保持

リセット時のスプライトスクロールレジスタおよびPCG/BGバッファの内容
---------------------------------------------------------------------------
　クリアされない

*/

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "fileio.h"
#include "render.h"
#include "sprite.h"

//===========================================================================
//
//	スプライト
//
//===========================================================================
//#define SPRITE_LOG

//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Sprite::Sprite(VM *p) : MemDevice(p)
{
	// デバイスIDを初期化
	dev.id = MAKEID('S', 'P', 'R', ' ');
	dev.desc = "Sprite (CYNTHIA)";

	// 開始アドレス、終了アドレス
	memdev.first = 0xeb0000;
	memdev.last = 0xebffff;

	// その他
	sprite = NULL;
	render = NULL;
	spr.mem = NULL;
	spr.pcg = NULL;
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Init()
{
	int i;

	ASSERT(this);

	// 基本クラス
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// メモリ確保、クリア
	try {
		sprite = new BYTE[ 0x10000 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!sprite) {
		return FALSE;
	}

	// EB0400-EB07FF, EB0812-EB7FFFはReserved(FF)
	memset(sprite, 0, 0x10000);
	memset(&sprite[0x400], 0xff, 0x400);
	memset(&sprite[0x812], 0xff, 0x77ee);

	// ワーク初期化
	memset(&spr, 0, sizeof(spr));
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// レンダラ取得
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// スプライト HSYNC要求
	for (i=0; i<128; i++) {
		sphsync[i] = 3;
	}

	// BG HSYNC要求
	bghsync = 0x1f;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Cleanup()
{
	// メモリ解放
	if (sprite) {
		delete[] sprite;
		sprite = NULL;
		spr.mem = NULL;
		spr.pcg = NULL;
	}

	// 基本クラスへ
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "リセット");

	// レジスタワーク(EB0800-EB0811)をクリア
	memset(&sprite[0x800], 0, 0x12);

	// レジスタ設定
	spr.connect = FALSE;
	spr.disp = FALSE;

	// BGページ初期化
	for (i=0; i<2; i++) {
		spr.bg_on[i] = FALSE;
		spr.bg_area[i] = 0;
		spr.bg_scrlx[i] = 0;
		spr.bg_scrly[i] = 0;
	}

	// BGサイズ初期化
	spr.bg_size = FALSE;

	// タイミング初期化
	spr.h_total = 0;
	spr.h_disp = 0;
	spr.v_disp = 0;
	spr.lowres = FALSE;
	spr.v_res = 0;
	spr.h_res = 0;

	// スプライト HSYNC要求
	for (i=0; i<128; i++) {
		sphsync[i] = 3;
	}

	// BG HSYNC要求
	bghsync = 0x1f;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(spr.mem);

	LOG0(Log::Normal, "セーブ");

	// サイズをセーブ
	sz = sizeof(sprite_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// 実体をセーブ
	if (!fio->Write(&spr, (int)sz)) {
		return FALSE;
	}

	// メモリをセーブ
	if (!fio->Write(sprite, 0x10000)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Sprite::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(spr.mem);

	LOG0(Log::Normal, "ロード");

	// サイズをロード、照合
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(sprite_t)) {
		return FALSE;
	}

	// 実体をロード
	if (!fio->Read(&spr, (int)sz)) {
		return FALSE;
	}

	// メモリをロード
	if (!fio->Read(sprite, 0x10000)) {
		return FALSE;
	}

	// ポインタを上書き
	spr.mem = &sprite[0x0000];
	spr.pcg = &sprite[0x8000];

	// レンダラへ通知
	NotifyRender();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	設定適用
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "設定適用");

	UNREFERENCED_PARAMETER(config);
}

//---------------------------------------------------------------------------
//
//	バイト読み込み
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// オフセット算出
	addr &= 0xffff;

	// ウェイト(エトワールプリンセス)
	if (addr & 1) {
		if (spr.disp) {
			scheduler->Wait(7);
		}
		else {
			scheduler->Wait(4);
		}
	}

	// 0800～7FFFはバスエラーの影響を受けない
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return sprite[addr ^ 1];
	}

	// 接続チェック
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// エンディアンを反転させて読み込み
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	ワード読み込み
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	// オフセット算出
	addr &= 0xffff;

	// ウェイト(エトワールプリンセス)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(4);
	}

	// 0800～7FFFはバスエラーの影響を受けない
	if ((addr >= 0x800) && (addr < 0x8000)) {
		return *(WORD *)(&sprite[addr]);
	}

	// 接続チェック
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xffff;
	}

	// 読み込み
	return *(WORD *)(&sprite[addr]);
}

//---------------------------------------------------------------------------
//
//	バイト書き込み
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	// オフセット算出
	addr &= 0xffff;

	// ウェイト(エトワールプリンセス)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(3);
	}

	// 800～811はコントロールレジスタ
	if ((addr >= 0x800) && (addr < 0x812)) {
		// 一致チェック
		if (sprite[addr ^ 1] == data) {
			return;
		}

		// データ書き込み
		sprite[addr ^ 1] = (BYTE)data;

		// ワードサイズデータ生成
		addr &= 0xfffe;
		data = *(WORD *)(&sprite[addr]);

		// レジスタマスク
		switch (addr & 0xff) {
			// BGスクロールレジスタ
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06:
				data &= 0x03ff;
				break;

			// BGコントロール(bit10有効)
			case 0x08:
				data &= 0x063f;
				break;

			// 水平トータル
			case 0x0a:
				data &= 0x00ff;
				break;

			// 水平表示位置
			case 0x0c:
				data &= 0x003f;
				break;

			// 垂直表示位置
			case 0x0e:
				data &= 0x00ff;
				break;

			// 解像度設定
			case 0x10:
				data &= 0x001f;
				break;
		}

		// マスクデータ書き込み
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// コントロール
		Control(addr, data);
		NotifyPx68kBGWrite(addr, (WORD)data);
		return;
	}

	// 0812-7FFFはリザーブ(バスエラーの影響を受けない)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// 接続チェック
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// 0400-07FFはリザーブ(バスエラーの影響を受ける)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// 0000-03FFと8000-FFFFはバイトアクセスできない
	// 上位、下位バイトが同じ値でワードアクセスになる。
	data = (data<<8) | data;
	addr &= 0xfffe;
	WriteWord(memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	ワード書き込み
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::WriteWord(DWORD addr, DWORD data)
{
	int index;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// オフセット算出
	addr &= 0xfffe;

	// ウェイト(エトワールプリンセス)
	if (spr.disp) {
		scheduler->Wait(7);
	}
	else {
		scheduler->Wait(3);
	}

	// 800～811はコントロールレジスタ
	if ((addr >= 0x800) && (addr < 0x812)) {
		// 一致チェック
		if (*(WORD *)(&sprite[addr]) == data) {
			return;
		}

		// レジスタマスク
		switch (addr & 0xff) {
			// BGスクロールレジスタ
			case 0x00:
			case 0x02:
			case 0x04:
			case 0x06:
				data &= 0x03ff;
				break;

			// BGコントロール(bit10有効)
			case 0x08:
				data &= 0x063f;
				break;

			// 水平トータル
			case 0x0a:
				data &= 0x00ff;
				break;

			// 水平表示位置
			case 0x0c:
				data &= 0x003f;
				break;

			// 垂直表示位置
			case 0x0e:
				data &= 0x00ff;
				break;

			// 解像度設定
			case 0x10:
				data &= 0x001f;
				break;
		}

		// マスクデータ書き込み
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// コントロール
		Control(addr, data);
		NotifyPx68kBGWrite(addr, (WORD)data);
		return;
	}

	// 0812-7FFFはリザーブ(バスエラーの影響を受けない)
	if ((addr >= 0x812) && (addr < 0x8000)) {
		return;
	}

	// 接続チェック
	if (!IsConnect()) {
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// 0400-07FFはリザーブ(バスエラーの影響を受ける)
	if ((addr >= 0x400) && (addr < 0x800)) {
		return;
	}

	// 一致チェック
	if (*(WORD *)(&sprite[addr]) == data) {
		return;
	}

	// スプライトスクロールレジスタ
	if (addr < 0x400) {
		// レジスタマスク
		switch (addr & 0x07) {
			// XPOS,YPOS
			case 0x00:
			case 0x02:
				data &= 0x03ff;
				break;

			// VR|HR|COLOR|SPAT#
			case 0x04:
				data &= 0xcfff;
				break;

			// PRW(bit2有効)
			case 0x06:
				data &= 0x0007;
				break;
		}

		// マスクデータ書き込み
		*(WORD *)(&sprite[addr]) = (WORD)data;

		// スプライトスクロールレジスタは書き換え後
		// 3ラスター後にしか反映しない
		index = (int)(addr >> 3);
		NotifyPx68kBGWrite(addr, (WORD)data);

		if (sphsync[index]==0) {
			sphsync[index]=3;

			// BG HSYNC要求
			bghsync |= 0x10;
		}
		return;
	}

	// 書き込み
	*(WORD *)(&sprite[addr]) = (WORD)data;

	if (addr >= 0x8000) {
		render->PCGMem(addr);
	}

	if (addr >= 0xc000) {
		render->BGMem(addr, (WORD)data);
	}
	if (addr >= 0x8000) {
		NotifyPx68kBGWrite(addr, (WORD)data);
	}
}

//---------------------------------------------------------------------------
//
//	読み込みのみ
//
//---------------------------------------------------------------------------
DWORD FASTCALL Sprite::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// オフセット算出
	addr &= 0xffff;

	// エンディアンを反転させて読み込み
	return sprite[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	コントロール
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::Control(DWORD addr, DWORD data)
{
	ASSERT((addr >= 0x800) && (addr < 0x812));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);

	// アドレスを整理
	addr -= 0x800;
	addr >>= 1;

	switch (addr) {
		// BG0スクロールX
		case 0:
			spr.bg_scrlx[0] = data & 0x3ff;
			bghsync |= 0x01;
			break;

		// BG0スクロールY
		case 1:
			spr.bg_scrly[0] = data & 0x3ff;
			bghsync |= 0x02;
			break;

		// BG1スクロールX
		case 2:
			spr.bg_scrlx[1] = data & 0x3ff;
			bghsync |= 0x04;
			break;

		// BG1スクロールY
		case 3:
			spr.bg_scrly[1] = data & 0x3ff;
			bghsync |= 0x08;
			break;

		// BGコントロール
		case 4:
#if defined(SPRITE_LOG)
			LOG1(Log::Normal, "BGコントロール $%04X", data);
#endif	// SPRITE_LOG
			// bit17 : DISP
			if (data & 0x0200) {
				if (!spr.disp) {
					spr.disp = TRUE;
					render->BGCtrl(5, TRUE);
				}
			}
			else {
				if (spr.disp) {
					spr.disp = FALSE;
					render->BGCtrl(5, FALSE);
				}
			}

			// BG1
			spr.bg_area[1] = (data >> 4) & 0x03;
			if (spr.bg_area[1] & 2) {
				LOG1(Log::Warning, "BG1データエリア未定義 $%02X", spr.bg_area[1]);
			}
			if (spr.bg_area[1] & 1) {
				render->BGCtrl(3, TRUE);
			}
			else {
				render->BGCtrl(3, FALSE);
			}
			if (data & 0x08) {
				spr.bg_on[1] = TRUE;
			}
			else {
				spr.bg_on[1] = FALSE;
			}
			render->BGCtrl(1, spr.bg_on[1]);

			// BG0
			spr.bg_area[0] = (data >> 1) & 0x03;
			if (spr.bg_area[0] & 2) {
				LOG1(Log::Warning, "BG0データエリア未定義 $%02X", spr.bg_area[0]);
			}
			if (spr.bg_area[0] & 1) {
				render->BGCtrl(2, TRUE);
			}
			else {
				render->BGCtrl(2, FALSE);
			}
			if (data & 0x01) {
				spr.bg_on[0] = TRUE;
			}
			else {
				spr.bg_on[0] = FALSE;
			}
			render->BGCtrl(0, spr.bg_on[0]);
			break;

		// 水平トータル
		case 5:
			spr.h_total = data & 0xff;
			break;

		// 水平表示
		case 6:
			spr.h_disp = data & 0x3f;
			break;

		// 垂直表示
		case 7:
			spr.v_disp = data & 0xff;
			break;

		// 画面モード
		case 8:
			spr.h_res = data & 0x03;
			spr.v_res = (data >> 2) & 0x03;

			// 15kHz
			if (data & 0x10) {
				spr.lowres = FALSE;
			}
			else {
				spr.lowres = TRUE;
			}

			// BGサイズ
			if (spr.h_res == 0) {
				// 8x8
				spr.bg_size = FALSE;
			}
			else {
				// 16x16
				spr.bg_size = TRUE;
			}
			render->BGCtrl(4, spr.bg_size);
			if (spr.h_res & 2) {
				LOG1(Log::Warning, "BG/スプライト H-Res未定義 %d", spr.h_res);
			}
			break;

		// その他
		default:
			ASSERT(FALSE);
			break;
	}

	// 表示位置調整のためレンダラへ通知
	if (addr > 4) {
		render->SetCRTC();
	}
}

//---------------------------------------------------------------------------
//
//	レンダラへ通知
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::NotifyPx68kBGWrite(DWORD addr, WORD data)
{
	if (!render) {
		return;
	}

	addr = memdev.first + (addr & 0xffff);
	render->SpriteBGWrite(addr, (BYTE)((data >> 8) & 0xff));
	render->SpriteBGWrite(addr + 1, (BYTE)(data & 0xff));
}

void FASTCALL Sprite::NotifyRender()
{
	int i;
	DWORD addr;
	DWORD data;
	DWORD reg[4];

	// レジスタ
	render->BGCtrl(4, spr.bg_size);
	for (i=0; i<2; i++) {
		// BGデータエリア
		if (spr.bg_area[i] & 1) {
			render->BGCtrl(i + 2, TRUE);
		}
		else {
			render->BGCtrl(i + 2, FALSE);
		}

		// BG表示ON/OFF
		render->BGCtrl(i, spr.bg_on[i]);

		// BGサイズ
		render->BGCtrl(4, spr.bg_size);

		// DISP/CPU
		render->BGCtrl(5, spr.disp);

		// BGスクロール
		render->BGScrl(i, spr.bg_scrlx[i], spr.bg_scrly[i]);
	}

	// メモリ:偶数アドレスのみ
	for (addr=0; addr<0x10000; addr+=2) {
		if (addr < 0x400) {
			if ((addr & 7)==0) {
				reg[0] = *(WORD*)(&sprite[addr  ]);
				reg[1] = *(WORD*)(&sprite[addr+2]);
				reg[2] = *(WORD*)(&sprite[addr+4]);
				reg[3] = *(WORD*)(&sprite[addr+6]);
				render->SpriteReg(addr, reg);
			}
			continue;
		}
		if (addr < 0x8000) {
			continue;
		}
		if (addr >= 0xc000) {
			data = *(WORD*)(&sprite[addr]);
			render->BGMem(addr, (WORD)data);
		}
		render->PCGMem(addr);
	}
}

//---------------------------------------------------------------------------
//
//	内部データ取得
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::GetSprite(sprite_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// 内部ワークをコピー
	*buffer = spr;
}

//---------------------------------------------------------------------------
//
//	メモリエリア取得
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Sprite::GetMem() const
{
	ASSERT(this);
	ASSERT(spr.mem);

	return spr.mem;
}

//---------------------------------------------------------------------------
//
//	PCGエリア取得
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL Sprite::GetPCG() const
{
	ASSERT(this);
	ASSERT(spr.pcg);

	return spr.pcg;
}

//---------------------------------------------------------------------------
//
//	H-Sync通知
//
//---------------------------------------------------------------------------
void FASTCALL Sprite::HSync()
{
	BOOL flag;
	int i;
	DWORD addr;
	DWORD reg[4];

	ASSERT(this);

	// HSyncでBGの更新が必要か
	if (bghsync == 0) {
		return;
	}

	if (bghsync & 0x01) {
		// BG0スクロールX
		render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
		bghsync &= ~0x01;
	}

	if (bghsync & 0x02) {
		// BG0スクロールY
		render->BGScrl(0, spr.bg_scrlx[0], spr.bg_scrly[0]);
		bghsync &= ~0x02;
	}

	if (bghsync & 0x04) {
		// BG1スクロールX
		render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
		bghsync &= ~0x04;
	}

	if (bghsync & 0x08) {
		// BG1スクロールY
		render->BGScrl(1, spr.bg_scrlx[1], spr.bg_scrly[1]);
		bghsync &= ~0x08;
	}

		// スプライト更新対象検索
	if (bghsync & 0x10) {
		// 次回チェックは無し
		flag = FALSE;

		for (i=0; i<128; i++) {
			// 更新対象か
			if (sphsync[i] == 0) {
				continue;
			}

			// カウントダウン
			sphsync[i]--;

			// 2ラスター目
			if (sphsync[i] == 0) {
				addr = i << 3;
				reg[0] = *(WORD*)(&sprite[addr  ]);
				reg[1] = *(WORD*)(&sprite[addr+2]);
				reg[2] = *(WORD*)(&sprite[addr+4]);
				reg[3] = *(WORD*)(&sprite[addr+6]);
				render->SpriteReg(addr, reg);
				continue;
			}

			// 次回チェックはやっぱり必要
			flag = TRUE;
		}

		// 次回必要なければフラグオフ
		if (!flag) {
			bghsync &= ~0x10;
		}
	}
}
