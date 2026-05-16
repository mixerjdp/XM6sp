//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ ƒŒƒ“ƒ_ƒ‰ ]
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
//	ƒŒƒ“ƒ_ƒ‰
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	’è”’è‹`
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// ƒJƒ‰[0ƒtƒ‰ƒO(rend_asm.asm‚Åg—p)
#define REND_COLORS		0x40000000		// “Áêƒtƒ‰ƒO(”¼“§–¾^“Áêƒvƒ‰ƒCƒIƒŠƒeƒB)
#define REND_COLORT		0x40000000		// “Áêƒtƒ‰ƒO(ƒeƒLƒXƒgƒpƒŒƒbƒg‚O)

static int FASTCALL CalcBGHAdjustPixels(int compositor_mode, const CRTC *crtc, const Sprite *sprite)
{
	(void)compositor_mode;
	if (!crtc || !sprite) {
		return 0;
	}

	const CRTC::crtc_t *crtc_state = crtc->GetWorkAddr();
	if (!crtc_state) {
		return 0;
	}

	Sprite::sprite_t spr;
	sprite->GetSprite(&spr);
	const int bg_hdisp = (int)(spr.h_disp & 0xff);
	const int crtc_hstart = (int)(crtc_state->reg[0x04] & 0xff);
	return (bg_hdisp - (crtc_hstart + 4)) * 8;
}

//---------------------------------------------------------------------------
//
/// ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
Render::Render(VM* p) : Device(p)
{
	// ƒfƒoƒCƒXID‚ğ‰Šú‰»
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// ƒfƒoƒCƒXƒ|ƒCƒ“ƒ^
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(CRTC)
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒpƒŒƒbƒg)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒeƒLƒXƒg)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒOƒ‰ƒtƒBƒbƒN)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(PCG,ƒXƒvƒ‰ƒCƒg,BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = 0;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(‡¬)
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(•`‰æ)
	memset(render.draw, 0, sizeof(render.draw));
	render.drawflag = NULL;
}

//---------------------------------------------------------------------------
//
//	‰Šú‰»
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTCæ“¾
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// CRTCƒ[ƒNƒAƒhƒŒƒXæ“¾
	cp = crtc->GetWorkAddr();

	// VCæ“¾
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// VCƒ[ƒNƒAƒhƒŒƒXæ“¾
	vp = vc->GetWorkAddr();

	px68k_adapter = new Px68kRenderAdapter();
	if (!px68k_adapter || !px68k_adapter->Init()) {
		return FALSE;
	}
	px68k_crtc_host.ctx = this;

#if LOCAL_EXCEPTION
	try {
#endif	// LOCAL_EXCEPTION

	// ƒpƒŒƒbƒgƒoƒbƒtƒ@Šm•Û(4MB)
	render.palbuf = new DWORD[0x10000 * 16];
	if (!render.palbuf) {
		return FALSE;
	}

	// ƒeƒLƒXƒgVRAMƒoƒbƒtƒ@Šm•Û(4.7MB)
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

	// ƒOƒ‰ƒtƒBƒbƒNVRAMƒoƒbƒtƒ@Šm•Û(8.2MB)
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

	// PCGƒoƒbƒtƒ@Šm•Û(4MB)
	render.pcgbuf = new DWORD[16 * 256 * 16 * 16];
	if (!render.pcgbuf) {
		return FALSE;
	}

	// ƒXƒvƒ‰ƒCƒgƒ|ƒCƒ“ƒ^Šm•Û(512KB)
	render.spptr = new DWORD*[128 * 512];
	if (!render.spptr) {
		return FALSE;
	}

	// BGƒ|ƒCƒ“ƒ^Šm•Û(768KB)
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

	// BG/ƒXƒvƒ‰ƒCƒgƒoƒbƒtƒ@Šm•Û(4MB)
	render.bgspbuf = new DWORD[1024 * 1024];
	if (!render.bgspbuf) {
		return FALSE;
	}

	// •`‰æƒtƒ‰ƒOƒoƒbƒtƒ@Šm•Û(256KB)
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

	// ƒpƒŒƒbƒgì¬
	MakePalette();

	// ‚»‚Ì‘¼ƒ[ƒNƒGƒŠƒA
	render.contlevel = 0;
	render.contvalue = 0;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒNƒŠ[ƒ“ƒAƒbƒv
//
//---------------------------------------------------------------------------
void FASTCALL Render::Cleanup()
{
	int i;

	ASSERT(this);

	// •`‰æƒtƒ‰ƒO
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/ƒXƒvƒ‰ƒCƒgƒoƒbƒtƒ@
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BGƒ|ƒCƒ“ƒ^
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// ƒXƒvƒ‰ƒCƒgƒ|ƒCƒ“ƒ^
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCGƒoƒbƒtƒ@
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// ƒOƒ‰ƒtƒBƒbƒNVRAMƒoƒbƒtƒ@
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

	// ƒeƒLƒXƒgVRAMƒoƒbƒtƒ@
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

	// ƒpƒŒƒbƒgƒoƒbƒtƒ@
	if (render.palbuf) {
		delete[] render.palbuf;
		render.palbuf = NULL;
	}

	// Šî–{ƒNƒ‰ƒX‚Ö
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
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
	LOG0(Log::Normal, "ƒŠƒZƒbƒg");

	// ƒrƒfƒIƒRƒ“ƒgƒ[ƒ‰‚æ‚èƒ|ƒCƒ“ƒ^æ“¾
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// ƒeƒLƒXƒgVRAM‚æ‚èƒ|ƒCƒ“ƒ^æ“¾
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// ƒOƒ‰ƒtƒBƒbƒNVRAM‚æ‚èƒ|ƒCƒ“ƒ^æ“¾
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// ƒXƒvƒ‰ƒCƒgƒRƒ“ƒgƒ[ƒ‰‚æ‚èƒ|ƒCƒ“ƒ^æ“¾
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»
	render.first = 0;
	render.last = 0;
	render.enable = TRUE;
	render.act = TRUE;
	render.count = 2;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(crtc, vc) --ÄŒvZ‚ğ‘£‚·‚½‚ßTRUE
	render.crtc = TRUE;
	render.vc = TRUE;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒRƒ“ƒgƒ‰ƒXƒg)--ÄŒvZ‚ğ‘£‚·‚½‚ßTRUE
	render.contrast = TRUE;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒpƒŒƒbƒg)--ÄŒvZ‚ğ‘£‚·‚½‚ßTRUE
	render.palette = TRUE;
	render.palptr = render.palbuf;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒeƒLƒXƒg)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒOƒ‰ƒtƒBƒbƒN)
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(PCG)
	// ƒŠƒZƒbƒg’¼Œã‚ÍBG,Sprite‚Æ‚à‚·‚×‚Ä•\¦‚µ‚È‚¢¨PCG‚Í–¢g—p
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(ƒXƒvƒ‰ƒCƒg)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(BG)
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(BG/ƒXƒvƒ‰ƒCƒg)
	render.bgsp = TRUE;
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BG‚Ì‰Šú‰»ó‘Ô‚ğ‚Â‚­‚é(‚·‚×‚Ä0000)
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

	// ƒ[ƒNƒGƒŠƒA‰Šú‰»(‡¬)
	render.mixtype = 0;
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio* /*fio*/, int /*ver*/)
{
	ASSERT(this);
	LOG0(Log::Normal, "ƒZ[ƒu");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Load(Fileio* /*fio*/, int /*ver*/)
{
	ASSERT(this);
	LOG0(Log::Normal, "ƒ[ƒh");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	İ’è“K—p
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "İ’è“K—p");

	// 15kHzƒXƒLƒƒƒ“ƒ‰ƒCƒ“•\¦
	render.scanline = config->disp_scanline;
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒtƒŒ[ƒ€ŠJn
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

	// ‚±‚ÌƒtƒŒ[ƒ€‚ÍƒXƒLƒbƒv‚·‚é‚©
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// ‚±‚ÌƒtƒŒ[ƒ€‚ÍƒŒƒ“ƒ_ƒŠƒ“ƒO‚·‚é
	render.act = TRUE;

	// ƒ‰ƒXƒ^‚ğƒNƒŠƒA
	render.first = 0;
	render.last = 0;

	// CRTC
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTCˆ—");
#endif	// RENDER_LOG
		Crtc();
	}

	// ƒRƒ“ƒgƒ‰ƒXƒg
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ƒRƒ“ƒgƒ‰ƒXƒgˆ—");
#endif	// RENDER_LOG
		Contrast();
	}

	// ƒCƒ“ƒ^ƒŒ[ƒX‚Ì‡¬‹ôŠï‚ğXV
	render.mixeven = cp->v_scaneven;
}

//---------------------------------------------------------------------------
//
//	ƒtƒŒ[ƒ€I—¹
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

	// –³Œø‚È‚ç‰½‚à‚µ‚È‚¢
	if (!render.act) {
		return;
	}

	// ‚±‚±‚Ü‚Å‚Ìƒ‰ƒXƒ^‚ğˆ—
	if (render.last > 0) {
		for (i=render.last; i<render.height; i++) {
			Process(i, 0);
		}
	}

	// ƒJƒEƒ“ƒgUp
	render.count++;

	// –³Œø‰»
	render.act = FALSE;

#if XM6_RENDER_SYNC == 2
	// •`‰æ
	ASSERT(m_pScheduler);
	m_pScheduler->UpdateFrame();
#endif	// XM6_RENDER_SYNC == 2
}

//---------------------------------------------------------------------------
//
//	‡¬ƒoƒbƒtƒ@ƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// İ’è
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// ‡¬ƒoƒbƒtƒ@‚Ìˆ—’·‚ğ’²®
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// ‚·‚×‚Ä‚Ì‡¬‚ğw¦
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	CRTCƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTC()
{
	ASSERT(this);

	// ƒtƒ‰ƒOON‚Ì‚İ
	render.crtc = TRUE;
}

//---------------------------------------------------------------------------
//
//	VCƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVC()
{
	ASSERT(this);

	// ƒtƒ‰ƒOON‚Ì‚İ
	render.vc = TRUE;
}

//---------------------------------------------------------------------------
//
//	CRTCˆ—
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

	// ƒf[ƒ^æ“¾
	sprite->GetSprite(&sprdata);

	// h_dotsAv_dots‚ª0‚È‚ç•Û—¯
	if ((cp->h_dots == 0) || (cp->v_dots == 0)) {
		return;
	}

	// CRTCî•ñ‚ğƒRƒs[
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

	// …•½ü”g”ƒ‚[ƒhŒˆ’è
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

	// CRTƒGƒ~ƒ…ƒŒ[ƒVƒ‡ƒ“—p‚Ìî•ñİ’è
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

	// ƒCƒ“ƒ^ƒŒ[ƒX‚Ü‚½‚Í…•½ü”g”ƒ‚[ƒh‚ª
	// 15kHz‚Ìê‡‚Íc‚Q”{‚Å‡¬‚·‚é‚Ì‚Å•â³‚·‚é
	if (cp->v_mul == 0 || render.hres == 0) {
		render.v_disp <<= 1;
		render.v_total <<= 1;
		render.v_pulse <<= 1;
		render.v_start <<= 1;
	}

	// ‡¬ƒoƒbƒtƒ@‚Ìˆ—’·‚ğ’²®
	mixlen = render.mixlen;
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// mixlen‚ª–³Œø‚È‚ç‡¬‚³‚¹‚È‚¢
	if (render.mixlen==0) {
		render.act = FALSE;
	}

	// BG/ƒXƒvƒ‰ƒCƒg‚ÌˆÊ’u’²®
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

	// ‡¬ƒ‚[ƒh
	if (render.lowres) {
		// 15kHz
		if (render.vd == 1) {
			// ƒCƒ“ƒ^ƒŒ[ƒX
			render.mixmode = 1;
		} else {
			// ’Êí
			render.mixmode = 0;
		}
	} else {
		// 31kHz
		if (render.vd == 0) {
			// ‚Q“x“Ç‚İ
			render.mixmode = 2;
		} else if (render.vd == 1) {
			// ’Êí
			render.mixmode = 0;
		} else {
			// ƒCƒ“ƒ^ƒŒ[ƒX
			render.mixmode = 1;
		}
	}

	// ‡¬ƒ‚[ƒh(BG,ƒXƒvƒ‰ƒCƒg)
	if (render.bgsp_lowres) {
		// 15kHz
		if (render.bgsp_vres == 0) {
			// ’Êí
			render.bgsp_mixmode = 0;
		} else {
			// ƒCƒ“ƒ^ƒŒ[ƒX
			render.bgsp_mixmode = 1;
		}
	} else {
		// 31kHz
		if (render.bgsp_vres == 0) {
			// ‚Q“x“Ç‚İƒ‚[ƒh
			render.bgsp_mixmode = 2;
		} else {
			// ’Êí
			render.bgsp_mixmode = 0;
		}
	}

	// ƒ‰ƒXƒ^[‚¸‚êŒŸØˆ—
	if (sprdata.h_res != 0) {
		// ƒCƒ“ƒfƒbƒNì¬
		index = render.hd;
		if (!render.lowres) {
			index += 4;
		}
		if (render.hrl) {
			index += 8;
		}

		// ŒŸØƒe[ƒuƒ‹”äŠr
		if (sprdata.h_disp > HDispTable[index]) {
			render.bgsp_v++;
		}
	}

	// ƒXƒvƒ‰ƒCƒgƒŠƒZƒbƒg(mixlen,bgsp_h,bgsp_v‚ÉˆË‘¶‚·‚é‚½‚ß)
	if (mixlen != render.mixlen ||
		bgsp_h != render.bgsp_h || bgsp_v != render.bgsp_v) {
		SpriteReset();
	}

	// ƒIƒt
	render.crtc = FALSE;

	// VC‚É’Ê’m
	SetVC();
}

//---------------------------------------------------------------------------
//
//	ƒ‰ƒXƒ^[‚¸‚êŒŸØƒe[ƒuƒ‹
//	lowres,hd,HRL‚©‚ç“¾‚ç‚ê‚é’lB
//
//	BGSP‚ÌH-DISP‚ª‚±‚Ì’lˆÈã‚É‚È‚é‚ÆBGSP–Ê‚ª‚Pƒ‰ƒXƒ^[ã‚É‚¸‚ê‚éŒ»Û‚ª‚Å‚é
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
//	VCˆ—
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

	// VCƒtƒ‰ƒO‚ğ~‚ë‚·
	render.vc = FALSE;

	// ƒtƒ‰ƒOON
	render.mixdirty = TRUE;
	render.textdirty = TRUE;
	render.grpdirty = TRUE;
	render.bgspdirty = TRUE;

	// ƒeƒLƒXƒgƒCƒl[ƒuƒ‹
	if (vp->ton && !cp->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// ƒOƒ‰ƒtƒBƒbƒNƒ^ƒCƒv
	type = 0;
	if (!vp->siz) {
		type = (int)(vp->col + 1);
	}
	if (type != render.grptype) {
		render.grptype = type;
	}


	// ƒOƒ‰ƒtƒBƒbƒN‡¬ƒ^ƒCƒv
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


	// ‘S‘Ì‡¬ˆÊ’u’²®—pƒVƒtƒg—Ê‰Šú‰»
	mixlshift = 0;
	mixrshift = 0;

	// ‚Q“x“Ç‚İ‚ÍˆÊ’u‚ğ”¼•ª‚É‚È‚é‚æ‚¤‚É‚·‚é
	if (render.mixmode == 2) {
		mixrshift = 1;
	}

	// BG/ƒXƒvƒ‰ƒCƒg‡¬—pƒVƒtƒg—Ê‰Šú‰»
	render.bgsp_rshift = 0;
	render.bgsp_lshift = 0;

	// BGSP‚ª’Êí‚ÅƒCƒ“ƒ^ƒŒ[ƒX‚Ì‚¾‚¯”¼•ª‚ÌˆÊ’u
	if (render.bgsp_mixmode == 0 && render.mixmode == 1) {
		render.bgsp_rshift = 1;
	}

	// BGSP‚ªƒCƒ“ƒ^ƒŒ[ƒX‚Ìê‡‚ÍƒCƒ“ƒ^ƒŒ[ƒX‚Å–³‚¢‚¾‚¯”{‚ÌˆÊ’u
	if (render.bgsp_mixmode == 1 && render.mixmode != 1) {
		render.bgsp_lshift = 1;
	}

	// BGSP‚ª‚Q“x“Ç‚İ‚Ìê‡‚ÍƒCƒ“ƒ^ƒŒ[ƒXó‘Ô‚Å”¼•ª‚©4•ª‚Ì1‚©Œˆ‚Ü‚é
	if (render.bgsp_mixmode == 2) {
		if (render.mixmode != 1) {
			render.bgsp_rshift = 1;
		} else {
			render.bgsp_rshift = 2;
		}
	}

	// ƒOƒ‰ƒtƒBƒbƒNƒoƒbƒtƒ@‚ğƒZƒbƒg
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

	// ƒOƒ‰ƒtƒBƒbƒN‚Ìİ’è
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

	// ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹‚Æ•\¦\¬ƒ`ƒFƒbƒN
	if (render.grpdirty) {
		GrpScrlCheck();
		GrpDispCheck();
	}

	// —Dæ‡ˆÊ‚ğæ“¾
	render.tx = vp->tx;
	render.sp = vp->sp;
	render.gr = vp->gr;

	// ƒ^ƒCƒv‰Šú‰»
	render.mixtype = 0;


	// “Áêˆ—
	//
	// VC‚ÌR1‚ÌƒOƒ‰ƒtƒBƒbƒN‰æ–Ê—Dæ“x‚ª–¢’è‹`‚Ì%11‚É
	// İ’è‚³‚ê‚é‚ÆƒeƒLƒXƒg‚àBGSP‚àÁ‚¦‚Ä‚µ‚Ü‚¤‚æ‚¤‚¾B
	//
	if (render.gr == 3) {
		if (render.mixpage == 0) {
			// ƒOƒ‰ƒtƒBƒbƒN‚È‚µ(type=0)
			render.mixtype = 0;
			return;
		}
		// ƒOƒ‰ƒtƒBƒbƒN1–ÊˆÈã‚Ì‚İ(type=3)
		render.mixtype = 3;
		return;
	}


	// BG/ƒXƒvƒ‰ƒCƒg•\¦Ø‘Ö‚©
	if ((cp->hd >= 2) || (!vp->son)) {
		if (render.bgspflag) {
			// BG/ƒXƒvƒ‰ƒCƒg•\¦ON->OFF
			render.bgspflag = FALSE;
		}
	} else {
		if (!render.bgspflag) {
			// BG/ƒXƒvƒ‰ƒCƒg•\¦OFF->ON
			render.bgspflag = TRUE;
		}
	}

	// ƒpƒŒƒbƒg‚O”¼“§–¾‚Í“Á•Êˆ—
	if (vp->ah) {
		// ƒpƒŒƒbƒg‚O”¼“§–¾(type=11)
		if (render.mixpage > 0) {
			render.mixtype = 11;
		} else {
			render.mixtype = 0;
		}
		return;
	}

	// ƒOƒ‰ƒtƒBƒbƒN‚Ì‚İ
	if (!render.texten && ((cp->hd >= 2) || (!vp->son))) {
		if (render.mixpage == 0) {
			// ƒOƒ‰ƒtƒBƒbƒN‚È‚µ(type=0)
			render.mixtype = 0;
			return;
		}
		// ƒOƒ‰ƒtƒBƒbƒN1–ÊˆÈã‚Ì‚İ(type=3)
		render.mixtype = 3;
		return;
	}

	// ƒeƒLƒXƒg‚Ì‚İ
	if (render.mixpage == 0 && render.texten && ((cp->hd >= 2) || (!vp->son))) {
		// ƒOƒ‰ƒtƒBƒbƒN‚È‚µBƒeƒLƒXƒg‚Ì‚İ(type=1)
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

	// ƒXƒvƒ‰ƒCƒg‚Ì‚İ
	if (render.mixpage == 0 && cp->hd < 2 && vp->son && !render.texten) {
		// ƒOƒ‰ƒtƒBƒbƒN‚È‚µAƒXƒvƒ‰ƒCƒg‚Ì‚İ(type=2)
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 512 - 1;
		render.mixandy[0] = 512 - 1;
		render.mixraster[0] = render.bgsp_v;
		render.mixtype = 2;
		return;
	}

	// —Dæ‡ˆÊŒˆ’è
	if (render.tx == 3)
		render.tx--;
	if (render.sp == 3)
		render.sp--;
	if (render.gr == 3)
		render.gr--;

	if (render.tx == render.sp) {
		// “K“–‚ÉŒˆ‚ß‚Ä‚¢‚é
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
		// “K“–‚ÉŒˆ‚ß‚Ä‚¢‚é
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
		// “K“–‚ÉŒˆ‚ß‚Ä‚¢‚é
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


	// ƒOƒ‰ƒtƒBƒbƒN‚ÆƒeƒLƒXƒg‚Ì‚İ(type=5,6,7)
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

		// ƒeƒLƒXƒgŒã–ÊAƒOƒ‰ƒtƒBƒbƒN’†ŠÔ‚È‚çƒOƒ‰ƒtƒBƒbƒN‘O–Ê‚Åˆµ‚¤(type=5)
		if (render.tx==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// ƒOƒ‰ƒtƒBƒbƒN‚ÆƒXƒvƒ‰ƒCƒg‚Ì‚İ(type=5,6,7)
	if (render.mixpage > 0 && cp->hd < 2 && vp->son && !render.texten) {
		// ƒOƒ‰ƒtƒBƒbƒN‚È‚µAƒXƒvƒ‰ƒCƒg‚Ì‚İ(type=2)
		render.mixptr[0] = render.bgspbuf;
		render.mixshift[0] = 9;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 512 - 1;
		render.mixandy[0] = 512 - 1;
		render.mixraster[0] = render.bgsp_v;
		render.mixtype = 5 + render.gr;

		// ƒXƒvƒ‰ƒCƒgŒã–ÊAƒOƒ‰ƒtƒBƒbƒN’†ŠÔ‚È‚çƒOƒ‰ƒtƒBƒbƒN‘O–Ê‚Åˆµ‚¤(type=5)
		if (render.sp==2 && render.gr==1) {
			render.mixtype = 5;
		}
		return;
	}

	// ƒeƒLƒXƒg‚ÆƒXƒvƒ‰ƒCƒg‚Ìİ’è
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
		render.mixshift[1] = 9;
		render.mixrshift[1] = render.bgsp_rshift;
		render.mixlshift[1] = render.bgsp_lshift;
		render.mixx[1] = &render.zero;
		render.mixy[1] = &render.zero;
		render.mixandx[1] = 512 - 1;
		render.mixandy[1] = 512 - 1;
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
		render.mixshift[0] = 9;
		render.mixrshift[0] = render.bgsp_rshift;
		render.mixlshift[0] = render.bgsp_lshift;
		render.mixx[0] = &render.zero;
		render.mixy[0] = &render.zero;
		render.mixandx[0] = 512 - 1;
		render.mixandy[0] = 512 - 1;
		render.mixraster[0] = render.bgsp_v;
	}

	// ƒXƒvƒ‰ƒCƒg‚ÆƒeƒLƒXƒg(type=4)
	if (render.mixpage == 0) {
		render.mixtype = 4;
		return;
	}

	// ƒOƒ‰ƒtƒBƒbƒN‚ÆƒeƒLƒXƒg‚ÆƒXƒvƒ‰ƒCƒg(type=8,9,10)
	render.mixtype = 8 + render.gr;
}

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒgƒ‰ƒXƒgİ’è
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont, BOOL immediate)
{
	// ƒVƒXƒeƒ€ƒ|[ƒg‚Ì“_‚Åˆê’vƒ`ƒFƒbƒN‚ğs‚¤‚Ì‚ÅAˆÙ‚È‚Á‚Ä‚¢‚éê‡‚Ì‚İ
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// •ÏX‚Æƒtƒ‰ƒOON
	render.contlevel = cont << 2;
	render.contrast = TRUE;

	// ‘¦”½‰f‚È‚ç’iŠK“I‚É•Ï‰»‚³‚¹‚È‚¢
	if (immediate) {
		render.contvalue = cont << 2;
	}
}

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒgƒ‰ƒXƒgæ“¾
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
//	ƒRƒ“ƒgƒ‰ƒXƒgˆ—
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	// ƒRƒ“ƒgƒ‰ƒXƒg‚ª“¯‚¶’l‚É‚È‚Á‚½‚Ì‚Åƒtƒ‰ƒO‚n‚e‚e
	if (render.contlevel == render.contvalue) {
		render.contrast = FALSE;
	} else {
		// Œ»İ‚Ì’l‚ğ–Ú•W’l‚ÉŒü‚©‚Á‚Ä’iŠK“I‚É‘Œ¸‚³‚¹‚é
		if (render.contlevel > render.contvalue) {
			render.contvalue = (render.contlevel < (render.contvalue + 2)) ? render.contlevel : (render.contvalue + 2);
		} else {
			render.contvalue = (render.contlevel > (render.contvalue - 2)) ? render.contlevel : (render.contvalue - 2);
		}
	}

	// ƒ|ƒCƒ“ƒgˆÊ’u‚ğ•ÏXAƒtƒ‰ƒODown
	render.palptr = render.palbuf;
	render.palptr += (render.contvalue >> 2) << 16;

	// ƒpƒŒƒbƒgƒtƒ‰ƒO‚ğ‘S‚ÄUp
	memset(render.palmod, TRUE, 0x200);
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒpƒŒƒbƒgì¬
//
//---------------------------------------------------------------------------
void FASTCALL Render::MakePalette()
{
	DWORD *p;
	int ratio;
	int i;
	int j;

	ASSERT(render.palbuf);

	// ‰Šú‰»
	p = render.palbuf;

	// ƒRƒ“ƒgƒ‰ƒXƒgƒ‹[ƒv
	for (i=0; i<16; i++) {
		// ”ä—¦‚ğZo
		ratio = 256 - ((15 - i) * 17);

		// ì¬ƒ‹[ƒv
		for (j=0; j<0x10000; j++) {
			*p++ = ConvPalette(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	ƒpƒŒƒbƒg•ÏŠ·
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

	// ‘S‚ÄƒRƒs[
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSB‚©‚çG:5AR:5AB:5AI:1‚Ì‡‚É‚È‚Á‚Ä‚¢‚é
	// ‚±‚ê‚ğ R:8 G:8 B:8‚ÌDWORD‚É•ÏŠ·Bb31-b24‚Íg‚í‚È‚¢
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// ‹P“xƒrƒbƒg‚Íˆê—¥Up(Œ³ƒf[ƒ^‚ª0‚Ìê‡‚àA!=0‚É‚·‚éŒø‰Ê‚ ‚è)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// ƒRƒ“ƒgƒ‰ƒXƒg‚ğ‰e‹¿‚³‚¹‚é
	b *= ratio;
	b >>= 8;
	g *= ratio;
	g >>= 8;
	g &= 0xff00;
	r *= ratio;
	r >>= 8;
	r &= 0xff0000;

	// ‡¬
	c = (DWORD)(r | g | b);

	// ‹P“xƒrƒbƒg‚ª‚n‚m‚ÌÅIƒ`ƒFƒbƒN
	// Œ³ƒf[ƒ^‚ª0‚©ƒRƒ“ƒgƒ‰ƒXƒgŒvZ‚Å0‚É‚È‚Á‚½‚Å‚à
	// !=0‚ÌŒø‰Ê‚ÍˆÛ‚µ‚È‚¢‚Æd‚Ë‡‚í‚¹‚Å”»’f‚Å‚«‚È‚¢
	if (color & 1) {
		if (color == 1 || c == 0) {
			c = 0x010101;
		}
	}

	return c;
}

//---------------------------------------------------------------------------
//
//	ƒpƒŒƒbƒgæ“¾
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
//	ƒpƒŒƒbƒgˆ—
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

	// ƒtƒ‰ƒOOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// ƒOƒ‰ƒtƒBƒbƒN
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ƒOƒ‰ƒtƒBƒbƒN‚É‰e‹¿Aƒtƒ‰ƒOOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// “§–¾F‚Ìˆ—
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536F‚Ì‚½‚ß‚ÌƒpƒŒƒbƒgƒf[ƒ^İ’è
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2    ] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;

			// ”¼“§–¾E“Áêƒvƒ‰ƒCƒIƒŠƒeƒB—pƒpƒŒƒbƒgƒf[ƒ^İ’è

			// GVRAMw’è
			if ((i & 1) == 0) {
				render.paldataGB[i    ] = render.paldata[i];
				render.paldataGB[i + 1] = render.paldata[i] | REND_COLORS;
				render.paldataGS[i    ] = render.paldata[i];
				render.paldataGS[i + 1] = render.paldata[i];
			}

			// ƒpƒŒƒbƒgw’è
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

	// ƒeƒLƒXƒgŒ“ƒXƒvƒ‰ƒCƒg
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ƒeƒLƒXƒg‚É‰e‹¿Aƒtƒ‰ƒOOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// “§–¾F‚Ìˆ—
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				render.paldata[i] |= REND_COLORT;
				// 0x100‚ÍBGEƒXƒvƒ‰ƒCƒg‚É‚à•K‚¸‰e‹¿
				sp = TRUE;

				// 0x100‚ÍƒOƒ‰ƒtƒBƒbƒN‚Ì”¼“§–¾‚É‰e‹¿‚·‚é‚©‚à‚µ‚ê‚È‚¢
				gr = TRUE;
			}

			// PCGŒŸ¸
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// ƒXƒvƒ‰ƒCƒg
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// ƒXƒvƒ‰ƒCƒg‚É‰e‹¿Aƒtƒ‰ƒOOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// “§–¾F‚Ìˆ—
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCGŒŸ¸
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// ƒOƒ‰ƒtƒBƒbƒNƒtƒ‰ƒO
	if (gr) {
		// ƒtƒ‰ƒOON
		render.grpdirty = TRUE;
	}

	// ƒeƒLƒXƒgƒtƒ‰ƒO
	if (tx) {
		render.textdirty = TRUE;
	}

	// ƒXƒvƒ‰ƒCƒgƒtƒ‰ƒO
	if (sp) {
		render.bgspdirty = TRUE;
	}

	// ƒpƒŒƒbƒgƒtƒ‰ƒOOFF
	render.palette = FALSE;
}

//---------------------------------------------------------------------------
//
//	ƒeƒLƒXƒgƒXƒNƒ[ƒ‹
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ”äŠrƒ`ƒFƒbƒN
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// ƒ[ƒNXV
	render.textx = x;
	render.texty = y;

	// ƒtƒ‰ƒOON
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "ƒeƒLƒXƒgƒXƒNƒ[ƒ‹ x=%d y=%d", x, y);
#endif	// REND_LOG

		render.mixdirty = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	ƒeƒLƒXƒgƒRƒs[
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// ƒAƒZƒ“ƒuƒ‰ƒTƒu
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	ƒeƒLƒXƒgƒoƒbƒtƒ@æ“¾
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
//	ƒeƒLƒXƒg•ÏŠ·
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

	// ‡¬ƒtƒ‰ƒOƒIƒt
	mixflag = FALSE;

	// À‰æ–ÊYZo
	y = (offset + render.texty) & 0x3ff;

	// •ÏXƒtƒ‰ƒO(’€ŸŒ^)
	if (render.textmod[y]) {
		// ƒtƒ‰ƒOˆ—
		render.textmod[y] = FALSE;
		mixflag = TRUE;

		// …•½‚’¼•ÏŠ·
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// ‚’¼ƒpƒŒƒbƒg•ÏŠ·
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// ƒpƒŒƒbƒg(ˆêŠ‡Œ^)
	if (render.textpal[y]) {
		// ƒtƒ‰ƒOˆ—
		render.textpal[y] = FALSE;

		// ‚’¼ƒpƒŒƒbƒg•ÏŠ·
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		mixflag = TRUE;

		// y == 1023‚È‚çƒRƒs[‚·‚é
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}

	return mixflag;
}

//---------------------------------------------------------------------------
//
//	ƒeƒLƒXƒgˆ—
//
//---------------------------------------------------------------------------
void FASTCALL Render::Text(int raster)
{
	int offset;
	BOOL mixflag;

	// ƒfƒBƒZ[ƒuƒ‹‚È‚ç‰½‚à‚µ‚È‚¢
	if (!render.texten) {
		return;
	}

	// ƒIƒtƒZƒbƒgZo
	offset = raster;

	// ‚Q“x“Ç‚İ‚Í”¼•ª‚ÌˆÊ’u
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// Œ»İ‚ÌƒIƒtƒZƒbƒg‚Ì•ÏŠ·
	mixflag = TextConv(offset);

	// XV‚ª‚ ‚ê‚ÎŒ»İ‚Ìƒ‰ƒXƒ^[‚ğƒtƒ‰ƒOƒAƒbƒv
	if (mixflag) {
		render.mix[raster] = TRUE;
	}

	// ƒXƒNƒ[ƒ‹‚ª‚ ‚ê‚ÎŸ‚ÌƒIƒtƒZƒbƒg‚à
	// Œ©‚¦‚Ä‚µ‚Ü‚¤‚Ì‚Å‚±‚±‚Å‡¬
	if (render.textx > 0 && offset < 1023) {
		mixflag = TextConv(offset + 1);

		// XV‚ª‚ ‚ê‚ÎŒ»İ‚Ìƒ‰ƒXƒ^[‚ğƒtƒ‰ƒOƒAƒbƒv
		if (mixflag) {
			render.mix[raster] = TRUE;
			render.mix[raster + 1] = TRUE;
		}

		// XV‚ª‚ ‚ê‚ÎŸ‚Ìƒ‰ƒXƒ^[‚àƒtƒ‰ƒOƒAƒbƒv
		if (mixflag) {
			if (render.mixmode == 2) {
				render.mix[raster + 2] = TRUE;
			} else {
				render.mix[raster + 1] = TRUE;
			}
		}
	}

	// ‘O‚Ü‚½‚ÍŸ‚Ìƒ‰ƒXƒ^[‚à‘ÎÛ
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	ƒOƒ‰ƒtƒBƒbƒNƒoƒbƒtƒ@æ“¾
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
//	ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹ƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
{
	BOOL flag;

	ASSERT(this);
	ASSERT((block >= 0) && (block <= 3));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ”äŠrƒ`ƒFƒbƒNB”ñ•\¦‚È‚çXV‚È‚µ
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpben[block];
	}

	// ƒtƒ‰ƒOˆ—
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹ block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	// ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹XVƒtƒ‰ƒOƒAƒbƒv
	render.grpscrl = TRUE;

	// ‡¬ƒtƒ‰ƒOON
	render.mixdirty = TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹ƒ`ƒFƒbƒNˆ—
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpScrlCheck()
{
	BOOL flag;
	int dx[4];
	int dy[4];
	int i;

	// ƒtƒ‰ƒOƒIƒt
	render.grpscrl = FALSE;

	// ƒXƒNƒ[ƒ‹XVŒŸØ
	flag = FALSE;

	// ‘Š‘ÎˆÊ’uƒ`ƒFƒbƒN‚Íƒ^ƒCƒv‚Q`‚S
	switch(render.grptype) {
		// ƒ^ƒCƒv0:1024~1024 16Color
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

		// ƒ^ƒCƒv1:512~512 16Color
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

		// ƒ^ƒCƒv2:512~512 256Color
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

		// ƒ^ƒCƒv3:512x512 –¢’è‹`
		case 3:
		// ƒ^ƒCƒv4:512x512 65536Color
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

	// XVƒ`ƒFƒbƒN
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

	// ƒuƒƒbƒNŠÔ‚Ì‘Š‘ÎƒXƒNƒ[ƒ‹’l‚ª
	// XV‚³‚ê‚½‚çƒoƒbƒtƒ@Äì¬
	if (!flag) {
		return;
	}

	// ƒoƒbƒtƒ@Äì¬•K—v
	render.grpdirty = TRUE;

	// •\¦\¬ƒ`ƒFƒbƒN
	GrpDispCheck();
}

//---------------------------------------------------------------------------
//
//	ƒOƒ‰ƒtƒBƒbƒN•\¦\¬ƒ`ƒFƒbƒNˆ—
//
//---------------------------------------------------------------------------
void FASTCALL Render::GrpDispCheck()
{
	// •\¦ó‘Ô‚Æ•\¦ƒuƒƒbƒN‚©‚ç\¬‚ğ’²‚×‚é
	switch(render.grptype) {
		// ƒ^ƒCƒv0:1024~1024 16Color
		case 0:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// ƒ^ƒCƒv1:512~512 16Color
		case 1:
			render.grpnorm[0] = TRUE;
			render.grpnorm[1] = TRUE;
			render.grpnorm[2] = TRUE;
			render.grpnorm[3] = TRUE;
			break;

		// ƒ^ƒCƒv2:512~512 256Color
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

		// ƒ^ƒCƒv3:512x512 –¢’è‹`
		case 3:
		// ƒ^ƒCƒv4:512x512 65536Color
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
//	GVRAMƒoƒbƒtƒ@\¬•ÏŠ·
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

	// ƒ^ƒCƒv–ˆ‚ÉGVRAM‚Ìƒoƒbƒtƒ@‚ğ‡¬‚·‚é
	switch(render.grptype) {
		// ƒ^ƒCƒv0:1024~1024 16Color
		case 0:
			break;

		// ƒ^ƒCƒv1:512~512 16Color
		case 1:
			break;

		// ƒ^ƒCƒv2:512~512 256Color
		case 2:
			// ƒuƒƒbƒN,X/YƒIƒtƒZƒbƒg,ƒAƒhƒŒƒXZo
			for (i=0; i<2; i++) {
				base = vp->gp[gd];
				block[i] = vp->gp[i+gd];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAM‡¬
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

		// ƒ^ƒCƒv3:512x512 –¢’è‹`
		case 3:
		// ƒ^ƒCƒv4:512x512 65536Color
		case 4:
			// ƒuƒƒbƒN,X/YƒIƒtƒZƒbƒg,ƒAƒhƒŒƒXZo
			for (i=0; i<4; i++) {
				base = vp->gp[0];
				block[i] = vp->gp[i];
				x[i] = (render.grpx[block[i]] - render.grpx[base]) & 0x1ff;
				y[i] = (offset + render.grpy[block[i]]) & 0x1ff;
				g[i] = (WORD*)(render.grpgv + (y[i] << 10));
				shift[i] = block[i] * 4;
			}

			// GVRAM‡¬
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
//	ƒOƒ‰ƒtƒBƒbƒNˆ—
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

	// ƒOƒ‰ƒtƒBƒbƒN‚Í•\¦‚³‚ê‚Ä‚¢‚é‚©
	if (render.mixpage == 0) {
		return;
	}

	// ƒ^ƒCƒv•Ê‚Ì‡¬”»’è
	if (render.grptype == 2) {
		// ƒ^ƒCƒv2:512~512 256Color
		if (gd != 0 && gd != 2) {
			return;
		}
	} else if (render.grptype > 2) {
		// ƒ^ƒCƒv3:512x512 –¢’è‹`
		// ƒ^ƒCƒv4:512x512 65536Color
		if (gd != 0) {
			return;
		}
	}

	// ƒIƒtƒZƒbƒgZo
	offset = raster;

	// ‚Q“x“Ç‚İ‚Í”¼•ª‚ÌˆÊ’u
	if (render.mixmode == 2) {
		offset >>= 1;
	}

	// ƒpƒŒƒbƒgƒfƒtƒHƒ‹ƒgİ’è
	paldata = render.paldata;

	// ƒpƒŒƒbƒgİ’è(“Áê‹@”\)
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

	// ƒ^ƒCƒv•Ê
	switch (render.grptype) {
		// ƒ^ƒCƒv0:1024~1024 16Color
		case 0:
			// ƒuƒƒbƒNZo
			block = vp->gp[gd];

			// ƒIƒtƒZƒbƒgZo
			offset = (offset + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// •\¦‘ÎÛƒ`ƒFƒbƒN
			if ((offset < 512) && (gd >= 2)) {
				return;
			}
			if ((offset >= 512) && (gd < 2)) {
				return;
			}

			// ƒpƒŒƒbƒg‚Ìê‡‚Í‘S—Ìˆæˆ—
			if (render.grppal[y + (gd << 9)]) {
				render.grppal[y + (gd << 9)] = FALSE;
				render.grpmod[y + (gd << 9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				switch (block) {
					// ƒuƒƒbƒN0
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ƒuƒƒbƒN1
					case 1:
						if (Rend1024C(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ƒuƒƒbƒN2
					case 2:
						if (Rend1024E(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
					// ƒuƒƒbƒN3
					case 3:
						if (Rend1024G(render.grpgv + (y << 10),
							render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9), paldata) != 0) {
							render.mix[raster] = TRUE;
						}
						break;
				}
				break;
			}

			// ‚»‚êˆÈŠO‚Ígrpmod‚ğŒ©‚Äˆ—
			if (!render.grpmod[y + (gd << 9)]) {
				return;
			}
			render.grpmod[y + (gd << 9)] = FALSE;
			render.mix[raster] = TRUE;

			switch (block) {
				// ƒuƒƒbƒN0
				case 0:
					Rend1024B(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ƒuƒƒbƒN1
				case 1:
					Rend1024D(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ƒuƒƒbƒN2
				case 2:
					Rend1024F(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
				// ƒuƒƒbƒN3
				case 3:
					Rend1024H(render.grpgv + (y << 10),
						render.grpbuf[0] + (offset << 11) + ((gd & 1) << 9),
						render.grpflag + (y << 5) + (gd << 14),
						paldata);
					break;
			}

			break;

		// ƒ^ƒCƒv1:512~512 16Color
		case 1:
			// ƒuƒƒbƒNZo
			block = vp->gp[gd];

			switch (block) {
				// ƒuƒƒbƒN0
				case 0:
					y = (offset + render.grpy[0]) & 0x1ff;
					// ƒpƒŒƒbƒg
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
					// ’Êí
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ƒuƒƒbƒN1
				case 1:
					y = (offset + render.grpy[1]) & 0x1ff;
					// ƒpƒŒƒbƒg
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
					// ’Êí
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ƒuƒƒbƒN2
				case 2:
					y = (offset + render.grpy[2]) & 0x1ff;
					// ƒpƒŒƒbƒg
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
					// ’Êí
					if (render.grpmod[y + (gd<<9)]) {
						render.grpmod[y + (gd<<9)] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[gd] + (y << 10),
								render.grpflag + (y << 5) + (gd << 14),
								paldata);
					}
					break;
				// ƒuƒƒbƒN3
				case 3:
					y = (offset + render.grpy[3]) & 0x1ff;
					// ƒpƒŒƒbƒg
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
					// ’Êí
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

		// ƒ^ƒCƒv2:512~512 256Color
		case 2:
			// ƒuƒƒbƒNZo
			block = vp->gp[gd];

			// ƒIƒtƒZƒbƒgZo
			y = (offset + render.grpy[block]) & 0x1ff;

			// ƒpƒŒƒbƒg‚Ìê‡‚Í‘S—Ìˆæˆ—
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

			// ‚»‚êˆÈŠO‚Ígrpmod‚ğŒ©‚Äˆ—
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

		// ƒ^ƒCƒv3:512x512 –¢’è‹`
		case 3:
		// ƒ^ƒCƒv4:512x512 65536Color
		case 4:
			// ƒuƒƒbƒNZo
			block = vp->gp[gd];

			// ƒIƒtƒZƒbƒgZo
			y = (offset + render.grpy[block]) & 0x1ff;

			// ƒpƒŒƒbƒg‚Ìê‡‚Í‘S—Ìˆæˆ—
			if (render.grppal[y + (gd<<9)]) {
				render.grppal[y + (gd<<9)] = FALSE;
				render.grpmod[y + (gd<<9)] = FALSE;
				memset(&render.grpflag[(y << 5) + (gd << 14)], FALSE, 32 * 4);

				if (render.grpnorm[0]) {
					grpgv = render.grpgv + (y << 10);
				} else {
					grpgv = MixGVRAM(buf, gd, offset);
				}

				// ”¼“§–¾/“Áêƒvƒ‰ƒCƒIƒŠƒeƒB—pƒpƒŒƒbƒg“ü‚ê‘Ö‚¦“Áê‡¬
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

			// ‚»‚êˆÈŠO‚Ígrpmod‚ğŒ©‚Äˆ—
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

			// ”¼“§–¾ƒpƒŒƒbƒg“ü‚ê‘Ö‚¦“Áê‡¬
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

	// ‘O‚Ü‚½‚ÍŸ‚Ìƒ‰ƒXƒ^[‚à‘ÎÛ
	if (render.mixmode == 2 && render.mix[raster]) {
		render.mix[raster ^ 1] = TRUE;
	}
}

//===========================================================================
//
//	ƒŒƒ“ƒ_ƒ‰(BGEƒXƒvƒ‰ƒCƒg•”)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	ƒXƒvƒ‰ƒCƒgƒŒƒWƒXƒ^ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	int i;
	DWORD addr;

	for (i=0; i<128; i++) {
		addr = i << 3;
		SpriteReg(addr, *(WORD*)(&render.sprmem[addr    ]));
		SpriteReg(addr + 2, *(WORD*)(&render.sprmem[addr + 2]));
		SpriteReg(addr + 4, *(WORD*)(&render.sprmem[addr + 4]));
		SpriteReg(addr + 6, *(WORD*)(&render.sprmem[addr + 6]));
	}
}

//---------------------------------------------------------------------------
//
//	’X’v’0’C’g’R’W’X’^"ÏX
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

	// ”äŠrAˆê’v‚µ‚Ä‚ê‚Î‰½‚à‚µ‚È‚¢
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// XV
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768~512‚È‚ç–³ˆÓ–¡
	if (!render.bgspflag) {
		return;
	}

	// •\¦’†‚È‚çABGSPMOD‚ğã‚°‚é
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
//	BGƒRƒ“ƒgƒ[ƒ‹•ÏX
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

	// ƒtƒ‰ƒOOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// ƒ^ƒCƒv•Ê
	switch (index) {
		// BG0 •\¦ƒtƒ‰ƒO
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 •\¦ƒtƒ‰ƒO
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 ƒGƒŠƒA•ÏX
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 ƒGƒŠƒA•ÏX
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BGƒTƒCƒY•ÏX
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
			// •\¦ó‘Ôƒ`ƒFƒbƒN‚ÍProcess‚ÅÀs
			render.bgsp = TRUE;
			return;

		// ‚»‚Ì‘¼(‚ ‚è‚¦‚È‚¢)
		default:
			ASSERT(FALSE);
			return;
	}

	// ƒtƒ‰ƒOˆ—
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// Œ»ó‚Åg‚Á‚Ä‚¢‚érender.pcguse‚ğƒJƒbƒg
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

			// ƒf[ƒ^ƒAƒhƒŒƒX‚ğZo($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64~64ƒ[ƒhƒRƒs[B$10000‚Ìƒrƒbƒg‚Íí‚É0
			if (render.bgsize) {
				// 16x16‚Í‚»‚Ì‚Ü‚Ü
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8‚ÍH•v‚ª•K—vBPCG(0-255)‚ğ>>2‚µAÁ‚¦‚½bit0,1‚ğbit17,18‚Ö
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

			// bgall‚ÌƒZƒbƒg
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// ‚Ç‚Ì•ÏX‚Å‚àA768~512ˆÈŠO‚È‚çbgspmod‚ğã‚°‚é
	if (render.bgspflag) {
		render.bgspdirty = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	BGƒƒ‚ƒŠ•ÏX
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

	// ƒy[ƒWƒ‹[ƒv
	for (i=0; i<2; i++) {
		// ŠY“–ƒy[ƒW‚Ìƒf[ƒ^ƒGƒŠƒA‚Æˆê’v‚µ‚Ä‚¢‚é‚©
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

		// ƒCƒ“ƒfƒbƒNƒX(<64x64)AƒŒƒWƒXƒ^ƒ|ƒCƒ“ƒ^æ“¾
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// ˆÈ‘O‚Ìpcguse‚ğÁ‚·
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[pcgno] > 0);
			render.pcguse[pcgno]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[pcgno] > 0);
			render.pcgpal[pcgno]--;
		}

		// ƒRƒs[
		if (render.bgsize) {
			// 16x16‚Í‚»‚Ì‚Ü‚Ü
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8‚ÍH•v‚ª•K—vBPCG(0-255)‚ğ>>2‚µAÁ‚¦‚½bit0,1‚ğbit17,18‚Ö
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

		// bgall‚ğã‚°‚é
		render.bgall[i][index >> 6] = TRUE;

		// •\¦’†‚Å‚È‚¯‚ê‚ÎI—¹Bbgsize=1‚Åƒy[ƒW1‚Ìê‡‚àI—¹
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// ƒXƒNƒ[ƒ‹ˆÊ’u‚©‚çŒvZ‚µAbgspmod‚ğã‚°‚é
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
//	PCGƒƒ‚ƒŠ•ÏX
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

	// ƒCƒ“ƒfƒbƒNƒX‚ğo‚·
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgready‚ğÁ‚·
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguse‚ª>0‚È‚ç
	for (i=0; i<16; i++) {
		if (render.pcguse[index + (i << 8)] == 0) {
			continue;
		}

		// d•û‚È‚¢‚Ì‚ÅABG/ƒXƒvƒ‰ƒCƒgÄ‡¬‚ğŒˆ’è
		render.bgspdirty = TRUE;
		break;
	}
}

//---------------------------------------------------------------------------
//
//	PCGƒoƒbƒtƒ@æ“¾
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
//	BG/ƒXƒvƒ‰ƒCƒgƒoƒbƒtƒ@æ“¾
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
//	BG/ƒXƒvƒ‰ƒCƒg
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSprite(int raster)
{
	int i;
	DWORD *reg;
	DWORD **ptr;
	DWORD *buf;
	DWORD pcgno;
	BYTE pri[512 + 16];
	int source_raster;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);
	const BOOL sprite_visible = sprite->IsDisplay();

	if (raster >= 1024) return;
	if (render.mixlen > 512) return;

	source_raster = (int)(((raster + render.bgsp_v) >> render.bgsp_rshift) << render.bgsp_lshift);
	source_raster &= 511;

	if (!render.bgspmod[source_raster]) {
		return;
	}
	render.bgspmod[source_raster] = FALSE;
	render.mix[raster] = TRUE;

	if (render.mixmode == 1) {
		render.mix[raster ^ 1] = TRUE;
	}
	if (render.bgsp_mixmode == 2 && render.mixmode != 1) {
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
	}
	if (render.bgsp_mixmode == 2 && render.mixmode == 1) {
		render.mix[(raster - 3) & 1023] = TRUE;
		render.mix[(raster - 2) & 1023] = TRUE;
		render.mix[(raster - 1) & 1023] = TRUE;
		render.mix[(raster + 1) & 1023] = TRUE;
		render.mix[(raster + 2) & 1023] = TRUE;
		render.mix[(raster + 3) & 1023] = TRUE;
	}

	buf = &render.bgspbuf[source_raster << 9];
	RendClrSprite(buf, render.paldata[0x100], render.mixlen);
	if (!sprite_visible) {
		RendClrSprite(buf, render.paldata[0x100] & 0x00ffffff, render.mixlen);
	}

	memset(pri, 0xff, sizeof(pri));

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += source_raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 1) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000, i, pri);
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[1] && !render.bgsize) {
		BG(1, source_raster, buf, TRUE);
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += source_raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 2) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000, i, pri);
			}
			reg -= 4;
			ptr -= 512;
		}
	}

	if (render.bgdisp[0]) {
		BG(0, source_raster, buf, (BOOL)(!render.bgdisp[1] || render.bgsize));
	}

	if (sprite_visible) {
		reg = &render.spreg[127 << 2];
		ptr = &render.spptr[127 << 9];
		ptr += source_raster;
		for (i=127; i>=0; i--) {
			if (render.spuse[i] && (reg[3] == 3) && *ptr) {
				pcgno = reg[2] & 0xfff;
				if (!render.pcgready[pcgno]) {
					ASSERT(render.pcguse[pcgno] > 0);
					render.pcgready[pcgno] = TRUE;
					RendPCGNew(pcgno, render.sprmem, render.pcgbuf, render.paldata);
				}
				const DWORD sprite_x = (DWORD)(((int)reg[0] + bg_hadjust) & 0x03ff);
				RendSprite(*ptr, buf, sprite_x, reg[2] & 0x4000, i, pri);
			}
			reg -= 4;
			ptr -= 512;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG
//
//---------------------------------------------------------------------------
void FASTCALL Render::BG(int page, int raster, DWORD *buf, BOOL force)
{
	int x;
	int y;
	bgdata_t *ptr;
	int len;
	int rest;
	const BOOL draw_force = (BOOL)(force && original_bg0_render_enabled);
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	ASSERT((page == 0) || (page == 1));
	ASSERT((raster >= 0) && (raster < 512));
	ASSERT(buf);

	y = render.bgy[page] + raster;
	if (render.bgsize) {
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	ptr = render.bgptr[page];
	if (!render.bgsize) {
		x = (render.bgx[page] - bg_hadjust) & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 6);

		if ((x & 7) == 0) {
			x >>= 3;
			if (draw_force) {
				RendBG8F(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			else {
				RendBG8(ptr, buf, x, render.mixlen, render.pcgready,
					render.sprmem, render.pcgbuf, render.paldata);
			}
			return;
		}

		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		if (draw_force) {
			RendBG8FP(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG8P(&ptr[(x & 0xfff8) >> 3], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}

		len = render.mixlen - rest;
		x += rest;
		x &= (512 - 1);
		ASSERT((x & 7) == 0);
		if (draw_force) {
			RendBG8F(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG8(ptr, &buf[rest], (x >> 3), (len & 0xfff8), render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}

		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			if (draw_force) {
				RendBG8FP(&ptr[x >> 3], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
			}
			else {
				RendBG8P(&ptr[x >> 3], &buf[rest + (len & 0xfff8)], 0, (len & 7),
					render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
			}
		}
		return;
	}

	x = (render.bgx[page] - bg_hadjust) & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 6);

	if ((x & 15) == 0) {
		x >>= 4;
		if (draw_force) {
			RendBG16F(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG16(ptr, buf, x, render.mixlen, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);
		}
		return;
	}

	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	if (draw_force) {
		RendBG16FP(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}
	else {
		RendBG16P(&ptr[(x & 0xfff0) >> 4], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}

	len = render.mixlen - rest;
	x += rest;
	x &= (1024 - 1);
	ASSERT((x & 15) == 0);
	if (draw_force) {
		RendBG16F(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}
	else {
		RendBG16(ptr, &buf[rest], (x >> 4), (len & 0xfff0), render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);
	}

	if (len & 15) {
		x += (len & 0xfff0);
		x &= (1024 - 1);
		x >>= 4;
		if (draw_force) {
			RendBG16FP(&ptr[x], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
		else {
			RendBG16P(&ptr[x], &buf[rest + (len & 0xfff0)], 0, (len & 15),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
	}
}


//---------------------------------------------------------------------------
//
//	BG(’u’’b’NÆ)
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

	// ƒŒƒWƒXƒ^ƒ|ƒCƒ“ƒ^‚ğ“¾‚é
	reg = &render.bgreg[page][y << 6];

	// BGƒ|ƒCƒ“ƒ^‚ğ“¾‚é
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 10);
	}
	else {
		ptr += (y << 9);
	}

	// ƒ‹[ƒv
	for (i=0; i<64; i++) {
		// æ“¾
		bgdata = reg[i];

		// $10000‚ª—§‚Á‚Ä‚¢‚ê‚ÎOK
		if (bgdata & 0x10000) {
			ptr += 1;
			continue;
		}

		// $10000‚ğOR
		reg[i] |= 0x10000;

		// pcgno‚ğ“¾‚é
		pcgno = bgdata & 0xfff;

		// ƒTƒCƒY•Ê
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
			if (bgdata & 0x8000) {
				// ã‰º”½“]
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// ’Êí
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
			// 8x8Bbit17,bit18‚ğl—¶‚·‚é
			pcgbuf = &render.pcgbuf[(pcgno << 8)];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// ã‰º”½“]
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf -= 0x10;
					ptr += 64;
				}
			}
			else {
				// ’Êí
				for (j=0; j<8; j++) {
					ptr->pcg = pcgbuf;
					ptr->bg = bgdata;
					pcgbuf += 0x10;
					ptr += 64;
				}
			}
			ptr -= 512;
		}

		// “o˜^ˆ—(PCG)
		render.pcguse[pcgno]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[pcgno]++;

		// ƒ|ƒCƒ“ƒ^‚ği‚ß‚é
		ptr += 1;
	}
}

//---------------------------------------------------------------------------
//
//	BG/ƒXƒvƒ‰ƒCƒgƒ`ƒFƒbƒN
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGSpriteCheck(int raster)
{
	// ‚Q“x“Ç‚İ‚Í‹ô”ƒ‰ƒXƒ^[‚Ì‚İ(ƒRƒbƒgƒ“)
	if (render.mixmode == 2 && raster & 1) {
		return;
	}

	// ƒtƒ‰ƒOƒIƒt
	render.bgsp = FALSE;

	// ƒXƒvƒ‰ƒCƒg•\¦Ø‘Ö‚¦ƒ`ƒFƒbƒN‚·‚é
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
//	ƒŒƒ“ƒ_ƒ‰(‡¬•”)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	‡¬
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

	// ‡¬w¦‚ª–³‚¢ê‡AyƒI[ƒo[‚Ìê‡return
	if (!render.mix[raster]) {
		return;
	}

	if (render.mixheight <= raster) {
		return;
	}

	// ‡¬’·‚ÌŒˆ’è
	mixlen = render.mixlen - (xoffset << 4);

	// ‡¬‚µ‚È‚¢
	if (mixlen<=0) {
		return;
	}

#if defined(REND_LOG)
	LOG1(Log::Normal, "‡¬ raster=%d", raster);
#endif	// REND_LOG

	// ƒtƒ‰ƒOOFF
	// …•½•`‰æ“r’†‚Ì‘‚«Š·‚¦‚ÍŸ‰ñXV•K—v‚È‚Ì‚ÅOFF‚µ‚È‚¢
	if (xoffset==0) {
		render.mix[raster] = FALSE;
	}

	// ƒoƒbƒtƒ@‡¬ˆÊ’u
	mixy = raster;

	// •\¦ü”g”15kHz‚ÅƒCƒ“ƒ^ƒŒ[ƒX‚Å–³‚¢
 	if (render.mixmode != 1 && render.hres == 0) {
		mixy <<= 1;
	}

	// ‚±‚Ìƒ‰ƒCƒ“‚ÍXV‚·‚é‰Â”\«—L‚è
	render.draw[mixy] = TRUE;

	// ‡¬ƒoƒbƒtƒ@ƒAƒhƒŒƒX‰Šú‰»
	mixbuf = &render.mixbuf[render.mixwidth * mixy];
	mixbuf += (xoffset << 4);
	mixflg = render.drawflag + (mixy << 6);
	mixflg += xoffset;

	switch (render.mixtype) {

		// ƒ^ƒCƒv0(•\¦‚µ‚È‚¢)
		case 0:
			RendMix00(mixbuf, mixflg, mixlen);
			break;

		// ƒ^ƒCƒv1(ƒeƒLƒXƒg‚Ì‚İ)
		case 1:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			RendMix01(mixbuf, ptr[0], mixflg, mixlen);
			break;

		// ƒ^ƒCƒv2(ƒXƒvƒ‰ƒCƒg‚Ì‚İ)
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

		// ƒ^ƒCƒv3(ƒOƒ‰ƒtƒBƒbƒN‚Ì‚İ)
		case 3:
			ptr[2]=MixGrp(grpbuf, raster, xoffset, mixlen);

			// ”¼“§–¾
			if (vp->exon && vp->hp && vp->gt) {
				RendMix01H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else {
				RendMix01(mixbuf, ptr[2], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv4(ƒeƒLƒXƒgAƒXƒvƒ‰ƒCƒg)
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

		// ƒ^ƒCƒv5(ƒeƒLƒXƒg or ƒXƒvƒ‰ƒCƒg’†ŠÔ‚Ü‚½‚ÍŒã–ÊAƒOƒ‰ƒtƒBƒbƒN‘O–Ê)
		case 5:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// ”¼“§–¾
				RendMix02BH(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix02B(mixbuf, ptr[2], ptr[0], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv6(ƒeƒLƒXƒg or ƒXƒvƒ‰ƒCƒg‘O–ÊAƒOƒ‰ƒtƒBƒbƒN’†ŠÔAŒã–Ê‚È‚µ)
		case 6:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// ”¼“§–¾
				RendMix02CH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// “Áêƒvƒ‰ƒCƒIƒŠƒeƒB
				RendMix02CS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix02C(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv7(ƒeƒLƒXƒg or ƒXƒvƒ‰ƒCƒg‘O–Ê‚Ü‚½‚Í’†ŠÔAƒOƒ‰ƒtƒBƒbƒNŒã–Ê)
		case 7:
			y = *render.mixy[0];
			y += (((raster + render.mixraster[0]) >> render.mixrshift[0]) << render.mixlshift[0]);
			y &= render.mixandy[0];
			ptr[0] = render.mixptr[0];
			ptr[0] += (y << render.mixshift[0]);
			ptr[0] += (*render.mixx[0] + (xoffset << 4)) & render.mixandx[0];
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);

			if (vp->exon && vp->hp && vp->gt) {
				// ”¼“§–¾
				RendMix02DH(mixbuf, ptr[0], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// “Áêƒvƒ‰ƒCƒIƒŠƒeƒB
				RendMix02DS(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix02D(mixbuf, ptr[0], ptr[2], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv8(ƒeƒLƒXƒgAƒXƒvƒ‰ƒCƒgAƒOƒ‰ƒtƒBƒbƒN‘O–Ê)
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
				// ”¼“§–¾
				RendMix03AH(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix03A(mixbuf, ptr[2], ptr[0], ptr[1], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv9(ƒeƒLƒXƒgAƒXƒvƒ‰ƒCƒgAƒOƒ‰ƒtƒBƒbƒN’†ŠÔ)
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
				// ”¼“§–¾
				RendMix03BH(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// “Áêƒvƒ‰ƒCƒIƒŠƒeƒB
				RendMix03BS(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix03B(mixbuf, ptr[0], ptr[2], ptr[1], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv10(ƒeƒLƒXƒgAƒXƒvƒ‰ƒCƒgAƒOƒ‰ƒtƒBƒbƒNŒã–Ê)
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
				// ”¼“§–¾
				RendMix03CH(mixbuf, ptr[0], ptr[1], ptr[2], render.paldata[0x100], mixflg, mixlen);
			} else if (vp->exon && !vp->hp) {
				// “Áêƒvƒ‰ƒCƒIƒŠƒeƒB
				RendMix03CS(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			} else {
				// ’Êí
				RendMix03C(mixbuf, ptr[0], ptr[1], ptr[2], mixflg, mixlen);
			}
			break;

		// ƒ^ƒCƒv11(ƒpƒŒƒbƒg‚O”¼“§–¾)
		case 11:
			ptr[2] = MixGrp(grpbuf, raster, xoffset, mixlen);
			RendMixP0H(mixbuf, ptr[2], render.paldata[0x100], mixflg, mixlen);
			break;

		// ‚»‚Ì‘¼
		default:
			ASSERT(FALSE);
			break;
	}

	// •\¦ü”g”15kHz‚ÅƒCƒ“ƒ^ƒŒ[ƒX‚Å–³‚¢‚Æ‚«
 	if (render.mixmode != 1 && render.hres == 0) {
		if (render.scanline) {
			// ƒXƒLƒƒƒ“ƒ‰ƒCƒ“•`‰æ
			memset(mixbuf + render.mixwidth, 0x00, render.mixlen << 2);
		} else {
			// ƒ‰ƒXƒ^[ƒRƒs[
			memcpy(mixbuf + render.mixwidth, mixbuf, render.mixlen << 2);
		}

		// ƒXƒLƒƒƒ“ƒ‰ƒCƒ“‚Ìƒ‰ƒXƒ^‚àƒtƒ‰ƒO‚ğİ’è
		memcpy(mixflg + 64, mixflg, (64 - xoffset) << 2);
		render.draw[mixy + 1] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	ƒOƒ‰ƒtƒBƒbƒNƒX‡¬
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

	// 0–Ê(‚ ‚è‚¦‚È‚¢)
	if (render.mixpage==0) {
		ASSERT(FALSE);
		return buf;
	}

	// ƒ[ƒN‰Šú‰»
	mixpage = 0;
	for (i=0; i<4; i++) {
		map[i] = -1;
	}

	// ƒoƒbƒtƒ@ŒvZ
	j = 0;
	for (i=0; i<render.mixpage; i++) {
		idx = i + 4;

		y = *render.mixy[idx];
		y += (((raster + render.mixraster[idx]) >> render.mixrshift[idx]) << render.mixlshift[idx]);
		y &= render.mixandy[idx];
		ptr[i] = render.mixptr[idx];
		ptr[i] += (y << render.mixshift[idx]);
		ptr[i] += (*render.mixx[idx] + (xoffset << 4)) & render.mixandx[idx];

		// ÀÛ‚Ì•\¦ƒy[ƒW”‚ğƒJƒEƒ“ƒg
		if (render.grppen[i]) {
			mixpage++;
			map[j++] = i;
		}
	}

	// 0–Ê‡¬(‡¬‚µ‚È‚¢)
	if (mixpage==0) {
		return buf;
	}

	// 1–Ê‚Å1–Ê•\¦
	if (render.mixpage == 1 && mixpage==1) {
		memcpy(buf, ptr[map[0]], mixlen << 2);
	}

	// 2–ÊˆÈã
	if (render.mixpage >= 2) {

		// ”¼“§–¾‚ğƒ`ƒFƒbƒN(2–ÊˆÈã‚Å‚©‚Âƒx[ƒXƒy[ƒW‚ª•\¦)
		if (vp->exon && vp->hp && vp->gg && render.grppen[0]) {
			if (render.grppen[1]) {
				// ƒx[ƒX•\¦AƒZƒJƒ“ƒh•\¦
				RendGrp02HBS(buf, ptr[0], ptr[1], mixlen);

				if (mixpage==3) {
					RendGrp02(buf, buf, ptr[map[2]], mixlen);
				}

				if (mixpage==4) {
					RendGrp03(buf, buf, ptr[map[2]], ptr[map[3]], mixlen);
				}
			} else {
				// ƒx[ƒX•\¦AƒZƒJƒ“ƒh”ñ•\¦
				// ƒZƒJƒ“ƒhƒy[ƒW‚ª•\¦‚³‚ê‚Ä‚¢‚È‚¢ê‡‚Í”¼“§–¾‚Ì•”•ª‚Ì‚İ‡¬
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
				// 1–Ê•\¦
				memcpy(buf, ptr[map[0]], mixlen << 2);
			} else if (mixpage==2) {
				// 2–Ê‡¬
				RendGrp02(buf, ptr[map[0]], ptr[map[1]], mixlen);
			} else if (mixpage==3) {
				// 3–Ê‡¬
				RendGrp03(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], mixlen);
			} else {
				// 4–Ê‡¬
				RendGrp04(buf, ptr[map[0]], ptr[map[1]], ptr[map[2]], ptr[map[3]], mixlen);
			}
		}
	}

	// •Ô‹p
	return buf;
}

//---------------------------------------------------------------------------
//
//	‡¬ƒoƒbƒtƒ@æ“¾
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULL‚Ìê‡‚à‚ ‚è
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	‡¬(‹­§)
//
//---------------------------------------------------------------------------
void FASTCALL Render::UpdateMixBuf()
{
	BOOL bScanEven;
	int i;
	int j;

	ASSERT(this);

	// ƒCƒ“ƒ^ƒŒ[ƒX‚ÌƒXƒLƒƒƒ“ƒ‰ƒCƒ“‹ôŠïæ“¾
	bScanEven = cp->v_scaneven;

	// ƒCƒ“ƒ^ƒŒ[ƒX‚É”õ‚¦‚Ä‚Q‰ñ‡¬‚·‚é
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
//	ƒŒƒ“ƒ_ƒŠƒ“ƒO
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
	// ˆÀ‘Sô
	if (render.mixlen <= 0) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ƒrƒfƒIˆ—");
#endif	// RENDER_LOG
		Video();
	}

	// ƒpƒŒƒbƒg
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ƒpƒŒƒbƒgˆ—");
#endif	// RENDER_LOG
		Palette();
	}

	// ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹ƒ`ƒFƒbƒN
	if (render.grpscrl) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹ƒ`ƒFƒbƒNˆ—");
#endif	// RENDER_LOG
		GrpScrlCheck();
	}

	// BGƒXƒvƒ‰ƒCƒgƒ`ƒFƒbƒN
	if (render.bgsp) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "BG/ƒXƒvƒ‰ƒCƒgƒ`ƒFƒbƒNˆ—");
#endif	// RENDER_LOG
		BGSpriteCheck(raster);
	}

	// ƒeƒLƒXƒgXV
	if (render.textdirty) {
		memset(render.textpal, TRUE, 1024);
		render.textdirty = FALSE;
	}

	// ƒOƒ‰ƒtƒBƒbƒNXV
	if (render.grpdirty) {
		memset(render.grppal, TRUE, 512 * 4);
		render.grpdirty = FALSE;
	}

	// BGƒXƒvƒ‰ƒCƒgXV
	if (render.bgspdirty) {
		memset(render.bgspmod, TRUE, 1024);
		render.bgspdirty = FALSE;
	}

	// ‡¬‘SXV
	if (render.mixdirty) {
		memset(render.mix, TRUE, render.mixheight);
		render.mixdirty = FALSE;
	}

	// ƒCƒ“ƒ^ƒŒ[ƒX‚È‚ç‹ô”‚ÆŠï”ƒ‰ƒXƒ^[‚ğˆêŠ‡‡¬
	if (render.mixmode == 1) {
		raster <<= 1;
		if (render.mixeven) {
			// ‡¬Às(‹ô”ƒ‰ƒCƒ“)
			Text(raster);
			Grp(0, raster);
			Grp(1, raster);
			Grp(2, raster);
			Grp(3, raster);
			BGSprite(raster);
			Mix(raster, xoffset);

			// ‡¬Às(Šï”ƒ‰ƒCƒ“)
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

	// ‡¬Às
	Text(raster);
	Grp(0, raster);
	Grp(1, raster);
	Grp(2, raster);
	Grp(3, raster);
	BGSprite(raster);
	Mix(raster, xoffset);
}
