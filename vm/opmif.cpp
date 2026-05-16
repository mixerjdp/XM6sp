//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
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
//	Constructor
//
//---------------------------------------------------------------------------
OPMIF::OPMIF(VM *p) : MemDevice(p)
{
	// Initialize device ID
	dev.id = MAKEID('O', 'P', 'M', ' ');
	dev.desc = "OPM (YM2151)";

	// Start address, End address
	memdev.first = 0xe90000;
	memdev.last = 0xe91fff;

	// Initialization
	mfp = NULL;
	fdd = NULL;
	adpcm = NULL;
	engine = NULL;
	opmbuf = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Init()
{
	int i;

 ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get MFP
 ASSERT(!mfp);
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
 ASSERT(mfp);

	// Get ADPCM
 ASSERT(!adpcm);
	adpcm = (ADPCM*)vm->SearchDevice(MAKEID('A', 'P', 'C', 'M'));
 ASSERT(adpcm);

	// Get FDD
 ASSERT(!fdd);
	fdd = (FDD*)vm->SearchDevice(MAKEID('F', 'D', 'D', ' '));
 ASSERT(fdd);

	// Create event
	event[0].SetDevice(this);
	event[0].SetDesc("Timer-A");
	event[1].SetDevice(this);
	event[1].SetDesc("Timer-B");
	for (i=0; i<2; i++) {
		event[i].SetUser(i);
		event[i].SetTime(0);
		scheduler->AddEvent(&event[i]);
	}

	// Allocate buffer
	try {
		opmbuf = new DWORD [BufMax * 2];
	}
	catch (...) {
		return FALSE;
	}
	if (!opmbuf) {
		return FALSE;
	}

	// Initialization
	memset(&opm, 0, sizeof(opm));
	memset(&bufinfo, 0, sizeof(bufinfo));
	bufinfo.max = BufMax;
	bufinfo.sound = TRUE;

	// Initialize buffer
	InitBuf(44100);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::Cleanup()
{
 ASSERT(this);

	if (opmbuf) {
		delete[] opmbuf;
		opmbuf = NULL;
	}

	// To base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::Reset()
{
	int i;

 ASSERT(this);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Stop event
	event[0].SetTime(0);
	event[1].SetTime(0);

	// Clear buffer
	memset(opmbuf, 0, sizeof(DWORD) * (BufMax * 2));

	// If engine is specified, reset
	if (engine) {
		engine->Reset();
	}

	// Clear register
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

	// Initialize other members
	opm.addr = 0;
	opm.busy = FALSE;
	for (i=0; i<2; i++) {
		opm.enable[i] = FALSE;
		opm.action[i] = FALSE;
		opm.interrupt[i] = FALSE;
		opm.time[i] = 0;
	}
	opm.started = FALSE;

	// No interrupt request
	mfp->SetGPIP(3, 1);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Save(Fileio *fio, int ver)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(opm_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save body
	if (!fio->Write(&opm, (int)sz)) {
		return FALSE;
	}

	// Save buffer info size
	sz = sizeof(opmbuf_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save buffer info
	if (!fio->Write(&bufinfo, (int)sz)) {
		return FALSE;
	}

	// Save event
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
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL OPMIF::Load(Fileio *fio, int ver)
{
	size_t sz;
	int i;

 ASSERT(this);
 ASSERT(fio);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(opm_t)) {
		return FALSE;
	}

	// Load body
	if (!fio->Read(&opm, (int)sz)) {
		return FALSE;
	}

	// Load buffer info size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(opmbuf_t)) {
		return FALSE;
	}

	// Load buffer info
	if (!fio->Read(&bufinfo, (int)sz)) {
		return FALSE;
	}

	// Load event
	if (!event[0].Load(fio, ver)) {
		return FALSE;
	}
	if (!event[1].Load(fio, ver)) {
		return FALSE;
	}

	// Initialize buffer
	InitBuf(bufinfo.rate * 100);

	// Re-set register to engine
	if (engine) {
		const bool raw_mode = engine->UseRawModeReg();
		// Register set: MFB, LFO, PMD, AMD, CSM
		engine->SetReg(0x0f, opm.reg[0x0f]);
		engine->SetReg(0x14, raw_mode ? opm.reg[0x14] : (opm.reg[0x14] & 0x80));
		engine->SetReg(0x18, opm.reg[0x18]);
		engine->SetReg(0x19, opm.reg[0x19]);

		// Register set: Register
		for (i=0x20; i<0x100; i++) {
			engine->SetReg(i, opm.reg[i]);
		}

		// Register set: Key
		for (i=0; i<8; i++) {
			engine->SetReg(8, opm.key[i]);
		}

		// Register set: LFO
		engine->SetReg(1, 2);
		engine->SetReg(1, 0);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::ApplyCfg(const Config* /*config*/)
{
 ASSERT(this);
 ASSERT_DIAG();

	LOG0(Log::Normal, "Apply config");
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assert
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::AssertDiag() const
{
	// Base class
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
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ReadByte(DWORD addr)
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

	// Only odd address is decoded
	if ((addr & 1) != 0) {
		// 4 byte align
		addr &= 3;

		// Wait
		scheduler->Wait(1);

		if (addr != 0x01) {
			// Data port is BUSY and Timer status
			data = 0;

			// BUSY(once only)
			if (opm.busy) {
				data |= 0x80;
				opm.busy = FALSE;
			}

			// Timer
			if (opm.interrupt[0]) {
				data |= 0x01;
			}
			if (opm.interrupt[1]) {
				data |= 0x02;
			}

			return data;
		}

		// Address port is FF
		return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
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
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);
 ASSERT_DIAG();

	// Only odd address is decoded
	if ((addr & 1) != 0) {
		// 4 byte align
		addr &= 3;

		// Wait
		scheduler->Wait(1);

		if (addr == 0x01) {
			// Address set (can set only when BUSY is cleared)
			opm.addr = data;

			// Clear BUSY. Waiting after address set does not occur
			opm.busy = FALSE;

			return;
		}
		else {
			// Data write (write is accepted when BUSY is cleared)
			Output(opm.addr, data);

			// Set BUSY
			opm.busy = TRUE;
			return;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Word write
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
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL OPMIF::ReadOnly(DWORD addr) const
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT_DIAG();

	// Only odd address is decoded
	if ((addr & 1) != 0) {
		// 4 byte align
		addr &= 3;

		if (addr != 0x01) {
			// Data port is BUSY and Timer status
			data = 0;

			// BUSY
			if (opm.busy) {
				data |= 0x80;
			}

			// Timer
			if (opm.interrupt[0]) {
				data |= 0x01;
			}
			if (opm.interrupt[1]) {
				data |= 0x02;
			}

			return data;
		}

		// Address port is FF
		return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get internal data
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::GetOPM(opm_t *buffer)
{
 ASSERT(this);
 ASSERT(buffer);
 ASSERT_DIAG();

	// Copy internal data
	*buffer = opm;
}

//---------------------------------------------------------------------------
//
//	Event callback
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

	// It is possible that NULL is specified
	engine = p;
	const bool raw_mode = engine ? engine->UseRawModeReg() : false;

	// Connect to ADPCM
	if (engine) {
		adpcm->Enable(TRUE);
	}
	else {
		adpcm->Enable(FALSE);
	}

	// If engine is specified, output OPM register
	if (!engine) {
		return;
	}
	ProcessBuf();

	// Register set: MFB, LFO, PMD, AMD, CSM
	engine->SetReg(0x0f, opm.reg[0x0f]);
	engine->SetReg(0x14, raw_mode ? opm.reg[0x14] : (opm.reg[0x14] & 0x80));
	engine->SetReg(0x18, opm.reg[0x18]);
	engine->SetReg(0x19, opm.reg[0x19]);

	// Register set: Register
	for (i=0x20; i<0x100; i++) {
		engine->SetReg(i, opm.reg[i]);
	}

	// Register set: Key
	for (i=0; i<8; i++) {
		engine->SetReg(8, opm.key[i]);
	}

	// Register set: LFO
	engine->SetReg(1, 2);
	engine->SetReg(1, 0);
}

//---------------------------------------------------------------------------
//
//	Register output
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

	// Register address switch
	switch (addr) {
		// Timer-A
		case 0x10:
		case 0x11:
			opm.reg[addr] = data;
			CalcTimerA();
			return;

		// Timer-B
		case 0x12:
			opm.reg[addr] = data;
			CalcTimerB();
			return;

		// Timer control
		case 0x14:
			CtrlTimer(data);
			data &= raw_mode ? 0xff : 0x80;
			break;

		// Control flag
		case 0x1b:
			CtrlCT(data);
			opm.reg[addr] = data;
			data &= raw_mode ? 0xff : 0x3f;
			break;

		// Key
		case 0x08:
			opm.key[data & 0x07] = data;
			opm.reg[addr] = data;
			break;

		// OPM operator/global writes
		default:
			opm.reg[addr] = data;
			break;
	}

	// If engine is connected, output to buffer
	if (engine) {
		if ((addr < 0x10) || (addr > 0x14)) {
			// If not timer register, process buffer
			ProcessBuf();

			// Key on triggers start flag
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
//	Timer-A calculation
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
	LOG1(Log::Normal, "Timer-A = %d", hus);
#endif	// OPM_LOG

	// If stopped, start
	if (event[0].GetTime() == 0) {
		event[0].SetTime(hus);
#if defined(OPM_LOG)
		LOG1(Log::Normal, "Timer-A Start Time %d", event[0].GetTime());
#endif	// OPM_LOG
	}
}

//---------------------------------------------------------------------------
//
//	Timer-B calculation
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
	LOG1(Log::Normal, "Timer-B = %d", hus);
#endif	// OPM_LOG

	// If stopped, start
	if (event[1].GetTime() == 0) {
		event[1].SetTime(hus);
#if defined(OPM_LOG)
		LOG1(Log::Normal, "Timer-B Start Time %d", event[1].GetTime());
#endif	// OPM_LOG
	}
}

//---------------------------------------------------------------------------
//
//	Timer control
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CtrlTimer(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);
 ASSERT_DIAG();

	// Clear interrupt flag
	if (data & 0x10) {
		opm.interrupt[0] = FALSE;
	}
	if (data & 0x20) {
		opm.interrupt[1] = FALSE;
	}

	// If both are cleared, cancel interrupt request
	if (!opm.interrupt[0] && !opm.interrupt[1]) {
		mfp->SetGPIP(3, 1);
	}

	// Timer-A operation control
	if (data & 0x04) {
		opm.action[0] = TRUE;
	}
	else {
		opm.action[0] = FALSE;
	}

	// Timer-A enable control
	if (data & 0x01) {
		// 0->1 loads timer value, otherwise timer ON
		if (!opm.enable[0]) {
			CalcTimerA();
		}
		opm.enable[0] = TRUE;
	}
	else {
		// (mask, ignore)
		event[0].SetTime(0);
		opm.enable[0] = FALSE;
	}

	// Timer-B operation control
	if (data & 0x08) {
		opm.action[1] = TRUE;
	}
	else {
		opm.action[1] = FALSE;
	}

	// Timer-B enable control
	if (data & 0x02) {
		// 0->1 loads timer value, otherwise timer ON
		if (!opm.enable[1]) {
			CalcTimerB();
		}
		opm.enable[1] = TRUE;
	}
	else {
		// (mask, ignore)
		event[1].SetTime(0);
		opm.enable[1] = FALSE;
	}

	// Store data
	opm.reg[0x14] = data;

#if defined(OPM_LOG)
	LOG1(Log::Normal, "Timer Control $%02X", data);
#endif	// OPM_LOG
}

//---------------------------------------------------------------------------
//
//	CT control
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::CtrlCT(DWORD data)
{
	DWORD ct;

 ASSERT(this);
 ASSERT_DIAG();

	// Only CT flag is extracted
	data &= 0xc0;

	// Get previous data
	ct = opm.reg[0x1b];
	ct &= 0xc0;

	// If equal, do nothing
	if (data == ct) {
		return;
	}

	// Check (ADPCM)
	if ((data & 0x80) != (ct & 0x80)) {
		if (data & 0x80) {
			adpcm->SetClock(4);
		}
		else {
			adpcm->SetClock(8);
		}
	}

	// Check (FDD)
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
//	Get buffer content
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
//	Initialize buffer
//
//---------------------------------------------------------------------------
void FASTCALL OPMIF::InitBuf(DWORD rate)
{
 ASSERT(this);
 ASSERT(rate > 0);
 ASSERT((rate % 100) == 0);
 ASSERT_DIAG();

	// Connect to ADPCM
	adpcm->InitBuf(rate);

	// Counter, pointer
	bufinfo.num = 0;
	bufinfo.read = 0;
	bufinfo.write = 0;
	bufinfo.under = 0;
	bufinfo.over = 0;

	// Sound time and synchronized item count
	scheduler->SetSoundTime(0);
	bufinfo.samples = 0;

	// Generate rate
	bufinfo.rate = rate / 100;
}

//---------------------------------------------------------------------------
//
//	Buffer processing
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

	// If no engine, return
	if (!engine) {
		return bufinfo.num;
	}

	// Get item count that is not processed from sound time
	sample = scheduler->GetSoundTime();
	stime = sample;

	sample *= bufinfo.rate;
	sample /= 20000;

	// Limited by BufMax
	if (sample > BufMax) {
		// Because it is overflowed, reset
		scheduler->SetSoundTime(0);
		bufinfo.samples = 0;
		return bufinfo.num;
	}

	// If equal to current, return
	if (sample <= bufinfo.samples) {
		// Decrease gradually
		while (stime >= 40000) {
			stime -= 20000;
			scheduler->SetSoundTime(stime);
			bufinfo.samples -= bufinfo.rate;
		}
		return bufinfo.num;
	}

	// Generate only difference from current
	sample -= bufinfo.samples;

	// Check 1 or 2 times
	first = sample;
	if ((first + bufinfo.write) > BufMax) {
		first = BufMax - bufinfo.write;
	}
	second = sample - first;

	// First time
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

	// Second time
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

	// Add to processed item count, decrease by 20000hus
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
//	Get from buffer
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

	// Check overflow first
	over = 0;
	if (bufinfo.num > (DWORD)samples) {
		over = bufinfo.num - samples;
	}

	// Calculate first, second, underflow request count
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

	// First time read
	memcpy(buf, &opmbuf[bufinfo.read * 2], (first * 8));
	buf += (first * 2);
	bufinfo.read += first;
	bufinfo.read &= (BufMax - 1);
	bufinfo.num -= first;

	// Second time read
	if (second > 0) {
		memcpy(buf, &opmbuf[bufinfo.read * 2], (second * 8));
		bufinfo.read += second;
		bufinfo.read &= (BufMax - 1);
		bufinfo.num -= second;
	}

	// Underflow
	if (under > 0) {
		// Only 1/4, next time will be compensated
		bufinfo.samples = 0;
		under *= 5000;
		under /= bufinfo.rate;
		scheduler->SetSoundTime(under);

		// Record
		bufinfo.under++;
	}

	// Overflow
	if (over > 0) {
		// Only 1/4, next time will be compensated
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

		// Record
		bufinfo.over++;
	}
}