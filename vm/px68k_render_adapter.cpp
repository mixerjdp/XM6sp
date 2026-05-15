//---------------------------------------------------------------------------
//
//	PX68k Render Adapter
//
//	This module adapts the Px68kVideoEngine to the XM6 Render system.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "px68k_render_adapter.h"
#include "render.h"
#include "crtc.h"
#include "vc.h"
#include "gvram.h"
#include "tvram.h"
#include "sprite.h"

#include <cstring>

namespace {

static BOOL FASTCALL HasAnyTrue(const BOOL *flags, int count)
{
	if (!flags || (count <= 0)) {
		return FALSE;
	}
	for (int i = 0; i < count; i++) {
		if (flags[i]) {
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL FASTCALL HasAnyTrue(const BYTE *flags, int count)
{
	if (!flags || (count <= 0)) {
		return FALSE;
	}
	for (int i = 0; i < count; i++) {
		if (flags[i]) {
			return TRUE;
		}
	}
	return FALSE;
}

static void FASTCALL RebuildPaletteLookup(Px68kVideoEngineState *state)
{
	if (!state) {
		return;
	}

	for (int i = 0; i < 256; i++) {
		WORD pal = (WORD)(((WORD)state->palette.regs[i * 2] << 8) |
			state->palette.regs[i * 2 + 1]);
		state->palette.grphpal[i] = state->palette.pal16[pal];

		pal = (WORD)(((WORD)state->palette.regs[i * 2 + 0x200] << 8) |
			state->palette.regs[i * 2 + 0x201]);
		state->palette.textpal[i] = state->palette.pal16[pal];
	}
}

static void FASTCALL RebuildTextDrawWork(Px68kVideoEngineState *state)
{
	if (!state) {
		return;
	}

	DWORD *pattern = (DWORD *)state->tvram.textdrawpattern;
	for (DWORD tvram_addr = 0; tvram_addr < 0x20000; tvram_addr++) {
		const DWORD workadr = ((tvram_addr & 0x1ff80u) + ((tvram_addr ^ 1u) & 0x7fu)) << 3;
		BYTE pat = state->tvram.tvram[tvram_addr + 0x60000];
		DWORD t0 = pattern[(pat * 2) + 1536];
		DWORD t1 = pattern[(pat * 2 + 1) + 1536];

		pat = state->tvram.tvram[tvram_addr + 0x40000];
		t0 |= pattern[(pat * 2) + 1024];
		t1 |= pattern[(pat * 2 + 1) + 1024];

		pat = state->tvram.tvram[tvram_addr + 0x20000];
		t0 |= pattern[(pat * 2) + 512];
		t1 |= pattern[(pat * 2 + 1) + 512];

		pat = state->tvram.tvram[tvram_addr];
		t0 |= pattern[(pat * 2)];
		t1 |= pattern[(pat * 2 + 1)];

		*((DWORD *)&state->tvram.textdrawwork[workadr]) = t0;
		*(((DWORD *)(&state->tvram.textdrawwork[workadr])) + 1) = t1;
	}
}

static void FASTCALL RebuildBGCharacterCache(Px68kVideoEngineState *state)
{
	if (!state) {
		return;
	}

	for (DWORD addr = 0; addr < PX68K_BG_SIZE; addr++) {
		const BYTE data = state->bgsprite.bg[addr];
		if (addr < 0x2000) {
			state->bgsprite.bgchr8[addr * 2] = (BYTE)(data >> 4);
			state->bgsprite.bgchr8[addr * 2 + 1] = (BYTE)(data & 0x0f);
		}

		const DWORD bg16chr = ((addr & 3) * 2) + ((addr & 0x3c) * 4) +
			((addr & 0x40) >> 3) + ((addr & 0x7f80) * 2);
		state->bgsprite.bgchr16[bg16chr] = (BYTE)(data >> 4);
		state->bgsprite.bgchr16[bg16chr + 1] = (BYTE)(data & 0x0f);
	}
}

static void FASTCALL RebuildBGDerivedState(Px68kVideoEngineState *state)
{
	if (!state) {
		return;
	}

	const BYTE plane_cfg = state->bgsprite.bg_regs[0x09];
	const BYTE mode = state->bgsprite.bg_regs[0x11];

	state->bgsprite.bg_chrsize = (mode & 3) ? 16 : 8;
	state->bgsprite.bg_adrmask = (mode & 3) ? 1023 : 511;

	state->bgsprite.bg0scrollx = (((DWORD)state->bgsprite.bg_regs[0x00] << 8) +
		state->bgsprite.bg_regs[0x01]) & state->bgsprite.bg_adrmask;
	state->bgsprite.bg0scrolly = (((DWORD)state->bgsprite.bg_regs[0x02] << 8) +
		state->bgsprite.bg_regs[0x03]) & state->bgsprite.bg_adrmask;
	state->bgsprite.bg1scrollx = (((DWORD)state->bgsprite.bg_regs[0x04] << 8) +
		state->bgsprite.bg_regs[0x05]) & state->bgsprite.bg_adrmask;
	state->bgsprite.bg1scrolly = (((DWORD)state->bgsprite.bg_regs[0x06] << 8) +
		state->bgsprite.bg_regs[0x07]) & state->bgsprite.bg_adrmask;

	if (plane_cfg & 0x08) {
		if (plane_cfg & 0x30) {
			state->bgsprite.bg_bg1top = 0x6000;
			state->bgsprite.bg_bg1end = 0x8000;
		}
		else {
			state->bgsprite.bg_bg1top = 0x4000;
			state->bgsprite.bg_bg1end = 0x6000;
		}
	}
	else {
		state->bgsprite.bg_bg1top = 0;
		state->bgsprite.bg_bg1end = 0;
	}

	if (plane_cfg & 0x01) {
		if (plane_cfg & 0x06) {
			state->bgsprite.bg_bg0top = 0x6000;
			state->bgsprite.bg_bg0end = 0x8000;
		}
		else {
			state->bgsprite.bg_bg0top = 0x4000;
			state->bgsprite.bg_bg0end = 0x6000;
		}
	}
	else {
		state->bgsprite.bg_bg0top = 0;
		state->bgsprite.bg_bg0end = 0;
	}

	if (mode & 3) {
		if ((state->bgsprite.bg_bg0top == 0x4000) || (state->bgsprite.bg_bg1top == 0x4000)) {
			state->bgsprite.bg_chrend = 0x4000;
		}
		else if ((state->bgsprite.bg_bg0top == 0x6000) || (state->bgsprite.bg_bg1top == 0x6000)) {
			state->bgsprite.bg_chrend = 0x6000;
		}
		else {
			state->bgsprite.bg_chrend = 0x8000;
		}
	}
	else {
		state->bgsprite.bg_chrend = 0x2000;
	}

	state->bgsprite.bg_hadjust = ((int)state->bgsprite.bg_regs[0x0d] - (state->crtc.hstart + 4)) * 8;
	const int crtc_vstart = (int)((((WORD)state->crtc.regs[0x0c] << 8) | state->crtc.regs[0x0d]) & 1023);
	state->bgsprite.bg_vline = ((int)state->bgsprite.bg_regs[0x0f] - crtc_vstart) /
		((state->bgsprite.bg_regs[0x11] & 4) ? 1 : 2);
}

static unsigned int FASTCALL KeepStableVisibleExtent(unsigned int current, unsigned int incoming)
{
	// The XM6 CRTC view can briefly report 1-2 line geometries while the
	// frame is still being assembled. Px68k keeps the last stable extent
	// until a real mode change lands, so we do the same here.
	if (incoming >= 8u && incoming <= PX68K_FULLSCREEN_WIDTH) {
		return incoming;
	}
	return current ? current : incoming;
}

} // namespace

//===========================================================================
//
//	Constructor/Destructor
//
//===========================================================================

Px68kRenderAdapter::Px68kRenderAdapter()
{
	engine_ = NULL;
	crtc_ = NULL;
	vc_ = NULL;
	gvram_ = NULL;
	tvram_ = NULL;
	sprite_ = NULL;
	debug_text_ = TRUE;
	debug_grp_ = TRUE;
	debug_sp_ = TRUE;
	current_raster_ = 0;
	drawn_lines_ = 0;
	crtc_dirty_ = TRUE;
	vc_dirty_ = TRUE;
	palette_dirty_ = TRUE;
	full_sync_pending_ = TRUE;
	contrast_synced_ = -1;
}

Px68kRenderAdapter::~Px68kRenderAdapter()
{
	Cleanup();
}

//===========================================================================
//
//	Initialization
//
//===========================================================================

BOOL Px68kRenderAdapter::Init()
{
	engine_ = new Px68kVideoEngine();
	if (!engine_) {
		return FALSE;
	}

	if (!engine_->Init()) {
		delete engine_;
		engine_ = NULL;
		return FALSE;
	}

	return TRUE;
}

void Px68kRenderAdapter::Cleanup()
{
	if (engine_) {
		engine_->Cleanup();
		delete engine_;
		engine_ = NULL;
	}
}

void Px68kRenderAdapter::Reset()
{
	if (engine_) {
		engine_->Reset();
	}
	crtc_dirty_ = TRUE;
	vc_dirty_ = TRUE;
	palette_dirty_ = TRUE;
	full_sync_pending_ = TRUE;
	contrast_synced_ = -1;
}

//===========================================================================
//
//	Sync Methods - Copy state from XM6 devices to PX68k engine
//
//===========================================================================

void Px68kRenderAdapter::SyncCRTCState(const CRTC *crtc)
{
	if (!crtc || !engine_) return;

	const Px68kCrtcStateView *view = crtc->GetPx68kStateView();
	if (!view) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const Px68kCrtcState *state = &view->state;
	const Px68kTimingVideoView *timing = &view->timing_view;
	const unsigned int incoming_textdotx = (unsigned int)state->textdotx;
	const unsigned int incoming_textdoty = (unsigned int)state->textdoty;

	std::memcpy(es->crtc.regs, state->regs, sizeof(es->crtc.regs));
	es->crtc.mode = state->mode;
	es->crtc.lowres = state->lowres;
	es->crtc.vd = state->vd;
	es->crtc.textdotx = KeepStableVisibleExtent((unsigned int)es->crtc.textdotx, incoming_textdotx);
	es->crtc.textdoty = KeepStableVisibleExtent((unsigned int)es->crtc.textdoty, incoming_textdoty);
	es->crtc.vstart = state->vstart;
	es->crtc.vend = state->vend;
	es->crtc.hstart = state->hstart;
	es->crtc.hend = state->hend;
	es->crtc.textscrollx = state->textscrollx;
	es->crtc.textscrolly = state->textscrolly;
	for (int i = 0; i < 4; i++) {
		es->crtc.grphscrollx[i] = state->grphscrollx[i];
		es->crtc.grphscrolly[i] = state->grphscrolly[i];
	}
	es->crtc.fastclr = state->fastclr;
	es->crtc.fastclrline = state->fastclrline;
	es->crtc.fastclrmask = state->fastclrmask;
	es->crtc.intline = state->intline;
	es->crtc.vstep = state->vstep;
	es->crtc.hsync_clk = state->hsync_clk;
	es->crtc.rcflag[0] = state->rcflag[0];
	es->crtc.rcflag[1] = state->rcflag[1];

	if (timing->valid) {
		es->crtc.vline_total = timing->crtc_vline_total;
	}
	else {
		es->crtc.vline_total = (DWORD)(((state->regs[0x08] << 8) | state->regs[0x09]) & 1023);
	}
	es->vline_total = es->crtc.vline_total;

	const int old_vid_mode = es->vid_mode;
	if (timing->valid) {
		es->vid_mode = timing->crtc_vsync_high ? 1 : 0;
	}
	else {
		es->vid_mode = (state->regs[0x29] & 0x10) ? 1 : 0;
	}
	if (es->vid_mode != old_vid_mode) {
		es->changeav_timing = TRUE;
	}

	RebuildBGDerivedState(es);
	crtc_dirty_ = FALSE;
}

void Px68kRenderAdapter::SyncVCState(const VC *vc)
{
	if (!vc || !engine_) return;

	const VC::vc_t *state = vc->GetWorkAddr();
	if (!state) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const BYTE old_vcreg0[2] = { es->vc.vcreg0[0], es->vc.vcreg0[1] };
	const BYTE old_vcreg1[2] = { es->vc.vcreg1[0], es->vc.vcreg1[1] };
	const BYTE old_vcreg2[2] = { es->vc.vcreg2[0], es->vc.vcreg2[1] };

	es->vc.vcreg0[0] = 0;
	es->vc.vcreg0[1] = (BYTE)((state->siz ? 0x04 : 0x00) | (state->col & 0x03));
	es->vc.vcreg1[0] = (BYTE)(state->vr1h & 0xff);
	es->vc.vcreg1[1] = (BYTE)(state->vr1l & 0xff);
	es->vc.vcreg2[0] = (BYTE)(state->vr2h & 0xff);
	es->vc.vcreg2[1] = (BYTE)(state->vr2l & 0xff);

	if ((old_vcreg0[0] != es->vc.vcreg0[0]) ||
	    (old_vcreg0[1] != es->vc.vcreg0[1]) ||
	    (old_vcreg1[0] != es->vc.vcreg1[0]) ||
	    (old_vcreg1[1] != es->vc.vcreg1[1]) ||
	    (old_vcreg2[0] != es->vc.vcreg2[0]) ||
	    (old_vcreg2[1] != es->vc.vcreg2[1])) {
		engine_->TVRAMSetAllDirty();
	}

	vc_dirty_ = FALSE;
}

void Px68kRenderAdapter::SyncGVRAMState(const GVRAM *gvram)
{
	if (!gvram || !engine_) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const BYTE *src = gvram->GetGVRAM();
	if (src) {
		std::memcpy(es->gvram.gvram, src, PX68K_GVRAM_SIZE);
		engine_->TVRAMSetAllDirty();
	}
	gvram_ = gvram;
}

void Px68kRenderAdapter::SyncTVRAMState(const TVRAM *tvram)
{
	if (!tvram || !engine_) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const BYTE *src = tvram->GetTVRAM();
	if (src) {
		std::memcpy(es->tvram.tvram, src, PX68K_TVRAM_SIZE);
		RebuildTextDrawWork(es);
		std::memset(es->tvram.textdirtyline, 1, sizeof(es->tvram.textdirtyline));
	}
	tvram_ = tvram;
}

void Px68kRenderAdapter::SyncSpriteState(const Sprite *sprite)
{
	if (!sprite || !engine_) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const BYTE *mem = sprite->GetMem();
	const BYTE *pcg = sprite->GetPCG();
	if (mem) {
		std::memcpy(es->bgsprite.sprite_regs, mem, PX68K_SPRITE_REGS_SIZE);
		for (int i = 0; i < 0x12; i++) {
			es->bgsprite.bg_regs[i] = mem[0x800 + (i ^ 1)];
		}
	}
	if (pcg) {
		std::memcpy(es->bgsprite.bg, pcg, PX68K_BG_SIZE);
		RebuildBGCharacterCache(es);
	}
	RebuildBGDerivedState(es);
	std::memset(es->tvram.textdirtyline, 1, sizeof(es->tvram.textdirtyline));

	sprite_ = sprite;
}

void Px68kRenderAdapter::SyncPaletteState(const VC *vc)
{
	if (!vc || !engine_) return;

	Px68kVideoEngineState *es = engine_->GetState();
	const WORD *palette = (const WORD *)vc->GetPalette();
	if (!palette) {
		return;
	}

	for (int i = 0; i < 512; i++) {
		const WORD w = palette[i];
		es->palette.regs[i * 2] = (BYTE)((w >> 8) & 0xff);
		es->palette.regs[i * 2 + 1] = (BYTE)(w & 0xff);
	}
	engine_->Pal_SetColor();
	if (contrast_synced_ >= 0) {
		engine_->Pal_ChangeContrast(contrast_synced_);
	}
	else {
		engine_->TVRAMSetAllDirty();
	}
	RebuildPaletteLookup(es);
	palette_dirty_ = FALSE;
}

void Px68kRenderAdapter::SyncPaletteContrast(Render *owner)
{
	if (!owner || !engine_) {
		return;
	}

	Px68kVideoEngineState *es = engine_->GetState();
	contrast_synced_ = owner->GetContrast();
	engine_->Pal_ChangeContrast(contrast_synced_);
	RebuildPaletteLookup(es);
}

void Px68kRenderAdapter::SyncAllState(Render *owner)
{
	if (!owner) return;

	// Get device pointers from Render
	crtc_ = owner->GetCRTCDevice();
	vc_ = owner->GetVCDevice();
	gvram_ = owner->GetGVRAMDevice();
	tvram_ = owner->GetTVRAMDevice();
	sprite_ = owner->GetSpriteDevice();

	// Keep all mapped state in sync each frame.
	SyncCRTCState(crtc_);
	SyncVCState(vc_);
	SyncPaletteState(vc_);
	SyncGVRAMState(gvram_);
	SyncTVRAMState(tvram_);
	SyncSpriteState(sprite_);
	full_sync_pending_ = FALSE;
}

void Px68kRenderAdapter::SyncDynamicState(Render *owner)
{
	if (!owner || !engine_) {
		return;
	}

	Render::render_t *work = owner->GetWorkAddr();
	if (!work) {
		return;
	}

	// CRTC/VC updates are inexpensive and keep scanline timing aligned.
	SyncCRTCState(crtc_);
	SyncVCState(vc_);
	if (work->palette || palette_dirty_) {
		SyncPaletteState(vc_);
		work->palette = FALSE;
	}
	if (work->contrast) {
		SyncPaletteContrast(owner);
		work->contrast = FALSE;
	}
	if (HasAnyTrue(work->textmod, 1024)) {
		SyncTVRAMState(tvram_);
		std::memset(work->textmod, 0, sizeof(work->textmod));
	}
	if (HasAnyTrue(work->grpmod, 2048) || HasAnyTrue(work->grppal, 2048)) {
		SyncGVRAMState(gvram_);
		std::memset(work->grpmod, 0, sizeof(work->grpmod));
		std::memset(work->grppal, 0, sizeof(work->grppal));
	}
	if (HasAnyTrue(work->bgspmod, 1024)) {
		SyncSpriteState(sprite_);
		std::memset(work->bgspmod, 0, sizeof(work->bgspmod));
	}
}

//===========================================================================
//
//	Frame Processing
//
//===========================================================================

void Px68kRenderAdapter::StartFrame(Render *owner)
{
	if (!engine_) return;

	if (full_sync_pending_) {
		SyncAllState(owner);
	}
	else if (owner) {
		crtc_ = owner->GetCRTCDevice();
		vc_ = owner->GetVCDevice();
		gvram_ = owner->GetGVRAMDevice();
		tvram_ = owner->GetTVRAMDevice();
		sprite_ = owner->GetSpriteDevice();
		SyncDynamicState(owner);
	}
	engine_->TVRAMSetAllDirty();
	engine_->StartFrame();
	current_raster_ = 0;
	drawn_lines_ = 0;
}

void Px68kRenderAdapter::EndFrame(Render *owner)
{
	if (!engine_) return;

	if (owner) {
		SyncAllState(owner);
		engine_->TVRAMSetAllDirty();
		engine_->DrawFrame();
	}

	engine_->EndFrame();
}

void Px68kRenderAdapter::HSync(Render *owner, int raster)
{
	if (!engine_) return;

	if (owner) {
		crtc_ = owner->GetCRTCDevice();
		vc_ = owner->GetVCDevice();
		gvram_ = owner->GetGVRAMDevice();
		tvram_ = owner->GetTVRAMDevice();
		sprite_ = owner->GetSpriteDevice();
		SyncDynamicState(owner);
	}

	current_raster_ = raster;
	const Px68kCrtcStateView *view = crtc_ ? crtc_->GetPx68kStateView() : NULL;
	if (!view) {
		return;
	}

	const DWORD visible_vline = view->state.visible_vline;
	if (visible_vline == 0xffffffffu) {
		return;
	}

	if (view->state.vstep == 1) {
		if ((raster & 1) == 0) {
			return;
		}
		DrawScanline(drawn_lines_);
		return;
	}

	if (view->state.vstep == 4) {
		DrawScanline(drawn_lines_);
		DrawScanline(drawn_lines_);
		return;
	}

	DrawScanline(drawn_lines_);
}

void Px68kRenderAdapter::DrawFrame(Render *owner)
{
	if (!engine_) return;

	if (owner) {
		SyncAllState(owner);
	}
	engine_->TVRAMSetAllDirty();
	engine_->DrawFrame();
	drawn_lines_ = (int)engine_->GetTextDotY();
}

void Px68kRenderAdapter::SetCRTC(Render *owner)
{
	crtc_dirty_ = TRUE;
	if (owner) {
		SyncCRTCState(owner->GetCRTCDevice());
	}
}

void Px68kRenderAdapter::SetVC(Render *owner)
{
	vc_dirty_ = TRUE;
	palette_dirty_ = TRUE;
	if (owner) {
		SyncVCState(owner->GetVCDevice());
		SyncPaletteState(owner->GetVCDevice());
	}
}

void Px68kRenderAdapter::Process(Render *owner)
{
	if (!engine_) return;

	// The fast pipeline is scanline-driven through HSync().
	// Keep dynamic state synchronized here but avoid a second full-frame draw.
	if (owner) {
		SyncDynamicState(owner);
	}
}

void Px68kRenderAdapter::DrawScanline(int visible_vline)
{
	if (!engine_) return;

	engine_->SetPhysicalVLine((DWORD)current_raster_);
	engine_->SetVLine((DWORD)visible_vline);
	if ((visible_vline >= 0) &&
	    (visible_vline < (int)engine_->GetTextDotY()) &&
	    (visible_vline < (int)PX68K_FULLSCREEN_HEIGHT)) {
		engine_->WinDraw_DrawLine();
		drawn_lines_++;
	}
}

//===========================================================================
//
//	Output Access
//
//===========================================================================

WORD* Px68kRenderAdapter::GetScreenBuffer() const
{
	if (!engine_) return NULL;
	return engine_->GetScreenBuffer();
}

DWORD Px68kRenderAdapter::GetScreenWidth() const
{
	if (!engine_) return 0;
	return engine_->GetTextDotX();
}

DWORD Px68kRenderAdapter::GetScreenHeight() const
{
	if (!engine_) return 0;
	return engine_->GetTextDotY();
}

DWORD Px68kRenderAdapter::GetScreenStride() const
{
	if (!engine_) return 0;
	return PX68K_FULLSCREEN_WIDTH;
}

void Px68kRenderAdapter::BGWrite(DWORD addr, BYTE data)
{
	if (!engine_) return;
	engine_->BG_Write(addr, data);
}

BYTE Px68kRenderAdapter::BGRead(DWORD addr)
{
	if (!engine_) return 0xff;
	return engine_->BG_Read(addr);
}

void Px68kRenderAdapter::GVRAMFastClear()
{
	if (!engine_) return;
	engine_->GVRAM_FastClear();
}

void Px68kRenderAdapter::CRTCRegWrite(DWORD addr, BYTE data)
{
	if (!engine_) return;
	engine_->CRTC_Write(addr, data);
}

BYTE Px68kRenderAdapter::CRTCRegRead(DWORD addr)
{
	if (!engine_) return 0xff;
	return engine_->CRTC_Read(addr);
}

BYTE Px68kRenderAdapter::VCtrlRead(DWORD addr)
{
	if (!engine_) return 0xff;
	return engine_->VCtrl_Read(addr);
}

BYTE Px68kRenderAdapter::TVRAMRead(DWORD addr)
{
	if (!engine_) return 0xff;
	return engine_->TVRAM_Read(addr);
}

void Px68kRenderAdapter::TVRAMWrite(DWORD addr, BYTE data)
{
	if (!engine_) return;
	engine_->TVRAM_Write(addr, data);
}

BYTE Px68kRenderAdapter::GVRAMRead(DWORD addr)
{
	if (!engine_) return 0xff;
	return engine_->GVRAM_Read(addr);
}

void Px68kRenderAdapter::GVRAMWrite(DWORD addr, BYTE data)
{
	if (!engine_) return;
	engine_->GVRAM_Write(addr, data);
}

void Px68kRenderAdapter::VCtrlWrite(DWORD addr, BYTE data)
{
	if (!engine_) return;
	engine_->VCtrl_Write(addr, data);
}
