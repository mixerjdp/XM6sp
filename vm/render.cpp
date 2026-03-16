//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ?o?h?D(ytanaka@ipc-tokai.or.jp)
//	[ ?????_?? ]
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
//	?????_??
//
//===========================================================================
//#define REND_LOG

//---------------------------------------------------------------------------
//
//	????`
//
//---------------------------------------------------------------------------
#define REND_COLOR0		0x80000000		// ?J???[0?t???O(rend_asm.asm??g?p)
#define REND_PX68K_IBIT	0x40000000		// px68k Ibit marker (XRGB8888: top byte ignored)
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
//	?R???X?g???N?^
//
//---------------------------------------------------------------------------
Render::Render(VM *p) : Device(p)
{
	// ?f?o?C?XID???????
	dev.id = MAKEID('R', 'E', 'N', 'D');
	dev.desc = "Renderer";

	// ?f?o?C?X?|?C???^
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
	render.fast_stamp_counter = 1;
	memset(render.fast_mix_stamp, 0, sizeof(render.fast_mix_stamp));
	memset(render.fast_mix_done, 0, sizeof(render.fast_mix_done));
	memset(render.fast_bg_stamp, 0, sizeof(render.fast_bg_stamp));
	memset(render.fast_bg_done, 0, sizeof(render.fast_bg_done));

	// ???[?N?G???A??????(CRTC)
	render.crtc = FALSE;
	render.width = 768;
	render.h_mul = 1;
	render.height = 512;
	render.v_mul = 1;

	// ???[?N?G???A??????(?p???b?g)
	render.palbuf = NULL;
	render.palptr = NULL;
	render.palvc = NULL;

	// ???[?N?G???A??????(?e?L?X?g)
	render.textflag = NULL;
	render.texttv = NULL;
	render.textbuf = NULL;
	render.textout = NULL;

	// ???[?N?G???A??????(?O???t?B?b?N)
	render.grpflag = NULL;
	render.grpgv = NULL;
	render.grpbuf[0] = NULL;
	render.grpbuf[1] = NULL;
	render.grpbuf[2] = NULL;
	render.grpbuf[3] = NULL;

	// ???[?N?G???A??????(PCG,?X?v???C?g,BG)
	render.pcgbuf = NULL;
	render.spptr = NULL;
	render.bgspbuf = NULL;
	render.zero = NULL;
	render.bgptr[0] = NULL;
	render.bgptr[1] = NULL;

	// ???[?N?G???A??????(????)
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

	// ???[?N?G???A??????(?`??)
	render.drawflag = NULL;

	// ?????
	cmov = FALSE;
}

//---------------------------------------------------------------------------
//
//	??????
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Init()
{
	int i;

	ASSERT(this);

	// ??{?N???X
	if (!Device::Init()) {
		return FALSE;
	}

	// CRTC?��
	crtc = (CRTC*)vm->SearchDevice(MAKEID('C', 'R', 'T', 'C'));
	ASSERT(crtc);

	// VC?��
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// ?p???b?g?o?b?t?@?m??(4MB x2: original + px68k-safe for fast)
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

	// ?e?L?X?gVRAM?o?b?t?@?m??(4.7MB)
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

	// ?O???t?B?b?NVRAM?o?b?t?@?m??(8.2MB)
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

	// PCG?o?b?t?@?m??(4MB)
	try {
		render.pcgbuf = new DWORD[ 16 * 256 * 16 * 16 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.pcgbuf) {
		return FALSE;
	}

	// ?X?v???C?g?|?C???^?m??(256KB)
	try {
		render.spptr = new DWORD*[ 128 * 512 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!render.spptr) {
		return FALSE;
	}

	// BG?|?C???^?m??(768KB)
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

	// BG/?X?v???C?g?o?b?t?@?m??(1MB)
	try {
		render.bgspbuf = new DWORD[ 512 * 512 + 16];	// +16??b??[?u
	}
	catch (...) {
		return FALSE;
	}
	if (!render.bgspbuf) {
		return FALSE;
	}

	// ?`??t???O?o?b?t?@?m??(256KB)
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

	// ?p???b?g??
	MakePalette();

	// ????????[?N?G???A
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
//	?N???[???A?b?v
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

	// ?`??t???O
	if (render.drawflag) {
		delete[] render.drawflag;
		render.drawflag = NULL;
	}

	// BG/?X?v???C?g?o?b?t?@
	if (render.bgspbuf) {
		delete[] render.bgspbuf;
		render.bgspbuf = NULL;
	}

	// BG?|?C???^
	if (render.bgptr[0]) {
		delete[] render.bgptr[0];
		render.bgptr[0] = NULL;
	}
	if (render.bgptr[1]) {
		delete[] render.bgptr[1];
		render.bgptr[1] = NULL;
	}

	// ?X?v???C?g?|?C???^
	if (render.spptr) {
		delete[] render.spptr;
		render.spptr = NULL;
	}

	// PCG?o?b?t?@
	if (render.pcgbuf) {
		delete[] render.pcgbuf;
		render.pcgbuf = NULL;
	}

	// ?O???t?B?b?NVRAM?o?b?t?@
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

	// ?e?L?X?gVRAM?o?b?t?@
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

	// ?p???b?g?o?b?t?@ (original + fast)
	if (palbuf_original) {
		delete[] palbuf_original;
		palbuf_original = NULL;
	}
	if (palbuf_fast) {
		delete[] palbuf_fast;
		palbuf_fast = NULL;
	}
	render.palbuf = NULL;

	// ??{?N???X??
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	???Z?b?g
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

	// ?r?f?I?R???g???[?????|?C???^?��
	ASSERT(vc);
	render.palvc = (const WORD*)vc->GetPalette();

	// ?e?L?X?gVRAM???|?C???^?��
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);
	render.texttv = tvram->GetTVRAM();

	// ?O???t?B?b?NVRAM???|?C???^?��
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);
	render.grpgv = gvram->GetGVRAM();

	// ?X?v???C?g?R???g???[?????|?C???^?��
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);
	render.sprmem = sprite->GetPCG() - 0x8000;

	// ???[?N?G???A??????
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

	// ???[?N?G???A??????(crtc, vc)
	render.crtc = FALSE;
	render.vc = FALSE;

	// ???[?N?G???A??????(?R???g???X?g)
	render.contrast = FALSE;

	// ???[?N?G???A??????(?p???b?g)
	render.palette = FALSE;
	render.palptr = render.palbuf;

	// ???[?N?G???A??????(?e?L?X?g)
	render.texten = FALSE;
	render.textx = 0;
	render.texty = 0;

	// ???[?N?G???A??????(?O???t?B?b?N)
	for (i=0; i<4; i++) {
		render.grpen[i] = FALSE;
		render.grpx[i] = 0;
		render.grpy[i] = 0;
	}
	render.grptype = 4;

	// ???[?N?G???A??????(PCG)
	// ???Z?b?g?????BG,Sprite???????\?????????PCG????g?p
	memset(render.pcgready, 0, sizeof(render.pcgready));
	memset(render.pcguse, 0, sizeof(render.pcguse));
	memset(render.pcgpal, 0, sizeof(render.pcgpal));

	// ???[?N?G???A??????(?X?v???C?g)
	memset(render.spptr, 0, sizeof(DWORD*) * 128 * 512);
	memset(render.spreg, 0, sizeof(render.spreg));
	memset(render.spuse, 0, sizeof(render.spuse));

	// ???[?N?G???A??????(BG)
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

	// ???[?N?G???A??????(BG/?X?v???C?g)
	render.bgspflag = FALSE;
	render.bgspdisp = FALSE;
	memset(render.bgspmod, 0, sizeof(render.bgspmod));

	// BG???????????????(?????0000)
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

	// ???[?N?G???A??????(????)
	render.mixtype = 0;
	if (backend) {
		backend->Activate(this);
	}
}

//---------------------------------------------------------------------------
//
//	?Z?[?u
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Save(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "?Z?[?u");


	return TRUE;
}

//---------------------------------------------------------------------------
//
//	???[?h
//
//---------------------------------------------------------------------------
BOOL FASTCALL Render::Load(Fileio *fio, int ver)
{
	ASSERT(this);
	LOG0(Log::Normal, "???[?h");

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	???K?p
//
//---------------------------------------------------------------------------
void FASTCALL Render::ApplyCfg(const Config *config)
{
	ASSERT(config);
	LOG0(Log::Normal, "???K?p");
}

//---------------------------------------------------------------------------
//
//	?t???[???J?n
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

void FASTCALL Render::StartFrameFast()
{
	render.count = 0;
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

	// ????t???[????X?L?b?v????
	if ((render.count != 0) || !render.enable) {
		render.act = FALSE;
		return;
	}

	// ????t???[????????_?????O????
	render.act = TRUE;

	// ???X?^??N???A
	render.first = 0;
	render.last = -1;

	// CRTC?t???O?????
	if (render.crtc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "CRTC????");
#endif	// REND_LOG

		// ?f?[?^?��
		crtc->GetCRTC(&crtcdata);

		// h_dots?Av_dots??0?????
		if ((crtcdata.h_dots == 0) || (crtcdata.v_dots == 0)) {
			return;
		}

		// ????R?s?[
		render.width = crtcdata.h_dots;
		render.h_mul = crtcdata.h_mul;
		render.height = crtcdata.v_dots;
		render.v_mul = crtcdata.v_mul;
		render.lowres = crtcdata.lowres;
		if ((render.v_mul == 2) && !render.lowres) {
			render.height >>= 1;
		}

		// ?????o?b?t?@???????????
		render.mixlen = render.width;
		if (render.mixwidth < render.width) {
			render.mixlen = render.mixwidth;
		}

		// ?X?v???C?g???Z?b?g(mixlen??????????)
		SpriteReset();

		// ?S???C??????
		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}

		// ?I?t
		render.crtc = FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	?t???[???I??
//
//---------------------------------------------------------------------------
void FASTCALL Render::EndFrameOriginal()
{
	ASSERT(this);

	// ??????��??????
	if (!render.act) {
		return;
	}

	// ??????????X?^?????
	if (render.last > 0) {
		render.last = render.height;
		Process();
	}

	// ?J?E???gUp
	render.count++;

	// ??????
	render.act = FALSE;
}

//---------------------------------------------------------------------------
//
//	?????o?b?t?@?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetMixBuf(DWORD *buf, int width, int height)
{
	int i;

	ASSERT(this);
	ASSERT(width >= 0);
	ASSERT(height >= 0);

	// ???
	render.mixbuf = buf;
	render.mixwidth = width;
	render.mixheight = height;

	// ?????o?b?t?@???????????
	render.mixlen = render.width;
	if (render.mixwidth < render.width) {
		render.mixlen = render.mixwidth;
	}

	// ???????????w??
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	CRTC?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetCRTCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// ?t???OON???
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
//	VC?Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetVCOriginal()
{
	int i;
	int from;
	DWORD stamp;

	ASSERT(this);

	// ?t???OON???
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
//	VC????
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

	// VC?t???O??~??
	render.vc = FALSE;

	// ?t???OON
	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}

	// VC?f?[?^?ACRTC?f?[?^??��
	p = vc->GetWorkAddr();
	q = crtc->GetWorkAddr();

	// ?e?L?X?g?C?l?[?u??
	if (p->ton && !q->tmem) {
		render.texten = TRUE;
	}
	else {
		render.texten = FALSE;
	}

	// ?O???t?B?b?N?^?C?v
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

	// ?O???t?B?b?N?C?l?[?u???A?D??x?}?b?v
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
					// ?y?[?W0??`?F?b?N
					if ((p->gp[i * 2 + 0] == 0) && (p->gp[i * 2 + 1] == 1)) {
						if (p->gs[i * 2 + 0]) {
							map[i] = 0;
							render.grpen[0] = TRUE;
							render.mixpage++;
						}
					}
					// ?y?[?W1??`?F?b?N
					if ((p->gp[i * 2 + 0] == 2) && (p->gp[i * 2 + 1] == 3)) {
						if (p->gs[i * 2 + 0]) {
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

	// ?O???t?B?b?N?o?b?t?@??Z?b?g
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

	// ?D?????��
	tx = p->tx;
	sp = p->sp;
	gr = p->gr;

	// ?^?C?v??????
	render.mixtype = 0;

	// BG/?X?v???C?g?\??????
	if ((q->hd >= 2) || (!p->son)) {
		if (render.bgspflag) {
			// BG/?X?v???C?g?\??ON->OFF
			render.bgspflag = FALSE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}
	else {
		if (!render.bgspflag) {
			// BG/?X?v???C?g?\??OFF->ON
			render.bgspflag = TRUE;
			for (i=0; i<512; i++) {
				render.bgspmod[i] = TRUE;
			}
			render.bgspdisp = sprite->IsDisplay();
		}
	}

	// ???(q->hd >= 2?????X?v???C?g????)
	if ((q->hd >= 2) || (!p->son)) {
		// ?X?v???C?g???
		if (!render.texten) {
			// ?e?L?X?g???
			if (render.mixpage == 0) {
				// ?O???t?B?b?N???(type=0)
				render.mixtype = 0;
				return;
			}
			if (render.mixpage == 1) {
				// ?O???t?B?b?N1????(type=1)
				render.mixptr[0] = ptr[0];
				render.mixshift[0] = shift[0];
				render.mixx[0] = &render.grpx[ map[0] ];
				render.mixy[0] = &render.grpy[ map[0] ];
				render.mixand[0] = an[0];
				render.mixtype = 1;
				return;
			}
			if (render.mixpage == 2) {
				// ?O???t?B?b?N2????(type=2)
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
			// ?O???t?B?b?N3??????(type=4)
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
		// ?e?L?X?g????
		if (render.mixpage == 0) {
			// ?O???t?B?b?N????B?e?L?X?g???(type=1)
			render.mixptr[0] = render.textout;
			render.mixshift[0] = 10;
			render.mixx[0] = &render.textx;
			render.mixy[0] = &render.texty;
			render.mixand[0] = 1024 - 1;
			render.mixtype = 1;
				return;
		}
		if (render.mixpage == 1) {
			// ?e?L?X?g+?O???t?B?b?N1??
			if (tx < gr) {
				// ?e?L?X?g?O??(type=3)
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
			// ?O???t?B?b?N?O??(type=3,tx=gr??O???t?B?b?N?O??A???II)
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
		// ?e?L?X?g+?O???t?B?b?N2????(type=5, type=6)
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

	// ?X?v???C?g????
	if (!render.texten) {
		// ?e?L?X?g???
		if (render.mixpage == 0) {
			// ?O???t?B?b?N????A?X?v???C?g???(type=1)
			render.mixptr[0] = render.bgspbuf;
			render.mixshift[0] = 9;
			render.mixx[0] = &render.zero;
			render.mixy[0] = &render.zero;
			render.mixand[0] = 512 - 1;
			render.mixtype = 1;
			return;
		}
		if (render.mixpage == 1) {
			// ?X?v???C?g+?O???t?B?b?N1??(type=3)
			if (sp < gr) {
				// ?X?v???C?g?O??
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
			// ?O???t?B?b?N?O??(sp=gr??s??)
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
		// ?X?v???C?g+?O???t?B?b?N2????(type=5, type=6)
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

	// ?e?L?X?g????
	if (render.mixpage == 0) {
		// ?O???t?B?b?N????B?e?L?X?g?{?X?v???C?g(type=3)
		if (tx <= sp) {
			// tx=sp??e?L?X?g?O??(LMZ2)
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
		// ?X?v???C?g?O??
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

	// ?D???????
	if (tx == 3) tx--;
	if (sp == 3) sp--;
	if (gr == 3) gr--;
	if (tx == sp) {
		// ?K???????????
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
		// ?K???????????
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
		// ?K???????????
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
		// ?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N1??(type=7)
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

	// ?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N?Q????(type=8)
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
//	?R???g???X?g???
//
//---------------------------------------------------------------------------
void FASTCALL Render::SetContrast(int cont)
{
	// ?V?X?e???|?[?g????_???v?`?F?b?N??s?????A?????????????
	ASSERT(this);
	ASSERT((cont >= 0) && (cont <= 15));

	// ??X??t???OON
	render.contlevel = cont;
	render.contrast = TRUE;
}

//---------------------------------------------------------------------------
//
//	?R???g???X?g?��
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
//	?R???g???X?g????
//
//---------------------------------------------------------------------------
void FASTCALL Render::Contrast()
{
	int i;

	// ?|?C???g??u???X?A?t???ODown
	render.palptr = render.palbuf;
	render.palptr += (render.contlevel << 16);
	render.contrast = FALSE;

	// ?p???b?g?t???O??S??Up
	for (i=0; i<0x200; i++) {
		render.palmod[i] = TRUE;
	}
	render.palette = TRUE;
}

//---------------------------------------------------------------------------
//
//	?p???b?g??
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

	// --- px68k-safe palette table for fast compositor ---
	// Matches px68k's "avoid zero output for non-zero source" behavior:
	// if a non-zero 15-bit color collapses to RGB=0 after contrast scaling,
	// clamp it to the smallest visible blue step so it won't act like "transparent"
	// in px68k-style pipelines.
	auto conv_px68k_safe = [](int color, int ratio) -> DWORD {
		DWORD r = (DWORD)color;
		DWORD g = (DWORD)color;
		DWORD b = (DWORD)color;
		r <<= 13; r &= 0xf80000;
		g &= 0x00f800;
		b <<= 2;  b &= 0x0000f8;
		if (color & 1) {
			r |= 0x070000;
			g |= 0x000700;
			b |= 0x000007;
		}
		b *= ratio; b >>= 8;
		g *= ratio; g >>= 8; g &= 0xff00;
		r *= ratio; r >>= 8; r &= 0xff0000;
		DWORD out = (DWORD)(r | g | b);
		if (color & 1) {
			out |= REND_PX68K_IBIT;
		}

		// px68k "avoid 0" hack: if a non-zero source color would quantize to 0 in 565I,
		// clamp to the smallest visible blue step.
		if (color) {
			const DWORD rgb = out & 0x00ffffff;
			const WORD r5 = (WORD)((rgb >> 19) & 0x1f);
			const WORD g5 = (WORD)((rgb >> 11) & 0x1f);
			const WORD b5 = (WORD)((rgb >>  3) & 0x1f);
			WORD packed = (WORD)((r5 << 11) | (g5 << 6) | b5);
			if (out & REND_PX68K_IBIT) {
				packed |= 0x0020;
			}
			if (packed == 0) {
				out |= 0x000008;	// ensures b5=1 after RGB565I quantization
			}
		}
		return out;
	};

	p = palbuf_fast;
	for (i=0; i<16; i++) {
		ratio = 256 - ((15 - i) << 4);
		for (j=0; j<0x10000; j++) {
			*p++ = conv_px68k_safe(j, ratio);
		}
	}
}

//---------------------------------------------------------------------------
//
//	?p???b?g???
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

	// ?S??R?s?[
	r = (DWORD)color;
	g = (DWORD)color;
	b = (DWORD)color;

	// MSB????G:5?AR:5?AB:5?AI:1????????????
	// ????? R:8 G:8 B:8??DWORD?????Bb31-b24??g????
	r <<= 13;
	r &= 0xf80000;
	g &= 0x00f800;
	b <<= 2;
	b &= 0x0000f8;

	// ?P?x?r?b?g???Up(???f?[?^??0?????A!=0???????????)
	if (color & 1) {
		r |= 0x070000;
		g |= 0x000700;
		b |= 0x000007;
	}

	// ?R???g???X?g??e????????
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
//	?p???b?g?��
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
//	?p???b?g????
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

	// ?t???OOFF
	tx = FALSE;
	gr = FALSE;
	sp = FALSE;

	// ?O???t?B?b?N
	for (i=0; i<0x100; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?O???t?B?b?N??e???A?t???OOFF
			gr = TRUE;
			render.palmod[i] = FALSE;

			// ?????F?????
			if (i == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// 65536?F??????p???b?g?f?[?^???
			j = i >> 1;
			if (i & 1) {
				j += 128;
			}
			render.pal64k[j * 2 + 0] = (BYTE)(data >> 8);
			render.pal64k[j * 2 + 1] = (BYTE)data;
		}
	}

	// ?e?L?X?g???X?v???C?g
	for (i=0x100; i<0x110; i++) {
		if (render.palmod[i]) {
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];

			// ?e?L?X?g??e???A?t???OOFF
			tx = TRUE;
			render.palmod[i] = FALSE;

			// ?????F?????
			if (i == 0x100) {
				render.paldata[i] |= REND_COLOR0;
				// 0x100??BG?E?X?v???C?g???K???e??
				sp = TRUE;
			}

			// PCG????
			memset(&render.pcgready[0], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[0] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?X?v???C?g
	for (i=0x110; i<0x200; i++) {
		if (render.palmod[i]) {
			// ?X?v???C?g??e???A?t???OOFF
			data = (DWORD)render.palvc[i];
			render.paldata[i] = render.palptr[data];
			render.palmod[i] = FALSE;

			// ?????F?????
			if ((i & 0x00f) == 0) {
				render.paldata[i] |= REND_COLOR0;
			}

			// PCG????
			memset(&render.pcgready[(i & 0xf0) << 4], 0, sizeof(BOOL) * 256);
			if (render.pcgpal[(i & 0xf0) >> 4] > 0) {
				sp = TRUE;
			}
		}
	}

	// ?O???t?B?b?N?t???O
	if (gr) {
		// ?t???OON
		for (i=0; i<512*4; i++) {
			render.grppal[i] = TRUE;
		}
	}

	// ?e?L?X?g?t???O
	if (tx) {
		for (i=0; i<1024; i++) {
			render.textpal[i] = TRUE;
		}
	}

	// ?X?v???C?g?t???O
	if (sp) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}

	// ?p???b?g?t???OOFF
	render.palette = FALSE;
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?X?N???[??
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextScrl(DWORD x, DWORD y)
{
	int i;

	ASSERT(this);
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ??r?`?F?b?N
	if ((render.textx == x) && (render.texty == y)) {
		return;
	}

	// ???[?N?X?V
	render.textx = x;
	render.texty = y;

	// ?t???OON
	if (render.texten) {
#if defined(REND_LOG)
		LOG2(Log::Normal, "?e?L?X?g?X?N???[?? x=%d y=%d", x, y);
#endif	// REND_LOG

		for (i=0; i<1024; i++) {
			render.mix[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?R?s?[
//
//---------------------------------------------------------------------------
void FASTCALL Render::TextCopy(DWORD src, DWORD dst, DWORD plane)
{
	ASSERT(this);
	ASSERT((src >= 0) && (src < 256));
	ASSERT((dst >= 0) && (dst < 256));
	ASSERT(plane < 16);

	// ?A?Z???u???T?u
	RendTextCopy(&render.texttv[src << 9],
				 &render.texttv[dst << 9],
				 plane,
				 &render.textflag[dst << 7],
				 &render.textmod[dst << 2]);
}

//---------------------------------------------------------------------------
//
//	?e?L?X?g?o?b?t?@?��
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
//	?e?L?X?g????
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

	// ?f?B?Z?[?u????��??????
	if (!render.texten) {
		return;
	}

	// ?????Y?Z?o
	y = (raster + render.texty) & 0x3ff;

	// ??X?t???O(?????^)
	if (render.textmod[y]) {
		// ?t???O????
		render.textmod[y] = FALSE;
		render.mix[raster] = TRUE;

		// ???????????
		RendTextMem(render.texttv + (y << 7),
					render.textflag + (y << 5),
					render.textbuf + (y << 9));

		// ?????p???b?g???
		RendTextPal(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.textflag + (y << 5),
					render.paldata + 0x100);
	}

	// ?p???b?g(???^)
	if (render.textpal[y]) {
		// ?t???O????
		render.textpal[y] = FALSE;

		// ?????p???b?g???
		RendTextAll(render.textbuf + (y << 9),
					render.textout + (y << 10),
					render.paldata + 0x100);
		render.mix[raster] = TRUE;

		// y == 1023???R?s?[????
		if (y == 1023) {
			memcpy(render.textout + (1024 << 10), render.textout + (1023 << 10), sizeof(DWORD) * 1024);
		}
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N?o?b?t?@?��
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
//	?O???t?B?b?N?X?N???[??
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

	// ??r?`?F?b?N?B??\?????X?V???
	flag = FALSE;
	if ((render.grpx[block] != x) || (render.grpy[block] != y)) {
		render.grpx[block] = x;
		render.grpy[block] = y;
		flag = render.grpen[block];
	}

	// ?t???O????
	if (!flag) {
		return;
	}

#if defined(REND_LOG)
	LOG3(Log::Normal, "?O???t?B?b?N?X?N???[?? block=%d x=%d y=%d", block, x, y);
#endif	// REND_LOG

	for (i=0; i<1024; i++) {
		render.mix[i] = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N????
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
		// 1024???[?h??y?[?W0????
		if (!render.grpen[0]) {
			return;
		}
	}
	else {
		// ?????O
		if (!render.grpen[block]) {
			return;
		}
	}

	// ?^?C?v??
	switch (render.grptype) {
		// ?^?C?v0:1024?~1024 16Color
		case 0:
			// ?I?t?Z?b?g?Z?o
			offset = (raster + render.grpy[0]) & 0x3ff;
			y = offset & 0x1ff;

			// ?\?????`?F?b?N
			if ((offset < 512) && (block >= 2)) {
				return;
			}
			if ((offset >= 512) && (block < 2)) {
				return;
			}

			// ?p???b?g?????S?????
			if (render.grppal[y + (block << 9)]) {
				render.grppal[y + (block << 9)] = FALSE;
				render.grpmod[y + (block << 9)] = FALSE;
				for (i=0; i<32; i++) {
					render.grpflag[(y << 5) + (block << 14) + i] = FALSE;
				}
				switch (block) {
					// ??????u???b?N0???E
					case 0:
						if (Rend1024A(render.grpgv + (y << 10),
									render.grpbuf[0] + (offset << 11),
									render.paldata) != 0) {
							render.mix[raster] = TRUE;
						}
					case 1:
						break;
					// ????????u???b?N2???E
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

			// ?????O??grpmod????????
			if (!render.grpmod[y + (block << 9)]) {
				return;
			}
			render.grpmod[y + (block << 9)] = FALSE;
			render.mix[raster] = TRUE;
			switch (block) {
				// ?u???b?N0-????
				case 0:
					Rend1024C(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5),
								render.paldata);
					break;
				// ?u???b?N1-?E??
				case 1:
					Rend1024D(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					break;
				// ?u???b?N2-????
				case 2:
					Rend1024E(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					break;
				// ?u???b?N3-?E??
				case 3:
					Rend1024F(render.grpgv + (y << 10),
								render.grpbuf[0] + (offset << 11),
								render.grpflag + (y << 5) + 0xc000,
								render.paldata);
					break;
			}
			return;

		// ?^?C?v1:512?~512 16Color
		case 1:
			switch (block) {
				// ?y?[?W0
				case 0:
					y = (raster + render.grpy[0]) & 0x1ff;
					// ?p???b?g
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
					// ???
					if (render.grpmod[y]) {
						render.grpmod[y] = FALSE;
						render.mix[raster] = TRUE;
						Rend16B(render.grpgv + (y << 10),
								render.grpbuf[0] + (y << 10),
								render.grpflag + (y << 5),
								render.paldata);
					}
					return;
				// ?y?[?W1
				case 1:
					y = (raster + render.grpy[1]) & 0x1ff;
					// ?p???b?g
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
					// ???
					if (render.grpmod[y + 512]) {
						render.grpmod[y + 512] = FALSE;
						render.mix[raster] = TRUE;
						Rend16D(render.grpgv + (y << 10),
								render.grpbuf[1] + (y << 10),
								render.grpflag + (y << 5) + 0x4000,
								render.paldata);
					}
					return;
				// ?y?[?W2
				case 2:
					y = (raster + render.grpy[2]) & 0x1ff;
					// ?p???b?g
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
					// ???
					if (render.grpmod[y + 1024]) {
						render.grpmod[y + 1024] = FALSE;
						render.mix[raster] = TRUE;
						Rend16F(render.grpgv + (y << 10),
								render.grpbuf[2] + (y << 10),
								render.grpflag + (y << 5) + 0x8000,
								render.paldata);
					}
					return;
				// ?y?[?W3
				case 3:
					y = (raster + render.grpy[3]) & 0x1ff;
					// ?p???b?g
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
					// ???
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

		// ?^?C?v2:512?~512 256Color
		case 2:
			ASSERT((block == 0) || (block == 2));
			if (block == 0) {
				// ?I?t?Z?b?g?Z?o
				y = (raster + render.grpy[0]) & 0x1ff;

				// ?p???b?g?????S?????
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

				// ?????O??grpmod????????
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
				// ?I?t?Z?b?g?Z?o
				y = (raster + render.grpy[2]) & 0x1ff;

				// ?p???b?g?????S?????
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

				// ?????O??grpmod????????
				if (!render.grpmod[0x400 + y]) {
					return;
				}

				render.grpmod[0x400 + y] = FALSE;
				render.mix[raster] = TRUE;

				// ?????_?????O
				Rend256B(render.grpgv + (y << 10),
							render.grpbuf[2] + (y << 10),
							render.grpflag + 0x8000 + (y << 5),
							render.paldata);
			}
			return;

		// ?^?C?v3:512x512 ????`
		case 3:
		// ?^?C?v4:512x512 65536Color

		case 4:

			ASSERT(block == 0);
			// ?I?t?Z?b?g?Z?o
			y = (raster + render.grpy[0]) & 0x1ff;

			// ?p???b?g?????S?????
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

			// ?????O??grpmod????????
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
//	?????_??(BG?E?X?v???C?g??)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	?X?v???C?g???W?X?^???Z?b?g
//
//---------------------------------------------------------------------------
void FASTCALL Render::SpriteReset()
{
	DWORD addr;
	WORD data;

	// ?X?v???C?g???W?X?^???
	for (addr=0; addr<0x400; addr+=2) {
		data = *(WORD*)(&render.sprmem[addr]);
		SpriteReg(addr, data);
	}
}

//---------------------------------------------------------------------------
//
//	?X?v???C?g???W?X?^??X
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

	// ?C???f?N?V???O??f?[?^????
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

	// ptr???(&spptr[index << 9])
	ptr = &render.spptr[index << 9];

	// ???W?X?^??o?b?N?A?b?v
	next = &render.spreg[index << 2];
	reg[0] = next[0];
	reg[1] = next[1];
	reg[2] = next[2];
	reg[3] = next[3];

	// ???W?X?^?????????
	render.spreg[addr >> 1] = data;

	// ????L???????`?F?b?N
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

	// ???????????A????????????��??????
	if (!render.spuse[index]) {
		if (!use) {
			return;
		}
	}

	// ??????L??????A??x????
	if (render.spuse[index]) {
		// ????????(PCG)
		pcgno = reg[2] & 0xfff;
		ASSERT(render.pcguse[ pcgno ] > 0);
		render.pcguse[ pcgno ]--;
		pcgno >>= 8;
		ASSERT(render.pcgpal[ pcgno ] > 0);
		render.pcgpal[ pcgno ]--;

		// ????????(?|?C???^)
		for (i=0; i<16; i++) {
			j = (int)(reg[1] - 16 + i);
			if ((j >= 0) && (j < 512)) {
				ptr[j] = NULL;
				render.bgspmod[j] = TRUE;
			}
		}

		// ?????????A??????I??
		if (!use) {
			render.spuse[index] = FALSE;
			return;
		}
	}

	// ?o?^????(?g?p?t???O)
	render.spuse[index] = TRUE;

	// ?o?^????(PCG)
	pcgno = next[2] & 0xfff;
	render.pcguse[ pcgno ]++;
	offset = pcgno << 8;
	pcgno >>= 8;
	render.pcgpal[ pcgno ]++;

	// PCG?A?h???X??v?Z?A?|?C???^?Z?b?g
	if (next[2] & 0x8000) {
		// V???]
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
		// ?m?[?}??
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
//	BG?X?N???[????X
//
//---------------------------------------------------------------------------
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
{
	BOOL flag;
	int i;

	ASSERT((page == 0) || (page == 1));
	ASSERT(x < 1024);
	ASSERT(y < 1024);

	// ??r?A??v?????????????
	if ((render.bgx[page] == x) && (render.bgy[page] == y)) {
		return;
	}

	// ?X?V
	render.bgx[page] = x;
	render.bgy[page] = y;

	// 768?~512??�p???
	if (!render.bgspflag) {
		return;
	}

	// ?\???????ABGSPMOD??�O??
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
//	BG?R???g???[????X
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

	// ?t???OOFF
	areaflag[0] = FALSE;
	areaflag[1] = FALSE;

	// ?^?C?v??
	switch (index) {
		// BG0 ?\???t???O
		case 0:
			if (render.bgdisp[0] == flag) {
				return;
			}
			render.bgdisp[0] = flag;
			break;

		// BG1 ?\???t???O
		case 1:
			if (render.bgdisp[1] == flag) {
				return;
			}
			render.bgdisp[1] = flag;
			break;

		// BG0 ?G???A??X
		case 2:
			if (render.bgarea[0] == flag) {
				return;
			}
			render.bgarea[0] = flag;
			areaflag[0] = TRUE;
			break;

		// BG1 ?G???A??X
		case 3:
			if (render.bgarea[1] == flag) {
				return;
			}
			render.bgarea[1] = flag;
			areaflag[1] = TRUE;
			break;

		// BG?T?C?Y??X
		case 4:

			if (render.bgsize == flag) {
				return;
			}
			render.bgsize = flag;
			areaflag[0] = TRUE;
			areaflag[1] = TRUE;
			break;

		// ?????(???????)
		default:
			ASSERT(FALSE);
			return;
	}

	// ?t???O????
	for (i=0; i<2; i++) {
		if (areaflag[i]) {
			// ?????g???????render.pcguse??J?b?g
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

			// ?f?[?^?A?h???X??Z?o($EBE000,$EBC000)
			area = (WORD*)render.sprmem;
			area += 0x6000;
			if (render.bgarea[i]) {
				area += 0x1000;
			}

			// 64?~64???[?h?R?s?[?B$10000??r?b?g????0
			if (render.bgsize) {
				// 16x16???????
				for (j=0; j<(64*64); j++) {
					render.bgreg[i][j] = (DWORD)area[j];
				}
			}
			else {
				// 8x8??H?v???K?v?BPCG(0-255)??>>2???A??????bit0,1??bit17,18??
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

			// bgall??Z?b?g
			for (j=0; j<64; j++) {
				render.bgall[i][j] = TRUE;
			}
		}
	}

	// ????X???A768?~512??O???bgspmod??�O??
	if (render.bgspflag) {
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	BG????????X
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

	// ?y?[?W???[?v
	for (i=0; i<2; i++) {
		// ?Y???y?[?W??f?[?^?G???A???v???????
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

		// ?C???f?b?N?X(<64x64)?A???W?X?^?|?C???^?��
		index = (int)(addr & 0x1fff);
		index >>= 1;
		ASSERT((index >= 0) && (index < 64*64));
		pcgno = render.bgreg[i][index];

		// ??O??pcguse?????
		if (pcgno & 0x10000) {
			pcgno &= 0xfff;
			ASSERT(render.pcguse[ pcgno ] > 0);
			render.pcguse[ pcgno ]--;
			pcgno = (pcgno >> 8) & 0x0f;
			ASSERT(render.pcgpal[ pcgno ] > 0);
			render.pcgpal[ pcgno ]--;
		}

		// ?R?s?[
		if (render.bgsize) {
			// 16x16???????
			render.bgreg[i][index] = (DWORD)data;
		}
		else {
			// 8x8??H?v???K?v?BPCG(0-255)??>>2???A??????bit0,1??bit17,18??
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

		// bgall??�O??
		render.bgall[i][index >> 6] = TRUE;

		// ?\???????????I???Bbgsize=1??y?[?W1?????I??
		if (!render.bgspflag || !render.bgdisp[i]) {
			continue;
		}
		if (render.bgsize && (i == 1)) {
			continue;
		}

		// ?X?N???[????u????v?Z???Abgspmod??�O??
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
//	PCG????????X
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

	// ?C???f?b?N?X??o??
	addr &= 0x7fff;
	index = (int)(addr >> 7);
	ASSERT((index >= 0) && (index < 256));

	// render.pcgready?????
	for (i=0; i<16; i++) {
		render.pcgready[index + (i << 8)] = FALSE;
	}

	// render.pcguse??>0???
	count = 0;
	for (i=0; i<16; i++) {
		count += render.pcguse[index + (i << 8)];
	}
	if (count > 0) {
		// ?d????????ABG/?X?v???C?g??????????
		for (i=0; i<512; i++) {
			render.bgspmod[i] = TRUE;
		}
	}
}

//---------------------------------------------------------------------------
//
//	PCG?o?b?t?@?��
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
//	BG/?X?v???C?g?o?b?t?@?��
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
//	BG/?X?v???C?g
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
		BG(1, raster, buf);
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
		BG(0, raster, buf);
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

	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	// y?u???b?N?????o??
	y = render.bgy[page] + raster;
	if (render.bgsize) {
		// 16x16???[?h
		y &= (1024 - 1);
		y >>= 4;
	}
	else {
		// 8x8???[?h
		y &= (512 - 1);
		y >>= 3;
	}
	ASSERT((y >= 0) && (y < 64));

	// bgall??TRUE???A????y?u???b?N???X?f?[?^????
	if (render.bgall[page][y]) {
		render.bgall[page][y] = FALSE;
		BGBlock(page, y);
	}

	// ?\??
	ptr = render.bgptr[page];
	if (!render.bgsize) {
		// 8x8??\??
		x = (render.bgx[page] - bg_hadjust) & (512 - 1);
		ptr += (((render.bgy[page] + raster) & (512 - 1)) << 7);

		// ????????`?F?b?N
		if ((x & 7) == 0) {
			// 8x8?A???????
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

		// ???????[?u???b?N????s
		rest = 8 - (x & 7);
		ASSERT((rest > 0) && (rest < 8));
		RendBG8P(&ptr[(x & 0xfff8) >> 2], buf, (x & 7), rest, render.pcgready,
				render.sprmem, render.pcgbuf, render.paldata);

		// ?]??????8dot?P????????
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

		// ???
		if (len & 7) {
			x += (len & 0xfff8);
			x &= (512 - 1);
			RendBG8P(&ptr[x >> 2], &buf[rest + (len & 0xfff8)], 0, (len & 7),
				render.pcgready, render.sprmem, render.pcgbuf, render.paldata);
		}
		return;
	}

	// 16x16??\??
	x = (render.bgx[page] - bg_hadjust) & (1024 - 1);
	ptr += (((render.bgy[page] + raster) & (1024 - 1)) << 7);

	// ????????`?F?b?N
	if ((x & 15) == 0) {
		// 16x16?A???????
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

	// ???????[?u???b?N????s
	rest = 16 - (x & 15);
	ASSERT((rest > 0) && (rest < 16));
	RendBG16P(&ptr[(x & 0xfff0) >> 3], buf, (x & 15), rest, render.pcgready,
			render.sprmem, render.pcgbuf, render.paldata);

	// ?]??????16dot?P????????
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

	// ???
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
//	BG(?u???b?N????)
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

	// ???W?X?^?|?C???^????
	reg = &render.bgreg[page][y << 6];

	// BG?|?C???^????
	ptr = render.bgptr[page];
	if (render.bgsize) {
		ptr += (y << 11);
	}
	else {
		ptr += (y << 10);
	}

	// ???[?v
	for (i=0; i<64; i++) {
		// ?��
		bgdata = reg[i];

		// $10000????????????OK
		if (bgdata & 0x10000) {
			ptr += 2;
			continue;
		}

		// $10000??OR
		reg[i] |= 0x10000;

		// pcgno????
		pcgno = bgdata & 0xfff;

		// ?T?C?Y??
		if (render.bgsize) {
			// 16x16
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x8000) {
				// ?????]
				pcgbuf += 0xf0;
				for (j=0; j<16; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// ???
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
			// 8x8?Bbit17,bit18??l??????
			pcgbuf = &render.pcgbuf[ (pcgno << 8) ];
			if (bgdata & 0x20000) {
				pcgbuf += 0x80;
			}
			if (bgdata & 0x40000) {
				pcgbuf += 8;
			}

			if (bgdata & 0x8000) {
				// ?????]
				pcgbuf += 0x70;
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf -= 0x10;
					ptr += 128;
				}
			}
			else {
				// ???
				for (j=0; j<8; j++) {
					ptr[0] = pcgbuf;
					ptr[1] = (DWORD*)bgdata;
					pcgbuf += 0x10;
					ptr += 128;
				}
			}
			ptr -= 1024;
		}

		// ?o?^????(PCG)
		render.pcguse[ pcgno ]++;
		pcgno = (pcgno >> 8) & 0x0f;
		render.pcgpal[ pcgno ]++;

		// ?|?C???^??i???
		ptr += 2;
	}
}

//===========================================================================
//
//	?????_??(??????)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	????
//
//---------------------------------------------------------------------------

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

	reg = &render.spreg[127 << 2];
	ptr = &render.spptr[127 << 9];
	ptr += (raster & 0x1ff);
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
				if ((dx < 0) || (dx >= render.mixlen)) {
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
	int bank;
	const int bg_hadjust = CalcBGHAdjustPixels(compositor_mode, crtc, sprite);

	(void)bg_pri;
	if (!gd) {
		return;
	}
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
		if (bgdata & 0x4000) {
			pixel = src[render.bgsize ? (15 - off) : (7 - off)];
		}
		else {
			pixel = src[off];
		}

		if (!(pixel & REND_COLOR0)) {
			continue;
		}
		bank = (int)((bgdata >> 8) & 0x0f);
		if (bank == 0) {
			continue;
		}
		if (bg_flag[i] & 2) {
			continue;
		}

		pixel &= ~REND_COLOR0;
		bg_line[i] = pixel;
		bg_flag[i] |= 2;
		*active = TRUE;
	}
}

static inline int FastCalcVLineBG(int vline, const CRTC *crtc, const Sprite *sprite)
{
	const CRTC::crtc_t *c;
	int bg11;
	int bg0f;
	int cr29;
	int cr0d;
	int s1;
	int s2;
	int diff;
	unsigned int v;

	c = (crtc ? crtc->GetWorkAddr() : NULL);
	if (!c || !sprite) {
		return (vline & 0x1ff);
	}

	bg11 = (int)sprite->ReadOnly(0xeb0800 + 0x11);
	bg0f = (int)sprite->ReadOnly(0xeb0800 + 0x0f);
	cr29 = (int)c->reg[0x29];
	cr0d = (int)c->reg[0x0d];

	s1 = (((bg11 & 4) ? 2 : 1) - ((bg11 & 16) ? 1 : 0));
	s2 = (((cr29 & 4) ? 2 : 1) - ((cr29 & 16) ? 1 : 0));

	v = (unsigned int)vline;
	v <<= s1;
	v >>= s2;

	if (!(bg11 & 16)) {
		diff = ((bg0f >> s1) - (cr0d >> s2));
		v = (unsigned int)((int)v - diff);
	}
	return (int)(v & 0x1ff);
}
void FASTCALL Render::FastBuildBGLinePX(int src_y, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, BOOL *active, BOOL *bg_opaq)
{
	int i;
	const BOOL sprite_visible = sprite->IsDisplay();
	const BOOL has_bg = (BOOL)(render.bgdisp[0] || (render.bgdisp[1] && !render.bgsize));
	const BOOL has_sources = (BOOL)(sprite_visible || has_bg);
	const BOOL gd = (BOOL)(sp_pri >= tx_pri);
	const BOOL opaq = gd ? TRUE : (BOOL)!ton;
	const DWORD base = (render.paldata[0x100] & ~REND_COLOR0);
	const int raster = (src_y & 0x1ff);
	const DWORD *src;
	for (i=0; i<render.mixlen; i++) {
		bg_line[i] = opaq ? base : REND_COLOR0;
		bg_flag[i] = 0;
	}
	*active = has_sources;
	if (bg_opaq) {
		*bg_opaq = FALSE;
	}
	if (!has_sources) {
		return;
	}

	src = &render.bgspbuf[raster << 9];
	for (i=0; i<render.mixlen; i++) {
		bg_line[i] = src[i];
		if (FastVisiblePixel(src[i])) {
			bg_flag[i] |= 2;
			*active = TRUE;
		}
	}

	if (bg_opaq) {
		*bg_opaq = opaq;
	}
}

static BOOL FastVisiblePixel(DWORD pixel)
{
	return (BOOL)((pixel & 0x80000000) == 0);
}

static BOOL FastVisibleBgPixel(DWORD pixel)
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
	if (pixel & REND_PX68K_IBIT) {
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
	return (DWORD)((r8 << 16) | (g8 << 8) | (b8 << 0));
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

static DWORD FastDimPixel(DWORD pixel)
{
	const WORD p = Px68kPack565I(pixel);
	const WORD half_mask = (WORD)0xF79E;
	const WORD out = (WORD)((p & half_mask) >> 1);
	return Px68kUnpack565I(out);
}
static void FastClearLine(DWORD *buf, int len)
{
	int i;
	for (i=0; i<len; i++) {
		buf[i] = REND_COLOR0;
	}
}

static void FastCopyRenderedLine512(const Render::render_t *work, int page, int y, int len,
	DWORD *dst, BOOL *active)
{
	const DWORD *src;
	int line;
	int x;
	int i;

	line = (y + (int)work->grpy[page]) & 0x1ff;
	x = work->grpx[page] & 0x1ff;
	src = &work->grpbuf[page][line << 10];
	for (i=0; i<len; i++) {
		dst[i] = src[(x + i) & 0x1ff];
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
	}
}

static void FastCopyRenderedLine1024(const Render::render_t *work, int y, int len,
	DWORD *dst, BOOL *active)
{
	const DWORD *src;
	int line;
	int x;
	int i;

	line = (y + (int)work->grpy[0]) & 0x3ff;
	x = work->grpx[0] & 0x3ff;
	src = &work->grpbuf[0][line << 11];
	for (i=0; i<len; i++) {
		dst[i] = src[(x + i) & 0x3ff];
		if (FastVisiblePixel(dst[i])) {
			*active = TRUE;
		}
	}
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
			dst[i] = REND_COLOR0;
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
		pixel = opaq ? REND_COLOR0 : dst[i];
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
			pixel = REND_COLOR0;
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
			if (even_index != 0) {
				grp_sp[i] |= REND_PX68K_IBIT;
			}
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
				FastCopyRenderedLine1024(&render, raster, render.mixlen, grp, &active);
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

			if (p->exon && p->gs[0] && render.grpen[top_pair * 2]) {
				active = FALSE;
				FastBuildSpecial8(&render, top_pair, raster, render.mixlen, grp_sp, grp_sp2, grp_sp_tr, &active);
				*tron = TRUE;
				*pron = TRUE;
			}

			if (p->gs[2] && render.grpen[other_pair * 2]) {
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
			if (p->gs[0] && render.grpen[top_pair * 2] && ((p->vr2h & 0x14) != 0x14)) {
				active = FALSE;
				FastBuildLine8(&render, top_pair, raster, render.mixlen, grp, opaq, &active);
				*gon = TRUE;
			}
			return;

		case 3:
		case 4:
			if (render.grpen[0] && p->gon) {
				active = FALSE;
				FastCopyRenderedLine512(&render, 0, raster, render.mixlen, grp, &active);
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
static void FastDrawLayerLine(DWORD *dst, const DWORD *src, int len, BOOL opaq)
{
	int i;
	if (opaq) {
		for (i=0; i<len; i++) {
			dst[i] = src[i];
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (FastVisiblePixel(src[i])) {
			dst[i] = src[i];
		}
	}
}

static void FastDrawLayerLineTR(DWORD *dst, const DWORD *src, const DWORD *grp_sp, int len, BOOL opaq)
{
	int i;
	DWORD pixel;

	if (opaq) {
		for (i=0; i<len; i++) {
			pixel = src[i];
			if (FastVisiblePixel(pixel) && FastVisiblePixel(grp_sp[i])) {
				pixel = FastBlendPixel(pixel, grp_sp[i]);
			}
			else if (!FastVisiblePixel(pixel)) {
				pixel = REND_COLOR0;
			}
			dst[i] = pixel;
		}
		return;
	}

	for (i=0; i<len; i++) {
		pixel = src[i];
		if (!FastVisiblePixel(pixel)) {
			continue;
		}
		if (FastVisiblePixel(grp_sp[i])) {
			pixel = FastBlendPixel(pixel, grp_sp[i]);
		}
		dst[i] = pixel;
	}
}

static void FastDrawLayerLineBG(DWORD *dst, const DWORD *src, int len, BOOL opaq)
{
	int i;
	if (opaq) {
		for (i=0; i<len; i++) {
			dst[i] = src[i];
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (FastVisibleBgPixel(src[i])) {
			dst[i] = src[i];
		}
	}
}

static void FastDrawLayerLineTRBG(DWORD *dst, const DWORD *src, const DWORD *grp_sp, int len, BOOL opaq)
{
	int i;
	DWORD pixel;

	if (opaq) {
		for (i=0; i<len; i++) {
			pixel = src[i];
			if (FastVisibleBgPixel(pixel) && FastVisiblePixel(grp_sp[i])) {
				pixel = FastBlendPixel(pixel, grp_sp[i]);
			}
			else if (!FastVisibleBgPixel(pixel)) {
				pixel = REND_COLOR0;
			}
			dst[i] = pixel;
		}
		return;
	}

	for (i=0; i<len; i++) {
		pixel = src[i];
		if (!FastVisibleBgPixel(pixel)) {
			continue;
		}
		if (FastVisiblePixel(grp_sp[i])) {
			pixel = FastBlendPixel(pixel, grp_sp[i]);
		}
		dst[i] = pixel;
	}
}

static void FastPrepareBGTextLine(DWORD *line, BYTE *tr_flag, const BYTE *bg_flag, const DWORD *text_line, const DWORD *bg_line,
	int len, BOOL ton, BOOL bgon, int tx_pri, int sp_pri, BOOL bg_opaq)
{
	int i;

	for (i=0; i<len; i++) {
		line[i] = REND_COLOR0;
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

static void FastDrawBGTextLineBG(DWORD *dst, const DWORD *line, const BYTE *tr_flag, int len, BOOL opaq, BOOL td)
{
	int i;
	if (opaq) {
		for (i=0; i<len; i++) {
			dst[i] = line[i];
		}
		return;
	}
	if (td) {
		for (i=0; i<len; i++) {
			if ((tr_flag[i] & 2) && FastVisibleBgPixel(line[i])) {
				dst[i] = line[i];
			}
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (FastVisibleBgPixel(line[i])) {
			dst[i] = line[i];
		}
	}
}

static void FastDrawBGTextLineTEXT(DWORD *dst, const DWORD *line, const BYTE *tr_flag, int len, BOOL opaq, BOOL td)
{
	int i;
	if (opaq) {
		for (i=0; i<len; i++) {
			dst[i] = line[i];
		}
		return;
	}
	if (td) {
		for (i=0; i<len; i++) {
			if ((tr_flag[i] & 1) && FastVisiblePixel(line[i])) {
				dst[i] = line[i];
			}
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (FastVisiblePixel(line[i])) {
			dst[i] = line[i];
		}
	}
}

static void FastDrawBGTextLineBGTR(DWORD *dst, const DWORD *line, const BYTE *tr_flag, const DWORD *grp_sp, int len, BOOL opaq)
{
	int i;
	DWORD pixel;
	if (opaq) {
		for (i=0; i<len; i++) {
			pixel = line[i];
			if (FastVisiblePixel(grp_sp[i])) {
				if (FastVisibleBgPixel(pixel)) {
					pixel = FastBlendPixel(pixel, grp_sp[i]);
				}
				else {
					pixel = FastDimPixel(grp_sp[i]);
				}
			}
			else if (!FastVisibleBgPixel(pixel)) {
				pixel = REND_COLOR0;
			}
			dst[i] = pixel;
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (!(tr_flag[i] & 2)) {
			continue;
		}
		pixel = line[i];
		if (!FastVisibleBgPixel(pixel)) {
			continue;
		}
		if (FastVisiblePixel(grp_sp[i])) {
			pixel = FastBlendPixel(pixel, grp_sp[i]);
		}
		dst[i] = pixel;
	}
}

static void FastDrawBGTextLineTEXTTR(DWORD *dst, const DWORD *line, const BYTE *tr_flag, const DWORD *grp_sp, int len, BOOL opaq)
{
	int i;
	DWORD pixel;
	if (opaq) {
		for (i=0; i<len; i++) {
			pixel = line[i];
			if (FastVisiblePixel(grp_sp[i])) {
				if (FastVisiblePixel(pixel)) {
					pixel = FastBlendPixel(pixel, grp_sp[i]);
				}
				else {
					pixel = FastDimPixel(grp_sp[i]);
				}
			}
			else if (!(tr_flag[i] & 1)) {
				pixel = REND_COLOR0;
			}
			dst[i] = pixel;
		}
		return;
	}
	for (i=0; i<len; i++) {
		if (!(tr_flag[i] & 1)) {
			continue;
		}
		pixel = line[i];
		if (!FastVisiblePixel(pixel)) {
			continue;
		}
		if (FastVisiblePixel(grp_sp[i])) {
			pixel = FastBlendPixel(pixel, grp_sp[i]);
		}
		dst[i] = pixel;
	}
}

static void FastDrawPriLine(DWORD *dst, const DWORD *grp_sp, int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (FastVisiblePixel(grp_sp[i])) {
			dst[i] = grp_sp[i];
		}
	}
}

static void FastDrawHalfFillLine(DWORD *dst, const DWORD *grp_sp, int len)
{
	int i;
	for (i=0; i<len; i++) {
		if (!FastVisiblePixel(dst[i]) && FastVisiblePixel(grp_sp[i])) {
			dst[i] = FastDimPixel(grp_sp[i]);
		}
	}
}

void FASTCALL Render::MixFast(int y)
{
	MixFastLine(y, y);
}

void FASTCALL Render::MixFastLine(int dst_y, int src_y)
{
	DWORD *dst;
	DWORD *grp = render.fast_grp_linebuf;
	DWORD *grp_sp = render.fast_grp_linebuf_sp;
	DWORD *grp_sp2 = render.fast_grp_linebuf_sp2;
	BOOL *grp_sp_tr = render.fast_grp_linebuf_sp_tr;
	DWORD text_line[1024];
	DWORD *bg_line = render.fast_bg_linebuf;
	DWORD bgtext_line[1024];
	DWORD out[1024];
	BYTE *bg_flag = render.fast_text_trflag;
	BYTE tr_flag[1024];
	const VC::vc_t *p;
	int tx_pri;
	int sp_pri;
	int gr_pri;
	BOOL gon;
	BOOL tron;
	BOOL pron;
	BOOL ton;
	BOOL bgon;
	BOOL opaq;
	BOOL tr_mode;
	BOOL dim_mode;
	BOOL pri_mode;
	BOOL tdrawed;
	BOOL bg_active;
	BOOL bg_opaq;
	DWORD mix_stamp;
	int i;

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
	tx_pri = (int)(p->tx & 3);
	sp_pri = (int)(p->sp & 3);
	gr_pri = (int)(p->gr & 3);
	ton = render.texten;
	tr_mode = (BOOL)((p->vr2h & 0x5d) == 0x1d);
	dim_mode = (BOOL)((p->vr2h & 0x5d) == 0x1c);
	pri_mode = (BOOL)((p->vr2h & 0x5c) == 0x14);

	FastMixGrp(src_y, grp, grp_sp, grp_sp2, grp_sp_tr, &gon, &tron, &pron);

	for (i=0; i<render.mixlen; i++) {
		out[i] = REND_COLOR0;
		if (ton) {
			text_line[i] = render.textout[(((src_y + (int)render.texty) & 0x3ff) << 10) + (((int)render.textx + i) & 0x3ff)];
		}
		else {
			text_line[i] = REND_COLOR0;
		}
		bg_line[i] = REND_COLOR0;
		bg_flag[i] = 0;
	}

	bg_active = FALSE;
	bg_opaq = FALSE;
	FastBuildBGLinePX(src_y, ton, tx_pri, sp_pri, bg_line, bg_flag, &bg_active, &bg_opaq);
	bgon = bg_active;
	FastPrepareBGTextLine(bgtext_line, tr_flag, bg_flag, text_line, bg_line, render.mixlen, ton, bgon, tx_pri, sp_pri, bg_opaq);

	opaq = TRUE;
	tdrawed = FALSE;

	if (gr_pri >= 2) {
		if (gon) {
			FastDrawLayerLine(out, grp, render.mixlen, opaq);
			opaq = FALSE;
		}
		if (tron) {
			FastDrawLayerLine(out, grp_sp2, render.mixlen, opaq);
			opaq = FALSE;
		}
	}
		if ((sp_pri >= 2) && bgon) {
			if (tr_mode && (gr_pri != 2) && tron) {
				if (gr_pri < tx_pri) {
					FastDrawBGTextLineBGTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
				}
				else {
					FastDrawBGTextLineBG(out, bgtext_line, tr_flag, render.mixlen, opaq, tdrawed);
				}
			}
			else {
				FastDrawBGTextLineBG(out, bgtext_line, tr_flag, render.mixlen, opaq, tdrawed);
			}
			tdrawed = TRUE;
			opaq = FALSE;
		}
	if ((tx_pri >= 2) && ton) {
		if (tr_mode && (gr_pri != 2) && tron) {
			FastDrawBGTextLineTEXTTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
		}
		else {
			FastDrawBGTextLineTEXT(out, bgtext_line, tr_flag, render.mixlen, opaq, tdrawed);
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	if ((gr_pri == 1) && gon) {
		FastDrawLayerLine(out, grp, render.mixlen, opaq);
		opaq = FALSE;
	}
		if ((sp_pri == 1) && bgon) {
			if (tr_mode && (gr_pri == 0) && tron) {
				if (gr_pri < tx_pri) {
					FastDrawBGTextLineBGTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
				}
				else {
					FastDrawBGTextLineBG(out, bgtext_line, tr_flag, render.mixlen, opaq, (BOOL)(tx_pri == 2));
				}
			}
			else {
				FastDrawBGTextLineBG(out, bgtext_line, tr_flag, render.mixlen, opaq, (BOOL)(tx_pri == 2));
			}
			tdrawed = TRUE;
			opaq = FALSE;
		}
	if ((tx_pri == 1) && tr_mode && (gr_pri != 0) && (sp_pri > gr_pri) && bgon && tron) {
		FastDrawBGTextLineBGTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			FastDrawLayerLine(out, grp_sp2, render.mixlen, opaq);
		}
	}
	else if ((gr_pri == 1) && tron && gon && p->exon) {
		FastDrawLayerLine(out, grp_sp2, render.mixlen, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 1) && ton) {
		if (tr_mode && (gr_pri == 0) && tron) {
			FastDrawBGTextLineTEXTTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
		}
		else {
			FastDrawBGTextLineTEXT(out, bgtext_line, tr_flag, render.mixlen, opaq, (BOOL)(sp_pri >= 1));
		}
		opaq = FALSE;
		tdrawed = TRUE;
	}

	if ((gr_pri == 0) && gon) {
		FastDrawLayerLine(out, grp, render.mixlen, opaq);
		opaq = FALSE;
	}
	if ((sp_pri == 0) && bgon) {
		FastDrawBGTextLineBG(out, bgtext_line, tr_flag, render.mixlen, opaq, (BOOL)(tx_pri >= 1));
		tdrawed = TRUE;
		opaq = FALSE;
	}
	if ((tx_pri == 0) && tr_mode && (sp_pri > gr_pri) && bgon && tron) {
		FastDrawBGTextLineBGTR(out, bgtext_line, tr_flag, grp_sp, render.mixlen, opaq);
		tdrawed = TRUE;
		opaq = FALSE;
		if (tron) {
			FastDrawLayerLine(out, grp_sp2, render.mixlen, opaq);
		}
	}
	else if ((gr_pri == 0) && tron && p->exon) {
		FastDrawLayerLine(out, grp_sp2, render.mixlen, opaq);
		opaq = FALSE;
	}
	if ((tx_pri == 0) && ton) {
		FastDrawBGTextLineTEXT(out, bgtext_line, tr_flag, render.mixlen, opaq, TRUE);
		tdrawed = TRUE;
		opaq = FALSE;
	}

	if (pri_mode && pron) {
		FastDrawPriLine(out, grp_sp, render.mixlen);
	}
	else if (dim_mode && tron) {
		FastDrawHalfFillLine(out, grp_sp, render.mixlen);
	}

	if (opaq) {
		for (i=0; i<render.mixlen; i++) {
			out[i] = REND_COLOR0;
		}
	}

	RendMix01(dst, out, render.drawflag + (src_y << 6), render.mixlen);
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

	// ?????w???????????A?????o?b?t?@?????????Ay?I?[?o?[???return
	if ((!render.mix[y]) || (!render.mixbuf)) {
		return;
	}
	if (render.mixheight <= y) {
		return;
	}
	ASSERT(render.mixlen > 0);

#if defined(REND_LOG)
	LOG1(Log::Normal, "???? y=%d", y);
#endif	// REND_LOG

	// ?t???OOFF?A?????o?b?t?@?A?h???X??????
	render.mix[y] = FALSE;
	q = &render.mixbuf[render.mixwidth * y];

	switch (render.mixtype) {
		// ?^?C?v0(?\???????)
		case 0:
			RendMix00(q, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v1(1????)
		case 1:
			offset = (*render.mixy[0] + y) & render.mixand[0];
			p = render.mixptr[0];
			ASSERT(p);
			p += (offset << render.mixshift[0]);
			p += (*render.mixx[0] & render.mixand[0]);
			RendMix01(q, p, render.drawflag + (y << 6), render.mixlen);
			return;

		// ?^?C?v2(2??A?J???[0?d?????)
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

		// ?^?C?v3(2??A???d?????)
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

		// ?^?C?v4(?O???t?B?b?N???3?? or 4??)
		case 4:

			MixGrp(y, buf);
			RendMix01(q, buf, render.drawflag + (y << 6), render.mixlen);
			break;

		// ?^?C?v5(?O???t?B?b?N?{?e?L?X?g?A?e?L?X?g?D????d?????)
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

		// ?^?C?v6(?O???t?B?b?N?{?e?L?X?g?A?O???t?B?b?N?D????d?????)
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

		// ?^?C?v7(?e?L?X?g?{?X?v???C?g?{?O???t?B?b?N1??)
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

		// ?^?C?v8(?e?L?X?g+?X?v???C?g+?O???t?B?b?N?Q????)
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

		// ?????
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	?O???t?B?b?N????
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
		// ???(????render.mixpage??z??)
		case 0:
			ASSERT(FALSE);
			return;

		// 1??(????render.mixpage??z??)
		case 1:
			ASSERT(FALSE);
			return;

		// 2??
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

		// 3??
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

		// 4??
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

		// ?????
		default:
			ASSERT(FALSE);
			break;
	}
}

//---------------------------------------------------------------------------
//
//	?????o?b?t?@?��
//
//---------------------------------------------------------------------------
const DWORD* FASTCALL Render::GetMixBuf() const
{
	ASSERT(this);

	// NULL????????
	return render.mixbuf;
}

//---------------------------------------------------------------------------
//
//	?????_?????O
//
//---------------------------------------------------------------------------
void FASTCALL Render::ProcessFast()
{
	int i;
	int src;
	DWORD stamp;
	BOOL sprite_disp;

	// ?s??????????????s?v
	if (render.first >= render.last) {
		return;
	}

	// VC
	if (render.vc) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?r?f?I????");
#endif	// RENDER_LOG
		Video();
	}

	// ?R???g???X?g
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// ?p???b?g
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0??A?X?v???C?g??\??ON/OFF?????
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
		// I/O????g?��????A?c??????????????????
		for (i=render.first; i<render.last; i++) {
			if ((i & 1) == 0) {
				src = (i >> 1);
				stamp = ++render.fast_stamp_counter;
				render.fast_mix_stamp[src] = stamp;
				render.fast_bg_stamp[src & 0x1ff] = stamp;
				render.mix[src] = TRUE;
				Text(src);
				Grp(0, src);
				Grp(1, src);
				Grp(2, src);
				Grp(3, src);
				render.bgspmod[src & 0x1ff] = TRUE;
				BGSprite(src);
				MixFastLine(src, src);
			}
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// ?C???^???[?X???
	if ((render.v_mul == 0) && render.lowres) {
		// ????E??????????(???@??????)
		for (i=render.first; i<render.last; i++) {
			// ?e?L?X?g?E?O???t?B?b?N
			src = (i << 1) + 0;
			stamp = ++render.fast_stamp_counter;
			render.fast_mix_stamp[src] = stamp;
			render.fast_bg_stamp[src & 0x1ff] = stamp;
			render.mix[src] = TRUE;
			Text(src);
			Grp(0, src);
			Grp(1, src);
			Grp(2, src);
			Grp(3, src);
			render.bgspmod[src & 0x1ff] = TRUE;
			BGSprite(src);
			MixFastLine(src, src);
			src = (i << 1) + 1;
			stamp = ++render.fast_stamp_counter;
			render.fast_mix_stamp[src] = stamp;
			render.fast_bg_stamp[src & 0x1ff] = stamp;
			render.mix[src] = TRUE;
			Text(src);
			Grp(0, src);
			Grp(1, src);
			Grp(2, src);
			Grp(3, src);
			render.bgspmod[src & 0x1ff] = TRUE;
			BGSprite(src);
			MixFastLine(src, src);
		}
		// ?X?V
		render.first = render.last;
		return;
	}

	// ????[?v
	for (i=render.first; i<render.last; i++) {
		stamp = ++render.fast_stamp_counter;
		render.fast_mix_stamp[i] = stamp;
		render.fast_bg_stamp[i & 0x1ff] = stamp;
		render.mix[i] = TRUE;
		Text(i);
		Grp(0, i);
		Grp(1, i);
		Grp(2, i);
		Grp(3, i);
		render.bgspmod[i & 0x1ff] = TRUE;
		BGSprite(i);
		MixFastLine(i, i);
	}
	// ?X?V
	render.first = render.last;
}

void FASTCALL Render::Process()
{
	int i;

	// ?s??????????????s?v
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

	// ?R???g???X?g
	if (render.contrast) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?R???g???X?g????");
#endif	// RENDER_LOG
		Contrast();
	}

	// ?p???b?g
	if (render.palette) {
#if defined(REND_LOG)
		LOG0(Log::Normal, "?p???b?g????");
#endif	// RENDER_LOG
		Palette();
	}

	// first==0??A?X?v???C?g??\??ON/OFF?????
	if (render.first == 0) {
		if (sprite->IsDisplay()) {
			if (!render.bgspdisp) {
				// ?X?v???C?gCPU??Video
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = TRUE;
			}
		}
		else {
			if (render.bgspdisp) {
				// ?X?v???C?gVideo??CPU
				for (i=0; i<512; i++) {
					render.bgspmod[i] = TRUE;
				}
				render.bgspdisp = FALSE;
			}
		}
	}

	// ????x2???
	if ((render.v_mul == 2) && !render.lowres) {
		// I/O????g?��????A?c??????????????????
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
		// ?X?V
		render.first = render.last;
		return;
	}

	// ?C???^???[?X???
	if ((render.v_mul == 0) && render.lowres) {
		// ????E??????????(???@??????)
		for (i=render.first; i<render.last; i++) {
			// ?e?L?X?g?E?O???t?B?b?N
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
		// ?X?V
		render.first = render.last;
		return;
	}

	// ????[?v
	for (i=render.first; i<render.last; i++) {
		Text(i);
		Grp(0, i);
		Grp(1, i);
		Grp(2, i);
		Grp(3, i);
		BGSprite(i);
		Mix(i);
	}
	// ?X?V
	render.first = render.last;
}









