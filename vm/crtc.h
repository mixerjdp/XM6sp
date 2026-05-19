//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2003 P.I. (ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#ifndef crtc_h
#define crtc_h

#include "device.h"
#include "event.h"
#include "px68k_crtc_port.h"

/// Eliminate display rendering latency
/*
VMの垂直帰線期間開始に合わせて描画を行うことにより、画面表示の遅延を
減らす。また、15kHzモード時にCPUパワーが十分にあるにもかかわらずフレー
ムスキップが発生してしまう原作の問題も修正される。
*/
#define XM6_RENDER_SYNC	1

//===========================================================================
//
/// CRTC
//
//===========================================================================
class CRTC : public MemDevice
{
public:
	// Internal data definition
	typedef struct {
		BYTE reg[24 * 2];				// CRTC registers
		BOOL hrl;						// HRL (system port)
		BOOL lowres;					// 15 kHz mode
		BOOL textres;					// 768x512 mode
		BOOL changed;					// Resolution change flag

		int h_sync;						// Horizontal sync period
		int h_pulse;					// Horizontal sync pulse width
		int h_back;						// Horizontal back porch
		int h_front;					// Horizontal front porch
		int h_dots;						// Horizontal dot count
		int h_mul;						// Horizontal scale factor
		int hd;							// 256, 512, 768, undefined

		int v_sync;						// Vertical sync period (H units)
		int v_pulse;					// Vertical sync pulse width (H units)
		int v_back;						// Vertical back porch (H units)
		int v_front;					// Vertical front porch (H units)
		int v_dots;						// Vertical dot count
		int v_mul;						// Vertical scale factor (0: interlace)
		int vd;							// 256, 512, undefined, undefined

		DWORD ns;						// ns counter
		DWORD hus;						// hus counter
		DWORD v_synccnt;				// V-SYNC counter
		DWORD v_blankcnt;				// V-BLANK counter
		int h_disp;						// Horizontal display flag
		BOOL v_disp;					// V-DISP flag
		BOOL v_blank;					// V-BLANK flag
		DWORD v_count;					// V-DISP counter
		int v_scan;						// Scanline

		// TypeG extensions
		BOOL disp_vsync;				// Sync with the host-side VSYNC
		int h_refresh;					// Adjusted horizontal sync period
		BOOL h_blockscan;				// Horizontal display-period scan mode
		int h_blocknum;					// Number of display-period scan blocks
		int h_blockpos;					// Display-period scan block position
		BOOL v_scaneven;				// Interlace even-field flag

		BOOL tmem;						// Text VRAM hidden
		BOOL gmem;						// Graphics VRAM hidden
		DWORD siz;						// Graphics VRAM 1024x1024 mode
		DWORD col;						// Graphics VRAM color mode

		DWORD text_scrlx;				// Text scroll X
		DWORD text_scrly;				// Text scroll Y
		DWORD grp_scrlx[4];				// Graphics scroll X
		DWORD grp_scrly[4];				// Graphics scroll Y

		int raster_count;				// Raster counter
		int raster_int;					// Raster interrupt position
		BOOL raster_copy;				// Raster copy flag
		BOOL raster_exec;				// Raster copy execution flag
		DWORD fast_clr;					// Graphics fast clear
	} crtc_t;

public:
	// Core functions
	explicit CRTC(VM* p);
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

	// Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read-only

	// External API
	void FASTCALL GetCRTC(crtc_t *buffer) const;
										// Get internal data
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL SetHRL(BOOL h);
										// Set HRL
	BOOL FASTCALL GetHRL() const;
										// Get HRL
	void FASTCALL GetHVHz(DWORD *h, DWORD *v) const;
										// Get display frequencies
	DWORD FASTCALL GetDispCount() const	{ return crtc.v_count; }
										// Get display counter
	const crtc_t* FASTCALL GetWorkAddr() const { return &crtc; }
	const Px68kCrtcStateView* FASTCALL GetPx68kStateView() const;
										// Get work address
	int FASTCALL Get8DotClock() const;
										// Get the 8-dot clock
#if XM6_RENDER_SYNC == 1
	void FASTCALL SetScheduler(class CScheduler* pScheduler) { m_pScheduler = pScheduler; }
										///< Attach scheduler
#endif	// XM6_RENDER_SYNC == 1

private:
	void FASTCALL ReCalc();
										// Recalculate
	void FASTCALL HSync();
										// H-SYNC start
	void FASTCALL HDispRS();
										// H-DISP start (raster scan)
	void FASTCALL HDispBS();
										// H-DISP start (block scan)
	void FASTCALL VSync();
										// V-SYNC start
	void FASTCALL VBlank();
										// V-BLANK start
	int FASTCALL Ns2Hus(int ns)			{ return ns / 500; }
										// Convert ns to 0.5 us
	int FASTCALL Hus2Ns(int hus)		{ return hus * 500; }
										// Convert 0.5 us to ns
	void FASTCALL Raster();
										// Raster counter processing
	void FASTCALL CheckRaster();
										// Raster interrupt check
	void FASTCALL TextVRAM();
	void FASTCALL SyncPx68kState() const;
										// Text VRAM effects
	static const int DotClockTable[16];
										// 8-dot clock table
	static const BYTE ResetTable[26];
										// Reset register table
	crtc_t crtc;
										// Internal CRTC data
	Event event;
										// Event
	TVRAM *tvram;
										// Text VRAM
	GVRAM *gvram;
										// Graphics VRAM
	Sprite *sprite;
										// Sprite controller
	MFP *mfp;
										// MFP
	Render *render;
										// Renderer
	Printer *printer;
										// Printer
	VC *vc;
										// VC
#if XM6_RENDER_SYNC == 1
	class CScheduler* m_pScheduler;
										///< Scheduler
#endif	// XM6_RENDER_SYNC == 1
	BOOL hsync;
										// HSYNC
	mutable Px68kCrtcStateView px68k_state_view;
										// PX68k CRTC state view request
};

#endif	// crtc_h
