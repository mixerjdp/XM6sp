//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ レンダラ ]
//
//---------------------------------------------------------------------------

#ifndef render_h
#define render_h

#include "device.h"
#include "crtc.h"
#include "vc.h"
#include "rend_asm.h"
#include "px68k_crtc_port.h"

class GVRAM;
class Px68kRenderAdapter;
class TVRAM;

//===========================================================================
//
//	レンダラ
//
//===========================================================================
class Render : public Device
{
public:
	// PX68k coexistence API
	enum compositor_mode_t {
		compositor_original = 0,
		compositor_fast = 1,
	};

	typedef struct {
		BOOL valid;
		int dst_y;
		int src_y;
		DWORD px68k_vline;
		int sprite_raster;
		int bg_raster;
		int layer_raster;
		int bg_vline;
		int vline_bg;
		BOOL visible;
		BOOL bg_on;
		BOOL bg_opaq;
		BOOL sprite_enabled;
		BOOL bgspflag;
		BOOL bgspdisp;
		BOOL gon;
		BOOL tron;
		BOOL pron;
		BOOL ton;
		BYTE vr2h;
		BYTE vr2l;
		int vscan;
		int vdots;
		DWORD vcount;
		BOOL vblank;
		int rcount;
		int vstep;
		int mixlen;
		BOOL lowres;
		int vmul;
	} fast_vertical_probe_sample_t;

	typedef struct {
		BOOL valid;
		int width;
		int h_mul;							// X multiplier
		int height;
		int v_mul;							// Y multiplier
		int mixwidth;
		int mixheight;
		int mixpage;
		int mixtype;
		BOOL lowres;
		BOOL bgspflag;
		BOOL bgspdisp;
		int sample_count;
		fast_vertical_probe_sample_t samples[6];
	} fast_vertical_probe_snapshot_t;

	// 内部データ定義
	typedef struct {
		// 全体制御
		BOOL act;						// 合成しているか
		BOOL enable;					// 合成許可
		int count;						// スケジューラ連携カウンタ
		BOOL ready;						// 描画準備できているか
		int first;						// 未処理ラスタ
		int last;						// 表示終了ラスタ

		// CRTC
		BOOL crtc;						// CRTC変更フラグ
		int width;						// X方向ドット数(256～)
		int h_mul;							// X multiplier
		int height;						// Y方向ドット数(256～)
		int v_mul;							// Y multiplier
		BOOL siz;						// 実画面サイズ
		BOOL lowres;					// 15kHzフラグ
		int hres;						// 水平周波数モード(0:15kHz, 1:24kHz, 2:31kHz)
		int hd;							// 水平ドット数
		int vd;							// 垂直ドット数
		BOOL hrl;						// HRL(システムポート)

		// CRTエミュレーション用
		int h_disp;						// 水平表示幅
		int v_disp;						// 垂直表示幅
		int h_total;					// 水平同期幅
		int v_total;					// 垂直同期幅
		int h_pulse;					// 水平パルス幅
		int v_pulse;					// 垂直パルス幅
		int h_start;					// 水平描画位置
		int v_start;					// 垂直描画位置
		BOOL scanline;					// スキャンラインモード

		// VC
		BOOL vc;						// VC変更フラグ

		// 合成
		BYTE mix[1024];					// 合成フラグ(ライン)
		DWORD *mixbuf;					// 合成バッファ
		DWORD *mixptr[8];				// 合成ポインタ
		DWORD mixshift[8];				// 合成ポインタのYシフト
		DWORD mixrshift[8];				// 合成ポインタのYシフト(右)
		DWORD mixlshift[8];				// 合成ポインタのYシフト(左)
		DWORD *mixx[8];					// 合成ポインタのXスクロールポインタ
		DWORD *mixy[8];					// 合成ポインタのYスクロールポインタ
		DWORD mixandx[8];				// 合成ポインタのスクロールAND値(X)
		DWORD mixandy[8];				// 合成ポインタのスクロールAND値(Y)
		DWORD mixraster[8];				// 合成ポインタのラスター位置調整値
		int mixmap[3];					// 合成マップ
		int mixtype;					// 合成タイプ
		int mixpage;					// 合成グラフィックページ数
		int mixwidth;					// 合成バッファ幅
		int mixheight;					// 合成バッファ高さ
		int mixlen;						// 合成時処理長さ(x方向)
		int mixmode;					// 合成モード(0:ノインタレース, 1:インタレース，2:2度読み)
		BOOL mixeven;					// インタレース時の合成偶奇
		int sp;							// 優先順位(スプライト)
		int gr;							// 優先順位(グラフィックス)
		int tx;							// 優先順位(テキスト)
		BOOL mixdirty;					// 合成ダーティーフラグ

		// 描画
		BOOL draw[1024];				// 描画フラグ(ライン)
		BOOL *drawflag;					// 描画フラグ(16dot)

		// コントラスト
		BOOL contrast;					// コントラスト変更フラグ
		int contlevel;					// コントラスト(設定レベル)
		int contvalue;					// コントラスト(現在の値)
		DWORD conttime;					// 前回コントラストを変更した時刻

		// パレット
		BOOL palette;					// パレット変更フラグ
		BYTE palmod[0x200];				// パレット変更フラグ
		DWORD *palbuf;					// パレットバッファ
		DWORD *palptr;					// パレットポインタ
		const WORD *palvc;				// パレットVCポインタ
		DWORD paldata[0x200];			// パレットデータ
		BYTE pal64k[0x200];				// パレットデータ変形

		// パレット(半透明、特殊プライオリティ)
		DWORD paldataGB[0x100];			// パレットデータ(GVRAM指定:ベースページ)
		DWORD paldataGS[0x100];			// パレットデータ(GVRAM指定:セカンドページ)
		DWORD paldataPB[0x100];			// パレットデータ(パレット指定:ベースページ)
		DWORD paldataPS[0x100];			// パレットデータ(パレット指定:セカンドページ)

		// テキストVRAM
		BOOL texten;					// テキスト表示フラグ
		BYTE textpal[1024];				// テキストパレットフラグ
		BOOL textmod[1024];				// テキスト更新フラグ(ライン)
		BOOL *textflag;					// テキスト更新フラグ(32dot)
		BYTE *textbuf;					// テキストバッファ(パレット前)
		DWORD *textout;					// テキストバッファ(パレット後)
		const BYTE *texttv;				// テキストTVRAMポインタ
		DWORD textx;					// テキストスクロールX
		DWORD texty;					// テキストスクロールY
		BOOL textdirty;					// テキストダーティーフラグ

		// グラフィックVRAM
		int grptype;					// グラフィックタイプ(0～4)
		BYTE grppal[2048];				// グラフィックパレットフラグ
		BOOL grpmod[2048];				// グラフィック更新フラグ(ライン)
		BOOL *grpflag;					// グラフィック更新フラグ(16dot)
		DWORD *grpbuf[4];				// グラフィックブロックバッファ
		const BYTE* grpgv;				// グラフィックGVRAMポインタ
		BOOL grpscrl;					// グラフィックスクロール更新フラグ
		DWORD grpx[4];					// グラフィックブロックスクロールX
		DWORD grpy[4];					// グラフィックブロックスクロールY
		int grpdx[4];					// グラフィックブロックスクロールX差分
		int grpdy[4];					// グラフィックブロックスクロールY差分
		BOOL grppen[4];					// グラフィックページ有効フラグ
		BOOL grpen[4];					// Graphic enable mirror
		BOOL grpben[4];					// グラフィックブロック有効フラグ
		BOOL grpnorm[4];				// グラフィック推奨設定ページフラグ
		BOOL grpdirty;					// グラフィックダーティーフラグ

		// PCG
		BOOL pcgready[256 * 16];		// PCG準備OKフラグ
		DWORD pcguse[256 * 16];			// PCG使用中カウント
		DWORD pcgpal[16];				// PCGパレット使用カウント
		DWORD *pcgbuf;					// PCGバッファ
		const BYTE* sprmem;				// スプライトメモリ

		// スプライト
		DWORD **spptr;					// スプライトポインタバッファ
		DWORD spreg[0x200];				// スプライトレジスタ保存
		BOOL spuse[128];				// スプライト使用中フラグ

		// BG
		DWORD bgreg[2][64 * 64];		// BGレジスタ＋変更フラグ($10000)
		BOOL bgall[2][64];				// BG変更フラグ(ブロック単位)
		BOOL bgdisp[2];					// BG表示フラグ
		BOOL bgarea[2];					// BG表示エリア
		BOOL bgsize;					// BG表示サイズ(16dot=TRUE)
		bgdata_t *bgptr[2];				// BGポインタ+データ
		BOOL bgmod[2][1024];			// BG更新フラグ
		DWORD bgx[2];					// BGスクロール(X)
		DWORD bgy[2];					// BGスクロール(Y)

		// BG/スプライト合成
		BOOL bgsp;						// BG/スプライト変更フラグ
		BOOL bgspflag;					// BG/スプライト表示フラグ
		BOOL bgspdisp;					// BG/スプライトCPU/Videoフラグ
		BYTE bgspmod[1024];				// BG/スプライト更新フラグ
		DWORD *bgspbuf;					// BG/スプライトバッファ
		int bgsp_h;						// BG/スプライト水平位置調整
		int bgsp_v;						// BG/スプライト垂直位置調整
		BOOL bgsp_lowres;				// BG/スプライト15kHz
		DWORD bgsp_vres;				// BG/スプライト垂直解像度
		int bgsp_mixmode;				// BG/スプライト合成モード(0:ノインタレース, 1:インタレース，2:2度読み)
		int bgsp_rshift;				// BG/スプライトラスター算出シフト量(右)
		int bgsp_lshift;				// BG/スプライトラスター算出シフト量(左)
		BOOL bgspdirty;					// BG/スプライトダーティーフラグ
		DWORD zero;						// スクロールダミー(0)
	} render_t;

public:
	// 基本ファンクション
	explicit Render(VM* p);
										///< コンストラクタ
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

	// 外部API(コントロール)
	void FASTCALL EnableAct(BOOL enable){ render.enable = enable; }
										// 合成許可
	BOOL FASTCALL IsActive() const		{ return render.act; }
										// アクティブか
	BOOL FASTCALL IsReady() const		{ return (BOOL)(render.count > 0); }
										// 描画レディ状況取得
	void FASTCALL Complete()			{ render.count = 0; }
										// 描画完了
	void FASTCALL StartFrame();
										// フレーム開始(V-DISP)
	void FASTCALL EndFrame();
										// フレーム終了(V-BLANK)
	void FASTCALL HSync(int raster, int xoffset);
	void FASTCALL HSync(int raster)		{ HSync(raster, 0); }
										// 水平同期(rasterまで終わり)
	void FASTCALL SetMixBuf(DWORD *buf, int width, int height);
										// 合成バッファ指定
	void FASTCALL UpdateMixBuf();
										// 合成バッファを強制更新
	render_t* FASTCALL GetWorkAddr() 	{ return &render; }
	const render_t* FASTCALL GetWorkAddr() const { return &render; }
	void FASTCALL SetRenderTarget(void*) {}
										// MFC render target compatibility hook
										// ワークアドレス取得
#if XM6_RENDER_SYNC == 2
	void FASTCALL SetScheduler(class CScheduler* pScheduler) { m_pScheduler = pScheduler; }
										///< スケジューラ接続
#endif	// XM6_RENDER_SYNC == 2

	// 外部API(画面)
	void FASTCALL SetCRTC();
										// CRTCセット
	void FASTCALL SetVC();
										// VCセット
	void FASTCALL SetContrast(int cont, BOOL immediate = FALSE);
										// コントラスト設定
	int FASTCALL GetContrast() const;
	void FASTCALL SetTransparencyEnabled(BOOL enabled)	{ transparency_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsTransparencyEnabled() const		{ return transparency_enabled; }
	void FASTCALL SetOriginalBG0RenderEnabled(BOOL enabled)	{ original_bg0_render_enabled = enabled ? TRUE : FALSE; }
	BOOL FASTCALL IsOriginalBG0RenderEnabled() const		{ return original_bg0_render_enabled; }
	BOOL FASTCALL SetCompositorMode(int mode);
	int FASTCALL GetCompositorMode() const		{ return compositor_mode; }
	DWORD FASTCALL GetFastFallbackCount() const	{ return 0; }
	void FASTCALL GetFastVerticalProbeSnapshot(fast_vertical_probe_snapshot_t *out) const;
	BOOL FASTCALL SetRenderFastDummyEnabled(BOOL enable);
	BOOL FASTCALL IsRenderFastDummyEnabled() const	{ return render_fast_dummy_enabled; }
	BOOL FASTCALL EnsurePx68kFrame();
	BOOL FASTCALL GetPx68kScreen(const WORD **out_pixels, int *out_width, int *out_height, int *out_stride) const;
	const Px68kCrtcHost* FASTCALL GetPx68kCrtcHost() const;
	void FASTCALL CachePx68kStateView(const Px68kCrtcStateView *view);
	void FASTCALL ForceRecompose();
										// コントラスト取得
	void FASTCALL SetPalette(int index);
										// パレット設定
	const DWORD* FASTCALL GetPalette() const;
										// パレットバッファ取得
	void FASTCALL TextMem(DWORD addr);
										// テキストVRAM変更
	void FASTCALL TextScrl(DWORD x, DWORD y);
										// テキストスクロール変更
	void FASTCALL TextCopy(DWORD src, DWORD dst, DWORD plane);
										// ラスタコピー
	void FASTCALL GrpMem(DWORD addr, DWORD block);
										// グラフィックVRAM変更
	void FASTCALL GrpAll(DWORD line, DWORD block);
										// グラフィックVRAM変更
	void FASTCALL GrpScrl(int block, DWORD x, DWORD y);
										// グラフィックスクロールセット
	void FASTCALL SpriteReg(DWORD addr, DWORD data[]);
										// スプライトレジスタ変更
	void FASTCALL BGScrl(int page, DWORD x, DWORD y);
										// BGスクロール変更
	void FASTCALL BGCtrl(int index, BOOL flag);
										// BGコントロール変更
	void FASTCALL BGMem(DWORD addr, WORD data);
										// BG変更
	void FASTCALL PCGMem(DWORD addr);
	void FASTCALL SpriteBGWrite(DWORD addr, BYTE data);
	BYTE FASTCALL TVRAMRead(DWORD addr);
	void FASTCALL TVRAMWrite(DWORD addr, BYTE data);
	BYTE FASTCALL GVRAMRead(DWORD addr);
	void FASTCALL GVRAMWrite(DWORD addr, BYTE data);
	BYTE FASTCALL BGRead(DWORD addr);
	void FASTCALL CRTCRegWrite(DWORD addr, BYTE data);
	BYTE FASTCALL CRTCRegRead(DWORD addr);
	BYTE FASTCALL VCtrlRead(DWORD addr);
	void FASTCALL VCtrlWrite(DWORD addr, BYTE data);
	void FASTCALL GVRAMFastClear();
										// PCG変更
	const DWORD* FASTCALL GetTextBuf() const;
										// テキストバッファ取得
	const DWORD* FASTCALL GetGrpBuf(int index) const;
										// グラフィックバッファ取得
	const DWORD* FASTCALL GetPCGBuf() const;
										// PCGバッファ取得
	const DWORD* FASTCALL GetBGSpBuf() const;
										// BG/スプライトバッファ取得
	const DWORD* FASTCALL GetMixBuf() const;
	const CRTC* FASTCALL GetCRTCDevice() const { return crtc; }
	const VC* FASTCALL GetVCDevice() const { return vc; }
	const TVRAM* FASTCALL GetTVRAMDevice() const;
	const GVRAM* FASTCALL GetGVRAMDevice() const;
	const Sprite* FASTCALL GetSpriteDevice() const { return sprite; }
										// 合成バッファ取得

private:
	void FASTCALL Process(int raster, int xoffset);
										// レンダリング
	void FASTCALL Crtc();
										// CRTC処理
	static const DWORD HDispTable[16];
										// ラスターずれ検証テーブル
	void FASTCALL Video();
										// VC処理
	void FASTCALL Contrast();
										// コントラスト処理
	void FASTCALL Palette();
										// パレット処理
	void FASTCALL MakePalette();
										// パレット作成
	DWORD FASTCALL ConvPalette(int color, int ratio);
										// 色変換
	BOOL FASTCALL TextConv(int offset);
										// テキスト変換
	void FASTCALL Text(int raster);
										// テキスト
	void FASTCALL GrpScrlCheck();
										// グラフィックスクロールチェック処理
	void FASTCALL GrpDispCheck();
										// グラフィック表示構成チェック処理
	BYTE* FASTCALL MixGVRAM(BYTE *buf, int gd, int offset);
										// GVRAMバッファ合成
	void FASTCALL Grp(int gd, int raster);
										// グラフィック
	void FASTCALL SpriteReset();
										// スプライトリセット
	void FASTCALL BGSprite(int raster);
										// BG/スプライト
	void FASTCALL BG(int page, int raster, DWORD *buf, BOOL force);
										// BG
	void FASTCALL BGBlock(int page, int y);
										// BG(横ブロック)
	void FASTCALL BGSpriteCheck(int raster);
										// BG/スプライトチェック
	void FASTCALL Mix(int raster, int xoffset);
										// 合成
	DWORD* FASTCALL MixGrp(DWORD *buf, int raster, int xoffset, int mixlen);
										// 合成(グラフィック)
	CRTC *crtc;
										// CRTC
	const CRTC::crtc_t *cp;
										// CRTC ワークアドレス
	VC *vc;
										// VC
	const VC::vc_t *vp;
										// VC ワークアドレス
	Sprite *sprite;
	Px68kRenderAdapter *px68k_adapter;
	Px68kCrtcHost px68k_crtc_host;
	Px68kCrtcStateView px68k_crtc_state_cache;
	BOOL render_fast_dummy_enabled;
	BOOL transparency_enabled;
	BOOL original_bg0_render_enabled;
	int compositor_mode;
										// スプライト
#if XM6_RENDER_SYNC == 2
	class CScheduler* m_pScheduler;
										///< スケジューラ
#endif	// XM6_RENDER_SYNC == 2
	render_t render;
										// 内部データ
};

#endif	// render_h
