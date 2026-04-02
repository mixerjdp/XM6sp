//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ FDC(uPD72065) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fdd.h"
#include "iosc.h"
#include "dmac.h"
#include "schedule.h"
#include "vm.h"
#include "log.h"
#include "fileio.h"
#include "config.h"
#include "fdc.h"

//===========================================================================
//
//	FDC
//
//===========================================================================
//#define FDC_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDC::FDC(VM *p) : MemDevice(p)
{
	// Device ID setting
	dev.id = MAKEID('F', 'D', 'C', ' ');
	dev.desc = "FDC (uPD72065)";

	// Start address, end address
	memdev.first = 0xe94000;
	memdev.last = 0xe95fff;

	// Object
	iosc = NULL;
	dmac = NULL;
	fdd = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get IOSC
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(iosc);

	// Get DMAC
	dmac = (DMAC*)vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	ASSERT(dmac);

	// Get FDD
	fdd = (FDD*)vm->SearchDevice(MAKEID('F', 'D', 'D', ' '));
	ASSERT(fdd);

	// Event setting
	event.SetDevice(this);
	event.SetDesc("Data Transfer");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	// Fast mode flag, dual drive flag (ApplyCfg)
	fdc.fast = FALSE;
	fdc.dual = FALSE;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Cleanup()
{
	ASSERT(this);

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Reset()
{
	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Data register, Status register
	fdc.dr = 0;
	fdc.sr = 0;
	fdc.sr |= sr_rqm;
	fdc.sr &= ~sr_dio;
	fdc.sr &= ~sr_ndm;
	fdc.sr &= ~sr_cb;

	// Drive select register and ST0-ST3
	fdc.dcr = 0;
	fdc.dsr = 0;
	fdc.st[0] = 0;
	fdc.st[1] = 0;
	fdc.st[2] = 0;
	fdc.st[3] = 0;

	// Command specific parameters
	fdc.srt = 1 * 2000;
	fdc.hut = 16 * 2000;
	fdc.hlt = 2 * 2000;
	fdc.hd = 0;
	fdc.us = 0;
	fdc.cyl[0] = 0;
	fdc.cyl[1] = 0;
	fdc.cyl[2] = 0;
	fdc.cyl[3] = 0;
	fdc.chrn[0] = 0;
	fdc.chrn[1] = 0;
	fdc.chrn[2] = 0;
	fdc.chrn[3] = 0;

	// Others
	fdc.eot = 0;
	fdc.gsl = 0;
	fdc.dtl = 0;
	fdc.sc = 0;
	fdc.gpl = 0;
	fdc.d = 0;
	fdc.err = 0;
	fdc.seek = FALSE;
	fdc.ndm = FALSE;
	fdc.mfm = FALSE;
	fdc.mt = FALSE;
	fdc.sk = FALSE;
	fdc.tc = FALSE;
	fdc.load = FALSE;

	// Transfer
	fdc.offset = 0;
	fdc.len = 0;
	memset(fdc.buffer, 0, sizeof(fdc.buffer));

	// Phase, Command
	fdc.phase = idle;
	fdc.cmd = no_cmd;

	// Packet management
	fdc.in_len = 0;
	fdc.in_cnt = 0;
	memset(fdc.in_pkt, 0, sizeof(fdc.in_pkt));
	fdc.out_len = 0;
	fdc.out_cnt = 0;
	memset(fdc.out_pkt, 0, sizeof(fdc.out_pkt));

	// Event stop
	event.SetTime(0);

	// Access stop (FDD reset makes fdd.selected=0)
	fdd->Access(FALSE);
}

//---------------------------------------------------------------------------
//
//	Software reset
//
//---------------------------------------------------------------------------
void FASTCALL FDC::SoftReset()
{
	// Data register (FDC)
	fdc.dr = 0;
	fdc.sr = 0;
	fdc.sr |= sr_rqm;
	fdc.sr &= ~sr_dio;
	fdc.sr &= ~sr_ndm;
	fdc.sr &= ~sr_cb;

	fdc.st[0] = 0;
	fdc.st[1] = 0;
	fdc.st[2] = 0;
	fdc.st[3] = 0;

	fdc.srt = 1 * 2000;
	fdc.hut = 16 * 2000;
	fdc.hlt = 2 * 2000;
	fdc.hd = 0;
	fdc.us = 0;
	fdc.cyl[0] = 0;
	fdc.cyl[1] = 0;
	fdc.cyl[2] = 0;
	fdc.cyl[3] = 0;
	fdc.chrn[0] = 0;
	fdc.chrn[1] = 0;
	fdc.chrn[2] = 0;
	fdc.chrn[3] = 0;

	fdc.eot = 0;
	fdc.gsl = 0;
	fdc.dtl = 0;
	fdc.sc = 0;
	fdc.gpl = 0;
	fdc.d = 0;
	fdc.err = 0;
	fdc.seek = FALSE;
	fdc.ndm = FALSE;
	fdc.mfm = FALSE;
	fdc.mt = FALSE;
	fdc.sk = FALSE;
	fdc.tc = FALSE;
	fdc.load = FALSE;

	fdc.offset = 0;
	fdc.len = 0;

	// Phase, Command
	fdc.phase = idle;
	fdc.cmd = fdc_reset;

	// Packet management
	fdc.in_len = 0;
	fdc.in_cnt = 0;
	memset(fdc.in_pkt, 0, sizeof(fdc.in_pkt));
	fdc.out_len = 0;
	fdc.out_cnt = 0;
	memset(fdc.out_pkt, 0, sizeof(fdc.out_pkt));

	// Event stop
	event.SetTime(0);

	// Access stop
	fdd->Access(FALSE);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(fdc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save self
	if (!fio->Write(&fdc, (int)sz)) {
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
BOOL FASTCALL FDC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Read size and check
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(fdc_t)) {
		return FALSE;
	}

	// Read self
	if (!fio->Read(&fdc, (int)sz)) {
		return FALSE;
	}

	// Load event
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
void FASTCALL FDC::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);

	LOG0(Log::Normal, "Apply config");

	// Fast mode
	fdc.fast = config->floppy_speed;
#if defined(FDC_LOG)
	if (fdc.fast) {
		LOG0(Log::Normal, "Fast mode ON");
	}
	else {
		LOG0(Log::Normal, "Fast mode OFF");
	}
#endif	// FDC_LOG

	// 2DD/2HD drive
	fdc.dual = config->dual_fdd;
#if defined(FDC_LOG)
	if (fdc.dual) {
		LOG0(Log::Normal, "2DD/2HD dual drive");
	}
	else {
		LOG0(Log::Normal, "2HD single drive");
	}
#endif	// FDC_LOG
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDC::ReadByte(DWORD addr)
{
	int i;
	int status;
	DWORD bit;
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Only even addresses are decoded
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Loop in 8 byte unit
	addr &= 0x07;
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	switch (addr) {
		// Status register
		case 0:
			return fdc.sr;

		// Data register
		case 1:
			// If SEEK pending interrupt not, trigger interrupt
			if (!fdc.seek) {
				Interrupt(FALSE);
			}

			switch (fdc.phase) {
				// Execute phase (ER)
				case read:
					fdc.sr &= ~sr_rqm;
					return Read();

				// Result phase
				case result:
					ASSERT(fdc.out_cnt >= 0);
					ASSERT(fdc.out_cnt < 0x10);
					ASSERT(fdc.out_len > 0);

					// Output packet data
					data = fdc.out_pkt[fdc.out_cnt];
					fdc.out_cnt++;
					fdc.out_len--;

					// If remaining allocation length becomes 0, go to idle phase
					if (fdc.out_len == 0) {
						Idle();
					}
					return data;
			}
			LOG0(Log::Warning, "FDC data register read error");
			return 0xff;

		// Drive status register
		case 2:
			data = 0;
			bit = 0x08;
			for (i=3; i>=0; i--) {
				// If DCR bit is set
				if ((fdc.dcr & bit) != 0) {
					// OR with drive status (b7,b6 only)
					status = fdd->GetStatus(i);
					data |= (DWORD)(status & 0xc0);
				}
				bit >>= 1;
			}

			// Clear FDD interrupt (not FDC interrupt, clear)
			iosc->IntFDD(FALSE);
			return data;

		// Drive select register
		case 3:
			LOG0(Log::Warning, "Drive select register read");
			return 0xff;
	}

	// Normal, this should not be
	ASSERT(FALSE);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDC::ReadWord(DWORD addr)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
void FASTCALL FDC::WriteByte(DWORD addr, DWORD data)
{
	int i;
	DWORD bit;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Only even addresses are decoded
	if ((addr & 1) == 0) {
		return;
	}

	// Loop in 8 byte unit
	addr &= 0x07;
	addr >>= 1;

	// Wait
	scheduler->Wait(1);

	switch (addr) {
		// Main command register
		case 0:
			switch (data) {
				// RESET STANDBY
				case 0x34:
#if defined(FDC_LOG)
					LOG0(Log::Normal, "RESET STANDBY command");
#endif	// FDC_LOG
					fdc.cmd = reset_stdby;
					Result();
					return;
				// SET STANDBY
				case 0x35:
#if defined(FDC_LOG)
					LOG0(Log::Normal, "SET STANDBY command");
#endif	// FDC_LOG
					fdc.cmd = set_stdby;
					Idle();
					return;
				// SOFTWARE RESET
				case 0x36:
#if defined(FDC_LOG)
					LOG0(Log::Normal, "SOFTWARE RESET command");
#endif	// FDC_LOG
					SoftReset();
					return;
			}
			LOG1(Log::Warning, "Invalid main command received %02X", data);
			return;

		// Data register
		case 1:
			// If SEEK pending interrupt not, trigger interrupt
			if (!fdc.seek) {
				Interrupt(FALSE);
			}

			switch (fdc.phase) {
				// Idle phase
				case idle:
					Command(data);
					return;

				// Command phase
				case command:
					ASSERT(fdc.in_cnt >= 0);
					ASSERT(fdc.in_cnt < 0x10);
					ASSERT(fdc.in_len > 0);

					// Set data to packet
					fdc.in_pkt[fdc.in_cnt] = data;
					fdc.in_cnt++;
					fdc.in_len--;

					// If remaining allocation length becomes 0, go to execute phase
					if (fdc.in_len == 0) {
						Execute();
					}
					return;

				// Execute phase (EW)
				case write:
					fdc.sr &= ~sr_rqm;
					Write(data);
					return;
			}
			LOG1(Log::Warning, "FDC data register write error $%02X", data);
			return;

		// Drive control register
		case 2:
			// Check which bit changed from 1 to 0
			bit = 0x01;
			for (i=0; i<4; i++) {
				if ((fdc.dcr & bit) != 0) {
					if ((data & bit) == 0) {
						// At 1 to 0 edge, apply the 4 bits of DCR status
						fdd->Control(i, fdc.dcr);
					}
				}
				bit <<= 1;
			}

			// Save value
			fdc.dcr = data;
			return;

		// Drive select register
		case 3:
			// Lower 2 bits select drive
			fdc.dsr = (DWORD)(data & 0x03);

			// Motor on/off in upper byte
			if (data & 0x80) {
				fdd->SetMotor(fdc.dsr, TRUE);
			}
			else {
				fdd->SetMotor(fdc.dsr, FALSE);
			}

			// 2HD/2DD switch (not supported if drive doesn't support)
			if (fdc.dual) {
				if (data & 0x10) {
					fdd->SetHD(FALSE);
				}
				else {
					fdd->SetHD(TRUE);
				}
			}
			else {
				fdd->SetHD(TRUE);
			}
			return;
	}

	// Normal, this should not be
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL FDC::WriteWord(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDC::ReadOnly(DWORD addr) const
{
	int i;
	int status;
	DWORD bit;
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Only even addresses are decoded
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// Loop in 8 byte unit
	addr &= 0x07;
	addr >>= 1;

	switch (addr) {
		// Status register
		case 0:
			return fdc.sr;

		// Data register
		case 1:
			if (fdc.phase == result) {
				// Output packet data (no update);
				return fdc.out_pkt[fdc.out_cnt];
			}
			return 0xff;

		// Drive status register
		case 2:
			data = 0;
			bit = 0x08;
			for (i=3; i>=0; i--) {
				// If DCR bit is set
				if ((fdc.dcr & bit) != 0) {
					// OR with drive status (b7,b6 only)
					status = fdd->GetStatus(i);
					data |= (DWORD)(status & 0xc0);
				}
				bit >>= 1;
			}
			return data;

		// Drive select register
		case 3:
			return 0xff;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::Callback(Event *ev)
{
	int i;
	int thres;

	ASSERT(this);
	ASSERT(ev);

	// Idle phase is hardware, unload
	if (fdc.phase == idle) {
		fdc.load = FALSE;

		// Once
		return FALSE;
	}

	// Head load mode
	fdc.load = TRUE;

	// Execute phase
	if (fdc.phase == execute) {
		// Time until ID read NO DATA is found
		Result();

		// Once
		return FALSE;
	}

	// Write ID is special
	if (fdc.cmd == write_id) {
		ASSERT(fdc.len > 0);
		ASSERT((fdc.len & 3) == 0);

		// Set time
		if (fdc.fast) {
			ev->SetTime(32 * 4);
		}
		else {
			ev->SetTime(fdd->GetRotationTime() / fdc.sc);
		}

		fdc.sr |= sr_rqm;
		dmac->ReqDMA(0);
		fdc.sr |= sr_rqm;
		dmac->ReqDMA(0);
		fdc.sr |= sr_rqm;
		dmac->ReqDMA(0);
		fdc.sr |= sr_rqm;
		dmac->ReqDMA(0);
		return TRUE;
	}

	// Read(Del)Data/Write(Del)Data/Scan/ReadDiag. Set time
	EventRW();

	// Set data transfer request
	fdc.sr |= sr_rqm;

	// Data transfer
	if (!fdc.ndm) {
		// DMA mode (DMA transfer request). Execute 64 bytes at once
		if (fdc.fast) {
			// In one event, transfer CPU cycle 2/3 reduction
			thres = (int)scheduler->GetCPUSpeed();
			thres <<= 1;
			thres /= 3;

			// Loop until entering result phase
			while (fdc.phase != result) {
				// Exit if CPU cycle becomes insufficient
				if (scheduler->GetCPUCycle() > thres) {
					break;
				}

				// Transfer
				dmac->ReqDMA(0);
			}
		}
		else {
			// Normal. 64 bytes at once
			for (i=0; i<64; i++) {
				if (fdc.phase == result) {
					break;
				}
				dmac->ReqDMA(0);
			}
		}
		return TRUE;
	}

	// Non-DMA mode (interrupt transfer request)
	Interrupt(TRUE);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get structure address
//
//---------------------------------------------------------------------------
const FDC::fdc_t* FASTCALL FDC::GetWork() const
{
	ASSERT(this);

	// Return address (fdc.buffer is large)
	return &fdc;
}

//---------------------------------------------------------------------------
//
//	Seek complete
//
//---------------------------------------------------------------------------
void FASTCALL FDC::CompleteSeek(int drive, BOOL status)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

#if defined(FDC_LOG)
	if (status) {
		LOG2(Log::Normal, "Seek complete Drive%d Cylinder_%02X",
					drive, fdd->GetCylinder(drive));
	}
	else {
		LOG2(Log::Normal, "Seek failed Drive%d Cylinder_%02X",
					drive, fdd->GetCylinder(drive));
	}
#endif	// FDC_LOG

	// Only valid for recalibrate or seek
	if ((fdc.cmd == recalibrate) || (fdc.cmd == seek)) {
		// Create ST0 (current US only)
		fdc.st[0] = fdc.us;

		// Status update
		if (status) {
			// Drive 2,3 add EC
			if (drive <= 1) {
				// Seek End
				fdc.st[0] |= 0x20;
			}
			else {
				// Equipment Check, Attention Interrupt
				fdc.st[0] |= 0x10;
				fdc.st[0] |= 0xc0;
			}
		}
		else {
			if (drive <= 1) {
				// Seek End
				fdc.st[0] |= 0x20;
			}
			// Not Ready, Abnormal Terminate
			fdc.st[0] |= 0x08;
			fdc.st[0] |= 0x40;
		}

		// SEEK pending interrupt
		Interrupt(TRUE);
		fdc.seek = TRUE;
		Idle();
		return;
	}

	LOG1(Log::Warning, "Invalid seek result Drive%d", drive);
}

//---------------------------------------------------------------------------
//
//	TC signal
//
//---------------------------------------------------------------------------
void FASTCALL FDC::SetTC()
{
	ASSERT(this);

	// Not idle phase because clear idle phase, if not idle phase
	if (fdc.phase != idle) {
		fdc.tc = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Idle phase
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Idle()
{
	ASSERT(this);

	// Phase setting
	fdc.phase = idle;
	fdc.err = 0;
	fdc.tc = FALSE;

	// Event stop
	event.SetTime(0);

	// If not head load mode, set event for head load
	if (fdc.load) {
		// Head load required
		if (fdc.hut > 0) {
			event.SetTime(fdc.hut);
		}
	}

	// Status register is command wait
	fdc.sr = sr_rqm;

	// Access stop
	fdd->Access(FALSE);
}

//---------------------------------------------------------------------------
//
//	Command phase
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Command(DWORD data)
{
	DWORD mask;

	ASSERT(this);
	ASSERT(data < 0x100);

	// Command phase (FDC BUSY)
	fdc.phase = command;
	fdc.sr |= sr_cb;

	// Input packet preparation
	fdc.in_pkt[0] = data;
	fdc.in_cnt = 1;
	fdc.in_len = 0;

	// Mask (1)
	mask = data;

	// FDC reset can only be performed here
	switch (mask) {
		// RESET STANDBY
		case 0x34:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "RESET STANDBY command");
#endif	// FDC_LOG
			fdc.cmd = reset_stdby;
			Result();
			return;

		// SET STANDBY
		case 0x35:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SET STANDBY command");
#endif	// FDC_LOG
			fdc.cmd = set_stdby;
			Idle();
			return;

		// SOFTWARE RESET
		case 0x36:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SOFTWARE RESET command");
#endif	// FDC_LOG
			SoftReset();
			return;
	}

	// Seek pending command execution, only SENSE INTERRUPT STATUS valid
	if (fdc.seek) {
		// SENSE INTERRUPT STATUS
		if (mask == 0x08) {
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SENSE INTERRUPT STATUS command");
#endif	// FDC_LOG
			fdc.cmd = sense_int_stat;

			// Clear interrupt trigger
			fdc.sr &= 0xf0;
			fdc.seek = FALSE;
			Interrupt(FALSE);

			// No parameters, if not execute phase
			Result();
			return;
		}

		// Others are all invalid command
#if defined(FDC_LOG)
		LOG0(Log::Normal, "INVALID command");
#endif	// FDC_LOG
		fdc.cmd = invalid;
		Result();
		return;
	}

	// SENSE INTERRUPT STATUS (here)
	if (mask == 0x08) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "INVALID command");
#endif	// FDC_LOG
		fdc.cmd = invalid;
		Result();
		return;
	}

	// Status clear
	fdc.st[0] = 0;
	fdc.st[1] = 0;
	fdc.st[2] = 0;

	// Normal
	switch (mask) {
		// READ DIAGNOSTIC(FM mode)
		case 0x02:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "READ DIAGNOSTIC command(FM mode)");
#endif	// FDC_LOG
			CommandRW(read_diag, data);
			return;

		// SPECIFY
		case 0x03:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SPECIFY command");
#endif	// FDC_LOG
			fdc.cmd = specify;
			fdc.in_len = 2;
			return;

		// SENSE DEVICE STATUS
		case 0x04:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SENSE DEVICE STATUS command");
#endif	// FDC_LOG
			fdc.cmd = sense_dev_stat;
			fdc.in_len = 1;
			return;

		// RECALIBRATE
		case 0x07:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "RECALIBRATE command");
#endif	// FDC_LOG
			fdc.cmd = recalibrate;
			fdc.in_len = 1;
			return;

		// READ ID(FM mode)
		case 0x0a:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "READ ID command(FM mode)");
#endif	// FDC_LOG
			fdc.cmd = read_id;
			fdc.mfm = FALSE;
			fdc.in_len = 1;
			return;

		// WRITE ID(FM mode)
		case 0x0d:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "WRITE ID command(FM mode)");
#endif	// FDC_LOG
			fdc.cmd = write_id;
			fdc.mfm = FALSE;
			fdc.in_len = 5;
			return;

		// SEEK
		case 0x0f:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "SEEK command");
#endif	// FDC_LOG
			fdc.cmd = seek;
			fdc.in_len = 2;
			return;

		// READ DIAGNOSTIC(MFM mode)
		case 0x42:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "READ DIAGNOSTIC command(MFM mode)");
#endif	// FDC_LOG
			CommandRW(read_diag, data);
			return;

		// READ ID(MFM mode)
		case 0x4a:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "READ ID command(MFM mode)");
#endif	// FDC_LOG
			fdc.cmd = read_id;
			fdc.mfm = TRUE;
			fdc.in_len = 1;
			return;

		// WRITE ID(MFM mode)
		case 0x4d:
#if defined(FDC_LOG)
			LOG0(Log::Normal, "WRITE ID command(MFM mode)");
#endif	// FDC_LOG
			fdc.cmd = write_id;
			fdc.mfm = TRUE;
			fdc.in_len = 5;
			return;
	}

	// Mask (2)
	mask &= 0x3f;

	// WRITE DATA
	if (mask == 0x05) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "WRITE DATA command");
#endif	// FDC_LOG
		CommandRW(write_data, data);
		return;
	}

	// WRITE DELETED DATA
	if (mask == 0x09) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "WRITE DELETED DATA command");
#endif	// FDC_LOG
		CommandRW(write_del_data, data);
		return;
	}

	// Mask (3);
	mask &= 0x1f;

	// READ DATA
	if (mask == 0x06) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "READ DATA command");
#endif	// FDC_LOG
		CommandRW(read_data, data);
		return;
	}

	// READ DELETED DATA
	if (mask == 0x0c) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "READ DELETED DATA command");
#endif	// FDC_LOG
		CommandRW(read_data, data);
		return;
	}

	// SCAN EQUAL
	if (mask == 0x11) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "SCAN EQUAL command");
#endif	// FDC_LOG
		CommandRW(scan_eq, data);
		return;
	}

	// SCAN LOW OR EQUAL
	if (mask == 0x19) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "SCAN LOW OR EQUAL command");
#endif	// FDC_LOG
		CommandRW(scan_lo_eq, data);
		return;
	}

	// SCAN HIGH OR EQUAL
	if (mask == 0x1d) {
#if defined(FDC_LOG)
		LOG0(Log::Normal, "SCAN HIGH OR EQUAL command");
#endif	// FDC_LOG
		CommandRW(scan_hi_eq, data);
		return;
	}

	// Invalid
	LOG1(Log::Warning, "Command phase unsupported command $%02X", data);
	Idle();
}

//---------------------------------------------------------------------------
//
//	Command phase (Read/Write sub)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::CommandRW(fdccmd cmd, DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	// Command
	fdc.cmd = cmd;

	// MT
	if (data & 0x80) {
		fdc.mt = TRUE;
	}
	else {
		fdc.mt = FALSE;
	}

	// MFM
	if (data & 0x40) {
		fdc.mfm = TRUE;
	}
	else {
		fdc.mfm = FALSE;
	}

	// SK(READ/SCAN only)
	if (data & 0x20) {
		fdc.sk = TRUE;
	}
	else {
		fdc.sk = FALSE;
	}

	// Command phase remaining bytes
	fdc.in_len = 8;
}

//---------------------------------------------------------------------------
//
//	Execute phase
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Execute()
{
	ASSERT(this);

	// Execute phase
	fdc.phase = execute;

	// Access start, event stop
	fdd->Access(TRUE);
	event.SetTime(0);

	// Command branch
	switch (fdc.cmd) {
		// SPECIFY
		case specify:
			// SRT
			fdc.srt = (fdc.in_pkt[1] >> 4) & 0x0f;
			fdc.srt = 16 - fdc.srt;
			fdc.srt <<= 11;

			// HUT (0 and 16 are the same. i82078 data sheet)
			fdc.hut = fdc.in_pkt[1] & 0x0f;
			if (fdc.hut == 0) {
				fdc.hut = 16;
			}
			fdc.hut <<= 15;

			// HLT (0 is same as HUT)
			fdc.hlt = (fdc.in_pkt[2] >> 1) & 0x7f;
			if (fdc.hlt == 0) {
				fdc.hlt = 0x80;
			}
			fdc.hlt <<= 12;

			// NDM
			if (fdc.in_pkt[2] & 1) {
				fdc.ndm = TRUE;
				LOG0(Log::Warning, "Set to Non-DMA mode");
			}
			else {
				fdc.ndm = FALSE;
			}

			// Execute phase unnecessary
			Idle();
			return;

		// SENSE DEVICE STATUS
		case sense_dev_stat:
			fdc.us = fdc.in_pkt[1] & 0x03;
			fdc.hd = fdc.in_pkt[1] & 0x04;

			// Result phase
			Result();
			return;

		// RECALIBRATE
		case recalibrate:
			// Seek to track 0
			fdc.us = fdc.in_pkt[1] & 0x03;
			fdc.cyl[fdc.us] = 0;

			// Create SR (SEEK sub command executes, Non-Busy)
			fdc.sr &= 0xf0;
			fdc.sr &= ~sr_cb;
			fdc.sr &= ~sr_rqm;
			fdc.sr |= (1 << fdc.dsr);

			// Execute later (will be called by CompleteSeek)
			fdd->Recalibrate(fdc.srt);
			return;

		// SEEK
		case seek:
			fdc.us = fdc.in_pkt[1] & 0x03;

			// Create SR (SEEK sub command executes, Non-Busy)
			fdc.sr &= 0xf0;
			fdc.sr &= ~sr_cb;
			fdc.sr &= ~sr_rqm;
			fdc.sr |= (1 << fdc.dsr);

			// Execute later (will be called by CompleteSeek)
			if (fdc.cyl[fdc.us] < fdc.in_pkt[2]) {
				// Step in
				fdd->StepIn(fdc.in_pkt[2] - fdc.cyl[fdc.us], fdc.srt);
			}
			else {
				// Step out
				fdd->StepOut(fdc.cyl[fdc.us] - fdc.in_pkt[2], fdc.srt);
			}
			fdc.cyl[fdc.us] = fdc.in_pkt[2];
			return;

		// READ ID
		case read_id:
			ReadID();
			return;

		// WRITE ID
		case write_id:
			fdc.us = fdc.in_pkt[1] & 0x03;
			fdc.hd = fdc.in_pkt[1] & 0x04;
			fdc.st[0] = fdc.us;
			fdc.st[0] |= fdc.hd;
			fdc.chrn[3] = fdc.in_pkt[2];
			fdc.sc = fdc.in_pkt[3];
			fdc.gpl = fdc.in_pkt[4];
			fdc.d = fdc.in_pkt[5];
			if (!WriteID()) {
				Result();
			}
			return;

		// READ DIAGNOSTIC
		case read_diag:
			ExecuteRW();
			if (!ReadDiag()) {
				Result();
			}
			return;

		// READ DATA
		case read_data:
			ExecuteRW();
			if (!ReadData()) {
				Result();
			}
			return;

		// READ DELETED DATA
		case read_del_data:
			ExecuteRW();
			if (!ReadData()) {
				Result();
			}
			return;

		// WRITE DATA
		case write_data:
			ExecuteRW();
			if (!WriteData()) {
				Result();
			}
			return;

		// WRITE DELETED_DATA
		case write_del_data:
			ExecuteRW();
			if (!WriteData()) {
				Result();
			}
			return;

		// SCAN family
		case scan_eq:
		case scan_lo_eq:
		case scan_hi_eq:
			ExecuteRW();
			if (!Scan()) {
				Result();
			}
			return;
	}

	LOG1(Log::Warning, "Execute phase unsupported command $%02X", fdc.in_pkt[0]);
}

//---------------------------------------------------------------------------
//
//	Execute phase (ReadID)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::ReadID()
{
	DWORD hus;

	ASSERT(this);

	// Valid HD, US
	fdc.us = fdc.in_pkt[1] & 0x03;
	fdc.hd = fdc.in_pkt[1] & 0x04;

	// FDD executes, if NOTREADY, NODATA, MAM error occurs
	fdc.err = fdd->ReadID(&(fdc.out_pkt[3]), fdc.mfm, fdc.hd);

	// If NOT READY, go to result phase
	if (fdc.err & FDD_NOTREADY) {
		Result();
		return;
	}

	// Set time until found
	hus = fdd->GetSearch();
	event.SetTime(hus);
	fdc.sr &= ~sr_rqm;
}

//---------------------------------------------------------------------------
//
//	Execute phase (Read/Write sub)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::ExecuteRW()
{
	ASSERT(this);

	// 8 byte packet split (last byte is usually DTL)
	fdc.us = fdc.in_pkt[1] & 0x03;
	fdc.hd = fdc.in_pkt[1] & 0x04;
	fdc.st[0] = fdc.us;
	fdc.st[0] |= fdc.hd;

	fdc.chrn[0] = fdc.in_pkt[2];
	fdc.chrn[1] = fdc.in_pkt[3];
	fdc.chrn[2] = fdc.in_pkt[4];
	fdc.chrn[3] = fdc.in_pkt[5];

	fdc.eot = fdc.in_pkt[6];
	fdc.gsl = fdc.in_pkt[7];
	fdc.dtl = fdc.in_pkt[8];
}

//---------------------------------------------------------------------------
//
//	Execute phase (Read)
//
//---------------------------------------------------------------------------
BYTE FASTCALL FDC::Read()
{
	BYTE data;

	ASSERT(fdc.len > 0);
	ASSERT(fdc.offset < 0x4000);

	// Buffer output
	data = fdc.buffer[fdc.offset];
	fdc.offset++;
	fdc.len--;

	// If not last, just return
	if (fdc.len > 0) {
		return data;
	}

	// For READ DIAGNOSTIC, end here
	if (fdc.cmd == read_diag) {
		// If no error, proceed to next sector
		if (fdc.err == FDD_NOERROR) {
			NextSector();
		}
		// End if event, go to result phase
		event.SetTime(0);
		Result();
		return data;
	}

	// If abnormal end, proceed to next sector
	if (fdc.err != FDD_NOERROR) {
		// End if event, go to result phase
		event.SetTime(0);
		Result();
		return data;
	}

	// Multi-sector processing
	if (!NextSector()) {
		// End if event, go to result phase
		event.SetTime(0);
		Result();
		return data;
	}

	// Read next sector
	if (!ReadData()) {
		// Sector read error
		event.SetTime(0);
		Result();
		return data;
	}

	// OK, next sector
	return data;
}

//---------------------------------------------------------------------------
//
//	Execute phase (Write)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Write(DWORD data)
{
	ASSERT(this);
	ASSERT(fdc.len > 0);
	ASSERT(fdc.offset < 0x4000);
	ASSERT(data < 0x100);

	// For WRITE ID, write to buffer
	if (fdc.cmd == write_id) {
		fdc.buffer[fdc.offset] = (BYTE)data;
		fdc.offset++;
		fdc.len--;

		// End check
		if (fdc.len == 0) {
			WriteBack();
			event.SetTime(0);
			Result();
		}
		return;
	}

	// For scan family, compare
	if ((fdc.cmd == scan_eq) || (fdc.cmd == scan_lo_eq) || (fdc.cmd == scan_hi_eq)) {
		Compare(data);
		return;
	}

	// Write data to buffer
	fdc.buffer[fdc.offset] = (BYTE)data;
	fdc.offset++;
	fdc.len--;

	// If not last, just return
	if (fdc.len > 0) {
		return;
	}

	// Write end
	WriteBack();
	if (fdc.err != FDD_NOERROR) {
		event.SetTime(0);
		Result();
		return;
	}

	// Multi-sector processing
	if (!NextSector()) {
		// End if event, go to result phase
		event.SetTime(0);
		Result();
		return;
	}

	// Write next sector
	if (!WriteData()) {
		event.SetTime(0);
		Result();
	}
}

//---------------------------------------------------------------------------
//
//	Execute phase (Compare)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Compare(DWORD data)
{
	ASSERT(this);
	ASSERT(data < 0x100);

	if (data != 0xff) {
		// In valid byte, if not skip
		if (!(fdc.err & FDD_SCANNOT)) {
			// Comparison required
			switch (fdc.cmd) {
				case scan_eq:
					if (fdc.buffer[fdc.offset] != (BYTE)data) {
						fdc.err |= FDD_SCANNOT;
					}
					break;
				case scan_lo_eq:
					if (fdc.buffer[fdc.offset] > (BYTE)data) {
						fdc.err |= FDD_SCANNOT;
					}
					break;
				case scan_hi_eq:
					if (fdc.buffer[fdc.offset] < (BYTE)data) {
						fdc.err |= FDD_SCANNOT;
					}
					break;
				default:
					ASSERT(FALSE);
					break;
			}

		}
	}

	// Next data
	fdc.offset++;
	fdc.len--;

	// If not last, just return
	if (fdc.len > 0) {
		return;
	}

	// If not last, combine result
	if (!(fdc.err & FDD_SCANNOT)) {
		// ok!
		fdc.err |= FDD_SCANEQ;
		event.SetTime(0);
		Result();
	}

	// For STP of 2, +1
	if (fdc.dtl == 0x02) {
		fdc.chrn[2]++;
	}

	// Multi-sector processing
	if (!NextSector()) {
		// SCAN NOT is raised, continue as is, not good
		event.SetTime(0);
		Result();
		return;
	}

	// Read next sector
	if (!Scan()) {
		// SCAN NOT is raised, continue as is, not good
		event.SetTime(0);
		Result();
	}

	// SCAN NOT is raised, proceed to next sector
	fdc.err &= ~FDD_SCANNOT;
}

//---------------------------------------------------------------------------
//
//	Result phase
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Result()
{
	ASSERT(this);

	// Result phase
	fdc.phase = result;
	fdc.sr |= sr_rqm;
	fdc.sr |= sr_dio;
	fdc.sr &= ~sr_ndm;

	// Command branch
	switch (fdc.cmd) {
		// SENSE DEVICE STATUS
		case sense_dev_stat:
			// Create ST3, data output
			MakeST3();
			fdc.out_pkt[0] = fdc.st[3];
			fdc.out_len = 1;
			fdc.out_cnt = 0;
			return;

		// SENSE INTERRUPT STATUS
		case sense_int_stat:
			// Return ST0, cylinder. Data output
			fdc.out_pkt[0] = fdc.st[0];
			fdc.out_pkt[1] = fdc.cyl[fdc.us];
			fdc.out_len = 2;
			fdc.out_cnt = 0;
			return;

		// READ ID
		case read_id:
			// Create ST0,ST1,ST2. If NOTREADY, NODATA, MAM error occurs
			fdc.st[0] = fdc.us;
			fdc.st[0] |= fdc.hd;
			if (fdc.err & FDD_NOTREADY) {
				// Not Ready
				fdc.st[0] |= 0x08;
				fdc.st[1] = 0;
				fdc.st[2] = 0;
			}
			else {
				if (fdc.err != FDD_NOERROR) {
					// Abnormal Teriminate
					fdc.st[0] |= 0x40;
				}
				fdc.st[1] = fdc.err >> 8;
				fdc.st[2] = fdc.err & 0xff;
			}

			// Data output, go to result phase, interrupt
			fdc.out_pkt[0] = fdc.st[0];
			fdc.out_pkt[1] = fdc.st[1];
			fdc.out_pkt[2] = fdc.st[2];
			fdc.out_len = 7;
			fdc.out_cnt = 0;
			Interrupt(TRUE);
			return;

		// INVALID, RESET STANDBY
		case invalid:
		case reset_stdby:
			fdc.out_pkt[0] = 0x80;
			fdc.out_len = 1;
			fdc.out_cnt = 0;
			return;

		// READ,WRITE,SCAN family
		case read_data:
		case read_del_data:
		case write_data:
		case write_del_data:
		case scan_eq:
		case scan_lo_eq:
		case scan_hi_eq:
		case read_diag:
		case write_id:
			ResultRW();
			return;
	}

	LOG1(Log::Warning, "Result phase unsupported command $%02X", fdc.in_pkt[0]);
}

//---------------------------------------------------------------------------
//
//	Result phase (Read/Write sub)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::ResultRW()
{
	ASSERT(this);

	// Create ST0,ST1,ST2
	if (fdc.err & FDD_NOTREADY) {
		// Not Ready
		fdc.st[0] |= 0x08;
		fdc.st[1] = 0;
		fdc.st[2] = 0;
	}
	else {
		if ((fdc.err != FDD_NOERROR) && (fdc.err != FDD_SCANEQ)) {
			// Abnormal Teriminate
			fdc.st[0] |= 0x40;
		}
		fdc.st[1] = fdc.err >> 8;
		fdc.st[2] = fdc.err & 0xff;
	}

	// READ DIAGNOSTIC outputs 0x40 or not
	if (fdc.cmd == read_diag) {
		if (fdc.st[0] & 0x40) {
			fdc.st[0] &= ~0x40;
		}
	}

	// Set result packet
	fdc.out_pkt[0] = fdc.st[0];
	fdc.out_pkt[1] = fdc.st[1];
	fdc.out_pkt[2] = fdc.st[2];
	fdc.out_pkt[3] = fdc.chrn[0];
	fdc.out_pkt[4] = fdc.chrn[1];
	fdc.out_pkt[5] = fdc.chrn[2];
	fdc.out_pkt[6] = fdc.chrn[3];
	fdc.out_len = 7;
	fdc.out_cnt = 0;

	// Usually, go to result phase, interrupt
	Interrupt(TRUE);
}

//---------------------------------------------------------------------------
//
//	Interrupt
//
//---------------------------------------------------------------------------
void FASTCALL FDC::Interrupt(BOOL flag)
{
	ASSERT(this);

	// Notify IOSC
	iosc->IntFDC(flag);
}

//---------------------------------------------------------------------------
//
//	ST3 creation
//
//---------------------------------------------------------------------------
void FASTCALL FDC::MakeST3()
{
	ASSERT(this);

	// Reset HD,US
	fdc.st[3] = fdc.hd;
	fdc.st[3] |= fdc.us;

	// If ready
	if (fdd->IsReady(fdc.dsr)) {
		// Ready
		fdc.st[3] |= 0x20;

		// Write protect
		if (fdd->IsWriteP(fdc.dsr)) {
			fdc.st[3] |= 0x40;
		}
	}
	else {
		// Not ready
		fdc.st[3] = 0x40;
	}

	// TRACK0 check
	if (fdd->GetCylinder(fdc.dsr) == 0) {
		fdc.st[3] |= 0x10;
	}
}

//---------------------------------------------------------------------------
//
//	READ (DELETED) DATA command
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::ReadData()
{
	int len;
	DWORD hus;

	ASSERT(this);
	ASSERT((fdc.cmd == read_data) || (fdc.cmd == read_del_data));

	// SR setting
	fdc.sr |= sr_cb;
	fdc.sr |= sr_dio;
	fdc.sr &= ~sr_d3b;
	fdc.sr &= ~sr_d2b;
	fdc.sr &= ~sr_d1b;
	fdc.sr &= ~sr_d0b;

	// Read from drive, if NOTREADY,NODATA,MAM,CYL end,CRC end,DDAM
#if defined(FDC_LOG)
	LOG4(Log::Normal, "(C:%02X H:%02X R:%02X N:%02X)",
		fdc.chrn[0], fdc.chrn[1], fdc.chrn[2], fdc.chrn[3]);
#endif
	fdc.err = fdd->ReadSector(fdc.buffer, &fdc.len,
									fdc.mfm, fdc.chrn, fdc.hd);

	// DDAM(Deleted Sector) valid, CM(Control Mark) should be set
	if (fdc.cmd == read_data) {
		// Read Data (DDAM is error)
		if (fdc.err & FDD_DDAM) {
			fdc.err &= ~FDD_DDAM;
			fdc.err |= FDD_CM;
		}
	}
	else {
		// Read Deleted Data (if not DDAM, error)
		if (!(fdc.err & FDD_DDAM)) {
			fdc.err |= FDD_CM;
		}
		fdc.err &= ~FDD_DDAM;
	}

	// If IDCRC or DATACRC, ADATAERR is expected
	if ((fdc.err & FDD_IDCRC) || (fdc.err & FDD_DATACRC)) {
		fdc.err &= ~FDD_IDCRC;
		fdc.err |= FDD_DATAERR;
	}

	// For N=0, use DTL instead
	if (fdc.chrn[3] == 0) {
		len = 1 << (fdc.dtl + 7);
		if (len < fdc.len) {
			fdc.len = len;
		}
	}
	else {
		// (Mac G3 exception)
		len = (1 << (fdc.chrn[3] + 7));
		if (len < fdc.len) {
			fdc.len = len;
		}
	}

	// Not Ready goes to result phase
	if (fdc.err & FDD_NOTREADY) {
		return FALSE;
	}

	// CM and SK=1, go to result phase (i82078 data sheet)
	if (fdc.err & FDD_CM) {
		if (fdc.sk) {
			return FALSE;
		}
	}

	// Calculate remaining time (head load is not considered)
	hus = fdd->GetSearch();

	// No Data goes to previous time, go to result phase
	if (fdc.err & FDD_NODATA) {
		EventErr(hus);
		return TRUE;
	}

	// Offset reset, event start, ER phase start
	fdc.offset = 0;
	EventRW();
	fdc.phase = read;

	// If remaining time < head load time, add
	if (!fdc.load) {
		if (hus < fdc.hlt) {
			hus += fdd->GetRotationTime();
		}
	}

	// Time addition
	if (!fdc.fast) {
		hus += event.GetTime();
		event.SetTime(hus);
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WRITE (DELETED) DATA command
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::WriteData()
{
	int len;
	DWORD hus;
	BOOL deleted;

	ASSERT(this);
	ASSERT((fdc.cmd == write_data) || (fdc.cmd == write_del_data));

	// SR setting
	fdc.sr |= sr_cb;
	fdc.sr &= ~sr_dio;
	fdc.sr &= ~sr_d3b;
	fdc.sr &= ~sr_d2b;
	fdc.sr &= ~sr_d1b;
	fdc.sr &= ~sr_d0b;

	// Read from drive, if NOTREADY,NOTWRITE,NODATA,MAM,CYL end,IDCRC,DDAM
	deleted = FALSE;
	if (fdc.cmd == write_del_data) {
		deleted = TRUE;
	}
#if defined(FDC_LOG)
	LOG4(Log::Normal, "(C:%02X H:%02X R:%02X N:%02X)",
		fdc.chrn[0], fdc.chrn[1], fdc.chrn[2], fdc.chrn[3]);
#endif
	fdc.err = fdd->WriteSector(NULL, &fdc.len,
									fdc.mfm, fdc.chrn, fdc.hd, deleted);
	fdc.err &= ~FDD_DDAM;

	// If IDCRC, ADATAERR is expected
	if (fdc.err & FDD_IDCRC) {
		fdc.err &= ~FDD_IDCRC;
		fdc.err |= FDD_DATAERR;
	}

	// For N=0, use DTL instead
	if (fdc.chrn[3] == 0) {
		len = 1 << (fdc.dtl + 7);
		if (len < fdc.len) {
			fdc.len = len;
		}
	}
	else {
		len = (1 << (fdc.chrn[3] + 7));
		if (len < fdc.len) {
			fdc.len = len;
		}
	}

	// Not Ready, Not Writable goes to result phase
	if ((fdc.err & FDD_NOTREADY) || (fdc.err & FDD_NOTWRITE)) {
		return FALSE;
	}

	// Calculate remaining time (head load is not considered)
	hus = fdd->GetSearch();

	// No Data fails, go to result
	if (fdc.err & FDD_NODATA) {
		EventErr(hus);
		return TRUE;
	}

	// Offset reset, event setting, EW phase start
	fdc.offset = 0;
	EventRW();
	fdc.phase = write;

	// If remaining time < head load time, add
	if (!fdc.load) {
		if (hus < fdc.hlt) {
			hus += fdd->GetRotationTime();
		}
	}

	// Time addition
	if (!fdc.fast) {
		hus += event.GetTime();
		event.SetTime(hus);
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SCAN family command
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::Scan()
{
	int len;
	DWORD hus;

	ASSERT(this);

	// SR setting
	fdc.sr |= sr_cb;
	fdc.sr &= ~sr_dio;
	fdc.sr &= ~sr_d3b;
	fdc.sr &= ~sr_d2b;
	fdc.sr &= ~sr_d1b;
	fdc.sr &= ~sr_d0b;

	// Read from drive, if NOTREADY,NODATA,MAM,CYL end,CRC end,DDAM
#if defined(FDC_LOG)
	LOG4(Log::Normal, "(C:%02X H:%02X R:%02X N:%02X)",
		fdc.chrn[0], fdc.chrn[1], fdc.chrn[2], fdc.chrn[3]);
#endif
	fdc.err = fdd->ReadSector(fdc.buffer, &fdc.len,
									fdc.mfm, fdc.chrn, fdc.hd);

	// DDAM(Deleted Sector) valid, CM(Control Mark) should be set
	if (fdc.err & FDD_DDAM) {
		fdc.err &= ~FDD_DDAM;
		fdc.err |= FDD_CM;
	}

	// If IDCRC or DATACRC, ADATAERR is expected
	if ((fdc.err & FDD_IDCRC) || (fdc.err & FDD_DATACRC)) {
		fdc.err &= ~FDD_IDCRC;
		fdc.err |= FDD_DATAERR;
	}

	// For N=0, use DTL instead
	if (fdc.chrn[3] == 0) {
		len = 1 << (fdc.dtl + 7);
		if (len < fdc.len) {
			fdc.len = len;
		}
	}
	else {
		len = (1 << (fdc.chrn[3] + 7));
		if (len < fdc.len) {
			fdc.len = len;
		}
	}

	// Not Ready goes to result phase
	if (fdc.err & FDD_NOTREADY) {
		return FALSE;
	}

	// CM and SK=1, go to result phase (i82078 data sheet)
	if (fdc.err & FDD_CM) {
		if (fdc.sk) {
			return FALSE;
		}
	}

	// Calculate remaining time (head load is not considered)
	hus = fdd->GetSearch();

	// No Data goes to previous time, go to result phase
	if (fdc.err & FDD_NODATA) {
		EventErr(hus);
		return TRUE;
	}

	// Offset reset, event start, ER phase start
	fdc.offset = 0;
	EventRW();
	fdc.phase = write;

	// If remaining time < head load time, add
	if (!fdc.load) {
		if (hus < fdc.hlt) {
			hus += fdd->GetRotationTime();
		}
	}

	// Time addition
	if (!fdc.fast) {
		hus += event.GetTime();
		event.SetTime(hus);
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ DIAGNOSTIC command
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::ReadDiag()
{
	DWORD hus;

	ASSERT(this);
	ASSERT(fdc.cmd == read_diag);

	// SR setting
	fdc.sr |= sr_cb;
	fdc.sr |= sr_dio;
	fdc.sr &= ~sr_d3b;
	fdc.sr &= ~sr_d2b;
	fdc.sr &= ~sr_d1b;
	fdc.sr &= ~sr_d0b;

	// EOT=0 goes to result phase (NO DATA)
	if (fdc.eot == 0) {
		if (fdd->IsReady(fdc.dsr)) {
			fdc.err = FDD_NODATA;
		}
		else {
			fdc.err = FDD_NOTREADY;
		}
		return FALSE;
	}

	// Read from drive, if NOTREADY,NODATA,MAM,CRC end,DDAM
	fdc.err = fdd->ReadDiag(fdc.buffer, &fdc.len, fdc.mfm,
								fdc.chrn, fdc.hd);
	// Not Ready goes to result phase
	if (fdc.err & FDD_NOTREADY) {
		return FALSE;
	}

	// Calculate remaining time (head load is not considered)
	hus = fdd->GetSearch();

	// MAM waits for time, go to result phase, NODATA is also error
	if (fdc.err & FDD_MAM) {
		EventErr(hus);
		return TRUE;
	}

	ASSERT(fdc.len > 0);

	// Offset reset, event start, ER phase start
	fdc.offset = 0;
	EventRW();
	fdc.phase = read;

	// Time addition
	if (!fdc.fast) {
		hus += event.GetTime();
		event.SetTime(hus);
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WRITE ID command
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::WriteID()
{
	DWORD hus;

	ASSERT(this);
	ASSERT(fdc.cmd == write_id);

	// SR setting
	fdc.sr |= sr_cb;
	fdc.sr &= ~sr_dio;
	fdc.sr &= ~sr_d3b;
	fdc.sr &= ~sr_d2b;
	fdc.sr &= ~sr_d1b;
	fdc.sr &= ~sr_d0b;

	// SC=0 check
	if (fdc.sc == 0) {
		fdc.err = 0;
		return FALSE;
	}

	// Read from drive, if NOTREADY,NOTWRITE
	fdc.err = fdd->WriteID(NULL, fdc.d, fdc.sc, fdc.mfm, fdc.hd, fdc.gpl);
	// Not Ready, Not Writable goes to result phase
	if ((fdc.err & FDD_NOTREADY) || (fdc.err & FDD_NOTWRITE)) {
		return FALSE;
	}

	// Offset reset
	fdc.offset = 0;
	fdc.len = fdc.sc * 4;

	// Event setting
	if (fdc.ndm) {
		fdc.sr |= sr_ndm;
		LOG0(Log::Warning, "Non-DMA mode Write ID");
	}
	else {
		fdc.sr &= ~sr_ndm;
	}
	// N is limited to 7 or less (N=7 is 16KB/sector, alternate format)
	if (fdc.chrn[3] > 7) {
		fdc.chrn[3] = 7;
	}

	// Time setting. Fill with one sector
	hus = fdd->GetSearch();
	hus += (fdd->GetRotationTime() / fdc.sc);
	if (fdc.fast) {
		hus = 32 * 4;
	}

	// Event start, RQM clear
	event.SetTime(hus);
	fdc.sr &= ~sr_rqm;
	fdc.phase = write;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Event (Read/Write sub)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::EventRW()
{
	DWORD hus;

	// SR setting (Non-DMA)
	if (fdc.ndm) {
		fdc.sr |= sr_ndm;
	}
	else {
		fdc.sr &= ~sr_ndm;
	}

	// Event creation
	if (fdc.ndm) {
		// Non-DMA. 16us/32us
		if (fdc.mfm) {
			hus = 32;
		}
		else {
			hus = 64;
		}
	}
	else {
		// DMA. 64 bytes at once. 1024us/2048us
		if (fdc.mfm) {
			hus = 32 * 64;
		}
		else {
			hus = 64 * 64;
		}
	}

	// DD is double
	if (!fdd->IsHD()) {
		hus <<= 1;
	}

	// fast mode is 64us limit (DMA exception)
	if (fdc.fast) {
		if (!fdc.ndm) {
			hus = 128;
		}
	}

	// Event start, RQM clear
	event.SetTime(hus);
	fdc.sr &= ~sr_rqm;
}

//---------------------------------------------------------------------------
//
//	Event (Error)
//
//---------------------------------------------------------------------------
void FASTCALL FDC::EventErr(DWORD hus)
{
	// SR setting (Non-DMA)
	if (fdc.ndm) {
		fdc.sr |= sr_ndm;
	}
	else {
		fdc.sr &= ~sr_ndm;
	}

	// Event start, RQM clear
	event.SetTime(hus);
	fdc.sr &= ~sr_rqm;
	fdc.phase = execute;
}

//---------------------------------------------------------------------------
//
//	Write back
//
//---------------------------------------------------------------------------
void FASTCALL FDC::WriteBack()
{
	switch (fdc.cmd) {
		// Write Data
		case write_data:
			fdc.err = fdd->WriteSector(fdc.buffer, &fdc.len,
							fdc.mfm, fdc.chrn, fdc.hd, FALSE);
			return;

		// Write Deleted Data
		case write_del_data:
			fdc.err = fdd->WriteSector(fdc.buffer, &fdc.len,
							fdc.mfm, fdc.chrn, fdc.hd, TRUE);
			return;

		// Write ID
		case write_id:
			fdc.err = fdd->WriteID(fdc.buffer, fdc.d, fdc.sc,
							fdc.mfm, fdc.hd, fdc.gpl);
			return;
	}

	// Should not be reached
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Multi-sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDC::NextSector()
{
	// TC check
	if (fdc.tc) {
		// C,H,R,N direct move
		if (fdc.chrn[2] < fdc.eot) {
			fdc.chrn[2]++;
			return FALSE;
		}
		fdc.chrn[2] = 0x01;
		// Side change by MT
		if (fdc.mt && (!(fdc.chrn[1] & 0x01))) {
			// Side 1
			fdc.chrn[1] |= 0x01;
			fdc.hd |= 0x04;
			return FALSE;
		}
		// C+1, R=1 end
		fdc.chrn[0]++;
		return FALSE;
	}

	// EOT check
	if (fdc.chrn[2] < fdc.eot) {
		fdc.chrn[2]++;
		return TRUE;
	}

	// EOT, R=1
	fdc.err |= FDD_EOT;
	fdc.chrn[2] = 0x01;

	// Side change by MT
	if (fdc.mt && (!(fdc.chrn[1] & 0x01))) {
		// Side 1
		fdc.chrn[1] |= 0x01;
		fdc.hd |= 0x04;
		return TRUE;
	}

	// C+1, R=1 end
	fdc.chrn[0]++;
	return FALSE;
}
