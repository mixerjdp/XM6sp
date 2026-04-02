//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ CRTC(VICON) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "tvram.h"
#include "mfp.h"
#include "sprite.h"
#include "render.h"
#include "schedule.h"
#include "cpu.h"
#include "gvram.h"
#include "printer.h"
#include "fileio.h"
#include "crtc.h"
#include "config.h"

//===========================================================================
//
//	CRTC
//
//===========================================================================
//#define CRTC_LOG

namespace {
BOOL g_alt_raster_timing = FALSE;
}

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM *p) : MemDevice(p)
{
	// Initialize device ID
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// Start address, end address
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// Other members
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	mfp = NULL;
	render = NULL;
	printer = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get text VRAM
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// Get graphic VRAM
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// Get sprite controller
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// Get MFP
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// Get renderer
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// Get printer
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// Event setup
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	event.SetTime(0);
	scheduler->AddEvent(&event);

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

	// To base class
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
	LOG0(Log::Normal, "Reset");

	// Clear registers
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
	crtc.changed = FALSE;

	// Raster
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

	// Events
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = TRUE;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// Non-display variables
	crtc.h_disptime = 0;
	crtc.h_synctime = 0;
	crtc.v_cycletime = 0;
	crtc.v_blanktime = 0;
	crtc.v_backtime = 0;
	crtc.v_synctime = 0;

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

	// H-Sync event setup (31.5us)
	event.SetTime(63);
}

//---------------------------------------------------------------------------
//
//	CRTC reset data
//
//---------------------------------------------------------------------------
const BYTE CRTC::ResetTable[] = {
	0x00, 0x89, 0x00, 0x0e, 0x00, 0x1c, 0x00, 0x7c,
	0x02, 0x37, 0x00, 0x05, 0x00, 0x28, 0x02, 0x28,
	0x00, 0x1b,
	0x0b, 0x16, 0x00, 0x33, 0x7a, 0x7b, 0x00, 0x00
};

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save entity
	if (!fio->Write(&crtc, (int)sz)) {
		return FALSE;
	}

	// Save event
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "Load");

	// Load size
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// Load entity
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// Load event
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// Notify renderer
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	render->SetCRTC();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	g_alt_raster_timing = config->alt_raster;
	LOG0(Log::Normal, "Apply configuration");
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

	// Loop at $800 boundary
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Only R20, R21 can be read. Others return $00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// Read (invert odd/even)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Even bytes are 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// Odd byte is raster copy, graphic clear reset
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "Illegal address read $%06X", memdev.first + addr);
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

	// Loop at $800 boundary
	addr &= 0x7ff;

	// Wait
	scheduler->Wait(1);

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// Write (invert odd/even)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;

		// GVRAM address display
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

			// Notify graphic VRAM
			gvram->SetType(data & 0x0f);
			return;
		}

		// Resolution change
		if ((addr <= 15) || (addr == 40)) {
			// Sprite connection/disconnection is done immediately (OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// Recalc at next timing
			crtc.changed = TRUE;
			return;
		}

		// Raster interrupt
		if ((addr == 18) || (addr == 19)) {
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
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
			render->TextScrl(crtc.text_scrlx, crtc.text_scrly);

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "Text scroll x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// Graphic scroll
		if ((addr >= 24) && (addr <= 39)) {
			reg = addr & ~3;
			addr -= 24;
			addr >>= 2;
			ASSERT(addr <= 3);
			crtc.grp_scrlx[addr] = (crtc.reg[reg+1] << 8) + crtc.reg[reg+0];
			crtc.grp_scrly[addr] = (crtc.reg[reg+3] << 8) + crtc.reg[reg+2];
			if (addr == 0) {
				crtc.grp_scrlx[addr] &= 0x3ff;
				crtc.grp_scrly[addr] &= 0x3ff;
			}
			else {
				crtc.grp_scrlx[addr] &= 0x1ff;
				crtc.grp_scrly[addr] &= 0x1ff;
			}
			render->GrpScrl(addr, crtc.grp_scrlx[addr], crtc.grp_scrly[addr]);
			return;
		}

		// Text VRAM
		if ((addr >= 42) && (addr <= 47)) {
			TextVRAM();
		}
		return;
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Even bytes are ignored
		if ((addr & 1) == 0) {
			return;
		}

		// Odd byte is raster copy, fast clear reset
		if (data & 0x08) {
			crtc.raster_copy = TRUE;
		}
		else {
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// Raster copy and fast clear (X68030'90)
			if ((crtc.fast_clr == 0) && !crtc.raster_copy) {
#if defined(CRTC_LOG)
				LOG0(Log::Normal, "Graphic clear fast enable");
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
			}
#if defined(CRTC_LOG)
			else {
				LOG1(Log::Normal, "Graphic clear fast state State=%d", crtc.fast_clr);
			}
#endif	//CRTC_LOG
		}
		return;
	}

	LOG2(Log::Warning, "Illegal address write $%06X <- $%02X",
							memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Loop at $800 boundary
	addr &= 0x7ff;

	// $E80000-$E803FF : Register area
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// Read (invert odd/even)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : Internal port
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// Even bytes are 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// Odd byte is raster copy, graphic clear reset
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

	// Copy internal data
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

	// HSync and HDisp are exclusive
	if (crtc.h_disp) {
		HSync();
	}
	else {
		HDisp();
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	H-SYNC start
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	int hus;

	ASSERT(this);

	// Notify printer (ignore busy)
	ASSERT(printer);
	printer->HSync();

	// V-SYNC count
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANK count
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// Set time until next timing (H-DISP start)
	crtc.ns += crtc.h_pulse;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// Adjust (every 40ms)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}

	// Set flag
	crtc.h_disp = FALSE;

	// GPIP set
	mfp->SetGPIP(7, 1);

	// Raster interrupt (Alt timing)
	if (g_alt_raster_timing) {
		CheckRaster();
	}

	// Rendering
	crtc.v_scan++;
	if (!crtc.v_blank) {
		// Renderer sync
		render->HSync(crtc.v_scan);
	}

	// Text screen raster copy
	if (crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// Graphic screen fast clear
	if (crtc.fast_clr == 2) {
		gvram->FastClr(&crtc);
	}

	if (g_alt_raster_timing) {
		crtc.raster_count++;
	}
}

//---------------------------------------------------------------------------
//
//	H-DISP start
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDisp()
{
	int ns;
	int hus;

	ASSERT(this);

	// Raster interrupt (Original timing)
	if (!g_alt_raster_timing) {
		CheckRaster();
		crtc.raster_count++;
	}
	else {
		CheckRaster();
	}

	// Set time until next timing (H-SYNC start)
	ns = crtc.h_sync - crtc.h_pulse;
	ASSERT(ns > 0);
	crtc.ns += ns;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// Set flag
	crtc.h_disp = TRUE;

	// GPIP set
	mfp->SetGPIP(7,0);

	// Raster copy start
	crtc.raster_exec = TRUE;
}

//---------------------------------------------------------------------------
//
//	V-SYNC start (also starts V-DISP)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// V-SYNC not ended yet
	if (!crtc.v_disp) {
		// Set flag
		crtc.v_disp = TRUE;

		// Set counter
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);
		return;
	}

	// If resolution changed, recalc now
	if (crtc.changed) {
		ReCalc();
	}

	// Set time until V-SYNC end
	crtc.v_synccnt = crtc.v_pulse;

	// V-BLANK state and counter setup
	if (crtc.v_front < 0) {
		// Not yet displayed (minus)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// Already in blank (normal)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// Set flag
	crtc.v_disp = FALSE;

	// Reset raster counter
	crtc.raster_count = 0;
}

//---------------------------------------------------------------------------
//
//	Recalculate
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);
	ASSERT(crtc.changed);

	// If CRTC register 0 is not zero, recalc (Mac compatible)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "Recalculate");
#endif	// CRTC_LOG

		// Get dot clock
		dc = Get8DotClock();

		// Horizontal (all in ns units)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// Vertical (all in H-Sync units)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// If V-FRONT is negative, 1 is subtracted (interlace, scan doubler)
		if (crtc.v_front < 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// Dot count
		crtc.h_dots = (crtc.reg[0x0] + 1);
		crtc.h_dots -= (crtc.reg[0x02] + 1);
		crtc.h_dots -= (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1);
		crtc.h_dots -= (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5);
		crtc.h_dots *= 8;
		crtc.v_dots = crtc.v_sync - crtc.v_pulse - crtc.v_back - crtc.v_front;
	}

	// Horizontal settings (normal)
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "High dot 50MHz mode (CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// If crtc.hd is 2 or more, sprites are disabled
	if (crtc.hd >= 2) {
		// 768x512 or VGA mode (no sprite)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512 mode (sprite enabled)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// Vertical settings (normal)
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// Interlace x1024dot mode
			crtc.v_mul = 0;
		}
		else {
			// Interlace, normal 512 mode (x1), normal 256dot mode (x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// Normal 256dot mode (x2)
			crtc.v_mul = 2;
		}
		else {
			// Interlace 512dot mode (x1)
			crtc.v_mul = 0;
		}
	}

	// Notify renderer
	render->SetCRTC();

	// Clear flag
	crtc.changed = FALSE;
}


//---------------------------------------------------------------------------
//
//	V-BLANK start (also starts V-SCREEN)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// If not displayed, start blank
	if (!crtc.v_blank) {
		// Set blank counter
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// Flag
		crtc.v_blank = TRUE;

		// GPIP
		mfp->EventCount(0, 0);
		mfp->SetGPIP(4, 0);

		// Graphic clear
		if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "Graphic clear end");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}

		// Renderer display end
		render->EndFrame();
		crtc.v_scan = crtc.v_dots + 1;
		return;
	}

	// Set non-display counter
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= (crtc.v_pulse + crtc.v_back + crtc.v_front);

	// Flag
	crtc.v_blank = FALSE;

	// GPIP
	mfp->EventCount(0, 1);
	mfp->SetGPIP(4, 1);

	// Graphic clear start
	if (crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "Graphic clear start data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// Renderer display start, counter up
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
}

//---------------------------------------------------------------------------
//
//	Get display frequency
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// assert
	ASSERT(h);
	ASSERT(v);

	// Check
	if ((crtc.h_sync == 0) || (crtc.v_sync < 100)) {
		// NO SIGNAL
		*h = 0;
		*v = 0;
		return;
	}

	// ex. 31.5kHz = 3150
	d = 100 * 1000 * 1000;
	d /= crtc.h_sync;
	*h = d;

	// ex. 55.46Hz = 5546
	t = crtc.v_sync;
    t *= crtc.h_sync;
	t /= 100;
	d = 1000 * 1000 * 1000;
	d /= t;
	*v = d;
}

//---------------------------------------------------------------------------
//
//	Get 8 dot clock (~100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// Get HF, HD from CRTC R20
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// Create index
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8 dot clock table
//	(HRL,HF,HD combined values. 0.01ns units)
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
		// Recalc at next timing
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
//	Raster interrupt check
//	Not supported in interlace mode
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
	BOOL hit;

	if (g_alt_raster_timing) {
		hit = (crtc.raster_count == crtc.raster_int);
	}
	else {
		hit = (crtc.raster_count == crtc.raster_int);
	}

	if (hit) {
		// Match
		mfp->SetGPIP(6, 0);
#if defined(CRTC_LOG)
		LOG2(Log::Normal, "Raster interrupt hit raster=%d scan=%d", crtc.raster_count, crtc.v_scan);
#endif	// CRTC_LOG
	}
	else {
		// No match
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Text VRAM setup
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// Access, multi
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4 is multi flag
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
