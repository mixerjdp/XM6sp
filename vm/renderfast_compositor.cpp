#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "crtc.h"
#include "vc.h"
#include "sprite.h"
#include "rend_soft.h"
#include "render.h"

#include <cstring>

#define REND_COLOR0 0x80000000u
#define REND_PX68K_IBIT 0x40000000u
enum {
	k_fast_line_margin = 16,
	k_fast_line_capacity = 1024 + k_fast_line_margin
};


static int FASTCALL CalcBGHAdjustPixels(int compositor_mode, const CRTC *crtc, const Sprite *sprite)
{
	if (compositor_mode != Render::compositor_fast) {
		return 0;
	}

	if (!sprite || !crtc) {
		return 0;
	}

	Sprite::sprite_t spr;
	sprite->GetSprite(&spr);

	const CRTC::crtc_t *const crtc_state = crtc->GetWorkAddr();
	if (!crtc_state) {
		return 0;
	}

	const int bg_hdisp = (int)(spr.h_disp & 0xff);
	const int crtc_hstart = (int)(crtc_state->reg[0x04] & 0xff);
	return (bg_hdisp - (crtc_hstart + 4)) * 8;
}

namespace {

static const DWORD k_render_color0 = 0x80000000u;
static const DWORD k_px68k_ibit_marker = 0x40000000u;

static int FASTCALL FastGetVerticalAdjust(const Render::render_t *work, const CRTC::crtc_t *c)
{
	if (!work || !c) {
		return 0;
	}

	if ((work->width != 512) || (work->height != 512) ||
		(work->h_mul != 1) || (work->v_mul != 1) || work->lowres) {
		return 0;
	}

	if (work->grptype != 2) {
		return 0;
	}

	const int vstart = (int)((((DWORD)c->reg[0x0c] << 8) + c->reg[0x0d]) & 0x3ff);
	const int scroll_y = (int)(work->grpy[0] & 0x3ff);
	const int divisor = ((c->reg[0x29] & 0x1c) == 0x1c) ? 1 : 2;
	const int adjust = (scroll_y - vstart) / divisor;

	return -adjust;
}

static BOOL FASTCALL FastNeedsDeflektorVerticalSplit(const Render::render_t *work, const VC::vc_t *v, const CRTC::crtc_t *c)
{
	if (!work || !v || !c) {
		return FALSE;
	}

	// Deflektor uses a mixed 512-line graphic page plus 256-line sprite timing
	// in Render Fast; keep this split narrow so other games stay on the normal path.
	if ((work->width != 512) || (work->height != 512) ||
		(work->h_mul != 1) || (work->v_mul != 1) || work->lowres) {
		return FALSE;
	}

	if ((work->grptype != 2) || (work->mixpage != 1) || (work->mixtype != 7)) {
		return FALSE;
	}

	if (!work->bgspflag) {
		return FALSE;
	}

	if (c->reg[0x29] != 0x01) {
		return FALSE;
	}

	if (!((v->vr2l == 0x73) || (v->vr2l == 0x7C))) {
		return FALSE;
	}

	return TRUE;
}

static inline WORD pack_rgb565i(DWORD pixel)
{
	if (pixel & k_render_color0) {
		return 0;
	}

	const DWORD rgb = pixel & 0x00ffffffu;
	const DWORD r8 = (rgb >> 16) & 0xffu;
	const DWORD g8 = (rgb >> 8) & 0xffu;
	const DWORD b8 = (rgb >> 0) & 0xffu;
	const WORD r5 = (WORD)(r8 >> 3);
	const WORD g5 = (WORD)(g8 >> 3);
	const WORD b5 = (WORD)(b8 >> 3);
	WORD out = (WORD)((r5 << 11) | (g5 << 6) | (b5 << 0));
	if (pixel & k_px68k_ibit_marker) {
		out |= (WORD)0x0020;
	}
	return out;
}

static inline DWORD unpack_rgb565i(WORD pixel)
{
	const DWORD r5 = (DWORD)((pixel >> 11) & 0x1f);
	const DWORD g5 = (DWORD)((pixel >> 6) & 0x1f);
	const DWORD b5 = (DWORD)((pixel >> 0) & 0x1f);
	const DWORD i1 = (DWORD)((pixel & 0x0020) ? 1u : 0u);
	const DWORD g6 = (DWORD)((g5 << 1) | i1);

	const DWORD r8 = (DWORD)((r5 << 3) | (r5 >> 2));
	const DWORD g8 = (DWORD)((g6 << 2) | (g6 >> 4));
	const DWORD b8 = (DWORD)((b5 << 3) | (b5 >> 2));

	DWORD out = (DWORD)((r8 << 16) | (g8 << 8) | (b8 << 0));
	if (pixel & 0x0020) {
		out |= k_px68k_ibit_marker;
	}
	return out;
}

static inline WORD blend_tr_half(WORD base, WORD top)
{
	// px68k TR half-color mixing:
	// w = top; v = base;
	// w &= Pal_HalfMask; if (v & Ibit) w += Pal_Ix2;
	// v &= Pal_HalfMask; out = (v + w) >> 1;
	static const WORD half_mask = (WORD)0xF79E;
	static const WORD ibit = (WORD)0x0020;
	static const WORD ix2 = (WORD)0x0040;

	WORD w = (WORD)(top & half_mask);
	if (base & ibit) {
		w = (WORD)(w + ix2);
	}
	const WORD v = (WORD)(base & half_mask);
	return (WORD)((v + w) >> 1);
}

static inline WORD dim_half_fill(WORD pixel)
{
	static const WORD half_mask = (WORD)0xF79E;
	return (WORD)((pixel & half_mask) >> 1);
}

static void draw_layer_opaq_or_overlay(WORD *dst, const WORD *src, int len, BOOL opaq)
{
	if (opaq) {
		std::memcpy(dst, src, (size_t)len * sizeof(WORD));
		return;
	}
	for (int i = 0; i < len; ++i) {
		const WORD w = src[i];
		if (w != 0) {
			dst[i] = w;
		}
	}
}

static void draw_bg_or_text_layer(WORD *dst, const WORD *line, const BYTE *tr_flag, int len, BOOL opaq, BOOL td, BYTE mask)
{
	if (opaq) {
		std::memcpy(dst, line, (size_t)len * sizeof(WORD));
		return;
	}
	if (td) {
		for (int i = 0; i < len; ++i) {
			if ((tr_flag[i] & mask) && line[i] != 0) {
				dst[i] = line[i];
			}
		}
		return;
	}
	for (int i = 0; i < len; ++i) {
		if (line[i] != 0) {
			dst[i] = line[i];
		}
	}
}

static void draw_text_line_tr(WORD *dst, const WORD *line, const BYTE *tr_flag, const WORD *grp_sp, int len, BOOL opaq)
{
	const BYTE text_mask = 1;

	if (opaq) {
		for (int i = 0; i < len; ++i) {
			WORD v = line[i];
			const WORD w = grp_sp[i];
			if (w != 0) {
				v = blend_tr_half(v, w);
			} else {
				if (!(tr_flag[i] & text_mask)) {
					v = 0;
				}
			}
			dst[i] = v;
		}
		return;
	}

	for (int i = 0; i < len; ++i) {
		if (!(tr_flag[i] & text_mask)) {
			continue;
		}
		WORD v = line[i];
		if (v == 0) {
			continue;
		}
		const WORD w = grp_sp[i];
		if (w != 0) {
			v = blend_tr_half(v, w);
		}
		dst[i] = v;
	}
}

static void draw_bg_line_tr(WORD *dst, const WORD *line, const BYTE *tr_flag, const WORD *grp_sp, int len, BOOL opaq)
{
	const BYTE bg_mask = 2;

	if (opaq) {
		for (int i = 0; i < len; ++i) {
			WORD v = line[i];
			const WORD w = grp_sp[i];
			if (w != 0) {
				v = blend_tr_half(v, w);
			}
			dst[i] = v;
		}
		return;
	}

	for (int i = 0; i < len; ++i) {
		if (!(tr_flag[i] & bg_mask)) {
			continue;
		}
		WORD v = line[i];
		if (v == 0) {
			continue;
		}
		const WORD w = grp_sp[i];
		if (w != 0) {
			v = blend_tr_half(v, w);
		}
		dst[i] = v;
	}
}

static void draw_pri_line(WORD *dst, const WORD *grp_sp, int len)
{
	for (int i = 0; i < len; ++i) {
		const WORD w = grp_sp[i];
		if (w != 0) {
			dst[i] = w;
		}
	}
}

static void draw_half_fill_line(WORD *dst, const WORD *grp_sp, int len)
{
	for (int i = 0; i < len; ++i) {
		if (dst[i] == 0 && grp_sp[i] != 0) {
			dst[i] = dim_half_fill(grp_sp[i]);
		}
	}
}

} // namespace

void FASTCALL Xm6Px68kComposeScanlineFast(DWORD *out_argb32,
                                         int len,
                                         const DWORD *grp,
                                         const DWORD *grp_sp,
                                         const DWORD *grp_sp2,
                                         const DWORD *bgtext_line,
                                         const BYTE *tr_flag,
                                         BOOL gon,
                                         BOOL tron,
                                         BOOL pron,
                                         BOOL ton,
                                         BOOL bgon,
                                         int gr_pri,
                                         int sp_pri,
                                         int tx_pri,
                                          BYTE vr2h,
                                          BOOL transparency_enabled)
{
	if (!transparency_enabled) {
		tron = FALSE;
		pron = FALSE;
	}
	if (!out_argb32 || len <= 0) {
		return;
	}
	if (len > 1024) {
		len = 1024;
	}

	const BOOL tr_mode = (BOOL)((vr2h & 0x5d) == 0x1d);
	const BOOL dim_mode = (BOOL)((vr2h & 0x5d) == 0x1c);
	const BOOL pri_mode = (BOOL)((vr2h & 0x5c) == 0x14);

	WORD dst565[1024];
	WORD grp565[1024];
	WORD grp_sp565[1024];
	WORD grp_sp2_565[1024];
	WORD bgtext565[1024];

	for (int i = 0; i < len; ++i) {
		dst565[i] = 0;
		grp565[i] = grp ? pack_rgb565i(grp[i]) : 0;
		grp_sp565[i] = grp_sp ? pack_rgb565i(grp_sp[i]) : 0;
		grp_sp2_565[i] = grp_sp2 ? pack_rgb565i(grp_sp2[i]) : 0;
		bgtext565[i] = bgtext_line ? pack_rgb565i(bgtext_line[i]) : 0;
	}

	BOOL opaq = TRUE;
	BOOL tdrawed = FALSE;

	// Pri = 2/3 layers.
	if (gr_pri >= 2) {
		if (gon) {
			draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
			opaq = FALSE;
		}
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
			opaq = FALSE;
		}
	}
	if ((sp_pri >= 2) && bgon) {
		if (tr_mode && (gr_pri != 2) && tron) {
			if (gr_pri < tx_pri) {
				draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
			} else {
				draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 2);
			}
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 2);
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri >= 2) && ton) {
		if (tr_mode && (gr_pri != 2) && tron) {
			draw_text_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, tdrawed, 1);
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	// Pri = 1 layers.
	if ((gr_pri == 1) && gon) {
		draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
		opaq = FALSE;
	}
	if ((sp_pri == 1) && bgon) {
		if (tr_mode && (gr_pri == 0) && tron) {
			if (gr_pri < tx_pri) {
				draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
			} else {
				draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri == 2), 2);
			}
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri == 2), 2);
		}
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 1) && tr_mode && (gr_pri != 0) && (sp_pri > gr_pri) && bgon && tron) {
		draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		}
	} else if ((gr_pri == 1) && tron && gon && (vr2h & 0x10)) {
		draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 1) && ton) {
		if (tr_mode && (gr_pri == 0) && tron) {
			draw_text_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		} else {
			draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(sp_pri >= 1), 1);
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	// Pri = 0 layers.
	if ((gr_pri == 0) && gon) {
		draw_layer_opaq_or_overlay(dst565, grp565, len, opaq);
		opaq = FALSE;
	}
	if ((sp_pri == 0) && bgon) {
		draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, (BOOL)(tx_pri >= 1), 2);
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 0) && tr_mode && (sp_pri > gr_pri) && bgon && tron) {
		draw_bg_line_tr(dst565, bgtext565, tr_flag, grp_sp565, len, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		}
	} else if ((gr_pri == 0) && tron && (vr2h & 0x10)) {
		draw_layer_opaq_or_overlay(dst565, grp_sp2_565, len, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 0) && ton) {
		draw_bg_or_text_layer(dst565, bgtext565, tr_flag, len, opaq, TRUE, 1);
		tdrawed = TRUE;
		opaq = FALSE;
	}

	// Special priority / half-fill.
	if (pri_mode && pron) {
		draw_pri_line(dst565, grp_sp565, len);
	} else if (dim_mode && tron) {
		draw_half_fill_line(dst565, grp_sp565, len);
	}

	if (opaq) {
		std::memset(dst565, 0, (size_t)len * sizeof(WORD));
	}

	for (int i = 0; i < len; ++i) {
		const WORD w = dst565[i];
		out_argb32[i] = (w != 0) ? unpack_rgb565i(w) : 0;
	}
}

static BOOL FastVisiblePixel(DWORD pixel);

void FASTCALL Render::FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active)
{
	int n;
	int i;
	DWORD *reg;
	DWORD **ptr;
	int x;
	int dx;
	DWORD pixel;
	const DWORD sprite_mask = 0x4000;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);
	int sprite_limit = render.mixlen + k_fast_line_margin;

	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += (raster & 0x1ff);
	if (sprite_limit > k_fast_line_capacity) {
		sprite_limit = k_fast_line_capacity;
	}
	for (n=127; n>=0; n--) {
		if (render.spuse[n] && ((int)(reg[3] & 3) == pri) && *ptr) {
			DWORD pcgno;
			pcgno = reg[2] & 0xfff;
			if (!render.pcgready[pcgno]) {
				ASSERT(render.pcguse[pcgno] > 0);
				render.pcgready[pcgno] = TRUE;
				RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
			}
			const DWORD *line = *ptr;
			x = ((int)reg[0] + bg_hadjust) & 0x3ff;
			for (i=0; i<16; i++) {
				dx = x - 16 + ((reg[2] & sprite_mask) ? (15 - i) : i);
				if ((dx < 0) || (dx >= sprite_limit)) {
					continue;
				}
				pixel = line[i];
				if (!FastVisiblePixel(pixel)) {
					continue;
				}
				if (bg_pri[dx] >= (WORD)(n * 8)) {
					bg_line[dx] = pixel;
					bg_flag[dx] |= 2;
					bg_pri[dx] = (WORD)(n * 8);
					*active = TRUE;
				}
			}
		}
		reg -= 4;
		ptr -= 512;
	}
}

void FASTCALL Render::FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active)
{
	int yblk;
	int line;
	DWORD **ptr;
	int x;
	int i;
	int gx;
	int tile;
	int off;
	DWORD **ent;
	DWORD *src;
	DWORD bgdata;
	DWORD pixel;
	DWORD pcgno;
	int bank;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	(void)bg_pri;
	if (!render.bgdisp[page]) {
		return;
	}
	if ((page == 1) && render.bgsize) {
		return;
	}

	yblk = (int)render.bgy[page] + (raster & 0x1ff);
	if (render.bgsize) {
		yblk &= 0x3ff;
		yblk >>= 4;
	}
	else {
		yblk &= 0x1ff;
		yblk >>= 3;
	}
	if (render.bgall[page][yblk]) {
		render.bgall[page][yblk] = FALSE;
		BGBlock(page, yblk);
	}

	line = (int)render.bgy[page] + (raster & 0x1ff);
	if (render.bgsize) {
		line &= 0x3ff;
		x = (int)((render.bgx[page] - bg_hadjust) & 0x3ff);
	}
	else {
		line &= 0x1ff;
		x = (int)((render.bgx[page] - bg_hadjust) & 0x1ff);
	}
	ptr = render.bgptr[page] + (line << 7);

	for (i=0; i<render.mixlen; i++) {
		if (render.bgsize) {
			gx = (x + i) & 0x3ff;
			tile = gx >> 4;
			off = gx & 15;
		}
		else {
			gx = (x + i) & 0x1ff;
			tile = gx >> 3;
			off = gx & 7;
		}
		ent = &ptr[tile << 1];
		src = ent[0];
		bgdata = (DWORD)(size_t)ent[1];
		if (!src) {
			continue;
		}
		pcgno = bgdata & 0x0fff;
		if (!render.pcgready[pcgno]) {
			ASSERT(render.pcguse[pcgno] > 0);
			render.pcgready[pcgno] = TRUE;
			RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
		}
		if (bgdata & 0x4000) {
			pixel = src[render.bgsize ? (15 - off) : (7 - off)];
		}
		else {
			pixel = src[off];
		}

		if (FastVisiblePixel(pixel)) {
			bg_line[i] = pixel;
			bg_flag[i] |= 2;
			*active = TRUE;
			continue;
		}
		if (!gd) {
			continue;
		}
		bank = (int)((bgdata >> 8) & 0x0f);
		if (bank == 0) {
			continue;
		}
		if (bg_flag[i] & 2) {
			continue;
		}

		pixel &= ~k_render_color0;
		bg_line[i] = pixel;
		bg_flag[i] |= 2;
		*active = TRUE;
	}
}

void FASTCALL Render::FastBuildBGLinePX(int sprite_raster, int bg_raster, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active, BOOL *bg_opaq)
{
	int i;
	const VC::vc_t *p;
	const CRTC::crtc_t *c;
	BOOL deflektor_split;
	int sprite_line;
	int bg_line_raster;
	const BOOL sprite_visible = sprite->IsDisplay();
	const BOOL has_bg = (BOOL)(render.bgdisp[0] || (render.bgdisp[1] && !render.bgsize));
	const BOOL bgsp_on = render.bgspflag;
	const BOOL gd = (BOOL)(sp_pri >= tx_pri);
	const BOOL opaq = gd ? TRUE : (BOOL)!ton;
	const DWORD base = (render.paldata[0x100] & ~k_render_color0);
	const BOOL has_sources = (BOOL)(sprite_visible || has_bg);

	p = vc->GetWorkAddr();
	c = crtc->GetWorkAddr();
	deflektor_split = FastNeedsDeflektorVerticalSplit(&render, p, c);
	if (deflektor_split) {
		sprite_line = sprite_raster & 0x1ff;
		bg_line_raster = bg_raster & 0x1ff;
	}
	else {
		sprite_line = bg_line_raster = (bg_raster & 0x1ff);
	}

	for (i=0; i<render.mixlen; i++) {
		bg_line[i] = opaq ? base : k_render_color0;
		bg_flag[i] = 0;
		bg_pri[i] = 0xffff;
	}
	*active = FALSE;
	if (bg_opaq) {
		*bg_opaq = FALSE;
	}
	if (!bgsp_on || !has_sources || (render.mixlen > 512)) {
		return;
	}

	*active = has_sources;
	if (sprite_visible) {
		FastDrawSpriteLinePX(sprite_line, 1, bg_line, bg_flag, bg_pri, active);
	}
	if (render.bgdisp[1] && !render.bgsize) {
		FastDrawBGPageLinePX(1, bg_line_raster, gd, bg_line, bg_flag, bg_pri, active);
	}
	if (sprite_visible) {
		FastDrawSpriteLinePX(sprite_line, 2, bg_line, bg_flag, bg_pri, active);
	}
	if (render.bgdisp[0]) {
		FastDrawBGPageLinePX(0, bg_line_raster, gd, bg_line, bg_flag, bg_pri, active);
	}
	if (sprite_visible) {
		FastDrawSpriteLinePX(sprite_line, 3, bg_line, bg_flag, bg_pri, active);
	}

	if (bg_opaq) {
		*bg_opaq = opaq;
	}
}

static BOOL FastVisiblePixel(DWORD pixel)
{
	return (BOOL)((pixel & 0x80000000) == 0);
}

static inline WORD Px68kPack565I(DWORD pixel)
{
	const DWORD rgb = pixel & 0x00ffffff;
	const DWORD r8 = (rgb >> 16) & 0xff;
	const DWORD g8 = (rgb >> 8) & 0xff;
	const DWORD b8 = (rgb >> 0) & 0xff;
	const WORD r5 = (WORD)(r8 >> 3);
	const WORD g5 = (WORD)(g8 >> 3);
	const WORD b5 = (WORD)(b8 >> 3);
	WORD out = (WORD)((r5 << 11) | (g5 << 6) | (b5 << 0));
	if (pixel & k_px68k_ibit_marker) {
		out |= 0x0020;
	}
	return out;
}

static inline DWORD Px68kUnpack565I(WORD pixel)
{
	const DWORD r5 = (DWORD)((pixel >> 11) & 0x1f);
	const DWORD g5 = (DWORD)((pixel >> 6) & 0x1f);
	const DWORD b5 = (DWORD)((pixel >> 0) & 0x1f);
	const DWORD i1 = (DWORD)((pixel & 0x0020) ? 1u : 0u);
	const DWORD g6 = (DWORD)((g5 << 1) | i1);

	const DWORD r8 = (DWORD)((r5 << 3) | (r5 >> 2));
	const DWORD g8 = (DWORD)((g6 << 2) | (g6 >> 4));
	const DWORD b8 = (DWORD)((b5 << 3) | (b5 >> 2));
	DWORD out = (DWORD)((r8 << 16) | (g8 << 8) | (b8 << 0));
	if (pixel & 0x0020) {
		out |= k_px68k_ibit_marker;
	}
	return out;
}

static DWORD FastBlendPixel(DWORD base, DWORD top)
{
	// px68k TR half-color mixing (see palette.c/gvram.c/windraw.c):
	// w = top; v = base; if (v has Ibit) then w += Pal_Ix2; out = (v + w) >> 1.
	const WORD w0 = Px68kPack565I(top);
	const WORD v0 = Px68kPack565I(base);
	const WORD half_mask = (WORD)0xF79E;
	const WORD ibit = (WORD)0x0020;
	const WORD ix2 = (WORD)0x0040;

	WORD w = (WORD)(w0 & half_mask);
	if (v0 & ibit) {
		w = (WORD)(w + ix2);
	}
	const WORD v = (WORD)(v0 & half_mask);
	const WORD out = (WORD)((v + w) >> 1);
	return Px68kUnpack565I(out);
}

static void FastClearLine(DWORD *buf, int len)
{
	int i;
	for (i=0; i<len; i++) {
		buf[i] = k_render_color0;
	}
}

static void FastBuildLine64K(const Render::render_t *work, int y, int len,
	DWORD *dst, BOOL *active)
{
	DWORD x;
	DWORD off;
	WORD raw;
	DWORD pixel;
	int i;

	x = work->grpx[0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[0]) & 0x1ff) << 10) + (x << 1);
	for (i=0; i<len; i++) {
		raw = *(WORD *)(&work->grpgv[off]);
		pixel = work->palptr[(DWORD)(raw & 0xfffe)];
		if ((raw & 0xfffe) == 0) {
			pixel |= k_render_color0;
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine1024(const Render::render_t *work, int y, int len,
	DWORD *dst, BOOL *active)
{
	const WORD *src;
	DWORD line;
	DWORD x;
	WORD raw;
	int bits;
	int remain;
	int index;
	int i;

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
		dst[i] = work->paldata[index];
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
		if (--remain == 0) {
			src -= 0x200;
			bits ^= 4;
			remain = 512;
		}
	}
}

static void FastBuildLine4(const Render::render_t *work, int page, int y, int len,
	DWORD *dst, BOOL opaq, BOOL *active)
{
	DWORD off;
	DWORD x;
	DWORD pixel;
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
		if (index != 0) {
			pixel = work->paldata[index];
			if (opaq || FastVisiblePixel(pixel)) {
				dst[i] = pixel;
			}
		}
		else if (opaq) {
			dst[i] = k_render_color0;
		}
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildLine4TR(const Render::render_t *work, int page, int y, int len,
	DWORD *dst, const DWORD *grp_sp, BOOL opaq, BOOL *active)
{
	DWORD off;
	DWORD x;
	DWORD pixel;
	DWORD base;
	int i;
	BYTE data;
	int index;

	page &= 3;
	x = work->grpx[page] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[page]) & 0x1ff) << 10) + (x << 1);
	if (page >= 2) {
		off++;
	}
	for (i=0; i<len; i++) {
		pixel = opaq ? k_render_color0 : dst[i];
		data = work->grpgv[off];
		index = (page & 1) ? ((data >> 4) & 0x0f) : (data & 0x0f);
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
		else if (opaq) {
			pixel = k_render_color0;
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
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
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
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
	DWORD x;
	DWORD off;
	DWORD pixel;
	int i;
	int index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (i=0; i<len; i++) {
		pixel = opaq ? k_render_color0 : dst[i];
		index = work->grpgv[off];
		if (index != 0) {
			pixel = work->paldata[index];
			if (!opaq && !FastVisiblePixel(pixel)) {
				pixel = dst[i];
			}
		}
		dst[i] = pixel;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
	}
}

static void FastBuildSpecial8(const Render::render_t *work, int pair, int y, int len,
	DWORD *grp_sp, DWORD *grp_sp2, BOOL *grp_sp_tr, BOOL *active)
{
	DWORD x;
	DWORD off;
	DWORD pixel;
	int i;
	int index;
	int even_index;

	x = work->grpx[pair * 2 + 0] & 0x1ff;
	off = ((DWORD)((y + (int)work->grpy[pair * 2 + 0]) & 0x1ff) << 10) + (x << 1) + pair;
	for (i=0; i<len; i++) {
		index = work->grpgv[off];
		even_index = (index & 0xfe);
		grp_sp_tr[i] = FALSE;
		if (even_index != 0) {
			pixel = work->paldata[even_index];
		}
		else {
			pixel = k_render_color0;
		}
		if (index & 1) {
			grp_sp[i] = pixel;
			if (even_index != 0) {
				grp_sp[i] |= k_px68k_ibit_marker;
			}
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
			grp_sp2[i] = pixel;
			if ((even_index != 0) && (Px68kPack565I(pixel) == 0)) {
				grp_sp_tr[i] = TRUE;
				*active = TRUE;
			}
			else if (FastVisiblePixel(grp_sp2[i])) {
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
		pixel = opaq ? 0 : dst[i];
		index = work->grpgv[off];
		if (FastVisiblePixel(grp_sp[i])) {
			if (index != 0) {
				base = work->paldata[index];
				if (Px68kPack565I(base) != 0) {
					pixel = FastBlendPixel(base, grp_sp[i]);
				}
				else {
					pixel = 0;
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
		if (x == 0) {
			off -= 0x400;
		}
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
		pixel = opaq ? 0 : dst[i];
		index = work->grpgv[off];
		if (FastVisiblePixel(grp_sp[i]) || grp_sp_tr[i]) {
			pixel = 0;
		}
		else if (index != 0) {
			pixel = work->paldata[index];
		}
		dst[i] = pixel;
		grp_sp_tr[i] = FALSE;
		if (FastVisiblePixel(pixel)) {
			*active = TRUE;
		}
		off += 2;
		x = (x + 1) & 0x1ff;
		if (x == 0) {
			off -= 0x400;
		}
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
			pixel |= k_render_color0;
		}
		if (raw & 1) {
			grp_sp[i] = pixel;
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
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
			grp_sp2[i] = k_render_color0;
			if (FastVisiblePixel(grp_sp[i])) {
				*active = TRUE;
			}
		}
		else {
			grp_sp[i] = k_render_color0;
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
	const CRTC::crtc_t *c;
	BOOL active;
	BOOL pair0_first;
	BOOL opaq;
	int page;
	int slot;
	int raster;
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
	c = crtc->GetWorkAddr();
	raster = y & 0x3ff;
	if (c && ((c->reg[0x29] & 0x1c) == 0x1c)) {
		raster = (raster << 1) & 0x3ff;
	}

	FastClearLine(grp, render.mixlen);
	FastClearLine(grp_sp, render.mixlen);
	FastClearLine(grp_sp2, render.mixlen);
	memset(grp_sp_tr, 0, (size_t)render.mixlen * sizeof(BOOL));
	*gon = FALSE;
	*tron = FALSE;
	*pron = FALSE;

	switch (render.grptype) {
		case 0:
			if (render.grpen[0] && p->gon) {
				active = FALSE;
				FastBuildLine1024(&render, raster, render.mixlen, grp, &active);
				*gon = TRUE;
			}
			if (render.grpen[0] && p->exon && p->bp && p->gon) {
				active = FALSE;
				FastBuildSpecial1024(&render, raster, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = TRUE;
				*pron = TRUE;
			}
			return;

		case 1:
			if (p->exon && p->gs[0]) {
				page = (int)(p->gp[0] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildSpecial4(&render, page, raster, render.mixlen, grp_sp, grp_sp2, &active);
					*tron = TRUE;
					*pron = TRUE;
				}
			}

			opaq = TRUE;
			if (p->gs[3]) {
				page = (int)(p->gp[3] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[2]) {
				page = (int)(p->gp[2] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[1]) {
				page = (int)(p->gp[1] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					if (((p->vr2h & 0x1e) == 0x1e) && *tron) {
						FastBuildLine4TR(&render, page, raster, render.mixlen, grp, grp_sp, opaq, &active);
					}
					else {
						FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					}
					*gon = TRUE;
					opaq = FALSE;
				}
			}
			if (p->gs[0] && ((p->vr2h & 0x14) != 0x14)) {
				page = (int)(p->gp[0] & 3);
				if (render.grpen[page]) {
					active = FALSE;
					FastBuildLine4(&render, page, raster, render.mixlen, grp, opaq, &active);
					*gon = TRUE;
				}
			}
			return;

		case 2:
			pair0_first = (BOOL)(((int)p->gp[0]) <= ((int)p->gp[2]));
			top_pair = pair0_first ? 0 : 1;
			other_pair = pair0_first ? 1 : 0;

			opaq = TRUE;

			if (p->exon && p->gs[0]) {
				active = FALSE;
				FastBuildSpecial8(&render, top_pair, raster, render.mixlen, grp_sp, grp_sp2, grp_sp_tr, &active);
				*tron = TRUE;
				*pron = TRUE;
			}

			if (render.grpen[other_pair * 2]) {
				active = FALSE;
				if (((p->vr2h & 0x1e) == 0x1e) && *tron) {
					FastBuildLine8TR(&render, other_pair, raster, render.mixlen, grp, grp_sp, opaq, &active);
				}
				else if (((p->vr2h & 0x1d) == 0x1d) && *tron) {
					FastBuildLine8TRGT(&render, other_pair, raster, render.mixlen, grp, grp_sp, grp_sp_tr, opaq, &active);
				}
				else {
					FastBuildLine8(&render, other_pair, raster, render.mixlen, grp, opaq, &active);
				}
				*gon = TRUE;
				opaq = FALSE;
			}
			if (render.grpen[top_pair * 2] && ((p->vr2h & 0x14) != 0x14)) {
				active = FALSE;
				FastBuildLine8(&render, top_pair, raster, render.mixlen, grp, opaq, &active);
				*gon = TRUE;
			}
			return;

		case 3:
		case 4:
			if (render.grpen[0] && p->gon) {
				active = FALSE;
				FastBuildLine64K(&render, raster, render.mixlen, grp, &active);
				*gon = TRUE;
			}
			if (render.grpen[0] && p->exon && p->bp && (render.mixpage > 0)) {
				active = FALSE;
				FastBuildSpecial64K(&render, raster, render.mixlen, grp_sp, grp_sp2, &active);
				*tron = TRUE;
				*pron = TRUE;
			}
			return;

		default:
			for (slot=0; slot<render.mixlen; slot++) {
				if (FastVisiblePixel(grp[slot])) {
					*gon = TRUE;
					break;
				}
			}
			return;
	}
}
static void FastPrepareBGTextLine(DWORD *line, BYTE *tr_flag, const BYTE *bg_flag, const DWORD *text_line, const DWORD *bg_line,
	int len, BOOL ton, BOOL bgon, int tx_pri, int sp_pri, BOOL bg_opaq)
{
	int i;

	for (i=0; i<len; i++) {
		line[i] = k_render_color0;
		tr_flag[i] = bg_flag ? (bg_flag[i] & 2) : 0;
	}

	if (sp_pri < tx_pri) {
		if (ton) {
			for (i=0; i<len; i++) {
				line[i] = text_line[i];
				if (FastVisiblePixel(text_line[i])) {
					tr_flag[i] |= 1;
				}
			}
		}
		if (bgon && bg_flag) {
			for (i=0; i<len; i++) {
				if (bg_opaq || (bg_flag[i] & 2)) {
					line[i] = bg_line[i];
				}
			}
		}
		return;
	}

	if (bgon && bg_flag) {
			for (i=0; i<len; i++) {
				if (bg_opaq || (bg_flag[i] & 2)) {
					line[i] = bg_line[i];
				}
			}
		}

	if (ton) {
		if (!bgon) {
			for (i=0; i<len; i++) {
				line[i] = text_line[i];
				if (FastVisiblePixel(text_line[i])) {
					tr_flag[i] |= 1;
				}
			}
		}
		else {
			for (i=0; i<len; i++) {
				if (FastVisiblePixel(text_line[i])) {
					line[i] = text_line[i];
					tr_flag[i] |= 1;
				}
			}
		}
	}
}

void FASTCALL Render::MixFast(int y)
{
	MixFastLine(y, y);
}

// px68k-style scanline compositor (Render Fast 1:1)
void FASTCALL Xm6Px68kComposeScanlineFast(DWORD *out_argb32, int len, const DWORD *grp, const DWORD *grp_sp, const DWORD *grp_sp2, const DWORD *bgtext_line, const BYTE *tr_flag, BOOL gon, BOOL tron, BOOL pron, BOOL ton, BOOL bgon, int gr_pri, int sp_pri, int tx_pri, BYTE vr2h, BOOL transparency_enabled);

void FASTCALL Render::MixFastLine(int dst_y, int src_y)
{
	DWORD *dst;
	DWORD grp[k_fast_line_capacity];
	DWORD grp_sp[k_fast_line_capacity];
	DWORD grp_sp2[k_fast_line_capacity];
	BOOL grp_sp_tr[k_fast_line_capacity];
	DWORD text_line[k_fast_line_capacity];
	DWORD bg_line[k_fast_line_capacity];
	WORD bg_pri[k_fast_line_capacity];
	DWORD bgtext_line[k_fast_line_capacity];
	DWORD out[k_fast_line_capacity];
	BYTE bg_flag[k_fast_line_capacity];
	BYTE tr_flag[k_fast_line_capacity];
	const VC::vc_t *p;
	const CRTC::crtc_t *c;
	int tx_pri;
	int sp_pri;
	int gr_pri;
	BOOL gon;
	BOOL tron;
	BOOL pron;
	BOOL ton;
	BOOL bgon;
	BOOL bg_active;
	BOOL bg_opaq;
	DWORD mix_stamp;
	int i;
	DWORD *const grp_visible = &grp[k_fast_line_margin];
	DWORD *const grp_sp_visible = &grp_sp[k_fast_line_margin];
	DWORD *const grp_sp2_visible = &grp_sp2[k_fast_line_margin];
	BOOL *const grp_sp_tr_visible = &grp_sp_tr[k_fast_line_margin];
	DWORD *const text_line_visible = &text_line[k_fast_line_margin];
	DWORD *const bg_line_visible = &bg_line[k_fast_line_margin];
	WORD *const bg_pri_visible = &bg_pri[k_fast_line_margin];
	DWORD *const bgtext_line_visible = &bgtext_line[k_fast_line_margin];
	DWORD *const out_visible = &out[k_fast_line_margin];
	BYTE *const bg_flag_visible = &bg_flag[k_fast_line_margin];
	BYTE *const tr_flag_visible = &tr_flag[k_fast_line_margin];
	BOOL deflektor_split;
	int v_adjust;
	int sprite_raster;
	int bg_raster;

	if (!render.mixbuf) {
		return;
	}
	if ((dst_y < 0) || (dst_y >= render.mixheight)) {
		return;
	}
	if ((src_y < 0) || (src_y >= 1024)) {
		return;
	}
	ASSERT(render.mixlen > 0);

	mix_stamp = render.fast_mix_stamp[src_y];
	if ((!render.mix[src_y]) && (render.fast_mix_done[src_y] == mix_stamp)) {
		return;
	}
	render.mix[src_y] = FALSE;
	render.fast_mix_done[src_y] = mix_stamp;
	dst = &render.mixbuf[render.mixwidth * dst_y];
	p = vc->GetWorkAddr();
	c = crtc->GetWorkAddr();
	deflektor_split = FastNeedsDeflektorVerticalSplit(&render, p, c);
	if (deflektor_split) {
		// Deflektor's 512x512 menu/game field needs a 512-line BG raster,
		// but sprite timing still follows the 256-line path.
		v_adjust = FastGetVerticalAdjust(&render, c);
		sprite_raster = ((src_y / 2) + v_adjust) & 0x3ff;
		bg_raster = (src_y + v_adjust) & 0x3ff;
	}
	else {
		sprite_raster = src_y;
		bg_raster = src_y;
	}
	tx_pri = (int)(p->tx & 3);
	sp_pri = (int)(p->sp & 3);
	gr_pri = (int)(p->gr & 3);
	ton = render.texten;

	std::memset(grp, 0, sizeof(grp));
	std::memset(grp_sp, 0, sizeof(grp_sp));
	std::memset(grp_sp2, 0, sizeof(grp_sp2));
	std::memset(grp_sp_tr, 0, sizeof(grp_sp_tr));
	std::memset(text_line, 0, sizeof(text_line));
	std::memset(bg_line, 0, sizeof(bg_line));
	std::memset(bg_pri, 0xff, sizeof(bg_pri));
	std::memset(bgtext_line, 0, sizeof(bgtext_line));
	std::memset(out, 0, sizeof(out));
	std::memset(bg_flag, 0, sizeof(bg_flag));
	std::memset(tr_flag, 0, sizeof(tr_flag));

	FastMixGrp(bg_raster, grp_visible, grp_sp_visible, grp_sp2_visible, grp_sp_tr_visible, &gon, &tron, &pron);

	for (i=0; i<render.mixlen; i++) {
		// px68k keeps a 16px guard band before the visible window.
		// We compose into the shifted slice and later copy only the visible region.
		out_visible[i] = 0;
		if (ton) {
			text_line_visible[i] = render.textout[(((bg_raster + (int)render.texty) & 0x3ff) << 10) + (((int)render.textx + i) & 0x3ff)];
		}
		else {
			text_line_visible[i] = k_render_color0;
		}
		bg_line_visible[i] = k_render_color0;
		bg_flag_visible[i] = 0;
	}

	bg_active = FALSE;
	bg_opaq = FALSE;
	FastBuildBGLinePX(sprite_raster, bg_raster, ton, tx_pri, sp_pri, bg_line_visible, bg_flag_visible, bg_pri_visible, &bg_active, &bg_opaq);
	bgon = bg_active;
	FastPrepareBGTextLine(bgtext_line_visible, tr_flag_visible, bg_flag_visible, text_line_visible, bg_line_visible, render.mixlen, ton, bgon, tx_pri, sp_pri, bg_opaq);

	// px68k-style 565+Ibit scanline composition (Render Fast 1:1 path)
	Xm6Px68kComposeScanlineFast(out_visible, render.mixlen,
		grp_visible, grp_sp_visible, grp_sp2_visible,
		bgtext_line_visible, tr_flag_visible,
		gon, tron, pron, ton, bgon,
		gr_pri, sp_pri, tx_pri,
		p->vr2h, transparency_enabled);

	RendMix01(dst, out_visible, render.drawflag + (src_y << 6), render.mixlen);
}

// ---- Fast pipeline methods moved from render.cpp ----

void FASTCALL Render::HSyncFast(int raster)
{
	render.last = raster;
	if (render.act) {
		ProcessFast();
	}
}

void FASTCALL Render::StartFrameFast()
{
	CRTC::crtc_t crtcdata;
	int i;

	ASSERT(this);

	render.count = 0;

	if (!render.enable) {
		render.act = FALSE;
		return;
	}

	render.act = TRUE;
	render.first = 0;
	render.last = -1;

	if (render.crtc) {
		crtc->GetCRTC(&crtcdata);
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		SpriteReset();
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}
		render.crtc = FALSE;
	}

	InvalidateFrame();
}

void FASTCALL Render::EndFrameFast()
{
	ASSERT(this);

	if (!render.act) {
		return;
	}

	if (render.last > 0) {
		render.last = render.height;
		ProcessFast();
	}

	render.count++;
	render.act = FALSE;
}

void FASTCALL Render::SetCRTCFast()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	render.crtc = TRUE;
	render.vc = TRUE;

	if (render.act) {
		from = render.last;
		if (from < 0) {
			from = 0;
		}
		stamp = ++render.fast_stamp_counter;
		for (i=from; i<render.height && i<1024; i++) {
			render.mix[i] = TRUE;
			render.fast_mix_stamp[i] = stamp;
			render.fast_mix_done[i] = 0;
			render.bgspmod[i & 0x1ff] = TRUE;
			render.fast_bg_stamp[i & 0x1ff] = stamp;
			render.fast_bg_done[i & 0x1ff] = 0;
		}
	}
}

void FASTCALL Render::SetVCFast()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	render.vc = TRUE;

	if (render.act) {
		from = render.last;
		if (from < 0) {
			from = 0;
		}
		stamp = ++render.fast_stamp_counter;
		for (i=from; i<render.height && i<1024; i++) {
			render.mix[i] = TRUE;
			render.fast_mix_stamp[i] = stamp;
			render.fast_mix_done[i] = 0;
			render.bgspmod[i & 0x1ff] = TRUE;
			render.fast_bg_stamp[i & 0x1ff] = stamp;
			render.fast_bg_done[i & 0x1ff] = 0;
		}
	}
}

void FASTCALL Render::VideoFastPX68K()
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

	// VC?to?Oo~o
	render.vc = FALSE;

	// ?to?OON
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VC?f?[?^?ACRTC?f?[?^o�E��E�
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// ?e?L?X?g?C?l?[?uo
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// ?Oo?t?B?b?N?^?C?v
	type = 0;
	if (!p->siz) {
		type = (int)(p->col + 1);
		if ((compositor_mode == compositor_fast) && (p->col == 2)) {
			type = 2;
		}
	}
	if (type != render.grptype) {
		render.grptype = type;
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?Oo?t?B?b?N?C?l?[?uo?A?Dox?}?b?v
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
				if (compositor_mode == compositor_fast) {
				// px68k semantics:
				// - Only GS0 and GS2 are meaningful enables in 256-color mode.
				// - Page priority is decided by comparing GP0 vs GP2 (equal => page0 wins).
				// - The two 8-bit pages are represented as blocks 0 and 2 in XM6.
				{
					const BOOL page0_on = (BOOL)p->gs[0];
					const BOOL page2_on = (BOOL)p->gs[2];
					const BOOL page0_top = (BOOL)(((int)p->gp[0]) <= ((int)p->gp[2]));

					if (page0_on) { render.grpen[0] = TRUE; }
					if (page2_on) { render.grpen[2] = TRUE; }

					// Build bottom -> top (later layers overlay earlier ones).
					if (page0_top) {
						if (page2_on) { map[0] = 2; render.mixpage++; }
						if (page0_on) { map[1] = 0; render.mixpage++; }
					}
					else {
						if (page0_on) { map[0] = 0; render.mixpage++; }
						if (page2_on) { map[1] = 2; render.mixpage++; }
					}
				}
				}
				else {
				for (i=0; i<2; i++) {
					// page0 check
					if ((p->gp[i * 2 + 0] == 0) && (p->gp[i * 2 + 1] == 1)) {
						if (p->gs[i * 2 + 0]) {
							map[i] = 0;
							render.grpen[0] = TRUE;
							render.mixpage++;
						}
					}
					// page1 check
					if ((p->gp[i * 2 + 0] == 2) && (p->gp[i * 2 + 1] == 3)) {
						if (p->gs[i * 2 + 0]) {
							map[i] = 2;
							render.grpen[2] = TRUE;
							render.mixpage++;
						}
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

	// ?Oo?t?B?b?N?o?b?t?@oZ?b?g
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

	// ?Doo?�E��E�
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// ?^?C?vooo
	render.mixtype = 0;

	// BG/?X?vo?C?g?\ooo
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/?X?vo?C?g?\oON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/?X?vo?C?g?\oOFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// o?(q->hd >= 2oo?X?vo?C?goo)
	if ((q->hd >= 2) || (!p->son)) {
		// ?X?vo?C?go?
		if (!render.texten) {
			// ?e?L?X?go?
			if (render.mixpage == 0) {
				// ?Oo?t?B?b?No?(type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// ?Oo?t?B?b?N1oo(type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// ?Oo?t?B?b?N2oo(type=2)
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
			// ?Oo?t?B?b?N3ooo(type=4)
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
		// ?e?L?X?goo
		if (render.mixpage == 0) {
			// ?Oo?t?B?b?NooB?e?L?X?go?(type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// ?e?L?X?g+?Oo?t?B?b?N1o
			if (tx < gr) {
				// ?e?L?X?g?Oo(type=3)
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
			// ?Oo?t?B?b?N?Oo(type=3,tx=groOo?t?B?b?N?OoAo?II)
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
		// ?e?L?X?g+?Oo?t?B?b?N2oo(type=5, type=6)
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

	// ?X?vo?C?goo
	if (!render.texten) {
		// ?e?L?X?go?
		if (render.mixpage == 0) {
			// ?Oo?t?B?b?NooA?X?vo?C?go?(type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// ?X?vo?C?g+?Oo?t?B?b?N1o(type=3)
			if (sp < gr) {
				// ?X?vo?C?g?Oo
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
			// ?Oo?t?B?b?N?Oo(sp=groso)
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
		// ?X?vo?C?g+?Oo?t?B?b?N2oo(type=5, type=6)
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

	// ?e?L?X?goo
	if (render.mixpage == 0) {
		// ?Oo?t?B?b?NooB?e?L?X?g?{?X?vo?C?g(type=3)
		if (tx <= sp) {
			// tx=spoe?L?X?g?Oo(LMZ2)
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
		// ?X?vo?C?g?Oo
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

	// ?Dooo?
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// ?Kooooo?
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
		// ?Kooooo?
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
		// ?Kooooo?
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
		// ?e?L?X?g?{?X?vo?C?g?{?Oo?t?B?b?N1o(type=7)
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

	// ?e?L?X?g?{?X?vo?C?g?{?Oo?t?B?b?N?Qoo(type=8)
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
//	?Ro?go?X?go?
//
//---------------------------------------------------------------------------

void FASTCALL Render::PaletteFastPX68K()
{
	DWORD data;
	BOOL tx;
	BOOL gr;
	BOOL sp;
	int i;
	int j;

	// ?to?OOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// ?Oo?t?B?b?N
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?Oo?t?B?b?Noeo?A?to?OOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// oo?Foo?
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536?Fooopo?b?g?f?[?^o?
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// ?e?L?X?go?X?vo?C?g
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?e?L?X?goeo?A?to?OOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// oo?Foo?
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100oBG?E?X?vo?C?go?Ko?eo
				sp = TRUE;
			}

			// PCGoo
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?X?vo?C?g
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// ?X?vo?C?goeo?A?to?OOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// oo?Foo?
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCGoo
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?Oo?t?B?b?N?to?O
	if (gr) {
		// ?to?OON
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?e?L?X?g?to?O
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// ?X?vo?C?g?to?O
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// ?po?b?g?to?OOFF
	render.palette = FALSE;
}

void FASTCALL Render::TextFastPX68K(int raster)
{
	int y;

	ASSERT((raster >= 0) && (raster < 1024));
	ASSERT(render.texttv);
	ASSERT(render.textflag);
	ASSERT(render.textbuf);
	ASSERT(render.palbuf);

	if (!render.texten) {
		return;
	}

	y = (raster + render.texty) & 0x3ff;

	if (render.textmod[y]) {
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		RendTextMem(render.texttv + (y << 7),
				render.textflag + (y << 5),
				render.textbuf + (y << 9));

		RendTextPal(render.textbuf + (y << 9),
				render.textout + (y << 10),
				render.textflag + (y << 5),
				render.paldata + 0x100);
	}

	if (render.textpal[y]) {
		render.textpal[y] = FALSE;

		RendTextAll(render.textbuf + (y << 9),
				render.textout + (y << 10),
				render.paldata + 0x100);
		render.mix[raster] = TRUE;

		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

void FASTCALL Render::ProcessFast()
{
	int i;
	int src;
	DWORD stamp;
	BOOL sprite_disp;

	// ?sooooooos?v
	if (render.first >= render.last) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?r?f?I????");
#endif	// RENDER_LOG
		VideoFastPX68K();
	}

	// ?Ro?go?X?g
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// ?po?b?g
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		PaletteFastPX68K();
	}

	// first==0oA?X?vo?C?go\oON/OFFoo?
	// Sprite display state can toggle mid-frame
	sprite_disp = sprite->IsDisplay();
	if (sprite_disp != render.bgspdisp) {
		stamp = ++render.fast_stamp_counter;
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
			render.fast_bg_stamp[i] = stamp;
			render.fast_bg_done[i] = 0;
		}
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
			render.fast_mix_stamp[i] = stamp;
			render.fast_mix_done[i] = 0;
		}
		render.bgspdisp = sprite_disp;
	}

	if ((render.v_mul == 2) && !render.lowres) {
		// I/Ooog?�E��E�ooA?cooooooooo
		for (i=render.first; i<render.last; i++) {
			if ((i & 1) == 0) {
				src = (i >> 1);
				stamp = ++render.fast_stamp_counter;
				render.fast_mix_stamp[src] = stamp;
				render.fast_bg_stamp[src & 0x1ff] = stamp;
				render.mix[src] = TRUE;
				render.bgspmod[src & 0x1ff] = TRUE;
				TextFastPX68K(src);
				MixFastLine(src, src);
			}
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// ?Co?^o?[?Xo?
	if ((render.v_mul == 0) && render.lowres) {
		// ooEooooo(o?@ooo)
		for (i=render.first; i<render.last; i++) {
			// ?e?L?X?g?E?Oo?t?B?b?N
			src = (i << 1) + 0;
			stamp = ++render.fast_stamp_counter;
			render.fast_mix_stamp[src] = stamp;
			render.fast_bg_stamp[src & 0x1ff] = stamp;
			render.mix[src] = TRUE;
			render.bgspmod[src & 0x1ff] = TRUE;
			TextFastPX68K(src);
			MixFastLine(src, src);
			src = (i << 1) + 1;
			stamp = ++render.fast_stamp_counter;
			render.fast_mix_stamp[src] = stamp;
			render.fast_bg_stamp[src & 0x1ff] = stamp;
			render.mix[src] = TRUE;
			render.bgspmod[src & 0x1ff] = TRUE;
			TextFastPX68K(src);
			MixFastLine(src, src);
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// oo[?v
	for (i=render.first; i<render.last; i++) {
		stamp = ++render.fast_stamp_counter;
		render.fast_mix_stamp[i] = stamp;
		render.fast_bg_stamp[i & 0x1ff] = stamp;
		render.mix[i] = TRUE;
		render.bgspmod[i & 0x1ff] = TRUE;
		TextFastPX68K(i);
		MixFastLine(i, i);
	}
	// ?X?V
	render.first = render.last;
}
