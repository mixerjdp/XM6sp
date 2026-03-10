//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 éoéhüD(ytanaka@ipc-tokai.or.jp)
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

//===========================================================================
//
//	CRTC
//
//===========================================================================
//#define CRTC_LOG

//---------------------------------------------------------------------------
//
//	âRâôâXâgâëâNâ^
//
//---------------------------------------------------------------------------
CRTC::CRTC(VM *p) : MemDevice(p)
{
	// âfâoâCâXIDé­Åëè·ë╗
	dev.id = MAKEID('C', 'R', 'T', 'C');
	dev.desc = "CRTC (VICON)";

	// èJÄnâAâhâîâXüAÅIù╣âAâhâîâX
	memdev.first = 0xe80000;
	memdev.last = 0xe81fff;

	// é╗é╠æ╝âÅü[âN
	tvram = NULL;
	gvram = NULL;
	sprite = NULL;
	mfp = NULL;
	render = NULL;
	printer = NULL;
}

//---------------------------------------------------------------------------
//
//	Åëè·ë╗
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Init()
{
	ASSERT(this);

	// è¯û{âNâëâX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// âeâLâXâgVRAMé­Äµô¥
	tvram = (TVRAM*)vm->SearchDevice(MAKEID('T', 'V', 'R', 'M'));
	ASSERT(tvram);

	// âOâëâtâBâbâNVRAMé­Äµô¥
	gvram = (GVRAM*)vm->SearchDevice(MAKEID('G', 'V', 'R', 'M'));
	ASSERT(gvram);

	// âXâvâëâCâgâRâôâgâìü[âëé­Äµô¥
	sprite = (Sprite*)vm->SearchDevice(MAKEID('S', 'P', 'R', ' '));
	ASSERT(sprite);

	// MFPé­Äµô¥
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// âîâôâ_âëé­Äµô¥
	render = (Render*)vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	ASSERT(render);

	// âvâèâôâ^é­Äµô¥
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	// âCâxâôâgÅëè·ë╗
	event.SetDevice(this);
	event.SetDesc("H-Sync");
	event.SetTime(0);
	scheduler->AddEvent(&event);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	âNâèü[âôâAâbâv
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Cleanup()
{
	ASSERT(this);

	// è¯û{âNâëâXéÍ
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	âèâZâbâg
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "âèâZâbâg");

	// âîâWâXâ^é­âNâèâA
	memset(crtc.reg, 0, sizeof(crtc.reg));
	for (i=0; i<18; i++) {
		crtc.reg[i] = ResetTable[i];
	}
	for (i=0; i<8; i++) {
		crtc.reg[i + 0x28] = ResetTable[i + 18];
	}

	// ë­æ£ôx
	crtc.hrl = FALSE;
	crtc.lowres = FALSE;
	crtc.textres = TRUE;
	crtc.changed = FALSE;

	// ô┴ÄÛï@ö
	crtc.raster_count = 0;
	crtc.raster_int = 0;
	crtc.raster_copy = FALSE;
	crtc.raster_exec = FALSE;
	crtc.fast_clr = 0;

	// Éàò¢
	crtc.h_sync = 31745;
	crtc.h_pulse = 3450;
	crtc.h_back = 4140;
	crtc.h_front = 2070;
	crtc.h_dots = 768;
	crtc.h_mul = 1;
	crtc.hd = 2;

	// ÉéÆ╝
	crtc.v_sync = 568;
	crtc.v_pulse = 6;
	crtc.v_back = 35;
	crtc.v_front = 15;
	crtc.v_dots = 512;
	crtc.v_mul = 1;
	crtc.vd = 1;

	// âCâxâôâg
	crtc.ns = 0;
	crtc.hus = 0;
	crtc.v_synccnt = 1;
	crtc.v_blankcnt = 1;
	crtc.h_disp = TRUE;
	crtc.v_disp = TRUE;
	crtc.v_blank = TRUE;
	crtc.v_count = 0;
	crtc.v_scan = 0;

	// ê╚ë║éóéþé╚éó
	crtc.h_disptime = 0;
	crtc.h_synctime = 0;
	crtc.v_cycletime = 0;
	crtc.v_blanktime = 0;
	crtc.v_backtime = 0;
	crtc.v_synctime = 0;

	// âüâéâèâéü[âh
	crtc.tmem = FALSE;
	crtc.gmem = TRUE;
	crtc.siz = 0;
	crtc.col = 3;

	// âXâNâìü[âï
	crtc.text_scrlx = 0;
	crtc.text_scrly = 0;
	for (i=0; i<4; i++) {
		crtc.grp_scrlx[i] = 0;
		crtc.grp_scrly[i] = 0;
	}

	// H-SyncâCâxâôâgé­É¦ÆÞ(31.5us)
	event.SetTime(63);
}

//---------------------------------------------------------------------------
//
//	CRTCâèâZâbâgâfü[â^
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
//	âZü[âu
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "âZü[âu");

	// âTâCâYé­âZü[âu
	sz = sizeof(crtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Ä└æ╠é­âZü[âu
	if (!fio->Write(&crtc, (int)sz)) {
		return FALSE;
	}

	// âCâxâôâgé­âZü[âu
	if (!event.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	âìü[âh
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	LOG0(Log::Normal, "âìü[âh");

	// âTâCâYé­âìü[âh
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(crtc_t)) {
		return FALSE;
	}

	// Ä└æ╠é­âìü[âh
	if (!fio->Read(&crtc, (int)sz)) {
		return FALSE;
	}

	// âCâxâôâgé­âìü[âh
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	// âîâôâ_âëéÍÆ╩Æm
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
//	É¦ÆÞôKùp
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "É¦ÆÞôKùp");
}

//---------------------------------------------------------------------------
//
//	âoâCâgôÃé¦ì×é¦
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadByte(DWORD addr)
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800ÆPê╩é┼âïü[âv
	addr &= 0x7ff;

	// âEâFâCâg
	scheduler->Wait(1);

	// $E80000-$E803FF : âîâWâXâ^âGâèâA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// R20, R21é╠é¦ôÃé¦Åæé½ë┬ö\üBé╗éÛê╚èOé═$00
		if ((addr < 40) || (addr > 43)) {
			return 0;
		}

		// ôÃé¦ì×é¦(âGâôâfâBâAâôé­ö¢ô]é│é╣éÚ)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : ô«ìýâ|ü[âg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ÅÒê╩âoâCâgé═ 0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ë║ê╩âoâCâgé═âëâXâ^âRâsü[üAâOâëâtâBâbâNìéæ¼âNâèâAé╠é¦
		data = 0;
		if (crtc.raster_copy) {
			data |= 0x08;
		}
		if (crtc.fast_clr == 2) {
			data |= 0x02;
		}
		return data;
	}

	LOG1(Log::Warning, "ûóÄ└æòâAâhâîâXôÃé¦ì×é¦ $%06X", memdev.first + addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	âoâCâgÅæé½ì×é¦
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::WriteByte(DWORD addr, DWORD data)
{
	int reg;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800ÆPê╩é┼âïü[âv
	addr &= 0x7ff;

	// âEâFâCâg
	scheduler->Wait(1);

	// $E80000-$E803FF : âîâWâXâ^âGâèâA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return;
		}

		// Åæé½ì×é¦(âGâôâfâBâAâôé­ö¢ô]é│é╣éÚ)
		addr ^= 1;
		if (crtc.reg[addr] == data) {
			return;
		}
		crtc.reg[addr] = (BYTE)data;

		// GVRAMâAâhâîâXì\É¼
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

			// âOâëâtâBâbâNVRAMéÍÆ╩Æm
			gvram->SetType(data & 0x0f);
			return;
		}

		// ë­æ£ôxò¤ìX
		if ((addr <= 15) || (addr == 40)) {
			// âXâvâëâCâgâüâéâèé╠É┌æ▒üEÉÏÆfé═ÅuÄ×é╔ìséñ(OS-9/68000)
			if (addr == 0x28) {
				if ((crtc.reg[0x28] & 3) >= 2) {
					sprite->Connect(FALSE);
				}
				else {
					sprite->Connect(TRUE);
				}
			}

			// Äƒé╠Ä³è·é┼ì─îvÄZ
			crtc.changed = TRUE;
			return;
		}

		// âëâXâ^èäéÞì×é¦
		if ((addr == 18) || (addr == 19)) {
			crtc.raster_int = (crtc.reg[19] << 8) + crtc.reg[18];
			crtc.raster_int &= 0x3ff;
			CheckRaster();
			return;
		}

		// âeâLâXâgâXâNâìü[âï
		if ((addr >= 20) && (addr <= 23)) {
			crtc.text_scrlx = (crtc.reg[21] << 8) + crtc.reg[20];
			crtc.text_scrlx &= 0x3ff;
			crtc.text_scrly = (crtc.reg[23] << 8) + crtc.reg[22];
			crtc.text_scrly &= 0x3ff;
			render->TextScrl(crtc.text_scrlx, crtc.text_scrly);

#if defined(CRTC_LOG)
			LOG2(Log::Normal, "âeâLâXâgâXâNâìü[âï x=%d y=%d", crtc.text_scrlx, crtc.text_scrly);
#endif	// CRTC_LOG
			return;
		}

		// âOâëâtâBâbâNâXâNâìü[âï
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

		// âeâLâXâgVRAM
		if ((addr >= 42) && (addr <= 47)) {
			TextVRAM();
		}
		return;
	}

	// $E80480-$E804FF : ô«ìýâ|ü[âg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ÅÒê╩âoâCâgé═ë¢éÓé╚éó
		if ((addr & 1) == 0) {
			return;
		}

		// ë║ê╩âoâCâgé═âëâXâ^âRâsü[üEìéæ¼âNâèâAÉºîõ
		if (data & 0x08) {
			crtc.raster_copy = TRUE;
		}
		else {
			crtc.raster_copy = FALSE;
		}
		if (data & 0x02) {
			// âëâXâ^âRâsü[éãïñùpüAâëâXâ^âRâsü[ùDÉµ(æÕÉÝù¬III'90)
			if ((crtc.fast_clr == 0) && !crtc.raster_copy) {
#if defined(CRTC_LOG)
				LOG0(Log::Normal, "âOâëâtâBâbâNìéæ¼âNâèâAÄwÄª");
#endif	// CRTC_LOG
				crtc.fast_clr = 1;
			}
#if defined(CRTC_LOG)
			else {
				LOG1(Log::Normal, "âOâëâtâBâbâNìéæ¼âNâèâAÄwÄªû│î° State=%d", crtc.fast_clr);
			}
#endif	//CRTC_LOG
		}
		return;
	}

	LOG2(Log::Warning, "ûóÄ└æòâAâhâîâXÅæé½ì×é¦ $%06X <- $%02X",
							memdev.first + addr, data);
}

//---------------------------------------------------------------------------
//
//	ôÃé¦ì×é¦é╠é¦
//
//---------------------------------------------------------------------------
DWORD FASTCALL CRTC::ReadOnly(DWORD addr) const
{
	BYTE data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// $800ÆPê╩é┼âïü[âv
	addr &= 0x7ff;

	// $E80000-$E803FF : âîâWâXâ^âGâèâA
	if (addr < 0x400) {
		addr &= 0x3f;
		if (addr >= 0x30) {
			return 0xff;
		}

		// ôÃé¦ì×é¦(âGâôâfâBâAâôé­ö¢ô]é│é╣éÚ)
		addr ^= 1;
		return crtc.reg[addr];
	}

	// $E80480-$E804FF : ô«ìýâ|ü[âg
	if ((addr >= 0x480) && (addr <= 0x4ff)) {
		// ÅÒê╩âoâCâgé═0
		if ((addr & 1) == 0) {
			return 0;
		}

		// ë║ê╩âoâCâgé═âOâëâtâBâbâNìéæ¼âNâèâAé╠é¦
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
//	ôÓòöâfü[â^Äµô¥
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetCRTC(crtc_t *buffer) const
{
	ASSERT(buffer);

	// ôÓòöâfü[â^é­âRâsü[
	*buffer = crtc;
}

//---------------------------------------------------------------------------
//
//	âCâxâôâgâRü[âïâoâbâN
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// HSync,HDispé╠2é┬é­î─éÐò¬é»éÚ
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
//	H-SYNCèJÄn
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HSync()
{
	int hus;

	ASSERT(this);

	// âvâèâôâ^é╔Æ╩Æm(ÆÞè·ôIé╔BUSYé­ùÄéãéÀé¢é▀)
	ASSERT(printer);
	printer->HSync();

	// V-SYNCâJâEâôâg
	crtc.v_synccnt--;
	if (crtc.v_synccnt == 0) {
		VSync();
	}

	// V-BLANKâJâEâôâg
	crtc.v_blankcnt--;
	if (crtc.v_blankcnt == 0) {
		VBlank();
	}

	// Äƒé╠â^âCâ~âôâO(H-DISPèJÄn)é▄é┼é╠Ä×èÈé­É¦ÆÞ
	crtc.ns += crtc.h_pulse;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// ô»è·ÅêùØ(40msé▓éã)
	if (crtc.hus >= 80000) {
		crtc.hus -= 80000;
		ASSERT(crtc.ns >= 40000000);
		crtc.ns -= 40000000;
	}

	// âtâëâOÉ¦ÆÞ
	crtc.h_disp = FALSE;

	// GPIPÉ¦ÆÞ
	mfp->SetGPIP(7, 1);

	// ò`ëµ
	crtc.v_scan++;
	if (!crtc.v_blank) {
		// âîâôâ_âèâôâO
		render->HSync(crtc.v_scan);
	}

	// âëâXâ^èäéÞì×é¦
#if 0
	CheckRaster();
	crtc.raster_count++;
#endif

	// âeâLâXâgëµû╩âëâXâ^âRâsü[
	if (crtc.raster_copy && crtc.raster_exec) {
		tvram->RasterCopy();
		crtc.raster_exec = FALSE;
	}

	// âOâëâtâBâbâNëµû╩ìéæ¼âNâèâA
	if (crtc.fast_clr == 2) {
		gvram->FastClr(&crtc);
	}
}

//---------------------------------------------------------------------------
//
//	H-DISPèJÄn
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::HDisp()
{
	int ns;
	int hus;

	ASSERT(this);

#if 1
	// âëâXâ^èäéÞì×é¦
	CheckRaster();
	crtc.raster_count++;
#endif

	// Äƒé╠â^âCâ~âôâO(H-SYNCèJÄn)é▄é┼é╠Ä×èÈé­É¦ÆÞ
	ns = crtc.h_sync - crtc.h_pulse;
	ASSERT(ns > 0);
	crtc.ns += ns;
	hus = Ns2Hus(crtc.ns);
	hus -= crtc.hus;
	event.SetTime(hus);
	crtc.hus += hus;

	// âtâëâOÉ¦ÆÞ
	crtc.h_disp = TRUE;

	// GPIPÉ¦ÆÞ
	mfp->SetGPIP(7,0);

	// âëâXâ^âRâsü[ïûë┬
	crtc.raster_exec = TRUE;
}

//---------------------------------------------------------------------------
//
//	V-SYNCèJÄn(V-DISPèJÄné­è▄éÌ)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VSync()
{
	ASSERT(this);

	// V-SYNCÅIù╣é╚éþ
	if (!crtc.v_disp) {
		// âtâëâOÉ¦ÆÞ
		crtc.v_disp = TRUE;

		// Ä×èÈÉ¦ÆÞ
		crtc.v_synccnt = (crtc.v_sync - crtc.v_pulse);
		return;
	}

	// ë­æ£ôxò¤ìXé¬éáéÛé╬üAé▒é▒é┼ò¤ìX
	if (crtc.changed) {
		ReCalc();
	}

	// V-SYNCÅIù╣é▄é┼é╠Ä×èÈé­É¦ÆÞ
	crtc.v_synccnt = crtc.v_pulse;

	// V-BLANKé╠Å¾æÈéãüAÄ×èÈé­É¦ÆÞ
	if (crtc.v_front < 0) {
		// é▄é¥ò\ÄªÆå(ô┴ÄÛ)
		crtc.v_blank = FALSE;
		crtc.v_blankcnt = (-crtc.v_front) + 1;
	}
	else {
		// éÀé┼é╔âuâëâôâNÆå(Æ╩ÅÝ)
		crtc.v_blank = TRUE;
		crtc.v_blankcnt = (crtc.v_pulse + crtc.v_back + 1);
	}

	// âtâëâOÉ¦ÆÞ
	crtc.v_disp = FALSE;

	// âëâXâ^âJâEâôâgÅëè·ë╗
	crtc.raster_count = 0;
}

//---------------------------------------------------------------------------
//
//	ì─îvÄZ
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::ReCalc()
{
	int dc;
	int over;
	WORD *p;

	ASSERT(this);
	ASSERT(crtc.changed);

	// CRTCâîâWâXâ^0é¬âNâèâAé│éÛé─éóéÛé╬üAû│î°(MacâGâ~âàâîü[â^)
	if (crtc.reg[0x0] != 0) {
#if defined(CRTC_LOG)
		LOG0(Log::Normal, "ì─îvÄZ");
#endif	// CRTC_LOG

		// âhâbâgâNâìâbâNé­Äµô¥
		dc = Get8DotClock();

		// Éàò¢(éÀéÎé─nsÆPê╩)
		crtc.h_sync = (crtc.reg[0x0] + 1) * dc / 100;
		crtc.h_pulse = (crtc.reg[0x02] + 1) * dc / 100;
		crtc.h_back = (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1) * dc / 100;
		crtc.h_front = (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5) * dc / 100;

		// ÉéÆ╝(éÀéÎé─H-SyncÆPê╩)
		p = (WORD *)crtc.reg;
		crtc.v_sync = ((p[4] & 0x3ff) + 1);
		crtc.v_pulse = ((p[5] & 0x3ff) + 1);
		crtc.v_back = ((p[6] & 0x3ff) + 1) - crtc.v_pulse;
		crtc.v_front = crtc.v_sync - ((p[7] & 0x3ff) + 1);

		// V-FRONTé¬â}âCâiâXéÀé¼éÚÅÛìçé═üA1Éàò¢è·èÈò¬é╠é¦(âwâïânâEâôâhüAâRâbâgâô)
		if (crtc.v_front < 0) {
			over = -crtc.v_front;
			over -= crtc.v_back;
			if (over >= crtc.v_pulse) {
				crtc.v_front = -1;
			}
		}

		// âhâbâgÉöé­ÄZÅo
		crtc.h_dots = (crtc.reg[0x0] + 1);
		crtc.h_dots -= (crtc.reg[0x02] + 1);
		crtc.h_dots -= (crtc.reg[0x04] + 5 - crtc.reg[0x02] - 1);
		crtc.h_dots -= (crtc.reg[0x0] + 1 - crtc.reg[0x06] - 5);
		crtc.h_dots *= 8;
		crtc.v_dots = crtc.v_sync - crtc.v_pulse - crtc.v_back - crtc.v_front;
	}

	// ö{ùªÉ¦ÆÞ(Éàò¢)
	crtc.hd = (crtc.reg[0x28] & 3);
	if (crtc.hd == 3) {
		LOG0(Log::Warning, "ëíâhâbâgÉö50MHzâéü[âh(CompactXVI)");
	}
	if (crtc.hd == 0) {
		crtc.h_mul = 2;
	}
	else {
		crtc.h_mul = 1;
	}

	// crtc.hdé¬2ê╚ÅÒé╠ÅÛìçüAâXâvâëâCâgé═ÉÏéÞùúé│éÛéÚ
	if (crtc.hd >= 2) {
		// 768x512 or VGAâéü[âh(âXâvâëâCâgé╚éÁ)
		sprite->Connect(FALSE);
		crtc.textres = TRUE;
	}
	else {
		// 256x256 or 512x512âéü[âh(âXâvâëâCâgéáéÞ)
		sprite->Connect(TRUE);
		crtc.textres = FALSE;
	}

	// ö{ùªÉ¦ÆÞ(ÉéÆ╝)
	crtc.vd = (crtc.reg[0x28] >> 2) & 3;
	if (crtc.reg[0x28] & 0x10) {
		// 31kHz
		crtc.lowres = FALSE;
		if (crtc.vd == 3) {
			// âCâôâ^âîü[âX1024dotâéü[âh
			crtc.v_mul = 0;
		}
		else {
			// âCâôâ^âîü[âXüAÆ╩ÅÝ512âéü[âh(x1)üAö{256dotâéü[âh(x2)
			crtc.v_mul = 2 - crtc.vd;
		}
	}
	else {
		// 15kHz
		crtc.lowres = TRUE;
		if (crtc.vd == 0) {
			// Æ╩ÅÝé╠256dotâéü[âh(x2)
			crtc.v_mul = 2;
		}
		else {
			// âCâôâ^âîü[âX512dotâéü[âh(x1)
			crtc.v_mul = 0;
		}
	}

	// âîâôâ_âëéÍÆ╩Æm
	render->SetCRTC();

	// âtâëâOé¿éÙéÀ
	crtc.changed = FALSE;
}


//---------------------------------------------------------------------------
//
//	V-BLANKèJÄn(V-SCREENèJÄné­è▄éÌ)
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::VBlank()
{
	ASSERT(this);

	// ò\ÄªÆåé┼éáéÛé╬üAâuâëâôâNèJÄn
	if (!crtc.v_blank) {
		// âuâëâôâNïµèÈé­É¦ÆÞ
		crtc.v_blankcnt = crtc.v_pulse + crtc.v_back + crtc.v_front;
		ASSERT((crtc.v_front < 0) || ((int)crtc.v_synccnt == crtc.v_front));

		// âtâëâO
		crtc.v_blank = TRUE;

		// GPIP
		mfp->EventCount(0, 0);
		mfp->SetGPIP(4, 0);

		// âOâëâtâBâbâNìéæ¼âNâèâA
		if (crtc.fast_clr == 2) {
#if defined(CRTC_LOG)
			LOG0(Log::Normal, "âOâëâtâBâbâNìéæ¼âNâèâAÅIù╣");
#endif	// CRTC_LOG
			crtc.fast_clr = 0;
		}

		// âîâôâ_âëìçÉ¼ÅIù╣
		render->EndFrame();
		crtc.v_scan = crtc.v_dots + 1;
		return;
	}

	// ò\ÄªïµèÈé­É¦ÆÞ
	crtc.v_blankcnt = crtc.v_sync;
	crtc.v_blankcnt -= (crtc.v_pulse + crtc.v_back + crtc.v_front);

	// âtâëâO
	crtc.v_blank = FALSE;

	// GPIP
	mfp->EventCount(0, 1);
	mfp->SetGPIP(4, 1);

	// âOâëâtâBâbâNìéæ¼âNâèâA
	if (crtc.fast_clr == 1) {
#if defined(CRTC_LOG)
		LOG1(Log::Normal, "âOâëâtâBâbâNìéæ¼âNâèâAèJÄn data=%02X", crtc.reg[42]);
#endif	// CRTC_LOG
		crtc.fast_clr = 2;
		gvram->FastSet((DWORD)crtc.reg[42]);
		gvram->FastClr(&crtc);
	}

	// âîâôâ_âëìçÉ¼èJÄnüAâJâEâôâ^âAâbâv
	crtc.v_scan = 0;
	render->StartFrame();
	crtc.v_count++;
}

//---------------------------------------------------------------------------
//
//	ò\ÄªÄ³ögÉöÄµô¥
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::GetHVHz(DWORD *h, DWORD *v) const
{
	DWORD d;
	DWORD t;

	// assert
	ASSERT(h);
	ASSERT(v);

	// â`âFâbâN
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
//	8âhâbâgâNâìâbâNé­Äµô¥(ü~100)
//
//---------------------------------------------------------------------------
int FASTCALL CRTC::Get8DotClock() const
{
	int hf;
	int hd;
	int index;

	ASSERT(this);

	// HF, HDé­CRTC R20éµéÞÄµô¥
	hf = (crtc.reg[0x28] >> 4) & 1;
	hd = (crtc.reg[0x28] & 3);

	// âCâôâfâbâNâXìýÉ¼
	index = hf * 4 + hd;
	if (crtc.hrl) {
		index += 8;
	}

	return DotClockTable[index];
}

//---------------------------------------------------------------------------
//
//	8âhâbâgâNâìâbâNâeü[âuâï
//	(HRL,HF,HDé®éþô¥éþéÛéÚÆlüB0.01nsÆPê╩)
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
//	HRLÉ¦ÆÞ
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::SetHRL(BOOL flag)
{
	if (crtc.hrl != flag) {
		// Äƒé╠Ä³è·é┼ì─îvÄZ
		crtc.hrl = flag;
		crtc.changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	HRLÄµô¥
//
//---------------------------------------------------------------------------
BOOL FASTCALL CRTC::GetHRL() const
{
	return crtc.hrl;
}

//---------------------------------------------------------------------------
//
//	âëâXâ^èäéÞì×é¦â`âFâbâN
//	üªâCâôâ^âîü[âXâéü[âhé╔é═ûóæ╬ë×
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::CheckRaster()
{
#if 1
	if (crtc.raster_count == crtc.raster_int) {
#else
	if (crtc.raster_count == crtc.raster_int) {
#endif
		// ùvïü
		mfp->SetGPIP(6, 0);
#if defined(CRTC_LOG)
		LOG2(Log::Normal, "âëâXâ^èäéÞì×é¦ùvïü raster=%d scan=%d", crtc.raster_count, crtc.v_scan);
#endif	// CRTC_LOG
	}
	else {
		// ÄµéÞë║é░
		mfp->SetGPIP(6, 1);
	}
}

//---------------------------------------------------------------------------
//
//	âeâLâXâgVRAMî°ë╩
//
//---------------------------------------------------------------------------
void FASTCALL CRTC::TextVRAM()
{
	DWORD b;
	DWORD w;

	// ô»Ä×âAâNâZâX
	if (crtc.reg[43] & 1) {
		b = (DWORD)crtc.reg[42];
		b >>= 4;

		// b4é═â}âïâ`âtâëâO
		b |= 0x10;
		tvram->SetMulti(b);
	}
	else {
		tvram->SetMulti(0);
	}

	// âAâNâZâXâ}âXâN
	if (crtc.reg[43] & 2) {
		w = (DWORD)crtc.reg[47];
		w <<= 8;
		w |= (DWORD)crtc.reg[46];
		tvram->SetMask(w);
	}
	else {
		tvram->SetMask(0);
	}

	// âëâXâ^âRâsü[
	tvram->SetCopyRaster((DWORD)crtc.reg[45], (DWORD)crtc.reg[44],
						(DWORD)(crtc.reg[42] & 0x0f));
}

