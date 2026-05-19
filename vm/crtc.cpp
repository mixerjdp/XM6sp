//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
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
/// ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM* p) : MemDevice(p)
{
	// ƒfƒoƒCƒXID‚ğ‰Šú‰»
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// ŠJnƒAƒhƒŒƒXAI—¹ƒAƒhƒŒƒX
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// ‚»‚Ì‘¼ƒ[ƒN
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
//	‰Šú‰»
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// ƒeƒLƒXƒgVRAM‚ğæ“¾
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// ƒOƒ‰ƒtƒBƒbƒNVRAM‚ğæ“¾
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// ƒXƒvƒ‰ƒCƒgƒRƒ“ƒgƒ[ƒ‰‚ğæ“¾
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// MFP‚ğæ“¾
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// ƒŒƒ“ƒ_ƒ‰‚ğæ“¾
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// ƒvƒŠƒ“ƒ^‚ğæ“¾
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// VCæ“¾
	vc = (VC*)vm->SearchDevice(MAKEID('V', 'C', ' ', ' '));
	ASSERT(vc);

	// ƒCƒxƒ“ƒg‰Šú‰»
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	// event.SetTime(0);
	scheduler->AddEventDirect(&event);

	// ƒuƒƒbƒNƒXƒLƒƒƒ“‰Šú‰»
	crtc.h_blockscan = FALSE;

	// PC‚ÌVSYNC‚Æ“¯Šú
	crtc.disp_vsync = FALSE;

	// HSync—v‹
	hsync = TRUE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒNƒŠ[ƒ“ƒAƒbƒv
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Cleanup()
{
	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX‚Ö
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "ƒŠƒZƒbƒg");

	// ƒŒƒWƒXƒ^‚ğƒNƒŠƒA
	memset(crtc.reg, 0, sizeof(crtc.reg));
	for (i=0; i<18; i++) {
		crtc.reg[i] = ResetTable[i];
	}
	for (i=0; i<8; i++) {
		crtc.reg[i + 0x28] = ResetTable[i + 18];
	}

	// ‰ğ‘œ“x
	crtc.hrl = FALSE;
	crtc.lowres = FALSE;
	crtc.textres = TRUE;
	crtc.changed = TRUE;

	// “Áê‹@”
	crtc.raster_count = 0;
	crtc.raster_int = 0;
	crtc.raster_copy = FALSE;
	crtc.raster_exec = FALSE;
	crtc.fast_clr = 0;

	// …•½
	crtc.h_sync = 31745;
	crtc.h_pulse = 3450;
	crtc.h_back = 4140;
	crtc.h_front = 2070;
	crtc.h_dots = 768;
	crtc.h_mul = 1;
	crtc.hd = 2;

	// ‚’¼
	crtc.v_sync = 568;
	crtc.v_pulse = 6;
	crtc.v_back = 35;
	crtc.v_front = 15;
	crtc.v_dots = 512;
	crtc.v_mul = 1;
	crtc.vd = 1;

	// ƒCƒxƒ“ƒg
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = -1;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// …•½•\¦ƒXƒLƒƒƒ“ŠÖ˜A
	crtc.h_blocknum = 0;
	crtc.h_blockpos = 0;

	// ƒƒ‚ƒŠƒ‚[ƒh
	crtc.tmem = FALSE;
	crtc.gmem = TRUE;
	crtc.siz = 0;
	crtc.col = 3;

	// ƒXƒNƒ[ƒ‹
	crtc.text_scrlx = 0;
	crtc.text_scrly = 0;
	for (i=0; i<4; i++) {
		crtc.grp_scrlx[i] = 0;
		crtc.grp_scrly[i] = 0;
	}

	// ƒCƒ“ƒ^ƒŒ[ƒX‹ôŠï
	crtc.v_scaneven = FALSE;

	// H-SyncƒCƒxƒ“ƒg‚ğİ’è(31.5us)
	event.SetTimeDirect(63);

	// ƒŒƒ“ƒ_ƒ‰‚Ö’Ê’m
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
	render->SetCRTC();
}

//---------------------------------------------------------------------------
//
//	CRTCƒŠƒZƒbƒgƒf[ƒ^
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
/// ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio* fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "ƒZ[ƒu");

	// ƒZ[ƒuƒf[ƒ^¶¬
	crtc_t data = crtc;
	data.disp_vsync = FALSE;
	ASSERT(data.h_refresh == 0);
	data.h_blockscan = FALSE;
	data.h_blocknum = 0;
	data.h_blockpos = 0;
	data.v_scaneven = FALSE;

	// ƒTƒCƒY‚ğƒZ[ƒu
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// À‘Ì‚ğƒZ[ƒu
	if (!fio->Write(&data, sz)) {
		return FALSE;
	}

	// ƒCƒxƒ“ƒg‚ğƒZ[ƒu
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio* fio, int ver)
{
	DWORD sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "ƒ[ƒh");

	// İ’è•Û‘¶
	crtc_t backup = crtc;

	// ƒTƒCƒY‚ğƒ[ƒh
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// À‘Ì‚ğƒ[ƒh
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// ƒCƒxƒ“ƒg‚ğƒ[ƒh
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// İ’è•œŒ³
	crtc.disp_vsync = backup.disp_vsync;
	crtc.h_blockscan = backup.h_blockscan;

	// ƒŒƒ“ƒ_ƒ‰‚Ö’Ê’m
	render->TextScrl(crtc.text_scrlx, crtc.text_scrly);
	render->GrpScrl(0, crtc.grp_scrlx[0], crtc.grp_scrly[0]);
	render->GrpScrl(1, crtc.grp_scrlx[1], crtc.grp_scrly[1]);
	render->GrpScrl(2, crtc.grp_scrlx[2], crtc.grp_scrly[2]);
	render->GrpScrl(3, crtc.grp_scrlx[3], crtc.grp_scrly[3]);
#if 0
	render->SetCRTC();
#else
	// …•½Šg‘å—¦‚ğ•ÏX‚µ‚½‚Ì‚Åb’è‘Î‰
	ReCalc();
#endif

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// İ’è“K—p
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	g_alt_raster_timing = config->alt_raster ? TRUE : FALSE;
	LOG0(Log::Normal, "İ’è“K—p");

	// …•½•\¦ŠúŠÔƒXƒLƒƒƒ“ƒ‚[ƒh
	crtc.h_blockscan = config->disp_blockscan;

	// ƒzƒXƒg‘¤‚ÌVSYNC‚Æ“¯Šú
	crtc.disp_vsync = FALSE;

	// ÄŒvZ‚ğ‘£‚·
	crtc.changed = TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg“Ç‚İ‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadByte(DWORD addr)
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800’PˆÊ‚Åƒ‹[ƒv
	addr &= 0x7ff;

	// ƒEƒFƒCƒg
	scheduler->Wait(1);

	// $E80000-$E803FF : ƒŒƒWƒXƒ^ƒGƒŠƒA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// R20, R21‚Ì‚İ“Ç‚İ‘‚«‰Â”\B‚»‚êˆÈŠO‚Í$00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// “Ç‚İ‚İ(ƒGƒ“ƒfƒBƒAƒ“‚ğ”½“]‚³‚¹‚é)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : “®ìƒ|[ƒg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ãˆÊƒoƒCƒg‚Í 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ‰ºˆÊƒoƒCƒg‚Íƒ‰ƒXƒ^ƒRƒs[AƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒA‚Ì‚İ
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "–¢À‘•ƒAƒhƒŒƒX“Ç‚İ‚İ $%06X", memdev.first + addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg‘‚«‚İ
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::WriteByte(DWORD addr, DWORD data)
{
	int reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800’PˆÊ‚Åƒ‹[ƒv
	addr &= 0x7ff;

	// ƒEƒFƒCƒg
	scheduler->Wait(1);

	// $E80000-$E803FF : ƒŒƒWƒXƒ^ƒGƒŠƒA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// ‘‚«‚İ(ƒGƒ“ƒfƒBƒAƒ“‚ğ”½“]‚³‚¹‚é)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;

		// GVRAMƒAƒhƒŒƒX\¬
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

			// ƒOƒ‰ƒtƒBƒbƒNVRAM‚Ö’Ê’m
			gvram->SetType(data & 0x0f);

			// Ÿ‚ÌüŠú‚ÅÄŒvZ
			crtc.changed = TRUE;
			return;
		}

		// ‰ğ‘œ“x•ÏX
		if ((addr <= 15) || (addr == 40)) {
			// ƒXƒvƒ‰ƒCƒgƒƒ‚ƒŠ‚ÌÚ‘±EØ’f‚Íu‚És‚¤(OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// Ÿ‚ÌüŠú‚ÅÄŒvZ
			crtc.changed = TRUE;
			return;
		}

		// ƒ‰ƒXƒ^Š„‚è‚İ
		if ((addr == 18) || (addr == 19)) {
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
#if defined(CRTC_LOG)
			LOG2(Log::Normal, "ƒ‰ƒXƒ^Š„‚è‚İ İ’è int=%d rast=%d", crtc.raster_int, crtc.raster_count);
#endif
			if (g_alt_raster_timing) {
				CheckRaster();
			}
			return;
		}

		// ƒeƒLƒXƒgƒXƒNƒ[ƒ‹
		if ((addr >= 20) && (addr <= 23)) {
			crtc.text_scrlx = (crtc.reg[21] << 8) + crtc.reg[20];
			crtc.text_scrlx &= 0x3ff;
			crtc.text_scrly = (crtc.reg[23] << 8) + crtc.reg[22];
			crtc.text_scrly &= 0x3ff;
			hsync = TRUE;

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "ƒeƒLƒXƒgƒXƒNƒ[ƒ‹ x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹
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

		// ƒeƒLƒXƒgVRAM
		if ((addr >= 42) && (addr <= 47)) {
			TextVRAM();
		}
		return;
	}

	// $E80480-$E804FF : “®ìƒ|[ƒg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ãˆÊƒoƒCƒg‚Í‰½‚à‚È‚¢
		if ((addr & 1) == 0) {
			return;
		}

		// ‰ºˆÊƒoƒCƒg‚Íƒ‰ƒXƒ^ƒRƒs[E‚‘¬ƒNƒŠƒA§Œä
		if (data & 0x08) {
#if defined(CRTC_LOG)
			if (!crtc.raster_copy) {
				LOG0(Log::Normal, "ƒ‰ƒXƒ^ƒRƒs[w¦");
			}
#endif	// CRTC_LOG
			crtc.raster_copy = TRUE;
		}
		else {
#if defined(CRTC_LOG)
			if (crtc.raster_copy) {
				LOG0(Log::Normal, "ƒ‰ƒXƒ^ƒRƒs[’â~");
			}
#endif	// CRTC_LOG
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// ‚‘¬ƒNƒŠƒA’†‚Íó‚¯•t‚¯‚È‚¢(ƒGƒgƒ[ƒ‹ƒvƒŠƒ“ƒZƒX)
			if (crtc.fast_clr != 2) {
#if defined(CRTC_LOG)
				if (crtc.fast_clr == 0) {
					LOG0(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒAw¦");
				}
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
			}
		} else {
#if defined(CRTC_LOG)
			if (crtc.fast_clr != 0) {
				LOG0(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒA’â~");
			}
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}
		return;
	}

	LOG2(Log::Warning, "–¢À‘•ƒAƒhƒŒƒX‘‚«‚İ $%06X <- $%02X",
							memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	“Ç‚İ‚İ‚Ì‚İ
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800’PˆÊ‚Åƒ‹[ƒv
	addr &= 0x7ff;

	// $E80000-$E803FF : ƒŒƒWƒXƒ^ƒGƒŠƒA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// “Ç‚İ‚İ(ƒGƒ“ƒfƒBƒAƒ“‚ğ”½“]‚³‚¹‚é)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : “®ìƒ|[ƒg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ãˆÊƒoƒCƒg‚Í0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ‰ºˆÊƒoƒCƒg‚ÍƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒA‚Ì‚İ
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
//	“à•”ƒf[ƒ^æ“¾
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetCRTC(crtc_t *buffer) const
{
	ASSERT(buffer);

	// “à•”ƒf[ƒ^‚ğƒRƒs[
	*buffer = crtc;
}

//---------------------------------------------------------------------------
//
//	ƒCƒxƒ“ƒgƒR[ƒ‹ƒoƒbƒN
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// HSync,HDisp‚Ì2‚Â‚ğŒÄ‚Ñ•ª‚¯‚é
	/// @todo ƒ‰ƒXƒ^A¨B¨C¨A‚ÆƒuƒƒbƒND¨E¨F¨D‚Ì2‚Â‚ÌŠÖ”ƒ|ƒCƒ“ƒ^ŒQ‚Å3‚Â‚Ìó‘Ô‘JˆÚ‚ğ‰ñ‚µğŒ”»’è(Å‘å3‰ñ)‚ğƒ[ƒ‚É‚·‚éB‰¼‘zŠÖ”ƒe[ƒuƒ‹‚Ì’¼Ú‘€ì‚ª—‘z“I
	if (crtc.h_disp < 0) {
		HSync();
	}
	else {
		if (!crtc.h_blockscan) {
			// ƒ‰ƒXƒ^[ƒXƒLƒƒƒ“
			HDispRS();
		} else {
			// ƒuƒƒbƒNƒXƒLƒƒƒ“
			HDispBS();
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
/// H-SYNCŠJn
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	size_t i;
	int hus;

	ASSERT(this);

	// ƒvƒŠƒ“ƒ^‚É’Ê’m(’èŠú“I‚ÉBUSY‚ğ—‚Æ‚·‚½‚ß)
	ASSERT(printer);
	printer->HSync();

	// VC‚É’Ê’m(’x‰„”½‰f)
	vc->HSync();

	// ƒXƒvƒ‰ƒCƒgƒRƒ“ƒgƒ[ƒ‰‚É’Ê’m(‚a‚fİ’è‚Ì’x‰„”½‰f)
	sprite->HSync();

	// HSync‚Å‚ÌXV—v‹
	if (hsync) {
		// ƒtƒ‰ƒOƒIƒt
		hsync = FALSE;

		// ƒeƒLƒXƒgƒXƒNƒ[ƒ‹’l‚Ì’x‰„”½‰f
		render->TextScrl(crtc.text_scrlx, crtc.text_scrly);

		// ƒOƒ‰ƒtƒBƒbƒNƒXƒNƒ[ƒ‹’l‚Ì’x‰„”½‰f
		for (i=0; i<4; i++) {
			render->GrpScrl(i, crtc.grp_scrlx[i], crtc.grp_scrly[i]);
		}
	}

	// ƒtƒ‰ƒOİ’è
	crtc.h_disp = 0;

	// GPIPİ’è
	mfp->SetGPIP(7, 1);

	// ƒXƒLƒƒƒ“ƒ‰ƒCƒ“XV
	crtc.v_scan++;

	if (g_alt_raster_timing) {
		Raster();
		CheckRaster();
	}

	// V-SYNCƒJƒEƒ“ƒg
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANKƒJƒEƒ“ƒg
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// ƒeƒLƒXƒg‰æ–Êƒ‰ƒXƒ^ƒRƒs[
	if (crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// ƒOƒ‰ƒtƒBƒbƒN‰æ–Ê‚‘¬ƒNƒŠƒA
	if (crtc.fast_clr == 2) {
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// Ÿ‚Ìƒ^ƒCƒ~ƒ“ƒO(‘–¸ŠJn)‚Ü‚Å‚ÌŠÔ‚ğİ’è
	crtc.ns += crtc.h_pulse;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);		/// @todo œZ‚ÍŒ™BDDA‚Å‚·‚è’×‚¹Bns‚ÍƒZ[ƒuƒ[ƒh‚Ì‚İŒvZ

	// Ÿ‰ñ—\
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Åˆ«ƒCƒxƒ“ƒg‚ª’â~‚µ‚È‚¢‚æ‚¤‚ÉƒK[ƒh‚·‚é (ƒŒƒAƒP[ƒX)
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);

	// “¯Šúˆ—(40ms‚²‚Æ)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}
}

//---------------------------------------------------------------------------
//
//	ƒ‰ƒXƒ^ƒJƒEƒ“ƒg
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Raster()
{
	// ƒ‰ƒXƒ^ƒJƒEƒ“ƒgXV
	crtc.raster_count++;

	// V-SYNC—§‰º‚è’¼‘O‚ÌH-SYNC—§‰º‚è‚ÅƒNƒŠƒA
	if (crtc.v_synccnt == 1 && crtc.v_disp) {
		crtc.raster_count = 0;
	}
}

//---------------------------------------------------------------------------
//
//	ƒ‰ƒXƒ^Š„‚è‚İƒ`ƒFƒbƒN
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
	if (crtc.raster_count == crtc.raster_int) {
		// —v‹
		mfp->SetGPIP(6, 0);
	} else {
		// æ‚è‰º‚°
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
/// H-DISPŠJn(ƒ‰ƒXƒ^[ƒXƒLƒƒƒ“)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDispRS()
{
	int ns;
	int hus;

	ASSERT(this);

	if (crtc.h_disp == 0) {
		// HDISPŠJn

		// GPIPİ’è
		mfp->SetGPIP(7, 0);

		if (!g_alt_raster_timing) {
			CheckRaster();
			crtc.raster_count++;
		}

		// ƒ‰ƒXƒ^ƒRƒs[‹–‰Â
		crtc.raster_exec = TRUE;

		// Ÿ‚Ìƒ^ƒCƒ~ƒ“ƒO(ƒtƒƒ“ƒgƒ|[ƒ`)‚Ü‚Å‚ÌŠÔ‚ğİ’è
		/// @todo ‚±‚±‚ÍXM6‚É‚¨‚¯‚é’´‚•p“xÀs‰ÓŠB–‘OŒvZ‚µ‚Ä‚¨‚­
		ns = crtc.h_sync - crtc.h_pulse - crtc.h_front;

		// ƒtƒƒ“ƒgƒ|[ƒ`ƒtƒF[ƒY‚ÖˆÚs
		crtc.h_disp = 1;
	} else {
		// ƒtƒƒ“ƒgƒ|[ƒ`

		// I—¹’¼‘O‚Å•`‰æ
		if (!crtc.v_blank) {
			// ƒŒƒ“ƒ_ƒŠƒ“ƒO
			render->HSync(crtc.v_scan, 0);
		}

		// Ÿ‚Ìƒ^ƒCƒ~ƒ“ƒO(H-SYNCŠJn)‚Ü‚Å‚ÌŠÔ‚ğİ’è
		ns = crtc.h_front;

		// •\¦I—¹ƒtƒF[ƒY‚ÖˆÚs
		crtc.h_disp = -1;
	}

	// “ÁêƒP[ƒX‚Å•‰‚É‚È‚é‚Ì‚ÅƒK[ƒh‚·‚é
	/// @todo ‚±‚±‚ÍXM6‚É‚¨‚¯‚é’´‚•p“xÀs‰ÓŠB–‘O”»’è‚µ‚Ä‚¨‚­
	if (ns <= 0) {
		ns = 1;
	}

	crtc.ns += ns;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);

	// Ÿ‰ñ—\
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Åˆ«ƒCƒxƒ“ƒg‚ª’â~‚µ‚È‚¢‚æ‚¤‚ÉƒK[ƒh‚·‚é (ƒŒƒAƒP[ƒX)
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);
}

//---------------------------------------------------------------------------
//
//	H-DISPŠJn(ƒuƒƒbƒNƒXƒLƒƒƒ“)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDispBS()
{
	int dc;
	int ns;
	int hus;

	ASSERT(this);

	// ƒhƒbƒgƒNƒƒbƒN‚ğæ“¾
	dc = Get8DotClock();

	if (crtc.h_disp == 0) {
		// ƒuƒƒbƒNˆÊ’u‰Šú‰»
		crtc.h_blockpos = 0;

		// ƒuƒƒbƒN”Zo
		crtc.h_blocknum = (crtc.reg[0x06] - crtc.reg[0x04]) >> 1;

		// ƒtƒF[ƒYİ’è
		if (crtc.h_blocknum > 0) {
			// •\¦ƒtƒF[ƒY
			crtc.h_disp = 1;
		} else {
			// ƒtƒƒ“ƒgƒ|[ƒ`ƒtƒF[ƒY
			crtc.h_disp = 2;
		}

		// GPIPİ’è
		mfp->SetGPIP(7, 0);

		if (!g_alt_raster_timing) {
			CheckRaster();
			crtc.raster_count++;
		}

		// ƒ‰ƒXƒ^ƒRƒs[‹–‰Â
		crtc.raster_exec = TRUE;

		// ƒoƒbƒNƒ|[ƒ`‚ÌŠúŠÔİ’è
		ns = crtc.h_back;
	} else if (crtc.h_disp == 1) {
		// Ÿ‚ÌƒuƒƒbƒN‚Ö
		crtc.h_blockpos++;

		// ƒtƒƒ“ƒgƒ|[ƒ`ƒtƒF[ƒY‚ÖˆÚs
		if (crtc.h_blockpos >= crtc.h_blocknum) {
			crtc.h_disp = 2;
		}

		// •`‰æ
		if (!crtc.v_blank) {
			// ƒŒƒ“ƒ_ƒŠƒ“ƒO
			render->HSync(crtc.v_scan, crtc.h_blockpos - 1);
		}

		// Ÿ‚Ì16ƒhƒbƒg‚Ü‚Å‚ÌŠÔ‚ğİ’è
		ns = 2 * dc / 100;
	} else {
		// •\¦I—¹ƒtƒF[ƒY‚Ö
		crtc.h_disp = -1;

		// Ÿ‚ÌHSYNC‚Ü‚Å‚ÌŠÔ‚ğİ’è(Œë·‚Í‚±‚±‚Å‹zû)
		ns = crtc.h_sync - crtc.h_pulse - crtc.h_back - crtc.h_front;
		ns -= (2 * dc / 100) * crtc.h_blocknum;
		ns += crtc.h_front;
	}

	// “ÁêƒP[ƒX‚Å•‰‚É‚È‚é‚Ì‚ÅƒK[ƒh‚·‚é
	if (ns <= 0) {
		ns = 1;
	}

	crtc.ns += ns;
	hus = crtc.hus;
	crtc.hus = Ns2Hus(crtc.ns);

	// Ÿ‰ñ—\
	ASSERT(crtc.hus >= (DWORD)hus);
	hus = crtc.hus - hus;
	if (hus <= 0) {
		// Åˆ«ƒCƒxƒ“ƒg‚ª’â~‚µ‚È‚¢‚æ‚¤‚ÉƒK[ƒh‚·‚é (ƒŒƒAƒP[ƒX)
		ASSERT(hus == 0);
		crtc.hus++;
		hus = 1;
	}
	event.SetTimeFast(hus);
}

//---------------------------------------------------------------------------
//
/// V-SYNCŠJn
///
/// V-DISPŠJn‚ğŠÜ‚ŞB
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// V-SYNCI—¹‚È‚ç
	if (!crtc.v_disp) {
		// ƒtƒ‰ƒOİ’è
		crtc.v_disp = TRUE;

		// ŠÔİ’è
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);

		// ƒCƒ“ƒ^ƒŒ[ƒXƒ‚[ƒh‚È‚ç‚Îƒ_ƒ~[‚Ì‚Pƒ‰ƒCƒ“‚ğ’Ç‰Á
		if ((crtc.lowres && crtc.vd > 0) || (!crtc.lowres && crtc.vd > 1)) {
			if (!crtc.v_scaneven) {
				crtc.v_synccnt++;
			}
		}

		return;
	}

	// ƒŒƒ“ƒ_ƒ‰‡¬I—¹
	render->EndFrame();
#if XM6_RENDER_SYNC == 1
	if (m_pScheduler) {
		m_pScheduler->UpdateFrame();
	}
#endif	// XM6_RENDER_SYNC == 1
	crtc.v_scan = crtc.v_dots + 1;

	// ‰ğ‘œ“x•ÏX‚ª‚ ‚ê‚ÎA‚±‚±‚Å•ÏX
	if (crtc.changed) {
		ReCalc();

		// ƒtƒ‰ƒO‚¨‚ë‚·
		crtc.changed = FALSE;
	}

	// V-SYNCI—¹‚Ü‚Å‚ÌŠÔ‚ğİ’è
	crtc.v_synccnt = crtc.v_pulse;

	// V-BLANK‚Ìó‘Ô‚ÆAŠÔ‚ğİ’è
	if (crtc.v_front <= 0) {
		// ‚Ü‚¾•\¦’†(“Áê)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// ‚·‚Å‚Éƒuƒ‰ƒ“ƒN’†(’Êí)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// ƒtƒ‰ƒOİ’è
	crtc.v_disp = FALSE;
	if (!g_alt_raster_timing) {
		crtc.raster_count = 0;
	}

	// ƒCƒ“ƒ^ƒŒ[ƒXƒ‚[ƒh‹ô”ƒtƒ‰ƒO”½“]
	crtc.v_scaneven = !crtc.v_scaneven;
}

//---------------------------------------------------------------------------
//
/// ÄŒvZ
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);

	// CRTCƒŒƒWƒXƒ^0‚ªƒNƒŠƒA‚³‚ê‚Ä‚¢‚ê‚ÎA–³Œø(MacƒGƒ~ƒ…ƒŒ[ƒ^)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "ÄŒvZ");
#endif	// CRTC_LOG

		// ƒhƒbƒgƒNƒƒbƒN‚ğæ“¾
		dc = Get8DotClock();

		// …•½(‚·‚×‚Äns’PˆÊ)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// ‚’¼(‚·‚×‚ÄH-Sync’PˆÊ)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// V-FRONT‚ªƒ}ƒCƒiƒX‚·‚¬‚éê‡‚ÍA1…•½ŠúŠÔ•ª‚Ì‚İ(ƒwƒ‹ƒnƒEƒ“ƒhAƒRƒbƒgƒ“)
		if (crtc.v_front <= 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// ƒhƒbƒg”‚ğZo
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

	// ”{—¦İ’è(…•½)
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "‰¡ƒhƒbƒg”50MHzƒ‚[ƒh(CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// ”{—¦İ’è(‚’¼)
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// ƒCƒ“ƒ^ƒŒ[ƒX1024dotƒ‚[ƒh
			crtc.v_mul = 0;
		}
		else {
			// ƒCƒ“ƒ^ƒŒ[ƒXA’Êí512ƒ‚[ƒh(x1)A”{256dotƒ‚[ƒh(x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// ’Êí‚Ì256dotƒ‚[ƒh(x2)
			crtc.v_mul = 2;
		}
		else {
			// ƒCƒ“ƒ^ƒŒ[ƒX512dotƒ‚[ƒh(x1)
			crtc.v_mul = 0;
		}
	}

	// crtc.hd‚ª2ˆÈã‚Ìê‡AƒXƒvƒ‰ƒCƒg‚ÍØ‚è—£‚³‚ê‚é
	if (crtc.hd >= 2) {
		// 768x512 or VGAƒ‚[ƒh(ƒXƒvƒ‰ƒCƒg‚È‚µ)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512ƒ‚[ƒh(ƒXƒvƒ‰ƒCƒg‚ ‚è)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// ƒCƒ“ƒ^ƒŒ[ƒX‹ôŠïƒtƒ‰ƒO‰Šú‰»
	crtc.v_scaneven = FALSE;

	// ƒŒƒ“ƒ_ƒ‰‚Ö’Ê’m
	render->SetCRTC();
}

//---------------------------------------------------------------------------
//
//	V-BLANKŠJn(V-SCREENŠJn‚ğŠÜ‚Ş)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// •\¦’†‚Å‚ ‚ê‚ÎAƒuƒ‰ƒ“ƒNŠJn
	if (!crtc.v_blank) {
		// ƒuƒ‰ƒ“ƒN‹æŠÔ‚ğİ’è
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
//		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// ƒtƒ‰ƒO
		crtc.v_blank = TRUE;

		// GPIPƒCƒxƒ“ƒgƒJƒEƒ“ƒg
		mfp->EventCount(0, 0);

		// GPIP’Ê’m
		mfp->SetGPIP(4, 0);

		// ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒA
		if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒAI—¹");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}

		return;
	}

	// •\¦‹æŠÔ‚ğİ’è
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= crtc.v_pulse + crtc.v_back + crtc.v_front;

	// ƒtƒ‰ƒO
	crtc.v_blank = FALSE;

	// GPIPƒCƒxƒ“ƒgƒJƒEƒ“ƒg
	mfp->EventCount(0, 1);

	// GPIP’Ê’m
	mfp->SetGPIP(4, 1);

	// ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒA
	// V-SYNCI—¹‚©‚çV-DISPŠJn‚Ü‚Å‚É‰ğ‘œ“x•ÏX‚ÆƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒAw¦‚·‚é‚ÆA
	// ’¼Œã‚ÌV-DISP‚Å‚ÍƒNƒŠƒA“®ì‚ªŠJn‚¹‚¸‚ÉŸ‚ÌV-DISP‚Ü‚Å‘Ò‚½‚³‚ê‚é(ƒiƒCƒAƒX)
	if (!crtc.changed && crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "ƒOƒ‰ƒtƒBƒbƒN‚‘¬ƒNƒŠƒAŠJn data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
	}

	// ƒŒƒ“ƒ_ƒ‰‡¬ŠJnAƒJƒEƒ“ƒ^ƒAƒbƒv
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
}

//---------------------------------------------------------------------------
//
//	•\¦ü”g”æ“¾
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// assert
	ASSERT(h);
	ASSERT(v);

	// ƒ`ƒFƒbƒN
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

	// ƒCƒ“ƒ^ƒŒ[ƒXƒ‚[ƒh‚Í‚’¼“¯ŠúŠúŠÔ‚ª
	// …•½“¯ŠúŠúŠÔ‚Ì”¼•ªˆø‚«‰„‚Î‚³‚ê‚é
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
//	8ƒhƒbƒgƒNƒƒbƒN‚ğæ“¾(~100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// HF, HD‚ğCRTC R20‚æ‚èæ“¾
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// ƒCƒ“ƒfƒbƒNƒXì¬
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8ƒhƒbƒgƒNƒƒbƒNƒe[ƒuƒ‹
//	(HRL,HF,HD‚©‚ç“¾‚ç‚ê‚é’lB0.01ns’PˆÊ)
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
//	HRLİ’è
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::SetHRL(BOOL flag)
{
	if (crtc.hrl != flag) {
		// Ÿ‚ÌüŠú‚ÅÄŒvZ
		crtc.hrl = flag;
		crtc.changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	HRLæ“¾
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::GetHRL() const
{
	return crtc.hrl;
}

//---------------------------------------------------------------------------
//
//	ƒeƒLƒXƒgVRAMŒø‰Ê
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// “¯ƒAƒNƒZƒX
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4‚Íƒ}ƒ‹ƒ`ƒtƒ‰ƒO
		b |= 0x10;
		tvram->SetMulti(b);
	}
	else {
		tvram->SetMulti(0);
	}

	// ƒAƒNƒZƒXƒ}ƒXƒN
	if (crtc.reg[43] & 2) {
		w = (DWORD)crtc.reg[47];
		w <<= 8;
		w |= (DWORD)crtc.reg[46];
		tvram->SetMask(w);
	}
	else {
		tvram->SetMask(0);
	}

	// ƒ‰ƒXƒ^ƒRƒs[
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
