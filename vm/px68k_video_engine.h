//---------------------------------------------------------------------------
//
//	PX68k Video Engine - Complete 1:1 Port
//
//	This module provides a complete implementation of the px68k-libretro
//	video pipeline, including CRTC, GVRAM, TVRAM, Palette, BG/Sprite,
//	and WinDraw compositor.
//
//---------------------------------------------------------------------------

#if !defined(px68k_video_engine_h)
#define px68k_video_engine_h

#include "os.h"

//===========================================================================
//
//	Constants
//
//===========================================================================

// Screen dimensions
#define PX68K_SCREEN_WIDTH			768
#define PX68K_SCREEN_HEIGHT			512
#define PX68K_FULLSCREEN_WIDTH		800
#define PX68K_FULLSCREEN_HEIGHT		600

// Memory sizes
#define PX68K_GVRAM_SIZE			0x80000		// 512KB
#define PX68K_TVRAM_SIZE			0x80000		// 512KB
#define PX68K_BG_SIZE				0x8000		// 32KB
#define PX68K_SPRITE_REGS_SIZE		0x800		// 2KB
#define PX68K_PALETTE_REGS_SIZE		1024		// 1KB

// Line buffer sizes
#define PX68K_LINEBUF_SIZE			1024
#define PX68K_BG_LINEBUF_SIZE		1600
#define PX68K_TEXT_WORK_SIZE		(1024 * 1024)
#define PX68K_TEXT_PATTERN_SIZE		(2048 * 4)
#define PX68K_BGCHR8_SIZE			(8 * 8 * 256)
#define PX68K_BGCHR16_SIZE			(16 * 16 * 256)

// VSYNC timing
#define PX68K_VSYNC_HIGH			180310L
#define PX68K_VSYNC_NORM			162707L

//===========================================================================
//
//	PX68k Video State
//
//===========================================================================

// Forward declarations
class VM;
class CRTC;
class GVRAM;
class TVRAM;
class VC;
class Sprite;
class Render;

// CRTC state (mirrors px68k CRTC_Regs)
typedef struct Px68kCrtcRegs {
	BYTE regs[48];					// CRTC registers (24 words)
	BYTE mode;						// Operation mode
	BOOL lowres;					// 15kHz mode
	int vd;							// Vertical resolution mode
	DWORD textdotx;					// Text dot X
	DWORD textdoty;					// Text dot Y
	WORD vstart;					// Vertical start
	WORD vend;						// Vertical end
	WORD hstart;					// Horizontal start
	WORD hend;						// Horizontal end
	DWORD textscrollx;				// Text scroll X
	DWORD textscrolly;				// Text scroll Y
	DWORD grphscrollx[4];			// Graphic scroll X
	DWORD grphscrolly[4];			// Graphic scroll Y
	BYTE fastclr;					// Fast clear state
	DWORD fastclrline;				// Fast clear line
	WORD fastclrmask;				// Fast clear mask
	WORD intline;					// Interrupt line
	BYTE vstep;						// Vertical step
	int hsync_clk;					// HSYNC clock
	BYTE rcflag[2];					// Raster copy flags
	DWORD vline_total;				// VLINE total
} Px68kCrtcRegs;

// Video Controller registers
typedef struct Px68kVCRegs {
	BYTE vcreg0[2];					// VCReg0 (screen mode)
	BYTE vcreg1[2];					// VCReg1 (priority control)
	BYTE vcreg2[2];					// VCReg2 (ON/OFF control)
} Px68kVCRegs;

// Palette state
typedef struct Px68kPaletteState {
	BYTE regs[PX68K_PALETTE_REGS_SIZE];	// Palette registers
	WORD textpal[256];				// Text palette (RGB565)
	WORD grphpal[256];				// Graphic palette (RGB565)
	WORD pal16[65536];				// 16-bit color lookup table
	WORD ibit;						// I-bit mask
	WORD halfmask;					// Half-color mask
	WORD ix2;						// I-bit x2 value
	WORD pal_r;						// Red mask
	WORD pal_g;						// Green mask
	WORD pal_b;						// Blue mask
} Px68kPaletteState;

// BG/Sprite state
typedef struct Px68kBGSpriteState {
	BYTE bg[PX68K_BG_SIZE];			// BG pattern/map RAM
	BYTE sprite_regs[PX68K_SPRITE_REGS_SIZE];	// Sprite registers
	BYTE bg_regs[0x12];				// BG control registers
	BYTE bgchr8[PX68K_BGCHR8_SIZE];	// 8x8 character patterns
	BYTE bgchr16[PX68K_BGCHR16_SIZE];	// 16x16 character patterns
	WORD bg_linebuf[PX68K_BG_LINEBUF_SIZE];	// BG line buffer
	WORD bg_pribuf[PX68K_BG_LINEBUF_SIZE];	// BG priority buffer
	int bg_hadjust;					// BG horizontal adjust
	int bg_vline;					// BG VLINE
	DWORD vlinebg;					// VLINEBG
	DWORD bg0scrollx;				// BG0 scroll X
	DWORD bg0scrolly;				// BG0 scroll Y
	DWORD bg1scrollx;				// BG1 scroll X
	DWORD bg1scrolly;				// BG1 scroll Y
	WORD bg_chrend;					// BG character end
	WORD bg_bg0top;					// BG0 top address
	WORD bg_bg0end;					// BG0 end address
	WORD bg_bg1top;					// BG1 top address
	WORD bg_bg1end;					// BG1 end address
	BYTE bg_chrsize;				// BG character size
	DWORD bg_adrmask;				// BG address mask
} Px68kBGSpriteState;

// TVRAM state
typedef struct Px68kTVRAMState {
	BYTE tvram[PX68K_TVRAM_SIZE];	// Text VRAM
	BYTE textdirtyline[1024];		// Dirty line flags
	BYTE text_trflag[1024];			// Transparent flags
	BYTE textdrawwork[PX68K_TEXT_WORK_SIZE];	// Text draw work
	BYTE textdrawpattern[PX68K_TEXT_PATTERN_SIZE];	// Text patterns
} Px68kTVRAMState;

// GVRAM state
typedef struct Px68kGVRAMState {
	BYTE gvram[PX68K_GVRAM_SIZE];	// Graphic VRAM
	WORD grp_linebuf[PX68K_LINEBUF_SIZE];		// Graphic line buffer
	WORD grp_linebufsp[PX68K_LINEBUF_SIZE];		// Special priority buffer
	WORD grp_linebufsp2[PX68K_LINEBUF_SIZE];	// Semi-transparent base
	WORD grp_linebufsp_tr[PX68K_LINEBUF_SIZE];	// Transparency flags
	WORD pal16adr[256];			// 16-bit palette address buffer
} Px68kGVRAMState;

// Complete video engine state
typedef struct Px68kVideoEngineState {
	Px68kCrtcRegs crtc;
	Px68kVCRegs vc;
	Px68kPaletteState palette;
	Px68kBGSpriteState bgsprite;
	Px68kTVRAMState tvram;
	Px68kGVRAMState gvram;
	
	// Runtime state
	DWORD vline;					// Current scanline
	DWORD vlinebg;					// BG scanline
	BOOL debug_text;				// Text display enable
	BOOL debug_grp;					// Graphic display enable
	BOOL debug_sp;					// Sprite display enable
	
	// Timing state for frontend synchronization
	int vid_mode;					// Current video mode (15kHz/31kHz)
	BOOL changeav_timing;			// Flag to notify timing change
	DWORD vline_total;				// Total vertical lines (VLINE_TOTAL)
} Px68kVideoEngineState;

//===========================================================================
//
//	PX68k Video Engine Class
//
//===========================================================================

class Px68kVideoEngine {
public:
	// Constructor/Destructor
	Px68kVideoEngine();
	~Px68kVideoEngine();

	// Initialization
	BOOL Init();
	void Cleanup();
	void Reset();

	// State save/load
	BOOL SaveState(class Fileio *fio);
	BOOL LoadState(class Fileio *fio);

	// Frame processing
	void StartFrame();
	void EndFrame();
	void DrawLine(int vline);
	void DrawFrame();

	// CRTC interface
	BYTE FASTCALL CRTC_Read(DWORD addr);
	void FASTCALL CRTC_Write(DWORD addr, BYTE data);
	void FASTCALL CRTCSetAllDirty();
	void FASTCALL CRTCRasterCopy();			// Raster copy implementation

	// VC interface
	BYTE FASTCALL VCtrl_Read(DWORD addr);
	void FASTCALL VCtrl_Write(DWORD addr, BYTE data);

	// Timing interface for frontend synchronization
	int GetVidMode() const { return state_.vid_mode; }
	BOOL GetChangeAVTiming() const { return state_.changeav_timing; }
	void ClearChangeAVTiming() { state_.changeav_timing = FALSE; }
	DWORD GetVLineTotal() const { return state_.crtc.vline_total; }
	void SetVLine(DWORD vline) { state_.vline = vline; }
	void SetPhysicalVLine(DWORD vline) { physical_vline_ = vline; }

	// Palette interface
	BYTE FASTCALL Pal_Read(DWORD addr);
	void FASTCALL Pal_Write(DWORD addr, BYTE data);
	void FASTCALL Pal_SetColor();
	void FASTCALL Pal_ChangeContrast(int num);

	// GVRAM interface
	BYTE FASTCALL GVRAM_Read(DWORD addr);
	void FASTCALL GVRAM_Write(DWORD addr, BYTE data);
	void FASTCALL GVRAM_FastClear();

	// TVRAM interface
	BYTE FASTCALL TVRAM_Read(DWORD addr);
	void FASTCALL TVRAM_Write(DWORD addr, BYTE data);
	void FASTCALL TVRAMSetAllDirty();
	void FASTCALL TVRAMRCUpdate();

	// BG/Sprite interface
	BYTE FASTCALL BG_Read(DWORD addr);
	void FASTCALL BG_Write(DWORD addr, BYTE data);
	void FASTCALL BG_DrawLine(int opaq, int gd);

	// WinDraw compositor
	void FASTCALL WinDraw_DrawLine();
	void FASTCALL WinDrawDrawFrame();

	// Accessors
	const Px68kVideoEngineState* GetState() const { return &state_; }
	Px68kVideoEngineState* GetState() { return &state_; }
	
	DWORD GetTextDotX() const { return state_.crtc.textdotx; }
	DWORD GetTextDotY() const { return state_.crtc.textdoty; }
	DWORD GetVLine() const { return state_.vline; }
	
	const BYTE* GetGVRAM() const { return state_.gvram.gvram; }
	const BYTE* GetTVRAM() const { return state_.tvram.tvram; }
	const WORD* GetGrpLineBuf() const { return state_.gvram.grp_linebuf; }
	const WORD* GetBGLineBuf() const { return state_.bgsprite.bg_linebuf; }
	const BYTE* GetTextTrFlag() const { return state_.tvram.text_trflag; }

	// RGB565 output
	void SetRGB565Masks(WORD r, WORD g, WORD b);
	WORD GetPal16(WORD index) const { return state_.palette.pal16[index]; }
	WORD GetTextPal(BYTE index) const { return state_.palette.textpal[index]; }
	WORD GetGrphPal(BYTE index) const { return state_.palette.grphpal[index]; }
	WORD* GetScreenBuffer() { return screen_buffer_; }

private:
	// Graphic drawing functions
	void FASTCALL GrpDrawLine16();
	void FASTCALL GrpDrawLine8(int page, int opaq);
	void FASTCALL GrpDrawLine4(DWORD page, int opaq);
	void FASTCALL GrpDrawLine4h();
	void FASTCALL GrpDrawLine16SP();
	void FASTCALL GrpDrawLine8SP(int page);
	void FASTCALL GrpDrawLine4SP(DWORD page);
	void FASTCALL GrpDrawLine4hSP();
	void FASTCALL GrpDrawLine8TR(int page, int opaq);
	void FASTCALL GrpDrawLine8TR_GT(int page, int opaq);
	void FASTCALL GrpDrawLine4TR(DWORD page, int opaq);

	// Text drawing
	void FASTCALL TextDrawLine(int opaq);

	// Sprite drawing
	void FASTCALL Sprite_DrawLineMcr(int pri);

	// BG drawing
	void FASTCALL BGDrawLineMcr8(WORD bgtop, DWORD scrollx, DWORD scrolly);
	void FASTCALL BGDrawLineMcr16(WORD bgtop, DWORD scrollx, DWORD scrolly);
	void FASTCALL BGDrawLineMcr8_ng(WORD bgtop, DWORD scrollx, DWORD scrolly);
	void FASTCALL BGDrawLineMcr16_ng(WORD bgtop, DWORD scrollx, DWORD scrolly);

	// WinDraw helpers
	void FASTCALL WinDrawDrawGrpLine(int opaq);
	void FASTCALL WinDrawDrawGrpLineNonSP(int opaq);
	void FASTCALL WinDrawDrawTextLine(int opaq, int td);
	void FASTCALL WinDrawDrawTextLineTR(int opaq);
	void FASTCALL WinDrawDrawBGLine(int opaq, int td);
	void FASTCALL WinDrawDrawBGLineTR(int opaq);
	void FASTCALL WinDrawDrawPriLine();
	void FASTCALL WinDrawDrawHalfFillLine();

	// Internal helpers
	void FASTCALL UpdateScreenMode();
	void FASTCALL UpdateBGScroll();
	void FASTCALL UpdateGrpScroll();
	void FASTCALL MakeTextPattern();
	DWORD FASTCALL CalcBGHAdjust();

	// State
	Px68kVideoEngineState state_;
	
	// Scanline physical (CRT beam line, like VLINE in original px68k)
	DWORD physical_vline_;
	
	// RGB565 masks
	WORD windraw_pal16r_;
	WORD windraw_pal16g_;
	WORD windraw_pal16b_;
	
	// Output buffer
	WORD *screen_buffer_;
	DWORD screen_width_;
	DWORD screen_height_;
};

//===========================================================================
//
//	Global accessor (singleton pattern for compatibility)
//
//===========================================================================

Px68kVideoEngine* FASTCALL GetPx68kVideoEngine();
void FASTCALL SetPx68kVideoEngine(Px68kVideoEngine* engine);

//===========================================================================
//
//	Inline helper functions (matching px68k exactly)
//
//===========================================================================

// Get word from byte array (handling endianness)
#ifdef MSB_FIRST
	#define PX68K_GET_WORD_W8(src) (*(WORD *)(src))
#else
	#define PX68K_GET_WORD_W8(src) ((WORD)(*((BYTE *)(src)) | (*((BYTE *)(src) + 1) << 8)))
#endif

// Fast clear mask table
static const WORD Px68kFastClearMask[16] = {
	0xffff, 0xfff0, 0xff0f, 0xff00,
	0xf0ff, 0xf0f0, 0xf00f, 0xf000,
	0x0fff, 0x0ff0, 0x0f0f, 0x0f00,
	0x00ff, 0x00f0, 0x000f, 0x0000
};

#endif	// px68k_video_engine_h
