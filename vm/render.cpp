//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ レンダラ ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "tvram.h"
#include "gvram.h"
#include "sprite.h"
#include "rend_soft.h"
#include "render.h"

BOOL FASTCALL IsCMOV(void);

//===========================================================================
//
//	レンダラ
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	定数定義
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// カラー0フラグ(rend_asm.asmで使用)


class Render::Backend
{
public:
	explicit Backend(int m) : mode(m)
	{
	}

	void Activate(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->InvalidateAll();
		}
	}

	void StartFrame(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->StartFrameFast();
			return;
		}
		owner->StartFrameOriginal();
	}

	void EndFrame(Render *owner)
	{
		owner->EndFrameOriginal();
	}

	void HSync(Render *owner, int raster)
	{
		owner->HSyncOriginal(raster);
	}

	void SetCRTC(Render *owner)
	{
		owner->SetCRTCOriginal();
	}

	void SetVC(Render *owner)
	{
		owner->SetVCOriginal();
	}

private:
	int mode;
};
//---------------------------------------------------------------------------
//
//	コンストラクタ
//
//---------------------------------------------------------------------------
Render::Render(VM *p) : Device(p)
{
	// デバイスIDを初期化
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// デバイスポインタ
	crtc = NULL;
	vc = NULL;
	sprite = NULL;
	backend = NULL;
	backend_original = NULL;
	backend_fast = NULL;
	compositor_mode = compositor_original;

	// ワークエリア初期化(CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

	// ワークエリア初期化(パレット)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// ワークエリア初期化(テキスト)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// ワークエリア初期化(グラフィック)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// ワークエリア初期化(PCG,スプライト,BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = NULL;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// ワークエリア初期化(合成)
	render.mixbuf = NULL;
	render.mixwidth = 0;
	render.mixheight = 0;
	render.mixlen = 0;
	render.mixtype = 0;
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixshift, 0, sizeof(render.mixshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixand, 0, sizeof(render.mixand));
	memset(render.mixmap, 0, sizeof(render.mixmap));

	// ワークエリア初期化(描画)
	render.drawflag = NULL;

	// その他
	cmov = FALSE;
}

//---------------------------------------------------------------------------
//
//	初期化
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// 基本クラス
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTC取得
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// VC取得
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// パレットバッファ確保(4MB)
	try {
		render.palbuf = new DWORD[0x10000 * 16];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.palbuf) {
		return FALSE;
	}

	// テキストVRAMバッファ確保(4.7MB)
	try {
		render.textflag = new BOOL[1024 * 32];
		render.textbuf = new BYTE[1024 * 512];
		render.textout = new DWORD[1024 * (1024 + 1)];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.textflag) {
		return FALSE;
	}
	if (!render.textbuf) {
		return FALSE;
	}
	if (!render.textout) {
		return FALSE;
	}
	for (i=0; i<1024*32; i++) {
		render.textflag[i] = TRUE;
	}
	for (i=0; i<1024; i++) {
		render.textmod[i] = TRUE;
	}

	// グラフィックVRAMバッファ確保(8.2MB)
	try {
		render.grpflag = new BOOL[512 * 32 * 4];
		render.grpbuf[0] = new DWORD[512 * 1024 * 4];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.grpflag) {
		return FALSE;
	}
	if (!render.grpbuf[0]) {
		return FALSE;
	}
	render.grpbuf[1] = render.grpbuf[0] + 512 * 1024;
	render.grpbuf[2] = render.grpbuf[1] + 512 * 1024;
	render.grpbuf[3] = render.grpbuf[2] + 512 * 1024;
	memset(render.grpflag, 0, sizeof(BOOL) * 32 * 512 * 4);
	for (i=0; i<512*4; i++) {
		render.grpmod[i] = FALSE;
		render.grppal[i] = TRUE;
	}

	// PCGバッファ確保(4MB)
	try {
		render.pcgbuf = new DWORD[ 16 * 256 * 16 * 16 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.pcgbuf) {
		return FALSE;
	}

	// スプライトポインタ確保(256KB)
	try {
		render.spptr = new DWORD*[ 128 * 512 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.spptr) {
		return FALSE;
	}

	// BGポインタ確保(768KB)
	try {
		render.bgptr[0] = new DWORD*[ (64 * 2) * 1024 ];
		memset(render.bgptr[0], 0, sizeof(DWORD*) * (64 * 2 * 1024));
		render.bgptr[1] = new DWORD*[ (64 * 2) * 1024 ];	// from 512 to 1024 since version2.04
		memset(render.bgptr[1], 0, sizeof(DWORD*) * (64 * 2 * 1024));
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgptr[0]) {
		return FALSE;
	}
	if (!render.bgptr[1]) {
		return FALSE;
	}
	memset(render.bgall, 0, sizeof(render.bgall));
	memset(render.bgmod, 0, sizeof(render.bgmod));

	// BG/スプライトバッファ確保(1MB)
	try {
		render.bgspbuf = new DWORD[ 512 * 512 + 16];	// +16は暫定措置
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgspbuf) {
		return FALSE;
	}

	// 描画フラグバッファ確保(256KB)
	try {
		render.drawflag = new BOOL[64 * 1024];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.drawflag) {
		return FALSE;
	}
	memset(render.drawflag, 0, sizeof(BOOL) * 64 * 1024);

	// パレット作成
	MakePalette();

	// その他ワークエリア
	render.contlevel = 0;
	cmov = ::IsCMOV();

	try {
		backend_original = new Backend(compositor_original);
		backend_fast = new Backend(compositor_fast);
	}
	catch (...) {
		return FALSE;
	}
	if (!backend_original || !backend_fast) {
		return FALSE;
	}
	backend = backend_original;
	compositor_mode = compositor_original;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	クリーンアップ
//
//---------------------------------------------------------------------------
void FASTCALL Render::Cleanup()
{
	int i;

	ASSERT(this);

	if (backend_original) {
		delete backend_original;
		backend_original = NULL;
	}
	if (backend_fast) {
		delete backend_fast;
		backend_fast = NULL;
	}
	backend = NULL;
	compositor_mode = compositor_original;

	// 描画フラグ
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/スプライトバッファ
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BGポインタ
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// スプライトポインタ
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCGバッファ
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// グラフィックVRAMバッファ
	if (render.grpflag) {
		delete[] render.grpflag;
		render.grpflag = NULL;
	}
	if (render.grpbuf[0]) {
		delete[] render.grpbuf[0];
		for (i=0; i<4; i++) {
			render.grpbuf[i] = NULL;
		}
	}

	// テキストVRAMバッファ
	if (render.textflag) {
		delete[] render.textflag;
		render.textflag = NULL;
	}
	if (render.textbuf) {
		delete[] render.textbuf;
		render.textbuf = NULL;
	}
	if (render.textout) {
		delete[] render.textout;
		render.textout = NULL;
	}

	// パレットバッファ
	if (render.palbuf) {
		delete[] render.palbuf;
		render.palbuf = NULL;
	}

	// 基本クラスへ
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	リセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::Reset()
{
	TVRAM *tvram;
	GVRAM *gvram;
	int i;
	int j;
	int k;
	DWORD **ptr;

	ASSERT(this);
	LOG0(Log::Normal, "リセット");

	// ビデオコントローラよりポインタ取得
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// テキストVRAMよりポインタ取得
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// グラフィックVRAMよりポインタ取得
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// スプライトコントローラよりポインタ取得
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// ワークエリア初期化
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = 2;

	// ワークエリア初期化(crtc, vc)
	render.crtc = FALSE;
	render.vc = FALSE;

	// ワークエリア初期化(コントラスト)
	render.contrast = FALSE;

	// ワークエリア初期化(パレット)
	render.palette = FALSE;
	render.palptr = render.palbuf;

	// ワークエリア初期化(テキスト)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// ワークエリア初期化(グラフィック)
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
	}
	render.grptype = 4;

	// ワークエリア初期化(PCG)
	// リセット直後はBG,Spriteともすべて表示しない→PCGは未使用
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// ワークエリア初期化(スプライト)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// ワークエリア初期化(BG)
	memset(render.bgreg, 0, sizeof(render.bgreg));
	render.bgdisp[0] = FALSE;
	render.bgdisp[1] = FALSE;
	render.bgarea[0] = FALSE;
	render.bgarea[1] = TRUE;
	render.bgsize = FALSE;
	render.bgx[0] = 0;
	render.bgx[1] = 0;
	render.bgy[0] = 0;
	render.bgy[1] = 0;

	// ワークエリア初期化(BG/スプライト)
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BGの初期化状態をつくる(すべて0000)
	for (i=0; i<(64*64); i++) {
		render.bgreg[0][i] = 0x10000;
		render.bgreg[1][i] = 0x10000;
	}
	render.pcgready[0] = TRUE;
	render.pcguse[0] = (64 * 64) * 2;
	render.pcgpal[0] = (64 * 64) * 2;
	memset(render.pcgbuf, 0, (16 * 16) * sizeof(DWORD));
	for (i=0; i<64; i++) {
		ptr = &render.bgptr[0][i << 3];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
		ptr = &render.bgptr[0][(512 + (i << 3)) << 7];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
		ptr = &render.bgptr[1][i << 10];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[(k << 7) + 0] = &render.pcgbuf[k << 4];
				ptr[(k << 7) + 1] = (DWORD*)0x10000;
			}
			ptr += 2;
		}
	}

	// ワークエリア初期化(合成)
	render.mixtype = 0;
	if (backend) {
		backend->Activate(this);
	}
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "セーブ");


	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ロード
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "ロード");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	設定適用
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "設定適用");
}

//---------------------------------------------------------------------------
//
//	フレーム開始
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::SetCompositorMode(int mode)
{
	Backend *next = NULL;

	switch (mode) {
	case compositor_original:
		next = backend_original;
		break;
	case compositor_fast:
		next = backend_fast;
		break;
	default:
		return FALSE;
	}

	compositor_mode = mode;
	if (!next) {
		return TRUE;
	}

	backend = next;
	backend->Activate(this);
	return TRUE;
}

void FASTCALL Render::StartFrame()
{
	if (backend) {
		backend->StartFrame(this);
		return;
	}
	StartFrameOriginal();
}

void FASTCALL Render::EndFrame()
{
	if (backend) {
		backend->EndFrame(this);
		return;
	}
	EndFrameOriginal();
}

void FASTCALL Render::HSync(int raster)
{
	if (backend) {
		backend->HSync(this, raster);
		return;
	}
	HSyncOriginal(raster);
}

void FASTCALL Render::SetCRTC()
{
	if (backend) {
		backend->SetCRTC(this);
		return;
	}
	SetCRTCOriginal();
}

void FASTCALL Render::SetVC()
{
	if (backend) {
		backend->SetVC(this);
		return;
	}
	SetVCOriginal();
}

void FASTCALL Render::InvalidateFrame()
{
	render.vc = TRUE;
	render.palette = TRUE;

	memset(render.palmod, 1, sizeof(render.palmod));
	memset(render.mix, 1, sizeof(render.mix));
	memset(render.textmod, 1, sizeof(render.textmod));
	memset(render.textpal, 1, sizeof(render.textpal));
	memset(render.grpmod, 1, sizeof(render.grpmod));
	memset(render.grppal, 1, sizeof(render.grppal));
	memset(render.bgspmod, 1, sizeof(render.bgspmod));
	if (render.drawflag) {
		memset(render.drawflag, 1, sizeof(BOOL) * (64 * 1024));
	}
}

void FASTCALL Render::InvalidateAll()
{
	render.crtc = TRUE;
	InvalidateFrame();
}

void FASTCALL Render::HSyncOriginal(int raster)
{
	render.last = raster;
	if (render.act) {
		Process();
	}
}

void FASTCALL Render::StartFrameFast()
{
	StartFrameOriginal();
	if (render.act) {
		InvalidateFrame();
	}
}
void FASTCALL Render::StartFrameOriginal()
{
	CRTC::crtc_t crtcdata;
	int i;

	ASSERT(this);

	// このフレームはスキップするか
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// このフレームはレンダリングする
	render.act = TRUE;

	// ラスタをクリア
	render.first = 0;
	render.last = -1;

	// CRTCフラグを検査
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC処理");
#endif	// REND_LOG

		// データ取得
		crtc->GetCRTC(&crtcdata);

		// h_dots、v_dotsが0なら保留
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		// 情報をコピー
		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		// 合成バッファの処理長を調整
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		// スプライトリセット(mixlenに依存するため)
		SpriteReset();

		// 全ライン合成
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}

		// オフ
		render.crtc = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	フレーム終了
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrameOriginal()
{
	ASSERT(this);

	// 無効なら何もしない
	if (!render.act) {
		return;
	}

	// ここまでのラスタを処理
	if (render.last > 0) {
		render.last = render.height;
		Process();
	}

	// カウントUp
	render.count++;

	// 無効化
	render.act = FALSE;
}

//---------------------------------------------------------------------------
//
//	合成バッファセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	int i;

	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// 設定
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// 合成バッファの処理長を調整
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// すべての合成を指示
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	CRTCセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTCOriginal()
{
	ASSERT(this);

	// フラグONのみ
	render.crtc = TRUE;
	render.vc = TRUE;
}

//---------------------------------------------------------------------------
//
//	VCセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVCOriginal()
{
	ASSERT(this);

	// フラグONのみ
	render.vc = TRUE;
}

//---------------------------------------------------------------------------
//
//	VC処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Video()
{
	const VC::vc_t *p;
	const CRTC::crtc_t *q;
	int type;
	int i;
	int j;
	int sp;
	int gr;
	int tx;
	int map[4];
	DWORD *ptr[4];
	DWORD shift[4];
	DWORD an[4];

	// VCフラグを降ろす
	render.vc = FALSE;

	// フラグON
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VCデータ、CRTCデータを取得
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// テキストイネーブル
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// グラフィックタイプ
	type = 0;
	if (!p->siz) {
		type = (int)(p->col + 1);
	}
	if (type != render.grptype) {
		render.grptype = type;
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// グラフィックイネーブル、優先度マップ
	render.mixpage = 0;
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		map[i] = -1;
		an[i] = 512 - 1;
	}
	render.mixpage = 0;
	if (!q->gmem) {
		switch (render.grptype) {
			// 1024x1024x1
			case 0:
				render.grpen[0] = p->gon;
				if (render.grpen[0]) {
					map[0] = 0;
					render.mixpage = 1;
					an[0] = 1024 - 1;
				}
				break;
			// 512x512x4
			case 1:
				for (i=0; i<4; i++) {
					if (p->gs[i]) {
						ASSERT((p->gp[i] >= 0) && (p->gp[i] < 4));
						render.grpen[ p->gp[i] ] = TRUE;
						map[i] = p->gp[i];
						render.mixpage++;
					}
				}
				break;
			// 512x512x2
			case 2:
				for (i=0; i<2; i++) {
					// ページ0のチェック
					if ((p->gp[i * 2 + 0] == 0) && (p->gp[i * 2 + 1] == 1)) {
						if (p->gs[i * 2 + 0] && p->gs[i * 2 + 1]) {
							map[i] = 0;
							render.grpen[0] = TRUE;
							render.mixpage++;
						}
					}
					// ページ1のチェック
					if ((p->gp[i * 2 + 0] == 2) && (p->gp[i * 2 + 1] == 3)) {
						if (p->gs[i * 2 + 0] && p->gs[i * 2 + 1]) {
							map[i] = 2;
							render.grpen[2] = TRUE;
							render.mixpage++;
						}
					}
				}
				break;
			// 512x512x1
			case 3:
			case 4:
				render.grpen[0] = TRUE;
				render.mixpage = 1;
				map[0] = 0;
				for (i=0; i<4; i++) {
					if (!p->gs[i]) {
						render.grpen[0] = FALSE;
						render.mixpage = 0;
						map[0] = -1;
						break;
					}
				}
				break;
			default:
				ASSERT(FALSE);
				break;
		}
	}

	// グラフィックバッファをセット
	j = 0;
	for (i=0; i<4; i++) {
		if (map[i] >= 0) {
			ASSERT((map[i] >= 0) && (map[i] <= 3));
			ptr[j] = render.grpbuf[ map[i] ];
			if (render.grptype == 0) {
				shift[j] = 11;
			}
			else {
				shift[j] = 10;
			}
			ASSERT(j <= i);
			map[j] = map[i];
			j++;
		}
	}

	// 優先順位を取得
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// タイプ初期化
	render.mixtype = 0;

	// BG/スプライト表示切替か
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/スプライト表示ON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/スプライト表示OFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// 設定(q->hd >= 2の場合はスプライト面なし)
	if ((q->hd >= 2) || (!p->son)) {
		// スプライトなし
		if (!render.texten) {
			// テキストなし
			if (render.mixpage == 0) {
				// グラフィックなし(type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// グラフィック1面のみ(type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// グラフィック2面のみ(type=2)
				for (i=0; i<2; i++) {
					render.mixptr[i] = ptr[i];
					render.mixshift[i] = shift[i];
					render.mixx[i] = &render.grpx[ map[i] ];
					render.mixy[i] = &render.grpy[ map[i] ];
					render.mixand[i] = an[i];
				}
				render.mixtype = 2;
				return;
			}
			ASSERT((render.mixpage == 3) || (render.mixpage == 4));
			// グラフィック3面以上のみ(type=4)
			for (i=0; i<render.mixpage; i++) {
				render.mixptr[i + 4] = ptr[i];
				render.mixshift[i + 4] = shift[i];
				render.mixx[i + 4] = &render.grpx[ map[i] ];
				render.mixy[i + 4] = &render.grpy[ map[i] ];
				render.mixand[i + 4] = an[i];
			}
			render.mixtype = 4;
			return;
		}
		// テキストあり
		if (render.mixpage == 0) {
			// グラフィックなし。テキストのみ(type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// テキスト+グラフィック1面
			if (tx < gr) {
				// テキスト前面(type=3)
				render.mixptr[0] = render.textout;
				render.mixshift[0] = 10;
				render.mixx[0] = &render.textx;
				render.mixy[0] = &render.texty;
				render.mixand[0] = 1024 - 1;
				render.mixptr[1] = ptr[0];
				render.mixshift[1] = shift[0];
				render.mixx[1] = &render.grpx[ map[0] ];
				render.mixy[1] = &render.grpy[ map[0] ];
				render.mixand[1] = an[0];
				render.mixtype = 3;
				return;
			}
			// グラフィック前面(type=3,tx=grはグラフィック前面、大戦略II)
			render.mixptr[1] = render.textout;
			render.mixshift[1] = 10;
			render.mixx[1] = &render.textx;
			render.mixy[1] = &render.texty;
			render.mixand[1] = 1024 - 1;
			render.mixptr[0] = ptr[0];
			render.mixshift[0] = shift[0];
			render.mixx[0] = &render.grpx[ map[0] ];
			render.mixy[0] = &render.grpy[ map[0] ];
			render.mixand[0] = an[0];
			render.mixtype = 3;
			return;
		}
		// テキスト+グラフィック2面以上(type=5, type=6)
		ASSERT((render.mixpage >= 2) && (render.mixpage <= 6));
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixand[0] = 1024 - 1;
		for (i=0; i<render.mixpage; i++) {
			render.mixptr[i + 4] = ptr[i];
			render.mixshift[i + 4] = shift[i];
			render.mixx[i + 4] = &render.grpx[ map[i] ];
			render.mixy[i + 4] = &render.grpy[ map[i] ];
			render.mixand[i + 4] = an[i];
		}
		if (tx < gr) {
			render.mixtype = 5;
		}
		else {
			render.mixtype = 6;
		}
		return;
	}

	// スプライトあり
	if (!render.texten) {
		// テキストなし
		if (render.mixpage == 0) {
			// グラフィックなし、スプライトのみ(type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// スプライト+グラフィック1面(type=3)
			if (sp < gr) {
				// スプライト前面
				render.mixptr[0] = render.bgspbuf;
				render.mixshift[0] = 9;
				render.mixx[0] = &render.zero;
				render.mixy[0] = &render.zero;
				render.mixand[0] = 512 - 1;
				render.mixptr[1] = ptr[0];
				render.mixshift[1] = shift[0];
				render.mixx[1] = &render.grpx[ map[0] ];
				render.mixy[1] = &render.grpy[ map[0] ];
				render.mixand[1] = an[0];
				render.mixtype = 3;
				return;
			}
			// グラフィック前面(sp=grは不明)
			render.mixptr[1] = render.bgspbuf;
			render.mixshift[1] = 9;
			render.mixx[1] = &render.zero;
			render.mixy[1] = &render.zero;
			render.mixand[1] = 512 - 1;
			render.mixptr[0] = ptr[0];
			render.mixshift[0] = shift[0];
			render.mixx[0] = &render.grpx[ map[0] ];
			render.mixy[0] = &render.grpy[ map[0] ];
			render.mixand[0] = an[0];
			render.mixtype = 3;
			return;
		}
		// スプライト+グラフィック2面以上(type=5, type=6)
		ASSERT((render.mixpage >= 2) && (render.mixpage <= 4));
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixand[0] = 512 - 1;
		for (i=0; i<render.mixpage; i++) {
			render.mixptr[i + 4] = ptr[i];
			render.mixshift[i + 4] = shift[i];
			render.mixx[i + 4] = &render.grpx[ map[i] ];
			render.mixy[i + 4] = &render.grpy[ map[i] ];
			render.mixand[i + 4] = an[i];
		}
		if (sp < gr) {
			render.mixtype = 5;
		}
		else {
			render.mixtype = 6;
		}
		return;
	}

	// テキストあり
	if (render.mixpage == 0) {
		// グラフィックなし。テキスト＋スプライト(type=3)
		if (tx <= sp) {
			// tx=spはテキスト前面(LMZ2)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixptr[1] = render.bgspbuf;
			render.mixshift[1] = 9;
			render.mixx[1] = &render.zero;
			render.mixy[1] = &render.zero;
			render.mixand[1] = 512 - 1;
			render.mixtype = 3;
			return;
		}
		// スプライト前面
		render.mixptr[1] = render.textout;
		render.mixshift[1] = 10;
		render.mixx[1] = &render.textx;
		render.mixy[1] = &render.texty;
		render.mixand[1] = 1024 - 1;
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixand[0] = 512 - 1;
		render.mixtype = 3;
		return;
	}

	// 優先順位決定
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// 適当に決めている
		if (tx < gr) {
			tx = 0;
			sp = 1;
			gr = 2;
		}
		else {
			gr = 0;
			tx = 1;
			sp = 2;
		}
	}
	if (tx == gr) {
		// 適当に決めている
		if (tx < sp) {
			tx = 0;
			gr = 1;
			sp = 2;
		}
		else {
			sp = 0;
			tx = 1;
			gr = 2;
		}
	}
	if (sp == gr) {
		// 適当に決めている
		if (sp < tx) {
			sp = 0;
			gr = 1;
			tx = 2;
		}
		else {
			tx = 0;
			sp = 1;
			gr = 2;
		}
	}
	ASSERT((tx != gr) && (gr != sp) && (tx != sp));
	ASSERT((tx >= 0) && (tx < 3));
	ASSERT((sp >= 0) && (sp < 3));
	ASSERT((gr >= 0) && (gr < 3));
	render.mixmap[tx] = 0;
	render.mixmap[sp] = 1;
	render.mixmap[gr] = 2;

	if (render.mixpage == 1) {
		// テキスト＋スプライト＋グラフィック1面(type=7)
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixand[0] = 1024 - 1;
		render.mixptr[1] = render.bgspbuf;
		render.mixshift[1] = 9;
		render.mixx[1] = &render.zero;
		render.mixy[1] = &render.zero;
		render.mixand[1] = 512 - 1;
		render.mixptr[2] = ptr[0];
		render.mixshift[2] = shift[0];
		render.mixx[2] = &render.grpx[ map[0] ];
		render.mixy[2] = &render.grpy[ map[0] ];
		render.mixand[2] = an[0];
		render.mixtype = 7;
		return;
	}

	// テキスト＋スプライト＋グラフィック２面以上(type=8)
	render.mixptr[0] = render.textout;
	render.mixshift[0] = 10;
	render.mixx[0] = &render.textx;
	render.mixy[0] = &render.texty;
	render.mixand[0] = 1024 - 1;
	render.mixptr[1] = render.bgspbuf;
	render.mixshift[1] = 9;
	render.mixx[1] = &render.zero;
	render.mixy[1] = &render.zero;
	render.mixand[1] = 512 - 1;
	for (i=0; i<render.mixpage; i++) {
		render.mixptr[i + 4] = ptr[i];
		render.mixshift[i + 4] = shift[i];
		render.mixx[i + 4] = &render.grpx[ map[i] ];
		render.mixy[i + 4] = &render.grpy[ map[i] ];
		render.mixand[i + 4] = an[i];
	}
	render.mixtype = 8;
}

//---------------------------------------------------------------------------
//
//	コントラスト設定
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont)
{
	// システムポートの時点で一致チェックを行うので、異なっている場合のみ
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// 変更とフラグON
	render.contlevel = cont;
	render.contrast = TRUE;
}

//---------------------------------------------------------------------------
//
//	コントラスト取得
//
//---------------------------------------------------------------------------
int FASTCALL Render::GetContrast() const
{
	ASSERT(this);
	ASSERT((render.contlevel >= 0) && (render.contlevel <= 15));

	return render.contlevel;
}

//---------------------------------------------------------------------------
//
//	コントラスト処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	int i;

	// ポイント位置を変更、フラグDown
	render.palptr = render.palbuf;
	render.palptr += (render.contlevel << 16);
	render.contrast = FALSE;

	// パレットフラグを全てUp
	for (i=0; i<0x200; i++) {
		render.palmod[i] = TRUE;
	}
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	パレット作成
//
//---------------------------------------------------------------------------
void FASTCALL Render::MakePalette()
{
	DWORD *p;
	int ratio;
	int i;
	int j;

	ASSERT(render.palbuf);

	// 初期化
	p = render.palbuf;

	// コントラストループ
	for (i=0; i<16; i++) {
		// 比率を算出
		ratio = 256 - ((15 - i) << 4);

		// 作成ループ
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	パレット変換
//
//---------------------------------------------------------------------------
DWORD FASTCALL Render::ConvPalette(int color, int ratio)
{
	DWORD r;
	DWORD g;
	DWORD b;

	// assert
	ASSERT((color >= 0) && (color < 0x10000));
	ASSERT((ratio >= 0) && (ratio <= 0x100));

	// 全てコピー
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSBからG:5、R:5、B:5、I:1の順になっている
	// これを R:8 G:8 B:8のDWORDに変換。b31-b24は使わない
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// 輝度ビットは一律Up(元データが0の場合も、!=0にする効果あり)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// コントラストを影響させる
	b *= ratio;
	b >>= 8;
	g *= ratio;
	g >>= 8;
	g &= 0xff00;
	r *= ratio;
	r >>= 8;
	r &= 0xff0000;

	return (DWORD)(r | g | b);
}

//---------------------------------------------------------------------------
//
//	パレット取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetPalette() const
{
	ASSERT(this);
	ASSERT(render.paldata);

	return render.paldata;
}

//---------------------------------------------------------------------------
//
//	パレット処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Palette()
{
	DWORD data;
	BOOL tx;
	BOOL gr;
	BOOL sp;
	int i;
	int j;

	// フラグOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// グラフィック
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// グラフィックに影響、フラグOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// 透明色の処理
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536色のためのパレットデータ設定
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// テキスト兼スプライト
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// テキストに影響、フラグOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// 透明色の処理
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100はBG・スプライトにも必ず影響
				sp = TRUE;
			}

			// PCG検査
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// スプライト
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// スプライトに影響、フラグOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// 透明色の処理
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCG検査
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// グラフィックフラグ
	if (gr) {
		// フラグON
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// テキストフラグ
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// スプライトフラグ
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// パレットフラグOFF
	render.palette = FALSE;
}

//---------------------------------------------------------------------------
//
//	テキストスクロール
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	int i;

	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// 比較チェック
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// ワーク更新
	render.textx = x;
	render.texty = y;

	// フラグON
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "テキストスクロール x=%d y=%d", x, y);
#endif	// REND_LOG

		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	テキストコピー
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// アセンブラサブ
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	テキストバッファ取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetTextBuf() const
{
	ASSERT(this);
	ASSERT(render.textout);

	return render.textout;
}

//---------------------------------------------------------------------------
//
//	テキスト処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Text(int raster)
{
	int y;

	// assert
	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.texttv);
	ASSERT(render.textflag);
	ASSERT(render.textbuf);
	ASSERT(render.palbuf);

	// ディセーブルなら何もしない
	if (!render.texten) {
		return;
	}

	// 実画面Y算出
	y = (raster + render.texty) & 0x3ff;

	// 変更フラグ(逐次型)
	if (render.textmod[y]) {
		// フラグ処理
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		// 水平垂直変換
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// 垂直パレット変換
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// パレット(一括型)
	if (render.textpal[y]) {
		// フラグ処理
		render.textpal[y] = FALSE;

		// 垂直パレット変換
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		render.mix[raster] = TRUE;

		// y == 1023ならコピーする
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

//---------------------------------------------------------------------------
//
//	グラフィックバッファ取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetGrpBuf(int index) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index <= 3));

	ASSERT(render.grpbuf[index]);
	return render.grpbuf[index];
}

//---------------------------------------------------------------------------
//
//	グラフィックスクロール
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT(this);
	ASSERT((block >= 0) && (block <= 3));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// 比較チェック。非表示なら更新なし
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpen[block];
	}

	// フラグ処理
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "グラフィックスクロール block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	グラフィック処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Grp(int block, int raster)
{
	int i;
	int y;
	int offset;

	ASSERT((block >= 0) && (block <= 3));
	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.grpbuf[block]);
	ASSERT(render.grpgv);

	if (render.grptype == 0) {
		// 1024モードはページ0をみる
		if (!render.grpen[0]) {
			return;
		}
	}
	else {
		// それ以外
		if (!render.grpen[block]) {
			return;
		}
	}

	// タイプ別
	switch (render.grptype) {
		// タイプ0:1024×1024 16Color
		case 0:
			// オフセット算出
			offset = (raster + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// 表示対象チェック
			if ((offset < 512) && (block >= 2)) {
				return;
			}
			if ((offset >= 512) && (block < 2)) {
				return;
			}

			// パレットの場合は全領域処理
			if (render.grppal[y + (block << 9)]) {
				render.grppal[y + (block << 9)] = FALSE;
				render.grpmod[y + (block << 9)] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + (block << 14) + i] = FALSE;
				}
				switch (block) {
					// 上半分はブロック0で代・
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 1:
						break;
					// 下半分はブロック2で代・
					case 2:
						if (Rend1024B(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 3:
						break;
				}
				return;
			}

			// それ以外はgrpmodを見て処理
			if (!render.grpmod[y + (block << 9)]) {
				return;
			}
			render.grpmod[y + (block << 9)] = FALSE;
			render.mix[raster] = TRUE;
			switch (block) {
				// ブロック0-左上
				case 0:
					Rend1024C(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5),
								render.paldata);
					break;
				// ブロック1-右上
				case 1:
					Rend1024D(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					break;
				// ブロック2-左下
				case 2:
					Rend1024E(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					break;
				// ブロック3-右下
				case 3:
					Rend1024F(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0xc000,
								render.paldata);
					break;
			}
			return;

		// タイプ1:512×512 16Color
		case 1:
			switch (block) {
				// ページ0
				case 0:
					y = (raster + render.grpy[0]) & 0x1ff;
					// パレット
					if (render.grppal[y]) {
						render.grppal[y] = FALSE;
						render.grpmod[y] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i] = FALSE;
						}
						if (Rend16A(render.grpgv + (y << 10),
										render.grpbuf[0] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// 通常
					if (render.grpmod[y]) {
						render.grpmod[y] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.paldata);
					}
					return;
				// ページ1
				case 1:
					y = (raster + render.grpy[1]) & 0x1ff;
					// パレット
					if (render.grppal[y + 512]) {
						render.grppal[y + 512] = FALSE;
						render.grpmod[y + 512] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0x4000] = FALSE;
						}
						if (Rend16C(render.grpgv + (y << 10),
										render.grpbuf[1] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// 通常
					if (render.grpmod[y + 512]) {
						render.grpmod[y + 512] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[1] + (y << 10),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					}
					return;
				// ページ2
				case 2:
					y = (raster + render.grpy[2]) & 0x1ff;
					// パレット
					if (render.grppal[y + 1024]) {
						render.grppal[y + 1024] = FALSE;
						render.grpmod[y + 1024] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0x8000] = FALSE;
						}
						if (Rend16E(render.grpgv + (y << 10),
										render.grpbuf[2] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// 通常
					if (render.grpmod[y + 1024]) {
						render.grpmod[y + 1024] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[2] + (y << 10),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					}
					return;
				// ページ3
				case 3:
					y = (raster + render.grpy[3]) & 0x1ff;
					// パレット
					if (render.grppal[y + 1536]) {
						render.grppal[y + 1536] = FALSE;
						render.grpmod[y + 1536] = FALSE;
						for (i=0; i<32; i++) {
							render.grpflag[(y << 5) + i + 0xC000] = FALSE;
						}
						if (Rend16G(render.grpgv + (y << 10),
										render.grpbuf[3] + (y << 10),
										render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						return;
					}
					// 通常
					if (render.grpmod[y + 1536]) {
						render.grpmod[y + 1536] = FALSE;
						render.mix[raster] = TRUE;
						Rend16H(render.grpgv + (y << 10),
								render.grpbuf[3] + (y << 10),
								render.grpflag + (y << 5) + 0xC000,
								render.paldata);
					}
					return;
			}
			return;

		// タイプ2:512×512 256Color
		case 2:
			ASSERT((block == 0) || (block == 2));
			if (block == 0) {
				// オフセット算出
				y = (raster + render.grpy[0]) & 0x1ff;

				// パレットの場合は全領域処理
				if (render.grppal[y]) {
					render.grppal[y] = FALSE;
					render.grpmod[y] = FALSE;
					for (i=0; i<32; i++) {
						render.grpflag[(y << 5) + i] = FALSE;
					}
					if (Rend256C(render.grpgv + (y << 10),
									render.grpbuf[0] + (y << 10),
									render.paldata) != 0) {
						render.mix[raster] = TRUE;
					}
					return;
				}

				// それ以外はgrpmodを見て処理
				if (!render.grpmod[y]) {
					return;
				}

				render.grpmod[y] = FALSE;
				render.mix[raster] = TRUE;
				Rend256A(render.grpgv + (y << 10),
							render.grpbuf[0] + (y << 10),
							render.grpflag + (y << 5),
							render.paldata);
			}
			else {
				// オフセット算出
				y = (raster + render.grpy[2]) & 0x1ff;

				// パレットの場合は全領域処理
				if (render.grppal[0x400 + y]) {
					render.grppal[0x400 + y] = FALSE;
					render.grpmod[0x400 + y] = FALSE;
					for (i=0; i<32; i++) {
						render.grpflag[(y << 5) + i + 0x8000] = FALSE;
					}
					if (Rend256D(render.grpgv + (y << 10),
									render.grpbuf[2] + (y << 10),
									render.paldata) != 0) {
						render.mix[raster] = TRUE;
					}
					return;
				}

				// それ以外はgrpmodを見て処理
				if (!render.grpmod[0x400 + y]) {
					return;
				}

				render.grpmod[0x400 + y] = FALSE;
				render.mix[raster] = TRUE;

				// レンダリング
				Rend256B(render.grpgv + (y << 10),
							render.grpbuf[2] + (y << 10),
							render.grpflag + 0x8000 + (y << 5),
							render.paldata);
			}
			return;

		// タイプ3:512x512 未定義
		case 3:
		// タイプ4:512x512 65536Color
		case 4:
			ASSERT(block == 0);
			// オフセット算出
			y = (raster + render.grpy[0]) & 0x1ff;

			// パレットの場合は全領域処理
			if (render.grppal[y]) {
				render.grppal[y] = FALSE;
				render.grpmod[y] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + i] = FALSE;
				}
				if (Rend64KB(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.pal64k,
								render.palptr) != 0) {
					render.mix[raster] = TRUE;
				}
				return;
			}

			// それ以外はgrpmodを見て処理
			if (!render.grpmod[y]) {
				return;
			}
			render.grpmod[y] = FALSE;
			render.mix[raster] = TRUE;
			Rend64KA(render.grpgv + (y << 10),
						render.grpbuf[0] + (y << 10),
						render.grpflag + (y << 5),
						render.pal64k,
						render.palptr);
			return;
	}
}

//===========================================================================
//
//	レンダラ(BG・スプライト部)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	スプライトレジスタリセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	DWORD addr;
	WORD data;

	// スプライトレジスタ設定
	for (addr=0; addr<0x400; addr+=2) {
		data = *(WORD*)(&render.sprmem[addr]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	スプライトレジスタ変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReg(DWORD addr, DWORD data)
{
	BOOL use;
	DWORD reg[4];
	DWORD *next;
	DWORD **ptr;
	int index;
	int i;
	int j;
	int offset;
	DWORD pcgno;

	ASSERT(this);
	ASSERT(addr < 0x400);
	ASSERT((addr & 1) == 0);

	// インデクシングとデータ制限
	index = (int)(addr >> 3);
	switch ((addr & 7) >> 1) {
		// X,Y(0～1023)
		case 0:
		case 1:
			data &= 0x3ff;
			break;
		// V,H,PAL,PCG
		case 2:
			data &= 0xcfff;
			break;
		// PRW(0,1,2,3)
		case 3:
			data &= 0x0003;
			break;
	}

	// ptr設定(&spptr[index << 9])
	ptr = &render.spptr[index << 9];

	// レジスタのバックアップ
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// レジスタへ書き込み
	render.spreg[addr >> 1] = data;

	// 今後有効になるかチェック
	use = TRUE;
	if (next[0] == 0) {
		use = FALSE;
	}
	if (next[0] >= (DWORD)(render.mixlen + 16)) {
		use = FALSE;
	}
	if (next[1] == 0) {
		use = FALSE;
	}
	if (next[1] >= (512 + 16)) {
		use = FALSE;
	}
	if (next[3] == 0) {
		use = FALSE;
	}

	// いままで無効で、これからも無効なら何もしない
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// いままで有効なので、一度とめる
	if (render.spuse[index]) {
		// 無効処理(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[ pcgno ] > 0);
		render.pcguse[ pcgno ]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[ pcgno ] > 0);
		render.pcgpal[ pcgno ]--;

		// 無効処理(ポインタ)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		// 今後無効なら、ここで終了
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// 登録処理(使用フラグ)
	render.spuse[index] = TRUE;

	// 登録処理(PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[ pcgno ]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[ pcgno ]++;

	// PCGアドレスを計算、ポインタセット
	if (next[2] & 0x8000) {
		// V反転
		offset += 0xf0;
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = &render.pcgbuf[offset];
				render.bgspmod[j] = TRUE;
			}
			offset -= 16;
		}
	}
	else {
		// ノーマル
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = &render.pcgbuf[offset];
				render.bgspmod[j] = TRUE;
			}
			offset += 16;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BGスクロール変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// 比較、一致してれば何もしない
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// 更新
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768×512なら無意味
	if (!render.bgspflag) {
		return;
	}

	// 表示中なら、BGSPMODを上げる
	flag = FALSE;
	if (render.bgdisp[0]) {
		flag = TRUE;
	}
	if (render.bgdisp[1] && !render.bgsize) {
		flag = TRUE;
	}
	if (flag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BGコントロール変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGCtrl(int index, BOOL flag)
{
	int i;
	int j;
	BOOL areaflag[2];
	DWORD *reg;
	WORD *area;
	DWORD pcgno;
	DWORD low;
	DWORD mid;
	DWORD high;

	// フラグOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// タイプ別
	switch (index) {
		// BG0 表示フラグ
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 表示フラグ
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 エリア変更
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 エリア変更
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BGサイズ変更
		case 4:
			if (render.bgsize == flag) {
				return;
			}
			render.bgsize = flag;
			areaflag[0] = TRUE;
			areaflag[1] = TRUE;
			break;

		// その他(ありえない)
		default:
			ASSERT(FALSE);
			return;
	}

	// フラグ処理
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// 現状で使っているrender.pcguseをカット
			reg = render.bgreg[i];
			for (j=0; j<(64 * 64); j++) {
				pcgno = reg[j];
				if (pcgno & 0x10000) {
					pcgno &= 0xfff;
					ASSERT(render.pcguse[ pcgno ] > 0);
					render.pcguse[ pcgno ]--;
					pcgno = (pcgno >> 8) & 0x0f;
					ASSERT(render.pcgpal[ pcgno ] > 0);
					render.pcgpal[ pcgno ]--;
				}
			}

			// データアドレスを算出($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64×64ワードコピー。$10000のビットは常に0
			if (render.bgsize) {
				// 16x16はそのまま
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8は工夫が必要。PCG(0-255)を>>2し、消えたbit0,1をbit17,18へ
				for (j=0; j<(64*64); j++) {
					low = (DWORD)area[j];
					mid = low;
					high = low;
					low >>= 2;
					low &= (64 - 1);
					mid &= 0xff00;
					high <<= 17;
					high &= 0x60000;
					render.bgreg[i][j] = (DWORD)(low | mid | high);
				}
			}

			// bgallのセット
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// どの変更でも、768×512以外ならbgspmodを上げる
	if (render.bgspflag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BGメモリ変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGMem(DWORD addr, WORD data)
{
	BOOL flag;
	int i;
	int j;
	int index;
	int raster;
	DWORD pcgno;
	DWORD low;
	DWORD mid;
	DWORD high;

	ASSERT((addr >= 0xc000) && (addr < 0x10000));

	// ページループ
	for (i=0; i<2; i++) {
		// 該当ページのデータエリアと一致しているか
		flag = FALSE;
		if ((render.bgarea[i] == FALSE) && (addr < 0xe000)) {
			flag = TRUE;
		}
		if ((render.bgarea[i] == TRUE) && (addr >= 0xe000)) {
			flag = TRUE;
		}
		if (!flag) {
			continue;
		}

		// インデックス(<64x64)、レジスタポインタ取得
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// 以前のpcguseを消す
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[ pcgno ] > 0);
			render.pcguse[ pcgno ]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[ pcgno ] > 0);
			render.pcgpal[ pcgno ]--;
		}

		// コピー
		if (render.bgsize) {
			// 16x16はそのまま
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8は工夫が必要。PCG(0-255)を>>2し、消えたbit0,1をbit17,18へ
			low = (DWORD)data;
			mid = low;
			high = low;
			low >>= 2;
			low &= (64 - 1);
			mid &= 0xff00;
			high <<= 17;
			high &= 0x60000;
			render.bgreg[i][index] = (DWORD)(low | mid | high);
		}

		// bgallを上げる
		render.bgall[i][index >> 6] = TRUE;

		// 表示中でなければ終了。bgsize=1でページ1の場合も終了
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// スクロール位置から計算し、bgspmodを上げる
		index >>= 6;
		if (render.bgsize) {
			// 16x16
			raster = render.bgy[i] + (index << 4);
			for (j=0; j<16; j++) {
				raster &= (1024 - 1);
				if ((raster >= 0) && (raster < 512)) {
					render.bgspmod[raster] = TRUE;
				}
				raster++;
			}
		}
		else {
			// 8x8
			raster = render.bgy[i] + (index << 3);
			for (j=0; j<16; j++) {
				raster &= (512 - 1);
				render.bgspmod[raster] = TRUE;
				raster++;
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	PCGメモリ変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::PCGMem(DWORD addr)
{
	int index;
	DWORD count;
	int i;

	ASSERT(this);
	ASSERT(addr >= 0x8000);
	ASSERT(addr < 0x10000);
	ASSERT((addr & 1) == 0);

	// インデックスを出す
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgreadyを消す
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguseが>0なら
	count = 0;
	for (i=0; i<16; i++) {
		count += render.pcguse[index + (i << 8)];
	}
	if (count > 0) {
		// 仕方ないので、BG/スプライト再合成を決定
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	PCGバッファ取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetPCGBuf() const
{
	ASSERT(this);
	ASSERT(render.pcgbuf);

	return render.pcgbuf;
}

//---------------------------------------------------------------------------
//
//	BG/スプライトバッファ取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetBGSpBuf() const
{
	ASSERT(this);
	ASSERT(render.bgspbuf);

	return render.bgspbuf;
}

//---------------------------------------------------------------------------
//
//	BG/スプライト
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSprite(int raster)
{
	int i;
	DWORD *reg;
	DWORD **ptr;
	DWORD *buf;
	DWORD pcgno;

	// BGスプライトをMixしないフラグのチェックが必要か。下のASSERT。

	// BG,スプライトとも512までしか考えていない
	if (raster >= 512) return;
//	ASSERT((raster >= 0) && (raster < 512));

	// 横幅も512まで。これも大前提
	if (render.mixlen > 512) return;
//	ASSERT(render.mixlen <= 512);

	// フラグチェック、オフ、合成指示
	if (!render.bgspmod[raster]) {
		return;
	}
	render.bgspmod[raster] = FALSE;
	render.mix[raster] = TRUE;

	// バッファクリア
	// ここでパレット$100で埋める(出たツイLoading)
	buf = &render.bgspbuf[raster << 9];
	RendClrSprite(buf, render.paldata[0x100], render.mixlen);
	if (!sprite->IsDisplay()) {
		// 非表示なら$80000000のビットは落とす(出たツイF3)
		RendClrSprite(buf, render.paldata[0x100] & 0x00ffffff, render.mixlen);
		return;
	}

	// 一番後ろにくる(PRW=1)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += raster;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// 使用中
			if (reg[3] == 1) {
				// PRW=1
				if (*ptr) {
					// 表示
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}
					if (cmov) {
						RendSpriteC(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
					else {
						RendSprite(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 512;
	}

	// BG1を表示
	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, raster, buf);
	}

	// 中間にくる(PRW=2)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += raster;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// 使用中
			if (reg[3] == 2) {
				// PRW=2
				if (*ptr) {
					// 表示
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}
					if (cmov) {
						RendSpriteC(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
					else {
						RendSprite(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 512;
	}

	// BG0を表示
	if (render.bgdisp[0]) {
		BG(0, raster, buf);
	}

	// 手前にくる(PRW=3)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += raster;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// 使用中
			if (reg[3] == 3) {
				// PRW=3
				if (*ptr) {
					// 表示
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}
					if (cmov) {
						RendSpriteC(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
					else {
						RendSprite(*ptr, buf, reg[0], reg[2] & 0x4000);
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 512;
	}
}

//---------------------------------------------------------------------------
//
//	BG
//
//---------------------------------------------------------------------------
void FASTCALL Render::BG(int page, int raster, DWORD *buf)
{
	int x;
	int y;
	DWORD **ptr;
	int len;
	int rest;

	ASSERT((page == 0) || (page == 1));
	ASSERT((raster >= 0) && (raster < 512));
	ASSERT(buf);

	// yブロックを割り出す
	y = render.bgy[page] + raster;
	if (render.bgsize) {
		// 16x16モード
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8モード
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// bgallがTRUEなら、そのyブロックで変更データあり
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// 表示
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// 8x8の表示
		x = render.bgx[page] & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 7);

		// 割り切れるかチェック
		if ((x & 7) == 0) {
			// 8x8、割り切れる
			x >>= 3;
			if (cmov) {
				RendBG8C(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			else {
				RendBG8(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			return;
		}

		// 最初の半端ブロックを実行
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		RendBG8P(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);

		// 余りを調べて8dot単位分を処理
		len = render.mixlen - rest;
		x += rest;
		x &= (512 - 1);
		ASSERT((x & 7) == 0);
		if (cmov) {
			RendBG8C(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG8(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}

		// 最後
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			RendBG8P(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
		return;
	}

	// 16x16の表示
	x = render.bgx[page] & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 7);

	// 割り切れるかチェック
	if ((x & 15) == 0) {
		// 16x16、割り切れる
		x >>= 4;
		if (cmov) {
			RendBG16C(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG16(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		return;
	}

	// 最初の半端ブロックを実行
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	RendBG16P(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);

	// 余りを調べて16dot単位分を処理
	len = render.mixlen - rest;
	x += rest;
	x &= (1024 - 1);
	ASSERT((x & 15) == 0);
	if (cmov) {
		RendBG16C(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}
	else {
		RendBG16(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}

	// 最後
	if (len & 15) {
		x += (len & 0xfff0);
		x &= (1024 - 1);
		x >>= 4;
		RendBG16P(&ptr[x << 1], &buf[rest + (len & 0xfff0)], 0, (len & 15),
			render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
	}
}

//---------------------------------------------------------------------------
//
//	BG(ブロック処理)
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGBlock(int page, int y)
{
	int i;
	int j;
	DWORD *reg;
	DWORD **ptr;
	DWORD *pcgbuf;
	DWORD bgdata;
	DWORD pcgno;

	ASSERT((page == 0) || (page == 1));
	ASSERT((y >= 0) && (y < 64));

	// レジスタポインタを得る
	reg = &render.bgreg[page][y << 6];

	// BGポインタを得る
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 11);
	}
	else {
		ptr += (y << 10);
	}

	// ループ
	for (i=0; i<64; i++) {
		// 取得
		bgdata = reg[i];

		// $10000が立っていればOK
		if (bgdata & 0x10000) {
			ptr += 2;
			continue;
		}

		// $10000をOR
		reg[i] |= 0x10000;

		// pcgnoを得る
		pcgno = bgdata & 0xfff;

		// サイズ別
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x8000) {
				// 上下反転
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// 通常
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 2048;
		}
		else {
			// 8x8。bit17,bit18を考慮する
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// 上下反転
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// 通常
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 1024;
		}

		// 登録処理(PCG)
		render.pcguse[ pcgno ]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[ pcgno ]++;

		// ポインタを進める
		ptr += 2;
	}
}

//===========================================================================
//
//	レンダラ(合成部)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	合成
//
//---------------------------------------------------------------------------

static BOOL FastVisiblePixel(DWORD pixel)
{
	return (BOOL)(((pixel & 0x80000000) == 0) && ((pixel & 0x00FFFFFF) != 0));
}

static DWORD FastBlendPixel(DWORD top, DWORD base)
{
	DWORD a;
	DWORD b;

	a = top & 0x00FEFEFE;
	b = base & 0x00FEFEFE;
	return (DWORD)((a >> 1) + (b >> 1));
}

static DWORD FastDimPixel(DWORD pixel)
{
	return (DWORD)((pixel & 0x00FEFEFE) >> 1);
}

static int FastNormalizePriority(int pri)
{
	if (pri == 3) {
		return 2;
	}
	return pri;
}

static void FastClearLine(DWORD *buf, int len)
{
	int i;
	for (i=0; i<len; i++) {
		buf[i] = REND_COLOR0;
	}
}

static const DWORD *FastGrpLine512(const Render::render_t *work, int page, int y)
{
	int line;
	line = (y + (int)work->grpy[page]) & 0x1ff;
	return &work->grpbuf[page][(line << 10) + (work->grpx[page] & 0x1ff)];
}

static const DWORD *FastGrpLine1024(const Render::render_t *work, int y)
{
	int line;
	line = (y + (int)work->grpy[0]) & 0x3ff;
	return &work->grpbuf[0][(line << 11) + (work->grpx[0] & 0x3ff)];
}

static void FastOverlayLine(DWORD *dst, const DWORD *src, int len, BOOL *active)
{
	int i;
	for (i=0; i<len; i++) {
		if (FastVisiblePixel(src[i])) {
			dst[i] = src[i];
			*active = TRUE;
		}
	}
}

static void FastOverlayLineTR(DWORD *dst, const DWORD *src, const DWORD *sp, int len, BOOL *active)
{
	int i;
	for (i=0; i<len; i++) {
		if (FastVisiblePixel(src[i])) {
			if (FastVisiblePixel(sp[i])) {
				dst[i] = FastBlendPixel(src[i], sp[i]);
			}
			else {
				dst[i] = src[i];
			}
			*active = TRUE;
		}
	}
}

static void FastOverlayLineGate(DWORD *dst, const DWORD *src, const DWORD *sp, int len, BOOL *active)
{
	int i;
	for (i=0; i<len; i++) {
		if (!FastVisiblePixel(sp[i]) && FastVisiblePixel(src[i])) {
			dst[i] = src[i];
			*active = TRUE;
		}
	}
}

static void FastBuildSpecial4(const Render::render_t *work, int page, int y, int len,
	DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	DWORD off;
	DWORD x;
	int i;
	BYTE data;
	int index;

	x = work->grpx[page] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[page]) & 0x1ff) << 10) + (x << 1);
	if (page >= 2) {
		off++;
	}
	for (i=0; i<len; i++) {
		data = work->grpgv[off];
		index = (page & 1) ? ((data >> 4) & 0x0f) : (data & 0x0f);
		if (index & 1) {
			grp_sp[i] = work->paldata[index & 0x0e];
			grp_sp2[i] = REND_COLOR0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = REND_COLOR0;
			grp_sp2[i] = work->paldata[index & 0x0e];
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine8(const Render::render_t *work, int pair, int y, int len,
	DWORD *dst, BOOL opaq, BOOL *active)
{
	DWORD x0;
	DWORD x1;
	DWORD off0;
	DWORD off1;
	DWORD pixel;
	int i;
	int index;

	x0 = work->grpx[pair * 2 + 0] & 0x1ff;
	x1 = work->grpx[pair * 2 + 1] & 0x1ff;
	off0 = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x0 << 1) + pair;
	off1 = ((DWORD)((y + (int)work->grpy[pair * 2 + 1]) & 0x1ff) << 10) + (x1 << 1) + pair;
	for (i=0; i<len; i++) {
		index = (work->grpgv[off0] & 0x0f) | (work->grpgv[off1] & 0xf0);
		if (index != 0) {
			pixel = work->paldata[index];
			if (opaq || FastVisiblePixel(pixel)) {
				dst[i] = pixel;
			}
		}
		else if (opaq) {
			dst[i] = REND_COLOR0;
		}
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
		off0 += 2;
		off1 += 2;
		x0 = (x0 + 1) & 0x1ff;
		x1 = (x1 + 1) & 0x1ff;
		if (x0 == 0) {
			off0 -= 0x400;
		}
		if (x1 == 0) {
			off1 -= 0x400;
		}
	}
}

static void FastBuildSpecial8(const Render::render_t *work, int pair, int y, int len,
	DWORD *grp_sp, DWORD *grp_sp2, BOOL *grp_sp_tr, BOOL *active)
{
	DWORD x0;
	DWORD x1;
	DWORD off0;
	DWORD off1;
	DWORD pixel;
	int i;
	int index;
	int even_index;

	x0 = work->grpx[pair * 2 + 0] & 0x1ff;
	x1 = work->grpx[pair * 2 + 1] & 0x1ff;
	off0 = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x0 << 1) + pair;
	off1 = ((DWORD)((y + (int)work->grpy[pair * 2 + 1]) & 0x1ff) << 10) + (x1 << 1) + pair;
	for (i=0; i<len; i++) {
		index = (work->grpgv[off0] & 0x0f) | (work->grpgv[off1] & 0xf0);
		even_index = (index & 0xfe);
		grp_sp_tr[i] = FALSE;
		if (even_index != 0) {
			pixel = work->paldata[even_index];
		}
		else {
			pixel = REND_COLOR0;
		}
		if (index & 1) {
			grp_sp[i] = pixel;
			grp_sp2[i] = REND_COLOR0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = REND_COLOR0;
			grp_sp2[i] = pixel;
			if ((even_index != 0) && (!FastVisiblePixel(pixel))) {
				grp_sp_tr[i] = TRUE;
				*active = TRUE;
			}
			else if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off0 += 2;
		off1 += 2;
		x0 = (x0 + 1) & 0x1ff;
		x1 = (x1 + 1) & 0x1ff;
		if (x0 == 0) {
			off0 -= 0x400;
		}
		if (x1 == 0) {
			off1 -= 0x400;
		}
	}
}

static void FastBuildLine8TR(const Render::render_t *work, int pair, int y, int len,
	DWORD *dst, const DWORD *grp_sp, BOOL opaq, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	DWORD base;
	int i;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (i=0; i<len; i++) {
		pixel = opaq ? REND_COLOR0 : dst[i];
		index = work->grpgv[off];
		if (FastVisiblePixel(grp_sp[i])) {
			if (index != 0) {
				base = work->paldata[index];
				if (FastVisiblePixel(base)) {
					pixel = FastBlendPixel(base, grp_sp[i]);
				}
			}
		}
		else if (index != 0) {
			base = work->paldata[index];
			if (opaq || FastVisiblePixel(base)) {
				pixel = base;
			}
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
	}
}

static void FastBuildLine8TRGT(const Render::render_t *work, int pair, int y, int len,
	DWORD *dst, const DWORD *grp_sp, BOOL *grp_sp_tr, BOOL opaq, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	int i;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (i=0; i<len; i++) {
		pixel = opaq ? REND_COLOR0 : dst[i];
		index = work->grpgv[off];
		if (!FastVisiblePixel(grp_sp[i]) && !grp_sp_tr[i] && (index != 0)) {
			pixel = work->paldata[index];
		}
		else if (opaq) {
			pixel = REND_COLOR0;
		}
		dst[i] = pixel;
		grp_sp_tr[i] = FALSE;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
	}
}

static void FastBuildSpecial64K(const Render::render_t *work, int y, int len,
	DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	DWORD x;
	DWORD off;
	int i;
	WORD raw;
	DWORD pixel;

	x = work->grpx[0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[0]) & 0x1ff) << 10) + (x << 1);
	for (i=0; i<len; i++) {
		raw = *(WORD *)(&work->grpgv[off]);
		pixel = work->palptr[(DWORD)(raw & 0xfffe)];
		if ((raw & 0xfffe) == 0) {
			pixel |= REND_COLOR0;
		}
		if (raw & 1) {
			grp_sp[i] = pixel;
			grp_sp2[i] = REND_COLOR0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = REND_COLOR0;
			grp_sp2[i] = pixel;
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial1024(const Render::render_t *work, int y, int len,
	DWORD *grp_sp, DWORD *grp_sp2, BOOL *active)
{
	const WORD *src;
	DWORD line;
	DWORD x;
	int bits;
	int remain;
	int i;
	WORD raw;
	int index;

	line = (DWORD)((y + (int)work->grpy[0]) & 0x3ff);
	if ((line & 0x200) == 0) {
		line <<= 10;
		bits = (work->grpx[0] & 0x200) ? 4 : 0;
	}
	else {
		line = (line & 0x1ff) << 10;
		bits = (work->grpx[0] & 0x200) ? 12 : 8;
	}
	x = work->grpx[0] & 0x1ff;
	src = (const WORD *)(&work->grpgv[line + (x << 1)]);
	remain = ((int)(x ^ 0x1ff) + 1);
	for (i=0; i<len; i++) {
		raw = *src++;
		index = (raw >> bits) & 0x0f;
		if (index & 1) {
			grp_sp[i] = work->paldata[index & 0x0e];
			grp_sp2[i] = REND_COLOR0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = REND_COLOR0;
			grp_sp2[i] = work->paldata[index & 0x0e];
			if (FastVisiblePixel(grp_sp2[i])) {
				*active = TRUE;
			}
		}
		if (--remain == 0) {
			src -= 0x200;
			bits ^= 4;
			remain = 512;
		}
	}
}

void FASTCALL Render::FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
	BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron)
{
	const VC::vc_t *p;
	BOOL active;
	BOOL pair0_first;
	BOOL pair_enabled[2];
	BOOL opaq;
	int slot;
	int page;
	int top_pair;
	int other_pair;

	ASSERT(grp);
	ASSERT(grp_sp);
	ASSERT(grp_sp2);
	ASSERT(grp_sp_tr);
	ASSERT(gon);
	ASSERT(tron);
	ASSERT(pron);

	p = vc->GetWorkAddr();
	MixGrp(y, grp);
	FastClearLine(grp_sp, render.mixlen);
	FastClearLine(grp_sp2, render.mixlen);
	memset(grp_sp_tr, 0, (size_t)render.mixlen * sizeof(BOOL));
	*gon = FALSE;
	*tron = FALSE;
	*pron = FALSE;

	for (slot=0; slot<render.mixlen; slot++) {
		if (FastVisiblePixel(grp[slot])) {
			*gon = TRUE;
			break;
		}
	}

	switch (render.grptype) {
		case 0:
			if (render.grpen[0] && p->exon && p->gon) {
				active = FALSE;
				FastBuildSpecial1024(&render, y, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = active;
				*pron = active;
				if (active) {
					*gon = TRUE;
				}
			}
			return;

		case 1:
			if (p->exon && p->gs[0]) {
				page = (int)p->gp[0];
				if ((page >= 0) && (page < 4) && render.grpen[page]) {
					active = FALSE;
					FastBuildSpecial4(&render, page, y, render.mixlen, grp_sp, grp_sp2, &active);
					*tron = active;
					*pron = active;
					if (active) {
						*gon = TRUE;
					}
				}
			}
			return;

		case 2:
			pair0_first = (BOOL)(((int)p->gp[0]) <= ((int)p->gp[2]));
			top_pair = pair0_first ? 0 : 1;
			other_pair = pair0_first ? 1 : 0;
			pair_enabled[0] = render.grpen[0];
			pair_enabled[1] = render.grpen[2];
			FastClearLine(grp, render.mixlen);
			*gon = FALSE;
			opaq = TRUE;
			if (p->exon && pair_enabled[top_pair]) {
				active = FALSE;
				FastBuildSpecial8(&render, top_pair, y, render.mixlen, grp_sp, grp_sp2, grp_sp_tr, &active);
				*tron = active;
				*pron = active;
			}
			if (pair_enabled[other_pair]) {
				active = FALSE;
				if (((p->vr2h & 0x1e) == 0x1e) && *tron) {
					FastBuildLine8TR(&render, other_pair, y, render.mixlen, grp, grp_sp, opaq, &active);
				}
				else if (((p->vr2h & 0x1d) == 0x1d) && *tron) {
					FastBuildLine8TRGT(&render, other_pair, y, render.mixlen, grp, grp_sp, grp_sp_tr, opaq, &active);
				}
				else {
					FastBuildLine8(&render, other_pair, y, render.mixlen, grp, opaq, &active);
				}
				if (active) {
					*gon = TRUE;
				}
				opaq = FALSE;
			}
			if (pair_enabled[top_pair] && ((p->vr2h & 0x14) != 0x14)) {
				active = FALSE;
				FastBuildLine8(&render, top_pair, y, render.mixlen, grp, opaq, &active);
				if (active) {
					*gon = TRUE;
				}
			}
			return;

		case 4:
			if (render.grpen[0] && p->exon && (render.mixpage > 0)) {
				active = FALSE;
				FastBuildSpecial64K(&render, y, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = active;
				*pron = active;
				if (active) {
					*gon = TRUE;
				}
			}
			return;

		default:
			return;
	}
}

void FASTCALL Render::MixFast(int y)
{
	DWORD *dst;
	DWORD grp[1024];
	DWORD grp_sp[1024];
	DWORD grp_sp2[1024];
	BOOL grp_sp_tr[1024];
	DWORD out[1024];
	const VC::vc_t *p;
	DWORD text_pixel;
	DWORD bg_pixel;
	DWORD grp_pixel;
	DWORD final_pixel;
	int tx_pri;
	int sp_pri;
	int gr_pri;
	BOOL gon;
	BOOL tron;
	BOOL pron;
	BOOL text_visible;
	BOOL bg_visible;
	BOOL grp_visible;
	BOOL sp_visible;
	int chosen;
	int i;

	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);

	render.mix[y] = FALSE;
	dst = &render.mixbuf[render.mixwidth * y];
	p = vc->GetWorkAddr();
	tx_pri = FastNormalizePriority((int)p->tx);
	sp_pri = FastNormalizePriority((int)p->sp);
	gr_pri = FastNormalizePriority((int)p->gr);

	FastMixGrp(y, grp, grp_sp, grp_sp2, grp_sp_tr, &gon, &tron, &pron);
	for (i=0; i<render.mixlen; i++) {
		text_pixel = REND_COLOR0;
		bg_pixel = REND_COLOR0;
		if (render.texten) {
			text_pixel = render.textout[(((y + (int)render.texty) & 0x3ff) << 10) + (((int)render.textx + i) & 0x3ff)];
		}
		if (render.bgspflag) {
			bg_pixel = render.bgspbuf[(y << 9) + i];
		}
		grp_pixel = grp[i];
		text_visible = FastVisiblePixel(text_pixel);
		bg_visible = FastVisiblePixel(bg_pixel);
		grp_visible = FastVisiblePixel(grp_pixel);
		sp_visible = FastVisiblePixel(grp_sp[i]);
		final_pixel = 0;
		chosen = -1;

		if (grp_visible) {
			chosen = 0;
			final_pixel = grp_pixel;
		}
		if (bg_visible) {
			if ((chosen < 0) || (sp_pri < gr_pri) || ((chosen == 0) && (sp_pri <= gr_pri))) {
				chosen = 1;
				final_pixel = bg_pixel;
			}
		}
		if (text_visible) {
			if ((chosen < 0) || (tx_pri < ((chosen == 1) ? sp_pri : gr_pri)) ||
				((chosen == 1) && (tx_pri <= sp_pri)) || ((chosen == 0) && (tx_pri <= gr_pri))) {
				chosen = 2;
				final_pixel = text_pixel;
			}
		}

		if (tron && FastVisiblePixel(grp_sp2[i])) {
			if ((chosen < 0) || (gr_pri <= ((chosen == 2) ? tx_pri : ((chosen == 1) ? sp_pri : gr_pri)))) {
				chosen = 3;
				final_pixel = grp_sp2[i];
			}
		}

		if (pron && sp_visible && ((p->vr2h & 0x5c) == 0x14)) {
			final_pixel = grp_sp[i];
		}
		else if (tron && sp_visible && ((p->vr2h & 0x5d) == 0x1d)) {
			if ((chosen == 2) && (tx_pri <= gr_pri)) {
				final_pixel = FastBlendPixel(text_pixel, grp_sp[i]);
			}
			else if ((chosen == 1) && (sp_pri <= gr_pri)) {
				final_pixel = FastBlendPixel(bg_pixel, grp_sp[i]);
			}
		}
		else if (tron && (final_pixel == 0) && sp_visible && ((p->vr2h & 0x5d) == 0x1c)) {
			final_pixel = FastDimPixel(grp_sp[i]);
		}
		out[i] = final_pixel;
	}

	RendMix01(dst, out, render.drawflag + (y << 6), render.mixlen);
}

void FASTCALL Render::Mix(int y)
{
	DWORD *p;
	DWORD *q;
	DWORD *r;
	DWORD *ptr[3];
	int offset;
	DWORD buf[1024];

	if (compositor_mode == compositor_fast) {
		MixFast(y);
		return;
	}

	// 合成指示が無い場合、合成バッファが無い場合、yオーバーの場合return
	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);

#if defined(REND_LOG)
	LOG1(Log::Normal, "合成 y=%d", y);
#endif	// REND_LOG

	// フラグOFF、合成バッファアドレス初期化
	render.mix[y] = FALSE;
	q = &render.mixbuf[render.mixwidth * y];

	switch (render.mixtype) {
		// タイプ0(表示しない)
		case 0:
			RendMix00(q, render.drawflag + (y << 6), render.mixlen);
			return;

		// タイプ1(1面のみ)
		case 1:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			RendMix01(q, p, render.drawflag + (y << 6), render.mixlen);
			return;

		// タイプ2(2面、カラー0重ね合わせ)
		case 2:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			r = render.mixptr[1];
			ASSERT(r);
			r += (offset << render.mixshift[1]);
			r += (*render.mixx[1] & render.mixand[1]);
			if (cmov) {
				RendMix02C(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix02(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// タイプ3(2面、通常重ね合わせ)
		case 3:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			r = render.mixptr[1];
			ASSERT(r);
			r += (offset << render.mixshift[1]);
			r += (*render.mixx[1] & render.mixand[1]);
			if (cmov) {
				RendMix03C(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, p, r, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// タイプ4(グラフィックのみ3面 or 4面)
		case 4:
			MixGrp(y, buf);
			RendMix01(q, buf, render.drawflag + (y << 6), render.mixlen);
			break;

		// タイプ5(グラフィック＋テキスト、テキスト優先通常重ね合わせ)
		case 5:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			if (cmov) {
				RendMix03C(q, p, buf, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, p, buf, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// タイプ6(グラフィック＋テキスト、グラフィック優先通常重ね合わせ)
		case 6:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			if (cmov) {
				RendMix03C(q, buf, p, render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix03(q, buf, p, render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// タイプ7(テキスト＋スプライト＋グラフィック1面)
		case 7:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (offset << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (offset << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] & render.mixand[1]);
			offset = (*render.mixy[2] + y) & render.mixand[2];
			ptr[2] = render.mixptr[2];
			ptr[2] += (offset << render.mixshift[2]);
			ptr[2] += (*render.mixx[2] & render.mixand[2]);
			if (cmov) {
				RendMix04C(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix04(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// タイプ8(テキスト+スプライト+グラフィック２面以上)
		case 8:
			MixGrp(y, buf);
			offset = (*render.mixy[0] + y) & render.mixand[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (offset << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] & render.mixand[0]);
			offset = (*render.mixy[1] + y) & render.mixand[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (offset << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] & render.mixand[1]);
			ptr[2] = buf;
			if (cmov) {
				RendMix04C(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			else {
				RendMix04(q, ptr[render.mixmap[0]], ptr[render.mixmap[1]],                                    	
					ptr[render.mixmap[2]], render.drawflag + (y << 6), render.mixlen);
			}
			return;

		// その他
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	グラフィック合成
//
//---------------------------------------------------------------------------
void FASTCALL Render::MixGrp(int y, DWORD *buf)
{
	DWORD *p;
	DWORD *q;
	DWORD *r;
	DWORD *s;
	int offset;

	ASSERT(buf);
	ASSERT((y >= 0) && (y < render.mixheight));

	switch (render.mixpage) {
		// なし(他のrender.mixpageで吸収)
		case 0:
			ASSERT(FALSE);
			return;

		// 1面(他のrender.mixpageで吸収)
		case 1:
			ASSERT(FALSE);
			return;

		// 2面
		case 2:
			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);
			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			if (cmov) {
				RendGrp02C(buf, p, q, render.mixlen);
			}
			else {
				RendGrp02(buf, p, q, render.mixlen);
			}
			break;

		// 3面
		case 3:
			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);

			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			offset = (*render.mixy[6] + y) & render.mixand[6];
			r = render.mixptr[6];
			ASSERT(r);
			r += (offset << render.mixshift[6]);
			r += (*render.mixx[6] & render.mixand[6]);

			if (cmov) {
				RendGrp03C(buf, p, q, r, render.mixlen);
			}
			else {
				RendGrp03(buf, p, q, r, render.mixlen);
			}
			break;

		// 4面
		case 4:
			offset = (*render.mixy[4] + y) & render.mixand[4];
			p = render.mixptr[4];
			ASSERT(p);
			p += (offset << render.mixshift[4]);
			p += (*render.mixx[4] & render.mixand[4]);

			offset = (*render.mixy[5] + y) & render.mixand[5];
			q = render.mixptr[5];
			ASSERT(q);
			q += (offset << render.mixshift[5]);
			q += (*render.mixx[5] & render.mixand[5]);

			offset = (*render.mixy[6] + y) & render.mixand[6];
			r = render.mixptr[6];
			ASSERT(r);
			r += (offset << render.mixshift[6]);
			r += (*render.mixx[6] & render.mixand[6]);

			offset = (*render.mixy[7] + y) & render.mixand[7];
			s = render.mixptr[7];
			ASSERT(s);
			s += (offset << render.mixshift[7]);
			s += (*render.mixx[7] & render.mixand[7]);

			RendGrp04(buf, p, q, r, s, render.mixlen);
			return;

		// その他
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	合成バッファ取得
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULLの場合もあり
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	レンダリング
//
//---------------------------------------------------------------------------
void FASTCALL Render::Process()
{
	int i;

	// 行きすぎている場合は不要
	if (render.first >= render.last) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ビデオ処理");
#endif	// RENDER_LOG
		Video();
	}

	// コントラスト
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "コントラスト処理");
#endif	// RENDER_LOG
		Contrast();
	}

	// パレット
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "パレット処理");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0は、スプライトの表示ON/OFFを検査
	if (render.first == 0) {
		if (sprite->IsDisplay()) {
			if (!render.bgspdisp) {
				// スプライトCPU→Video
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = TRUE;
			}
		}
		else {
			if (render.bgspdisp) {
				// スプライトVideo→CPU
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = FALSE;
			}
		}
	}

	// 垂直x2の場合
	if ((render.v_mul == 2) && !render.lowres) {
		// I/O側で拡大するため、縦方向は半分しか作らない
		for (i=render.first; i<render.last; i++) {
			if ((i & 1) == 0) {
				Text(i >> 1);
				Grp(0, i >> 1);
				Grp(1, i >> 1);
				Grp(2, i >> 1);
				Grp(3, i >> 1);
				BGSprite(i >> 1);
				Mix(i >> 1);
			}
		}
		// 更新
		render.first = render.last;
		return;
	}

	// インタレースの場合
	if ((render.v_mul == 0) && render.lowres) {
		// 偶数・奇数を同時に作る(実機とは異なる)
		for (i=render.first; i<render.last; i++) {
			// テキスト・グラフィック
			Text((i << 1) + 0);
			Text((i << 1) + 1);
			Grp(0, (i << 1) + 0);
			Grp(0, (i << 1) + 1);
			Grp(1, (i << 1) + 0);
			Grp(1, (i << 1) + 1);
			Grp(2, (i << 1) + 0);
			Grp(2, (i << 1) + 1);
			Grp(3, (i << 1) + 0);
			Grp(3, (i << 1) + 1);
			BGSprite((i << 1) + 0);
			BGSprite((i << 1) + 1);
			Mix((i << 1) + 0);
			Mix((i << 1) + 1);
		}
		// 更新
		render.first = render.last;
		return;
	}

	// 通常ループ
	for (i=render.first; i<render.last; i++) {
		Text(i);
		Grp(0, i);
		Grp(1, i);
		Grp(2, i);
		Grp(3, i);
		BGSprite(i);
		Mix(i);
	}
	// 更新
	render.first = render.last;
}









