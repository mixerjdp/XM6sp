//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#if !defined(crtc_h)
#define crtc_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	CRTC
//
//===========================================================================
class CRTC : public MemDevice
{
public:
	// Internal data definition
	typedef struct {
		BYTE reg[24 * 2];				// CRTC registers
		BOOL hrl;						// HRL (system port)
		BOOL lowres;					// 15kHz mode
		BOOL textres;					// 768x512 mode
		BOOL changed;					// Resolution change flag

		int h_sync;						// Horizontal sync total
		int h_pulse;					// Horizontal sync pulse width
		int h_back;					// Horizontal back porch
		int h_front;					// Horizontal front porch
		int h_dots;					// Horizontal dot count
		int h_mul;					// Horizontal multiplier
		int hd;						// 256,512,768,etc.

		int v_sync;					// Vertical sync total (H units)
		int v_pulse;					// Vertical sync pulse width (H units)
		int v_back;					// Vertical back porch (H units)
		int v_front;					// Vertical front porch (H units)
		int v_dots;					// Vertical dot count
		int v_mul;					// Vertical multiplier (0:interlace)
		int vd;						// 256,512,etc.,interlace,etc.

		DWORD ns;					// ns counter
		DWORD hus;					// hus counter
		DWORD v_synccnt;				// V-SYNC counter
		DWORD v_blankcnt;				// V-BLANK counter
		BOOL h_disp;					// Horizontal display flag
		BOOL v_disp;					// V-DISP flag
		BOOL v_blank;					// V-BLANK flag
		DWORD v_count;					// V-DISP counter
		int v_scan;					// Scan line position

		// Non-display variables
		int h_synctime;					// Sync time (hus)
		int h_disptime;					// Display time (hus)
		int v_cycletime;				// Cycle time (hus)
		int v_blanktime;				// V-blank time (hus)
		int v_synctime;					// Sync time (hus)
		int v_backtime;					// Back porch (hus)

		BOOL tmem;					// Text VRAM display
		BOOL gmem;					// Graphic VRAM display
		DWORD siz;					// Graphic VRAM 1024x1024 mode
		DWORD col;					// Graphic VRAM color mode

		DWORD text_scrlx;				// Text scroll X
		DWORD text_scrly;				// Text scroll Y
		DWORD grp_scrlx[4];				// Graphic scroll X
		DWORD grp_scrly[4];				// Graphic scroll Y

		int raster_count;				// Raster counter
		int raster_int;					// Raster interrupt position
		BOOL raster_copy;				// Raster copy flag
		BOOL raster_exec;				// Raster copy execute flag
		DWORD fast_clr;					// Graphic clear fast
	} crtc_t;

public:
	// Basic functions
	CRTC(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply configuration

	// Memory device interface
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only

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
										// Get display frequency
	DWORD FASTCALL GetDispCount() const	{ return crtc.v_count; }
										// Get display counter
	const crtc_t* FASTCALL GetWorkAddr() const { return &crtc; }
										// Get work address

private:
	void FASTCALL ReCalc();
										// Recalculate
	void FASTCALL HSync();
										// H-SYNC start
	void FASTCALL HDisp();
										// H-DISP start
	void FASTCALL VSync();
										// V-SYNC start
	void FASTCALL VBlank();
										// V-BLANK start
	int FASTCALL Ns2Hus(int ns)			{ return ns / 500; }
										// ns to 0.5us conversion
	int FASTCALL Hus2Ns(int hus)		{ return hus * 500; }
										// 0.5us to ns conversion
	void FASTCALL CheckRaster();
										// Raster interrupt check
	void FASTCALL TextVRAM();
										// Text VRAM setup
	int FASTCALL Get8DotClock() const;
										// Get 8 dot clock
	static const int DotClockTable[16];
										// 8 dot clock table
	static const BYTE ResetTable[26];
										// RESET register table
	crtc_t crtc;
										// CRTC internal data
	Event event;
										// Event
	TVRAM *tvram;
										// Text VRAM
	GVRAM *gvram;
										// Graphic VRAM
	Sprite *sprite;
										// Sprite controller
	MFP *mfp;
										// MFP
	Render *render;
										// Renderer
	Printer *printer;
										// Printer
};

#endif	// crtc_h
