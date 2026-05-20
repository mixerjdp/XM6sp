//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 P.I. (ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Renderer ]
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
#if XM6_RENDER_SYNC == 2
#include "mfc_com.h"
#include "mfc_sch.h"
#endif	// XM6_RENDER_SYNC == 2

//=========================================================================
//
//	Renderer
//
//=========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	Constant definitions
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// Color 0 flag (used by rend_asm.asm)
#define REND_COLORS		0x40000000		// Special flag (semi-transparent/special priority)
#define REND_COLORT		0x40000000		// Special flag (text palette 0)

class Render::Backend
{
public:
	explicit Backend(int m) : mode(m)
	{
	}

	void Activate(Render *owner)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->InvalidateFrame();
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

	void HSync(Render *owner, int raster, int xoffset)
	{
		if ((mode == Render::compositor_fast) && owner) {
			owner->HSyncFast(raster);
			return;
		}
		owner->HSyncOriginal(raster, xoffset);
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
/// Constructor
//
//---------------------------------------------------------------------------
Render::Render(VM* p) : Device(p)
{
	// Initialize the device ID
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// devicepointer
	crtc = NULL;
	vc = NULL;
	sprite = NULL;
	memset(&px68k_crtc_host, 0, sizeof(px68k_crtc_host));
	memset(&px68k_crtc_state_cache, 0, sizeof(px68k_crtc_state_cache));
	render_fast_dummy_enabled = FALSE;
	backend = NULL;
	backend_original = NULL;
	backend_fast = NULL;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));
	transparency_enabled = TRUE;
	original_bg0_render_enabled = TRUE;
	compositor_mode = compositor_original;
#if XM6_RENDER_SYNC == 2
	m_pScheduler = NULL;
#endif	// XM6_RENDER_SYNC == 2

	// Initialize the state area (CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

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

	// Initialize the state area (palette)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// Initialize the state area (text)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// Initialize the state area (graphics)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// Initialize the state area (PCG, sprite, BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = 0;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// Initialize the state area (composition)
	render.mixbuf = NULL;
	render.mixwidth = 0;
	render.mixheight = 0;
	render.mixlen = 0;
	render.mixtype = 0;
	memset(render.mixptr, 0, sizeof(render.mixptr));
	memset(render.mixand, 0, sizeof(render.mixand));
	memset(render.mixrshift, 0, sizeof(render.mixrshift));
	memset(render.mixlshift, 0, sizeof(render.mixlshift));
	memset(render.mixx, 0, sizeof(render.mixx));
	memset(render.mixy, 0, sizeof(render.mixy));
	memset(render.mixandx, 0, sizeof(render.mixandx));
	memset(render.mixandy, 0, sizeof(render.mixandy));
	memset(render.mixmap, 0, sizeof(render.mixmap));

	// Initialize the state area (draw)
	memset(render.draw, 0, sizeof(render.draw));
	render.drawflag = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTCget
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// Get the CRTC state address
	cp = crtc->GetWorkAddr();

	// VCget
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// Get the VC state address
	vp = vc->GetWorkAddr();

	px68k_crtc_host.ctx = this;

#if LOCAL_EXCEPTION
	try {
#endif	// LOCAL_EXCEPTION

	// Allocate the palette buffer (4 MB)
	render.palbuf = new DWORD[0x10000 * 16];
	if (!render.palbuf) {
		return FALSE;
	}

	// Allocate the text VRAM buffer (4.7 MB)
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

	// Allocate the graphics VRAM buffer (8.2 MB)
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

	// Allocate the PCG buffer (4 MB)
	render.pcgbuf = new DWORD[16 * 256 * 16 * 16];
	if (!render.pcgbuf) {
		return FALSE;
	}

	// spritepointerallocate(512KB)
	render.spptr = new DWORD*[128 * 1024];
	if (!render.spptr) {
		return FALSE;
	}

	// BGpointerallocate(768KB)
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

	// Allocate the BG/sprite buffer (4 MB)
	render.bgspbuf = new DWORD[1024 * 1024];
	if (!render.bgspbuf) {
		return FALSE;
	}

	// Allocate the draw-flag buffer (256 KB)
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

	// Build the palette
	MakePalette();

	// Other state areas
	render.contlevel = 0;
	render.contvalue = 0;

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
	render_fast_dummy_enabled = FALSE;

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

	// drawflag
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/sprite buffer
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BGpointer
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// spritepointer
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCG buffer
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// Graphics VRAM buffer
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

	// Palette buffer
	if (render.palbuf) {
		delete[] render.palbuf;
		render.palbuf = NULL;
	}

	// Return to the base class
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
	bgdata_t *ptr;

	ASSERT(this);
	LOG0(Log::Normal, "リセット");

	// Get the pointer from the video controller
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// Get the pointer from text VRAM
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// Get the pointer from graphics VRAM
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// Get the pointer from the sprite controller
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// Initialize the state area
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = (compositor_mode == compositor_fast) ? 0 : 2;
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	// Initialize the state area (CRTC, VC) -- TRUE to force recalculation
	render.crtc = TRUE;
	render.vc = TRUE;

	// Initialize the state area (contrast) -- TRUE to force recalculation
	render.contrast = TRUE;

	// Initialize the state area (palette) -- TRUE to force recalculation
	render.palette = TRUE;
	render.palptr = render.palbuf;

	// Initialize the state area (text)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// Initialize the state area (graphics)
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

	// Initialize the state area (PCG)
	// Immediately after reset, neither BG nor sprites are displayed, so PCG is unused
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// Initialize the state area (sprites)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 1024);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// Initialize the state area (BG)
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

	// Initialize the state area (BG/sprite)
	render.bgsp = TRUE;
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// Build the initial BG state (all 0000)
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

	// Initialize the state area (composition)
	render.mixtype = 0;
}

//---------------------------------------------------------------------------
//
//	Save
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
//	Load
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
//	Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "設定適用");

	// 15 kHz scanline display
	render.scanline = config->disp_scanline;
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	Frame start
//
//---------------------------------------------------------------------------
void FASTCALL Render::StartFrame()
{
	ASSERT(this);
	if (backend) {
		backend->StartFrame(this);
	}
}

void FASTCALL Render::StartFrameOriginal()
{
	ASSERT(this);


	// Whether to skip this frame
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// Render this frame
	render.act = TRUE;

	// Clear the raster
	render.first = 0;
	render.last = 0;

	// CRTC
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC処理");
#endif	// RENDER_LOG
		Crtc();
	}

	// Contrast
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "コントラスト処理");
#endif	// RENDER_LOG
		Contrast();
	}

	// Update the composition even/odd state for interlace mode
	render.mixeven = cp->v_scaneven;
}

//---------------------------------------------------------------------------
//
//	Frame end
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrame()
{
	ASSERT(this);
	if (backend) {
		backend->EndFrame(this);
	}
}

void FASTCALL Render::EndFrameOriginal()
{
	int i;
	ASSERT(this);


	// Do nothing if invalid
	if (!render.act) {
		return;
	}

	// Process rasters up to this point
	if (render.last > 0) {
		for (i=render.last; i<render.height; i++) {
			Process(i, 0);
		}
	}

	// Increment the count
	render.count++;

	// Invalidate
	render.act = FALSE;

#if XM6_RENDER_SYNC == 2
	// Draw
	ASSERT(m_pScheduler);
	m_pScheduler->UpdateFrame();
#endif	// XM6_RENDER_SYNC == 2
}

//---------------------------------------------------------------------------
//
//	Set the composition buffer
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// set
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// Adjust the composition buffer processing length
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// Mark all composition layers for update
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	Set CRTC
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTC()
{
	ASSERT(this);
	if (backend) {
		backend->SetCRTC(this);
	}
}

void FASTCALL Render::SetCRTCOriginal()
{
	ASSERT(this);
	render.crtc = TRUE;
}

//---------------------------------------------------------------------------
//
//	Set VC
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVC()
{
	ASSERT(this);
	if (backend) {
		backend->SetVC(this);
	}
}

void FASTCALL Render::SetVCOriginal()
{
	ASSERT(this);
	render.vc = TRUE;
}

//---------------------------------------------------------------------------
//
//	CRTC processing
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

	// Get data
	sprite->GetSprite(&sprdata);

	// Defer processing if h_dots or v_dots is 0
	if ((cp->h_dots == 0) || (cp->v_dots == 0)) {
		return;
	}

	// Copy the CRTC info
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

	// Determine the horizontal frequency mode
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

	// Set up CRT-emulation info
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

	// If interlace mode or the horizontal-frequency mode
	// Adjust because 15 kHz mode is composed at double vertical size
	if (cp->v_mul == 0 || render.hres == 0) {
		render.v_disp <<= 1;
		render.v_total <<= 1;
		render.v_pulse <<= 1;
		render.v_start <<= 1;
	}

	// Adjust the composition buffer processing length
	mixlen = render.mixlen;
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// Do not compose if mixlen is invalid
	if (render.mixlen==0) {
		render.act = FALSE;
	}

	// BG/sprite position adjustment
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

	// Composition mode
	if (render.lowres) {
		// 15kHz
		if (render.vd == 1) {
			// Interlace
			render.mixmode = 1;
		} else {
			// normal
			render.mixmode = 0;
		}
	} else {
		// 31kHz
		if (render.vd == 0) {
			// Double read
			render.mixmode = 2;
		} else if (render.vd == 1) {
			// normal
			render.mixmode = 0;
		} else {
			// Interlace
			render.mixmode = 1;
		}
	}

	// Composition mode (BG, sprite)
	if (render.bgsp_lowres) {
		// 15kHz
		if (render.bgsp_vres == 0) {
			// normal
			render.bgsp_mixmode = 0;
		} else {
			// Interlace
			render.bgsp_mixmode = 1;
		}
	} else {
		// 31kHz
		if (render.bgsp_vres == 0) {
			// Double-read mode
			render.bgsp_mixmode = 2;
		} else {
			// normal
			render.bgsp_mixmode = 0;
		}
	}

	// Raster misalignment validation processing
	if (sprdata.h_res != 0) {
		// Build the index
		index = render.hd;
		if (!render.lowres) {
			index += 4;
		}
		if (render.hrl) {
			index += 8;
		}

		// Compare against the validation table
		if (sprdata.h_disp > HDispTable[index]) {
			render.bgsp_v++;
		}
	}

	// Reset sprites (depends on mixlen, bgsp_h, and bgsp_v)
	if (mixlen != render.mixlen ||
		bgsp_h != render.bgsp_h || bgsp_v != render.bgsp_v) {
		SpriteReset();
	}

	// Off
	render.crtc = FALSE;

	// Notify VC
	SetVC();
}

//---------------------------------------------------------------------------
//
//	Raster misalignment validation table
//	Values derived from lowres, hd, and HRL.
//
//	If BGSP H-DISP reaches or exceeds this value, the BGSP plane shifts upward by one raster
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
//	VC processing
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

	// Clear the VC flag
	render.vc = FALSE;

	// flagON
	render.mixdirty = TRUE;
	render.textdirty = TRUE;
	render.grpdirty = TRUE;
	render.bgspdirty = TRUE;

	// Text enable
	if (vp->ton && !cp->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// Graphics type
	type = 0;
	if (!vp->siz) {
		type = (int)(vp->col + 1);
	}
	if (type != render.grptype) {
		render.grptype = type;
	}


	// Graphics composition type
	render.mixpage = 0;
	for (i=0; i<4; i++) {
		render.grppen[i] = FALSE;
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


	// Initialize the shift amount used for overall composition position adjustment
	mixlshift = 0;
	mixrshift = 0;

	// For double-scanning, use half position
	if (render.mixmode == 2) {
		mixrshift = 1;
	}

	// Initialize the shift amount used for BG/sprite composition
	render.bgsp_rshift = 0;
	render.bgsp_lshift = 0;

	// Use half position only when BG/SP is normal and interlaced
	if (render.bgsp_mixmode == 0 && render.mixmode == 1) {
		render.bgsp_rshift = 1;
	}

	// If BG/SP is interlaced, double the position only in non-interlace mode
	if (render.bgsp_mixmode == 1 && render.mixmode != 1) {
		render.bgsp_lshift = 1;
	}

	// With double-scanning BG/SP, choose half or quarter position based on interlace state
	if (render.bgsp_mixmode == 2) {
		if (render.mixmode != 1) {
			render.bgsp_rshift = 1;
		} else {
			render.bgsp_rshift = 2;
		}
	}

	// Set the graphics buffer
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

	// Set the graphics state
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

	// Check graphics scroll and display layout
	if (render.grpdirty) {
		GrpScrlCheck();
		GrpDispCheck();
	}

	// Get the priority
	render.tx = vp->tx;
	render.sp = vp->sp;
	render.gr = vp->gr;

	// Initialize the type
	render.mixtype = 0;


	// special handling
	//
	// If VC R1 sets the graphics-screen priority to the undefined value %11
	// both text and BGSP appear to disappear
	//
	if (render.gr == 3) {
		if (render.mixpage == 0) {
			// graphicsnone(type=0)
			render.mixtype = 0;
			return;
		}
		// Only when at least one graphics plane is enabled (type=3)
		render.mixtype = 3;
		return;
	}


	// Whether BG/sprite display is being toggled
	if ((cp->hd >= 2) || (!vp->son)) {
		if (render.bgspflag) {
			// BG/spritedisplayON->OFF
			render.bgspflag = FALSE;
		}
	} else {
		if (!render.bgspflag) {
			// BG/spritedisplayOFF->ON
			render.bgspflag = TRUE;
		}
	}

	// Palette 0 semi-transparency requires special handling
	if (vp->ah) {
		// Palette 0 semi-transparency (type=11)
		if (render.mixpage > 0) {
			render.mixtype = 11;
		} else {
			render.mixtype = 0;
		}
		return;
	}

	// Graphics only
	if (!render.texten && ((cp->hd >= 2) || (!vp->son))) {
		if (render.mixpage == 0) {
			// graphicsnone(type=0)
			render.mixtype = 0;
			return;
		}
		// Only when at least one graphics plane is enabled (type=3)
		render.mixtype = 3;
		return;
	}

	// Text only
	if (render.mixpage == 0 && render.texten && ((cp->hd >= 2) || (!vp->son))) {
		// No graphics; text only (type=1)
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

	// Sprites only
	if (render.mixpage == 0 && cp->hd < 2 && vp->son && !render.texten) {
		// No graphics; sprites only (type=2)
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

	// Determine the priority order
	if (render.tx == 3)
		render.tx--;
	if (render.sp == 3)
		render.sp--;
	if (render.gr == 3)
		render.gr--;

	if (render.tx == render.sp) {
		// Chosen heuristically
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
		// Chosen heuristically
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
		// Chosen heuristically
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


	// Graphics and text only (types 5, 6, 7)
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

		// If text is in the back layer and graphics are in the middle layer, treat graphics as the front layer (type=5)
		if (render.tx==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// Graphics and sprites only (types 5, 6, 7)
	if (render.mixpage > 0 && cp->hd < 2 && vp->son && !render.texten) {
		// No graphics; sprites only (type=2)
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

		// If sprites are in the back layer and graphics are in the middle layer, treat graphics as the front layer (type=5)
		if (render.sp==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// Set text and sprite composition
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

	// Sprites and text (type=4)
	if (render.mixpage == 0) {
		render.mixtype = 4;
		return;
	}

	// Graphics, text, and sprites (types 8, 9, 10)
	render.mixtype = 8 + render.gr;
}

//---------------------------------------------------------------------------
//
//	Set contrast
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont, BOOL immediate)
{
	// Since the system port already checks for a match, only handle differing values here
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// Apply the change and raise the flag
	render.contlevel = cont << 2;
	render.contrast = TRUE;

	// If the change is applied immediately, do not transition gradually
	if (immediate) {
		render.contvalue = cont << 2;
	}
}

//---------------------------------------------------------------------------
//
//	Get contrast
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
//	Contrast processing
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	// The contrast reached the target value, so clear the flag
	if (render.contlevel == render.contvalue) {
		render.contrast = FALSE;
	} else {
		// Gradually move the current value toward the target value
		if (render.contlevel > render.contvalue) {
			render.contvalue = (render.contlevel < (render.contvalue + 2)) ? render.contlevel : (render.contvalue + 2);
		} else {
			render.contvalue = (render.contlevel > (render.contvalue - 2)) ? render.contlevel : (render.contvalue - 2);
		}
	}

	// Change the point position and lower the flag
	render.palptr = render.palbuf;
	render.palptr += (render.contvalue >> 2) << 16;

	// Raise all palette flags
	memset(render.palmod, TRUE, 0x200);
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	Build the palette
//
//---------------------------------------------------------------------------
void FASTCALL Render::MakePalette()
{
	DWORD *p;
	int ratio;
	int i;
	int j;

	ASSERT(render.palbuf);

	// Initialize
	p = render.palbuf;

	// Contrast loop
	for (i=0; i<16; i++) {
		// Calculate the ratio
		ratio = 256 - ((15 - i) * 17);

		// Build loop
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Palette conversion
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

	// Copy everything
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// Bits are ordered from MSB as G:5, R:5, B:5, I:1
	// Convert this to an R:8 G:8 B:8 DWORD; b31-b24 are unused
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// Always raise the intensity bit (even when the source data is 0, this still forces !=0)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// Apply contrast
	b *= ratio;
	b >>= 8;
	g *= ratio;
	g >>= 8;
	g &= 0xff00;
	r *= ratio;
	r >>= 8;
	r &= 0xff0000;

	// composition
	c = (DWORD)(r | g | b);

	// Final check for the intensity bit being ON
	// Even when the source data is 0 or contrast computation yields 0
	// the !=0 behavior must be preserved so layered composition can still distinguish it
	if (color & 1) {
		if (color == 1 || c == 0) {
			c = 0x010101;
		}
	}

	return c;
}

//---------------------------------------------------------------------------
//
//	paletteget
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
//	Palette processing
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

	// flagOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// Graphics
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// Affects graphics; clear the flag
			gr = TRUE;
			render.palmod[i] = FALSE;

			// Transparent-color processing
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// Set palette data for 65536-color mode
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2    ] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;

			// Set palette data for semi-transparent/special-priority mode

			// GVRAM-selected
			if ((i & 1) == 0) {
				render.paldataGB[i    ] = render.paldata[i];
				render.paldataGB[i + 1] = render.paldata[i] | REND_COLORS;
				render.paldataGS[i    ] = render.paldata[i];
				render.paldataGS[i + 1] = render.paldata[i];
			}

			// Palette-selected
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

	// Text and sprite
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// Affects text; clear the flag
			tx = TRUE;
			render.palmod[i] = FALSE;

			// Transparent-color processing
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				render.paldata[i] |= REND_COLORT;
				// 0x100 always affects BG and sprites as well
				sp = TRUE;

				// 0x100 may also affect graphics semi-transparency
				gr = TRUE;
			}

			// PCG check
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// sprite
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// Affects sprites; clear the flag
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// Transparent-color processing
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCG check
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// graphicsflag
	if (gr) {
		// flagON
		render.grpdirty = TRUE;
	}

	// textflag
	if (tx) {
		render.textdirty = TRUE;
	}

	// spriteflag
	if (sp) {
		render.bgspdirty = TRUE;
	}

	// paletteflagOFF
	render.palette = FALSE;
}

//---------------------------------------------------------------------------
//
//	textscroll
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// Comparison check
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// stateupdate
	render.textx = x;
	render.texty = y;

	// flagON
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "テキストスクロール x=%d y=%d", x, y);
#endif	// REND_LOG

		render.mixdirty = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Copy text
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// Assembly helper
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	Get the text buffer
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
//	Text conversion
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

	// Clear the composition flag
	mixflag = FALSE;

	// Calculate the actual screen Y coordinate
	y = (offset + render.texty) & 0x3ff;

	// Change flag (incremental mode)
	if (render.textmod[y]) {
		// flagprocessing
		render.textmod[y] = FALSE;
		mixflag = TRUE;

		// Horizontal/vertical conversion
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// Vertical palette conversion
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// Palette (bulk mode)
	if (render.textpal[y]) {
		// flagprocessing
		render.textpal[y] = FALSE;

		// Vertical palette conversion
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		mixflag = TRUE;

		// Copy when y == 1023
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}

	return mixflag;
}

//---------------------------------------------------------------------------
//
//	textprocessing
//
//---------------------------------------------------------------------------
void FASTCALL Render::Text(int raster)
{
	int offset;
	BOOL mixflag;

	// Do nothing if disabled
	if (!render.texten) {
		return;
	}

	// Calculate the offset
	offset = raster;

	// Double-scanning uses half position
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// Convert the current offset
	mixflag = TextConv(offset);

	// If updated, raise the flag for the current raster
	if (mixflag) {
		render.mix[raster] = TRUE;
	}

	// If scrolling is active, the next offset also
	// can become visible, so compose it here
	if (render.textx > 0 && offset < 1023) {
		mixflag = TextConv(offset + 1);

		// If updated, raise the flag for the current raster
		if (mixflag) {
			render.mix[raster] = TRUE;
			render.mix[raster + 1] = TRUE;
		}

		// If updated, also raise the flag for the next raster
		if (mixflag) {
			if (render.mixmode == 2) {
				render.mix[raster + 2] = TRUE;
			} else {
				render.mix[raster + 1] = TRUE;
			}
		}
	}

	// Target the previous or next raster as well
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Get the graphics buffer
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
//	Set graphics scroll
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
{
	BOOL flag;

	ASSERT(this);
	ASSERT((block >= 0) && (block <= 3));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// Perform a comparison check; if not displayed, do not update
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpben[block];
	}

	// flagprocessing
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "グラフィックスクロール block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	// Raise the graphics scroll update flag
	render.grpscrl = TRUE;

	// compositionflagON
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	Graphics scroll check processing
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrlCheck()
{
	BOOL flag;
	int dx[4];
	int dy[4];
	int i;

	// Clear the flag
	render.grpscrl = FALSE;

	// scrollupdatevalidation
	flag = FALSE;

	// Relative-position checks apply to types 2 through 4
	switch(render.grptype) {
		// Type 0: 1024x1024 16 colors
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

		// Type 1: 512x512 16 colors
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

		// Type 2: 512x512 256 colors
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

		// Type 3:512x512 undefined
		case 3:
		// Type 4:512x512 65536Color
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

	// Check for updates
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

	// If the relative scroll value between blocks
	// changes, rebuild the buffer
	if (!flag) {
		return;
	}

	// Buffer rebuild required
	render.grpdirty = TRUE;

	// Check the display layout
	GrpDispCheck();
}

//---------------------------------------------------------------------------
//
//	Graphics display-layout check processing
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpDispCheck()
{
	// Inspect the layout from the display state and display blocks
	switch(render.grptype) {
		// Type 0: 1024x1024 16 colors
		case 0:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// Type 1: 512x512 16 colors
		case 1:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// Type 2: 512x512 256 colors
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

		// Type 3:512x512 undefined
		case 3:
		// Type 4:512x512 65536Color
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
//	Convert the GVRAM buffer layout
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

	// Compose the GVRAM buffer for each type
	switch(render.grptype) {
		// Type 0: 1024x1024 16 colors
		case 0:
			break;

		// Type 1: 512x512 16 colors
		case 1:
			break;

		// Type 2: 512x512 256 colors
		case 2:
			// Calculate the block, X/Y offsets, and address
			for (i=0; i<2; i++) {
				base = vp->gp[gd];
				block[i] = vp->gp[i+gd];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAMcomposition
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

		// Type 3:512x512 undefined
		case 3:
		// Type 4:512x512 65536Color
		case 4:
			// Calculate the block, X/Y offsets, and address
			for (i=0; i<4; i++) {
				base = vp->gp[0];
				block[i] = vp->gp[i];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAMcomposition
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
//	graphicsprocessing
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

	// Whether graphics are displayed
	if (render.mixpage == 0) {
		return;
	}

	// Determine composition by type
	if (render.grptype == 2) {
		// Type 2: 512x512 256 colors
		if (gd != 0 && gd != 2) {
			return;
		}
	} else if (render.grptype > 2) {
		// Type 3:512x512 undefined
		// Type 4:512x512 65536Color
		if (gd != 0) {
			return;
		}
	}

	// Calculate the offset
	offset = raster;

	// Double-scanning uses half position
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// Set the default palette
	paldata = render.paldata;

	// Set the palette (special function)
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

	// By type
	switch (render.grptype) {
		// Type 0: 1024x1024 16 colors
		case 0:
			// Calculate the block
			block = vp->gp[gd];

			// Calculate the offset
			offset = (offset + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// Check whether it is a display target
			if ((offset < 512) && (gd >= 2)) {
				return;
			}
			if ((offset >= 512) && (gd < 2)) {
				return;
			}

			// For palette mode, process the entire region
			if (render.grppal[y + (gd << 9)]) {
				render.grppal[y + (gd << 9)] = FALSE;
				render.grpmod[y + (gd << 9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				switch (block) {
					// Block 0
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// Block 1
					case 1:
						if (Rend1024C(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// Block 2
					case 2:
						if (Rend1024E(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// Block 3
					case 3:
						if (Rend1024G(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
				}
				break;
			}

			// Otherwise, process according to grpmod
			if (!render.grpmod[y + (gd << 9)]) {
				return;
			}
			render.grpmod[y + (gd << 9)] = FALSE;
			render.mix[raster] = TRUE;

			switch (block) {
				// Block 0
				case 0:
					Rend1024B(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// Block 1
				case 1:
					Rend1024D(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// Block 2
				case 2:
					Rend1024F(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// Block 3
				case 3:
					Rend1024H(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
			}

			break;

		// Type 1: 512x512 16 colors
		case 1:
			// Calculate the block
			block = vp->gp[gd];

			switch (block) {
				// Block 0
				case 0:
					y = (offset + render.grpy[0]) & 0x1ff;
					// Palette
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
					// normal
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// Block 1
				case 1:
					y = (offset + render.grpy[1]) & 0x1ff;
					// Palette
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
					// normal
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// Block 2
				case 2:
					y = (offset + render.grpy[2]) & 0x1ff;
					// Palette
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
					// normal
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// Block 3
				case 3:
					y = (offset + render.grpy[3]) & 0x1ff;
					// Palette
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
					// normal
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

		// Type 2: 512x512 256 colors
		case 2:
			// Calculate the block
			block = vp->gp[gd];

			// Calculate the offset
			y = (offset + render.grpy[block]) & 0x1ff;

			// For palette mode, process the entire region
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

			// Otherwise, process according to grpmod
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

		// Type 3:512x512 undefined
		case 3:
		// Type 4:512x512 65536Color
		case 4:
			// Calculate the block
			block = vp->gp[gd];

			// Calculate the offset
			y = (offset + render.grpy[block]) & 0x1ff;

			// For palette mode, process the entire region
			if (render.grppal[y + (gd<<9)]) {
				render.grppal[y + (gd<<9)] = FALSE;
				render.grpmod[y + (gd<<9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				if (render.grpnorm[0]) {
					grpgv = render.grpgv + (y << 10);
				} else {
					grpgv = MixGVRAM(buf, gd, offset);
				}

				// Special composition with semi-transparent/special-priority palette swapping
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

			// Otherwise, process according to grpmod
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

			// Special composition with semi-transparent palette swapping
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

	// Target the previous or next raster as well
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
	}
}

//=========================================================================
//
//	Renderer (BG/sprite section)
//
//=========================================================================

//---------------------------------------------------------------------------
//
//	Reset sprite registers
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	int i;
	DWORD addr;
	DWORD data[4];

	// Set sprite registers
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
//	Sprite register change
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

	// Indexing and data limits
	index = (int)(addr >> 3);

	// X(0~1023)
	data[0] &= 0x3ff;

	// Y(0~1023)
	data[1] &= 0x3ff;
	y=data[1];
//	y+=render.bgsp_v;
	data[1] = y;

	// V,H,PAL,PCG
	data[2] &= 0xcfff;

	// PRW(0,1,2,3)
	data[3] &= 0x0003;

	// ptrset
	ptr = &render.spptr[index << 10];

	// Register backup
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// Write to the registers
	next[0] = data[0];
	next[1] = data[1];
	next[2] = data[2];
	next[3] = data[3];

	// Check whether it will become valid
	use = TRUE;
	x = next[0];
	y = next[1];

	// Standard range check
	if (x == 0) {
		use = FALSE;
	}

	if (x >= (512 + 16)) {
		use = FALSE;
	}

	x -= render.bgsp_h;
	if (render.bgsp_h >= 0) {
		// Shift to the left
		if (x <= 0) {
			use = FALSE;
		}
		if (x >= (render.mixlen + 16)) {
			use = FALSE;
		}
	} else {
		// Shift to the right
		x &= 511;
		if (x >= (render.mixlen + 16)) {
			use = FALSE;
		}
	}

	y &= 0x3ff;

	if (next[3] == 0) {
		use = FALSE;
	}

	// If it was invalid and remains invalid, do nothing
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// It was valid before, so stop once
	if (render.spuse[index]) {
		// invalidprocessing(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[pcgno] > 0);
		render.pcguse[pcgno]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[pcgno] > 0);
		render.pcgpal[pcgno]--;

		// invalidprocessing(pointer)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			j &= 0x3ff;
			ptr[j] = NULL;
			render.bgspmod[j] = TRUE;
		}

		// If it becomes invalid, end here
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// Registration processing (usage flags)
	render.spuse[index] = TRUE;

	// Registration processing (PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[pcgno]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[pcgno]++;

	// Calculate the PCG address and set the pointer
	if (next[2] & 0x8000) {
		// Vertical flip
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
		// Normal
		for (i=0; i<16; i++) {
			j = (int)(next[1] - 16 + i);
			j &= 0x3ff;
			ptr[j] = &render.pcgbuf[offset];
			render.bgspmod[j] = TRUE;
			offset += 16;
		}
	}
}



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

	index = (int)(addr >> 3);
	switch ((addr & 7) >> 1) {
		case 0:
		case 1:
			data &= 0x3ff;
			break;
		case 2:
			data &= 0xcfff;
			break;
		case 3:
			data &= 0x0003;
			break;
	}

	ptr = &render.spptr[index << 9];
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	render.spreg[addr >> 1] = data;

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

	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	if (render.spuse[index]) {
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[pcgno] > 0);
		render.pcguse[pcgno]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[pcgno] > 0);
		render.pcgpal[pcgno]--;

		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	render.spuse[index] = TRUE;
	pcgno = next[2] & 0xfff;
	render.pcguse[pcgno]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[pcgno]++;

	if (next[2] & 0x8000) {
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

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// Compare and do nothing if it matches
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// update
	render.bgx[page] = x;
	render.bgy[page] = y;

	// Meaningless at 768x512
	if (!render.bgspflag) {
		return;
	}

	// If displaymiddle is set, raise BGSPMOD
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
//	BG control change
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

	// flagOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// By type
	switch (index) {
		// BG0 displayflag
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 displayflag
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 area change
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 area change
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BGsizechange
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
			// Display-state checks are handled in Process
			render.bgsp = TRUE;
			return;

		// Other (should never happen)
		default:
			ASSERT(FALSE);
			return;
	}

	// flagprocessing
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// Clear the currently used render.pcguse entries
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

			// Calculate the data address ($EBE000/$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// Copy 64x64 words; bit $10000 is always 0
			if (render.bgsize) {
				// 16x16 can be used as-is
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8 needs remapping: PCG(0-255)>>2 and move lost bits 0/1 to bits 17/18
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

			// Set bgall
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// For any change, raise bgspmod unless in 768x512 mode
	if (render.bgspflag) {
		render.bgspdirty = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	BG memory change
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

	// Page loop
	for (i=0; i<2; i++) {
		// Check whether it matches the data area for this page
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

		// Get the index (<64x64) and register pointer
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// Clear the previous pcguse entry
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[pcgno] > 0);
			render.pcguse[pcgno]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[pcgno] > 0);
			render.pcgpal[pcgno]--;
		}

		// Copy
		if (render.bgsize) {
			// 16x16 can be used as-is
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8 needs remapping: PCG(0-255)>>2 and move lost bits 0/1 to bits 17/18
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

		// Raise bgall
		render.bgall[i][index >> 6] = TRUE;

		// End unless displaymiddle is set; also end for page 1 when bgsize=1
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// Calculate from the scroll position and raise bgspmod
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
//	PCG memory change
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

	// Get the index
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// Clear render.pcgready
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// If render.pcguse > 0
	for (i=0; i<16; i++) {
		if (render.pcguse[index + (i << 8)] == 0) {
			continue;
		}

		// Cannot avoid it, so recompute BG/sprite composition
		render.bgspdirty = TRUE;
		break;
	}
}

//---------------------------------------------------------------------------
//
//	Get the PCG buffer
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
//	Get the BG/sprite buffer
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
//	BG/sprite
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

	// Assume a width up to 1024; this is also a prerequisite
	if (render.mixlen > 1024) return;

	// Calculate the offset
	offset = ((raster + render.bgsp_v) >> render.bgsp_rshift) << render.bgsp_lshift;
	offset &= 1023;

	// Check flags, offset, and the composition directive
	if (!render.bgspmod[offset]) {
		return;
	}
	render.bgspmod[offset] = FALSE;
	render.mix[raster] = TRUE;

	// In interlace mode, pair even and odd rasters
	if (render.mixmode == 1) {
		render.mix[raster ^ 1] = TRUE;
	}

	// For double-scanning in non-interlace mode, target the previous and next rasters
	if (render.bgsp_mixmode == 2 && render.mixmode != 1) {
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
	}

	// For double-scanning in interlace mode, target the previous and next three rasters
	if (render.bgsp_mixmode == 2 && render.mixmode == 1) {
		render.mix[(raster - 3) & 1023] = TRUE;
		render.mix[(raster - 2) & 1023] = TRUE;
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
		render.mix[(raster + 2) & 1023] = TRUE;
		render.mix[(raster + 3) & 1023] = TRUE;
	}

	// Clear the buffer
	// Fill here with text palette 0 (visible during loading)
	buf = &render.bgspbuf[offset << 10];
	RendClrSprite(buf, render.paldata[0x100],
		render.mixlen + 16 > 1024 ? 1024 : render.mixlen + 16);

	if (!render.bgspflag || !render.bgspdisp) {
		// End if not displayed
		return;
	}

	// Clear the sprite priority buffer
	memset(pri, 0xff, sizeof(pri));

	// Sprites behind one layer (PRW=1)
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// Use the middle layer
			if (reg[3] == 1) {
				// PRW=1
				if (*ptr) {
					// Display
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}

					// Adjust the composition reference position
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// Shift to the left
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// Shift to the right
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
		// Next sprite (SP0 is the frontmost)
		reg -= 4;
		ptr -= 1024;
	}

	// Display BG1
	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, offset, buf, TRUE);
	}

	// Sprites in the middle layer (PRW=2)
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// Use the middle layer
			if (reg[3] == 2) {
				// PRW=2
				if (*ptr) {
					// Display
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}

					// Adjust the composition reference position
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// Shift to the left
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// Shift to the right
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
		// Next sprite (SP0 is the frontmost)
		reg -= 4;
		ptr -= 1024;
	}

	// Display BG0
	if (render.bgdisp[0]) {
		if (render.bgdisp[1] && !render.bgsize) {
			BG(0, offset, buf, FALSE);
		} else {
			BG(0, offset, buf, TRUE);
		}
	}

	// Sprites in the front layer (PRW=3)
	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 10];
	ptr += offset;
	for (i=127; i>=0; i--) {
		if (render.spuse[i]) {
			// Use the middle layer
			if (reg[3] == 3) {
				// PRW=3
				if (*ptr) {
					// Display
					pcgno = reg[2] & 0xfff;
					if (!render.pcgready[pcgno]) {
						ASSERT(render.pcguse[pcgno] > 0);
						render.pcgready[pcgno] = TRUE;
						RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
					}

					// Adjust the composition reference position
					x = reg[0] - render.bgsp_h;
					if (render.bgsp_h >= 0) {
						// Shift to the left
						if (reg[0] <= 512) {
							RendSprite(*ptr, buf, x, reg[2] & 0x4000, i, pri);
						} else {
							RendSpriteP(*ptr, buf, x, reg[2] & 0x4000, i, pri, 528 - reg[0]);
						}
					} else {
						// Shift to the right
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
		// Next sprite (SP0 is the frontmost)
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

	// Do not display the position-adjustment area
	y = offset;

	// Extract the y block
	y = render.bgy[page] + offset;
	if (render.bgsize) {
		// 16x16 mode
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8 mode
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// If bgall is TRUE, this y block contains changed data
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// Display
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// Draw 8x8
		x = (render.bgx[page] + render.bgsp_h) & (512 - 1);
		ptr += (((render.bgy[page] + offset) & (512 - 1)) << 6);

		// Check whether it fits evenly
		if ((x & 7) == 0) {
			// 8x8, fits evenly
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

		// Process the first partial block
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		if (force) {
			RendBG8FP(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
		} else {
			RendBG8P(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
		}

		// Check the remainder and process it in 8-dot units
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

		// last
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

	// Draw 16x16
	x = (render.bgx[page] + render.bgsp_h) & (1024 - 1);
	ptr += (((render.bgy[page] + offset) & (1024 - 1)) << 6);

	// Check whether it fits evenly
	if ((x & 15) == 0) {
		// 16x16, fits evenly
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

	// Process the first partial block
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	if (force) {
		RendBG16FP(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
	} else {
		RendBG16P(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
	}

	// Check the remainder and process it in 16-dot units
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

	// last
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
//	BG (horizontal block reference)
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

	// Get the register pointer
	reg = &render.bgreg[page][y << 6];

	// Get the BG pointer
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 10);
	}
	else {
		ptr += (y << 9);
	}

	// loop
	for (i=0; i<64; i++) {
		// get
		bgdata = reg[i];

		// If $10000 is set, it is OK
		if (bgdata & 0x10000) {
			ptr += 1;
			continue;
		}

		// OR in $10000
		reg[i] |= 0x10000;

		// Get the PCG number
		pcgno = bgdata & 0xfff;

		// By size
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
			if (bgdata & 0x8000) {
				// Vertical flip
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// normal
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
			// Account for bits 17 and 18 in 8x8 mode
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// Vertical flip
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// normal
				for (j=0; j<8; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf += 0x10;
					ptr += 64;
				}
			}
			ptr -= 512;
		}

		// Registration processing (PCG)
		render.pcguse[pcgno]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[pcgno]++;

		// Advance the pointer
		ptr += 1;
	}
}

//---------------------------------------------------------------------------
//
//	BG/sprite check
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSpriteCheck(int raster)
{
	// Double-scanning uses even rasters only (Cotton)
	if (render.mixmode == 2 && raster & 1) {
		return;
	}

	// Clear the flag
	render.bgsp = FALSE;

	// Check whether sprite display is enabled
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

//=========================================================================
//
//	Renderer (composition section)
//
//=========================================================================

//---------------------------------------------------------------------------
//
//	composition
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

	// If there is no composition directive and Y overflows, return
	if (!render.mix[raster]) {
		return;
	}

	if (render.mixheight <= raster) {
		return;
	}

	// Determine the composition length
	mixlen = render.mixlen - (xoffset << 4);

	// No composition
	if (mixlen<=0) {
		return;
	}

#if defined(REND_LOG)
	LOG1(Log::Normal, "合成 raster=%d", raster);
#endif	// REND_LOG

	// flagOFF
	// Do not turn it off; writes during horizontal drawing require an update next time
	if (xoffset==0) {
		render.mix[raster] = FALSE;
	}

	// Buffer composition position
	mixy = raster;

	// 15 kHz and non-interlaced
 	if (render.mixmode != 1 && render.hres == 0) {
		mixy <<= 1;
	}

	// This line may need updating
	render.draw[mixy] = TRUE;

	// Initialize the composition buffer address
	mixbuf = &render.mixbuf[render.mixwidth * mixy];
	mixbuf += (xoffset << 4);
	mixflg = render.drawflag + (mixy << 6);
	mixflg += xoffset;

	switch (render.mixtype) {

		// Type 0 (do not display)
		case 0:
			RendMix00(mixbuf, mixflg, mixlen);
			break;

		// Type 1 (text only)
		case 1:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			RendMix01(mixbuf, ptr[0], mixflg, mixlen);
			break;

		// Type 2 (sprite only)
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

		// Type 3 (graphics only)
		case 3:
			ptr[2]=MixGrp(grpbuf, raster, xoffset, mixlen);

			// semi-transparent
			if (vp->exon && vp->hp && vp->gt) {
				RendMix01H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else {
				RendMix01(mixbuf, ptr[2], mixflg, mixlen);
			}
			break;

		// Type 4 (text, sprite)
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

		// Type 5 (text or sprite middle/back layer, graphics front layer)
		case 5:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// semi-transparent
				RendMix02BH(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			} else {
				// normal
				RendMix02B(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			}
			break;

		// Type 6 (text or sprite front layer, graphics middle layer, no back layer)
		case 6:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// semi-transparent
				RendMix02CH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// special priority
				RendMix02CS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// normal
				RendMix02C(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// Type 7 (text or sprite front/middle layer, graphics back layer)
		case 7:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// semi-transparent
				RendMix02DH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// special priority
				RendMix02DS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// normal
				RendMix02D(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// Type 8 (text, sprite, graphics front layer)
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
				// semi-transparent
				RendMix03AH(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			} else {
				// normal
				RendMix03A(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			}
			break;

		// Type 9 (text, sprite, graphics middle layer)
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
				// semi-transparent
				RendMix03BH(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// special priority
				RendMix03BS(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else {
				// normal
				RendMix03B(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			}
			break;

		// Type 10 (text, sprite, graphics back layer)
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
				// semi-transparent
				RendMix03CH(mixbuf, ptr[0], ptr[1], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// special priority
				RendMix03CS(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			} else {
				// normal
				RendMix03C(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			}
			break;

		// Type 11 (palette 0 semi-transparent)
		case 11:
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);
			RendMixP0H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			break;

		// other
		default:
			ASSERT(FALSE);
			break;
	}

	// At 15 kHz when not interlaced
 	if (render.mixmode != 1 && render.hres == 0) {
		if (render.scanline) {
			// Draw scanlines
			memset(mixbuf + render.mixwidth, 0x00, render.mixlen << 2);
		} else {
			// Raster copy
			memcpy(mixbuf + render.mixwidth, mixbuf, render.mixlen << 2);
		}

		// Also set the flag for the scanline raster
		memcpy(mixflg + 64, mixflg, (64 - xoffset) << 2);
		render.draw[mixy + 1] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Graphics composition
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

	// 0 planes (should never happen)
	if (render.mixpage==0) {
		ASSERT(FALSE);
		return buf;
	}

	// stateinitialize
	mixpage = 0;
	for (i=0; i<4; i++) {
		map[i] = -1;
	}

	// Calculate the buffer
	j = 0;
	for (i=0; i<render.mixpage; i++) {
		idx = i + 4;

		y = *render.mixy[idx];
		y += (((raster + render.mixraster[idx]) >> render.mixrshift[idx]) << render.mixlshift[idx]);
		y &= render.mixandy[idx];
		ptr[i] = render.mixptr[idx];
		ptr[i] += (y << render.mixshift[idx]);
		ptr[i] += (*render.mixx[idx] + (xoffset << 4)) & render.mixandx[idx];

		// Count the number of actually displayed pages
		if (render.grppen[i]) {
			mixpage++;
			map[j++] = i;
		}
	}

	// 0-plane composition (no composition)
	if (mixpage==0) {
		return buf;
	}

	// Display 1 page
	if (render.mixpage == 1 && mixpage==1) {
		memcpy(buf, ptr[map[0]], mixlen << 2);
	}

	// 2 pages or more
	if (render.mixpage >= 2) {

		// Check semi-transparency (2+ pages with the base page displayed)
		if (vp->exon && vp->hp && vp->gg && render.grppen[0]) {
			if (render.grppen[1]) {
				// Base display, second display
				RendGrp02HBS(buf, ptr[0], ptr[1], mixlen);

				if (mixpage==3) {
					RendGrp02(buf, buf, ptr[map[2]], mixlen);
				}

				if (mixpage==4) {
					RendGrp03(buf, buf, ptr[map[2]], ptr[map[3]], mixlen);
				}
			} else {
				// Base display, second hidden
				// If the second page is hidden, compose only the semi-transparent area
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
				// Display 1 page
				memcpy(buf, ptr[map[0]], mixlen << 2);
			} else if (mixpage==2) {
				// Compose 2 pages
				RendGrp02(buf, ptr[map[0]], ptr[map[1]], mixlen);
			} else if (mixpage==3) {
				// Compose 3 pages
				RendGrp03(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], mixlen);
			} else {
				// Compose 4 pages
				RendGrp04(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], ptr[map[3]], mixlen);
			}
		}
	}

	// return
	return buf;
}

//---------------------------------------------------------------------------
//
//	Get the composition buffer
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// May also be NULL
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	Composition (forced)
//
//---------------------------------------------------------------------------
void FASTCALL Render::UpdateMixBuf()
{
	BOOL bScanEven;
	int i;
	int j;

	ASSERT(this);

	// Get the even/odd scanline for interlace mode
	bScanEven = cp->v_scaneven;

	// Compose twice in preparation for interlace mode
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
//	Rendering
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
	if (backend) {
		backend->HSync(this, raster, xoffset);
	}
}

void FASTCALL Render::HSyncOriginal(int raster, int xoffset)
{
	ASSERT(this);
	render.last = raster + 1;
	if (render.act) {
		Process(raster, xoffset);
	}
}

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
	render_fast_dummy_enabled = (mode == compositor_fast) ? TRUE : FALSE;
	if (next) {
		backend = next;
		backend->Activate(this);
	}
	ForceRecompose();
	return TRUE;
}

BOOL FASTCALL Render::SetRenderFastDummyEnabled(BOOL enable)
{
	SetCompositorMode(enable ? compositor_fast : compositor_original);
	return render_fast_dummy_enabled;
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

void FASTCALL Render::InvalidateFrame()
{
	int i;
	DWORD stamp;

	render.vc = TRUE;
	render.palette = TRUE;
	render.textdirty = TRUE;
	render.grpdirty = TRUE;
	render.bgspdirty = TRUE;
	render.mixdirty = TRUE;
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

void FASTCALL Render::ApplyPendingCompositorMode()
{
}

void FASTCALL Render::ForceRecompose()
{
	render.crtc = TRUE;
	InvalidateFrame();
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
	(void)addr;
	(void)data;
}

BYTE FASTCALL Render::TVRAMRead(DWORD addr)
{
	(void)addr;
	return 0xff;
}

void FASTCALL Render::TVRAMWrite(DWORD addr, BYTE data)
{
	(void)addr;
	(void)data;
}

BYTE FASTCALL Render::GVRAMRead(DWORD addr)
{
	(void)addr;
	return 0xff;
}

void FASTCALL Render::GVRAMWrite(DWORD addr, BYTE data)
{
	(void)addr;
	(void)data;
}

BYTE FASTCALL Render::BGRead(DWORD addr)
{
	(void)addr;
	return 0xff;
}

void FASTCALL Render::CRTCRegWrite(DWORD addr, BYTE data)
{
	(void)addr;
	(void)data;
}

BYTE FASTCALL Render::CRTCRegRead(DWORD addr)
{
	(void)addr;
	return 0xff;
}

BYTE FASTCALL Render::VCtrlRead(DWORD addr)
{
	(void)addr;
	return 0xff;
}

void FASTCALL Render::VCtrlWrite(DWORD addr, BYTE data)
{
	(void)addr;
	(void)data;
}

void FASTCALL Render::GVRAMFastClear()
{
}
void FASTCALL Render::Process(int raster, int xoffset)
{
	// Safety measure
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

	// Palette
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "パレット処理");
#endif	// RENDER_LOG
		Palette();
	}

	// Check graphics scroll
	if (render.grpscrl) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "グラフィックスクロールチェック処理");
#endif	// RENDER_LOG
		GrpScrlCheck();
	}

	// Check BG/sprite
	if (render.bgsp) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "BG/スプライトチェック処理");
#endif	// RENDER_LOG
		BGSpriteCheck(raster);
	}

	// textupdate
	if (render.textdirty) {
		memset(render.textpal, TRUE, 1024);
		render.textdirty = FALSE;
	}

	// graphicsupdate
	if (render.grpdirty) {
		memset(render.grppal, TRUE, 512 * 4);
		render.grpdirty = FALSE;
	}

	// BGspriteupdate
	if (render.bgspdirty) {
		memset(render.bgspmod, TRUE, 1024);
		render.bgspdirty = FALSE;
	}

	// Update the entire composition
	if (render.mixdirty) {
		memset(render.mix, TRUE, render.mixheight);
		render.mixdirty = FALSE;
	}

	// If interlaced, bulk-compose even and odd rasters
	if (render.mixmode == 1) {
		raster <<= 1;
		if (render.mixeven) {
			// Run composition (even lines)
			Text(raster);
			Grp(0, raster);
			Grp(1, raster);
			Grp(2, raster);
			Grp(3, raster);
			BGSprite(raster);
			Mix(raster, xoffset);

			// Run composition (odd lines)
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

	// compositionexecute
	Text(raster);
	Grp(0, raster);
	Grp(1, raster);
	Grp(2, raster);
	Grp(3, raster);
	BGSprite(raster);
	Mix(raster, xoffset);
}
