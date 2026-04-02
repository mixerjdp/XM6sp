//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ ADPCM (MSM6258V) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "dmac.h"
#include "fileio.h"
#include "config.h"
#include "adpcm.h"
#include "x68sound_bridge.h"

//===========================================================================
//
//	ADPCM
//
//===========================================================================
//#define ADPCM_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
ADPCM::ADPCM(VM *p) : MemDevice(p)
{
	// Device ID
	dev.id = MAKEID('A', 'P', 'C', 'M');
	dev.desc = "ADPCM (MSM6258V)";

	// Memory map
	memdev.first = 0xe92000;
	memdev.last = 0xe93fff;

	// Reset any extra state the constructor does not cover
	memset(&adpcm, 0, sizeof(adpcm));
	adpcm.sync_rate = 882;
	adpcm.sync_step = 0x9c4000 / 882;
	adpcm.vol = 0;
	adpcm.enable = FALSE;
	adpcm.sound = TRUE;
	dmac = NULL;
	adpcmbuf = NULL;
	quirk_arianshuu_loop_fix = FALSE;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;
	memset(&diag, 0, sizeof(diag));
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Init()
{
	ASSERT(this);

	// Base class initialization
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Locate the DMAC
	ASSERT(!dmac);
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	// Allocate the buffer
	try {
		adpcmbuf = new DWORD[ BufMax * 2 ];
	}
	catch (...) {
		return FALSE;
	}
	if (!adpcmbuf) {
		return FALSE;
	}
	memset(adpcmbuf, 0, sizeof(DWORD) * (BufMax * 2));

	// Create the sampling event
	event.SetDevice(this);
	event.SetDesc("Sampling");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	// Reset interpolation parameters
	adpcm.interp = FALSE;

	// Reset the default table and runtime state
	adpcm.ratio = 0;
	adpcm.speed = 0x400;
	adpcm.clock = 8;

	// Build the lookup table and apply the default settings
	MakeTable();
	SetVolume(50);

	// Initialize the audio buffer
	InitBuf(adpcm.sync_rate * 50);

	// Recalculate speed parameters
	CalcSpeed();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Cleanup()
{
	ASSERT(this);

	// Free the audio buffer
	if (adpcmbuf) {
		delete[] adpcmbuf;
		adpcmbuf = NULL;
	}

	// Cleanup base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Reset");

	// Clear playback registers
	adpcm.play = FALSE;
	adpcm.rec = FALSE;
	adpcm.active = FALSE;
	adpcm.started = FALSE;
	adpcm.panpot = 0;
	adpcm.ratio = 0;
	adpcm.speed = 0x400;
	adpcm.data = 0;
	adpcm.offset = 0;
	adpcm.out = 0;
	adpcm.sample = 0;
	adpcm.clock = 8;
	CalcSpeed();
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;

	// Buffer initialization
	InitBuf(adpcm.sync_rate * 50);

	// Stop event
	event.SetUser(0);
	event.SetTime(0);
	event.SetDesc("Sampling");
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(adpcm_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save this data
	if (!fio->Write(&adpcm, (int)sz)) {
		return FALSE;
	}

	// Save event data
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
BOOL FASTCALL ADPCM::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(adpcm_t)) {
		return FALSE;
	}

	// Load this data
	if (!fio->Read(&adpcm, (int)sz)) {
		return FALSE;
	}

	// Load event data
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "Apply config");

	// Interpolation
	adpcm.interp = config->adpcm_interp;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	Assertion
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::AssertDiag() const
{
	// Base class
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('A', 'P', 'C', 'M'));
	ASSERT(memdev.first == 0xe92000);
	ASSERT(memdev.last == 0xe93fff);
	ASSERT(dmac);
	ASSERT(dmac->GetID() == MAKEID('D', 'M', 'A', 'C'));
	ASSERT((adpcm.panpot >= 0) && (adpcm.panpot <= 3));
	ASSERT((adpcm.play == TRUE) || (adpcm.play == FALSE));
	ASSERT((adpcm.rec == TRUE) || (adpcm.rec == FALSE));
	ASSERT((adpcm.active == TRUE) || (adpcm.active == FALSE));
	ASSERT((adpcm.started == TRUE) || (adpcm.started == FALSE));
	ASSERT((adpcm.clock == 4) || (adpcm.clock == 8));
	ASSERT((adpcm.ratio == 0) || (adpcm.ratio == 1) || (adpcm.ratio == 2));
	ASSERT((adpcm.speed & 0x007f) == 0);
	ASSERT((adpcm.speed >= 0x0100) && (adpcm.speed <= 0x400));
	ASSERT(adpcm.data < 0x100);
	ASSERT((adpcm.offset >= 0) && (adpcm.offset <= 48));
	ASSERT((adpcm.enable == TRUE) || (adpcm.enable == FALSE));
	ASSERT((adpcm.sound == TRUE) || (adpcm.sound == FALSE));
	ASSERT(adpcm.readpoint < BufMax);
	ASSERT(adpcm.writepoint < BufMax);
	ASSERT(adpcm.number <= BufMax);
	ASSERT(adpcm.sync_cnt < 0x4000);
	ASSERT((adpcm.interp == TRUE)  || (adpcm.interp == FALSE));
	ASSERT(adpcmbuf);
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadByte(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {
		// Loop at 4 byte boundary
		addr &= 3;

		// Wait
		scheduler->Wait(1);

		// Upper address
		if (addr == 3) {
			// Data register
			if (adpcm.rec && adpcm.active) {
				// Returns 0x80 as talk data
				return 0x80;
			}
			return adpcm.data;
		}

		// Status register
		if (adpcm.play) {
			if (quirk_arianshuu_loop_fix) {
				return 0xc0;
			}
			// Playing, or playing stopped
			return 0x7f;
		}
		else {
			if (quirk_arianshuu_loop_fix) {
				return 0x40;
			}
			// Talk mode, or playback not allowed
			return 0xff;
		}
	}

	// Even addresses are not decoded
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL ADPCM::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {
		// Loop at 4 byte boundary
		addr &= 3;

		// Wait
		scheduler->Wait(1);

		// Upper address
		if (addr == 3) {
			// Data register
			adpcm.data = data;
			return;
		}

#if defined(ADPCM_LOG)
		LOG1(Log::Normal, "ADPCM Command $%02X", data);
#endif	// ADPCM_LOG

		// Command register
		if (quirk_arianshuu_loop_fix) {
			// PCM8A/MCDRV compatible: STOP bit takes priority (treats 0x03 as STOP)
			if (data & 1) {
				Stop();
				Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
				return;
			}
			if (data & 2) {
				// PCM8A compatible: START resets boundary even during playback
				adpcm.offset = 0;
				adpcm.sample = 0;
				adpcm.out = 0;
				adpcm.sync_cnt = 0;
				adpcm.number = 0;
				adpcm.readpoint = adpcm.writepoint;
				quirk_stuck_l = 0;
				quirk_stuck_r = 0;
				if (adpcmbuf) {
					const DWORD right = (adpcm.readpoint + 1) & (BufMax - 1);
					adpcmbuf[adpcm.readpoint] = 0;
					adpcmbuf[right] = 0;
				}

				if (!adpcm.play || !adpcm.active) {
					adpcm.play = TRUE;
					Start(0);
				}
				Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
				return;
			}
			if (data & 4) {
				adpcm.number = 0;
				adpcm.readpoint = adpcm.writepoint;
				if (!adpcm.rec || !adpcm.active) {
					adpcm.rec = TRUE;
					Start(1);
				}
				Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
				return;
			}
			Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
			return;
		}

		if (data & 1) {
			// Stop
			Stop();
		}
		if (data & 2) {
			// Play start
			adpcm.play = TRUE;
			Start(0);
			Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
			return;
		}
		if (data & 4) {
			// Talk start
			adpcm.rec = TRUE;
			Start(1);
			Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
			return;
		}
		Xm6X68Sound::WriteAdpcm(static_cast<unsigned char>(data));
		return;
	}

	// Even addresses are not decoded
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::WriteWord(DWORD addr, DWORD data)
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
DWORD FASTCALL ADPCM::ReadOnly(DWORD addr) const
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// Only odd addresses are decoded
	if (addr & 1) {
		// Loop at 4 byte boundary
		addr &= 3;

		// Upper address
		if (addr == 3) {
			// Data register
			if (adpcm.rec && adpcm.active) {
				return 0x80;
			}
			return adpcm.data;
		}

		// Status register
		if (adpcm.play) {
			if (quirk_arianshuu_loop_fix) {
				return 0xc0;
			}
			// Playing, or playing stopped
			return 0x7f;
		}
		else {
			if (quirk_arianshuu_loop_fix) {
				return 0x40;
			}
			// Talk mode, or playback not allowed
			return 0xff;
		}
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	ADPCM data get
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetADPCM(adpcm_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT_DIAG();

	// Copy ADPCM data
	*buffer = adpcm;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL ADPCM::Callback(Event *ev)
{
	BOOL valid;
	DWORD num;
	char string[64];

	ASSERT(this);
	ASSERT(ev);
	ASSERT_DIAG();

	// If wait is finished, process now, otherwise continue counting down
	if (adpcm.wait <= 0) {
		while (adpcm.wait <= 0) {
			// If inactive or ReqDMA fails, set 0x80
			adpcm.data = 0x80;
			valid = FALSE;
			diag.req_total++;

			// Process 1 event = 1 byte (2 samples) of data
			if (adpcm.active) {
				if (dmac->ReqDMA(3)) {
					// DMA transfer success
					valid = TRUE;
					diag.req_ok++;
				}
				else {
					diag.req_fail++;
				}
			}
			else {
				diag.req_fail++;
			}
			diag.last_data = adpcm.data;

			// Playback
			if (ev->GetUser() == 0) {
				// 0x88,0x80,0x00 are flags
				if ((adpcm.data != 0x88) && (adpcm.data != 0x80) && (adpcm.data != 0x00)) {
#if defined(ADPCM_LOG)
					if (!adpcm.started) {
						LOG0(Log::Normal, "First valid data detected");
					}
#endif	// ADPCM_LOG
					adpcm.started = TRUE;
				}

				// ADPCM to PCM conversion, send to buffer
				num = adpcm.speed;
				num >>= 7;
				ASSERT((num >= 2) && (num <= 16));

				Decode((int)adpcm.data, num, valid);
				Decode((int)(adpcm.data >> 4), num, valid);
			}
			adpcm.wait++;
		}

		// Reset wait counter
		adpcm.wait = 0;

		// Adapt to speed change
		if (ev->GetTime() == adpcm.speed) {
			return TRUE;
		}
		sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
		ev->SetDesc(string);
		ev->SetTime(adpcm.speed);
		return TRUE;
	}

	// Decrement wait counter
	adpcm.wait--;

	// Adapt to speed change
	if (ev->GetTime() == adpcm.speed) {
		return TRUE;
	}
	sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
	ev->SetDesc(string);
	ev->SetTime(adpcm.speed);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Master clock setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetClock(DWORD clk)
{
	ASSERT(this);
	ASSERT((clk == 4) || (clk == 8));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Clock %dMHz", clk);
#endif	// ADPCM_LOG

	// Recalculate speed
	adpcm.clock = clk;
	CalcSpeed();
}

//---------------------------------------------------------------------------
//
//	Clock ratio setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetRatio(DWORD ratio)
{
	ASSERT(this);
	ASSERT((ratio >= 0) || (ratio <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Speed ratio %d", ratio);
#endif	// ADPCM_LOG

	// ratio=3 is treated as 2
	if (ratio == 3) {
		LOG0(Log::Warning, "Illegal ratio setting $03");
		ratio = 2;
	}

	// Recalculate speed
	if (adpcm.ratio != ratio) {
		adpcm.ratio = ratio;
		CalcSpeed();
	}
}

//---------------------------------------------------------------------------
//
//	Panpot setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetPanpot(DWORD panpot)
{
	ASSERT(this);
	ASSERT((panpot >= 0) || (panpot <= 3));
	ASSERT_DIAG();

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Panpot setting %d", panpot);
#endif	// ADPCM_LOG

	adpcm.panpot = panpot;
}

//---------------------------------------------------------------------------
//
//	Speed recalculation
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::CalcSpeed()
{
	ASSERT(this);

	// First, 2, 3, 4 are possible
	adpcm.speed = 2 - adpcm.ratio;
	ASSERT(adpcm.speed <= 2);
	adpcm.speed += 2;

	// Clock 4MHz: 256, Clock 8MHz: 128
	adpcm.speed <<= 7;
	if (adpcm.clock == 4) {
		adpcm.speed <<= 1;
	}
}

//---------------------------------------------------------------------------
//
//	Record/playback start
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Start(int type)
{
	char string[64];

	ASSERT(this);
	ASSERT((type == 0) || (type == 1));
	ASSERT_DIAG();
	diag.start_events++;

#if defined(ADPCM_LOG)
	LOG1(Log::Normal, "Playback start %d", type);
#endif	// ADPCM_LOG

	// Set active flag
	adpcm.active = TRUE;

	// Reset data position
	adpcm.offset = 0;

	// Set event user
	event.SetUser(type);

	// Set sampling rate (different from FM which is interrupt priority)
	sprintf(string, "Sampling %dHz", (2 * 1000 * 1000) / adpcm.speed);
	event.SetDesc(string);
	event.SetTime(adpcm.speed);

	// Run this event immediately (unlike FM which is interrupt priority)
	Callback(&event);
}

//---------------------------------------------------------------------------
//
//	Record/playback stop
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Stop()
{
	ASSERT(this);
	ASSERT_DIAG();
	diag.stop_events++;

#if defined(ADPCM_LOG)
	LOG0(Log::Normal, "Stop");
#endif	// ADPCM_LOG

	// Stop flags
	adpcm.active = FALSE;
	adpcm.play = FALSE;
	adpcm.rec = FALSE;

	if (quirk_arianshuu_loop_fix) {
		adpcm.number = 0;
		adpcm.readpoint = adpcm.writepoint;
		quirk_stuck_l = 0;
		quirk_stuck_r = 0;
		if (adpcmbuf) {
			const DWORD right = (adpcm.readpoint + 1) & (BufMax - 1);
			adpcmbuf[adpcm.readpoint] = 0;
			adpcmbuf[right] = 0;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Next table
//
//---------------------------------------------------------------------------
const int ADPCM::NextTable[] = {
	-1, -1, -1, -1, 2, 4, 6, 8,
	-1, -1, -1, -1, 2, 4, 6, 8
};

//---------------------------------------------------------------------------
//
//	Offset table
//
//---------------------------------------------------------------------------
const int ADPCM::OffsetTable[] = {
	 0,
	 0,  1,  2,  3,  4,  5,  6,  7,
	 8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 48, 48, 48, 48, 48, 48, 48,
	48
};

//---------------------------------------------------------------------------
//
//	Decode
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Decode(int data, int num, BOOL valid)
{
	int i;
	int store;
	int sample;
	int diff;
	int base;

	ASSERT(this);
	ASSERT((data >= 0) && (data < 0x100));
	ASSERT(num >= 2);
	ASSERT_DIAG();
	diag.decode_calls++;

	// Ignore if disabled
	if (!adpcm.enable) {
		return;
	}

	// Mask data
	data &= 0x0f;

	// Build diff table index
	i = adpcm.offset << 4;
	i |= data;
	sample = DiffTable[i];

	// Return next offset
	adpcm.offset += NextTable[data];
	adpcm.offset = OffsetTable[adpcm.offset + 1];

	// Scale sample data
	store = (sample << 8) + (adpcm.sample * 245);
	store >>= 8;

	// Calculate base value
	base = adpcm.sample;
	base *= adpcm.vol;
	base >>= 9;
	adpcm.sample = store;
	store *= adpcm.vol;
	store >>= 9;
	adpcm.out = store;

	// If no valid data yet, gradually increase sample
	if (!valid) {
		if (adpcm.sample < 0) {
			adpcm.sample++;
		}
		if (adpcm.sample > 0) {
			adpcm.sample--;
		}
	}

	// Flattening operation
	if ((adpcm.sample == 0) || (adpcm.sample == -1) || (adpcm.sample == 1)) {
		store = 0;
	}

	// Get diff
	diff = store - base;

	// Calculate and output num interpolated samples
	switch (adpcm.panpot) {
		// Output both
		case 0:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// Left only output
		case 1:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// Right only output
		case 2:
			for (i=0; i<num; i++) {
				store = base + (diff * (i + 1)) / num;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcmbuf[adpcm.writepoint++] = store;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// Mute
		case 3:
			for (i=0; i<num; i++) {
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcmbuf[adpcm.writepoint++] = 0;
				adpcm.writepoint &= (BufMax - 1);
			}
			break;

		// Default, should not occur
		default:
			ASSERT(FALSE);
	}

	// Update sample count
	adpcm.number += (num << 1);
	if (adpcm.number >= BufMax) {
#if defined(ADPCM_LOG)
		LOG0(Log::Warning, "ADPCM buffer overflow");
#endif	// ADPCM_LOG
		adpcm.number = BufMax;
		adpcm.readpoint = adpcm.writepoint;
	}

	if (adpcm.number > diag.max_buffer_samples) {
		diag.max_buffer_samples = adpcm.number;
	}
}

//---------------------------------------------------------------------------
//
//	Internal enable
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Enable(BOOL enable)
{
	ASSERT(this);
	ASSERT_DIAG();

	adpcm.enable = enable;
}

//---------------------------------------------------------------------------
//
//	Buffer initialization
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::InitBuf(DWORD rate)
{
	ASSERT(this);
	ASSERT(rate > 0);
	ASSERT((rate % 100) == 0);

#if defined(ADPCM_LOG)
	LOG0(Log::Normal, "Buffer initialization");
#endif	// ADPCM_LOG

	// Counter, pointers
	adpcm.number = 0;
	adpcm.readpoint = 0;
	adpcm.writepoint = 0;
	adpcm.wait = 0;
	adpcm.sync_cnt = 0;
	adpcm.sync_rate = rate / 50;
	adpcm.sync_step = 0x9c4000 / adpcm.sync_rate;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;

	// Set first data to 0
	if (adpcmbuf) {
		adpcmbuf[0] = 0;
		adpcmbuf[1] = 0;
	}
}

void FASTCALL ADPCM::SetArianshuuLoopFix(BOOL enabled)
{
	quirk_arianshuu_loop_fix = enabled ? TRUE : FALSE;
	quirk_stuck_l = 0;
	quirk_stuck_r = 0;
}

void FASTCALL ADPCM::GetDiag(adpcm_diag_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	if (!buffer) {
		return;
	}

	*buffer = diag;
}

//---------------------------------------------------------------------------
//
//	Buffer audio data get
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::GetBuf(DWORD *buffer, int samples)
{
	int i;
	int j;
	int l;
	int r;
	int *ptr;
	DWORD point;

	ASSERT(this);
	ASSERT(buffer);
	ASSERT(samples >= 0);
	ASSERT_DIAG();

	// If disabled or mono mode, clear buffer
	if (!adpcm.enable || !adpcm.sound) {
		ASSERT(adpcm.sync_rate != 0);
		InitBuf(adpcm.sync_rate * 50);
		return;
	}

	// Output
	ptr = (int*)buffer;

	// If buffer cannot keep up, repeat last data
	if (adpcm.number <= 2) {
		diag.underrun_head_events++;
		l = adpcmbuf[adpcm.readpoint];
		r = adpcmbuf[adpcm.readpoint + 1];
		if ((l > 8) || (l < -8) || (r > 8) || (r < -8)) {
			diag.stale_nonzero_events++;
		}

		if (quirk_arianshuu_loop_fix) {
			diag.silence_fill_events++;
			for (i=samples; i>0; i--) {
				*ptr += 0;
				ptr++;
				*ptr += 0;
				ptr++;
			}
			quirk_stuck_l = 0;
			quirk_stuck_r = 0;
			adpcmbuf[adpcm.readpoint] = 0;
			adpcmbuf[adpcm.readpoint + 1] = 0;
			return;
		}

		quirk_stuck_l = l;
		quirk_stuck_r = r;
		for (i=samples; i>0; i--) {
			*ptr += l;
			ptr++;
			*ptr += r;
			ptr++;
		}
		return;
	}

	// Valid interpolation
	if (adpcm.interp) {

		// Main loop
		for (i=samples; i>0; i--) {
			// Step up
			adpcm.sync_cnt += adpcm.sync_step;
			if (adpcm.sync_cnt >= 0x4000) {
				// Wrap
				adpcm.sync_cnt &= 0x3fff;

				// Advance
				if (adpcm.number >= 4) {
					adpcm.readpoint += 2;
					adpcm.readpoint &= (BufMax - 1);
					adpcm.number -= 2;
				}
			}

			// Check remaining data
			if (adpcm.number < 4 ) {
				diag.underrun_interp_events++;
				if (quirk_arianshuu_loop_fix) {
					diag.silence_fill_events++;
					*ptr += 0;
					ptr++;
					*ptr += 0;
					ptr++;
					quirk_stuck_l = 0;
					quirk_stuck_r = 0;
					continue;
				}

				// No data left, repeat last data
				*ptr += adpcmbuf[adpcm.readpoint];
				ptr++;
				*ptr += adpcmbuf[adpcm.readpoint + 1];
				ptr++;
			}
			else {
				// Calculate next point
				point = adpcm.readpoint + 2;
				point &= (BufMax - 1);

				// Interpolate L from next data and current data
				l = adpcmbuf[point] * (int)adpcm.sync_cnt;
				r = adpcmbuf[adpcm.readpoint + 0] * (int)(0x4000 - adpcm.sync_cnt);
				*ptr += ((l + r) >> 14);
				ptr++;

				// Interpolate R from next data and current data
				l = adpcmbuf[point + 1] * (int)adpcm.sync_cnt;
				r = adpcmbuf[adpcm.readpoint + 1] * (int)(0x4000 - adpcm.sync_cnt);
				*ptr += ((l + r) >> 14);
				ptr++;
			}
		}
	}
	else {
		// Non-interpolation
		for (i=samples; i>0; i--) {
			// Output from current position data
			*buffer += adpcmbuf[adpcm.readpoint];
			buffer++;
			*buffer += adpcmbuf[adpcm.readpoint + 1];
			buffer++;

			// Subtract sync_step
			adpcm.sync_cnt += adpcm.sync_step;

			// Reset when reaching 0x4000
			if (adpcm.sync_cnt < 0x4000) {
				continue;
			}
			adpcm.sync_cnt &= 0x3fff;

			// Move to next ADPCM sample
			if (adpcm.number <= 2) {
				diag.underrun_linear_events++;
				if (quirk_arianshuu_loop_fix) {
					diag.silence_fill_events++;
					for (j=i-1; j>0; j--) {
						*buffer += 0;
						buffer++;
						*buffer += 0;
						buffer++;
						adpcm.sync_cnt += adpcm.sync_step;
					}
					adpcm.sync_cnt &= 0x3fff;
					quirk_stuck_l = 0;
					quirk_stuck_r = 0;
					adpcmbuf[adpcm.readpoint] = 0;
					adpcmbuf[adpcm.readpoint + 1] = 0;
					return;
				}

				// Repeat last data
				l = adpcmbuf[adpcm.readpoint];
				r = adpcmbuf[adpcm.readpoint + 1];
				for (j=i-1; j>0; j--) {
					*buffer += l;
					buffer++;
					*buffer += r;
					buffer++;
					adpcm.sync_cnt += adpcm.sync_step;
				}
				adpcm.sync_cnt &= 0x3fff;
				return;
			}
			adpcm.readpoint += 2;
			adpcm.readpoint &= (BufMax - 1);
			adpcm.number -= 2;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Wait processing
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::Wait(int num)
{
	int i;

	ASSERT(this);
	ASSERT_DIAG();

	// If event not started, 0
	if (event.GetTime() == 0) {
		adpcm.wait = 0;
		return;
	}

	// If OPM audio is different (waits less)
	if ((int)adpcm.number <= num) {
		// Subtract remainder. This adds 1/4 wait
		num -= (adpcm.number);
		num >>= 2;

		// Calculation formula, inverse
		i = adpcm.speed >> 6;
		i *= adpcm.sync_rate;
		adpcm.wait = -((625 * num) / i);

#if defined(ADPCM_LOG)
		if (adpcm.wait != 0) {
			LOG1(Log::Normal, "Wait set %d", adpcm.wait);
		}
#endif	// ADPCM_LOG
		return;
	}

	// Subtract remainder. This adds 1/4 wait
	num = adpcm.number - num;
	num >>= 2;

	// For 44.1k,48k etc. sample rate differences
	// Wait count: (625 * x) / (adpcm.sync_rate * (adpcm.speed >> 6))
	i = adpcm.speed >> 6;
	i *= adpcm.sync_rate;
	adpcm.wait = (625 * num) / i;

#if defined(ADPCM_LOG)
	if (adpcm.wait != 0) {
		LOG1(Log::Normal, "Wait set %d", adpcm.wait);
	}
#endif	// ADPCM_LOG
}

//---------------------------------------------------------------------------
//
//	Table creation
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::MakeTable()
{
	int i;
	int j;
	int base;
	int diff;
	int *p;

	ASSERT(this);

	// Create table (flood with panic value for safety)
	p = DiffTable;
	for (i=0; i<49; i++) {
		base = (int)floor(16.0 * pow(1.1, i));

		// Division must be done in int
		for (j=0; j<16; j++) {
			diff = 0;
			if (j & 4) {
				diff += base;
			}
			if (j & 2) {
				diff += (base >> 1);
			}
			if (j & 1) {
				diff += (base >> 2);
			}
			diff += (base >> 3);
			if (j & 8) {
				diff = -diff;
			}

			*p++ = diff;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Volume setting
//
//---------------------------------------------------------------------------
void FASTCALL ADPCM::SetVolume(int volume)
{
	double offset;
	double vol;

	ASSERT(this);
	ASSERT((volume >= 0) && (volume <= 100));

	if (volume <= 0) {
		adpcm.vol = 0;
		return;
	}

	// Calculate and set 16384 * 10^((volume-140) / 200)
	offset = (double)(volume - 140);
	offset /= (double)200.0;
	vol = pow((double)10.0, offset);
	vol *= (double)16384.0;
	adpcm.vol = int(vol);
}
