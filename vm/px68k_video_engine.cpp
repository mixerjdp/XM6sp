//---------------------------------------------------------------------------
//
//	PX68k Video Engine - Complete 1:1 Port
//
//	This module provides a complete implementation of the px68k-libretro
//	video pipeline.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "px68k_video_engine.h"
#include "fileio.h"
#include <cstring>
#include <cstdlib>

//===========================================================================
//
//	Global singleton
//
//===========================================================================

static Px68kVideoEngine *g_px68k_video_engine = NULL;

Px68kVideoEngine* FASTCALL GetPx68kVideoEngine()
{
	return g_px68k_video_engine;
}

void FASTCALL SetPx68kVideoEngine(Px68kVideoEngine* engine)
{
	g_px68k_video_engine = engine;
}

//===========================================================================
//
//	Constructor/Destructor
//
//===========================================================================

Px68kVideoEngine::Px68kVideoEngine()
{
	std::memset(&state_, 0, sizeof(state_));
	physical_vline_ = 0;
	screen_buffer_ = NULL;
	screen_width_ = PX68K_FULLSCREEN_WIDTH;
	screen_height_ = PX68K_FULLSCREEN_HEIGHT;
	windraw_pal16r_ = 0xf800;
	windraw_pal16g_ = 0x07e0;
	windraw_pal16b_ = 0x001f;
}

Px68kVideoEngine::~Px68kVideoEngine()
{
	Cleanup();
}

//===========================================================================
//
//	Initialization
//
//===========================================================================

BOOL Px68kVideoEngine::Init()
{
	// Allocate screen buffer
	screen_buffer_ = (WORD*)std::malloc(PX68K_FULLSCREEN_WIDTH * PX68K_FULLSCREEN_HEIGHT * 2);
	if (!screen_buffer_) {
		return FALSE;
	}
	std::memset(screen_buffer_, 0, PX68K_FULLSCREEN_WIDTH * PX68K_FULLSCREEN_HEIGHT * 2);

	// Initialize CRTC
	std::memset(state_.crtc.regs, 0, sizeof(state_.crtc.regs));
	state_.crtc.textdotx = 768;
	state_.crtc.textdoty = 512;
	state_.crtc.vstart = 40;
	state_.crtc.vend = 552;
	state_.crtc.hstart = 28;
	state_.crtc.hend = 124;
	state_.crtc.hsync_clk = 324;
	state_.crtc.vline_total = 567;
	state_.crtc.vstep = 2;

	// Initialize VC
	std::memset(&state_.vc, 0, sizeof(state_.vc));

	// Initialize timing state
	state_.vid_mode = 0;
	state_.changeav_timing = FALSE;
	state_.vline_total = 567;

	// Initialize Palette
	Pal_SetColor();

	// Initialize GVRAM
	std::memset(state_.gvram.gvram, 0, PX68K_GVRAM_SIZE);
	std::memset(state_.gvram.grp_linebuf, 0, sizeof(state_.gvram.grp_linebuf));
	std::memset(state_.gvram.grp_linebufsp, 0, sizeof(state_.gvram.grp_linebufsp));
	std::memset(state_.gvram.grp_linebufsp2, 0, sizeof(state_.gvram.grp_linebufsp2));
	std::memset(state_.gvram.grp_linebufsp_tr, 0, sizeof(state_.gvram.grp_linebufsp_tr));
	
	// Initialize Pal16Adr for 16-bit color
	for (int i = 0; i < 128; i++) {
		state_.gvram.pal16adr[i * 2] = i * 4;
		state_.gvram.pal16adr[i * 2 + 1] = i * 4 + 1;
	}

	// Initialize TVRAM
	std::memset(state_.tvram.tvram, 0, PX68K_TVRAM_SIZE);
	std::memset(state_.tvram.textdirtyline, 1, sizeof(state_.tvram.textdirtyline));
	std::memset(state_.tvram.text_trflag, 0, sizeof(state_.tvram.text_trflag));
	std::memset(state_.tvram.textdrawwork, 0, PX68K_TEXT_WORK_SIZE);
	MakeTextPattern();

	// Initialize BG/Sprite
	std::memset(state_.bgsprite.bg, 0, PX68K_BG_SIZE);
	std::memset(state_.bgsprite.sprite_regs, 0, PX68K_SPRITE_REGS_SIZE);
	std::memset(state_.bgsprite.bg_regs, 0, sizeof(state_.bgsprite.bg_regs));
	std::memset(state_.bgsprite.bgchr8, 0, PX68K_BGCHR8_SIZE);
	std::memset(state_.bgsprite.bgchr16, 0, PX68K_BGCHR16_SIZE);
	std::memset(state_.bgsprite.bg_linebuf, 0, sizeof(state_.bgsprite.bg_linebuf));
	std::memset(state_.bgsprite.bg_pribuf, 0xff, sizeof(state_.bgsprite.bg_pribuf));
	state_.bgsprite.bg_chrend = 0x8000;
	state_.bgsprite.bg_chrsize = 16;
	state_.bgsprite.bg_adrmask = 1023;

	// Initialize debug flags
	state_.debug_text = TRUE;
	state_.debug_grp = TRUE;
	state_.debug_sp = TRUE;

	// Set as global
	SetPx68kVideoEngine(this);

	return TRUE;
}

void Px68kVideoEngine::Cleanup()
{
	if (screen_buffer_) {
		std::free(screen_buffer_);
		screen_buffer_ = NULL;
	}
	SetPx68kVideoEngine(NULL);
}

void Px68kVideoEngine::Reset()
{
	// Reset CRTC
	std::memset(state_.crtc.regs, 0, sizeof(state_.crtc.regs));
	state_.crtc.textdotx = 768;
	state_.crtc.textdoty = 512;
	state_.crtc.mode = 0;
	state_.crtc.fastclr = 0;
	state_.crtc.fastclrline = 0;
	state_.crtc.fastclrmask = 0;
	state_.crtc.rcflag[0] = 0;
	state_.crtc.rcflag[1] = 0;

	// Reset VC
	std::memset(&state_.vc, 0, sizeof(state_.vc));

	// Reset Palette
	std::memset(state_.palette.regs, 0, PX68K_PALETTE_REGS_SIZE);
	std::memset(state_.palette.textpal, 0, sizeof(state_.palette.textpal));
	std::memset(state_.palette.grphpal, 0, sizeof(state_.palette.grphpal));
	Pal_SetColor();

	// Reset GVRAM
	std::memset(state_.gvram.gvram, 0, PX68K_GVRAM_SIZE);

	// Reset TVRAM
	std::memset(state_.tvram.tvram, 0, PX68K_TVRAM_SIZE);
	TVRAMSetAllDirty();

	// Reset BG/Sprite
	std::memset(state_.bgsprite.bg, 0, PX68K_BG_SIZE);
	std::memset(state_.bgsprite.sprite_regs, 0, PX68K_SPRITE_REGS_SIZE);
	std::memset(state_.bgsprite.bg_regs, 0, sizeof(state_.bgsprite.bg_regs));
	std::memset(state_.bgsprite.bgchr8, 0, PX68K_BGCHR8_SIZE);
	std::memset(state_.bgsprite.bgchr16, 0, PX68K_BGCHR16_SIZE);
	state_.bgsprite.bg_chrend = 0x8000;
}

//===========================================================================
//
//	Make Text Pattern Table
//
//===========================================================================

void FASTCALL Px68kVideoEngine::MakeTextPattern()
{
	int i, j, bit;

	std::memset(state_.tvram.textdrawpattern, 0, PX68K_TEXT_PATTERN_SIZE);

	for (i = 0; i < 256; i++) {
		for (j = 0, bit = 0x80; j < 8; j++, bit >>= 1) {
			if (i & bit) {
				state_.tvram.textdrawpattern[i * 8 + j] = 1;
				state_.tvram.textdrawpattern[i * 8 + j + 2048] = 2;
				state_.tvram.textdrawpattern[i * 8 + j + 4096] = 4;
				state_.tvram.textdrawpattern[i * 8 + j + 6144] = 8;
			}
		}
	}
}

//===========================================================================
//
//	Palette Functions
//
//===========================================================================

void FASTCALL Px68kVideoEngine::Pal_SetColor()
{
	int i;
	WORD bit;
	WORD R[5] = {0, 0, 0, 0, 0};
	WORD G[5] = {0, 0, 0, 0, 0};
	WORD B[5] = {0, 0, 0, 0, 0};
	int r = 5, g = 5, b = 5;
	WORD tempmask = 0;

	state_.palette.pal_r = 0;
	state_.palette.pal_g = 0;
	state_.palette.pal_b = 0;

	for (bit = 0x8000; bit; bit >>= 1) {
		if ((windraw_pal16r_ & bit) && r) {
			R[--r] = bit;
			tempmask |= bit;
			state_.palette.pal_r |= bit;
		}
		if ((windraw_pal16g_ & bit) && g) {
			G[--g] = bit;
			tempmask |= bit;
			state_.palette.pal_g |= bit;
		}
		if ((windraw_pal16b_ & bit) && b) {
			B[--b] = bit;
			tempmask |= bit;
			state_.palette.pal_b |= bit;
		}
	}

	// Find I-bit (first unused bit)
	state_.palette.ibit = 1;
	for (bit = 1; bit; bit <<= 1) {
		if (!(tempmask & bit)) {
			state_.palette.ibit = bit;
			break;
		}
	}

	state_.palette.halfmask = (WORD)~(B[0] | R[0] | G[0] | state_.palette.ibit);
	state_.palette.ix2 = state_.palette.ibit << 1;

	// Build Pal16 lookup table
	for (i = 0; i < 65536; i++) {
		bit = 0;
		if (i & 0x8000) bit |= G[4];
		if (i & 0x4000) bit |= G[3];
		if (i & 0x2000) bit |= G[2];
		if (i & 0x1000) bit |= G[1];
		if (i & 0x0800) bit |= G[0];
		if (i & 0x0400) bit |= R[4];
		if (i & 0x0200) bit |= R[3];
		if (i & 0x0100) bit |= R[2];
		if (i & 0x0080) bit |= R[1];
		if (i & 0x0040) bit |= R[0];
		if (i & 0x0020) bit |= B[4];
		if (i & 0x0010) bit |= B[3];
		if (i & 0x0008) bit |= B[2];
		if (i & 0x0004) bit |= B[1];
		if (i & 0x0002) bit |= B[0];
		if (i & 0x0001) bit |= state_.palette.ibit;
		state_.palette.pal16[i] = bit;
	}
}

void FASTCALL Px68kVideoEngine::Pal_ChangeContrast(int num)
{
	WORD bit;
	WORD R[5] = {0, 0, 0, 0, 0};
	WORD G[5] = {0, 0, 0, 0, 0};
	WORD B[5] = {0, 0, 0, 0, 0};
	int r, g, b, i;
	int palr, palg, palb;
	WORD pal;

	TVRAMSetAllDirty();

	r = g = b = 5;

	for (bit = 0x8000; bit; bit >>= 1) {
		if ((windraw_pal16r_ & bit) && r) R[--r] = bit;
		if ((windraw_pal16g_ & bit) && g) G[--g] = bit;
		if ((windraw_pal16b_ & bit) && b) B[--b] = bit;
	}

	for (i = 0; i < 65536; i++) {
		palr = palg = palb = 0;
		if (i & 0x8000) palg |= G[4];
		if (i & 0x4000) palg |= G[3];
		if (i & 0x2000) palg |= G[2];
		if (i & 0x1000) palg |= G[1];
		if (i & 0x0800) palg |= G[0];
		if (i & 0x0400) palr |= R[4];
		if (i & 0x0200) palr |= R[3];
		if (i & 0x0100) palr |= R[2];
		if (i & 0x0080) palr |= R[1];
		if (i & 0x0040) palr |= R[0];
		if (i & 0x0020) palb |= B[4];
		if (i & 0x0010) palb |= B[3];
		if (i & 0x0008) palb |= B[2];
		if (i & 0x0004) palb |= B[1];
		if (i & 0x0002) palb |= B[0];
		pal = (WORD)(palr | palb | palg);
		palg = (WORD)((palg * num) / 15) & state_.palette.pal_g;
		palr = (WORD)((palr * num) / 15) & state_.palette.pal_r;
		palb = (WORD)((palb * num) / 15) & state_.palette.pal_b;
		state_.palette.pal16[i] = (WORD)(palr | palb | palg);
		if ((pal) && (!state_.palette.pal16[i])) {
			state_.palette.pal16[i] = B[0];
		}
		if (i & 0x0001) {
			state_.palette.pal16[i] |= state_.palette.ibit;
		}
	}

	// Update palette lookups
	for (i = 0; i < 256; i++) {
		pal = state_.palette.regs[i * 2];
		pal = (pal << 8) + state_.palette.regs[i * 2 + 1];
		state_.palette.grphpal[i] = state_.palette.pal16[pal];

		pal = state_.palette.regs[i * 2 + 512];
		pal = (pal << 8) + state_.palette.regs[i * 2 + 513];
		state_.palette.textpal[i] = state_.palette.pal16[pal];
	}
}

BYTE FASTCALL Px68kVideoEngine::Pal_Read(DWORD addr)
{
	if (addr < 0xe82400) {
		return state_.palette.regs[addr - 0xe82000];
	}
	return 0xff;
}

void FASTCALL Px68kVideoEngine::Pal_Write(DWORD addr, BYTE data)
{
	WORD pal;

	if (addr >= 0xe82400) return;

	addr -= 0xe82000;
	if (state_.palette.regs[addr] == data) return;

	if (addr < 0x200) {
		state_.palette.regs[addr] = data;
		TVRAMSetAllDirty();
		pal = state_.palette.regs[addr & 0xfffe];
		pal = (pal << 8) + state_.palette.regs[addr | 1];
		state_.palette.grphpal[addr / 2] = state_.palette.pal16[pal];
	}
	else if (addr < 0x400) {
		state_.palette.regs[addr] = data;
		TVRAMSetAllDirty();
		pal = state_.palette.regs[addr & 0xfffe];
		pal = (pal << 8) + state_.palette.regs[addr | 1];
		state_.palette.textpal[(addr - 0x200) / 2] = state_.palette.pal16[pal];
	}
}

//===========================================================================
//
//	CRTC Functions
//
//===========================================================================

BYTE FASTCALL Px68kVideoEngine::CRTC_Read(DWORD addr)
{
	if (addr < 0xe803ff) {
		int reg = addr & 0x3f;
		if ((reg >= 0x28) && (reg <= 0x2b)) {
			return state_.crtc.regs[reg];
		}
	}
	else if (addr == 0xe80481) {
		BYTE data = state_.crtc.mode;
		if (state_.crtc.fastclr) {
			data |= 0x02;
		}
		else {
			data &= 0xfd;
		}
		return data;
	}
	return 0x00;
}

void FASTCALL Px68kVideoEngine::CRTC_Write(DWORD addr, BYTE data)
{
	BYTE reg = (BYTE)(addr & 0x3f);

	if (addr < 0xe80400) {
		if (reg >= 0x30) return;
		if (state_.crtc.regs[reg] == data) return;
		state_.crtc.regs[reg] = data;
		TVRAMSetAllDirty();

		switch (reg) {
		case 0x04:
		case 0x05:
			state_.crtc.hstart = (((WORD)state_.crtc.regs[0x04] << 8) + state_.crtc.regs[0x05]) & 1023;
			if (state_.crtc.hend > state_.crtc.hstart) {
				state_.crtc.textdotx = (state_.crtc.hend - state_.crtc.hstart) * 8;
			}
			state_.bgsprite.bg_hadjust = ((int)state_.bgsprite.bg_regs[0x0d] - (state_.crtc.hstart + 4)) * 8;
			break;

		case 0x06:
		case 0x07:
			state_.crtc.hend = (((WORD)state_.crtc.regs[0x06] << 8) + state_.crtc.regs[0x07]) & 1023;
			if (state_.crtc.hend > state_.crtc.hstart) {
				state_.crtc.textdotx = (state_.crtc.hend - state_.crtc.hstart) * 8;
			}
			break;

		case 0x08:
		case 0x09:
			state_.crtc.vline_total = (((WORD)state_.crtc.regs[0x08] << 8) + state_.crtc.regs[0x09]) & 1023;
			state_.vline_total = state_.crtc.vline_total;  // Sync to timing state
			state_.crtc.hsync_clk = (int)(((state_.crtc.regs[0x29] & 0x10) ? PX68K_VSYNC_HIGH : PX68K_VSYNC_NORM) / state_.crtc.vline_total);
			break;

		case 0x0c:
		case 0x0d:
			state_.crtc.vstart = (((WORD)state_.crtc.regs[0x0c] << 8) + state_.crtc.regs[0x0d]) & 1023;
			state_.bgsprite.bg_vline = ((int)state_.bgsprite.bg_regs[0x0f] - state_.crtc.vstart) / ((state_.bgsprite.bg_regs[0x11] & 4) ? 1 : 2);
			state_.crtc.textdoty = state_.crtc.vend - state_.crtc.vstart;
			UpdateScreenMode();
			break;

		case 0x0e:
		case 0x0f:
			state_.crtc.vend = (((WORD)state_.crtc.regs[0x0e] << 8) + state_.crtc.regs[0x0f]) & 1023;
			state_.crtc.textdoty = state_.crtc.vend - state_.crtc.vstart;
			UpdateScreenMode();
			break;

		case 0x28:
			TVRAMSetAllDirty();
			break;

		case 0x29:
			state_.crtc.hsync_clk = (int)(((state_.crtc.regs[0x29] & 0x10) ? PX68K_VSYNC_HIGH : PX68K_VSYNC_NORM) / state_.crtc.vline_total);
			state_.crtc.textdoty = state_.crtc.vend - state_.crtc.vstart;
			UpdateScreenMode();
			break;

		case 0x12:
		case 0x13:
			state_.crtc.intline = (((WORD)state_.crtc.regs[0x12] << 8) + state_.crtc.regs[0x13]) & 1023;
			break;

		case 0x14:
		case 0x15:
			state_.crtc.textscrollx = (((DWORD)state_.crtc.regs[0x14] << 8) + state_.crtc.regs[0x15]) & 1023;
			break;

		case 0x16:
		case 0x17:
			state_.crtc.textscrolly = (((DWORD)state_.crtc.regs[0x16] << 8) + state_.crtc.regs[0x17]) & 1023;
			break;

		case 0x18:
		case 0x19:
			state_.crtc.grphscrollx[0] = (((DWORD)state_.crtc.regs[0x18] << 8) + state_.crtc.regs[0x19]) & 1023;
			break;

		case 0x1a:
		case 0x1b:
			state_.crtc.grphscrolly[0] = (((DWORD)state_.crtc.regs[0x1a] << 8) + state_.crtc.regs[0x1b]) & 1023;
			break;

		case 0x1c:
		case 0x1d:
			state_.crtc.grphscrollx[1] = (((DWORD)state_.crtc.regs[0x1c] << 8) + state_.crtc.regs[0x1d]) & 511;
			break;

		case 0x1e:
		case 0x1f:
			state_.crtc.grphscrolly[1] = (((DWORD)state_.crtc.regs[0x1e] << 8) + state_.crtc.regs[0x1f]) & 511;
			break;

		case 0x20:
		case 0x21:
			state_.crtc.grphscrollx[2] = (((DWORD)state_.crtc.regs[0x20] << 8) + state_.crtc.regs[0x21]) & 511;
			break;

		case 0x22:
		case 0x23:
			state_.crtc.grphscrolly[2] = (((DWORD)state_.crtc.regs[0x22] << 8) + state_.crtc.regs[0x23]) & 511;
			break;

		case 0x24:
		case 0x25:
			state_.crtc.grphscrollx[3] = (((DWORD)state_.crtc.regs[0x24] << 8) + state_.crtc.regs[0x25]) & 511;
			break;

		case 0x26:
		case 0x27:
			state_.crtc.grphscrolly[3] = (((DWORD)state_.crtc.regs[0x26] << 8) + state_.crtc.regs[0x27]) & 511;
			break;

		case 0x2c:
		case 0x2d:
			state_.crtc.rcflag[reg - 0x2c] = 1;
			if ((state_.crtc.mode & 8) && state_.crtc.rcflag[1]) {
				// Execute raster copy
				CRTCRasterCopy();
				state_.crtc.rcflag[0] = 0;
				state_.crtc.rcflag[1] = 0;
			}
			break;
		}
	}
	else if (addr == 0xe80481) {
		state_.crtc.mode = (data | (state_.crtc.mode & 2));
		if (state_.crtc.mode & 8) {
			// Execute raster copy on mode write
			CRTCRasterCopy();
			state_.crtc.rcflag[0] = 0;
			state_.crtc.rcflag[1] = 0;
		}
		if (state_.crtc.mode & 2) {
			// Fast clear
			state_.crtc.fastclrline = state_.vline;
			state_.crtc.fastclrmask = Px68kFastClearMask[state_.crtc.regs[0x2b] & 15];
		}
	}
}

//===========================================================================
//
//	CRTC Raster Copy Implementation
//
//===========================================================================

void FASTCALL Px68kVideoEngine::CRTCRasterCopy()
{
	// Get source and destination from CRTC registers
	DWORD line = (((DWORD)state_.crtc.regs[0x2d]) << 2);
	DWORD src  = (((DWORD)state_.crtc.regs[0x2c]) << 9);
	DWORD dst  = (((DWORD)state_.crtc.regs[0x2d]) << 9);

	// Offsets for the 4 TVRAM planes
	static const DWORD off[4] = { 0, 0x20000, 0x40000, 0x60000 };
	int i, bit;

	// Copy data between TVRAM planes based on mask in register 0x2b
	for (bit = 0; bit < 4; bit++) {
		if (state_.crtc.regs[0x2b] & (1 << bit)) {
			std::memmove(&state_.tvram.tvram[dst + off[bit]], 
			             &state_.tvram.tvram[src + off[bit]],
			             sizeof(DWORD) * 128);
		}
	}

	// Mark affected lines as dirty
	line = (line - state_.crtc.textscrolly) & 0x3ff;
	for (i = 0; i < 4; i++) {
		state_.tvram.textdirtyline[line] = 1;
		line = (line + 1) & 0x3ff;
	}

	// Update TextDrawWork for the destination
	TVRAMRCUpdate();
}

void FASTCALL Px68kVideoEngine::CRTCSetAllDirty()
{
	std::memset(state_.tvram.textdirtyline, 1, sizeof(state_.tvram.textdirtyline));
}

void FASTCALL Px68kVideoEngine::UpdateScreenMode()
{
	int old_vid_mode = state_.vid_mode;
	
	// Calculate video mode from CRTC register 0x29
	// Bit 4: 0=15kHz, 1=31kHz
	state_.vid_mode = (state_.crtc.regs[0x29] & 0x10) ? 1 : 0;
	
	// Calculate vertical step based on register 0x29 bits 2 and 4
	if ((state_.crtc.regs[0x29] & 0x14) == 0x10) {
		// 512 lines mode (divide by 2)
		state_.crtc.textdoty = (state_.crtc.vend - state_.crtc.vstart) / 2;
		state_.crtc.vstep = 1;
	}
	else if ((state_.crtc.regs[0x29] & 0x14) == 0x04) {
		// 1024 lines mode (multiply by 2)
		state_.crtc.textdoty = (state_.crtc.vend - state_.crtc.vstart) * 2;
		state_.crtc.vstep = 4;
	}
	else {
		// Normal 512 lines
		state_.crtc.textdoty = state_.crtc.vend - state_.crtc.vstart;
		state_.crtc.vstep = 2;
	}
	
	// Notify frontend if video mode changed
	if (state_.vid_mode != old_vid_mode) {
		state_.changeav_timing = TRUE;
	}
}

//===========================================================================
//
//	VC Functions
//
//===========================================================================

BYTE FASTCALL Px68kVideoEngine::VCtrl_Read(DWORD addr)
{
	if (addr < 0x00e82400) {
		return Pal_Read(addr);
	}
	if (addr < 0x00e82500) {
		return state_.vc.vcreg0[addr & 1];
	}
	if (addr < 0x00e82600) {
		return state_.vc.vcreg1[addr & 1];
	}
	if (addr < 0x00e82700) {
		return state_.vc.vcreg2[addr & 1];
	}
	return 0xff;
}

void FASTCALL Px68kVideoEngine::VCtrl_Write(DWORD addr, BYTE data)
{
	if (addr < 0x00e82400) {
		Pal_Write(addr, data);
	}
	else if (addr < 0x00e82500) {
		if (state_.vc.vcreg0[addr & 1] != data) {
			state_.vc.vcreg0[addr & 1] = data;
			TVRAMSetAllDirty();
		}
	}
	else if (addr < 0x00e82600) {
		if (state_.vc.vcreg1[addr & 1] != data) {
			state_.vc.vcreg1[addr & 1] = data;
			TVRAMSetAllDirty();
		}
	}
	else if (addr < 0x00e82700) {
		if (state_.vc.vcreg2[addr & 1] != data) {
			state_.vc.vcreg2[addr & 1] = data;
			TVRAMSetAllDirty();
		}
	}
}

//===========================================================================
//
//	GVRAM Functions
//
//===========================================================================

BYTE FASTCALL Px68kVideoEngine::GVRAM_Read(DWORD addr)
{
	int type;

	addr &= 0x1fffff;

	if (state_.crtc.regs[0x28] & 8) {
		type = 4;
	}
	else if (state_.crtc.regs[0x28] & 4) {
		type = 0;
	}
	else {
		type = (state_.crtc.regs[0x28] & 3) + 1;
	}

	switch (type) {
	case 0: // 1024 dot, 16 colors
		if ((addr & 1) == 0) return 0;
		if (addr & 0x100000) {
			if (addr & 0x400) {
				addr = ((addr >> 1) & 0x7fc00) | (addr & 0x3ff);
				return (state_.gvram.gvram[addr] >> 4);
			}
			else {
				addr = ((addr >> 1) & 0x7fc00) | (addr & 0x3ff);
				return (state_.gvram.gvram[addr] & 0x0f);
			}
		}
		else {
			if (addr & 0x400) {
				addr = ((addr >> 1) & 0x7fc00) | (addr & 0x3ff);
				return (state_.gvram.gvram[addr ^ 1] >> 4);
			}
			else {
				addr = ((addr >> 1) & 0x7fc00) | (addr & 0x3ff);
				return (state_.gvram.gvram[addr ^ 1] & 0x0f);
			}
		}
		break;

	case 1: // 512 dot, 16 colors
		if ((addr & 1) == 0) return 0;
		if (addr < 0x80000) {
			return (state_.gvram.gvram[addr ^ 1] & 0x0f);
		}
		if (addr < 0x100000) {
			addr &= 0x7ffff;
			return (state_.gvram.gvram[addr ^ 1] >> 4);
		}
		if (addr < 0x180000) {
			addr &= 0x7ffff;
			return (state_.gvram.gvram[addr] & 0x0f);
		}
		addr &= 0x7ffff;
		return (state_.gvram.gvram[addr] >> 4);

	case 2: // 512 dot, 256 colors
	case 3:
		if (addr < 0x80000) {
			if (addr & 1) {
				return state_.gvram.gvram[addr ^ 1];
			}
			return 0;
		}
		if (addr < 0x100000) {
			addr &= 0x7ffff;
			if (addr & 1) {
				return state_.gvram.gvram[addr];
			}
			return 0;
		}
		break;

	case 4: // 65536 colors
		if (addr < 0x80000) {
			return state_.gvram.gvram[addr ^ 1];
		}
		break;
	}

	return 0;
}

void FASTCALL Px68kVideoEngine::GVRAM_Write(DWORD addr, BYTE data)
{
	int line = 1023;
	int scr = 0;
	DWORD temp;
	int type;

	addr &= 0x1fffff;

	if (state_.crtc.regs[0x28] & 8) {
		type = 4;
	}
	else if (state_.crtc.regs[0x28] & 4) {
		type = 0;
	}
	else {
		type = (state_.crtc.regs[0x28] & 3) + 1;
	}

	switch (type) {
	case 0: // 1024 dot, 16 colors
		if ((addr & 1) == 0) break;
		line = ((addr / 2048) - state_.crtc.grphscrolly[0]) & 1023;
		if (addr & 0x100000) {
			if (addr & 0x400) {
				addr = ((addr & 0xff800) >> 1) + (addr & 0x3ff);
				temp = state_.gvram.gvram[addr] & 0x0f;
				temp |= (data & 0x0f) << 4;
				state_.gvram.gvram[addr] = (BYTE)temp;
			}
			else {
				addr = ((addr & 0xff800) >> 1) + (addr & 0x3ff);
				temp = state_.gvram.gvram[addr] & 0xf0;
				temp |= data & 0x0f;
				state_.gvram.gvram[addr] = (BYTE)temp;
			}
		}
		else {
			if (addr & 0x400) {
				addr = ((addr & 0xff800) >> 1) + (addr & 0x3ff);
				temp = state_.gvram.gvram[addr ^ 1] & 0x0f;
				temp |= (data & 0x0f) << 4;
				state_.gvram.gvram[addr ^ 1] = (BYTE)temp;
			}
			else {
				addr = ((addr & 0xff800) >> 1) + (addr & 0x3ff);
				temp = state_.gvram.gvram[addr ^ 1] & 0xf0;
				temp |= data & 0x0f;
				state_.gvram.gvram[addr ^ 1] = (BYTE)temp;
			}
		}
		break;

	case 1: // 16 colors
		if ((addr & 1) == 0) break;
		scr = state_.crtc.grphscrolly[(addr >> 19) & 3];
		line = (((addr & 0x7ffff) >> 10) - scr) & 511;
		if (addr < 0x80000) {
			temp = (state_.gvram.gvram[addr ^ 1] & 0xf0);
			temp |= (data & 0x0f);
			state_.gvram.gvram[addr ^ 1] = (BYTE)temp;
		}
		else if (addr < 0x100000) {
			addr &= 0x7ffff;
			temp = (state_.gvram.gvram[addr ^ 1] & 0x0f);
			temp |= (data << 4);
			state_.gvram.gvram[addr ^ 1] = (BYTE)temp;
		}
		else if (addr < 0x180000) {
			addr &= 0x7ffff;
			temp = (state_.gvram.gvram[addr] & 0xf0);
			temp |= (data & 0x0f);
			state_.gvram.gvram[addr] = (BYTE)temp;
		}
		else {
			addr &= 0x7ffff;
			temp = (state_.gvram.gvram[addr] & 0x0f);
			temp |= (data << 4);
			state_.gvram.gvram[addr] = (BYTE)temp;
		}
		break;

	case 2: // 256 colors
	case 3:
		if ((addr & 1) == 0) break;
		if (addr < 0x100000) {
			scr = state_.crtc.grphscrolly[(addr >> 18) & 2];
			line = (((addr & 0x7ffff) >> 10) - scr) & 511;
			state_.tvram.textdirtyline[line] = 1;
			scr = state_.crtc.grphscrolly[((addr >> 18) & 2) + 1];
			line = (((addr & 0x7ffff) >> 10) - scr) & 511;
			if (addr < 0x80000) {
				state_.gvram.gvram[addr ^ 1] = (BYTE)data;
			}
			else {
				addr &= 0x7ffff;
				state_.gvram.gvram[addr] = (BYTE)data;
			}
		}
		break;

	case 4: // 65536 colors
		if (addr < 0x80000) {
			line = (((addr & 0x7ffff) >> 10) - state_.crtc.grphscrolly[0]) & 511;
			state_.gvram.gvram[addr ^ 1] = (BYTE)data;
		}
		break;
	}

	state_.tvram.textdirtyline[line] = 1;
}

void FASTCALL Px68kVideoEngine::GVRAM_FastClear()
{
	TVRAMSetAllDirty();

	DWORD v = ((state_.crtc.regs[0x29] & 4) ? 512 : 256);
	DWORD h = ((state_.crtc.regs[0x29] & 3) ? 512 : 256);

	WORD *p;
	DWORD x, y, offset;

	DWORD w[2];

	w[0] = h;
	w[1] = 0;

	if (((state_.crtc.grphscrollx[0] & 0x1ff) + w[0]) > 512) {
		w[1] = (state_.crtc.grphscrollx[0] & 0x1ff) + w[0] - 512;
		w[0] = 512 - (state_.crtc.grphscrollx[0] & 0x1ff);
	}

	for (y = 0; y < v; y++) {
		offset = ((y + state_.crtc.grphscrolly[0]) & 0x1ff) << 10;
		p = (WORD *)(state_.gvram.gvram + offset + ((state_.crtc.grphscrollx[0] & 0x1ff) * 2));

		for (x = 0; x < w[0]; x++) {
			*p++ &= state_.crtc.fastclrmask;
		}

		if (w[1] > 0) {
			p = (WORD *)(state_.gvram.gvram + offset);
			for (x = 0; x < w[1]; x++) {
				*p++ &= state_.crtc.fastclrmask;
			}
		}
	}
}

//===========================================================================
//
//	TVRAM Functions
//
//===========================================================================

BYTE FASTCALL Px68kVideoEngine::TVRAM_Read(DWORD addr)
{
	addr &= 0x7ffff;
#ifndef MSB_FIRST
	addr ^= 1;
#endif
	return state_.tvram.tvram[addr];
}

void FASTCALL Px68kVideoEngine::TVRAM_Write(DWORD addr, BYTE data)
{
	addr &= 0x7ffff;
#ifndef MSB_FIRST
	addr ^= 1;
#endif

	if (state_.crtc.regs[0x2a] & 1) {		// Concurrent access
		DWORD tadr = addr & 0x1ffff;
		if (state_.crtc.regs[0x2a] & 2) {	// Text Mask
			BYTE mask;
			if (state_.crtc.regs[0x2b] & 0x10) {
				mask = (state_.tvram.tvram[tadr] & state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]) | (data & (~state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]));
				if (state_.tvram.tvram[tadr] != mask) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr] = mask;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x20) {
				mask = (state_.tvram.tvram[tadr + 0x20000] & state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]) | (data & (~state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]));
				if (state_.tvram.tvram[tadr + 0x20000] != mask) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x20000] = mask;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x40) {
				mask = (state_.tvram.tvram[tadr + 0x40000] & state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]) | (data & (~state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]));
				if (state_.tvram.tvram[tadr + 0x40000] != mask) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x40000] = mask;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x80) {
				mask = (state_.tvram.tvram[tadr + 0x60000] & state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]) | (data & (~state_.crtc.regs[0x2e + ((tadr ^ 1) & 1)]));
				if (state_.tvram.tvram[tadr + 0x60000] != mask) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x60000] = mask;
				}
			}
		}
		else {
			if (state_.crtc.regs[0x2b] & 0x10) {
				if (state_.tvram.tvram[tadr] != data) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr] = data;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x20) {
				if (state_.tvram.tvram[tadr + 0x20000] != data) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x20000] = data;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x40) {
				if (state_.tvram.tvram[tadr + 0x40000] != data) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x40000] = data;
				}
			}
			if (state_.crtc.regs[0x2b] & 0x80) {
				if (state_.tvram.tvram[tadr + 0x60000] != data) {
					state_.tvram.textdirtyline[(((tadr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
					state_.tvram.tvram[tadr + 0x60000] = data;
				}
			}
		}
	}
	else {						// single access
		if (state_.crtc.regs[0x2a] & 2) {	// Text Mask
			BYTE mask = (state_.tvram.tvram[addr] & state_.crtc.regs[0x2e + ((addr ^ 1) & 1)]) | (data & (~state_.crtc.regs[0x2e + ((addr ^ 1) & 1)]));
			if (state_.tvram.tvram[addr] != mask) {
				state_.tvram.textdirtyline[(((addr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
				state_.tvram.tvram[addr] = mask;
			}
		}
		else {
			if (state_.tvram.tvram[addr] != data) {
				state_.tvram.textdirtyline[(((addr & 0x1ffff) / 128) - state_.crtc.textscrolly) & 1023] = 1;
				state_.tvram.tvram[addr] = data;
			}
		}
	}

	// Update TextDrawWork
	{
		DWORD *ptr = (DWORD *)state_.tvram.textdrawpattern;
		DWORD tvram_addr = addr & 0x1ffff;
		DWORD workadr = ((addr & 0x1ff80) + ((addr ^ 1) & 0x7f)) << 3;
		BYTE pat = state_.tvram.tvram[tvram_addr + 0x60000];
		DWORD t0 = ptr[(pat * 2) + 1536];
		DWORD t1 = ptr[(pat * 2 + 1) + 1536];

		pat = state_.tvram.tvram[tvram_addr + 0x40000];
		t0 |= ptr[(pat * 2) + 1024];
		t1 |= ptr[(pat * 2 + 1) + 1024];

		pat = state_.tvram.tvram[tvram_addr + 0x20000];
		t0 |= ptr[(pat * 2) + 512];
		t1 |= ptr[(pat * 2 + 1) + 512];

		pat = state_.tvram.tvram[tvram_addr];
		t0 |= ptr[(pat * 2)];
		t1 |= ptr[(pat * 2 + 1)];

		*((DWORD *)&state_.tvram.textdrawwork[workadr]) = t0;
		*(((DWORD *)(&state_.tvram.textdrawwork[workadr])) + 1) = t1;
	}
}

void FASTCALL Px68kVideoEngine::TVRAMSetAllDirty()
{
	std::memset(state_.tvram.textdirtyline, 1, 1024);
}

void FASTCALL Px68kVideoEngine::TVRAMRCUpdate()
{
	DWORD adr = ((DWORD)state_.crtc.regs[0x2d] << 9);

	DWORD *ptr = (DWORD *)state_.tvram.textdrawpattern;
	DWORD *wptr = (DWORD *)(state_.tvram.textdrawwork + (adr << 3));
	DWORD t0, t1;
	DWORD tadr;
	BYTE pat;
	int i;

	for (i = 0; i < 512; i++, adr++) {
		tadr = adr ^ 1;

		pat = state_.tvram.tvram[tadr + 0x60000];
		t0 = ptr[(pat * 2) + 1536];
		t1 = ptr[(pat * 2 + 1) + 1536];

		pat = state_.tvram.tvram[tadr + 0x40000];
		t0 |= ptr[(pat * 2) + 1024];
		t1 |= ptr[(pat * 2 + 1) + 1024];

		pat = state_.tvram.tvram[tadr + 0x20000];
		t0 |= ptr[(pat * 2) + 512];
		t1 |= ptr[(pat * 2 + 1) + 512];

		pat = state_.tvram.tvram[tadr];
		t0 |= ptr[(pat * 2)];
		t1 |= ptr[(pat * 2 + 1)];

		*wptr++ = t0;
		*wptr++ = t1;
	}
}

//===========================================================================
//
//	BG/Sprite Functions
//
//===========================================================================

BYTE FASTCALL Px68kVideoEngine::BG_Read(DWORD addr)
{
	if ((addr >= 0xeb0000) && (addr < 0xeb0400)) {
		addr -= 0xeb0000;
#ifndef MSB_FIRST
		addr ^= 1;
#endif
		return state_.bgsprite.sprite_regs[addr];
	}
	else if ((addr >= 0xeb0800) && (addr < 0xeb0812)) {
		return state_.bgsprite.bg_regs[addr - 0xeb0800];
	}
	else if ((addr >= 0xeb8000) && (addr < 0xec0000)) {
		return state_.bgsprite.bg[addr - 0xeb8000];
	}
	return 0xff;
}

void FASTCALL Px68kVideoEngine::BG_Write(DWORD addr, BYTE data)
{
	DWORD bg16chr;
	int v = 0;
	int s1 = (((state_.bgsprite.bg_regs[0x11] & 4) ? 2 : 1) - ((state_.bgsprite.bg_regs[0x11] & 16) ? 1 : 0));
	int s2 = (((state_.crtc.regs[0x29] & 4) ? 2 : 1) - ((state_.crtc.regs[0x29] & 16) ? 1 : 0));
	if (!(state_.bgsprite.bg_regs[0x11] & 16)) v = ((state_.bgsprite.bg_regs[0x0f] >> s1) - (state_.crtc.regs[0x0d] >> s2));

	if ((addr >= 0xeb0000) && (addr < 0xeb0400)) {
		addr &= 0x3ff;
#ifndef MSB_FIRST
		addr ^= 1;
#endif
		if (state_.bgsprite.sprite_regs[addr] != data) {
			WORD t0, t;
			WORD *pw;

			v = state_.bgsprite.bg_vline - 16 - v;
			pw = (WORD *)(state_.bgsprite.sprite_regs + (addr & 0x3f8) + 2);

			t = t0 = (*pw + v) & 0x3ff;
			for (int i = 0; i < 16; i++) {
				state_.tvram.textdirtyline[t] = 1;
				t = (t + 1) & 0x3ff;
			}

			state_.bgsprite.sprite_regs[addr] = data;

			t = (*pw + v) & 0x3ff;
			if (t != t0) {
				for (int i = 0; i < 16; i++) {
					state_.tvram.textdirtyline[t] = 1;
					t = (t + 1) & 0x3ff;
				}
			}
		}
	}
	else if ((addr >= 0xeb0800) && (addr < 0xeb0812)) {
		addr -= 0xeb0800;
		if (state_.bgsprite.bg_regs[addr] == data) return;
		state_.bgsprite.bg_regs[addr] = data;

		switch (addr) {
		case 0x00:
		case 0x01:
			state_.bgsprite.bg0scrollx = (((DWORD)state_.bgsprite.bg_regs[0x00] << 8) + state_.bgsprite.bg_regs[0x01]) & state_.bgsprite.bg_adrmask;
			TVRAMSetAllDirty();
			break;
		case 0x02:
		case 0x03:
			state_.bgsprite.bg0scrolly = (((DWORD)state_.bgsprite.bg_regs[0x02] << 8) + state_.bgsprite.bg_regs[0x03]) & state_.bgsprite.bg_adrmask;
			TVRAMSetAllDirty();
			break;
		case 0x04:
		case 0x05:
			state_.bgsprite.bg1scrollx = (((DWORD)state_.bgsprite.bg_regs[0x04] << 8) + state_.bgsprite.bg_regs[0x05]) & state_.bgsprite.bg_adrmask;
			TVRAMSetAllDirty();
			break;
		case 0x06:
		case 0x07:
			state_.bgsprite.bg1scrolly = (((DWORD)state_.bgsprite.bg_regs[0x06] << 8) + state_.bgsprite.bg_regs[0x07]) & state_.bgsprite.bg_adrmask;
			TVRAMSetAllDirty();
			break;

		case 0x08:		// BG On/Off Changed
			TVRAMSetAllDirty();
			break;

		case 0x0d:
			state_.bgsprite.bg_hadjust = ((int)state_.bgsprite.bg_regs[0x0d] - (state_.crtc.hstart + 4)) * 8;
			TVRAMSetAllDirty();
			break;
		case 0x0f:
			state_.bgsprite.bg_vline = ((int)state_.bgsprite.bg_regs[0x0f] - state_.crtc.vstart) / ((state_.bgsprite.bg_regs[0x11] & 4) ? 1 : 2);
			TVRAMSetAllDirty();
			break;

		case 0x11:		// BG ScreenRes Changed
			if (data & 3) {
				if ((state_.bgsprite.bg_bg0top == 0x4000) || (state_.bgsprite.bg_bg1top == 0x4000))
					state_.bgsprite.bg_chrend = 0x4000;
				else if ((state_.bgsprite.bg_bg0top == 0x6000) || (state_.bgsprite.bg_bg1top == 0x6000))
					state_.bgsprite.bg_chrend = 0x6000;
				else
					state_.bgsprite.bg_chrend = 0x8000;
			}
			else
				state_.bgsprite.bg_chrend = 0x2000;
			state_.bgsprite.bg_chrsize = ((data & 3) ? 16 : 8);
			state_.bgsprite.bg_adrmask = ((data & 3) ? 1023 : 511);
			state_.bgsprite.bg_hadjust = ((int)state_.bgsprite.bg_regs[0x0d] - (state_.crtc.hstart + 4)) * 8;
			state_.bgsprite.bg_vline = ((int)state_.bgsprite.bg_regs[0x0f] - state_.crtc.vstart) / ((state_.bgsprite.bg_regs[0x11] & 4) ? 1 : 2);
			break;

		case 0x09:		// BG Plane Cfg Changed
			TVRAMSetAllDirty();
			if (data & 0x08) {
				if (data & 0x30) {
					state_.bgsprite.bg_bg1top = 0x6000;
					state_.bgsprite.bg_bg1end = 0x8000;
				}
				else {
					state_.bgsprite.bg_bg1top = 0x4000;
					state_.bgsprite.bg_bg1end = 0x6000;
				}
			}
			else
				state_.bgsprite.bg_bg1top = state_.bgsprite.bg_bg1end = 0;

			if (data & 0x01) {
				if (data & 0x06) {
					state_.bgsprite.bg_bg0top = 0x6000;
					state_.bgsprite.bg_bg0end = 0x8000;
				}
				else {
					state_.bgsprite.bg_bg0top = 0x4000;
					state_.bgsprite.bg_bg0end = 0x6000;
				}
			}
			else
				state_.bgsprite.bg_bg0top = state_.bgsprite.bg_bg0end = 0;

			if (state_.bgsprite.bg_regs[0x11] & 3) {
				if ((state_.bgsprite.bg_bg0top == 0x4000) || (state_.bgsprite.bg_bg1top == 0x4000))
					state_.bgsprite.bg_chrend = 0x4000;
				else if ((state_.bgsprite.bg_bg0top == 0x6000) || (state_.bgsprite.bg_bg1top == 0x6000))
					state_.bgsprite.bg_chrend = 0x6000;
				else
					state_.bgsprite.bg_chrend = 0x8000;
			}
			break;
		}
	}
	else if ((addr >= 0xeb8000) && (addr < 0xec0000)) {
		addr -= 0xeb8000;
		if (state_.bgsprite.bg[addr] == data) return;
		state_.bgsprite.bg[addr] = data;

		if (addr < 0x2000) {
			state_.bgsprite.bgchr8[addr * 2] = data >> 4;
			state_.bgsprite.bgchr8[addr * 2 + 1] = data & 15;
		}
		bg16chr = ((addr & 3) * 2) + ((addr & 0x3c) * 4) + ((addr & 0x40) >> 3) + ((addr & 0x7f80) * 2);
		state_.bgsprite.bgchr16[bg16chr] = data >> 4;
		state_.bgsprite.bgchr16[bg16chr + 1] = data & 15;

		if (addr < state_.bgsprite.bg_chrend)		// pattern area
			TVRAMSetAllDirty();
		if ((addr >= state_.bgsprite.bg_bg1top) && (addr < state_.bgsprite.bg_bg1end))	// BG1 MAP Area
			TVRAMSetAllDirty();
		if ((addr >= state_.bgsprite.bg_bg0top) && (addr < state_.bgsprite.bg_bg0end))	// BG0 MAP Area
			TVRAMSetAllDirty();
	}
}


//===========================================================================
//
//	Graphic Drawing Functions (Grp_DrawLine*)
//
//===========================================================================

void FASTCALL Px68kVideoEngine::GrpDrawLine16()
{
	WORD *srcp, *destp;
	DWORD x;
	DWORD i;
	WORD v, v0;
	DWORD y = state_.crtc.grphscrolly[0] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y = (y & 0x1ff) << 10;

	x = state_.crtc.grphscrollx[0] & 0x1ff;
	srcp = (WORD *)(state_.gvram.gvram + y + x * 2);
	destp = (WORD *)state_.gvram.grp_linebuf;

	x = (x ^ 0x1ff) + 1;

	v = v0 = 0;
	i = 0;
	if (x < state_.crtc.textdotx) {
		for (; i < x; ++i) {
			v = *srcp++;
			if (v != 0) {
				v0 = (v >> 8) & 0xff;
				v &= 0x00ff;

				v = state_.palette.regs[state_.gvram.pal16adr[v]];
				v |= state_.palette.regs[state_.gvram.pal16adr[v0] + 2] << 8;
				v = state_.palette.pal16[v];
			}
			*destp++ = v;
		}
		srcp -= 0x200;
	}

	for (; i < state_.crtc.textdotx; ++i) {
		v = *srcp++;
		if (v != 0) {
			v0 = (v >> 8) & 0xff;
			v &= 0x00ff;

			v = state_.palette.regs[state_.gvram.pal16adr[v]];
			v |= state_.palette.regs[state_.gvram.pal16adr[v0] + 2] << 8;
			v = state_.palette.pal16[v];
		}
		*destp++ = v;
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine8(int page, int opaq)
{
	WORD *srcp, *destp;
	DWORD x, x0;
	DWORD y, y0;
	DWORD off;
	DWORD i;
	WORD v;

	page &= 1;

	y = state_.crtc.grphscrolly[page * 2] + state_.vline;
	y0 = state_.crtc.grphscrolly[page * 2 + 1] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c) {
		y += state_.vline;
		y0 += state_.vline;
	}
	y = ((y & 0x1ff) << 10) + page;
	y0 = ((y0 & 0x1ff) << 10) + page;

	x = state_.crtc.grphscrollx[page * 2] & 0x1ff;
	x0 = state_.crtc.grphscrollx[page * 2 + 1] & 0x1ff;

	off = y0 + x0 * 2;
	srcp = (WORD *)(state_.gvram.gvram + y + x * 2);
	destp = (WORD *)state_.gvram.grp_linebuf;

	x = (x ^ 0x1ff) + 1;

	v = 0;
	i = 0;

	if (opaq) {
		if (x < state_.crtc.textdotx) {
			for (; i < x; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v = state_.palette.grphpal[(state_.gvram.gvram[off] & 0xf0) | (v & 0x0f)];
				*destp++ = v;

				off += 2;
				if ((off & 0x3fe) == 0x000)
					off -= 0x400;
			}
			srcp -= 0x200;
		}

		for (; i < state_.crtc.textdotx; ++i) {
			v = PX68K_GET_WORD_W8(srcp);
			srcp++;
			v = state_.palette.grphpal[(state_.gvram.gvram[off] & 0xf0) | (v & 0x0f)];
			*destp++ = v;

			off += 2;
			if ((off & 0x3fe) == 0x000)
				off -= 0x400;
		}
	}
	else {
		if (x < state_.crtc.textdotx) {
			for (; i < x; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v = (state_.gvram.gvram[off] & 0xf0) | (v & 0x0f);
				if (v != 0x00)
					*destp = state_.palette.grphpal[v];
				destp++;

				off += 2;
				if ((off & 0x3fe) == 0x000)
					off -= 0x400;
			}
			srcp -= 0x200;
		}

		for (; i < state_.crtc.textdotx; ++i) {
			v = PX68K_GET_WORD_W8(srcp);
			srcp++;
			v = (state_.gvram.gvram[off] & 0xf0) | (v & 0x0f);
			if (v != 0x00)
				*destp = state_.palette.grphpal[v];
			destp++;

			off += 2;
			if ((off & 0x3fe) == 0x000)
				off -= 0x400;
		}
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine4(DWORD page, int opaq)
{
	WORD *srcp, *destp;
	DWORD x, y;
	DWORD off;
	DWORD i;
	WORD v;

	page &= 3;

	y = state_.crtc.grphscrolly[page] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y = (y & 0x1ff) << 10;

	x = state_.crtc.grphscrollx[page] & 0x1ff;
	off = y + x * 2;

	x ^= 0x1ff;

	srcp = (WORD *)(state_.gvram.gvram + off + (page >> 1));
	destp = (WORD *)state_.gvram.grp_linebuf;

	v = 0;
	i = 0;

	if (page & 1) {
		if (opaq) {
			if (x < state_.crtc.textdotx) {
				for (; i < x; ++i) {
					v = PX68K_GET_WORD_W8(srcp);
					srcp++;
					v = state_.palette.grphpal[(v >> 4) & 0xf];
					*destp++ = v;
				}
				srcp -= 0x200;
			}
			for (; i < state_.crtc.textdotx; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v = state_.palette.grphpal[(v >> 4) & 0xf];
				*destp++ = v;
			}
		}
		else {
			if (x < state_.crtc.textdotx) {
				for (; i < x; ++i) {
					v = PX68K_GET_WORD_W8(srcp);
					srcp++;
					v = (v >> 4) & 0x0f;
					if (v != 0x00)
						*destp = state_.palette.grphpal[v];
					destp++;
				}
				srcp -= 0x200;
			}
			for (; i < state_.crtc.textdotx; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v = (v >> 4) & 0x0f;
				if (v != 0x00)
					*destp = state_.palette.grphpal[v];
				destp++;
			}
		}
	}
	else {
		if (opaq) {
			if (x < state_.crtc.textdotx) {
				for (; i < x; ++i) {
					v = PX68K_GET_WORD_W8(srcp);
					srcp++;
					v = state_.palette.grphpal[v & 0x0f];
					*destp++ = v;
				}
				srcp -= 0x200;
			}
			for (; i < state_.crtc.textdotx; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v = state_.palette.grphpal[v & 0x0f];
				*destp++ = v;
			}
		}
		else {
			if (x < state_.crtc.textdotx) {
				for (; i < x; ++i) {
					v = PX68K_GET_WORD_W8(srcp);
					srcp++;
					v &= 0x0f;
					if (v != 0x00)
						*destp = state_.palette.grphpal[v];
					destp++;
				}
				srcp -= 0x200;
			}
			for (; i < state_.crtc.textdotx; ++i) {
				v = PX68K_GET_WORD_W8(srcp);
				srcp++;
				v &= 0x0f;
				if (v != 0x00)
					*destp = state_.palette.grphpal[v];
				destp++;
			}
		}
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine4h()
{
	WORD *srcp, *destp;
	DWORD x, y;
	DWORD i;
	WORD v;
	int bits;

	y = state_.crtc.grphscrolly[0] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y &= 0x3ff;

	if ((y & 0x200) == 0x000) {
		y <<= 10;
		bits = (state_.crtc.grphscrollx[0] & 0x200) ? 4 : 0;
	}
	else {
		y = (y & 0x1ff) << 10;
		bits = (state_.crtc.grphscrollx[0] & 0x200) ? 12 : 8;
	}

	x = state_.crtc.grphscrollx[0] & 0x1ff;
	srcp = (WORD *)(state_.gvram.gvram + y + x * 2);
	destp = (WORD *)state_.gvram.grp_linebuf;

	x = ((x & 0x1ff) ^ 0x1ff) + 1;

	for (i = 0; i < state_.crtc.textdotx; ++i) {
		v = *srcp++;
		*destp++ = state_.palette.grphpal[(v >> bits) & 0x0f];

		if (--x == 0) {
			srcp -= 0x200;
			bits ^= 4;
			x = 512;
		}
	}
}

//===========================================================================
//
//	Special Priority Drawing Functions (Grp_DrawLine*SP)
//
//===========================================================================

void FASTCALL Px68kVideoEngine::GrpDrawLine16SP()
{
	DWORD x, y;
	DWORD off;
	DWORD i;
	WORD v;

	y = state_.crtc.grphscrolly[0] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y = (y & 0x1ff) << 10;

	x = state_.crtc.grphscrollx[0] & 0x1ff;
	off = y + x * 2;
	x = (x ^ 0x1ff) + 1;

	for (i = 0; i < state_.crtc.textdotx; ++i) {
		v = (state_.palette.regs[state_.gvram.gvram[off + 1] * 2] << 8) | state_.palette.regs[state_.gvram.gvram[off] * 2 + 1];
		if ((state_.gvram.gvram[off] & 1) == 0) {
			state_.gvram.grp_linebufsp[i] = 0;
			state_.gvram.grp_linebufsp2[i] = state_.palette.pal16[v & 0xfffe];
		}
		else {
			state_.gvram.grp_linebufsp[i] = state_.palette.pal16[v & 0xfffe];
			state_.gvram.grp_linebufsp2[i] = 0;
		}

		off += 2;
		if (--x == 0)
			off -= 0x400;
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine8SP(int page)
{
	DWORD x, x0;
	DWORD y, y0;
	DWORD off, off0;
	DWORD i;
	WORD v;

	page &= 1;

	y = state_.crtc.grphscrolly[page * 2] + state_.vline;
	y0 = state_.crtc.grphscrolly[page * 2 + 1] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c) {
		y += state_.vline;
		y0 += state_.vline;
	}
	y = (y & 0x1ff) << 10;
	y0 = (y0 & 0x1ff) << 10;

	x = state_.crtc.grphscrollx[page * 2] & 0x1ff;
	x0 = state_.crtc.grphscrollx[page * 2 + 1] & 0x1ff;

	off = y + x * 2 + page;
	off0 = y0 + x0 * 2 + page;

	x = (x ^ 0x1ff) + 1;

	for (i = 0; i < state_.crtc.textdotx; ++i) {
		v = (state_.gvram.gvram[off] & 0x0f) | (state_.gvram.gvram[off0] & 0xf0);
		state_.gvram.grp_linebufsp_tr[i] = 0;

		if ((v & 1) == 0) {
			v &= 0xfe;
			if (v != 0x00) {
				v = state_.palette.grphpal[v];
				if (!v)
					state_.gvram.grp_linebufsp_tr[i] = 0x1234;
			}

			state_.gvram.grp_linebufsp[i] = 0;
			state_.gvram.grp_linebufsp2[i] = v;
		}
		else {
			v &= 0xfe;
			if (v != 0x00)
				v = state_.palette.grphpal[v] | state_.palette.ibit;
			state_.gvram.grp_linebufsp[i] = v;
			state_.gvram.grp_linebufsp2[i] = 0;
		}

		off += 2;
		off0 += 2;
		if ((off0 & 0x3fe) == 0)
			off0 -= 0x400;
		if (--x == 0)
			off -= 0x400;
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine4SP(DWORD page)
{
	DWORD x, y;
	DWORD off;
	DWORD i;
	WORD v;
	DWORD scrx, scry;

	page &= 3;
	switch (page) {
	case 0:
		scrx = state_.crtc.grphscrollx[0];
		scry = state_.crtc.grphscrolly[0];
		break;
	case 1:
		scrx = state_.crtc.grphscrollx[1];
		scry = state_.crtc.grphscrolly[1];
		break;
	case 2:
		scrx = state_.crtc.grphscrollx[2];
		scry = state_.crtc.grphscrolly[2];
		break;
	case 3:
	default:
		scrx = state_.crtc.grphscrollx[3];
		scry = state_.crtc.grphscrolly[3];
		break;
	}

	if (page & 1) {
		y = scry + state_.vline;
		if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
			y += state_.vline;
		y = (y & 0x1ff) << 10;

		x = scrx & 0x1ff;
		off = y + x * 2;
		if (page & 2)
			off++;
		x = (x ^ 0x1ff) + 1;

		for (i = 0; i < state_.crtc.textdotx; ++i) {
			v = state_.gvram.gvram[off] >> 4;
			if ((v & 1) == 0) {
				v &= 0x0e;
				state_.gvram.grp_linebufsp[i] = 0;
				state_.gvram.grp_linebufsp2[i] = state_.palette.grphpal[v];
			}
			else {
				v &= 0x0e;
				state_.gvram.grp_linebufsp[i] = state_.palette.grphpal[v];
				state_.gvram.grp_linebufsp2[i] = 0;
			}

			off += 2;
			if (--x == 0)
				off -= 0x400;
		}
	}
	else {
		y = scry + state_.vline;
		if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
			y += state_.vline;
		y = (y & 0x1ff) << 10;

		x = scrx & 0x1ff;
		off = y + x * 2;
		if (page & 2)
			off++;
		x = (x ^ 0x1ff) + 1;

		for (i = 0; i < state_.crtc.textdotx; ++i) {
			v = state_.gvram.gvram[off];
			if ((v & 1) == 0) {
				v &= 0x0e;
				state_.gvram.grp_linebufsp[i] = 0;
				state_.gvram.grp_linebufsp2[i] = state_.palette.grphpal[v];
			}
			else {
				v &= 0x0e;
				state_.gvram.grp_linebufsp[i] = state_.palette.grphpal[v];
				state_.gvram.grp_linebufsp2[i] = 0;
			}

			off += 2;
			if (--x == 0)
				off -= 0x400;
		}
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine4hSP()
{
	WORD *srcp;
	DWORD x;
	DWORD i;
	int bits;
	WORD v;
	DWORD y = state_.crtc.grphscrolly[0] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y &= 0x3ff;

	if ((y & 0x200) == 0x000) {
		y <<= 10;
		bits = (state_.crtc.grphscrollx[0] & 0x200) ? 4 : 0;
	}
	else {
		y = (y & 0x1ff) << 10;
		bits = (state_.crtc.grphscrollx[0] & 0x200) ? 12 : 8;
	}

	x = state_.crtc.grphscrollx[0] & 0x1ff;
	srcp = (WORD *)(state_.gvram.gvram + y + x * 2);
	x = ((x & 0x1ff) ^ 0x1ff) + 1;

	for (i = 0; i < state_.crtc.textdotx; ++i) {
		v = *srcp++ >> bits;
		if ((v & 1) == 0) {
			state_.gvram.grp_linebufsp[i] = 0;
			state_.gvram.grp_linebufsp2[i] = state_.palette.grphpal[v & 0x0e];
		}
		else {
			state_.gvram.grp_linebufsp[i] = state_.palette.grphpal[v & 0x0e];
			state_.gvram.grp_linebufsp2[i] = 0;
		}

		if (--x == 0)
			srcp -= 0x400;
	}
}

//===========================================================================
//
//	Transparency Drawing Functions (Grp_DrawLine*TR)
//
//===========================================================================

void FASTCALL Px68kVideoEngine::GrpDrawLine8TR(int page, int opaq)
{
	if (opaq) {
		DWORD x, y;
		DWORD v, v0;
		DWORD i;

		page &= 1;

		y = state_.crtc.grphscrolly[page * 2] + state_.vline;
		if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
			y += state_.vline;
		y = ((y & 0x1ff) << 10) + page;
		x = state_.crtc.grphscrollx[page * 2] & 0x1ff;

		for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
			v0 = state_.gvram.grp_linebufsp[i];
			v = state_.gvram.gvram[y + x * 2];

			if (v0 != 0) {
				if (v != 0) {
					v = state_.palette.grphpal[v];
					if (v != 0) {
						v0 &= state_.palette.halfmask;
						if (v & state_.palette.ibit)
							v0 |= state_.palette.ix2;
						v &= state_.palette.halfmask;
						v += v0;
						v >>= 1;
					}
				}
			}
			else
				v = state_.palette.grphpal[v];
			state_.gvram.grp_linebuf[i] = (WORD)v;
		}
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine8TR_GT(int page, int opaq)
{
	if (opaq) {
		DWORD x, y;
		DWORD i;

		page &= 1;

		y = state_.crtc.grphscrolly[page * 2] + state_.vline;
		if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
			y += state_.vline;
		y = ((y & 0x1ff) << 10) + page;
		x = state_.crtc.grphscrollx[page * 2] & 0x1ff;

		for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
			state_.gvram.grp_linebuf[i] = (state_.gvram.grp_linebufsp[i] || state_.gvram.grp_linebufsp_tr[i]) ? 0 : state_.palette.grphpal[state_.gvram.gvram[y + x * 2]];
			state_.gvram.grp_linebufsp_tr[i] = 0;
		}
	}
}

void FASTCALL Px68kVideoEngine::GrpDrawLine4TR(DWORD page, int opaq)
{
	DWORD x, y;
	DWORD v, v0;
	DWORD i;

	page &= 3;

	y = state_.crtc.grphscrolly[page] + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y = (y & 0x1ff) << 10;
	x = state_.crtc.grphscrollx[page] & 0x1ff;

	if (page & 1) {
		page >>= 1;
		y += page;

		if (opaq) {
			for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
				v0 = state_.gvram.grp_linebufsp[i];
				v = state_.gvram.gvram[y + x * 2] >> 4;

				if (v0 != 0) {
					if (v != 0) {
						v = state_.palette.grphpal[v];
						if (v != 0) {
							v0 &= state_.palette.halfmask;
							if (v & state_.palette.ibit)
								v0 |= state_.palette.ix2;
							v &= state_.palette.halfmask;
							v += v0;
							v >>= 1;
						}
					}
				}
				else
					v = state_.palette.grphpal[v];
				state_.gvram.grp_linebuf[i] = (WORD)v;
			}
		}
		else {
			for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
				v0 = state_.gvram.grp_linebufsp[i];

				if (v0 == 0) {
					v = state_.gvram.gvram[y + x * 2] >> 4;
					if (v != 0)
						state_.gvram.grp_linebuf[i] = state_.palette.grphpal[v];
				}
				else {
					v = state_.gvram.gvram[y + x * 2] >> 4;
					if (v != 0) {
						v = state_.palette.grphpal[v];
						if (v != 0) {
							v0 &= state_.palette.halfmask;
							if (v & state_.palette.ibit)
								v0 |= state_.palette.ix2;
							v &= state_.palette.halfmask;
							v += v0;
							v = state_.palette.grphpal[v >> 1];
							state_.gvram.grp_linebuf[i] = (WORD)v;
						}
					}
					else
						state_.gvram.grp_linebuf[i] = (WORD)v;
				}
			}
		}
	}
	else {
		page >>= 1;
		y += page;

		if (opaq) {
			for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
				v = state_.gvram.gvram[y + x * 2] & 0x0f;
				v0 = state_.gvram.grp_linebufsp[i];

				if (v0 != 0) {
					if (v != 0) {
						v = state_.palette.grphpal[v];
						if (v != 0) {
							v0 &= state_.palette.halfmask;
							if (v & state_.palette.ibit)
								v0 |= state_.palette.ix2;
							v &= state_.palette.halfmask;
							v += v0;
							v >>= 1;
						}
					}
				}
				else
					v = state_.palette.grphpal[v];
				state_.gvram.grp_linebuf[i] = (WORD)v;
			}
		}
		else {
			for (i = 0; i < state_.crtc.textdotx; ++i, x = (x + 1) & 0x1ff) {
				v = state_.gvram.gvram[y + x * 2] & 0x0f;
				v0 = state_.gvram.grp_linebufsp[i];

				if (v0 != 0) {
					if (v != 0) {
						v = state_.palette.grphpal[v];
						if (v != 0) {
							v0 &= state_.palette.halfmask;
							if (v & state_.palette.ibit)
								v0 |= state_.palette.ix2;
							v &= state_.palette.halfmask;
							v += v0;
							v >>= 1;
							state_.gvram.grp_linebuf[i] = (WORD)v;
						}
					}
					else
						state_.gvram.grp_linebuf[i] = (WORD)v;
				}
				else if (v != 0)
					state_.gvram.grp_linebuf[i] = state_.palette.grphpal[v];
			}
		}
	}
}


//===========================================================================
//
//	Text Drawing Function
//
//===========================================================================

void FASTCALL Px68kVideoEngine::TextDrawLine(int opaq)
{
	DWORD addr;
	DWORD x;
	DWORD off = 16;
	DWORD i;
	BYTE t;
	DWORD y = state_.crtc.textscrolly + state_.vline;
	if ((state_.crtc.regs[0x29] & 0x1c) == 0x1c)
		y += state_.vline;
	y = (y & 0x3ff) << 10;

	x = state_.crtc.textscrollx & 0x3ff;
	addr = x + y;
	x = (x ^ 0x3ff) + 1;

	if (opaq) {
		for (i = 0; (i < state_.crtc.textdotx) && (x > 0); i++, x--, off++) {
			t = state_.tvram.textdrawwork[addr++] & 0xf;
			state_.tvram.text_trflag[off] = t ? 1 : 0;
			state_.bgsprite.bg_linebuf[off] = state_.palette.textpal[t];
		}
		if (i++ != state_.crtc.textdotx) {
			for (; i < state_.crtc.textdotx; i++, off++) {
				state_.bgsprite.bg_linebuf[off] = state_.palette.textpal[0];
				state_.tvram.text_trflag[off] = 0;
			}
		}
	}
	else {
		for (i = 0; (i < state_.crtc.textdotx) && (x > 0); i++, x--, off++) {
			t = state_.tvram.textdrawwork[addr++] & 0xf;
			if (t) {
				state_.tvram.text_trflag[off] |= 1;
				state_.bgsprite.bg_linebuf[off] = state_.palette.textpal[t];
			}
		}
	}
}

//===========================================================================
//
//	Sprite Drawing Function
//
//===========================================================================

void FASTCALL Px68kVideoEngine::Sprite_DrawLineMcr(int pri)
{
	// Sprite control table structure
	// Using #pragma pack for MSVC compatibility instead of __attribute__((packed))
#pragma pack(push, 1)
	struct SpriteCtrlTbl {
		WORD sprite_posx;
		WORD sprite_posy;
		WORD sprite_ctrl;
		BYTE sprite_ply;
		BYTE dummy;
	};
#pragma pack(pop)

	SpriteCtrlTbl *sct = (SpriteCtrlTbl *)state_.bgsprite.sprite_regs;
	DWORD y;
	DWORD t;
	int n;

	for (n = 127; n >= 0; --n) {
		if ((sct[n].sprite_ply & 3) == pri) {
			SpriteCtrlTbl *sctp = &sct[n];
			int calc;

			t = (sctp->sprite_posx + state_.bgsprite.bg_hadjust) & 0x3ff;
			if (t >= state_.crtc.textdotx + 16)
				continue;

			y = sctp->sprite_posy & 0x3ff;
			y -= state_.bgsprite.vlinebg;
			y += state_.bgsprite.bg_vline;
			calc = (int)y;
			y = -y;
			y += 16;

			// Check if sprite is visible on this line
			if (y <= 15) {
				BYTE *p;
				DWORD pal;
				int i, d;

				if (sctp->sprite_ctrl < 0x4000) {
					p = &state_.bgsprite.bgchr16[((sctp->sprite_ctrl * 256) & 0xffff) + (y * 16)];
					d = 1;
				}
				else if ((sctp->sprite_ctrl - 0x4000) & 0x8000) {
					p = &state_.bgsprite.bgchr16[((sctp->sprite_ctrl * 256) & 0xffff) + (((y * 16) & 0xff) ^ 0xf0) + 15];
					d = -1;
				}
				else if ((short)(sctp->sprite_ctrl) >= 0x4000) {
					p = &state_.bgsprite.bgchr16[((sctp->sprite_ctrl * 256) & 0xffff) + (y * 16) + 15];
					d = -1;
				}
				else {
					p = &state_.bgsprite.bgchr16[((sctp->sprite_ctrl << 8) & 0xffff) + (((y * 16) & 0xff) ^ 0xf0)];
					d = 1;
				}

				for (i = 0; i < 16; i++, t++, p += d) {
					pal = *p & 0xf;
					if (pal) {
						pal |= (sctp->sprite_ctrl >> 4) & 0xf0;
						if (state_.bgsprite.bg_pribuf[t] >= n * 8) {
							state_.bgsprite.bg_linebuf[t] = state_.palette.textpal[pal];
							state_.tvram.text_trflag[t] |= 2;
							state_.bgsprite.bg_pribuf[t] = n * 8;
						}
					}
				}
			}
		}
	}
}

//===========================================================================
//
//	BG Drawing Functions
//
//===========================================================================

// Macro-style helper for BG drawing loops
#define BG_DRAWLINE_LOOPY(cnt) \
{ \
	bl = bl << 4; \
	for (j = 0; j < cnt; j++, esi += d, edi++) { \
		dat = *esi | bl; \
		if (dat == 0) \
			continue; \
		if ((dat & 0xf) || !(state_.tvram.text_trflag[edi + 1] & 2)) { \
			state_.bgsprite.bg_linebuf[1 + edi] = state_.palette.textpal[dat]; \
			state_.tvram.text_trflag[edi + 1] |= 2; \
		} \
	} \
}

#define BG_DRAWLINE_LOOPY_NG(cnt) \
{ \
	bl = bl << 4; \
	for (j = 0; j < cnt; j++, esi += d, edi++) { \
		dat = *esi & 0xf; \
		if (dat) { \
			dat |= bl; \
			state_.bgsprite.bg_linebuf[1 + edi] = state_.palette.textpal[dat]; \
			state_.tvram.text_trflag[edi + 1] |= 2; \
		} \
	} \
}

void FASTCALL Px68kVideoEngine::BGDrawLineMcr8(WORD BGTOP, DWORD BGScrollX, DWORD BGScrollY)
{
	BYTE dat, bl;
	int i, j, d;
	WORD si;
	BYTE *esi;
	DWORD ebp = ((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 7) << 3;
	DWORD edx = BGTOP + (((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 0x1f8) << 4);
	DWORD edi = ((BGScrollX - state_.bgsprite.bg_hadjust) & 7) ^ 15;
	DWORD ecx = ((BGScrollX - state_.bgsprite.bg_hadjust) & 0x1f8) >> 2;

	for (i = state_.crtc.textdotx >> 3; i >= 0; i--) {
		bl = state_.bgsprite.bg[ecx + edx];
		si = (WORD)state_.bgsprite.bg[ecx + edx + 1] << 6;

		if (bl < 0x40) {
			esi = &state_.bgsprite.bgchr8[si + ebp];
			d = +1;
		}
		else if ((bl - 0x40) & 0x80) {
			esi = &state_.bgsprite.bgchr8[si + 0x3f - ebp];
			d = -1;
		}
		else if ((signed char)bl >= 0x40) {
			esi = &state_.bgsprite.bgchr8[si + ebp + 7];
			d = -1;
		}
		else {
			esi = &state_.bgsprite.bgchr8[si + 0x38 - ebp];
			d = +1;
		}
		BG_DRAWLINE_LOOPY(8);
		ecx += 2;
		ecx &= 0x7f;
	}
}

void FASTCALL Px68kVideoEngine::BGDrawLineMcr16(WORD BGTOP, DWORD BGScrollX, DWORD BGScrollY)
{
	WORD si;
	BYTE dat, bl;
	int i, j, d;
	BYTE *esi;
	DWORD ebp = ((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 15) << 4;
	DWORD edx = BGTOP + (((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 0x3f0) << 3);
	DWORD edi = ((BGScrollX - state_.bgsprite.bg_hadjust) & 15) ^ 15;
	DWORD ecx = ((BGScrollX - state_.bgsprite.bg_hadjust) & 0x3f0) >> 3;

	for (i = state_.crtc.textdotx >> 4; i >= 0; i--) {
		bl = state_.bgsprite.bg[ecx + edx];
		si = state_.bgsprite.bg[ecx + edx + 1] << 8;

		if (bl < 0x40) {
			esi = &state_.bgsprite.bgchr16[si + ebp];
			d = +1;
		}
		else if ((bl - 0x40) & 0x80) {
			esi = &state_.bgsprite.bgchr16[si + 0xff - ebp];
			d = -1;
		}
		else if ((signed char)bl >= 0x40) {
			esi = &state_.bgsprite.bgchr16[si + ebp + 15];
			d = -1;
		}
		else {
			esi = &state_.bgsprite.bgchr16[si + 0xf0 - ebp];
			d = +1;
		}
		BG_DRAWLINE_LOOPY(16);
		ecx += 2;
		ecx &= 0x7f;
	}
}

void FASTCALL Px68kVideoEngine::BGDrawLineMcr8_ng(WORD BGTOP, DWORD BGScrollX, DWORD BGScrollY)
{
	BYTE dat, bl;
	int i, j, d;
	WORD si;
	BYTE *esi;
	DWORD ebp = ((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 7) << 3;
	DWORD edx = BGTOP + (((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 0x1f8) << 4);
	DWORD edi = ((BGScrollX - state_.bgsprite.bg_hadjust) & 7) ^ 15;
	DWORD ecx = ((BGScrollX - state_.bgsprite.bg_hadjust) & 0x1f8) >> 2;

	for (i = state_.crtc.textdotx >> 3; i >= 0; i--) {
		bl = state_.bgsprite.bg[ecx + edx];
		si = (WORD)state_.bgsprite.bg[ecx + edx + 1] << 6;

		if (bl < 0x40) {
			esi = &state_.bgsprite.bgchr8[si + ebp];
			d = +1;
		}
		else if ((bl - 0x40) & 0x80) {
			esi = &state_.bgsprite.bgchr8[si + 0x3f - ebp];
			d = -1;
		}
		else if ((signed char)bl >= 0x40) {
			esi = &state_.bgsprite.bgchr8[si + ebp + 7];
			d = -1;
		}
		else {
			esi = &state_.bgsprite.bgchr8[si + 0x38 - ebp];
			d = +1;
		}
		BG_DRAWLINE_LOOPY_NG(8);
		ecx += 2;
		ecx &= 0x7f;
	}
}

void FASTCALL Px68kVideoEngine::BGDrawLineMcr16_ng(WORD BGTOP, DWORD BGScrollX, DWORD BGScrollY)
{
	WORD si;
	BYTE dat, bl;
	int i, j, d;
	BYTE *esi;
	DWORD ebp = ((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 15) << 4;
	DWORD edx = BGTOP + (((BGScrollY + state_.bgsprite.vlinebg - state_.bgsprite.bg_vline) & 0x3f0) << 3);
	DWORD edi = ((BGScrollX) & 15) ^ 15;
	DWORD ecx = ((BGScrollX) & 0x3f0) >> 3;

	for (i = state_.crtc.textdotx >> 4; i >= 0; i--) {
		bl = state_.bgsprite.bg[ecx + edx];
		si = state_.bgsprite.bg[ecx + edx + 1] << 8;

		if (bl < 0x40) {
			esi = &state_.bgsprite.bgchr16[si + ebp];
			d = +1;
		}
		else if ((bl - 0x40) & 0x80) {
			esi = &state_.bgsprite.bgchr16[si + 0xff - ebp];
			d = -1;
		}
		else if ((signed char)bl >= 0x40) {
			esi = &state_.bgsprite.bgchr16[si + ebp + 15];
			d = -1;
		}
		else {
			esi = &state_.bgsprite.bgchr16[si + 0xf0 - ebp];
			d = +1;
		}
		BG_DRAWLINE_LOOPY_NG(16);
		ecx += 2;
		ecx &= 0x7f;
	}
}

void FASTCALL Px68kVideoEngine::BG_DrawLine(int opaq, int gd)
{
	int i;

	if (opaq) {
		for (i = 16; i < state_.crtc.textdotx + 16; ++i) {
			state_.bgsprite.bg_linebuf[i] = state_.palette.textpal[0];
			state_.bgsprite.bg_pribuf[i] = 0xffff;
		}
	}
	else {
		for (i = 16; i < state_.crtc.textdotx + 16; ++i)
			state_.bgsprite.bg_pribuf[i] = 0xffff;
	}

	Sprite_DrawLineMcr(1);
	if ((state_.bgsprite.bg_regs[9] & 8) && (state_.bgsprite.bg_chrsize == 8)) { // BG1 on
		if (gd) {
			BGDrawLineMcr8(state_.bgsprite.bg_bg1top, state_.bgsprite.bg1scrollx, state_.bgsprite.bg1scrolly);
		}
		else {
			BGDrawLineMcr8_ng(state_.bgsprite.bg_bg1top, state_.bgsprite.bg1scrollx, state_.bgsprite.bg1scrolly);
		}
	}
	Sprite_DrawLineMcr(2);
	if (state_.bgsprite.bg_regs[9] & 1) { // BG0 on
		if (state_.bgsprite.bg_chrsize == 8) {
			if (gd) {
					BGDrawLineMcr8(state_.bgsprite.bg_bg0top, state_.bgsprite.bg0scrollx, state_.bgsprite.bg0scrolly);
			}
			else {
					BGDrawLineMcr8_ng(state_.bgsprite.bg_bg0top, state_.bgsprite.bg0scrollx, state_.bgsprite.bg0scrolly);
			}
		}
		else {
			if (gd) {
					BGDrawLineMcr16(state_.bgsprite.bg_bg0top, state_.bgsprite.bg0scrollx, state_.bgsprite.bg0scrolly);
			}
			else {
					BGDrawLineMcr16_ng(state_.bgsprite.bg_bg0top, state_.bgsprite.bg0scrollx, state_.bgsprite.bg0scrolly);
			}
		}
	}
	Sprite_DrawLineMcr(3);
}


//===========================================================================
//
//	WinDraw Compositor Functions
//
//===========================================================================

void FASTCALL Px68kVideoEngine::WinDrawDrawGrpLine(int opaq)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		std::memcpy(&screen_buffer_[adr], state_.gvram.grp_linebuf, state_.crtc.textdotx * 2);
	}
	else {
		for (i = 0; i < (int)state_.crtc.textdotx; i++, adr++) {
			w = state_.gvram.grp_linebuf[i];
			if (w != 0)
				screen_buffer_[adr] = w;
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawGrpLineNonSP(int opaq)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		std::memcpy(&screen_buffer_[adr], state_.gvram.grp_linebufsp2, state_.crtc.textdotx * 2);
	}
	else {
		for (i = 0; i < (int)state_.crtc.textdotx; i++, adr++) {
			w = state_.gvram.grp_linebufsp2[i];
			if (w != 0)
				screen_buffer_[adr] = w;
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawTextLine(int opaq, int td)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		std::memcpy(&screen_buffer_[adr], &state_.bgsprite.bg_linebuf[16], state_.crtc.textdotx * 2);
	}
	else {
		if (td) {
			for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
				if (state_.tvram.text_trflag[i] & 1) {
					w = state_.bgsprite.bg_linebuf[i];
					if (w != 0)
						screen_buffer_[adr] = w;
				}
			}
		}
		else {
			for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
				w = state_.bgsprite.bg_linebuf[i];
				if (w != 0)
					screen_buffer_[adr] = w;
			}
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawTextLineTR(int opaq)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	DWORD v;
	WORD w;
	int i;

	if (opaq) {
		for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
			w = state_.gvram.grp_linebufsp[i - 16];
			if (w != 0) {
				w &= state_.palette.halfmask;
				v = state_.bgsprite.bg_linebuf[i];
				if (v & state_.palette.ibit)
					w += state_.palette.ix2;
				v &= state_.palette.halfmask;
				v += w;
				v >>= 1;
			}
			else {
				if (state_.tvram.text_trflag[i] & 1)
					v = state_.bgsprite.bg_linebuf[i];
				else
					v = 0;
			}
			screen_buffer_[adr] = (WORD)v;
		}
	}
	else {
		for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
			if (state_.tvram.text_trflag[i] & 1) {
				w = state_.gvram.grp_linebufsp[i - 16];
				v = state_.bgsprite.bg_linebuf[i];

				if (v != 0) {
					if (w != 0) {
						w &= state_.palette.halfmask;
						if (v & state_.palette.ibit)
							w += state_.palette.ix2;
						v &= state_.palette.halfmask;
						v += w;
						v >>= 1;
					}
					screen_buffer_[adr] = (WORD)v;
				}
			}
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawBGLine(int opaq, int td)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	if (opaq) {
		std::memcpy(&screen_buffer_[adr], &state_.bgsprite.bg_linebuf[16], state_.crtc.textdotx * 2);
	}
	else {
		if (td) {
			for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
				if (state_.tvram.text_trflag[i] & 2) {
					w = state_.bgsprite.bg_linebuf[i];
					if (w != 0)
						screen_buffer_[adr] = w;
				}
			}
		}
		else {
			for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
				w = state_.bgsprite.bg_linebuf[i];
				if (w != 0)
					screen_buffer_[adr] = w;
			}
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawBGLineTR(int opaq)
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	DWORD v;
	WORD w;
	int i;

	if (opaq) {
		for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
			w = state_.gvram.grp_linebufsp[i - 16];
			v = state_.bgsprite.bg_linebuf[i];

			if (w != 0) {
				w &= state_.palette.halfmask;
				if (v & state_.palette.ibit)
					w += state_.palette.ix2;
				v &= state_.palette.halfmask;
				v += w;
				v >>= 1;
			}
			screen_buffer_[adr] = (WORD)v;
		}
	}
	else {
		for (i = 16; i < (int)state_.crtc.textdotx + 16; i++, adr++) {
			if (state_.tvram.text_trflag[i] & 2) {
				w = state_.gvram.grp_linebufsp[i - 16];
				v = state_.bgsprite.bg_linebuf[i];

				if (v != 0) {
					if (w != 0) {
						w &= state_.palette.halfmask;
						if (v & state_.palette.ibit)
							w += state_.palette.ix2;
						v &= state_.palette.halfmask;
						v += w;
						v >>= 1;
					}
					screen_buffer_[adr] = (WORD)v;
				}
			}
		}
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawPriLine()
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	for (i = 0; i < (int)state_.crtc.textdotx; i++, adr++) {
		w = state_.gvram.grp_linebufsp[i];
		if (w != 0)
			screen_buffer_[adr] = w;
	}
}

void FASTCALL Px68kVideoEngine::WinDrawDrawHalfFillLine()
{
	DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
	WORD w;
	int i;

	for (i = 0; i < (int)state_.crtc.textdotx; i++, adr++) {
		w = state_.gvram.grp_linebufsp[i];
		if (w != 0 && screen_buffer_[adr] == 0)
			screen_buffer_[adr] = (w & state_.palette.halfmask) >> 1;
	}
}

//===========================================================================
//
//	Main DrawLine Function (WinDraw_DrawLine equivalent)
//
//===========================================================================

void FASTCALL Px68kVideoEngine::WinDraw_DrawLine()
{
	int opaq, ton = 0, gon = 0, bgon = 0, tron = 0, pron = 0, tdrawed = 0;

	if (state_.vline == (DWORD)-1)
		return;
	if (!state_.tvram.textdirtyline[state_.vline])
		return;

	state_.tvram.textdirtyline[state_.vline] = 0;

	if (state_.debug_grp) {
		switch (state_.vc.vcreg0[1] & 3) {
		case 0:					// 16 colors
			if (state_.vc.vcreg0[1] & 4) {	// 1024dot
				if (state_.vc.vcreg2[1] & 0x10) {
					if ((state_.vc.vcreg2[0] & 0x14) == 0x14) {
						GrpDrawLine4hSP();
						pron = tron = 1;
					}
					else {
						GrpDrawLine4h();
						gon = 1;
					}
				}
			}
			else {				// 512dot
				if ((state_.vc.vcreg2[0] & 0x10) && (state_.vc.vcreg2[1] & 1)) {
					GrpDrawLine4SP((state_.vc.vcreg1[1]) & 3);
					pron = tron = 1;
				}
				opaq = 1;
				if (state_.vc.vcreg2[1] & 8) {
					GrpDrawLine4((state_.vc.vcreg1[1] >> 6) & 3, 1);
					opaq = 0;
					gon = 1;
				}
				if (state_.vc.vcreg2[1] & 4) {
					GrpDrawLine4((state_.vc.vcreg1[1] >> 4) & 3, opaq);
					opaq = 0;
					gon = 1;
				}
				if (state_.vc.vcreg2[1] & 2) {
					if (((state_.vc.vcreg2[0] & 0x1e) == 0x1e) && (tron))
						GrpDrawLine4TR((state_.vc.vcreg1[1] >> 2) & 3, opaq);
					else
						GrpDrawLine4((state_.vc.vcreg1[1] >> 2) & 3, opaq);
					opaq = 0;
					gon = 1;
				}
				if (state_.vc.vcreg2[1] & 1) {
					if ((state_.vc.vcreg2[0] & 0x14) != 0x14) {
						GrpDrawLine4((state_.vc.vcreg1[1]) & 3, opaq);
						gon = 1;
					}
				}
			}
			break;

		case 1:
		case 2:
			opaq = 1; // 256 colors
			if ((state_.vc.vcreg1[1] & 3) <= ((state_.vc.vcreg1[1] >> 4) & 3)) {
				if ((state_.vc.vcreg2[0] & 0x10) && (state_.vc.vcreg2[1] & 1)) {
					GrpDrawLine8SP(0);
					tron = pron = 1;
				}
				if (state_.vc.vcreg2[1] & 4) {
					if (((state_.vc.vcreg2[0] & 0x1e) == 0x1e) && (tron))
						GrpDrawLine8TR(1, 1);
					else if (((state_.vc.vcreg2[0] & 0x1d) == 0x1d) && (tron))
						GrpDrawLine8TR_GT(1, 1);
					else
						GrpDrawLine8(1, 1);
					opaq = 0;
					gon = 1;
				}
				if (state_.vc.vcreg2[1] & 1) {
					if ((state_.vc.vcreg2[0] & 0x14) != 0x14) {
						GrpDrawLine8(0, opaq);
						gon = 1;
					}
				}
			}
			else {
				if ((state_.vc.vcreg2[0] & 0x10) && (state_.vc.vcreg2[1] & 1)) {
					GrpDrawLine8SP(1);
					tron = pron = 1;
				}
				if (state_.vc.vcreg2[1] & 4) {
					if (((state_.vc.vcreg2[0] & 0x1e) == 0x1e) && (tron))
						GrpDrawLine8TR(0, 1);
					else if (((state_.vc.vcreg2[0] & 0x1d) == 0x1d) && (tron))
						GrpDrawLine8TR_GT(0, 1);
					else
						GrpDrawLine8(0, 1);
					opaq = 0;
					gon = 1;
				}
				if (state_.vc.vcreg2[1] & 1) {
					if ((state_.vc.vcreg2[0] & 0x14) != 0x14) {
						GrpDrawLine8(1, opaq);
						gon = 1;
					}
				}
			}
			break;

		case 3:					// 65536 colors
			if (state_.vc.vcreg2[1] & 15) {
				if ((state_.vc.vcreg2[0] & 0x14) == 0x14) {
					GrpDrawLine16SP();
					tron = pron = 1;
				}
				else {
					GrpDrawLine16();
					gon = 1;
				}
			}
			break;
		}
	}

	// BG/Text priority handling
	if (((state_.vc.vcreg1[0] & 0x30) >> 2) < (state_.vc.vcreg1[0] & 0x0c)) {
		// BG has higher priority
		if ((state_.vc.vcreg2[1] & 0x20) && (state_.debug_text)) {
			TextDrawLine(1);
			ton = 1;
		}
		else
			std::memset(state_.tvram.text_trflag, 0, state_.crtc.textdotx + 16);

		if ((state_.vc.vcreg2[1] & 0x40) && (state_.bgsprite.bg_regs[8] & 2) && (!(state_.bgsprite.bg_regs[0x11] & 2)) && (state_.debug_sp)) {
			int s1, s2;
			s1 = (((state_.bgsprite.bg_regs[0x11] & 4) ? 2 : 1) - ((state_.bgsprite.bg_regs[0x11] & 16) ? 1 : 0));
			s2 = (((state_.crtc.regs[0x29] & 4) ? 2 : 1) - ((state_.crtc.regs[0x29] & 16) ? 1 : 0));
			state_.bgsprite.vlinebg = state_.vline;
			state_.bgsprite.vlinebg <<= s1;
			state_.bgsprite.vlinebg >>= s2;
			if (!(state_.bgsprite.bg_regs[0x11] & 16)) state_.bgsprite.vlinebg -= ((state_.bgsprite.bg_regs[0x0f] >> s1) - (state_.crtc.regs[0x0d] >> s2));
			BG_DrawLine(!ton, 0);
			bgon = 1;
		}
	}
	else {
		// Text has higher priority
		if ((state_.vc.vcreg2[1] & 0x40) && (state_.bgsprite.bg_regs[8] & 2) && (!(state_.bgsprite.bg_regs[0x11] & 2)) && (state_.debug_sp)) {
			int s1, s2;
			s1 = (((state_.bgsprite.bg_regs[0x11] & 4) ? 2 : 1) - ((state_.bgsprite.bg_regs[0x11] & 16) ? 1 : 0));
			s2 = (((state_.crtc.regs[0x29] & 4) ? 2 : 1) - ((state_.crtc.regs[0x29] & 16) ? 1 : 0));
			state_.bgsprite.vlinebg = state_.vline;
			state_.bgsprite.vlinebg <<= s1;
			state_.bgsprite.vlinebg >>= s2;
			if (!(state_.bgsprite.bg_regs[0x11] & 16)) state_.bgsprite.vlinebg -= ((state_.bgsprite.bg_regs[0x0f] >> s1) - (state_.crtc.regs[0x0d] >> s2));
			std::memset(state_.tvram.text_trflag, 0, state_.crtc.textdotx + 16);
			BG_DrawLine(1, 1);
			bgon = 1;
		}
		else {
			if ((state_.vc.vcreg2[1] & 0x20) && (state_.debug_text)) {
				for (int i = 16; i < (int)state_.crtc.textdotx + 16; ++i)
					state_.bgsprite.bg_linebuf[i] = state_.palette.textpal[0];
			}
			else {
				std::memset(&state_.bgsprite.bg_linebuf[16], 0, state_.crtc.textdotx * 2);
			}
			std::memset(state_.tvram.text_trflag, 0, state_.crtc.textdotx + 16);
			bgon = 1;
		}

		if ((state_.vc.vcreg2[1] & 0x20) && (state_.debug_text)) {
			TextDrawLine(!bgon);
			ton = 1;
		}
	}

	opaq = 1;

	// Priority 2 or 3 (lowest)
	if (state_.vc.vcreg1[0] & 0x02) {
		if (gon) {
			WinDrawDrawGrpLine(opaq);
			opaq = 0;
		}
		if (tron) {
			WinDrawDrawGrpLineNonSP(opaq);
			opaq = 0;
		}
	}
	if ((state_.vc.vcreg1[0] & 0x20) && (bgon)) {
		if (((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && ((state_.vc.vcreg1[0] & 0x03) != 0x02) && (tron)) {
			if ((state_.vc.vcreg1[0] & 3) < ((state_.vc.vcreg1[0] >> 2) & 3)) {
				WinDrawDrawBGLineTR(opaq);
				tdrawed = 1;
				opaq = 0;
			}
		}
		else {
			WinDrawDrawBGLine(opaq, tdrawed);
			tdrawed = 1;
			opaq = 0;
		}
	}
	if ((state_.vc.vcreg1[0] & 0x08) && (ton)) {
		if (((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && ((state_.vc.vcreg1[0] & 0x03) != 0x02) && (tron))
			WinDrawDrawTextLineTR(opaq);
		else
			WinDrawDrawTextLine(opaq, tdrawed);
		opaq = 0;
		tdrawed = 1;
	}

	// Priority 1 or 2
	if (((state_.vc.vcreg1[0] & 0x03) == 0x01) && (gon)) {
		WinDrawDrawGrpLine(opaq);
		opaq = 0;
	}
	if (((state_.vc.vcreg1[0] & 0x30) == 0x10) && (bgon)) {
		if (((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && (!(state_.vc.vcreg1[0] & 0x03)) && (tron)) {
			if ((state_.vc.vcreg1[0] & 3) < ((state_.vc.vcreg1[0] >> 2) & 3)) {
				WinDrawDrawBGLineTR(opaq);
				tdrawed = 1;
				opaq = 0;
			}
		}
		else {
			WinDrawDrawBGLine(opaq, ((state_.vc.vcreg1[0] & 0xc) == 0x8));
			tdrawed = 1;
			opaq = 0;
		}
	}
	if (((state_.vc.vcreg1[0] & 0x0c) == 0x04) && ((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && (state_.vc.vcreg1[0] & 0x03) && (((state_.vc.vcreg1[0] >> 4) & 3) > (state_.vc.vcreg1[0] & 3)) && (bgon) && (tron)) {
		WinDrawDrawBGLineTR(opaq);
		tdrawed = 1;
		opaq = 0;
		if (tron) {
			WinDrawDrawGrpLineNonSP(opaq);
		}
	}
	else if (((state_.vc.vcreg1[0] & 0x03) == 0x01) && (tron) && (gon) && (state_.vc.vcreg2[0] & 0x10)) {
		WinDrawDrawGrpLineNonSP(opaq);
		opaq = 0;
	}
	if (((state_.vc.vcreg1[0] & 0x0c) == 0x04) && (ton)) {
		if (((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && (!(state_.vc.vcreg1[0] & 0x03)) && (tron))
			WinDrawDrawTextLineTR(opaq);
		else
			WinDrawDrawTextLine(opaq, ((state_.vc.vcreg1[0] & 0x30) >= 0x10));
		opaq = 0;
		tdrawed = 1;
	}

	// Priority 0 (lowest)
	if ((!(state_.vc.vcreg1[0] & 0x03)) && (gon)) {
		WinDrawDrawGrpLine(opaq);
		opaq = 0;
	}
	if ((!(state_.vc.vcreg1[0] & 0x30)) && (bgon)) {
		WinDrawDrawBGLine(opaq, ((state_.vc.vcreg1[0] & 0xc) >= 0x4));
		tdrawed = 1;
		opaq = 0;
	}
	if ((!(state_.vc.vcreg1[0] & 0x0c)) && ((state_.vc.vcreg2[0] & 0x5d) == 0x1d) && (((state_.vc.vcreg1[0] >> 4) & 3) > (state_.vc.vcreg1[0] & 3)) && (bgon) && (tron)) {
		WinDrawDrawBGLineTR(opaq);
		tdrawed = 1;
		opaq = 0;
		if (tron) {
			WinDrawDrawGrpLineNonSP(opaq);
		}
	}
	else if ((!(state_.vc.vcreg1[0] & 0x03)) && (tron) && (state_.vc.vcreg2[0] & 0x10)) {
		WinDrawDrawGrpLineNonSP(opaq);
		opaq = 0;
	}
	if ((!(state_.vc.vcreg1[0] & 0x0c)) && (ton)) {
		WinDrawDrawTextLine(opaq, 1);
		tdrawed = 1;
		opaq = 0;
	}

	// Special priority handling
	if (((state_.vc.vcreg2[0] & 0x5c) == 0x14) && (pron)) {
		WinDrawDrawPriLine();
	}
	else if (((state_.vc.vcreg2[0] & 0x5d) == 0x1c) && (tron)) {
		WinDrawDrawHalfFillLine();
	}

	if (opaq) {
		DWORD adr = state_.vline * PX68K_FULLSCREEN_WIDTH;
		std::memset(&screen_buffer_[adr], 0, state_.crtc.textdotx * 2);
	}
}

//===========================================================================
//
//	Frame Processing Functions
//
//===========================================================================

void Px68kVideoEngine::StartFrame()
{
	// Called at the start of each frame
}

void Px68kVideoEngine::EndFrame()
{
	// Called at the end of each frame
}

void Px68kVideoEngine::DrawLine(int vline)
{
	state_.vline = vline;
	WinDraw_DrawLine();
}

void Px68kVideoEngine::DrawFrame()
{
	DWORD y;
	
	StartFrame();

	DWORD lines = state_.crtc.textdoty;
	if (lines > PX68K_FULLSCREEN_HEIGHT) {
		lines = PX68K_FULLSCREEN_HEIGHT;
	}

	for (y = 0; y < lines; y++) {
		DrawLine((int)y);
	}
	for (; y < PX68K_FULLSCREEN_HEIGHT; y++) {
		std::memset(&screen_buffer_[y * PX68K_FULLSCREEN_WIDTH], 0, state_.crtc.textdotx * 2);
	}

	EndFrame();
}

//===========================================================================
//
//	State Save/Load Functions
//
//===========================================================================

BOOL Px68kVideoEngine::SaveState(class Fileio *fio)
{
	// Save video engine state
	DWORD size = sizeof(Px68kVideoEngineState);
	if (!fio->Write(&size, sizeof(DWORD))) return FALSE;
	if (!fio->Write(&state_, size)) return FALSE;
	return TRUE;
}

BOOL Px68kVideoEngine::LoadState(class Fileio *fio)
{
	// Load video engine state
	DWORD size;
	if (!fio->Read(&size, sizeof(DWORD))) return FALSE;
	if (size > sizeof(Px68kVideoEngineState)) size = sizeof(Px68kVideoEngineState);
	if (!fio->Read(&state_, size)) return FALSE;

	// Rebuild derived state
	Pal_SetColor();
	TVRAMSetAllDirty();

	return TRUE;
}

//===========================================================================
//
//	RGB565 Output Configuration
//
//===========================================================================

void Px68kVideoEngine::SetRGB565Masks(WORD r, WORD g, WORD b)
{
	windraw_pal16r_ = r;
	windraw_pal16g_ = g;
	windraw_pal16b_ = b;
	Pal_SetColor();
}
