//--------------------------------------------------------------------------
//
//	PX68k CRTC compatibility bridge.
//	Minimal state mirror used by the XM6 CRTC while the rest of the video
//	pipeline continues to fall back to XM6 implementations.
//
//--------------------------------------------------------------------------

#if !defined(px68k_crtc_port_h)
#define px68k_crtc_port_h

#include "os.h"

typedef struct Px68kTimingVideoView {
	BYTE valid;
	BYTE crtc_vsync_high;
	DWORD crtc_vline_total;
	WORD crtc_vstart;
	WORD crtc_vend;
	WORD crtc_intline;
	BYTE crtc_vstep;
	BYTE crtc_mode;
	BYTE crtc_fastclr;
} Px68kTimingVideoView;

typedef void (FASTCALL *Px68kCrtcApplyStateViewFn)(void *ctx, const struct Px68kCrtcStateView *view);
typedef void (FASTCALL *Px68kCrtcTextScrollChangedFn)(void *ctx, DWORD x, DWORD y);
typedef void (FASTCALL *Px68kCrtcGrpScrollChangedFn)(void *ctx, int block, DWORD x, DWORD y);
typedef void (FASTCALL *Px68kCrtcModeChangedFn)(void *ctx, BYTE mode);

typedef struct Px68kCrtcHost {
	void *ctx;
	Px68kCrtcApplyStateViewFn ApplyStateView;
	void (FASTCALL *MarkAllTextDirty)(void *ctx);
	void (FASTCALL *MarkTextDirtyLine)(void *ctx, DWORD line);
	void (FASTCALL *ScreenChanged)(void *ctx);
	void (FASTCALL *GeometryChanged)(void *ctx);
	void (FASTCALL *TimingChanged)(void *ctx);
	Px68kCrtcModeChangedFn ModeChanged;
	Px68kCrtcTextScrollChangedFn TextScrollChanged;
	Px68kCrtcGrpScrollChangedFn GrpScrollChanged;
} Px68kCrtcHost;

typedef struct Px68kCrtcState {
	BYTE regs[48];
	BYTE mode;
	BOOL hrl;
	BOOL lowres;
	BOOL textres;
	BOOL changed;
	BOOL h_disp;
	BOOL v_disp;
	BOOL v_blank;
	DWORD v_count;
	int raster_count;
	DWORD textdotx;
	DWORD textdoty;
	WORD vstart;
	WORD vend;
	WORD hstart;
	WORD hend;
	DWORD h_sync;
	DWORD h_pulse;
	DWORD h_back;
	DWORD h_front;
	DWORD v_sync;
	DWORD v_pulse;
	DWORD v_back;
	DWORD v_front;
	DWORD ns;
	DWORD hus;
	DWORD v_synccnt;
	DWORD v_blankcnt;
	DWORD textscrollx;
	DWORD textscrolly;
	DWORD grphscrollx[4];
	DWORD grphscrolly[4];
	BYTE fastclr;
	BYTE dispscan;
	DWORD fastclrline;
	WORD fastclrmask;
	WORD intline;
	BYTE vstep;
	DWORD visible_vline;
	int hsync_clk;
	int hd;
	int vd;
	BYTE rcflag[2];
	BYTE vcreg0[2];
	BYTE vcreg1[2];
	BYTE vcreg2[2];
} Px68kCrtcState;

typedef struct Px68kCrtcStateView {
	Px68kCrtcState state;
	Px68kTimingVideoView timing_view;
} Px68kCrtcStateView;

#endif	// px68k_crtc_port_h
