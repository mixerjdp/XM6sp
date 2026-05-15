//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
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
#include "config.h"
#include "render.h"
#include "rend_asm.h"
#include "px68k_render_adapter.h"
#if XM6_RENDER_SYNC == 2
#include "mfc_com.h"
#include "mfc_sch.h"
#endif	// XM6_RENDER_SYNC == 2

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
#define REND_COLORS		0x40000000		// 特殊フラグ(半透明／特殊プライオリティ)
#define REND_COLORT		0x40000000		// 特殊フラグ(テキストパレット０)

//---------------------------------------------------------------------------
//
/// コンストラクタ
//
//---------------------------------------------------------------------------
Render::Render(VM* p) : Device(p)
{
	// デバイスIDを初期化
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// デバイスポインタ
	crtc = NULL;
	vc = NULL;
	sprite = NULL;
	px68k_adapter = NULL;
	memset(&px68k_crtc_host, 0, sizeof(px68k_crtc_host));
	memset(&px68k_crtc_state_cache, 0, sizeof(px68k_crtc_state_cache));
	render_fast_dummy_enabled = FALSE;
	transparency_enabled = TRUE;
	original_bg0_render_enabled = TRUE;
	compositor_mode = compositor_original;
#if XM6_RENDER_SYNC == 2
	m_pScheduler = NULL;
#endif	// XM6_RENDER_SYNC == 2

	// ワークエリア初期化(CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.height = 512;

	render.hd = 2;
	render.vd = 1;
	render.hrl = FALSE;
	render.lowres = FALSE;
	render.hres = 2;
	render.siz = 0;
	render.h_disp = 768;
	render.v_disp = 512;
	render.h_total = 1096;
	render.h_pulse = 120;
	render.h_start = 112;
	render.v_total = 567;
	render.v_pulse = 5;
	render.v_start = 40;
	render.scanline = FALSE;

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
	render.zero = 0;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// ワークエリア初期化(合成)
	render.mixbuf = NULL;
	render.mixwidth = 0;
	render.mixheight = 0;
	render.mixlen = 0;
	render.mixtype = 0;
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixrshift, 0, sizeof(render.mixrshift));
	memset(render.mixlshift, 0, sizeof(render.mixlshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixandx, 0, sizeof(render.mixandx));
	memset(render.mixandy, 0, sizeof(render.mixandy));
	memset(render.mixmap, 0, sizeof(render.mixmap));

	// ワークエリア初期化(描画)
	memset(render.draw, 0, sizeof(render.draw));
	render.drawflag = NULL;
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

	// CRTCワークアドレス取得
	cp = crtc->GetWorkAddr();

	// VC取得
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// VCワークアドレス取得
	vp = vc->GetWorkAddr();

	px68k_adapter = new Px68kRenderAdapter();
	if (!px68k_adapter || !px68k_adapter->Init()) {
		return FALSE;
	}
	px68k_crtc_host.ctx = this;

#if LOCAL_EXCEPTION
	try {
#endif	// LOCAL_EXCEPTION

	// パレットバッファ確保(4MB)
	render.palbuf = new DWORD[0x10000 * 16];
	if (!render.palbuf) {
		return FALSE;
	}

	// テキストVRAMバッファ確保(4.7MB)
	render.textflag = new BOOL[1024 * 32];
	render.textbuf = new BYTE[1024 * 512];
	render.textout = new DWORD[1024 * (1024 + 1)];
	if (!render.textflag) {
		return FALSE;
	}
	if (!render.textbuf) {
		return FALSE;
	}
	if (!render.textout) {
		return FALSE;
	}
	for (i=0; i<1024 * 32; i++) {
		render.textflag[i] = TRUE;
	}
	for (i=0; i<1024; i++) {
		render.textmod[i] = TRUE;
	}

	// グラフィックVRAMバッファ確保(8.2MB)
	render.grpflag = new BOOL[512 * 32 * 4];
	render.grpbuf[0] = new DWORD[512 * 1024 * 4];
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
	for (i=0; i<512 * 4; i++) {
		render.grpmod[i] = FALSE;
		render.grppal[i] = TRUE;
	}

	// PCGバッファ確保(4MB)
	render.pcgbuf = new DWORD[16 * 256 * 16 * 16];
	if (!render.pcgbuf) {
		return FALSE;
	}

	// スプライトポインタ確保(512KB)
	render.spptr = new DWORD*[128 * 1024];
	if (!render.spptr) {
		return FALSE;
	}

	// BGポインタ確保(768KB)
	render.bgptr[0] = new bgdata_t[64 * 1024];
	memset(render.bgptr[0], 0, sizeof(bgdata_t) * (64 * 1024));
	render.bgptr[1] = new bgdata_t[64 * 1024];	// from 512 to 1024 since version2.04
	memset(render.bgptr[1], 0, sizeof(bgdata_t) * (64 * 1024));
	if (!render.bgptr[0]) {
		return FALSE;
	}
	if (!render.bgptr[1]) {
		return FALSE;
	}
	memset(render.bgall, 0, sizeof(render.bgall));
	memset(render.bgmod, 0, sizeof(render.bgmod));

	// BG/スプライトバッファ確保(4MB)
	render.bgspbuf = new DWORD[1024 * 1024];
	if (!render.bgspbuf) {
		return FALSE;
	}

	// 描画フラグバッファ確保(256KB)
	render.drawflag = new BOOL[64 * 1024];
	if (!render.drawflag) {
		return FALSE;
	}
	memset(render.draw, 0, sizeof(render.draw));
	memset(render.drawflag, 0, sizeof(BOOL) * 64 * 1024);

#if LOCAL_EXCEPTION
	}
	catch (...) {
		return FALSE;
	}
#endif	// LOCAL_EXCEPTION

	// パレット作成
	MakePalette();

	// その他ワークエリア
	render.contlevel = 0;
	render.contvalue = 0;

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
	bgdata_t *ptr;

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

	// ワークエリア初期化(crtc, vc) --再計算を促すためTRUE
	render.crtc = TRUE;
	render.vc = TRUE;

	// ワークエリア初期化(コントラスト)--再計算を促すためTRUE
	render.contrast = TRUE;

	// ワークエリア初期化(パレット)--再計算を促すためTRUE
	render.palette = TRUE;
	render.palptr = render.palbuf;

	// ワークエリア初期化(テキスト)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// ワークエリア初期化(グラフィック)
	for (i=0; i<4; i++) {
		render.grppen[i] = FALSE;
		render.grpen[i] = FALSE;
		render.grpben[i] = FALSE;
		render.grpnorm[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
		render.grpdx[i] = 0;
		render.grpdy[i] = 0;
	}
	render.grptype = 4;
	render.grpscrl = FALSE;

	// ワークエリア初期化(PCG)
	// リセット直後はBG,Spriteともすべて表示しない→PCGは未使用
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// ワークエリア初期化(スプライト)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 1024);
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
	render.bgsp = TRUE;
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
	memset(render.pcgbuf, 0, sizeof(DWORD) * (16 * 256 * 16 * 16));
	for (i=0; i<64; i++) {
		ptr = &render.bgptr[0][(i << 3) << 6];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[k << 6].pcg = &render.pcgbuf[k << 4];
				ptr[k << 6].bg = (DWORD)0x10000;
			}
			ptr += 1;
		}
		ptr = &render.bgptr[0][(512 + (i << 3)) << 6];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[k << 6].pcg = &render.pcgbuf[k << 4];
				ptr[k << 6].bg = (DWORD)0x10000;
			}
			ptr += 1;
		}
		ptr = &render.bgptr[1][(i << 3) << 6];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[k << 6].pcg = &render.pcgbuf[k << 4];
				ptr[k << 6].bg = (DWORD)0x10000;
			}
			ptr += 1;
		}
		ptr = &render.bgptr[1][(512 + (i << 3)) << 6];
		for (j=0; j<64; j++) {
			for (k=0; k<8; k++) {
				ptr[k << 6].pcg = &render.pcgbuf[k << 4];
				ptr[k << 6].bg = (DWORD)0x10000;
			}
			ptr += 1;
		}
	}

	// ワークエリア初期化(合成)
	render.mixtype = 0;
}

//---------------------------------------------------------------------------
//
//	セーブ
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio* /*fio*/, int /*ver*/)
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
BOOL FASTCALL Render::Load(Fileio* /*fio*/, int /*ver*/)
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
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "設定適用");

	// 15kHzスキャンライン表示
	render.scanline = config->disp_scanline;
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	フレーム開始
//
//---------------------------------------------------------------------------
void FASTCALL Render::StartFrame()
{
	ASSERT(this);

	if (render_fast_dummy_enabled && px68k_adapter) {
		render.act = TRUE;
		px68k_adapter->StartFrame(this);
		return;
	}

	// このフレームはスキップするか
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// このフレームはレンダリングする
	render.act = TRUE;

	// ラスタをクリア
	render.first = 0;
	render.last = 0;

	// CRTC
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC処理");
#endif	// RENDER_LOG
		Crtc();
	}

	// コントラスト
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "コントラスト処理");
#endif	// RENDER_LOG
		Contrast();
	}

	// インタレースの合成偶奇を更新
	render.mixeven = cp->v_scaneven;
}

//---------------------------------------------------------------------------
//
//	フレーム終了
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrame()
{
	int i;
	ASSERT(this);

	if (render_fast_dummy_enabled && px68k_adapter) {
		px68k_adapter->EndFrame(this);
		render.count++;
		render.act = FALSE;
		return;
	}

	// 無効なら何もしない
	if (!render.act) {
		return;
	}

	// ここまでのラスタを処理
	if (render.last > 0) {
		for (i=render.last; i<render.height; i++) {
			Process(i, 0);
		}
	}

	// カウントUp
	render.count++;

	// 無効化
	render.act = FALSE;

#if XM6_RENDER_SYNC == 2
	// 描画
	ASSERT(m_pScheduler);
	m_pScheduler->UpdateFrame();
#endif	// XM6_RENDER_SYNC == 2
}

//---------------------------------------------------------------------------
//
//	合成バッファセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
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
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	CRTCセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTC()
{
	ASSERT(this);

	// フラグONのみ
	render.crtc = TRUE;
}

//---------------------------------------------------------------------------
//
//	VCセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVC()
{
	ASSERT(this);

	// フラグONのみ
	render.vc = TRUE;
}

//---------------------------------------------------------------------------
//
//	CRTC処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Crtc()
{
	Sprite::sprite_t sprdata;
	WORD *p;
	DWORD h_hz;
	DWORD v_hz;
	int mixlen;
	int bgsp_h;
	int bgsp_v;
	int index;

	// データ取得
	sprite->GetSprite(&sprdata);

	// h_dots、v_dotsが0なら保留
	if ((cp->h_dots == 0) || (cp->v_dots == 0)) {
		return;
	}

	// CRTC情報をコピー
	p = (WORD *)cp->reg;
	render.siz = cp->siz;
	render.hd = cp->hd;
	render.vd = cp->vd;
	render.hrl = cp->hrl;
	render.lowres = cp->lowres;
	render.width = cp->h_dots;
	render.h_mul = cp->h_mul;
	render.height = cp->v_dots;
	render.v_mul = cp->v_mul;

	// 水平周波数モード決定
	crtc->GetHVHz(&h_hz, &v_hz);
	render.hres = -1;
	if (h_hz > 1400 && h_hz < 1800) {
		render.hres = 0;
	}

	if (h_hz > 2300 && h_hz < 2700) {
		render.hres = 1;
	}

	if (h_hz > 2950 && h_hz < 3350) {
		render.hres = 2;
	}

	// CRTエミュレーション用の情報設定
	render.h_disp = cp->h_dots;
	render.h_total = p[0] & 0xff;
	render.h_total += 1;
	render.h_total *= 8;
	render.h_pulse = p[1] & 0xff;
	render.h_pulse += 1;
	render.h_pulse *= 8;
	render.h_start = p[2] & 0xff;
	render.h_start += 5;
	render.h_start *= 8;
	render.v_disp = cp->v_dots;
	render.v_total = p[4] & 0x3ff;
	render.v_pulse = p[5] & 0x3ff;
	render.v_start = p[6] & 0x3ff;

	// インタレースまたは水平周波数モードが
	// 15kHzの場合は縦２倍で合成するので補正する
	if (cp->v_mul == 0 || render.hres == 0) {
		render.v_disp <<= 1;
		render.v_total <<= 1;
		render.v_pulse <<= 1;
		render.v_start <<= 1;
	}

	// 合成バッファの処理長を調整
	mixlen = render.mixlen;
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// mixlenが無効なら合成させない
	if (render.mixlen==0) {
		render.act = FALSE;
	}

	// BG/スプライトの位置調整
	bgsp_h = render.bgsp_h;
	render.bgsp_h = p[2] & 0xff;
	render.bgsp_h += 4;
	render.bgsp_h -= sprdata.h_disp;
	render.bgsp_h *= 8;
	bgsp_v = render.bgsp_v;
	render.bgsp_v = p[6] & 0x3ff;
	render.bgsp_v -= sprdata.v_disp;
	render.bgsp_lowres = sprdata.lowres;
	render.bgsp_vres = sprdata.v_res;

	// 合成モード
	if (render.lowres) {
		// 15kHz
		if (render.vd == 1) {
			// インタレース
			render.mixmode = 1;
		} else {
			// 通常
			render.mixmode = 0;
		}
	} else {
		// 31kHz
		if (render.vd == 0) {
			// ２度読み
			render.mixmode = 2;
		} else if (render.vd == 1) {
			// 通常
			render.mixmode = 0;
		} else {
			// インタレース
			render.mixmode = 1;
		}
	}

	// 合成モード(BG,スプライト)
	if (render.bgsp_lowres) {
		// 15kHz
		if (render.bgsp_vres == 0) {
			// 通常
			render.bgsp_mixmode = 0;
		} else {
			// インタレース
			render.bgsp_mixmode = 1;
		}
	} else {
		// 31kHz
		if (render.bgsp_vres == 0) {
			// ２度読みモード
			render.bgsp_mixmode = 2;
		} else {
			// 通常
			render.bgsp_mixmode = 0;
		}
	}

	// ラスターずれ検証処理
	if (sprdata.h_res != 0) {
		// インデック作成
		index = render.hd;
		if (!render.lowres) {
			index += 4;
		}
		if (render.hrl) {
			index += 8;
		}

		// 検証テーブル比較
		if (sprdata.h_disp > HDispTable[index]) {
			render.bgsp_v++;
		}
	}

	// スプライトリセット(mixlen,bgsp_h,bgsp_vに依存するため)
	if (mixlen != render.mixlen ||
		bgsp_h != render.bgsp_h || bgsp_v != render.bgsp_v) {
		SpriteReset();
	}

	// オフ
	render.crtc = FALSE;

	// VCに通知
	SetVC();
}

//---------------------------------------------------------------------------
//
//	ラスターずれ検証テーブル
//	lowres,hd,HRLから得られる値。
//
//	BGSPのH-DISPがこの値以上になるとBGSP面が１ラスター上にずれる現象がでる
//
//---------------------------------------------------------------------------
const DWORD Render::HDispTable[16] = {
	// HRL=0
	64, 15, 64, 64,
	17, 31, 64, 64,
	// HRL=1
	64, 15, 64, 64,
	 5,  7, 64, 64,
};

//---------------------------------------------------------------------------
//
//	VC処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Video()
{
	int type;
	int i;
	int j;
	int map[4];
	DWORD *ptr[4];
	DWORD shift[4];
	DWORD an[4];
	DWORD mixlshift;
	DWORD mixrshift;

	// VCフラグを降ろす
	render.vc = FALSE;

	// フラグON
	render.mixdirty = TRUE;
	render.textdirty = TRUE;
	render.grpdirty = TRUE;
	render.bgspdirty = TRUE;

	// テキストイネーブル
	if (vp->ton && !cp->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// グラフィックタイプ
	type = 0;
	if (!vp->siz) {
		type = (int)(vp->col + 1);
	}
	if (type != render.grptype) {
		render.grptype = type;
	}


	// グラフィック合成タイプ
	render.mixpage = 0;
	for (i=0; i<4; i++) {
		render.grppen[i] = FALSE;
		render.grpen[i] = FALSE;
		render.grpben[i] = FALSE;
		map[i] = -1;
		an[i] = 512 - 1;
	}

	if (!cp->gmem) {
		switch (render.grptype) {
			// 1024x1024x1
			case 0:
				if (vp->gon) {
					render.grppen[0] = TRUE;
					render.grpben[vp->gp[0]] = TRUE;
					render.grpben[vp->gp[1]] = TRUE;
					render.grpben[vp->gp[2]] = TRUE;
					render.grpben[vp->gp[3]] = TRUE;
					map[0] = 0;
					an[0] = 1024 - 1;
					render.mixpage = 1;
				}
				break;
			// 512x512x4
			case 1:
				if (vp->gs[0] || vp->gs[1] || vp->gs[2] || vp->gs[3]) {
					for (i=0; i<4; i++) {
						ASSERT((vp->gp[i] >= 0) && (vp->gp[i] < 4));
						render.grppen[i] = vp->gs[i];
						render.grpben[vp->gp[i]] = TRUE;
						map[i] = i;
						render.mixpage++;
					}
				}
				break;
			// 512x512x2
			case 2:
				if (vp->gs[0] || vp->gs[1] || vp->gs[2] || vp->gs[3]) {
					for (i=0; i<2; i++) {
						ASSERT((vp->gp[i * 2 + 0] >= 0) && (vp->gp[i * 2 + 1] < 4));
						render.grppen[i] = vp->gs[i * 2 + 0] || vp->gs[i * 2 + 1];
						render.grpben[vp->gp[i * 2 + 0]] = TRUE;
						render.grpben[vp->gp[i * 2 + 1]] = TRUE;
						map[i] = i * 2;
						render.mixpage++;
					}
				}
				break;
			// 512x512x1
			case 3:
			case 4:
				if (vp->gs[0] || vp->gs[1] || vp->gs[2] || vp->gs[3]) {
					render.grppen[0] = TRUE;
					render.grpben[vp->gp[0]] = TRUE;
					render.grpben[vp->gp[1]] = TRUE;
					render.grpben[vp->gp[2]] = TRUE;
					render.grpben[vp->gp[3]] = TRUE;
					map[0] = 0;
					render.mixpage = 1;
				}
				break;
			default:
				ASSERT(FALSE);
				break;
		}
	}


	// 全体合成位置調整用シフト量初期化
	mixlshift = 0;
	mixrshift = 0;

	// ２度読みは位置を半分になるようにする
	if (render.mixmode == 2) {
		mixrshift = 1;
	}

	// BG/スプライト合成用シフト量初期化
	render.bgsp_rshift = 0;
	render.bgsp_lshift = 0;

	// BGSPが通常でインタレースの時だけ半分の位置
	if (render.bgsp_mixmode == 0 && render.mixmode == 1) {
		render.bgsp_rshift = 1;
	}

	// BGSPがインタレースの場合はインタレースで無い時だけ倍の位置
	if (render.bgsp_mixmode == 1 && render.mixmode != 1) {
		render.bgsp_lshift = 1;
	}

	// BGSPが２度読みの場合はインタレース状態で半分か4分の1か決まる
	if (render.bgsp_mixmode == 2) {
		if (render.mixmode != 1) {
			render.bgsp_rshift = 1;
		} else {
			render.bgsp_rshift = 2;
		}
	}

	// グラフィックバッファをセット
	j = 0;
	for (i=0; i<4; i++) {
		if (map[i] >= 0) {
			ASSERT((map[i] >= 0) && (map[i] <= 3));
			ptr[j] = render.grpbuf[map[i]];
			if (render.grptype == 0) {
				shift[j] = 11;
			}
			else {
				shift[j] = 10;
			}
			ASSERT(j <= i);
			if (render.grptype==0) {
				map[j] = map[i];
			} else {
				map[j] = vp->gp[map[i]];
			}
			j++;
		}
	}

	// グラフィックの設定
	for (i=0; i<render.mixpage; i++) {
		render.mixptr[i + 4] = ptr[i];
		render.mixshift[i + 4] = shift[i];
		render.mixrshift[i + 4] = mixrshift;
		render.mixlshift[i + 4] = mixlshift;
		render.mixx[i + 4] = &render.grpx[map[i]];
		render.mixy[i + 4] = &render.grpy[map[i]];
		render.mixandx[i + 4] = an[i];
		render.mixandy[i + 4] = an[i];
		render.mixraster[i + 4] = 0;
	}

	// グラフィックスクロールと表示構成チェック
	if (render.grpdirty) {
		GrpScrlCheck();
		GrpDispCheck();
	}

	// 優先順位を取得
	render.tx = vp->tx;
	render.sp = vp->sp;
	render.gr = vp->gr;

	// タイプ初期化
	render.mixtype = 0;


	// 特殊処理
	//
	// VCのR1のグラフィック画面優先度が未定義の%11に
	// 設定されるとテキストもBGSPも消えてしまうようだ。
	//
	if (render.gr == 3) {
		if (render.mixpage == 0) {
			// グラフィックなし(type=0)
			render.mixtype = 0;
			return;
		}
		// グラフィック1面以上のみ(type=3)
		render.mixtype = 3;
		return;
	}


	// BG/スプライト表示切替か
	if ((cp->hd >= 2) || (!vp->son)) {
		if (render.bgspflag) {
			// BG/スプライト表示ON->OFF
			render.bgspflag = FALSE;
		}
	} else {
		if (!render.bgspflag) {
			// BG/スプライト表示OFF->ON
			render.bgspflag = TRUE;
		}
	}

	// パレット０半透明は特別処理
	if (vp->ah) {
		// パレット０半透明(type=11)
		if (render.mixpage > 0) {
			render.mixtype = 11;
		} else {
			render.mixtype = 0;
		}
		return;
	}

	// グラフィックのみ
	if (!render.texten && ((cp->hd >= 2) || (!vp->son))) {
		if (render.mixpage == 0) {
			// グラフィックなし(type=0)
			render.mixtype = 0;
			return;
		}
		// グラフィック1面以上のみ(type=3)
		render.mixtype = 3;
		return;
	}

	// テキストのみ
	if (render.mixpage == 0 && render.texten && ((cp->hd >= 2) || (!vp->son))) {
		// グラフィックなし。テキストのみ(type=1)
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixrshift[0] = mixrshift;
		render.mixlshift[0] = mixlshift;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = 0;
		render.mixtype = 1;
		return;
	}

	// スプライトのみ
	if (render.mixpage == 0 && cp->hd < 2 && vp->son && !render.texten) {
		// グラフィックなし、スプライトのみ(type=2)
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 10;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = render.bgsp_v;
		render.mixtype = 2;
		return;
	}

	// 優先順位決定
	if (render.tx == 3)
		render.tx--;
	if (render.sp == 3)
		render.sp--;
	if (render.gr == 3)
		render.gr--;

	if (render.tx == render.sp) {
		// 適当に決めている
		if (render.tx < render.gr) {
			render.tx = 0;
			render.sp = 1;
			render.gr = 2;
		}
		else {
			render.gr = 0;
			render.tx = 1;
			render.sp = 2;
		}
	}
	if (render.tx == render.gr) {
		// 適当に決めている
		if (render.tx < render.sp) {
			render.tx = 0;
			render.gr = 1;
			render.sp = 2;
		}
		else {
			render.sp = 0;
			render.tx = 1;
			render.gr = 2;
		}
	}
	if (render.sp == render.gr) {
		// 適当に決めている
		if (render.sp < render.tx) {
			render.sp = 0;
			render.gr = 1;
			render.tx = 2;
		}
		else {
			render.tx = 0;
			render.sp = 1;
			render.gr = 2;
		}
	}
	ASSERT((render.tx != render.gr) && (render.gr != render.sp) && (render.tx != render.sp));
	ASSERT((render.tx >= 0) && (render.tx < 3));
	ASSERT((render.sp >= 0) && (render.sp < 3));
	ASSERT((render.gr >= 0) && (render.gr < 3));
	render.mixmap[render.tx] = 0;
	render.mixmap[render.sp] = 1;
	render.mixmap[render.gr] = 2;


	// グラフィックとテキストのみ(type=5,6,7)
	if (render.mixpage > 0 && render.texten && ((cp->hd >= 2) || (!vp->son))) {
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixrshift[0] = mixrshift;
		render.mixlshift[0] = mixlshift;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = 0;
		render.mixtype = 5 + render.gr;

		// テキスト後面、グラフィック中間ならグラフィック前面で扱う(type=5)
		if (render.tx==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// グラフィックとスプライトのみ(type=5,6,7)
	if (render.mixpage > 0 && cp->hd < 2 && vp->son && !render.texten) {
		// グラフィックなし、スプライトのみ(type=2)
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 10;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = render.bgsp_v;
		render.mixtype = 5 + render.gr;

		// スプライト後面、グラフィック中間ならグラフィック前面で扱う(type=5)
		if (render.sp==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// テキストとスプライトの設定
	if (render.tx < render.sp) {
		render.mixptr[0] = render.textout;
		render.mixshift[0] = 10;
		render.mixrshift[0] = mixrshift;
		render.mixlshift[0] = mixlshift;
		render.mixx[0] = &render.textx;
		render.mixy[0] = &render.texty;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = 0;

		render.mixptr[1] = render.bgspbuf;
		render.mixshift[1] = 10;
		render.mixrshift[1] = render.bgsp_rshift;
		render.mixlshift[1] = render.bgsp_lshift;
		render.mixx[1] = &render.zero;
		render.mixy[1] = &render.zero;
		render.mixandx[1] = 1024 - 1;
		render.mixandy[1] = 1024 - 1;
		render.mixraster[1] = render.bgsp_v;
	} else {
		render.mixptr[1] = render.textout;
		render.mixshift[1] = 10;
		render.mixrshift[1] = mixrshift;
		render.mixlshift[1] = mixlshift;
		render.mixx[1] = &render.textx;
		render.mixy[1] = &render.texty;
		render.mixandx[1] = 1024 - 1;
		render.mixandy[1] = 1024 - 1;
		render.mixraster[1] = 0;

		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 10;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 1024 - 1;
		render.mixandy[0] = 1024 - 1;
		render.mixraster[0] = render.bgsp_v;
	}

	// スプライトとテキスト(type=4)
	if (render.mixpage == 0) {
		render.mixtype = 4;
		return;
	}

	// グラフィックとテキストとスプライト(type=8,9,10)
	render.mixtype = 8 + render.gr;
}

//---------------------------------------------------------------------------
//
//	コントラスト設定
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont, BOOL immediate)
{
	// システムポートの時点で一致チェックを行うので、異なっている場合のみ
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// 変更とフラグON
	render.contlevel = cont << 2;
	render.contrast = TRUE;

	// 即時反映なら段階的に変化させない
	if (immediate) {
		render.contvalue = cont << 2;
	}
}

//---------------------------------------------------------------------------
//
//	コントラスト取得
//
//---------------------------------------------------------------------------
int FASTCALL Render::GetContrast() const
{
	ASSERT(this);
	ASSERT(((render.contlevel >> 2) >= 0) && ((render.contlevel >> 2) <= 15));

	return render.contlevel >> 2;
}

//---------------------------------------------------------------------------
//
//	コントラスト処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	// コントラストが同じ値になったのでフラグＯＦＦ
	if (render.contlevel == render.contvalue) {
		render.contrast = FALSE;
	} else {
		// 現在の値を目標値に向かって段階的に増減させる
		if (render.contlevel > render.contvalue) {
			render.contvalue = (render.contlevel < (render.contvalue + 2)) ? render.contlevel : (render.contvalue + 2);
		} else {
			render.contvalue = (render.contlevel > (render.contvalue - 2)) ? render.contlevel : (render.contvalue - 2);
		}
	}

	// ポイント位置を変更、フラグDown
	render.palptr = render.palbuf;
	render.palptr += (render.contvalue >> 2) << 16;

	// パレットフラグを全てUp
	memset(render.palmod, TRUE, 0x200);
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
		ratio = 256 - ((15 - i) * 17);

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
	DWORD c;

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

	// 合成
	c = (DWORD)(r | g | b);

	// 輝度ビットがＯＮの最終チェック
	// 元データが0かコントラスト計算で0になった時でも
	// !=0の効果は維持しないと重ね合わせで判断できない
	if (color & 1) {
		if (color == 1 || c == 0) {
			c = 0x010101;
		}
	}

	return c;
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
			render.pal64k[j * 2    ] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;

			// 半透明・特殊プライオリティ用パレットデータ設定

			// GVRAM指定
			if ((i & 1) == 0) {
				render.paldataGB[i    ] = render.paldata[i];
				render.paldataGB[i + 1] = render.paldata[i] | REND_COLORS;
				render.paldataGS[i    ] = render.paldata[i];
				render.paldataGS[i + 1] = render.paldata[i];
			}

			// パレット指定
			if ((i & 1) == 0) {
				render.paldataPB[i] = render.paldata[i];

				if ((data & 1) == 1) {
					render.paldataPB[i    ] |= REND_COLORS;
					render.paldataPB[i + 1] |= REND_COLORS;
				} else {
					render.paldataPB[i + 1] &= ~REND_COLORS;
				}

				render.paldataPS[i    ] = render.paldataPB[i];
				render.paldataPS[i + 1] = render.paldataPB[i];

			} else {
				render.paldataPB[i] = render.paldata[i];

				data = (DWORD)render.palvc[i - 1];
				if ((data & 1) == 1) {
					render.paldataPB[i] |= REND_COLORS;
				}
			}
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
				render.paldata[i] |= REND_COLORT;
				// 0x100はBG・スプライトにも必ず影響
				sp = TRUE;

				// 0x100はグラフィックの半透明に影響するかもしれない
				gr = TRUE;
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
		render.grpdirty = TRUE;
	}

	// テキストフラグ
	if (tx) {
		render.textdirty = TRUE;
	}

	// スプライトフラグ
	if (sp) {
		render.bgspdirty = TRUE;
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

		render.mixdirty = TRUE;
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
//	テキスト変換
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::TextConv(int offset)
{
	int y;
	BOOL mixflag;

	// assert
	ASSERT((offset >= 0) && (offset < 1024));
	ASSERT(render.texttv);
	ASSERT(render.textflag);
	ASSERT(render.textbuf);
	ASSERT(render.palbuf);

	// 合成フラグオフ
	mixflag = FALSE;

	// 実画面Y算出
	y = (offset + render.texty) & 0x3ff;

	// 変更フラグ(逐次型)
	if (render.textmod[y]) {
		// フラグ処理
		render.textmod[y] = FALSE;
		mixflag = TRUE;

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
		mixflag = TRUE;

		// y == 1023ならコピーする
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}

	return mixflag;
}

//---------------------------------------------------------------------------
//
//	テキスト処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Text(int raster)
{
	int offset;
	BOOL mixflag;

	// ディセーブルなら何もしない
	if (!render.texten) {
		return;
	}

	// オフセット算出
	offset = raster;

	// ２度読みは半分の位置
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// 現在のオフセットの変換
	mixflag = TextConv(offset);

	// 更新があれば現在のラスターをフラグアップ
	if (mixflag) {
		render.mix[raster] = TRUE;
	}

	// スクロールがあれば次のオフセットも
	// 見えてしまうのでここで合成
	if (render.textx > 0 && offset < 1023) {
		mixflag = TextConv(offset + 1);

		// 更新があれば現在のラスターをフラグアップ
		if (mixflag) {
			render.mix[raster] = TRUE;
			render.mix[raster + 1] = TRUE;
		}

		// 更新があれば次のラスターもフラグアップ
		if (mixflag) {
			if (render.mixmode == 2) {
				render.mix[raster + 2] = TRUE;
			} else {
				render.mix[raster + 1] = TRUE;
			}
		}
	}

	// 前または次のラスターも対象
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
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
//	グラフィックスクロールセット
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
{
	BOOL flag;

	ASSERT(this);
	ASSERT((block >= 0) && (block <= 3));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// 比較チェック。非表示なら更新なし
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpben[block];
	}

	// フラグ処理
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "グラフィックスクロール block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	// グラフィックスクロール更新フラグアップ
	render.grpscrl = TRUE;

	// 合成フラグON
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	グラフィックスクロールチェック処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrlCheck()
{
	BOOL flag;
	int dx[4];
	int dy[4];
	int i;

	// フラグオフ
	render.grpscrl = FALSE;

	// スクロール更新検証
	flag = FALSE;

	// 相対位置チェックはタイプ２～４
	switch(render.grptype) {
		// タイプ0:1024×1024 16Color
		case 0:
			dx[0] = 0;
			dy[0] = 0;
			dx[1] = 0;
			dy[1] = 0;
			dx[2] = 0;
			dy[2] = 0;
			dx[3] = 0;
			dy[3] = 0;
			break;

		// タイプ1:512×512 16Color
		case 1:
			dx[0] = 0;
			dy[0] = 0;
			dx[1] = 0;
			dy[1] = 0;
			dx[2] = 0;
			dy[2] = 0;
			dx[3] = 0;
			dy[3] = 0;
			break;

		// タイプ2:512×512 256Color
		case 2:
			dx[0] = 0;
			dy[0] = 0;
			dx[1] = (render.grpx[vp->gp[1]] - render.grpx[vp->gp[0]]) & 0x1ff;
			dy[1] = (render.grpy[vp->gp[1]] - render.grpy[vp->gp[0]]) & 0x1ff;
			dx[2] = 0;
			dy[2] = 0;
			dx[3] = (render.grpx[vp->gp[3]] - render.grpx[vp->gp[2]]) & 0x1ff;
			dy[3] = (render.grpy[vp->gp[3]] - render.grpy[vp->gp[2]]) & 0x1ff;
			break;

		// タイプ3:512x512 未定義
		case 3:
		// タイプ4:512x512 65536Color
		case 4:
			dx[0] = 0;
			dy[0] = 0;
			dx[1] = (render.grpx[vp->gp[1]] - render.grpx[vp->gp[0]]) & 0x1ff;
			dy[1] = (render.grpy[vp->gp[1]] - render.grpy[vp->gp[0]]) & 0x1ff;
			dx[2] = (render.grpx[vp->gp[2]] - render.grpx[vp->gp[0]]) & 0x1ff;
			dy[2] = (render.grpy[vp->gp[2]] - render.grpy[vp->gp[0]]) & 0x1ff;
			dx[3] = (render.grpx[vp->gp[3]] - render.grpx[vp->gp[0]]) & 0x1ff;
			dy[3] = (render.grpy[vp->gp[3]] - render.grpy[vp->gp[0]]) & 0x1ff;
			break;
	}

	// 更新チェック
	for (i=0; i<4; i++) {
		if (render.grpdx[i] != dx[i]) {
			flag = TRUE;
			render.grpdx[i] = dx[i];
		}

		if (render.grpdy[i] != dy[i]) {
			flag = TRUE;
			render.grpdy[i] = dy[i];
		}
	}

	// ブロック間の相対スクロール値が
	// 更新されたらバッファ再作成
	if (!flag) {
		return;
	}

	// バッファ再作成必要
	render.grpdirty = TRUE;

	// 表示構成チェック
	GrpDispCheck();
}

//---------------------------------------------------------------------------
//
//	グラフィック表示構成チェック処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpDispCheck()
{
	// 表示状態と表示ブロックから構成を調べる
	switch(render.grptype) {
		// タイプ0:1024×1024 16Color
		case 0:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// タイプ1:512×512 16Color
		case 1:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// タイプ2:512×512 256Color
		case 2:
			render.grpnorm[0] = FALSE;
			render.grpnorm[1] = FALSE;

			if (vp->gs[0] && vp->gs[1]) {
				if (vp->gp[0] == 0 && vp->gp[1] == 1) {
					if (render.grpdx[1] == 0 && render.grpdy[1] == 0) {
						render.grpnorm[0] = TRUE;
					}
				}

				if (vp->gp[0] == 2 && vp->gp[1] == 3) {
					if (render.grpdx[1] == 0 && render.grpdy[1] == 0) {
						render.grpnorm[0] = TRUE;
					}
				}
			}

			render.grpnorm[2] = FALSE;
			render.grpnorm[3] = FALSE;

			if (vp->gs[2] && vp->gs[3]) {
				if (vp->gp[2] == 0 && vp->gp[3] == 1) {
					if (render.grpdx[3] == 0 && render.grpdy[3] == 0) {
						render.grpnorm[2] = TRUE;
					}
				}

				if (vp->gp[2] == 2 && vp->gp[3] == 3) {
					if (render.grpdx[3] == 0 && render.grpdy[3] == 0) {
						render.grpnorm[2] = TRUE;
					}
				}
			}
			break;

		// タイプ3:512x512 未定義
		case 3:
		// タイプ4:512x512 65536Color
		case 4:
			render.grpnorm[0] = FALSE;
			render.grpnorm[1] = FALSE;
			render.grpnorm[2] = FALSE;
			render.grpnorm[3] = FALSE;

			if (vp->gs[0] && vp->gs[1] && vp->gs[2] && vp->gs[3]) {
				if (vp->gp[0] == 0 && vp->gp[1] == 1 && vp->gp[2] == 2 && vp->gp[3] == 3) {
					if (render.grpdx[0] == 0 && render.grpdy[1] == 0 &&
						render.grpdx[2] == 0 && render.grpdy[3] == 0) {
						render.grpnorm[0] = TRUE;
					}
				}
			}
			break;
	}
}

//---------------------------------------------------------------------------
//
//	GVRAMバッファ構成変換
//
//---------------------------------------------------------------------------
BYTE* FASTCALL Render::MixGVRAM(BYTE *buf, int gd, int offset)
{
	int i;
	int base;
	int block[4];
	int y[4];
	int x[4];
	WORD *g[4];
	int shift[4];
	WORD *q;
	WORD mask;
	WORD data;

	// タイプ毎にGVRAMのバッファを合成する
	switch(render.grptype) {
		// タイプ0:1024×1024 16Color
		case 0:
			break;

		// タイプ1:512×512 16Color
		case 1:
			break;

		// タイプ2:512×512 256Color
		case 2:
			// ブロック,X/Yオフセット,アドレス算出
			for (i=0; i<2; i++) {
				base = vp->gp[gd];
				block[i] = vp->gp[i+gd];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAM合成
			q = (WORD*)buf;
			i = 512;
			mask  = vp->gs[gd + 1] ? 0xf0 : 0;
			mask |= vp->gs[gd    ] ? 0x0f : 0;

			while (i--) {
				data  = (WORD)(((*(g[1] + x[1])) >> shift[1]) << 4) & 0xf0;
				data |= (WORD)((*(g[0] + x[0])) >> shift[0]) & 0xf;
				*q++ = data & mask ? data : 0;
				x[0] = (x[0] + 1) & 0x1ff;
				x[1] = (x[1] + 1) & 0x1ff;
			}
			break;

		// タイプ3:512x512 未定義
		case 3:
		// タイプ4:512x512 65536Color
		case 4:
			// ブロック,X/Yオフセット,アドレス算出
			for (i=0; i<4; i++) {
				base = vp->gp[0];
				block[i] = vp->gp[i];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAM合成
			q = (WORD*)buf;
			i = 512;
			mask  = vp->gs[3] ? 0xf000 : 0;
			mask |= vp->gs[2] ? 0x0f00 : 0;
			mask |= vp->gs[1] ? 0x00f0 : 0;
			mask |= vp->gs[0] ? 0x000f : 0;

			while (i--) {
				data  = (WORD)(((*(g[3] + x[3])) >> shift[3]) << 12) & 0xf000;
				data |= (WORD)(((*(g[2] + x[2])) >> shift[2]) << 8) & 0xf00;
				data |= (WORD)(((*(g[1] + x[1])) >> shift[1]) << 4) & 0xf0;
				data |= (WORD)((*(g[0] + x[0])) >> shift[0]) & 0xf;
				*q++ = data & mask ? data : data & 0xff00;
				x[0] = (x[0] + 1) & 0x1ff;
				x[1] = (x[1] + 1) & 0x1ff;
				x[2] = (x[2] + 1) & 0x1ff;
				x[3] = (x[3] + 1) & 0x1ff;
			}
			break;
	}

	return buf;
}

//---------------------------------------------------------------------------
//
//	グラフィック処理
//
//---------------------------------------------------------------------------
void FASTCALL Render::Grp(int gd, int raster)
{
	int offset;
	DWORD *paldata;
	const BYTE *grpgv;
	int block;
	int y;
	BYTE buf[1024];

	ASSERT((gd >= 0) && (gd <= 3));
	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.grpbuf[gd]);
	ASSERT(render.grpgv);

	// グラフィックは表示されているか
	if (render.mixpage == 0) {
		return;
	}

	// タイプ別の合成判定
	if (render.grptype == 2) {
		// タイプ2:512×512 256Color
		if (gd != 0 && gd != 2) {
			return;
		}
	} else if (render.grptype > 2) {
		// タイプ3:512x512 未定義
		// タイプ4:512x512 65536Color
		if (gd != 0) {
			return;
		}
	}

	// オフセット算出
	offset = raster;

	// ２度読みは半分の位置
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// パレットデフォルト設定
	paldata = render.paldata;

	// パレット設定(特殊機能)
	if (vp->exon) {
		if (vp->bp) {
			if (gd == 0) {
				paldata = render.paldataGB;
			} else {
				paldata = render.paldataGS;
			}
		} else {
			if (!vp->gg) {
				paldata = render.paldataPB;
			} else {
				paldata = render.paldataPS;
			}
		}
	}

	// タイプ別
	switch (render.grptype) {
		// タイプ0:1024×1024 16Color
		case 0:
			// ブロック算出
			block = vp->gp[gd];

			// オフセット算出
			offset = (offset + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// 表示対象チェック
			if ((offset < 512) && (gd >= 2)) {
				return;
			}
			if ((offset >= 512) && (gd < 2)) {
				return;
			}

			// パレットの場合は全領域処理
			if (render.grppal[y + (gd << 9)]) {
				render.grppal[y + (gd << 9)] = FALSE;
				render.grpmod[y + (gd << 9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				switch (block) {
					// ブロック0
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ブロック1
					case 1:
						if (Rend1024C(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ブロック2
					case 2:
						if (Rend1024E(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ブロック3
					case 3:
						if (Rend1024G(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
				}
				break;
			}

			// それ以外はgrpmodを見て処理
			if (!render.grpmod[y + (gd << 9)]) {
				return;
			}
			render.grpmod[y + (gd << 9)] = FALSE;
			render.mix[raster] = TRUE;

			switch (block) {
				// ブロック0
				case 0:
					Rend1024B(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ブロック1
				case 1:
					Rend1024D(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ブロック2
				case 2:
					Rend1024F(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ブロック3
				case 3:
					Rend1024H(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
			}

			break;

		// タイプ1:512×512 16Color
		case 1:
			// ブロック算出
			block = vp->gp[gd];

			switch (block) {
				// ブロック0
				case 0:
					y = (offset + render.grpy[0]) & 0x1ff;
					// パレット
					if (render.grppal[y + (gd<<9)]) {
						render.grppal[y + (gd<<9)] = FALSE;
						render.grpmod[y + (gd<<9)] = FALSE;
						memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);
						if (Rend16A(render.grpgv + (y << 10),
										render.grpbuf[gd] + (y << 10),
										paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					}
					// 通常
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ブロック1
				case 1:
					y = (offset + render.grpy[1]) & 0x1ff;
					// パレット
					if (render.grppal[y + (gd<<9)]) {
						render.grppal[y + (gd<<9)] = FALSE;
						render.grpmod[y + (gd<<9)] = FALSE;
						memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);
						if (Rend16C(render.grpgv + (y << 10),
										render.grpbuf[gd] + (y << 10),
										paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					}
					// 通常
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ブロック2
				case 2:
					y = (offset + render.grpy[2]) & 0x1ff;
					// パレット
					if (render.grppal[y + (gd<<9)]) {
						render.grppal[y + (gd<<9)] = FALSE;
						render.grpmod[y + (gd<<9)] = FALSE;
						memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);
						if (Rend16E(render.grpgv + (y << 10),
										render.grpbuf[gd] + (y << 10),
										paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					}
					// 通常
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ブロック3
				case 3:
					y = (offset + render.grpy[3]) & 0x1ff;
					// パレット
					if (render.grppal[y + (gd<<9)]) {
						render.grppal[y + (gd<<9)] = FALSE;
						render.grpmod[y + (gd<<9)] = FALSE;
						memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);
						if (Rend16G(render.grpgv + (y << 10),
										render.grpbuf[gd] + (y << 10),
										paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					}
					// 通常
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16H(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
			}

			break;

		// タイプ2:512×512 256Color
		case 2:
			// ブロック算出
			block = vp->gp[gd];

			// オフセット算出
			y = (offset + render.grpy[block]) & 0x1ff;

			// パレットの場合は全領域処理
			if (render.grppal[y + (gd<<9)]) {
				render.grppal[y + (gd<<9)] = FALSE;
				render.grpmod[y + (gd<<9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				if (render.grpnorm[gd]) {
					if (block == 0) {
						if (Rend256A(render.grpgv + (y << 10),
							render.grpbuf[gd] + (y << 10),	paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					} else {
						if (Rend256C(render.grpgv + (y << 10),
							render.grpbuf[gd] + (y << 10),	paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					}
				} else {
					grpgv = MixGVRAM(buf, gd, offset);
					if (Rend256A(grpgv, render.grpbuf[gd] + (y << 10), paldata) != 0) {
						render.mix[raster] = TRUE;
					}
				}

				break;
			}

			// それ以外はgrpmodを見て処理
			if (!render.grpmod[y + (gd<<9)]) {
				return;
			}

			render.grpmod[y + (gd<<9)] = FALSE;
			render.mix[raster] = TRUE;

			if (render.grpnorm[gd]) {
				if (block == 0) {
					Rend256B(render.grpgv + (y << 10),
						render.grpbuf[gd] + (y << 10),
						render.grpflag + (y << 5) + (gd << 14), paldata);
				} else {
					Rend256D(render.grpgv + (y << 10),
						render.grpbuf[gd] + (y << 10),
						render.grpflag + (y << 5) + (gd << 14), paldata);
				}
			} else {
				grpgv = MixGVRAM(buf, gd, offset);
				Rend256B(grpgv, render.grpbuf[gd] + (y << 10),
					render.grpflag + (y << 5) + (gd << 14), paldata);
			}
			break;

		// タイプ3:512x512 未定義
		case 3:
		// タイプ4:512x512 65536Color
		case 4:
			// ブロック算出
			block = vp->gp[gd];

			// オフセット算出
			y = (offset + render.grpy[block]) & 0x1ff;

			// パレットの場合は全領域処理
			if (render.grppal[y + (gd<<9)]) {
				render.grppal[y + (gd<<9)] = FALSE;
				render.grpmod[y + (gd<<9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				if (render.grpnorm[0]) {
					grpgv = render.grpgv + (y << 10);
				} else {
					grpgv = MixGVRAM(buf, gd, offset);
				}

				// 半透明/特殊プライオリティ用パレット入れ替え特殊合成
				if (vp->exon) {
					if (vp->bp) {
						if (Rend64KBH(grpgv, render.grpbuf[0] + (y << 10),
										render.pal64k,
										render.palptr) != 0) {
							render.mix[raster] = TRUE;
						}
					} else {
						if (Rend64KBHP(grpgv, render.grpbuf[0] + (y << 10),
										render.pal64k,
										render.palptr) != 0) {
							render.mix[raster] = TRUE;
						}
					}
					break;
				}

				if (Rend64KB(grpgv, render.grpbuf[0] + (y << 10),
								render.pal64k,
								render.palptr) != 0) {
					render.mix[raster] = TRUE;
				}
				break;
			}

			// それ以外はgrpmodを見て処理
			if (!render.grpmod[y + (gd<<9)]) {
				return;
			}
			render.grpmod[y + (gd<<9)] = FALSE;
			render.mix[raster] = TRUE;

			if (render.grpnorm[0]) {
				grpgv = render.grpgv + (y << 10);
			} else {
				grpgv = MixGVRAM(buf, gd, offset);
			}

			// 半透明パレット入れ替え特殊合成
			if (vp->exon) {
				if (vp->bp) {
					Rend64KAH(grpgv, render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.pal64k,
								render.palptr);
				} else {
					Rend64KAHP(grpgv, render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.pal64k,
								render.palptr);
				}
				break;
			}

			Rend64KA(grpgv,	render.grpbuf[0] + (y << 10),
						render.grpflag + (y << 5),
						render.pal64k,
						render.palptr);

			break;
	}

	// 前または次のラスターも対象
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
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
	int i;
	DWORD addr;
	DWORD data[4];

	// スプライトレジスタ設定
	for (i=0; i<128; i++) {
		addr = i << 3;
		data[0] = *(WORD*)(&render.sprmem[addr    ]);
		data[1] = *(WORD*)(&render.sprmem[addr + 2]);
		data[2] = *(WORD*)(&render.sprmem[addr + 4]);
		data[3] = *(WORD*)(&render.sprmem[addr + 6]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	スプライトレジスタ変更
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReg(DWORD addr, DWORD data[])
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
	int x;
	int y;

	ASSERT(this);
	ASSERT(addr < 0x400);
	ASSERT((addr & 1) == 0);

	// インデクシングとデータ制限
	index = (int)(addr >> 3);

	// X(0～1023)
	data[0] &= 0x3ff;

	// Y(0～1023)
	data[1] &= 0x3ff;
	y=data[1];
//	y+=render.bgsp_v;
	data[1] = y;

	// V,H,PAL,PCG
	data[2] &= 0xcfff;

	// PRW(0,1,2,3)
	data[3] &= 0x0003;

	// ptr設定
	ptr = &render.spptr[index << 10];

	// レジスタのバックアップ
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// レジスタへ書き込み
	next[0] = data[0];
	next[1] = data[1];
	next[2] = data[2];
	next[3] = data[3];

	// 今後有効になるかチェック
	use = TRUE;
	x = next[0];
	y = next[1];

	// 標準範囲チェック
	if (x == 0) {
		use = FALSE;
	}

	if (x >= (512 + 16)) {
		use = FALSE;
	}

	x -= render.bgsp_h;
	if (render.bgsp_h >= 0) {
		// 左にずれる
		if (x <= 0) {
			use = FALSE;
		}
		if (x >= (render.mixlen + 16)) {
			use = FALSE;
		}
	} else {
		// 右にずれる
		x &= 511;
		if (x >= (render.mixlen + 16)) {
			use = FALSE;
		}
	}

	y &= 0x3ff;

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
		ASSERT(render.pcguse[pcgno] > 0);
		render.pcguse[pcgno]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[pcgno] > 0);
		render.pcgpal[pcgno]--;

		// 無効処理(ポインタ)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			j &= 0x3ff;
			ptr[j] = NULL;
			render.bgspmod[j] = TRUE;
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
	render.pcguse[pcgno]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[pcgno]++;

	// PCGアドレスを計算、ポインタセット
	if (next[2] & 0x8000) {
		// V反転
		offset += 0xf0;
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			j &= 0x3ff;
			ptr[j] = &render.pcgbuf[offset];
			render.bgspmod[j] = TRUE;
			offset -= 16;
		}
	}
	else {
		// ノーマル
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			j &= 0x3ff;
			ptr[j] = &render.pcgbuf[offset];
			render.bgspmod[j] = TRUE;
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
	if (page==0) {
		flag = TRUE;
	}
	if (page==1 && !render.bgsize) {
		flag = TRUE;
	}
	if (flag) {
		render.bgspdirty = TRUE;
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

		// DISP/CPU
		case 5:
			if (render.bgspdisp == flag) {
				return;
			}
			// 表示状態チェックはProcessで実行
			render.bgsp = TRUE;
			return;

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
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcguse[pcgno]--;
					pcgno = (pcgno >> 8) & 0x0f;
					ASSERT(render.pcgpal[pcgno] > 0);
					render.pcgpal[pcgno]--;
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
		render.bgspdirty = TRUE;
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
			ASSERT(render.pcguse[pcgno] > 0);
			render.pcguse[pcgno]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[pcgno] > 0);
			render.pcgpal[pcgno]--;
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
			raster = (index << 4);
			raster -= render.bgy[i];

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
			raster = (index << 3);
			raster -= render.bgy[i];

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
	for (i=0; i<16; i++) {
		if (render.pcguse[index + (i << 8)] == 0) {
			continue;
		}

		// 仕方ないので、BG/スプライト再合成を決定
		render.bgspdirty = TRUE;
		break;
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
	int offset;
	int i;
	DWORD *reg;
	DWORD **ptr;
	DWORD *buf;
	DWORD pcgno;
	BYTE pri[512 + 16];
	int x;

	// 横幅も1024まで。これも大前提
	if (render.mixlen > 1024) return;

	// オフセットを算出
	offset = ((raster + render.bgsp_v) >> render.bgsp_rshift) << render.bgsp_lshift;
	offset &= 1023;

	// フラグチェック、オフ、合成指示
	if (!render.bgspmod[offset]) {
		return;
	}
	render.bgspmod[offset] = FALSE;
	render.mix[raster] = TRUE;

	// インタレースのときは偶奇ラスターペア
	if (render.mixmode == 1) {
		render.mix[raster ^ 1] = TRUE;
	}

	// ２度読みの場合でノンインタレースなら前後ラスター対象
	if (render.bgsp_mixmode == 2 && render.mixmode != 1) {
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
	}

	// ２度読みの場合でインタレースなら前後3ラスター対象
	if (render.bgsp_mixmode == 2 && render.mixmode == 1) {
		render.mix[(raster - 3) & 1023] = TRUE;
		render.mix[(raster - 2) & 1023] = TRUE;
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
		render.mix[(raster + 2) & 1023] = TRUE;
		render.mix[(raster + 3) & 1023] = TRUE;
	}

	// バッファクリア
	// ここでテキストパレット0で埋める(出たツイLoading)
	buf = &render.bgspbuf[offset << 10];
	RendClrSprite(buf, render.paldata[0x100],
		render.mixlen + 16 > 1024 ? 1024 : render.mixlen + 16);

	if (!render.bgspflag || !render.bgspdisp) {
		// 非表示なら終了
		return;
	}

	// スプライト間優先度バッファをクリア
	memset(pri, 0xff, sizeof(pri));

	// 一番後ろにくる(PRW=1)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
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

					// 合成基準位置調整
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// 左にずれる
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// 右にずれる
						if (x <= 512) {
							RendSprite(*ptr, buf - render.bgsp_h,
								reg[0], reg[2] & 0x4000, i, pri - render.bgsp_h);
						} else if (x < 528) {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - x);
						}

						if (x > 512) {
							x &= 511;
							if (reg[0] <= 512) {
								RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
							} else {
								RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
							}
						}
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 1024;
	}

	// BG1を表示
	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, offset, buf, TRUE);
	}

	// 中間にくる(PRW=2)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
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

					// 合成基準位置調整
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// 左にずれる
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// 右にずれる
						if (x <= 512) {
							RendSprite(*ptr, buf - render.bgsp_h,
								reg[0], reg[2] & 0x4000, i, pri - render.bgsp_h);
						} else if (x < 528) {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - x);
						}

						if (x > 512) {
							x &= 511;
							if (reg[0] <= 512) {
								RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
							} else {
								RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
							}
						}
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 1024;
	}

	// BG0を表示
	if (render.bgdisp[0]) {
		if (render.bgdisp[1] && !render.bgsize) {
			BG(0, offset, buf, FALSE);
		} else {
			BG(0, offset, buf, TRUE);
		}
	}

	// 手前にくる(PRW=3)スプライト
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
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

					// 合成基準位置調整
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// 左にずれる
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// 右にずれる
						if (x <= 512) {
							RendSprite(*ptr, buf - render.bgsp_h,
								reg[0], reg[2] & 0x4000, i, pri - render.bgsp_h);
						} else if (x < 528) {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - x);
						}

						if (x > 512) {
							x &= 511;
							if (reg[0] <= 512) {
								RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
							} else {
								RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
							}
						}
					}
				}
			}
		}
		// 次のスプライト(SP0がもっとも手前)
		reg -= 4;
		ptr -= 1024;
	}
}

//---------------------------------------------------------------------------
//
//	BG
//
//---------------------------------------------------------------------------
void FASTCALL Render::BG(int page, int offset, DWORD *buf, BOOL force)
{
	int x;
	int y;
	bgdata_t *ptr;
	int len;
	int rest;

	ASSERT((page == 0) || (page == 1));
	ASSERT((offset >= 0) && (offset < 1024));
	ASSERT(buf);

	// 位置調整分は表示しない
	y = offset;

	// yブロックを割り出す
	y = render.bgy[page] + offset;
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
		x = (render.bgx[page] + render.bgsp_h) & (512 - 1);
		ptr += (((render.bgy[page] + offset) & (512 - 1)) << 6);

		// 割り切れるかチェック
		if ((x & 7) == 0) {
			// 8x8、割り切れる
			x >>= 3;
			if (force) {
				RendBG8F(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			} else {
				RendBG8(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			return;
		}

		// 最初の半端ブロックを実行
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		if (force) {
			RendBG8FP(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
		} else {
			RendBG8P(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
		}

		// 余りを調べて8dot単位分を処理
		len = render.mixlen - rest;
		x += rest;
		x &= (512 - 1);
		ASSERT((x & 7) == 0);

		if (force) {
			RendBG8F(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		} else {
			RendBG8(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}

		// 最後
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);

			if (force) {
				RendBG8FP(&ptr[x >> 3], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
			} else {
				RendBG8P(&ptr[x >> 3], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
			}
		}
		return;
	}

	// 16x16の表示
	x = (render.bgx[page] + render.bgsp_h) & (1024 - 1);
	ptr += (((render.bgy[page] + offset) & (1024 - 1)) << 6);

	// 割り切れるかチェック
	if ((x & 15) == 0) {
		// 16x16、割り切れる
		x >>= 4;

		if (force) {
			RendBG16F(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		} else {
			RendBG16(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		return;
	}

	// 最初の半端ブロックを実行
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	if (force) {
		RendBG16FP(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
	} else {
		RendBG16P(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
	}

	// 余りを調べて16dot単位分を処理
	len = render.mixlen - rest;
	x += rest;
	x &= (1024 - 1);
	ASSERT((x & 15) == 0);
	if (force) {
		RendBG16F(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	} else {
		RendBG16(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}

	// 最後
	if (len & 15) {
		x += (len & 0xfff0);
		x &= (1024 - 1);
		x >>= 4;

		if (force) {
			RendBG16FP(&ptr[x], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		} else {
			RendBG16P(&ptr[x], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
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
	bgdata_t *ptr;
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
		ptr += (y << 10);
	}
	else {
		ptr += (y << 9);
	}

	// ループ
	for (i=0; i<64; i++) {
		// 取得
		bgdata = reg[i];

		// $10000が立っていればOK
		if (bgdata & 0x10000) {
			ptr += 1;
			continue;
		}

		// $10000をOR
		reg[i] |= 0x10000;

		// pcgnoを得る
		pcgno = bgdata & 0xfff;

		// サイズ別
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
			if (bgdata & 0x8000) {
				// 上下反転
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// 通常
				for (j=0; j<16; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf += 0x10;
					ptr += 64;
				}
			}
			ptr -= 1024;
		}
		else {
			// 8x8。bit17,bit18を考慮する
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
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
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// 通常
				for (j=0; j<8; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf += 0x10;
					ptr += 64;
				}
			}
			ptr -= 512;
		}

		// 登録処理(PCG)
		render.pcguse[pcgno]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[pcgno]++;

		// ポインタを進める
		ptr += 1;
	}
}

//---------------------------------------------------------------------------
//
//	BG/スプライトチェック
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSpriteCheck(int raster)
{
	// ２度読みは偶数ラスターのみ(コットン)
	if (render.mixmode == 2 && raster & 1) {
		return;
	}

	// フラグオフ
	render.bgsp = FALSE;

	// スプライト表示切替えチェックする
	if (sprite->IsDisplay()) {
		if (!render.bgspdisp) {
			render.bgspdisp = TRUE;
			render.bgspdirty = TRUE;
		}
	} else {
		if (render.bgspdisp) {
			render.bgspdisp = FALSE;
			render.bgspdirty = TRUE;
		}
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
void FASTCALL Render::Mix(int raster, int xoffset)
{
	int mixlen;
	int mixy;
	DWORD *mixbuf;
	BOOL *mixflg;
	DWORD y;
	DWORD *ptr[4];
	DWORD grpbuf[1024];

	// 合成指示が無い場合、yオーバーの場合return
	if (!render.mix[raster]) {
		return;
	}

	if (render.mixheight <= raster) {
		return;
	}

	// 合成長の決定
	mixlen = render.mixlen - (xoffset << 4);

	// 合成しない
	if (mixlen<=0) {
		return;
	}

#if defined(REND_LOG)
	LOG1(Log::Normal, "合成 raster=%d", raster);
#endif	// REND_LOG

	// フラグOFF
	// 水平描画途中の書き換えは次回更新必要なのでOFFしない
	if (xoffset==0) {
		render.mix[raster] = FALSE;
	}

	// バッファ合成位置
	mixy = raster;

	// 表示周波数15kHzでインタレースで無い
 	if (render.mixmode != 1 && render.hres == 0) {
		mixy <<= 1;
	}

	// このラインは更新する可能性有り
	render.draw[mixy] = TRUE;

	// 合成バッファアドレス初期化
	mixbuf = &render.mixbuf[render.mixwidth * mixy];
	mixbuf += (xoffset << 4);
	mixflg = render.drawflag + (mixy << 6);
	mixflg += xoffset;

	switch (render.mixtype) {

		// タイプ0(表示しない)
		case 0:
			RendMix00(mixbuf, mixflg, mixlen);
			break;

		// タイプ1(テキストのみ)
		case 1:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			RendMix01(mixbuf, ptr[0], mixflg, mixlen);
			break;

		// タイプ2(スプライトのみ)
		case 2:
			y = raster + render.mixraster[0];
			y >>= render.mixrshift[0];
			y <<= render.mixlshift[0];
			y += *render.mixy[0];
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			RendMix01(mixbuf, ptr[0], mixflg, mixlen);
			break;

		// タイプ3(グラフィックのみ)
		case 3:
			ptr[2]=MixGrp(grpbuf, raster, xoffset, mixlen);

			// 半透明
			if (vp->exon && vp->hp && vp->gt) {
				RendMix01H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else {
				RendMix01(mixbuf, ptr[2], mixflg, mixlen);
			}
			break;

		// タイプ4(テキスト、スプライト)
		case 4:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			y = *render.mixy[1];
			y += (((raster + render.mixraster[1]) >> render.mixrshift[1]) << render.mixlshift[1]);
			y &= render.mixandy[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (y << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] + (xoffset << 4)) & render.mixandx[1];
			RendMix02A(mixbuf, ptr[0], ptr[1], mixflg, mixlen);
			break;

		// タイプ5(テキスト or スプライト中間または後面、グラフィック前面)
		case 5:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix02BH(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			} else {
				// 通常
				RendMix02B(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			}
			break;

		// タイプ6(テキスト or スプライト前面、グラフィック中間、後面なし)
		case 6:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix02CH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// 特殊プライオリティ
				RendMix02CS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// 通常
				RendMix02C(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// タイプ7(テキスト or スプライト前面または中間、グラフィック後面)
		case 7:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix02DH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// 特殊プライオリティ
				RendMix02DS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// 通常
				RendMix02D(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// タイプ8(テキスト、スプライト、グラフィック前面)
		case 8:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			y = *render.mixy[1];
			y += (((raster + render.mixraster[1]) >> render.mixrshift[1]) << render.mixlshift[1]);
			y &= render.mixandy[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (y << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] + (xoffset << 4)) & render.mixandx[1];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix03AH(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			} else {
				// 通常
				RendMix03A(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			}
			break;

		// タイプ9(テキスト、スプライト、グラフィック中間)
		case 9:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			y = *render.mixy[1];
			y += (((raster + render.mixraster[1]) >> render.mixrshift[1]) << render.mixlshift[1]);
			y &= render.mixandy[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (y << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] + (xoffset << 4)) & render.mixandx[1];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix03BH(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// 特殊プライオリティ
				RendMix03BS(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else {
				// 通常
				RendMix03B(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			}
			break;

		// タイプ10(テキスト、スプライト、グラフィック後面)
		case 10:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			y = *render.mixy[1];
			y += (((raster + render.mixraster[1]) >> render.mixrshift[1]) << render.mixlshift[1]);
			y &= render.mixandy[1];
			ptr[1] = render.mixptr[1];
			ptr[1] += (y << render.mixshift[1]);
			ptr[1] += (*render.mixx[1] + (xoffset << 4)) & render.mixandx[1];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// 半透明
				RendMix03CH(mixbuf, ptr[0], ptr[1], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// 特殊プライオリティ
				RendMix03CS(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			} else {
				// 通常
				RendMix03C(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			}
			break;

		// タイプ11(パレット０半透明)
		case 11:
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);
			RendMixP0H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			break;

		// その他
		default:
			ASSERT(FALSE);
			break;
	}

	// 表示周波数15kHzでインタレースで無いとき
 	if (render.mixmode != 1 && render.hres == 0) {
		if (render.scanline) {
			// スキャンライン描画
			memset(mixbuf + render.mixwidth, 0x00, render.mixlen << 2);
		} else {
			// ラスターコピー
			memcpy(mixbuf + render.mixwidth, mixbuf, render.mixlen << 2);
		}

		// スキャンラインのラスタもフラグを設定
		memcpy(mixflg + 64, mixflg, (64 - xoffset) << 2);
		render.draw[mixy + 1] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	グラフィックス合成
//
//---------------------------------------------------------------------------
DWORD* FASTCALL Render::MixGrp(DWORD *buf, int raster, int xoffset, int mixlen)
{
	int i;
	int j;
	DWORD *ptr[4];
	int idx;
	int y;
	int mixpage;
	int map[4];

	ASSERT(buf);
	ASSERT((raster >= 0) && (raster < render.mixheight));

	// 0面(ありえない)
	if (render.mixpage==0) {
		ASSERT(FALSE);
		return buf;
	}

	// ワーク初期化
	mixpage = 0;
	for (i=0; i<4; i++) {
		map[i] = -1;
	}

	// バッファ計算
	j = 0;
	for (i=0; i<render.mixpage; i++) {
		idx = i + 4;

		y = *render.mixy[idx];
		y += (((raster + render.mixraster[idx]) >> render.mixrshift[idx]) << render.mixlshift[idx]);
		y &= render.mixandy[idx];
		ptr[i] = render.mixptr[idx];
		ptr[i] += (y << render.mixshift[idx]);
		ptr[i] += (*render.mixx[idx] + (xoffset << 4)) & render.mixandx[idx];

		// 実際の表示ページ数をカウント
		if (render.grppen[i]) {
			mixpage++;
			map[j++] = i;
		}
	}

	// 0面合成(合成しない)
	if (mixpage==0) {
		return buf;
	}

	// 1面で1面表示
	if (render.mixpage == 1 && mixpage==1) {
		memcpy(buf, ptr[map[0]], mixlen << 2);
	}

	// 2面以上
	if (render.mixpage >= 2) {

		// 半透明をチェック(2面以上でかつベースページが表示)
		if (vp->exon && vp->hp && vp->gg && render.grppen[0]) {
			if (render.grppen[1]) {
				// ベース表示、セカンド表示
				RendGrp02HBS(buf, ptr[0], ptr[1], mixlen);

				if (mixpage==3) {
					RendGrp02(buf, buf, ptr[map[2]], mixlen);
				}

				if (mixpage==4) {
					RendGrp03(buf, buf, ptr[map[2]], ptr[map[3]], mixlen);
				}
			} else {
				// ベース表示、セカンド非表示
				// セカンドページが表示されていない場合は半透明の部分のみ合成
				RendGrp02HB(buf, ptr[0], ptr[1], mixlen);

				if (mixpage==2) {
					RendGrp02(buf, buf, ptr[map[1]], mixlen);
				}

				if (mixpage==3) {
					RendGrp03(buf, buf, ptr[map[1]], ptr[map[2]], mixlen);
				}
			}
		} else {
			if (mixpage==1) {
				// 1面表示
				memcpy(buf, ptr[map[0]], mixlen << 2);
			} else if (mixpage==2) {
				// 2面合成
				RendGrp02(buf, ptr[map[0]], ptr[map[1]], mixlen);
			} else if (mixpage==3) {
				// 3面合成
				RendGrp03(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], mixlen);
			} else {
				// 4面合成
				RendGrp04(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], ptr[map[3]], mixlen);
			}
		}
	}

	// 返却
	return buf;
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
//	合成(強制)
//
//---------------------------------------------------------------------------
void FASTCALL Render::UpdateMixBuf()
{
	BOOL bScanEven;
	int i;
	int j;

	ASSERT(this);

	// インタレース時のスキャンライン偶奇取得
	bScanEven = cp->v_scaneven;

	// インタレース時に備えて２回合成する
	for (i=0; i<2; i++) {
		Complete();
		EnableAct(TRUE);
		render.crtc = TRUE;
		render.vc = TRUE;
		render.grpscrl = TRUE;
		render.palette = TRUE;
		render.textdirty = TRUE;
		render.bgsp = TRUE;
		render.grpdirty = TRUE;
		render.bgspdirty = TRUE;
		render.mixdirty = TRUE;
		StartFrame();
		render.mixeven = bScanEven;
		for (j=0; j<render.height; j++) {
			HSync(j, 0);
		}
		EndFrame();
		bScanEven = !bScanEven;
	}
}

//---------------------------------------------------------------------------
//
//	レンダリング
//
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//
//	PX68k coexistence bridge
//
//---------------------------------------------------------------------------
void FASTCALL Render::HSync(int raster, int xoffset)
{
	ASSERT(this);

	if (render_fast_dummy_enabled && px68k_adapter) {
		px68k_adapter->HSync(this, raster);
		return;
	}

	render.last = raster + 1;
	if (render.act) {
		Process(raster, xoffset);
	}
}

BOOL FASTCALL Render::SetCompositorMode(int mode)
{
	if ((mode != compositor_original) && (mode != compositor_fast)) {
		return FALSE;
	}
	compositor_mode = mode;
	render_fast_dummy_enabled = (mode == compositor_fast) ? TRUE : FALSE;
	ForceRecompose();
	return TRUE;
}

void FASTCALL Render::GetFastVerticalProbeSnapshot(fast_vertical_probe_snapshot_t *out) const
{
	if (out) {
		memset(out, 0, sizeof(*out));
	}
}

BOOL FASTCALL Render::SetRenderFastDummyEnabled(BOOL enable)
{
	SetCompositorMode(enable ? compositor_fast : compositor_original);
	return render_fast_dummy_enabled;
}

BOOL FASTCALL Render::EnsurePx68kFrame()
{
	if (!render_fast_dummy_enabled || !px68k_adapter) {
		return FALSE;
	}
	px68k_adapter->DrawFrame(this);
	return TRUE;
}

BOOL FASTCALL Render::GetPx68kScreen(const WORD **out_pixels, int *out_width, int *out_height, int *out_stride) const
{
	if (!px68k_adapter) {
		return FALSE;
	}
	if (out_pixels) *out_pixels = px68k_adapter->GetScreenBuffer();
	if (out_width) *out_width = (int)px68k_adapter->GetScreenWidth();
	if (out_height) *out_height = (int)px68k_adapter->GetScreenHeight();
	if (out_stride) *out_stride = (int)px68k_adapter->GetScreenStride();
	return TRUE;
}

const Px68kCrtcHost* FASTCALL Render::GetPx68kCrtcHost() const
{
	return &px68k_crtc_host;
}

void FASTCALL Render::CachePx68kStateView(const Px68kCrtcStateView *view)
{
	if (view) {
		px68k_crtc_state_cache = *view;
	}
}

void FASTCALL Render::ForceRecompose()
{
	render.crtc = TRUE;
	render.vc = TRUE;
	render.palette = TRUE;
	render.textdirty = TRUE;
	render.grpdirty = TRUE;
	render.bgspdirty = TRUE;
	render.mixdirty = TRUE;
}

const TVRAM* FASTCALL Render::GetTVRAMDevice() const
{
	return (const TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
}

const GVRAM* FASTCALL Render::GetGVRAMDevice() const
{
	return (const GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
}

void FASTCALL Render::SpriteBGWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) px68k_adapter->BGWrite(addr, data);
}

BYTE FASTCALL Render::TVRAMRead(DWORD addr)
{
	return px68k_adapter ? px68k_adapter->TVRAMRead(addr) : 0xff;
}

void FASTCALL Render::TVRAMWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) px68k_adapter->TVRAMWrite(addr, data);
}

BYTE FASTCALL Render::GVRAMRead(DWORD addr)
{
	return px68k_adapter ? px68k_adapter->GVRAMRead(addr) : 0xff;
}

void FASTCALL Render::GVRAMWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) px68k_adapter->GVRAMWrite(addr, data);
}

BYTE FASTCALL Render::BGRead(DWORD addr)
{
	return px68k_adapter ? px68k_adapter->BGRead(addr) : 0xff;
}

void FASTCALL Render::CRTCRegWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) px68k_adapter->CRTCRegWrite(addr, data);
}

BYTE FASTCALL Render::CRTCRegRead(DWORD addr)
{
	return px68k_adapter ? px68k_adapter->CRTCRegRead(addr) : 0xff;
}

BYTE FASTCALL Render::VCtrlRead(DWORD addr)
{
	return px68k_adapter ? px68k_adapter->VCtrlRead(addr) : 0xff;
}

void FASTCALL Render::VCtrlWrite(DWORD addr, BYTE data)
{
	if (px68k_adapter) px68k_adapter->VCtrlWrite(addr, data);
}

void FASTCALL Render::GVRAMFastClear()
{
	if (px68k_adapter) px68k_adapter->GVRAMFastClear();
}
void FASTCALL Render::Process(int raster, int xoffset)
{
	// 安全策
	if (render.mixlen <= 0) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ビデオ処理");
#endif	// RENDER_LOG
		Video();
	}

	// パレット
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "パレット処理");
#endif	// RENDER_LOG
		Palette();
	}

	// グラフィックスクロールチェック
	if (render.grpscrl) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "グラフィックスクロールチェック処理");
#endif	// RENDER_LOG
		GrpScrlCheck();
	}

	// BGスプライトチェック
	if (render.bgsp) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "BG/スプライトチェック処理");
#endif	// RENDER_LOG
		BGSpriteCheck(raster);
	}

	// テキスト更新
	if (render.textdirty) {
		memset(render.textpal, TRUE, 1024);
		render.textdirty = FALSE;
	}

	// グラフィック更新
	if (render.grpdirty) {
		memset(render.grppal, TRUE, 512 * 4);
		render.grpdirty = FALSE;
	}

	// BGスプライト更新
	if (render.bgspdirty) {
		memset(render.bgspmod, TRUE, 1024);
		render.bgspdirty = FALSE;
	}

	// 合成全更新
	if (render.mixdirty) {
		memset(render.mix, TRUE, render.mixheight);
		render.mixdirty = FALSE;
	}

	// インタレースなら偶数と奇数ラスターを一括合成
	if (render.mixmode == 1) {
		raster <<= 1;
		if (render.mixeven) {
			// 合成実行(偶数ライン)
			Text(raster);
			Grp(0, raster);
			Grp(1, raster);
			Grp(2, raster);
			Grp(3, raster);
			BGSprite(raster);
			Mix(raster, xoffset);

			// 合成実行(奇数ライン)
			raster++;
			Text(raster);
			Grp(0, raster);
			Grp(1, raster);
			Grp(2, raster);
			Grp(3, raster);
			BGSprite(raster);
			Mix(raster, xoffset);
		}
		return;
	}

	// 合成実行
	Text(raster);
	Grp(0, raster);
	Grp(1, raster);
	Grp(2, raster);
	Grp(3, raster);
	BGSprite(raster);
	Mix(raster, xoffset);
}
