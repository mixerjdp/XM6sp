//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ I/O Controller (IOSC-2) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "cpu.h"
#include "vm.h"
#include "log.h"
#include "fileio.h"
#include "schedule.h"
#include "printer.h"
#include "iosc.h"

//===========================================================================
//
//	IOSC
//
//===========================================================================
//#define IOSC_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
IOSC::IOSC(VM *p) : MemDevice(p)
{
	// Device ID creation
	dev.id = MAKEID('I', 'O', 'S', 'C');
	dev.desc = "I/O Ctrl (IOSC-2)";

	// Start address, I/O address
	memdev.first = 0xe9c000;
	memdev.last = 0xe9dfff;

	// Others
	printer = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL IOSC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get printer
	printer = (Printer*)vm->SearchDevice(MAKEID('P', 'R', 'N', ' '));
	ASSERT(printer);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::Cleanup()
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
void FASTCALL IOSC::Reset()
{
	LOG0(Log::Normal, "Reset");

	// Internal state clear
	iosc.prt_int = FALSE;
	iosc.prt_en = FALSE;
	iosc.fdd_int = FALSE;
	iosc.fdd_en = FALSE;
	iosc.fdc_int = FALSE;
	iosc.fdc_en = FALSE;
	iosc.hdc_int = FALSE;
	iosc.hdc_en = FALSE;
	iosc.vbase = 0;

	// Vector none
	iosc.vector = -1;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL IOSC::Save(Fileio *fio, int /*ver*/)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);
	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(iosc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save data
	if (!fio->Write(&iosc, sizeof(iosc))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL IOSC::Load(Fileio *fio, int /*ver*/)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);
	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(iosc_t)) {
		return FALSE;
	}

	// Load data
	if (!fio->Read(&iosc, sizeof(iosc))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::ApplyCfg(const Config* /*config*/)
{
 ASSERT(this);
	LOG0(Log::Normal, "Apply config");
}

//---------------------------------------------------------------------------
//
//	Read byte
//
//---------------------------------------------------------------------------
DWORD FASTCALL IOSC::ReadByte(DWORD addr)
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// 16-byte unit loop
	addr &= 0x0f;

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {

		// Wait
		scheduler->Wait(2);

		// $E9C001 Interrupt status
		if (addr == 1) {
			data = 0;
			if (iosc.fdc_int) {
				data |= 0x80;
			}
			if (iosc.fdd_int) {
				data |= 0x40;
			}
			if (iosc.hdc_int) {
				data |= 0x10;
			}
			if (iosc.hdc_en) {
				data |= 0x08;
			}
			if (iosc.fdc_en) {
				data |= 0x04;
			}
			if (iosc.fdd_en) {
				data |= 0x02;
			}
			if (iosc.prt_en) {
				data |= 0x01;
			}

			// Printer READY display
			if (printer->IsReady()) {
				data |= 0x20;
			}

			// Printer interrupt is cleared at this point
			if (iosc.prt_int) {
				iosc.prt_int = FALSE;
				IntChk();
			}
			return data;
		}

		// $E9C003 Interrupt vector
		if (addr == 3) {
			return 0xff;
		}

		LOG1(Log::Warning, "Undefined I/O register read $%06X", memdev.first + addr);
		return 0xff;
	}

	// Upper errors are ignored
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Read word
//
//---------------------------------------------------------------------------
DWORD FASTCALL IOSC::ReadWord(DWORD addr)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);

	return (0xff00 | ReadByte(addr + 1));
}

//---------------------------------------------------------------------------
//
//	Write byte
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);

	// 16-byte unit loop
	addr &= 0x0f;

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {

		// Wait
		scheduler->Wait(2);

		// $E9C001 Interrupt mask
		if (addr == 1) {
			if (data & 0x01) {
				iosc.prt_en = TRUE;
			}
			else {
				iosc.prt_en = FALSE;
			}
			if (data & 0x02) {
				iosc.fdd_en = TRUE;
			}
			else {
				iosc.fdd_en = FALSE;
			}
			if (data & 0x04) {
				iosc.fdc_en = TRUE;
			}
			else {
				iosc.fdc_en = FALSE;
			}
			if (data & 0x08) {
				iosc.hdc_en = TRUE;
			}
			else {
				iosc.hdc_en = FALSE;
			}

			// Interrupt check (FORMULA 68K)
			IntChk();
			return;
		}

		// $E9C003 Interrupt vector
		if (addr == 3) {
			data &= 0xfc;
			iosc.vbase &= 0x03;
			iosc.vbase |= data;

			LOG1(Log::Detail, "Interrupt vector base $%02X", iosc.vbase);
			return;
		}

		// Invalid
		LOG2(Log::Warning, "Undefined I/O register write $%06X <- $%02X",
										memdev.first + addr, data);
		return;
	}
}

//---------------------------------------------------------------------------
//
//	Write word
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::WriteWord(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT((addr & 1) == 0);
 ASSERT(data < 0x10000);

	WriteByte(addr + 1, (BYTE)data);
}

//---------------------------------------------------------------------------
//
//	Read only
//
//---------------------------------------------------------------------------
DWORD FASTCALL IOSC::ReadOnly(DWORD addr) const
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// 16-byte unit loop
	addr &= 0x0f;

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {

		// $E9C001 Interrupt status
		if (addr == 1) {
			data = 0;
			if (iosc.fdc_int) {
				data |= 0x80;
			}
			if (iosc.fdd_int) {
				data |= 0x40;
			}
			if (iosc.prt_int) {
				data |= 0x20;
			}
			if (iosc.hdc_int) {
				data |= 0x10;
			}
			if (iosc.hdc_en) {
				data |= 0x08;
			}
			if (iosc.fdc_en) {
				data |= 0x04;
			}
			if (iosc.fdd_en) {
				data |= 0x02;
			}
			if (iosc.prt_en) {
				data |= 0x01;
			}
			return data;
		}

		// $E9C003 Interrupt vector
		if (addr == 3) {
			return 0xff;
		}

		// Invalid address
		return 0xff;
	}

	// Even address
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get IOSC
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::GetIOSC(iosc_t *buffer) const
{
 ASSERT(this);
 ASSERT(buffer);

	// Copy internal state
	*buffer = iosc;
}

//---------------------------------------------------------------------------
//
//	Interrupt check
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntChk()
{
	int v;

 ASSERT(this);

	// Clear interrupt pending
	v = -1;

	// Printer interrupt
	if (iosc.prt_int && iosc.prt_en) {
		v = iosc.vbase + 3;
	}

	// HDC interrupt
	if (iosc.hdc_int && iosc.hdc_en) {
		v = iosc.vbase + 2;
	}

	// FDD interrupt
	if (iosc.fdd_int && iosc.fdd_en) {
		v = iosc.vbase + 1;
	}

	// FDC interrupt
	if (iosc.fdc_int && iosc.fdc_en) {
		v = iosc.vbase;
	}

	// If no devices requesting interrupt
	if (v < 0) {
		// If no interrupt requested, OK
		if (iosc.vector < 0) {
			return;
		}

		// Since other interrupts are being requested, cancel interrupt
#if defined(IOSC_LOG)
		LOG1(Log::Normal, "Interrupt cancel vector$%02X", iosc.vector);
#endif	// IOSC_LOG
		cpu->IntCancel(1);
		iosc.vector = -1;
		return;
	}

	// If same vector as currently requested, OK
	if (iosc.vector == v) {
		return;
	}

	// Different, so cancel pending
	if (iosc.vector >= 0) {
#if defined(IOSC_LOG)
		LOG1(Log::Normal, "Interrupt cancel vector$%02X", iosc.vector);
#endif	// IOSC_LOG
		cpu->IntCancel(1);
		iosc.vector = -1;
	}

	// Interrupt request
#if defined(IOSC_LOG)
	LOG1(Log::Normal, "Interrupt request vector$%02X", v);
#endif	// IOSC_LOG
	cpu->Interrupt(1, (BYTE)v);

	// Save
	iosc.vector = v;
}

//---------------------------------------------------------------------------
//
//	Interrupt acknowledge
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntAck()
{
 ASSERT(this);

	// After reset, if CPU acknowledges interrupt before device
	// Or other devices
	if (iosc.vector < 0) {
#if defined(IOSC_LOG)
		LOG0(Log::Warning, "Unexpected interrupt acknowledge");
#endif	// IOSC_LOG
		return;
	}

#if defined(IOSC_LOG)
	LOG1(Log::Normal, "Interrupt ACK vector$%02X", iosc.vector);
#endif	// IOSC_LOG

	// Clear interrupt
	iosc.vector = -1;
}

//---------------------------------------------------------------------------
//
//	FDC interrupt
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntFDC(BOOL flag)
{
 ASSERT(this);

	iosc.fdc_int = flag;

	// Interrupt check
	IntChk();
}

//---------------------------------------------------------------------------
//
//	FDD interrupt
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntFDD(BOOL flag)
{
 ASSERT(this);

	iosc.fdd_int = flag;

	// Interrupt check
	IntChk();
}

//---------------------------------------------------------------------------
//
//	HDC interrupt
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntHDC(BOOL flag)
{
 ASSERT(this);

	iosc.hdc_int = flag;

	// Interrupt check
	IntChk();
}

//---------------------------------------------------------------------------
//
//	Printer interrupt
//
//---------------------------------------------------------------------------
void FASTCALL IOSC::IntPRT(BOOL flag)
{
 ASSERT(this);

	iosc.prt_int = flag;

	// Interrupt check
	IntChk();
}
