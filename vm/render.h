//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001,2002 P.I. (ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ Renderer ]
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
class TVRAM;

//=========================================================================
//
//	Renderer
//
//=========================================================================
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

	// Internal data definition
	typedef struct {
		// Overall control
		BOOL act;						// Whether composition is active
		BOOL enable;					// Composition enabled
		int count;						// Scheduler integration counter
		BOOL ready;						// Whether rendering is ready
		int first;						// First unprocessed raster
		int last;						// Last display raster

		// CRTC
		BOOL crtc;						// CRTC change flag
		int width;						// X-direction dot count (256+)
		int h_mul;							// X multiplier
		int height;						// Y-direction dot count (256+)
		int v_mul;							// Y multiplier
		BOOL siz;						// Actual screen size
		BOOL lowres;					// 15 kHz flag
		int hres;						// Horizontal frequency mode (0:15 kHz, 1:24 kHz, 2:31 kHz)
		int hd;							// Horizontal dot count
		int vd;							// Vertical dot count
		BOOL hrl;						// HRL (system port)

		// For CRT emulation
		int h_disp;						// Horizontal display width
		int v_disp;						// Vertical display width
		int h_total;					// Horizontal sync width
		int v_total;					// Vertical sync width
		int h_pulse;					// Horizontal pulse width
		int v_pulse;					// Vertical pulse width
		int h_start;					// Horizontal draw position
		int v_start;					// Vertical draw position
		BOOL scanline;					// Scanline mode

		// VC
		BOOL vc;						// VC change flag

		// composition
		BYTE mix[1024];					// Composition flag (line)
		DWORD *mixbuf;					// Composition buffer
		DWORD *mixptr[8];				// Composition pointer
		DWORD mixshift[8];				// Composition pointer Y shift
		DWORD mixand[8];
		DWORD mixrshift[8];				// Composition pointer Y shift (right)
		DWORD mixlshift[8];				// Composition pointer Y shift (left)
		DWORD *mixx[8];					// Composition pointer X scroll pointer
		DWORD *mixy[8];					// Composition pointer Y scroll pointer
		DWORD mixandx[8];				// Composition pointer scroll AND mask (X)
		DWORD mixandy[8];				// Composition pointer scroll AND mask (Y)
		DWORD mixraster[8];				// Composition pointer raster position adjustment
		int mixmap[3];					// Composition map
		int mixtype;					// Composition type
		int mixpage;					// Composited graphics page count
		int mixwidth;					// Composition buffer width
		int mixheight;					// Composition buffer height
		int mixlen;						// Composition processing length (X direction)
		int mixmode;					// Composition mode (0: non-interlace, 1: interlace, 2: double read)
		BOOL mixeven;					// Interlace composition even/odd state
		int sp;							// Priority (sprites)
		int gr;							// Priority (graphics)
		int tx;							// Priority (text)
		BOOL mixdirty;					// Composition dirty flag

		// Draw
		BOOL draw[1024];				// Draw flag (line)
		BOOL *drawflag;					// Draw flag (16 dots)

		// Contrast
		BOOL contrast;					// Contrast change flag
		int contlevel;					// Contrast (configured level)
		int contvalue;					// Contrast (current value)
		DWORD conttime;					// Time when contrast was last changed

		// Palette
		BOOL palette;					// Palette change flag
		BYTE palmod[0x200];				// Palette change flag
		DWORD *palbuf;					// Palette buffer
		DWORD *palptr;					// Palette pointer
		const WORD *palvc;				// Palette VC pointer
		DWORD paldata[0x200];			// Palette data
		BYTE pal64k[0x200];				// Converted palette data

		// palette(semi-transparent, special priority)
		DWORD paldataGB[0x100];			// Palette data (GVRAM-selected: base page)
		DWORD paldataGS[0x100];			// Palette data (GVRAM-selected: second page)
		DWORD paldataPB[0x100];			// Palette data (palette-selected: base page)
		DWORD paldataPS[0x100];			// Palette data (palette-selected: second page)

		// Text VRAM
		BOOL texten;					// Text display flag
		BYTE textpal[1024];				// Text palette flag
		BOOL textmod[1024];				// Text update flag (line)
		BOOL *textflag;					// Text update flag (32 dots)
		BYTE *textbuf;					// Text buffer (before palette)
		DWORD *textout;					// Text buffer (after palette)
		const BYTE *texttv;				// Text TVRAM pointer
		DWORD textx;					// Text scroll X
		DWORD texty;					// Text scroll Y
		BOOL textdirty;					// Text dirty flag

		// Graphics VRAM
		int grptype;					// Graphics type (0-4)
		BYTE grppal[2048];				// Graphics palette flag
		BOOL grpmod[2048];				// Graphics update flag (line)
		BOOL *grpflag;					// Graphics update flag (16 dots)
		DWORD *grpbuf[4];				// Graphics block buffer
		const BYTE* grpgv;				// Graphics GVRAM pointer
		BOOL grpscrl;					// Graphics scroll update flag
		DWORD grpx[4];					// Graphics block scroll X
		DWORD grpy[4];					// Graphics block scroll Y
		int grpdx[4];					// Graphics block scroll X delta
		int grpdy[4];					// Graphics block scroll Y delta
		BOOL grppen[4];					// Graphics page enable flag
		BOOL grpen[4];					// Graphics enable mirror
		BOOL grpben[4];					// Graphics block enable flag
		BOOL grpnorm[4];				// Preferred graphics page flag
		BOOL grpdirty;					// Graphics dirty flag

		// PCG
		BOOL pcgready[256 * 16];		// PCG ready flag
		DWORD pcguse[256 * 16];			// PCG in-use count
		DWORD pcgpal[16];				// PCG palette usage count
		DWORD *pcgbuf;					// PCG buffer
		const BYTE* sprmem;				// Sprite memory

		// sprite
		DWORD **spptr;					// Sprite pointer buffer
		DWORD spreg[0x200];				// Saved sprite registers
		BOOL spuse[128];				// Sprite in-use flag

		// BG
		DWORD bgreg[2][64 * 64];		// BG register + change flag ($10000)
		BOOL bgall[2][64];				// BG change flag (per block)
		BOOL bgdisp[2];					// BG display flag
		BOOL bgarea[2];					// BG display area
		BOOL bgsize;					// BG display size (16 dots = TRUE)
		bgdata_t *bgptr[2];				// BG pointer + data
		BOOL bgmod[2][1024];			// BG update flag
		DWORD bgx[2];					// BG scroll (X)
		DWORD bgy[2];					// BG scroll (Y)

		// BG/sprite composition
		BOOL bgsp;						// BG/sprite change flag
		BOOL bgspflag;					// BG/sprite display flag
		BOOL bgspdisp;					// BG/sprite CPU/video flag
		BYTE bgspmod[1024];				// BG/sprite update flag
		DWORD *bgspbuf;					// BG/sprite buffer
		int bgsp_h;						// BG/sprite horizontal position adjustment
		int bgsp_v;						// BG/sprite vertical position adjustment
		BOOL bgsp_lowres;				// BG/sprite 15 kHz flag
		DWORD bgsp_vres;				// BG/sprite vertical resolution
		int bgsp_mixmode;				// BG/sprite composition mode (0: non-interlace, 1: interlace, 2: double read)
		int bgsp_rshift;				// BG/sprite raster shift amount (right)
		int bgsp_lshift;				// BG/sprite raster shift amount (left)
		BOOL bgspdirty;					// BG/sprite dirty flag
		DWORD fast_stamp_counter;
		DWORD fast_mix_stamp[1024];
		DWORD fast_mix_done[1024];
		DWORD fast_bg_stamp[512];
		DWORD fast_bg_done[512];
		DWORD zero;						// Scroll dummy (0)
	} render_t;

public:
	// Core functions
	explicit Render(VM* p);
										///< Constructor
	BOOL FASTCALL Init();
										// Initialize
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply settings

	// External API (control)
	void FASTCALL EnableAct(BOOL enable){ render.enable = enable; }
										// Composition enabled
	BOOL FASTCALL IsActive() const		{ return render.act; }
										// Whether active
	BOOL FASTCALL IsReady() const		{ return (BOOL)(render.count > 0); }
										// Get render-ready state
	void FASTCALL Complete()			{ render.count = 0; }
										// Rendering complete
	void FASTCALL StartFrame();
										// Frame start (V-DISP)
	void FASTCALL EndFrame();
										// Frame end (V-BLANK)
	void FASTCALL HSync(int raster, int xoffset);
	void FASTCALL HSync(int raster)		{ HSync(raster, 0); }
										// Horizontal sync (through raster processing)
	void FASTCALL SetMixBuf(DWORD *buf, int width, int height);
										// Set the composition buffer
	void FASTCALL UpdateMixBuf();
										// Force a composition buffer update
	render_t* FASTCALL GetWorkAddr() 	{ return &render; }
	const render_t* FASTCALL GetWorkAddr() const { return &render; }
	void FASTCALL SetRenderTarget(void*) {}
										// MFC render target compatibility hook
										// Get the work address
#if XM6_RENDER_SYNC == 2
	void FASTCALL SetScheduler(class CScheduler* pScheduler) { m_pScheduler = pScheduler; }
										///< Attach scheduler
#endif	// XM6_RENDER_SYNC == 2

	// External API (display)
	void FASTCALL SetCRTC();
										// Set CRTC
	void FASTCALL SetVC();
										// Set VC
	void FASTCALL SetContrast(int cont, BOOL immediate = FALSE);
										// Set contrast
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
	const Px68kCrtcHost* FASTCALL GetPx68kCrtcHost() const;
	void FASTCALL CachePx68kStateView(const Px68kCrtcStateView *view);
	void FASTCALL ForceRecompose();
										// Get contrast
	void FASTCALL SetPalette(int index);
										// Set palette
	const DWORD* FASTCALL GetPalette() const;
										// Get the palette buffer
	void FASTCALL TextMem(DWORD addr);
										// Text VRAM change
	void FASTCALL TextScrl(DWORD x, DWORD y);
										// Text scroll change
	void FASTCALL TextCopy(DWORD src, DWORD dst, DWORD plane);
										// Raster copy
	void FASTCALL GrpMem(DWORD addr, DWORD block);
										// Graphics VRAM change
	void FASTCALL GrpAll(DWORD line, DWORD block);
										// Graphics VRAM change
	void FASTCALL GrpScrl(int block, DWORD x, DWORD y);
										// Set graphics scroll
	void FASTCALL SpriteReg(DWORD addr, DWORD data[]);
	void FASTCALL SpriteReg(DWORD addr, DWORD data);
										// Sprite register change
	void FASTCALL BGScrl(int page, DWORD x, DWORD y);
										// BG scroll change
	void FASTCALL BGCtrl(int index, BOOL flag);
										// BG control change
	void FASTCALL BGMem(DWORD addr, WORD data);
										// BG change
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
										// PCG change
	const DWORD* FASTCALL GetTextBuf() const;
										// Get the text buffer
	const DWORD* FASTCALL GetGrpBuf(int index) const;
										// Get the graphics buffer
	const DWORD* FASTCALL GetPCGBuf() const;
										// Get the PCG buffer
	const DWORD* FASTCALL GetBGSpBuf() const;
										// Get the BG/sprite buffer
	const DWORD* FASTCALL GetMixBuf() const;
	const CRTC* FASTCALL GetCRTCDevice() const { return crtc; }
	const VC* FASTCALL GetVCDevice() const { return vc; }
	const TVRAM* FASTCALL GetTVRAMDevice() const;
	const GVRAM* FASTCALL GetGVRAMDevice() const;
	const Sprite* FASTCALL GetSpriteDevice() const { return sprite; }
										// Get the composition buffer

private:
	class Backend;
	void FASTCALL StartFrameOriginal();
	void FASTCALL EndFrameOriginal();
	void FASTCALL HSyncOriginal(int raster, int xoffset = 0);
	void FASTCALL SetCRTCOriginal();
	void FASTCALL SetVCOriginal();
	void FASTCALL Process(int raster, int xoffset);
										// Rendering
	void FASTCALL Crtc();
										// CRTC processing
	static const DWORD HDispTable[16];
										// Raster misalignment validation table
	void FASTCALL Video();
										// VC processing
	void FASTCALL Contrast();
										// Contrast processing
	void FASTCALL Palette();
										// Palette processing
	void FASTCALL MakePalette();
										// Build the palette
	DWORD FASTCALL ConvPalette(int color, int ratio);
										// Color conversion
	BOOL FASTCALL TextConv(int offset);
										// Text conversion
	void FASTCALL Text(int raster);
										// Text
	void FASTCALL GrpScrlCheck();
										// Graphics scroll check processing
	void FASTCALL GrpDispCheck();
										// Graphics display-layout check processing
	BYTE* FASTCALL MixGVRAM(BYTE *buf, int gd, int offset);
										// GVRAM buffer composition
	void FASTCALL Grp(int gd, int raster);
										// Graphics
	void FASTCALL SpriteReset();
										// Reset sprites
	void FASTCALL BGSprite(int raster);
										// BG/sprite
	void FASTCALL BG(int page, int raster, DWORD *buf, BOOL force);
										// BG
	void FASTCALL BGBlock(int page, int y);
										// BG (horizontal blocks)
	void FASTCALL BGSpriteCheck(int raster);
										// BG/sprite check
	void FASTCALL Mix(int raster, int xoffset);
										// composition
	DWORD* FASTCALL MixGrp(DWORD *buf, int raster, int xoffset, int mixlen);
										// Composition (graphics)
	void FASTCALL HSyncFast(int raster);
	void FASTCALL StartFrameFast();
	void FASTCALL EndFrameFast();
	void FASTCALL SetCRTCFast();
	void FASTCALL SetVCFast();
	void FASTCALL VideoFastPX68K();
	void FASTCALL PaletteFastPX68K();
	void FASTCALL TextFastPX68K(int raster);
	void FASTCALL ProcessFast();
	void FASTCALL MixFast(int y);
	void FASTCALL MixFastLine(int dst_y, int src_y);
	void FASTCALL FastBuildBGLinePX(int sprite_raster, int bg_raster, BOOL ton, int tx_pri, int sp_pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active, BOOL *bg_opaq);
	void FASTCALL FastDrawSpriteLinePX(int raster, int pri, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastDrawBGPageLinePX(int page, int raster, BOOL gd, DWORD *bg_line, BYTE *bg_flag, WORD *bg_pri, BOOL *active);
	void FASTCALL FastMixGrp(int y, DWORD *grp, DWORD *grp_sp, DWORD *grp_sp2,
		BOOL *grp_sp_tr, BOOL *gon, BOOL *tron, BOOL *pron);
	void FASTCALL InvalidateFrame();
	void FASTCALL ApplyPendingCompositorMode();
	CRTC *crtc;
										// CRTC
	const CRTC::crtc_t *cp;
										// CRTC work address
	VC *vc;
										// VC
	const VC::vc_t *vp;
										// VC work address
	Sprite *sprite;
	Px68kCrtcHost px68k_crtc_host;
	Px68kCrtcStateView px68k_crtc_state_cache;
	BOOL render_fast_dummy_enabled;
	BOOL transparency_enabled;
	BOOL original_bg0_render_enabled;
	int compositor_mode;
	Backend *backend;
	Backend *backend_original;
	Backend *backend_fast;
										// sprite
#if XM6_RENDER_SYNC == 2
	class CScheduler* m_pScheduler;
										///< Scheduler
#endif	// XM6_RENDER_SYNC == 2
	render_t render;
										// Internal data
};

#endif	// render_h
