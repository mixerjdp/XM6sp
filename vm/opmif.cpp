//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ‚o‚hD(ytanaka@ipc-tokai.or.jp)
//	[ OPM(YM2151) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "mfp.h"
#include "schedule.h"
#include "adpcm.h"
#include "fdd.h"
#include "fileio.h"
#include "config.h"
#include "opm.h"
#include "opmif.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	OPM
//
//===========================================================================
//#define OPM_LOG

//---------------------------------------------------------------------------
//
//	ƒRƒ“ƒXƒgƒ‰ƒNƒ^
//
//---------------------------------------------------------------------------
OPMIF::OPMIF(VM *p) : MemDevice(p)
{
	// ƒfƒoƒCƒXID‚ð‰Šú‰»
	dev.id = MAKEID('O', 'P', 'M', ' ');
	dev.desc = "OPM (YM2151)";

	// ŠJŽnƒAƒhƒŒƒXAI—¹ƒAƒhƒŒƒX
	memdev.first = 0xe90000;
	memdev.last = 0xe91fff;

	// ƒ[ƒNƒNƒŠƒA
	mfp = NULL;
	fdd = NULL;
	adpcm = NULL;
	engine = NULL;
	opmbuf = NULL;
}

//---------------------------------------------------------------------------
//
//	‰Šú‰»
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Init()
{
	int i;

	ASSERT(this);

	// Šî–{ƒNƒ‰ƒX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// MFPŽæ“¾
	ASSERT(!mfp);
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// ADPCMŽæ“¾
	ASSERT(!adpcm);
	adpcm = (ADPCM*)vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
	ASSERT(adpcm);

	// FDDŽæ“¾
	ASSERT(!fdd);
	fdd = (FDD*)vm->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ASSERT(fdd);

	// ƒCƒxƒ“ƒgì¬
	event[0].SetDevice(this);
	event[0].SetDesc("Timer-A");
	event[1].SetDevice(this);
	event[1].SetDesc("Timer-B");
	for (i=0; i<2; i++) {
		event[i].SetUser(i);
		event[i].SetTime(0);
		scheduler->AddEvent(&event[i]);
	}

	// ƒoƒbƒtƒ@Šm•Û
	try {
		opmbuf = new DWORD [BufMax * 2];
	}
	catch (...) {
		return FALSE;
	}
	if (!opmbuf) {
		return FALSE;
	}

	// ƒ[ƒNƒNƒŠƒA
	memset(&opm, 0, sizeof(opm));
	memset(&bufinfo, 0, sizeof(bufinfo));
	bufinfo.max = BufMax;
	bufinfo.sound = TRUE;

	// ƒoƒbƒtƒ@‰Šú‰»
	InitBuf(44100);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒNƒŠ[ƒ“ƒAƒbƒv
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::Cleanup()
{
	ASSERT(this);

	if (opmbuf) {
		delete[] opmbuf;
		opmbuf = NULL;
	}

	// Šî–{ƒNƒ‰ƒX‚Ö
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ƒŠƒZƒbƒg
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::Reset()
{
	int i;

	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒŠƒZƒbƒg");

	// ƒCƒxƒ“ƒg‚ðŽ~‚ß‚é
	event[0].SetTime(0);
	event[1].SetTime(0);

	// ƒoƒbƒtƒ@ƒNƒŠƒA
	memset(opmbuf, 0, sizeof(DWORD) * (BufMax * 2));

	// ƒGƒ“ƒWƒ“‚ªŽw’è‚³‚ê‚Ä‚¢‚ê‚ÎƒŠƒZƒbƒg
	if (engine) {
		engine->Reset();
	}

	// ƒŒƒWƒXƒ^ƒNƒŠƒA
	for (i=0; i<0x100; i++) {
		if (i == 8) {
			continue;
		}
		if ((i >= 0x60) && (i <= 0x7f)) {
			Output(i, 0x7f);
			continue;
		}
		if ((i >= 0xe0) && (i <= 0xff)) {
			Output(i, 0xff);
			continue;
		}
		Output(i, 0);
	}
	Output(0x19, 0x80);
	for (i=0; i<8; i++) {
		Output(8, i);
		opm.key[i] = i;
	}

	// ‚»‚Ì‘¼ƒ[ƒNƒGƒŠƒA‚ð‰Šú‰»
	opm.addr = 0;
	opm.busy = FALSE;
	for (i=0; i<2; i++) {
		opm.enable[i] = FALSE;
		opm.action[i] = FALSE;
		opm.interrupt[i] = FALSE;
		opm.time[i] = 0;
	}
	opm.started = FALSE;

	// Š„‚èž‚Ý—v‹‚È‚µ
	mfp->SetGPIP(3, 1);
}

//---------------------------------------------------------------------------
//
//	ƒZ[ƒu
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒZ[ƒu");

	// ƒTƒCƒY‚ðƒZ[ƒu
	sz = sizeof(opm_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// –{‘Ì‚ðƒZ[ƒu
	if (!fio->Write(&opm, (int)sz)) {
		return FALSE;
	}

	// ƒoƒbƒtƒ@î•ñ‚ÌƒTƒCƒY‚ðƒZ[ƒu
	sz = sizeof(opmbuf_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// ƒoƒbƒtƒ@î•ñ‚ðƒZ[ƒu
	if (!fio->Write(&bufinfo, (int)sz)) {
		return FALSE;
	}

	// ƒCƒxƒ“ƒg‚ðƒZ[ƒu
	if (!event[0].Save(fio, ver)) {
		return FALSE;
	}
	if (!event[1].Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Load(Fileio *fio, int ver)
{
	size_t sz;
	int i;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "ƒ[ƒh");

	// ƒTƒCƒY‚ðƒ[ƒhAÆ‡
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(opm_t)) {
		return FALSE;
	}

	// –{‘Ì‚ðƒ[ƒh
	if (!fio->Read(&opm, (int)sz)) {
		return FALSE;
	}

	// ƒoƒbƒtƒ@î•ñ‚ÌƒTƒCƒY‚ðƒ[ƒhAÆ‡
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(opmbuf_t)) {
		return FALSE;
	}

	// ƒoƒbƒtƒ@î•ñ‚ðƒ[ƒh
	if (!fio->Read(&bufinfo, (int)sz)) {
		return FALSE;
	}

	// ƒCƒxƒ“ƒg‚ðƒ[ƒh
	if (!event[0].Load(fio, ver)) {
		return FALSE;
	}
	if (!event[1].Load(fio, ver)) {
		return FALSE;
	}

	// ƒoƒbƒtƒ@‚ðƒNƒŠƒA
	InitBuf(bufinfo.rate * 100);

	// ƒGƒ“ƒWƒ“‚Ö‚ÌƒŒƒWƒXƒ^ÄÝ’è
	if (engine) {
		const bool raw_mode = engine->UseRawModeReg();
		// ƒŒƒWƒXƒ^•œ‹Œ:ƒmƒCƒYALFOAPMDAAMDACSM
		engine->SetReg(0x0f, opm.reg[0x0f]);
		engine->SetReg(0x14, raw_mode ? opm.reg[0x14] : (opm.reg[0x14] & 0x80));
		engine->SetReg(0x18, opm.reg[0x18]);
		engine->SetReg(0x19, opm.reg[0x19]);

		// ƒŒƒWƒXƒ^•œ‹Œ:ƒŒƒWƒXƒ^
		for (i=0x20; i<0x100; i++) {
			engine->SetReg(i, opm.reg[i]);
		}

		// ƒŒƒWƒXƒ^•œ‹Œ:ƒL[
		for (i=0; i<8; i++) {
			engine->SetReg(8, opm.key[i]);
		}

		// ƒŒƒWƒXƒ^•œ‹Œ:LFO
		engine->SetReg(1, 2);
		engine->SetReg(1, 0);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Ý’è“K—p
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Ý’è“K—p");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	f’f
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::AssertDiag() const
{
	// Šî–{ƒNƒ‰ƒX
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('O', 'P', 'M', ' '));
	ASSERT(memdev.first == 0xe90000);
	ASSERT(memdev.last == 0xe91fff);
	ASSERT(mfp);
	ASSERT(mfp->GetID() == MAKEID('M', 'F', 'P', ' '));
	ASSERT(adpcm);
	ASSERT(adpcm->GetID() == MAKEID('A', 'P', 'C', 'M'));
	ASSERT(fdd);
	ASSERT(fdd->GetID() == MAKEID('F', 'D', 'D', ' '));
	ASSERT(opm.addr < 0x100);
	ASSERT((opm.busy == TRUE) || (opm.busy == FALSE));
	ASSERT((opm.enable[0] == TRUE) || (opm.enable[0] == FALSE));
	ASSERT((opm.enable[1] == TRUE) || (opm.enable[1] == FALSE));
	ASSERT((opm.action[0] == TRUE) || (opm.action[0] == FALSE));
	ASSERT((opm.action[1] == TRUE) || (opm.action[1] == FALSE));
	ASSERT((opm.interrupt[0] == TRUE) || (opm.interrupt[0] == FALSE));
	ASSERT((opm.interrupt[1] == TRUE) || (opm.interrupt[1] == FALSE));
	ASSERT((opm.started == TRUE) || (opm.started == FALSE));
	ASSERT(bufinfo.max == BufMax);
	ASSERT(bufinfo.num <= bufinfo.max);
	ASSERT(bufinfo.read < bufinfo.max);
	ASSERT(bufinfo.write < bufinfo.max);
	ASSERT((bufinfo.sound == TRUE) || (bufinfo.sound == FALSE));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg“Ç‚Ýž‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ReadByte(DWORD addr)
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) != 0) {
		// 4ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
		addr &= 3;

		// ƒEƒFƒCƒg
		scheduler->Wait(1);

		if (addr != 0x01) {
			// ƒf[ƒ^ƒ|[ƒg‚ÍBUSY‚Æƒ^ƒCƒ}‚Ìó‘Ô
			data = 0;

			// BUSY(1‰ñ‚¾‚¯)
			if (opm.busy) {
				data |= 0x80;
				opm.busy = FALSE;
			}

			// ƒ^ƒCƒ}
			if (opm.interrupt[0]) {
				data |= 0x01;
			}
			if (opm.interrupt[1]) {
				data |= 0x02;
			}

			return data;
		}

		// ƒAƒhƒŒƒXƒ|[ƒg‚ÍFF
		return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh‘‚«ž‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	return (WORD)(0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	ƒoƒCƒg‘‚«ž‚Ý
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) != 0) {
		// 4ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
		addr &= 3;

		// ƒEƒFƒCƒg
		scheduler->Wait(1);

		if (addr == 0x01) {
			// ƒAƒhƒŒƒXŽw’è(BUSY‚ÉŠÖ‚í‚ç‚¸Žw’è‚Å‚«‚é‚±‚Æ‚É‚·‚é)
			opm.addr = data;

			// BUSY‚ð~‚ë‚·BƒAƒhƒŒƒXŽw’èŒã‚Ì‘Ò‚¿‚Í”­¶‚µ‚È‚¢‚±‚Æ‚É‚·‚é
			opm.busy = FALSE;

			return;
		}
		else {
			// ƒf[ƒ^‘‚«ž‚Ý(BUSY‚ÉŠÖ‚í‚ç‚¸‘‚«ž‚ß‚é‚±‚Æ‚É‚·‚é)
			Output(opm.addr, data);

			// BUSY‚ðã‚°‚é
			opm.busy = TRUE;
			return;
		}
	}
}

//---------------------------------------------------------------------------
//
//	ƒ[ƒh‘‚«ž‚Ý
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);
	ASSERT_DIAG();

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	“Ç‚Ýž‚Ý‚Ì‚Ý
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ReadOnly(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Šï”ƒAƒhƒŒƒX‚Ì‚ÝƒfƒR[ƒh‚³‚ê‚Ä‚¢‚é
	if ((addr & 1) != 0) {
		// 4ƒoƒCƒg’PˆÊ‚Åƒ‹[ƒv
		addr &= 3;

		if (addr != 0x01) {
			// ƒf[ƒ^ƒ|[ƒg‚ÍBUSY‚Æƒ^ƒCƒ}‚Ìó‘Ô
			data = 0;

			// BUSY
			if (opm.busy) {
				data |= 0x80;
			}

			// ƒ^ƒCƒ}
			if (opm.interrupt[0]) {
				data |= 0x01;
			}
			if (opm.interrupt[1]) {
				data |= 0x02;
			}

			return data;
		}

		// ƒAƒhƒŒƒXƒ|[ƒg‚ÍFF
		return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	“à•”ƒf[ƒ^Žæ“¾
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::GetOPM(opm_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// “à•”ƒf[ƒ^‚ðƒRƒs[
	*buffer = opm;
}

//---------------------------------------------------------------------------
//
//	ƒCƒxƒ“ƒgƒR[ƒ‹ƒoƒbƒN
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Callback(Event *ev)
{
	int index;

	ASSERT(this);
	ASSERT(ev);
	ASSERT_DIAG();

	index = ev->GetUser();
	ASSERT((index >= 0) && (index <= 1));

	if (opm.enable[index] && opm.action[index]) {
		opm.action[index] = FALSE;
		opm.interrupt[index] = TRUE;
		mfp->SetGPIP(3, 0);
	}

	if (ev->GetTime() != opm.time[index]) {
		ev->SetTime(opm.time[index]);
	}

	if ((index == 0) && engine) {
		if (opm.reg[0x14] & 0x80) {
			ProcessBuf();
			engine->TimerA();
			Xm6X68Sound::TimerA();
		}
	}

	return TRUE;
}

void FASTCALL OPMIF::SetEngine(FM::OPM *p)
{
	int i;

	ASSERT(this);
	ASSERT_DIAG();

	// NULL‚ðŽw’è‚³‚ê‚éê‡‚à‚ ‚é
	engine = p;
	const bool raw_mode = engine ? engine->UseRawModeReg() : false;

	// ADPCM‚Ö’Ê’m
	if (engine) {
		adpcm->Enable(TRUE);
	}
	else {
		adpcm->Enable(FALSE);
	}

	// engineŽw’è‚ ‚è‚È‚çAOPMƒŒƒWƒXƒ^‚ðo—Í
	if (!engine) {
		return;
	}
	ProcessBuf();

	// ƒŒƒWƒXƒ^•œ‹Œ:ƒmƒCƒYALFOAPMDAAMDACSM
	engine->SetReg(0x0f, opm.reg[0x0f]);
	engine->SetReg(0x14, raw_mode ? opm.reg[0x14] : (opm.reg[0x14] & 0x80));
	engine->SetReg(0x18, opm.reg[0x18]);
	engine->SetReg(0x19, opm.reg[0x19]);

	// ƒŒƒWƒXƒ^•œ‹Œ:ƒŒƒWƒXƒ^
	for (i=0x20; i<0x100; i++) {
		engine->SetReg(i, opm.reg[i]);
	}

	// ƒŒƒWƒXƒ^•œ‹Œ:ƒL[
	for (i=0; i<8; i++) {
		engine->SetReg(8, opm.key[i]);
	}

	// ƒŒƒWƒXƒ^•œ‹Œ:LFO
	engine->SetReg(1, 2);
	engine->SetReg(1, 0);
}

//---------------------------------------------------------------------------
//
//	ƒŒƒWƒXƒ^o—Í
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::Output(DWORD addr, DWORD data)
{
	const bool raw_mode = engine ? engine->UseRawModeReg() : false;
	const DWORD x68sound_data = data;

	ASSERT(this);
	ASSERT(addr < 0x100);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// ??????z???
	switch (addr) {
		// ???C?}A
		case 0x10:
		case 0x11:
			opm.reg[addr] = data;
			CalcTimerA();
			return;

		// ???C?}B
		case 0x12:
			opm.reg[addr] = data;
			CalcTimerB();
			return;

		// ???C?}????
		case 0x14:
			CtrlTimer(data);
			data &= raw_mode ? 0xff : 0x80;
			break;

		// ???p?[?g?t??
		case 0x1b:
			CtrlCT(data);
			opm.reg[addr] = data;
			data &= raw_mode ? 0xff : 0x3f;
			break;

		// ?L?[
		case 0x08:
			opm.key[data & 0x07] = data;
			opm.reg[addr] = data;
			break;

		// OPM operator/global writes
		default:
			opm.reg[addr] = data;
			break;
	}

	// ?G???W????w???????A?o?b?t?@??O??o??
	if (engine) {
		if ((addr < 0x10) || (addr > 0x14)) {
			// ???C?}???A????????
			ProcessBuf();

			// ?L?[?I?t??O??X?^?[?g?t???OUp
			if ((addr != 0x08) || (data >= 0x08)) {
				opm.started = TRUE;
			}
		}
		engine->SetReg(addr, data);
	}

	if (addr == 0x1b) {
		Xm6X68Sound::WriteOpm(static_cast<unsigned char>(addr), static_cast<unsigned char>(x68sound_data));
	} else {
		Xm6X68Sound::WriteOpm(static_cast<unsigned char>(addr), static_cast<unsigned char>(data));
	}
}

//---------------------------------------------------------------------------
//
//	???C?}A?Z?o
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CalcTimerA()
{
	DWORD hus;
	DWORD low;

	ASSERT(this);
	ASSERT_DIAG();

	hus = opm.reg[0x10];
	hus <<= 2;
	low = opm.reg[0x11] & 3;
	hus |= low;
	hus = (0x400 - hus);
	hus <<= 5;
	opm.time[0] = hus;
#if defined(OPM_LOG)
	LOG1(Log::Normal, "???C?}A = %d", hus);
#endif	// OPM_LOG

	// ?~??????????????
	if (event[0].GetTime() == 0) {
		event[0].SetTime(hus);
#if defined(OPM_LOG)
		LOG1(Log::Normal, "???C?}A ?X?^?[?g Time %d", event[0].GetTime());
#endif	// OPM_LOG
	}
}

//---------------------------------------------------------------------------
//
//	???C?}B?Z?o
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CalcTimerB()
{
	DWORD hus;

	ASSERT(this);
	ASSERT_DIAG();

	hus = opm.reg[0x12];
	hus = (0x100 - hus);
	hus <<= 9;
	opm.time[1] = hus;
#if defined(OPM_LOG)
	LOG1(Log::Normal, "???C?}B = %d", hus);
#endif	// OPM_LOG

	// ?~??????????????
	if (event[1].GetTime() == 0) {
		event[1].SetTime(hus);
#if defined(OPM_LOG)
		LOG1(Log::Normal, "???C?}B ?X?^?[?g Time %d", event[1].GetTime());
#endif	// OPM_LOG
	}
}

//---------------------------------------------------------------------------
//
//	???C?}????
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CtrlTimer(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// ƒI[ƒo[ƒtƒ[ƒtƒ‰ƒO‚ÌƒNƒŠƒA
	if (data & 0x10) {
		opm.interrupt[0] = FALSE;
	}
	if (data & 0x20) {
		opm.interrupt[1] = FALSE;
	}

	// —¼•û—Ž‚¿‚½‚çAŠ„‚èž‚Ý‚ð—Ž‚Æ‚·
	if (!opm.interrupt[0] && !opm.interrupt[1]) {
		mfp->SetGPIP(3, 1);
	}

	// ƒ^ƒCƒ}AƒAƒNƒVƒ‡ƒ“§Œä
	if (data & 0x04) {
		opm.action[0] = TRUE;
	}
	else {
		opm.action[0] = FALSE;
	}

	// ƒ^ƒCƒ}AƒCƒl[ƒuƒ‹§Œä
	if (data & 0x01) {
		// 0¨1‚Åƒ^ƒCƒ}’l‚ðƒ[ƒhA‚»‚êˆÈŠO‚Å‚àƒ^ƒCƒ}ON
		if (!opm.enable[0]) {
			CalcTimerA();
		}
		opm.enable[0] = TRUE;
	}
	else {
		// (ƒ}ƒ“ƒnƒbƒ^ƒ“EƒŒƒNƒCƒGƒ€)
		event[0].SetTime(0);
		opm.enable[0] = FALSE;
	}

	// ƒ^ƒCƒ}BƒAƒNƒVƒ‡ƒ“§Œä
	if (data & 0x08) {
		opm.action[1] = TRUE;
	}
	else {
		opm.action[1] = FALSE;
	}

	// ƒ^ƒCƒ}BƒCƒl[ƒuƒ‹§Œä
	if (data & 0x02) {
		// 0¨1‚Åƒ^ƒCƒ}’l‚ðƒ[ƒhA‚»‚êˆÈŠO‚Å‚àƒ^ƒCƒ}ON
		if (!opm.enable[1]) {
			CalcTimerB();
		}
		opm.enable[1] = TRUE;
	}
	else {
		// (ƒ}ƒ“ƒnƒbƒ^ƒ“EƒŒƒNƒCƒGƒ€)
		event[1].SetTime(0);
		opm.enable[1] = FALSE;
	}

	// ƒf[ƒ^‚ð‹L‰¯
	opm.reg[0x14] = data;

#if defined(OPM_LOG)
	LOG1(Log::Normal, "ƒ^ƒCƒ}§Œä $%02X", data);
#endif	// OPM_LOG
}

//---------------------------------------------------------------------------
//
//	CT§Œä
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CtrlCT(DWORD data)
{
	DWORD ct;

	ASSERT(this);
	ASSERT_DIAG();

	// CTƒ|[ƒg‚Ì‚ÝŽæ‚èo‚·
	data &= 0xc0;

	// ˆÈ‘O‚Ìƒf[ƒ^‚ð“¾‚é
	ct = opm.reg[0x1b];
	ct &= 0xc0;

	// ˆê’v‚µ‚Ä‚¢‚ê‚Î‰½‚à‚µ‚È‚¢
	if (data == ct) {
		return;
	}

	// ƒ`ƒFƒbƒN(ADPCM)
	if ((data & 0x80) != (ct & 0x80)) {
		if (data & 0x80) {
			adpcm->SetClock(4);
		}
		else {
			adpcm->SetClock(8);
		}
	}

	// ƒ`ƒFƒbƒN(FDD)
	if ((data & 0x40) != (ct & 0x40)) {
		if (data & 0x40) {
			fdd->ForceReady(TRUE);
		}
		else {
			fdd->ForceReady(FALSE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	ƒoƒbƒtƒ@“à—e‚ð“¾‚é
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::GetBufInfo(opmbuf_t *buf)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT_DIAG();

	*buf = bufinfo;
}

//---------------------------------------------------------------------------
//
//	ƒoƒbƒtƒ@‰Šú‰»
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::InitBuf(DWORD rate)
{
	ASSERT(this);
	ASSERT(rate > 0);
	ASSERT((rate % 100) == 0);
	ASSERT_DIAG();

	// ADPCM‚Éæ‚É’Ê’m
	adpcm->InitBuf(rate);

	// ƒJƒEƒ“ƒ^Aƒ|ƒCƒ“ƒ^
	bufinfo.num = 0;
	bufinfo.read = 0;
	bufinfo.write = 0;
	bufinfo.under = 0;
	bufinfo.over = 0;

	// ƒTƒEƒ“ƒhŽžŠÔ‚ÆA˜AŒg‚·‚éƒTƒ“ƒvƒ‹”
	scheduler->SetSoundTime(0);
	bufinfo.samples = 0;

	// ‡¬ƒŒ[ƒg
	bufinfo.rate = rate / 100;
}

//---------------------------------------------------------------------------
//
//	ƒoƒbƒtƒ@ƒŠƒ“ƒO
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ProcessBuf()
{
	DWORD stime;
	DWORD sample;
	DWORD first;
	DWORD second;

	ASSERT(this);
	ASSERT_DIAG();

	// ƒGƒ“ƒWƒ“‚ª‚È‚¯‚ê‚ÎƒŠƒ^[ƒ“
	if (!engine) {
		return bufinfo.num;
	}

	// ƒTƒEƒ“ƒhŽžŠÔ‚©‚çA“KØ‚ÈƒTƒ“ƒvƒ‹”‚ð“¾‚é
	sample = scheduler->GetSoundTime();
	stime = sample;

	sample *= bufinfo.rate;
	sample /= 20000;

	// bufmax‚É§ŒÀ
	if (sample > BufMax) {
		// ƒI[ƒo[‚µ‚·‚¬‚Ä‚¢‚é‚Ì‚ÅAƒŠƒZƒbƒg
		scheduler->SetSoundTime(0);
		bufinfo.samples = 0;
		return bufinfo.num;
	}

	// Œ»ó‚Æˆê’v‚µ‚Ä‚¢‚ê‚ÎƒŠƒ^[ƒ“
	if (sample <= bufinfo.samples) {
		// ƒVƒ“ƒNƒ‚³‚¹‚é
		while (stime >= 40000) {
			stime -= 20000;
			scheduler->SetSoundTime(stime);
			bufinfo.samples -= bufinfo.rate;
		}
		return bufinfo.num;
	}

	// Œ»ó‚Æˆá‚¤•”•ª‚¾‚¯¶¬‚·‚é
	sample -= bufinfo.samples;

	// 1‰ñ‚Ü‚½‚Í2‰ñ‚Ì‚Ç‚¿‚ç‚©ƒ`ƒFƒbƒN
	first = sample;
	if ((first + bufinfo.write) > BufMax) {
		first = BufMax - bufinfo.write;
	}
	second = sample - first;

	// 1‰ñ–Ú
	memset(&opmbuf[bufinfo.write * 2], 0, first * 8);
	if (bufinfo.sound) {
		engine->Mix((int32*)&opmbuf[bufinfo.write * 2], first);
	}
	bufinfo.write += first;
	bufinfo.write &= (BufMax - 1);
	bufinfo.num += first;
	if (bufinfo.num > BufMax) {
		bufinfo.num = BufMax;
		bufinfo.read = bufinfo.write;
	}

	// 2‰ñ–Ú
	if (second > 0) {
		memset(opmbuf, 0, second * 8);
		if (bufinfo.sound) {
			engine->Mix((int32*)opmbuf, second);
		}
		bufinfo.write += second;
		bufinfo.write &= (BufMax - 1);
		bufinfo.num += second;
		if (bufinfo.num > BufMax) {
			bufinfo.num = BufMax;
			bufinfo.read = bufinfo.write;
		}
	}

	// ‡¬Ï‚ÝƒTƒ“ƒvƒ‹”‚Ö‰ÁŽZ‚µA20000hus‚²‚Æ‚ÉƒVƒ“ƒNƒ‚³‚¹‚é
	bufinfo.samples += sample;
	while (stime >= 40000) {
		stime -= 20000;
		scheduler->SetSoundTime(stime);
		bufinfo.samples -= bufinfo.rate;
	}

	return bufinfo.num;
}

//---------------------------------------------------------------------------
//
//	ƒoƒbƒtƒ@‚©‚çŽæ“¾
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::GetBuf(DWORD *buf, int samples)
{
	DWORD first;
	DWORD second;
	DWORD under;
	DWORD over;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(samples > 0);
	ASSERT(engine);
	ASSERT_DIAG();

	// ƒI[ƒo[ƒ‰ƒ“ƒ`ƒFƒbƒN‚ðæ‚É
	over = 0;
	if (bufinfo.num > (DWORD)samples) {
		over = bufinfo.num - samples;
	}

	// ‰‰ñA2‰ñ–ÚAƒAƒ“ƒ_[ƒ‰ƒ“‚Ì—v‹”‚ðŒˆ‚ß‚é
	first = samples;
	second = 0;
	under = 0;
	if (bufinfo.num < first) {
		first = bufinfo.num;
		under = samples - first;
		samples = bufinfo.num;
	}
	if ((first + bufinfo.read) > BufMax) {
		first = BufMax - bufinfo.read;
		second = samples - first;
	}

	// ‰‰ñ“Ç‚ÝŽæ‚è
	memcpy(buf, &opmbuf[bufinfo.read * 2], (first * 8));
	buf += (first * 2);
	bufinfo.read += first;
	bufinfo.read &= (BufMax - 1);
	bufinfo.num -= first;

	// 2‰ñ–Ú“Ç‚ÝŽæ‚è
	if (second > 0) {
		memcpy(buf, &opmbuf[bufinfo.read * 2], (second * 8));
		bufinfo.read += second;
		bufinfo.read &= (BufMax - 1);
		bufinfo.num -= second;
	}

	// ƒAƒ“ƒ_[ƒ‰ƒ“
	if (under > 0) {
		// ‚±‚Ì1/4‚¾‚¯AŽŸ‰ñ‚É‡¬‚³‚ê‚é‚æ‚¤Žd‘g‚Þ
		bufinfo.samples = 0;
		under *= 5000;
		under /= bufinfo.rate;
		scheduler->SetSoundTime(under);

		// ‹L˜^
		bufinfo.under++;
	}

	// ƒI[ƒo[ƒ‰ƒ“
	if (over > 0) {
		// ‚±‚Ì1/4‚¾‚¯AŽŸ‰ñ’x‚ç‚¹‚é‚æ‚¤Žd‘g‚Þ
		over *= 5000;
		over /= bufinfo.rate;
		under = scheduler->GetSoundTime();
		if (under < over) {
			scheduler->SetSoundTime(0);
		}
		else {
			under -= over;
			scheduler->SetSoundTime(under);
		}

		// ‹L˜^
		bufinfo.over++;
	}
}
