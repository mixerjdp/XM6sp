//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 P.I. (ytanaka@ipc-tokai.or.jp)
//	Copyright (C) 2010-2014 GIMONS
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "tvram.h"
#include "mfp.h"
#include "sprite.h"
#include "render.h"
#include "schedule.h"
#include "gvram.h"
#include "printer.h"
#include "vc.h"
#include "config.h"
#include "fileio.h"
#include "memory.h"
#include "crtc.h"
#include "px68k_crtc_port.h"
#if XM6_RENDER_SYNC == 1
class CScheduler {
public:
	void FASTCALL UpdateFrame() {}
};
#endif	// XM6_RENDER_SYNC == 1

//===========================================================================
//
//	CRTC
//
//===========================================================================
//#define CRTC_LOG

namespace {
BOOL g_alt_raster_timing = TRUE;
}

//---------------------------------------------------------------------------
//
/// Constructor
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM* p) : MemDevice(p)
{
	// Initialize the device ID
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// Start and end addresses
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// Other working state
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	mfp = NULL;
	render = NULL;
	printer = NULL;
	vc = NULL;
#if XM6_RENDER_SYNC == 1
	m_pScheduler = NULL;
#endif	// XM6_RENDER_SYNC == 1

	memset(&crtc, 0, sizeof(crtc));
	memset(&px68k_state_view, 0, sizeof(px68k_state_view));
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Acquire text VRAM
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// Acquire graphics VRAM
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// Acquire the sprite controller
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// Acquire MFP
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// Acquire the renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Acquire the printer
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// Acquire VC
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// Initialize the event
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	// event.SetTime(0);
	scheduler->AddEventDirect(&event);

	// Initialize block scan mode
	crtc.h_blockscan = FALSE;

	// Sync with the host VSYNC
	crtc.disp_vsync = FALSE;

	// HSync request
	hsync = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Cleanup()
{
	ASSERT(this);

	// Base class cleanup
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "リセット");

	// Clear the registers
	memset(crtc.reg, 0, sizeof(crtc.reg));
	for (i=0; i<18; i++) {
		crtc.reg[i] = ResetTable[i];
	}
	for (i=0; i<8; i++) {
		crtc.reg[i + 0x28] = ResetTable[i + 18];
	}

	// Resolution
	crtc.hrl = FALSE;
	crtc.lowres = FALSE;
	crtc.textres = TRUE;
	crtc.changed = TRUE;

	// Special state
	crtc.raster_count = 0;
	crtc.raster_int = 0;
	crtc.raster_copy = FALSE;
	crtc.raster_exec = FALSE;
	crtc.fast_clr = 0;

	// Horizontal
	crtc.h_sync = 31745;
	crtc.h_pulse = 3450;
	crtc.h_back = 4140;
	crtc.h_front = 2070;
	crtc.h_dots = 768;
	crtc.h_mul = 1;
	crtc.hd = 2;

	// Vertical
	crtc.v_sync = 568;
	crtc.v_pulse = 6;
	crtc.v_back = 35;
	crtc.v_front = 15;
	crtc.v_dots = 512;
	crtc.v_mul = 1;
	crtc.vd = 1;

	// Event state
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = -1;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// Horizontal display scan state
	crtc.h_blocknum = 0;
	crtc.h_blockpos = 0;

	// Memory mode
	crtc.tmem = FALSE;
	crtc.gmem = TRUE;
	crtc.siz = 0;
	crtc.col = 3;

	// Scroll
	crtc.text_scrlx = 0;
	crtc.text_scrly = 0;
	for (i=0; i<4; i++) {
		crtc.grp_scrlx[i] = 0;
		crtc.grp_scrly[i] = 0;
	}

	// Interlace even/odd state
	crtc.v_scaneven = FALSE;

	// Set the H-Sync event (31.5 us)
	event.SetTimeDirect(63);

	// Notify the renderer
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	render->SetCRTC();
}

//---------------------------------------------------------------------------
//
//	CRTC reset data
//
//---------------------------------------------------------------------------
const BYTE CRTC::ResetTable[] = {
	0x89, 0x00, 0x0e, 0x00, 0x1c, 0x00, 0x7c, 0x00,
	0x37, 0x02, 0x05, 0x00, 0x28, 0x00, 0x28, 0x02,
	0x1b, 0x00,
	0x16, 0x0b, 0x33, 0x00, 0x7b, 0x7a, 0x00, 0x00
};

//---------------------------------------------------------------------------
//
/// Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio* fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "セーブ");

	// Build save data
	crtc_t data = crtc;
	data.disp_vsync = FALSE;
	ASSERT(data.h_refresh == 0);
	data.h_blockscan = FALSE;
	data.h_blocknum = 0;
	data.h_blockpos = 0;
	data.v_scaneven = FALSE;

	// Save the size
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save the payload
	if (!fio->Write(&data, sz)) {
		return FALSE;
	}

	// Save the event
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio* fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "ロード");

	// Preserve settings
	crtc_t backup = crtc;

	// Load the size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// Load the payload
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// Load the event
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// Restore settings
	crtc.disp_vsync = backup.disp_vsync;
	crtc.h_blockscan = backup.h_blockscan;

	// Notify the renderer
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
#if 0
	render->SetCRTC();
#else
	// Temporary workaround because the horizontal scale factor changed
	ReCalc();
#endif

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// Apply settings
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	g_alt_raster_timing = config->alt_raster ? TRUE : FALSE;
	LOG0(Log::Normal, "設定適用");

	// Horizontal display period scan mode
	crtc.h_blockscan = config->disp_blockscan;

	// Sync with the host-side VSYNC
	crtc.disp_vsync = FALSE;

	// Force recalculation
	crtc.changed = TRUE;
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadByte(DWORD addr)
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Mirror every 0x800 bytes
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// 0xE80000-0xE803FF: register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Only R20 and R21 are readable/writable. Everything else returns 0x00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// Read (swap endian order)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// 0xE80480-0xE804FF: operation ports
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Upper byte is 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// Lower byte exposes raster copy and graphics fast clear only
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "未実装アドレス読み込み $%06X", memdev.first + addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::WriteByte(DWORD addr, DWORD data)
{
	int reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Mirror every 0x800 bytes
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// 0xE80000-0xE803FF: register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// Write (swap endian order)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;

		// GVRAM address configuration
		if (addr == 0x29) {
			if (data & 0x10) {
				crtc.tmem = TRUE;
			}
			else {
				crtc.tmem = FALSE;
			}
			if (data & 0x08) {
				crtc.gmem = TRUE;
			}
			else {
				crtc.gmem = FALSE;
			}
			crtc.siz = (data & 4) >> 2;
			crtc.col = (data & 3);

			// Notify graphics VRAM
			gvram->SetType(data & 0x0f);

			// Recalculate on the next cycle
			crtc.changed = TRUE;
			return;
		}

		// Resolution change
		if ((addr <= 15) || (addr == 40)) {
			// Connect/disconnect sprite memory immediately (OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// Recalculate on the next cycle
			crtc.changed = TRUE;
			return;
		}

		// Raster interrupt
		if ((addr == 18) || (addr == 19)) {
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
#if defined(CRTC_LOG)
			LOG2(Log::Normal, "ラスタ割り込み 設定 int=%d rast=%d", crtc.raster_int, crtc.raster_count);
#endif
			if (g_alt_raster_timing) {
				CheckRaster();
			}
			return;
		}

		// Text scroll
		if ((addr >= 20) && (addr <= 23)) {
			crtc.text_scrlx = (crtc.reg[21] << 8) + crtc.reg[20];
			crtc.text_scrlx &= 0x3ff;
			crtc.text_scrly = (crtc.reg[23] << 8) + crtc.reg[22];
			crtc.text_scrly &= 0x3ff;
			hsync = TRUE;

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "テキストスクロール x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// Graphics scroll
		if ((addr >= 24) && (addr <= 39)) {
			reg = addr & ~3;
			addr -= 24;
			addr >>= 2;
			ASSERT(addr <= 3);
			crtc.grp_scrlx[addr] = (crtc.reg[reg + 1] << 8) + crtc.reg[reg + 0];
			crtc.grp_scrly[addr] = (crtc.reg[reg + 3] << 8) + crtc.reg[reg + 2];
			if (addr == 0) {
				crtc.grp_scrlx[addr] &= 0x3ff;
				crtc.grp_scrly[addr] &= 0x3ff;
			}
			else {
				crtc.grp_scrlx[addr] &= 0x1ff;
				crtc.grp_scrly[addr] &= 0x1ff;
			}
			hsync = TRUE;
			return;
		}

		// Text VRAM
		if ((addr >= 42) && (addr <= 47)) {
			TextVRAM();
		}
		return;
	}

	// 0xE80480-0xE804FF: operation ports
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Upper byte has no function
		if ((addr & 1) == 0) {
			return;
		}

		// Lower byte controls raster copy and fast clear
		if (data & 0x08) {
#if defined(CRTC_LOG)
			if (!crtc.raster_copy) {
				LOG0(Log::Normal, "ラスタコピー指示");
			}
#endif	// CRTC_LOG
			crtc.raster_copy = TRUE;
		}
		else {
#if defined(CRTC_LOG)
			if (crtc.raster_copy) {
				LOG0(Log::Normal, "ラスタコピー停止");
			}
#endif	// CRTC_LOG
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// Ignore requests while a fast clear is already active (Etoile Princess)
			if (crtc.fast_clr != 2) {
#if defined(CRTC_LOG)
				if (crtc.fast_clr == 0) {
					LOG0(Log::Normal, "グラフィック高速クリア指示");
				}
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
			}
		} else {
#if defined(CRTC_LOG)
			if (crtc.fast_clr != 0) {
				LOG0(Log::Normal, "グラフィック高速クリア停止");
			}
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}
		return;
	}

	LOG2(Log::Warning, "未実装アドレス書き込み $%06X <- $%02X",
							memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	Read-only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Mirror every 0x800 bytes
	addr &= 0x7ff;

	// 0xE80000-0xE803FF: register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Read (swap endian order)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// 0xE80480-0xE804FF: operation ports
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Upper byte is 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// Lower byte exposes raster copy and graphics fast clear
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetCRTC(crtc_t *buffer) const
{
	ASSERT(buffer);

	// Copy the internal data
	*buffer = crtc;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// Dispatch between HSync and HDisp
	/// @todo Replace the conditional state machine with raster/block function-pointer groups so the three transitions need no runtime checks; direct vtable-style dispatch would be ideal
	if (crtc.h_disp < 0) {
		HSync();
	}
	else {
		if (!crtc.h_blockscan) {
			// Raster scan
			HDispRS();
		} else {
			// Block scan
			HDispBS();
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// H-SYNC start
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	size_t i;
	int hus;

	ASSERT(this);

	// Notify the printer (to drop BUSY periodically)
	ASSERT(printer);
	printer->HSync();

	// Notify VC (apply delayed state changes)
	vc->HSync();

	// Notify the sprite controller (apply delayed BG setting changes)
	sprite->HSync();

	// Pending HSync updates
	if (hsync) {
		// Clear the flag
		hsync = FALSE;

		// Apply delayed text scroll values
		render->TextScrl(crtc.text_scrlx, crtc.text_scrly);

		// Apply delayed graphics scroll values
		for (i=0; i<4; i++) {
			render->GrpScrl(i, crtc.grp_scrlx[i], crtc.grp_scrly[i]);
		}
	}

	// Update flags
	crtc.h_disp = 0;

	// Update GPIP
	mfp->SetGPIP(7, 1);

	// Advance the scanline
	crtc.v_scan++;

	if (g_alt_raster_timing) {
		Raster();
		CheckRaster();
	}

	// V-SYNC counter
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANK counter
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// Text raster copy
	if (crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// Graphics fast clear
	if (crtc.fast_clr == 2) {
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// Set the time until the next timing point (scan start)
	crtc.ns += crtc.h_pulse;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);		/// @todo Avoid the division here. Use a DDA accumulator instead; ns only needs recomputing during save/load

	// Schedule the next callback
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Guard against the event stopping in rare cases
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);

	// Synchronization processing (every 40 ms)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}
}

//---------------------------------------------------------------------------
//
//	Raster counter
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Raster()
{
	// Update the raster counter
	crtc.raster_count++;

	// Clear it on the H-SYNC falling edge just before the V-SYNC falling edge
	if (crtc.v_synccnt == 1 && crtc.v_disp) {
		crtc.raster_count = 0;
	}
}

//---------------------------------------------------------------------------
//
//	Raster interrupt check
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
	if (crtc.raster_count == crtc.raster_int) {
		// Assert the request
		mfp->SetGPIP(6, 0);
	} else {
		// Deassert the request
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
/// H-DISP start (raster scan)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDispRS()
{
	int ns;
	int hus;

	ASSERT(this);

	if (crtc.h_disp == 0) {
		// HDISP start

		// Update GPIP
		mfp->SetGPIP(7, 0);

		if (!g_alt_raster_timing) {
			CheckRaster();
			crtc.raster_count++;
		}

		// Allow raster copy
		crtc.raster_exec = TRUE;

		// Set the time until the next timing point (front porch)
		/// @todo This is an extremely hot XM6 path. Precompute it
		ns = crtc.h_sync - crtc.h_pulse - crtc.h_front;

		// Move to the front porch phase
		crtc.h_disp = 1;
	} else {
		// Front porch

		// Render just before the end
		if (!crtc.v_blank) {
			// Render
			render->HSync(crtc.v_scan, 0);
		}

		// Set the time until the next timing point (H-SYNC start)
		ns = crtc.h_front;

		// Move to the display-end phase
		crtc.h_disp = -1;
	}

	// Guard against negative values in special cases
	/// @todo This is an extremely hot XM6 path. Detect this case in advance
	if (ns <= 0) {
		ns = 1;
	}

	crtc.ns += ns;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);

	// Schedule the next callback
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Guard against the event stopping in rare cases
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);
}

//---------------------------------------------------------------------------
//
//	H-DISP start (block scan)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDispBS()
{
	int dc;
	int ns;
	int hus;

	ASSERT(this);

	// Get the dot clock
	dc = Get8DotClock();

	if (crtc.h_disp == 0) {
		// Initialize the block position
		crtc.h_blockpos = 0;

		// Calculate the block count
		crtc.h_blocknum = (crtc.reg[0x06] - crtc.reg[0x04]) >> 1;

		// Set the phase
		if (crtc.h_blocknum > 0) {
			// Display phase
			crtc.h_disp = 1;
		} else {
			// Front porch phase
			crtc.h_disp = 2;
		}

		// Update GPIP
		mfp->SetGPIP(7, 0);

		if (!g_alt_raster_timing) {
			CheckRaster();
			crtc.raster_count++;
		}

		// Allow raster copy
		crtc.raster_exec = TRUE;

		// Set the back porch duration
		ns = crtc.h_back;
	} else if (crtc.h_disp == 1) {
		// Advance to the next block
		crtc.h_blockpos++;

		// Move to the front porch phase
		if (crtc.h_blockpos >= crtc.h_blocknum) {
			crtc.h_disp = 2;
		}

		// Render
		if (!crtc.v_blank) {
			// Render
			render->HSync(crtc.v_scan, crtc.h_blockpos - 1);
		}

		// Set the time until the next 16 dots
		ns = 2 * dc / 100;
	} else {
		// Move to the display-end phase
		crtc.h_disp = -1;

		// Set the time until the next HSYNC (absorb the error here)
		ns = crtc.h_sync - crtc.h_pulse - crtc.h_back - crtc.h_front;
		ns -= (2 * dc / 100) * crtc.h_blocknum;
		ns += crtc.h_front;
	}

	// Guard against negative values in special cases
	if (ns <= 0) {
		ns = 1;
	}

	crtc.ns += ns;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);

	// Schedule the next callback
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Guard against the event stopping in rare cases
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);
}

//---------------------------------------------------------------------------
//
/// V-SYNC start
///
/// Includes the start of V-DISP.
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// If V-SYNC is ending
	if (!crtc.v_disp) {
		// Update flags
		crtc.v_disp = TRUE;

		// Set the timing
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);

		// Add a dummy line in interlace mode
		if ((crtc.lowres && crtc.vd > 0) || (!crtc.lowres && crtc.vd > 1)) {
			if (!crtc.v_scaneven) {
				crtc.v_synccnt++;
			}
		}

		return;
	}

	// Finish renderer composition
	render->EndFrame();
#if XM6_RENDER_SYNC == 1
	if (m_pScheduler) {
		m_pScheduler->UpdateFrame();
	}
#endif	// XM6_RENDER_SYNC == 1
	crtc.v_scan = crtc.v_dots + 1;

	// Apply any pending resolution change here
	if (crtc.changed) {
		ReCalc();

		// Clear the flag
		crtc.changed = FALSE;
	}

	// Set the time until V-SYNC ends
	crtc.v_synccnt = crtc.v_pulse;

	// Set the V-BLANK state and duration
	if (crtc.v_front <= 0) {
		// Still in the display period (special case)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// Already in blanking (normal case)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// Update flags
	crtc.v_disp = FALSE;
	if (!g_alt_raster_timing) {
		crtc.raster_count = 0;
	}

	// Toggle the interlace even/odd flag
	crtc.v_scaneven = !crtc.v_scaneven;
}

//---------------------------------------------------------------------------
//
/// Recalculate
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);

	// If CRTC register 0 is cleared, disable output (Mac emulator)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "再計算");
#endif	// CRTC_LOG

		// Get the dot clock
		dc = Get8DotClock();

		// Horizontal timing (all values in ns)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// Vertical timing (all values in H-Sync units)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// If V-FRONT goes too negative, clamp it to one horizontal period only (Hellhound, Cotton)
		if (crtc.v_front <= 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// Calculate the dot counts
		crtc.h_dots = (crtc.reg[0x0] + 1);
		crtc.h_dots -= (crtc.reg[0x02] + 1);
		crtc.h_dots -= (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1);
		crtc.h_dots -= (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5);
		crtc.h_dots *= 8;
		crtc.v_dots = crtc.v_sync - crtc.v_pulse - crtc.v_back;
		if (crtc.v_front > 0) {
			crtc.v_dots -= crtc.v_front;
		}
	}

	// Set the horizontal scale factor
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "横ドット数50MHzモード(CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// Set the vertical scale factor
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31 kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// Interlaced 1024-dot mode
			crtc.v_mul = 0;
		}
		else {
			// Interlace, normal 512-dot mode (x1), doubled 256-dot mode (x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15 kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// Normal 256-dot mode (x2)
			crtc.v_mul = 2;
		}
		else {
			// Interlaced 512-dot mode (x1)
			crtc.v_mul = 0;
		}
	}

	// When crtc.hd is 2 or greater, sprites are disconnected
	if (crtc.hd >= 2) {
		// 768x512 or VGA mode (no sprites)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512 mode (sprites enabled)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// Initialize the interlace even/odd flag
	crtc.v_scaneven = FALSE;

	// Notify the renderer
	render->SetCRTC();
}

//---------------------------------------------------------------------------
//
//	V-BLANK start (includes V-SCREEN start)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// If currently displaying, start blanking
	if (!crtc.v_blank) {
		// Set the blanking interval
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
//		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// Flags
		crtc.v_blank = TRUE;

		// GPIP event count
		mfp->EventCount(0, 0);

		// Notify GPIP
		mfp->SetGPIP(4, 0);

		// Graphics fast clear
		if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "グラフィック高速クリア終了");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}

		return;
	}

	// Set the display interval
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= crtc.v_pulse + crtc.v_back + crtc.v_front;

	// Flags
	crtc.v_blank = FALSE;

	// GPIP event count
	mfp->EventCount(0, 1);

	// Notify GPIP
	mfp->SetGPIP(4, 1);

	// Graphics fast clear
	// If a resolution change and graphics fast clear are requested between V-SYNC end and V-DISP start,
	// the clear is deferred until the next V-DISP instead of starting immediately (Naious)
	if (!crtc.changed && crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "グラフィック高速クリア開始 data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
	}

	// Start renderer composition and increment the frame counter
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
}

//---------------------------------------------------------------------------
//
//	Get the display frequencies
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// Assert
	ASSERT(h);
	ASSERT(v);

	// Check
	if ((crtc.h_sync == 0) || (crtc.v_sync < 100)) {
		// No signal
		*h = 0;
		*v = 0;
		return;
	}

	// e.g. 31.5 kHz = 3150
	d = 100 * 1000 * 1000;
	d /= crtc.h_sync;
	*h = d;

	// e.g. 55.46 Hz = 5546
	t = crtc.v_sync;
	t *= crtc.h_sync;

	// In interlace mode, the vertical sync period is
	// extended by half of a horizontal sync period
	if ((crtc.lowres && crtc.vd > 0) ||
			(!crtc.lowres && crtc.vd > 1)) {
		t += crtc.h_sync >> 1;
	}

	t /= 100;
	d = 1000 * 1000 * 1000;
	d /= t;
	*v = d;
}

//---------------------------------------------------------------------------
//
//	Get the 8-dot clock (x100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// Get HF and HD from CRTC R20
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// Build the lookup index
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8-dot clock table
//	(value derived from HRL, HF, and HD; units are 0.01 ns)
//
//---------------------------------------------------------------------------
const int CRTC::DotClockTable[16] = {
	// HRL=0
	164678, 82339, 164678, 164678,
	69013, 34507, 23004, 31778,
	// HRL=1
	164678, 82339, 164678, 164678,
	92017, 46009, 23004, 31778
};

//---------------------------------------------------------------------------
//
//	Set HRL
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::SetHRL(BOOL flag)
{
	if (crtc.hrl != flag) {
		// Recalculate on the next cycle
		crtc.hrl = flag;
		crtc.changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Get HRL
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::GetHRL() const
{
	return crtc.hrl;
}

//---------------------------------------------------------------------------
//
//	Text VRAM effects
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// Simultaneous access
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4 is the multi flag
		b |= 0x10;
		tvram->SetMulti(b);
	}
	else {
		tvram->SetMulti(0);
	}

	// Access mask
	if (crtc.reg[43] & 2) {
		w = (DWORD)crtc.reg[47];
		w <<= 8;
		w |= (DWORD)crtc.reg[46];
		tvram->SetMask(w);
	}
	else {
		tvram->SetMask(0);
	}

	// Raster copy
	tvram->SetCopyRaster((DWORD)crtc.reg[45], (DWORD)crtc.reg[44],
						(DWORD)(crtc.reg[42] & 0x0f));
}

//---------------------------------------------------------------------------
//
//	PX68k CRTC state view
//
//---------------------------------------------------------------------------
const Px68kCrtcStateView* FASTCALL CRTC::GetPx68kStateView() const
{
	SyncPx68kState();
	return &px68k_state_view;
}
void FASTCALL CRTC::SyncPx68kState() const
{
	int i;
	const DWORD hstart = (DWORD)(((crtc.reg[0x04] << 8) | crtc.reg[0x05]) & 1023);
	const DWORD hend = (DWORD)(((crtc.reg[0x06] << 8) | crtc.reg[0x07]) & 1023);
	const DWORD vstart = (DWORD)(((crtc.reg[0x0c] << 8) | crtc.reg[0x0d]) & 1023);
	const DWORD vend = (DWORD)(((crtc.reg[0x0e] << 8) | crtc.reg[0x0f]) & 1023);
	BYTE vstep;

	memset(&px68k_state_view, 0, sizeof(px68k_state_view));
	for (i = 0; i < 48; i++) {
		px68k_state_view.state.regs[i] = crtc.reg[i];
	}

	if ((crtc.reg[0x29] & 0x14) == 0x10) {
		vstep = 1;
	}
	else if ((crtc.reg[0x29] & 0x14) == 0x04) {
		vstep = 4;
	}
	else {
		vstep = 2;
	}

	px68k_state_view.state.mode = crtc.reg[0x29];
	px68k_state_view.state.hrl = crtc.hrl;
	px68k_state_view.state.lowres = crtc.lowres;
	px68k_state_view.state.textres = crtc.textres;
	px68k_state_view.state.changed = crtc.changed;
	px68k_state_view.state.h_disp = crtc.h_disp;
	px68k_state_view.state.v_disp = crtc.v_disp;
	px68k_state_view.state.v_blank = crtc.v_blank;
	px68k_state_view.state.v_count = crtc.v_count;
	px68k_state_view.state.raster_count = crtc.raster_count;
	px68k_state_view.state.textdotx = (crtc.h_dots > 0) ? (DWORD)crtc.h_dots : ((hend > hstart) ? ((hend - hstart) * 8) : 0);
	px68k_state_view.state.textdoty = (crtc.v_dots > 0) ? (DWORD)crtc.v_dots : ((vend > vstart) ? (vend - vstart) : 0);
	px68k_state_view.state.vstart = (WORD)vstart;
	px68k_state_view.state.vend = (WORD)vend;
	px68k_state_view.state.hstart = (WORD)hstart;
	px68k_state_view.state.hend = (WORD)hend;
	px68k_state_view.state.h_sync = (DWORD)crtc.h_sync;
	px68k_state_view.state.h_pulse = (DWORD)crtc.h_pulse;
	px68k_state_view.state.h_back = (DWORD)crtc.h_back;
	px68k_state_view.state.h_front = (DWORD)crtc.h_front;
	px68k_state_view.state.v_sync = (DWORD)crtc.v_sync;
	px68k_state_view.state.v_pulse = (DWORD)crtc.v_pulse;
	px68k_state_view.state.v_back = (DWORD)crtc.v_back;
	px68k_state_view.state.v_front = (DWORD)crtc.v_front;
	px68k_state_view.state.ns = crtc.ns;
	px68k_state_view.state.hus = crtc.hus;
	px68k_state_view.state.v_synccnt = crtc.v_synccnt;
	px68k_state_view.state.v_blankcnt = crtc.v_blankcnt;
	px68k_state_view.state.textscrollx = crtc.text_scrlx;
	px68k_state_view.state.textscrolly = crtc.text_scrly;
	for (i = 0; i < 4; i++) {
		px68k_state_view.state.grphscrollx[i] = crtc.grp_scrlx[i];
		px68k_state_view.state.grphscrolly[i] = crtc.grp_scrly[i];
	}
	px68k_state_view.state.fastclr = (BYTE)crtc.fast_clr;
	px68k_state_view.state.dispscan = (BYTE)((crtc.v_scan >= 0) ? crtc.v_scan : 0);
	px68k_state_view.state.intline = (WORD)crtc.raster_int;
	px68k_state_view.state.vstep = vstep;
	px68k_state_view.state.visible_vline = 0xffffffffu;
	if ((crtc.v_scan >= (int)vstart) && (crtc.v_scan < (int)vend)) {
		px68k_state_view.state.visible_vline = (DWORD)(((DWORD)(crtc.v_scan - (int)vstart) * (DWORD)vstep) / 2u);
	}
	px68k_state_view.state.hsync_clk = crtc.h_sync;
	px68k_state_view.state.hd = crtc.hd;
	px68k_state_view.state.vd = crtc.vd;
	px68k_state_view.state.rcflag[0] = crtc.raster_copy ? 1 : 0;
	px68k_state_view.state.rcflag[1] = crtc.raster_exec ? 1 : 0;
	px68k_state_view.state.vcreg0[0] = crtc.reg[0x28];
	px68k_state_view.state.vcreg0[1] = crtc.reg[0x29];
	px68k_state_view.state.vcreg1[0] = crtc.reg[0x2a];
	px68k_state_view.state.vcreg1[1] = crtc.reg[0x2b];
	px68k_state_view.state.vcreg2[0] = crtc.reg[0x2c];
	px68k_state_view.state.vcreg2[1] = crtc.reg[0x2d];

	px68k_state_view.timing_view.valid = 1;
	px68k_state_view.timing_view.crtc_vsync_high = (crtc.reg[0x29] & 0x10) ? 1 : 0;
	px68k_state_view.timing_view.crtc_vline_total = (DWORD)crtc.v_sync;
	px68k_state_view.timing_view.crtc_vstart = (WORD)vstart;
	px68k_state_view.timing_view.crtc_vend = (WORD)vend;
	px68k_state_view.timing_view.crtc_intline = (WORD)crtc.raster_int;
	px68k_state_view.timing_view.crtc_vstep = vstep;
	px68k_state_view.timing_view.crtc_mode = crtc.reg[0x29];
	px68k_state_view.timing_view.crtc_fastclr = (BYTE)crtc.fast_clr;
}
