//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Video ]
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
//	Video
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	Variable
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// Color0 flag (used by rend_asm.asm)
#define REND_PX68K_IBIT	0x40000000		// px68k Ibit marker (XRGB8888: top byte ignored)

static int FASTCALL CalcBGHAdjustPixels(int compositor_mode, const CRTC *crtc, const Sprite *sprite)
{
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
		if ((mode == Render::compositor_fast) && owner) {
			owner->EndFrameFast();
			return;
		}
		owner->EndFrameOriginal();
	}

	void HSync(Render *owner, int raster)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->HSyncFast(raster);
			return;
		}
		owner->HSyncOriginal(raster);
	}

	void SetCRTC(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->SetCRTCFast();
			return;
		}
		owner->SetCRTCOriginal();
	}

	void SetVC(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->SetVCFast();
			return;
		}
		owner->SetVCOriginal();
	}

private:
	int mode;
};
//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Render::Render(VM *p) : Device(p)
{
	// Device ID
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// Device pointer
	crtc = NULL;
	vc = NULL;
	sprite = NULL;
	backend = NULL;
	backend_original = NULL;
	backend_fast = NULL;
	compositor_mode = compositor_original;
	palbuf_original = NULL;
	palbuf_fast = NULL;
	fast_fallback_count = 0;
	transparency_enabled = TRUE;
	original_bg0_render_enabled = TRUE;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	// Work area init (CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

	// render
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// render
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// render
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// Work area init (PCG, sprite, BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = NULL;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// Work area init (mix)
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

	// Work area init (draw)
	render.drawflag = NULL;

	// oo?
	cmov = FALSE;
}

//---------------------------------------------------------------------------
//
//	Init
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// Device::Init
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTC search
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// VC search
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// Palette buffer alloc (4MB x2: original + px68k-safe for fast)
	palbuf_original = NULL;
	palbuf_fast = NULL;
	try {
		palbuf_original = new DWORD[0x10000 * 16];
		palbuf_fast = new DWORD[0x10000 * 16];
	}
	catch (...) {
		if (palbuf_original) {
			delete[] palbuf_original;
			palbuf_original = NULL;
		}
		if (palbuf_fast) {
			delete[] palbuf_fast;
			palbuf_fast = NULL;
		}
		return FALSE;
	}
	if (!palbuf_original || !palbuf_fast) {
		if (palbuf_original) {
			delete[] palbuf_original;
			palbuf_original = NULL;
		}
		if (palbuf_fast) {
			delete[] palbuf_fast;
			palbuf_fast = NULL;
		}
		return FALSE;
	}
	render.palbuf = palbuf_original;

	// Text VRAM buffer alloc (4.7MB)
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

	// Graphic VRAM buffer alloc (8.2MB)
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

	// PCG buffer alloc (4MB)
	try {
		render.pcgbuf = new DWORD[ 16 * 256 * 16 * 16 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.pcgbuf) {
		return FALSE;
	}

	// Sprite pointer alloc (256KB)
	try {
		render.spptr = new DWORD*[ 128 * 512 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.spptr) {
		return FALSE;
	}

	// BG pointer alloc (768KB)
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

	// BG/Sprite buffer alloc (1MB)
	try {
		render.bgspbuf = new DWORD[ 512 * 512 + 16];	// +16 for safety
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgspbuf) {
		return FALSE;
	}

	// Draw flag buffer alloc (256KB)
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

	// Make palette
	MakePalette();

	// Contrast work init
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
//	Cleanup
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

	// Draw flag
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/Sprite buffer
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BG pointer
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// Sprite pointer
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCG buffer
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// Graphic VRAM buffer
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

	// Text VRAM buffer
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

	// Palette buffer (original + fast)
	if (palbuf_original) {
		delete[] palbuf_original;
		palbuf_original = NULL;
	}
	if (palbuf_fast) {
		delete[] palbuf_fast;
		palbuf_fast = NULL;
	}
	render.palbuf = NULL;

	// Device::Cleanup
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
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
	LOG0(Log::Normal, "???Z?b?g");

	// Get VC palette pointer
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// Get Text VRAM pointer
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// Get Graphic VRAM pointer
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// Get Sprite register pointer
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// Work area init
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = (compositor_mode == compositor_fast) ? 0 : 2;
	memset(render.mix, 0, sizeof(render.mix));
	memset(render.draw, 0, sizeof(render.draw));
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixshift, 0, sizeof(render.mixshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixand, 0, sizeof(render.mixand));
	memset(render.mixmap, 0, sizeof(render.mixmap));
	render.mixpage = 0;
	fast_fallback_count = 0;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	if (render.mixbuf && (render.mixwidth > 0) && (render.mixheight > 0)) {
		memset(render.mixbuf, 0, sizeof(DWORD) * render.mixwidth * render.mixheight);
	}
	if (render.textbuf) {
		memset(render.textbuf, 0, sizeof(BYTE) * 1024 * 512);
	}
	if (render.textout) {
		memset(render.textout, 0, sizeof(DWORD) * 1024 * (1024 + 1));
	}
	if (render.grpbuf[0]) {
		memset(render.grpbuf[0], 0, sizeof(DWORD) * 512 * 1024 * 4);
	}
	if (render.bgspbuf) {
		memset(render.bgspbuf, 0, sizeof(DWORD) * (512 * 512 + 16));
	}

	// Work area init (crtc, vc)
	render.crtc = FALSE;
	render.vc = FALSE;

	// Work area init (contrast)
	render.contrast = FALSE;

	// Work area init (palette)
	render.palette = FALSE;
	render.palptr = render.palbuf;

	// Work area init (text)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// Work area init (graphic)
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
	}
	render.grptype = 4;

	// Work area init (PCG)
	// Reset BG,Sprite,etc PCG usage
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// Work area init (sprite)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// Work area init (BG)
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

	// Work area init (BG/sprite)
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BG table init (value 10000)
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

	// Work area init (mix)
	render.mixtype = 0;
	if (backend) {
		backend->Activate(this);
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "Save");


	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "Load");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(this);
	LOG0(Log::Normal, "Apply");
}

//---------------------------------------------------------------------------
//
//	Compositor start
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::SetCompositorMode(int mode)
{
	Backend *next = NULL;
	DWORD *next_palbuf = NULL;

	switch (mode) {
	case compositor_original:
		next = backend_original;
		next_palbuf = palbuf_original;
		break;
	case compositor_fast:
		next = backend_fast;
		next_palbuf = palbuf_fast;
		break;
	default:
		return FALSE;
	}

	compositor_mode = mode;
	if (!next) {
		return TRUE;
	}

	backend = next;
	if (next_palbuf && render.palbuf != next_palbuf) {
		render.palbuf = next_palbuf;
		render.palptr = render.palbuf + (render.contlevel << 16);
		InvalidateAll();
	}
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

void FASTCALL Render::ForceRecompose()
{
	InvalidateAll();
}

void FASTCALL Render::InvalidateFrame()
{
	int i;
	DWORD stamp;

	render.vc = TRUE;
	render.palette = TRUE;

	memset(render.palmod, 1, sizeof(render.palmod));
	memset(render.mix, 1, sizeof(render.mix));
	memset(render.textmod, 1, sizeof(render.textmod));
	memset(render.textpal, 1, sizeof(render.textpal));
	memset(render.grpmod, 1, sizeof(render.grpmod));
	memset(render.grppal, 1, sizeof(render.grppal));
	memset(render.bgspmod, 1, sizeof(render.bgspmod));
	stamp = ++render.fast_stamp_counter;
	for (i=0; i<1024; i++) {
		render.fast_mix_stamp[i] = stamp;
		render.fast_mix_done[i] = 0;
	}
	for (i=0; i<512; i++) {
		render.fast_bg_stamp[i] = stamp;
		render.fast_bg_done[i] = 0;
	}
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

void FASTCALL Render::StartFrameOriginal()
{
	CRTC::crtc_t crtcdata;
	int i;

	ASSERT(this);

	// Skip if rendering disabled or still counting
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// Rendering now active
	render.act = TRUE;

	// Reset scanline
	render.first = 0;
	render.last = -1;

	// CRTC flag check
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC changed");
#endif	// REND_LOG

		// Get data
		crtc->GetCRTC(&crtcdata);

		// h_dots and v_dots not 0?
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		// Calculate resolution
		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		// Update mix buffer length
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		// Sprite reset (mixlen update)
		SpriteReset();

		// Clear line
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}

		// Clear
		render.crtc = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Frame end
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrameOriginal()
{
	ASSERT(this);

	// Not active, skip
	if (!render.act) {
		return;
	}

	// Process last scanline
	if (render.last > 0) {
		render.last = render.height;
		Process();
	}

	// Counter up
	render.count++;

	// Inactive
	render.act = FALSE;
}

//---------------------------------------------------------------------------
//
//	Mix buffer set
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	int i;

	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// Set
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// Update mix buffer length
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// Set all line
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	CRTC reset
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// Flag on
	render.crtc = TRUE;
	render.vc = TRUE;

	if ((compositor_mode == compositor_fast) && render.act) {
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

//---------------------------------------------------------------------------
//
//	VC reset
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// Flag on
	render.vc = TRUE;

	if ((compositor_mode == compositor_fast) && render.act) {
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

//---------------------------------------------------------------------------
//
//	VC process
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

	// VC flag clear
	render.vc = FALSE;

	// Flag on
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VC data and CRTC data get
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// Text check
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// Graphic type
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

	// Graphic check and composition
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

	// Graphic buffer reset
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

	// Priority get
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// Type clear
	render.mixtype = 0;

	// BG/sprite display
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/sprite display ON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/sprite display OFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// If (q->hd >= 2 or sprite off)
	if ((q->hd >= 2) || (!p->son)) {
		// Sprite off
		if (!render.texten) {
			// Graphic only
			if (render.mixpage == 0) {
				// Graphic none (type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// Graphic 1 page (type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// Graphic 2 page (type=2)
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
			// Graphic 3 or 4 page (type=4)
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
		// Text only
		if (render.mixpage == 0) {
			// Graphic none + Text only (type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// Text+Graphic 1 page
			if (tx < gr) {
				// Text top (type=3)
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
			// Graphic top (type=3,tx=gr means graphic over text)
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
		// Text+Graphic 2+ page (type=5, type=6)
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

	// Sprite on
	if (!render.texten) {
		// Graphic only
		if (render.mixpage == 0) {
			// Graphic none + Sprite only (type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// Sprite+Graphic 1 page (type=3)
			if (sp < gr) {
				// Sprite top
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
			// Graphic top (sp=gr)
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
		// Sprite+Graphic 2+ page (type=5, type=6)
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

	// Text only
	if (render.mixpage == 0) {
		// Graphic none + Text + Sprite (type=3)
		if (tx <= sp) {
			// tx=sp means text top (LMZ2)
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
		// Sprite top
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

	// Priority swap
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// Priority exchange
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
		// Priority exchange
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
		// Priority exchange
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
		// Text+Sprite+Graphic 1 page (type=7)
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

	// Text+Sprite+Graphic 2+ page (type=8)
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
//	Contrast set
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont)
{
	// System parameter validity check and adjustment
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// Flag on
	render.contlevel = cont;
	render.contrast = TRUE;
}

//---------------------------------------------------------------------------
//
//	Contrast get
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
//	Contrast operation
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	int i;

	// Palette base, contrast down
	render.palptr = render.palbuf;
	render.palptr += (render.contlevel << 16);
	render.contrast = FALSE;

	// Palette flag set up
	for (i=0; i<0x200; i++) {
		render.palmod[i] = TRUE;
	}
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	Make palette
//
//---------------------------------------------------------------------------
void FASTCALL Render::MakePalette()
{
	DWORD *p;
	int ratio;
	int i;
	int j;

	ASSERT(palbuf_original);
	ASSERT(palbuf_fast);

	// --- Original palette table ---
	p = palbuf_original;
	for (i=0; i<16; i++) {
		ratio = 256 - ((15 - i) << 4);
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}

	p = palbuf_fast;
	for (i=0; i<16; i++) {
		ratio = 256 - ((15 - i) << 4);
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Palette converter
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

	// Gray scale
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSB=G:5 R:5 B:5 I:1 padding
	// Convert R:8 G:8 B:8 to DWORD with bits 31-24 padding
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// Odd color brighten up (data is 0, but !=0 bright)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// Contrast apply
	b *= ratio;
	b >>= 8;
	g *= ratio;
	g >>= 8;
	g &= 0xff00;
	r *= ratio;
	r >>= 8;
	r &= 0xff0000;

	DWORD out = (DWORD)(r | g | b);
	if (color & 1) {
		out |= REND_PX68K_IBIT;
	}
	return out;
}

//---------------------------------------------------------------------------
//
//	Palette get
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
//	Palette operation
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

	// Flag off
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// Graphic
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// Graphic on and flag off
			gr = TRUE;
			render.palmod[i] = FALSE;

			// Zero flag
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536 color zero palette data
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// Text or Sprite
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// Text on and flag off
			tx = TRUE;
			render.palmod[i] = FALSE;

			// Zero flag
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100 is BG or Sprite top priority
				sp = TRUE;
			}

			// PCG on
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// Sprite
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// Sprite on and flag off
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// Zero flag
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCG on
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// Graphic flag
	if (gr) {
		// Flag on
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// Text flag
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// Sprite flag
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// Palette flag off
	render.palette = FALSE;
}

void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	int i;

	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// Value check
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// Work update
	render.textx = x;
	render.texty = y;

	// Flag on
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "Text Scroll x=%d y=%d", x, y);
#endif	// REND_LOG

		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Text copy
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// Bank copy
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	Text buffer get
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
//	Text
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

	// Text off skip
	if (!render.texten) {
		return;
	}

	// Offset Y calc
	y = (raster + render.texty) & 0x3ff;

	// Update flag (this line)
	if (render.textmod[y]) {
		// Flag clear
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		// Render
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// Update palette
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// Palette (this line)
	if (render.textpal[y]) {
		// Flag clear
		render.textpal[y] = FALSE;

		// Update palette
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		render.mix[raster] = TRUE;

		// y == 1023 copy line
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Graphic buffer get
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
//	Graphic scroll
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

	// Value check and update
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpen[block];
	}

	// Flag clear
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "Graphic Scroll block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Graphic
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
		// 1024 unit and page 0 only
		if (!render.grpen[0]) {
			return;
		}
	}
	else {
		// This page
		if (!render.grpen[block]) {
			return;
		}
	}

	// Type
	switch (render.grptype) {
		// Type 0: 1024x1024 16Color
		case 0:
			// Initial offset calc
			offset = (raster + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// Outside check
			if ((offset < 512) && (block >= 2)) {
				return;
			}
			if ((offset >= 512) && (block < 2)) {
				return;
			}

			// Palette and render
			if (render.grppal[y + (block << 9)]) {
				render.grppal[y + (block << 9)] = FALSE;
				render.grpmod[y + (block << 9)] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + (block << 14) + i] = FALSE;
				}
				switch (block) {
					// Upper block 0 or 1
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 1:
						break;
					// Lower block 2 or 3
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

			// Other grpmod check
			if (!render.grpmod[y + (block << 9)]) {
				return;
			}
			render.grpmod[y + (block << 9)] = FALSE;
			render.mix[raster] = TRUE;
			switch (block) {
				// Block 0 upper
				case 0:
					Rend1024C(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5),
								render.paldata);
					break;
				// Block 1 lower
				case 1:
					Rend1024D(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					break;
				// Block 2 upper
				case 2:
					Rend1024E(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					break;
				// Block 3 lower
				case 3:
					Rend1024F(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0xc000,
								render.paldata);
					break;
			}
			return;

		// Type 1: 512x512 16Color
		case 1:
			switch (block) {
				// Plane 0
				case 0:
					y = (raster + render.grpy[0]) & 0x1ff;
					// Palette
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
					// o?
					if (render.grpmod[y]) {
						render.grpmod[y] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.paldata);
					}
					return;
				// Plane 1
				case 1:
					y = (raster + render.grpy[1]) & 0x1ff;
					// Palette
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
					// o?
					if (render.grpmod[y + 512]) {
						render.grpmod[y + 512] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[1] + (y << 10),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					}
					return;
				// Plane 2
				case 2:
					y = (raster + render.grpy[2]) & 0x1ff;
					// Palette
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
					// o?
					if (render.grpmod[y + 1024]) {
						render.grpmod[y + 1024] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[2] + (y << 10),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					}
					return;
				// Plane 3
				case 3:
					y = (raster + render.grpy[3]) & 0x1ff;
					// Palette
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
					// o?
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

		// Type 2: 512x512 256Color
		case 2:
			ASSERT((block == 0) || (block == 2));
			if (block == 0) {
				// Initial offset calc
				y = (raster + render.grpy[0]) & 0x1ff;

				// Palette and render
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

				// grpmod
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
				// Initial offset calc
				y = (raster + render.grpy[2]) & 0x1ff;

				// Palette and render
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

				// grpmod
				if (!render.grpmod[0x400 + y]) {
					return;
				}

				render.grpmod[0x400 + y] = FALSE;
				render.mix[raster] = TRUE;

				// oo?_oo?O
				Rend256B(render.grpgv + (y << 10),
							render.grpbuf[2] + (y << 10),
							render.grpflag + 0x8000 + (y << 5),
							render.paldata);
			}
			return;

		// Type 3: 512x512 mono
		case 3:
		// Type 4: 512x512 65536Color
		case 4:

			ASSERT(block == 0);
			// Initial offset calc
			y = (raster + render.grpy[0]) & 0x1ff;

			// Palette and render
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

			// oo?Oogrpmodoooo
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
//	oo?_o(BG?E?X?vo?C?go)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Sprite Reset
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	DWORD addr;
	WORD data;

	// Sprite work register
	for (addr=0; addr<0x400; addr+=2) {
		data = *(WORD*)(&render.sprmem[addr]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	Sprite
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

	// Register value change check
	index = (int)(addr >> 3);
	switch ((addr & 7) >> 1) {
		// X,Y(0?`1023)
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

	// ptro?(&spptr[index << 9])
	ptr = &render.spptr[index << 9];

	// o?W?X?^oo?b?N?A?b?v
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// o?W?X?^oooo?
	render.spreg[addr >> 1] = data;

	// ooLooo?`?F?b?N
	use = TRUE;
	if (next[1] == 0) {
		use = FALSE;
	}
	if (next[1] >= (512 + 16)) {
		use = FALSE;
	}
	if (next[3] == 0) {
		use = FALSE;
	}

	// ooooo?Aoooooo�E��E�ooo
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// oooLoooAoxoo
	if (render.spuse[index]) {
		// oooo(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[ pcgno ] > 0);
		render.pcguse[ pcgno ]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[ pcgno ] > 0);
		render.pcgpal[ pcgno ]--;

		// oooo(?|?Co?^)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		// oooo?AoooIo
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// Pattern (unused)
	render.spuse[index] = TRUE;

	// Pattern (PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[ pcgno ]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[ pcgno ]++;

	// PCG?A?ho?Xov?Z?A?|?Co?^?Z?b?g
	if (next[2] & 0x8000) {
		// Vo?]
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
			// Search
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
//	BGScrl
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// or?Aovoooooo?
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// Render
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768x512 check
	if (!render.bgspflag) {
		return;
	}

	// Update and BGSPMOD flag on
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
//	BG Control
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

	// Flag off
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// Type
	switch (index) {
		// BG0 ?\o?to?O
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 ?\o?to?O
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 ?Go?AoX
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 ?Go?AoX
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BG?T?C?YoX
		case 4:

			if (render.bgsize == flag) {
				return;
			}
			render.bgsize = flag;
			areaflag[0] = TRUE;
			areaflag[1] = TRUE;
			break;

		// oo?(ooo?)
		default:
			ASSERT(FALSE);
			return;
	}

	// Flag clear
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// oo?gooo?render.pcguseoJ?b?g
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

			// Data and address calc ($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64?~64o?[?h?R?s?[?B$10000or?b?goo0
			if (render.bgsize) {
				// 16x16ooo?
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8oH?vo?K?v?BPCG(0-255)o>>2o?Aooobit0,1obit17,18o
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

			// bgalloZ?b?g
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// ooXo?A768?~512oOo?bgspmodo�E�Oo
	if (render.bgspflag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG Mem
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

	// Plane loop
	for (i=0; i<2; i++) {
		// Lower plane of same color priority
		flag = FALSE;
		if ((render.bgarea[i] == FALSE) && (addr < 0xe000)) {
			flag = TRUE;
		}
		if (render.bgarea[i] && (addr >= 0xe000)) {
			flag = TRUE;
		}
		if (!flag) {
			continue;
		}

		// Block (<64x64) from address pointer get
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// oOopcguseoo?
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[ pcgno ] > 0);
			render.pcguse[ pcgno ]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[ pcgno ] > 0);
			render.pcgpal[ pcgno ]--;
		}

		// Render
		if (render.bgsize) {
			// 16x16ooo?
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8oH?vo?K?v?BPCG(0-255)o>>2o?Aooobit0,1obit17,18o
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

		// bgallo�E�Oo
		render.bgall[i][index >> 6] = TRUE;

		// Double check if bgsize=1 and plane1 only
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// Scroll and update BGSPMOD flag on
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
//	PCG Mem
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

	// Block loop
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgready?
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguseo>0o?
	count = 0;
	for (i=0; i<16; i++) {
		count += render.pcguse[index + (i << 8)];
	}
	if (count > 0) {
		// Do or do not BG/sprite render
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Get PCG Buffer
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
//	GetBGSpBuf
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
//	BGSprite
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSprite(int raster)
{
	int i;
	DWORD *reg;
	DWORD **ptr;
	DWORD *buf;
	DWORD pcgno;
	DWORD stamp;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);
	const BOOL sprite_visible = sprite->IsDisplay();

	if (raster >= 512) return;
	if (render.mixlen > 512) return;

	stamp = render.fast_bg_stamp[raster];
	if (!render.bgspmod[raster] && (render.fast_bg_done[raster] == stamp)) {
		return;
	}
	render.bgspmod[raster] = FALSE;
	render.mix[raster] = TRUE;

	buf = &render.bgspbuf[raster << 9];
	RendClrSprite(buf, render.paldata[0x100], render.mixlen);
	if (!sprite_visible) {
		RendClrSprite(buf, render.paldata[0x100] & 0x00ffffff, render.mixlen);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 1) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, raster, buf, TRUE);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 2) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[0]) {
		BG(0, raster, buf, (BOOL)(!render.bgdisp[1] || render.bgsize));
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 3) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				if (cmov) {
					RendSpriteC(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
				else {
					RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000);
				}
			}
			reg -= 4;
			ptr -= 512;
		}
	}
	render.fast_bg_done[raster] = stamp;
}

void FASTCALL Render::BG(int page, int raster, DWORD *buf, BOOL force)
{
	int x;
	int y;
	DWORD **ptr;
	int len;
	int rest;
	const BOOL legacy_bg_transparency = !original_bg0_render_enabled;
	const BOOL draw_force = (BOOL)(force && original_bg0_render_enabled);

	ASSERT((page == 0) || (page == 1));
	ASSERT((raster >= 0) && (raster < 512));
	ASSERT(buf);

	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	// y?uo?b?Noo?oo
	y = render.bgy[page] + raster;
	if (render.bgsize) {
		// 16x16o?[?h
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8o?[?h
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// bgalloTRUEo?Aooy?uo?b?No?X?f?[?^oo
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// Loop
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// 8x8o\o
		x = (render.bgx[page] - bg_hadjust) & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 7);

		// oooo`?F?b?N
		if ((x & 7) == 0) {
			// 8x8?Aooo?
			x >>= 3;
			if (draw_force) {
				RendBG8F(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			else if (cmov) {
				RendBG8C(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
			}
			else {
				RendBG8(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
			}
			return;
		}

		// ooo?[?uo?b?Noos
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		if (draw_force) {
			RendBG8FP(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG8P(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}

		// Line 8dot pattern
		len = render.mixlen - rest;
		x += rest;
		x &= (512 - 1);
		ASSERT((x & 7) == 0);
		if (draw_force) {
			RendBG8F(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else if (cmov) {
			RendBG8C(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		else {
			RendBG8(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}

		// o?
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			if (draw_force) {
				RendBG8FP(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
			}
			else {
				RendBG8P(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata,
					legacy_bg_transparency);
			}
		}
		return;
	}

	// 16x16o\o
	x = (render.bgx[page] - bg_hadjust) & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 7);

	// oooo`?F?b?N
	if ((x & 15) == 0) {
		// 16x16?Aooo?
		x >>= 4;
		if (draw_force) {
			RendBG16F(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else if (cmov) {
			RendBG16C(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		else {
			RendBG16(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
		}
		return;
	}

	// ooo?[?uo?b?Noos
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	if (draw_force) {
		RendBG16FP(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}
	else {
		RendBG16P(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
	}

		// Line 16dot pattern
	len = render.mixlen - rest;
	x += rest;
	x &= (1024 - 1);
	ASSERT((x & 15) == 0);
	if (draw_force) {
		RendBG16F(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}
	else if (cmov) {
		RendBG16C(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
	}
	else {
		RendBG16(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata, legacy_bg_transparency);
	}

	// o?
	if (len & 15) {
		x += (len & 0xfff0);
		x &= (1024 - 1);
		x >>= 4;
		if (draw_force) {
			RendBG16FP(&ptr[x << 1], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG16P(&ptr[x << 1], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata,
				legacy_bg_transparency);
		}
	}
}

//---------------------------------------------------------------------------
//
//	BGBlock
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

	// o?W?X?^?|?Co?^oo
	reg = &render.bgreg[page][y << 6];

	// BG?|?Co?^oo
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 11);
	}
	else {
		ptr += (y << 10);
	}

	// o?[?v
	for (i=0; i<64; i++) {
		// Copy
		bgdata = reg[i];

		// $10000ooooooOK
		if (bgdata & 0x10000) {
			ptr += 2;
			continue;
		}

		// $10000oOR
		reg[i] |= 0x10000;

		// pcgnooo
		pcgno = bgdata & 0xfff;

		// Type
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x8000) {
				// oo?]
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// o?
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 2048;
		}
		else {
			// 8x8?Bbit17,bit18olooo
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// oo?]
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// Pcgbuf
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = reinterpret_cast<DWORD*>(static_cast<size_t>(bgdata));
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 1024;
		}

	// Pattern (PCG)
		render.pcguse[ pcgno ]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[ pcgno ]++;

		// Pointer update
		ptr += 2;
	}
}

//===========================================================================
//
//	 Render Mix
//
//===========================================================================

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

	// oo?wooooo?Aoo?o?b?t?@oooo?Ay?I?[?o?[o?return
	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);


	// Original compositor: if half-transparency / special priority is active,
	// use the px68k-style scanline compositor so effects like transparent clouds match.
	 {
		const VC::vc_t *vp = (vc ? vc->GetWorkAddr() : NULL);
		if (vp) {
			const BYTE vr2h = (BYTE)vp->vr2h;
			const BOOL tr_mode = (BOOL)((vr2h & 0x5d) == 0x1d);
			const BOOL dim_mode = (BOOL)((vr2h & 0x5d) == 0x1c);
			const BOOL pri_mode = (BOOL)((vr2h & 0x5c) == 0x14);
			if (transparency_enabled &&
				(vp->hp || vp->exon || vp->gg || vp->gt || vp->ah || vp->vht || tr_mode || dim_mode || pri_mode)) {
				MixFastLine(y, y);
				return;
			}
		}
	}

#if defined(REND_LOG)
	LOG1(Log::Normal, "???? y=%d", y);
#endif	// REND_LOG

	// Flag off and copy buffer and address calc
	render.mix[y] = FALSE;
	q = &render.mixbuf[render.mixwidth * y];

	switch (render.mixtype) {
		// Type 0 (display off)
		case 0:
			RendMix00(q, render.drawflag + (y << 6), render.mixlen);
			return;

		// Type 1 (1 page)
		case 1:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			RendMix01(q, p, render.drawflag + (y << 6), render.mixlen);
			return;

		// Type 2 (2 and plane 0 only)
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

		// Type 3 (2 or only)
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

		// Type 4 (graphic 3 or 4 page)
		case 4:

			MixGrp(y, buf);
			RendMix01(q, buf, render.drawflag + (y << 6), render.mixlen);
			break;

		// Type 5 (graphic + text and text do over)
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

		// Type 6 (graphic + text and graphic do over)
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

		// Type 7 (text + sprite + graphic 1 page)
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

		// Type 8 (text + sprite + graphic 2+ page)
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

		// oo?
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	MixGrp
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
		// render.mixpage
		case 0:
			ASSERT(FALSE);
			return;

		// render.mixpage
		case 1:
			ASSERT(FALSE);
			return;

		// 2o
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

		// 3o
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

		// 4o
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

		// oo?
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	GetMixBuf
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULLoooo
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	Process
//
//---------------------------------------------------------------------------
void FASTCALL Render::Process()
{
	int i;

	// Transparency
	if (render.first >= render.last) {
		return;
	}

	if (compositor_mode == compositor_fast) {
		ProcessFast();
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?r?f?I????");
#endif	// RENDER_LOG
		Video();
	}

	// Contrast
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// Palette
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0oA?X?vo?C?go\oON/OFFoo?
	if (render.first == 0) {
		if (sprite->IsDisplay()) {
			if (!render.bgspdisp) {
				// Sprite CPU to Video
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = TRUE;
			}
		}
		else {
			if (render.bgspdisp) {
				// Sprite Video to CPU
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = FALSE;
			}
		}
	}

	// oox2o?
	if ((render.v_mul == 2) && !render.lowres) {
		// I/Ooog?�E��E�ooA?cooooooooo
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
		// Render
		render.first = render.last;
		return;
	}

	// Pointer update
	if ((render.v_mul == 0) && render.lowres) {
		// Lowres (2 line mode)
		for (i=render.first; i<render.last; i++) {
			// Text and Graphic
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
		// Render
		render.first = render.last;
		return;
	}

	// Loop
	for (i=render.first; i<render.last; i++) {
		Text(i);
		Grp(0, i);
		Grp(1, i);
		Grp(2, i);
		Grp(3, i);
		BGSprite(i);
		Mix(i);
	}
	// Render
	render.first = render.last;
}









