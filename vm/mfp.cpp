//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ MFP(MC68901) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "cpu.h"
#include "vm.h"
#include "log.h"
#include "event.h"
#include "schedule.h"
#include "keyboard.h"
#include "fileio.h"
#include "sync.h"
#include "mfp.h"

//===========================================================================
//
//	MFP
//
//===========================================================================
//#define MFP_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
MFP::MFP(VM *p) : MemDevice(p)
{
	// Device ID creation
	dev.id = MAKEID('M', 'F', 'P', ' ');
	dev.desc = "MFP (MC68901)";

	// Start address, I/O address
	memdev.first = 0xe88000;
	memdev.last = 0xe89fff;

	// Sync object
	sync = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL MFP::Init()
{
	int i;
	char buf[0x20];

 ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Sync create
	sync = new Sync;

	// Timer event create
	for (i=0; i<4; i++) {
		timer[i].SetDevice(this);
		sprintf(buf, "Timer-%c", 'A' + i);
		timer[i].SetDesc(buf);
		timer[i].SetUser(i);
		timer[i].SetTime(0);

		// Timer-B is used for event count, so not used
		if (i != 1) {
			scheduler->AddEvent(&timer[i]);
		}
	}

	// Keyboard get
	keyboard = (Keyboard*)vm->SearchDevice(MAKEID('K', 'E', 'Y', 'B'));

	// USART event create
	// 1(us)x13(clock)x(clock50%)x16(divider)x10(bit)=2400bps
	usart.SetDevice(this);
	usart.SetUser(4);
	usart.SetDesc("USART 2400bps");
	usart.SetTime(8320);
	scheduler->AddEvent(&usart);

	// Reset does not set values, so set here (data format 3.3)
	for (i=0; i<4; i++) {
		if (i == 0) {
			// Timer-A, set to 1, VDISPST does not start (DiskX)
			SetTDR(i, 1);
			mfp.tir[i] = 1;
		}
		else {
			// Timer-B,Timer-C,Timer-D = 0
			SetTDR(i, 0);
			mfp.tir[i] = 0;
		}
	}
	mfp.tsr = 0;
	mfp.rur = 0;

	// Reset clear keyboard to send $FF, so initialize at Init
	sync->Lock();
	mfp.datacount = 0;
	mfp.readpoint = 0;
	mfp.writepoint = 0;
	sync->Unlock();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Cleanup()
{
 ASSERT(this);

	// Sync delete
	if (sync) {
		delete sync;
		sync = NULL;
	}

	// Base class cleanup
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Reset()
{
	int i;

 ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Interrupt controller
	mfp.vr = 0;
	mfp.iidx = -1;
	for (i=0; i<0x10; i++) {
		mfp.ier[i] = FALSE;
		mfp.ipr[i] = FALSE;
		mfp.imr[i] = FALSE;
		mfp.isr[i] = FALSE;
		mfp.ireq[i] = FALSE;
	}

	// Timer
	for (i=0; i<4; i++) {
		timer[i].SetTime(0);
		SetTCR(i, 0);
	}
	mfp.tbr[0] = 0;
	mfp.tbr[1] = 0;
	mfp.sram = 0;
	mfp.tecnt = 0;

	// GPIP (GPIP5 is power failure)
	mfp.gpdr = 0;
	mfp.aer = 0;
	mfp.ddr = 0;
	mfp.ber = (DWORD)~mfp.aer;
	mfp.ber ^= mfp.gpdr;
	SetGPIP(5, 1);

	// USART
	mfp.scr = 0;
	mfp.ucr = 0;
	mfp.rsr = 0;
	mfp.tsr = (DWORD)(mfp.tsr & ~0x01);
	mfp.tur = 0;

	// GPIP set (power related)
	SetGPIP(1, 1);
	if (vm->IsPowerSW()) {
		SetGPIP(2, 0);
	}
	else {
		SetGPIP(2, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL MFP::Save(Fileio *fio, int ver)
{
	size_t sz;
	int i;

 ASSERT(this);
	LOG0(Log::Normal, "Save");

	// Main
	sz = sizeof(mfp_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&mfp, (int)sz)) {
		return FALSE;
	}

	// Event(Timer)
	for (i=0; i<4; i++) {
		if (!timer[i].Save(fio, ver)) {
			return FALSE;
		}
	}

	// Event(USART)
	if (!usart.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL MFP::Load(Fileio *fio, int ver)
{
	int i;
	size_t sz;

 ASSERT(this);
	LOG0(Log::Normal, "Load");

	// Main
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(mfp_t)) {
		return FALSE;
	}
	if (!fio->Read(&mfp, (int)sz)) {
		return FALSE;
	}

	// Event(Timer)
	for (i=0; i<4; i++) {
		if (!timer[i].Load(fio, ver)) {
			return FALSE;
		}
	}

	// Event(USART)
	if (!usart.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL MFP::ApplyCfg(const Config* /*config*/)
{
 ASSERT(this);

	LOG0(Log::Normal, "Apply config");
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::ReadByte(DWORD addr)
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Only odd addresses are decoded
	if ((addr & 1) != 0) {
		// Wait
		scheduler->Wait(3);

		// 64-byte unit loop
		addr &= 0x3f;
		addr >>= 1;

		switch (addr) {
			// GPIP
			case 0x00:
				return mfp.gpdr;

			// AER
			case 0x01:
				return mfp.aer;

			// DDR
			case 0x02:
				return mfp.ddr;

			// IER(A)
			case 0x03:
				return GetIER(0);

			// IER(B)
			case 0x04:
				return GetIER(1);

			// IPR(A)
			case 0x05:
				return GetIPR(0);

			// IPR(B)
			case 0x06:
				return GetIPR(1);

			// ISR(A)
			case 0x07:
				return GetISR(0);

			// ISR(B)
			case 0x08:
				return GetISR(1);

			// IMR(A)
			case 0x09:
				return GetIMR(0);

			// IMR(B)
			case 0x0a:
				return GetIMR(1);

			// VR
			case 0x0b:
				return GetVR();

			// Timer-A control
			case 0x0c:
				return GetTCR(0);

			// Timer-B control
			case 0x0d:
				return GetTCR(1);

			// Timer-C&D control
			case 0x0e:
				data = GetTCR(2);
				data <<= 4;
				data |= GetTCR(3);
				return data;

			// Timer-A data
			case 0x0f:
				return GetTIR(0);

			// Timer-B data
			case 0x10:
				return GetTIR(1);

			// Timer-C data
			case 0x11:
				return GetTIR(2);

			// Timer-D data
			case 0x12:
				return GetTIR(3);

			// SYNC register
			case 0x13:
				return mfp.scr;

			// USART control
			case 0x14:
				return mfp.ucr;

			// Receiver status
			case 0x15:
				return mfp.rsr;

			// Transmitter status
			case 0x16:
				// TE bit is cleared
				mfp.tsr = (DWORD)(mfp.tsr & ~0x40);
				return mfp.tsr;

			// USART data
			case 0x17:
				Receive();
				return mfp.rur;

			// Others
			default:
				LOG1(Log::Warning, "Undefined I/O register read R%02d", addr);
				return 0xff;
		}
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Word read
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::ReadWord(DWORD addr)
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
void FASTCALL MFP::WriteByte(DWORD addr, DWORD data)
{
 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));
 ASSERT(data < 0x100);

	// Only odd addresses are decoded
	if ((addr & 1) == 0) {
		// Upper errors are ignored
		return;
	}

	// 64-byte unit loop
	addr &= 0x3f;
	addr >>= 1;

	// Wait
	scheduler->Wait(3);

	switch (addr) {
		// GPIP
		case 0x00:
			// If VDISP checked with AND.B (MOON FIGHTER)
			SetGPDR(data);
			return;

		// AER
		case 0x01:
			mfp.aer = data;
			mfp.ber = (DWORD)(~data);
			mfp.ber ^= mfp.gpdr;
			IntGPIP();
			return;

		// DDR
		case 0x02:
			mfp.ddr = data;
			if (mfp.ddr != 0) {
				LOG0(Log::Warning, "GPIP output direction");
			}
			return;

		// IER(A)
		case 0x03:
			SetIER(0, data);
			return;

		// IER(B)
		case 0x04:
			SetIER(1, data);
			return;

		// IPR(A)
		case 0x05:
			SetIPR(0, data);
			return;

		// IPR(B)
		case 0x06:
			SetIPR(1, data);
			return;

		// ISR(A)
		case 0x07:
			SetISR(0, data);
			return;

		// ISR(B)
		case 0x08:
			SetISR(1, data);
			return;

		// IMR(A)
		case 0x09:
			SetIMR(0, data);
			return;

		// IMR(B)
		case 0x0a:
			SetIMR(1, data);
			return;

		// VR
		case 0x0b:
			SetVR(data);
			return;

		// Timer-A control
		case 0x0c:
			SetTCR(0, data);
			return;

		// Timer-B control
		case 0x0d:
			SetTCR(1, data);
			return;

		// Timer-C&D control
		case 0x0e:
			SetTCR(2, (DWORD)(data >> 4));
			SetTCR(3, (DWORD)(data & 0x0f));
			return;

		// Timer-A data
		case 0x0f:
			SetTDR(0, data);
			return;

		// Timer-B data
		case 0x10:
			SetTDR(1, data);
			return;

		// Timer-C data
		case 0x11:
			SetTDR(2, data);
			return;

		// Timer-D data
		case 0x12:
			SetTDR(3, data);
			return;

		// SYNC register
		case 0x13:
			mfp.scr = data;
			return;

		// USART control
		case 0x14:
			if (data != 0x88) {
				LOG1(Log::Warning, "USART parameter error %02X", data);
			}
			mfp.ucr = data;
			return;

		// Receiver status
		case 0x15:
			SetRSR(data);
			return;

		// Transmitter status
		case 0x16:
			SetTSR(data);
			return;

		// USART data
		case 0x17:
			Transmit(data);
			return;
	}

	LOG2(Log::Warning, "Undefined I/O register write R%02d <- $%02X",
							addr, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL MFP::WriteWord(DWORD addr, DWORD data)
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
DWORD FASTCALL MFP::ReadOnly(DWORD addr) const
{
	DWORD data;

 ASSERT(this);
 ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Only odd addresses are decoded
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// 64-byte unit loop
	addr &= 0x3f;
	addr >>= 1;

	switch (addr) {
		// GPIP
		case 0x00:
			return mfp.gpdr;

		// AER
		case 0x01:
			return mfp.aer;

		// DDR
		case 0x02:
			return mfp.ddr;

		// IER(A)
		case 0x03:
			return GetIER(0);

		// IER(B)
		case 0x04:
			return GetIER(1);

		// IPR(A)
		case 0x05:
			return GetIPR(0);

		// IPR(B)
		case 0x06:
			return GetIPR(1);

		// ISR(A)
		case 0x07:
			return GetISR(0);

		// ISR(B)
		case 0x08:
			return GetISR(1);

		// IMR(A)
		case 0x09:
			return GetIMR(0);

		// IMR(B)
		case 0x0a:
			return GetIMR(1);

		// VR
		case 0x0b:
			return GetVR();

		// Timer-A control
		case 0x0c:
			return mfp.tcr[0];

		// Timer-B control
		case 0x0d:
			return mfp.tcr[1];

		// Timer-C&D control
		case 0x0e:
			data = mfp.tcr[2];
			data <<= 4;
			data |= mfp.tcr[3];
			return data;

		// Timer-A data
		case 0x0f:
			return mfp.tir[0];

		// Timer-B data (returns pseudo value)
		case 0x10:
			return ((scheduler->GetTotalTime() % 13) + 1);

		// Timer-C data
		case 0x11:
			return mfp.tir[2];

		// Timer-D data
		case 0x12:
			return mfp.tir[3];

		// SYNC register
		case 0x13:
			return mfp.scr;

		// USART control
		case 0x14:
			return mfp.ucr;

		// Receiver status
		case 0x15:
			return mfp.rsr;

		// Transmitter status
		case 0x16:
			return mfp.tsr;

		// USART data
		case 0x17:
			return mfp.rur;
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get MFP
//
//---------------------------------------------------------------------------
void FASTCALL MFP::GetMFP(mfp_t *buffer) const
{
 ASSERT(this);
 ASSERT(buffer);

	// Copy data
	*buffer = mfp;
}

//---------------------------------------------------------------------------
//
//	Interrupt
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Interrupt(int level, BOOL enable)
{
	int index;

 ASSERT(this);
 ASSERT((level >= 0) && (level < 0x10));

	index = 15 - level;
	if (enable) {
		// If already requested
		if (mfp.ireq[index]) {
			return;
		}

		// Enable register check
		if (!mfp.ier[index]) {
			return;
		}

		// Flag Up, interrupt check
		mfp.ireq[index] = TRUE;
		IntCheck();
	}
	else {
		// If already accepted and service completed
		if (!mfp.ireq[index] && !mfp.ipr[index]) {
			return;
		}
		mfp.ireq[index] = FALSE;

		// Pending flag clear, interrupt check
		mfp.ipr[index] = FALSE;
		IntCheck();
	}
}

//---------------------------------------------------------------------------
//
//	Interrupt priority check
//
//---------------------------------------------------------------------------
void FASTCALL MFP::IntCheck()
{
	int i;
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);

	// Interrupt service check
	if (mfp.iidx >= 0) {
		// If pending flag not set or mask, cancel interrupt (data format 2.7.1)
		if (!mfp.ipr[mfp.iidx] || !mfp.imr[mfp.iidx]) {
			cpu->IntCancel(6);
#if defined(MFP_LOG)
			sprintf(buffer, "Interrupt cancel %s", IntDesc[mfp.iidx]);
			LOG0(Log::Normal, buffer);
#endif	// MFP_LOG
			mfp.iidx = -1;
		}
	}

	// Interrupt request
	for (i=0; i<0x10; i++) {
		// Interrupt enable
		if (mfp.ier[i]) {
			// If interrupt requested
			if (mfp.ireq[i]) {
				// Set pending flag to 1 (interrupt occur)
				mfp.ipr[i] = TRUE;
				mfp.ireq[i] = FALSE;
			}
		}
		else {
			// Enable register 0, clear pending flag
			mfp.ipr[i] = FALSE;
			mfp.ireq[i] = FALSE;
		}
	}

	// Interrupt vector output
	for (i=0; i<0x10; i++) {
		// Interrupt pending
		if (!mfp.ipr[i]) {
			continue;
		}

		// Interrupt mask
		if (!mfp.imr[i]) {
			continue;
		}

		// Service not
		if (mfp.isr[i]) {
			continue;
		}

		// If higher priority interrupt requested, cancel interrupt
		if (mfp.iidx > i) {
			cpu->IntCancel(6);
#if defined(MFP_LOG)
			sprintf(buffer, "Interrupt priority change %s", IntDesc[mfp.iidx]);
			LOG0(Log::Normal, buffer);
#endif	// MFP_LOG
			mfp.iidx = -1;
		}

		// Vector output
		if (cpu->Interrupt(6, (mfp.vr & 0xf0) + (15 - i))) {
			// CPU accepted. Index save
#if defined(MFP_LOG)
			sprintf(buffer, "Interrupt request %s", IntDesc[i]);
			LOG0(Log::Normal, buffer);
#endif	// MFP_LOG
			mfp.iidx = i;
			break;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Interrupt acknowledge
//
//---------------------------------------------------------------------------
void FASTCALL MFP::IntAck()
{
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);

	// Reset, if CPU acknowledges interrupt before device
	if (mfp.iidx < 0) {
		LOG0(Log::Warning, "Unexpected interrupt acknowledge");
		return;
	}

#if defined(MFP_LOG)
	sprintf(buffer, "Interrupt acknowledge %s", IntDesc[mfp.iidx]);
	LOG0(Log::Normal, buffer);
#endif	// MFP_LOG

	// Interrupt accepted. Pending flag clear
	mfp.ipr[mfp.iidx] = FALSE;

	// End service (AutoEOI=0, NonAutoEOI=1)
	if (mfp.vr & 0x08) {
		mfp.isr[mfp.iidx] = TRUE;
	}
	else {
		mfp.isr[mfp.iidx] = FALSE;
	}

	// Change index to none
	mfp.iidx = -1;

	// Check again
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	IER set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetIER(int offset, DWORD data)
{
	int i;
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));
 ASSERT(data < 0x100);

	// Offset set
	offset <<= 3;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		if (data & 0x80) {
			mfp.ier[i] = TRUE;
		}
		else {
			// Clear pending (data format 4.3.1)
			mfp.ier[i] = FALSE;
			mfp.ipr[i] = FALSE;
		}

		data <<= 1;
	}

	// Interrupt priority check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	IER get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetIER(int offset) const
{
	int i;
	DWORD bit;

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));

	// Offset set
	offset <<= 3;
	bit = 0;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		bit <<= 1;
		if (mfp.ier[i]) {
			bit |= 0x01;
		}
	}

	return bit;
}

//---------------------------------------------------------------------------
//
//	IPR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetIPR(int offset, DWORD data)
{
	int i;
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));

	// Offset set
	offset <<= 3;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		if (!(data & 0x80)) {
			// Cannot set ipr to 1 from CPU
			mfp.ipr[i] = FALSE;
		}

		data <<= 1;
	}

	// Interrupt priority check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	IPR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetIPR(int offset) const
{
	int i;
	DWORD bit;

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));

	// Offset set
	offset <<= 3;
	bit = 0;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		bit <<= 1;
		if (mfp.ipr[i]) {
			bit |= 0x01;
		}
	}

	return bit;
}

//---------------------------------------------------------------------------
//
//	ISR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetISR(int offset, DWORD data)
{
	int i;
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));
 ASSERT(data < 0x100);

	// Offset set
	offset <<= 3;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		if (!(data & 0x80)) {
			mfp.isr[i] = FALSE;
		}

		data <<= 1;
	}

	// Interrupt priority check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	ISR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetISR(int offset) const
{
	int i;
	DWORD bit;

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));

	// Offset set
	offset <<= 3;
	bit = 0;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		bit <<= 1;
		if (mfp.isr[i]) {
			bit |= 0x01;
		}
	}

	return bit;
}

//---------------------------------------------------------------------------
//
//	IMR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetIMR(int offset, DWORD data)
{
	int i;
#if defined(MFP_LOG)
	char buffer[0x40];
#endif	// MFP_LOG

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));
 ASSERT(data < 0x100);

	// Offset set
	offset <<= 3;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		if (data & 0x80) {
			mfp.imr[i] = TRUE;
		}
		else {
			mfp.imr[i] = FALSE;
		}

		data <<= 1;
	}

	// Interrupt priority check
	IntCheck();
}

//---------------------------------------------------------------------------
//
//	IMR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetIMR(int offset) const
{
	int i;
	DWORD bit;

 ASSERT(this);
 ASSERT((offset == 0) || (offset == 1));

	// Offset set
	offset <<= 3;
	bit = 0;

	// 8 bit shift
	for (i=offset; i<offset+8; i++) {
		bit <<= 1;
		if (mfp.imr[i]) {
			bit |= 0x01;
		}
	}

	return bit;
}

//---------------------------------------------------------------------------
//
//	VR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetVR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);

	if (mfp.vr != data) {
		mfp.vr = data;
		LOG1(Log::Detail, "Interrupt vector base $%02X", data & 0xf0);

		if (mfp.vr & 0x08) {
			LOG0(Log::Warning, "AutoEOI mode");
		}
	}
}

//---------------------------------------------------------------------------
//
//	VR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetVR() const
{
 ASSERT(this);
	return mfp.vr;
}

//---------------------------------------------------------------------------
//
//	Interrupt cause table
//
//---------------------------------------------------------------------------
const char* MFP::IntDesc[0x10] = {
	"H-SYNC",
	"CIRQ",
	"Timer-A",
	"RxFull",
	"RxError",
	"TxEmpty",
	"TxError",
	"Timer-B",
	"(NoUse)",
	"V-DISP",
	"Timer-C",
	"Timer-D",
	"FMIRQ",
	"POW SW",
	"EXPON",
	"ALARM"
};

//---------------------------------------------------------------------------
//
//	Event callback (used in broadcast)
//
//---------------------------------------------------------------------------
BOOL FASTCALL MFP::Callback(Event *ev)
{
	int channel;
	DWORD low;

 ASSERT(this);
 ASSERT(ev);

	// Get user data
	channel = (int)ev->GetUser();

	// Timer
	if ((channel >= 0) && (channel <= 3)) {
		low = (mfp.tcr[channel] & 0x0f);

		// Timer end
		if (low == 0) {
			// Timer off
			return FALSE;
		}

		// Broadcast mode
		if (low & 0x08) {
			// Count 0, increment level, interrupt request
			Interrupt(TimerInt[channel], TRUE);

			// Next event
			return FALSE;
		}

		// Timer proceed
		Proceed(channel);
		return TRUE;
	}

	// USART
 ASSERT(channel == 4);
	USART();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Timer event count (used in event count mode)
//
//---------------------------------------------------------------------------
void FASTCALL MFP::EventCount(int channel, int value)
{
	DWORD edge;
	BOOL flag;

 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 1));
 ASSERT((value == 0) || (value == 1));
 ASSERT((mfp.tbr[channel] == 0) || (mfp.tbr[channel] == 1));

	// Event count mode (Timer A,B only)
	if ((mfp.tcr[channel] & 0x0f) == 0x08) {
		// Get from GPIP4, GPIP3
		if (channel == 0) {
			edge = mfp.aer & 0x10;
		}
		else {
			edge = mfp.aer & 0x08;
		}

		// Flag off
		flag = FALSE;

		// Edge check
		if (edge == 1) {
			// Edge=1, timer proceed at 0->1
			if ((mfp.tbr[channel] == 0) && (value == 1)) {
				flag = TRUE;
			}
		}
		else {
			// Edge=0, timer proceed at 1->0
			if ((mfp.tbr[channel] == 1) && (value == 0)) {
				flag = TRUE;
			}
		}

		// Timer proceed
		if (flag) {
			Proceed(channel);
		}
	}

	// TBR update
	mfp.tbr[channel] = (DWORD)value;
}

//---------------------------------------------------------------------------
//
//	TCR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetTCR(int channel, DWORD data)
{
	DWORD prev;
	DWORD now;
	DWORD speed;

 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 3));
 ASSERT(data < 0x100);

	// Conversion, mode check
	now = data;
	now &= 0x0f;
	prev = mfp.tcr[channel];
	if (now == prev) {
		return;
	}
	mfp.tcr[channel] = now;

	// Timer-B only 0x01 special (not emulation)
	if (channel == 1) {
		if ((now != 0x01) && (now != 0x00)) {
			LOG1(Log::Warning, "Timer-B control $%02X", now);
		}
		now = 0;
	}

	// Timer stop
	if (now == 0) {
#if defined(MFP_LOG)
		LOG1(Log::Normal, "Timer%c stop", channel + 'A');
#endif	// MFP_LOG
		timer[channel].SetTime(0);

		// Cancel if interrupt pending (prevent freeze)
		Interrupt(TimerInt[channel], FALSE);

		// Timer-D, CPU clock speed (CH30.SYS)
		if (channel == 3) {
			if (mfp.sram != 0) {
				// CPU clock speed
				scheduler->SetCPUSpeed(mfp.sram);
				mfp.sram = 0;
			}
		}
		return;
	}

	// Pass through mode is not supported
	if (now > 0x08) {
		LOG2(Log::Warning, "Timer%c pass through mode$%02X", channel + 'A', now);
		return;
	}

	// Event count mode
	if (now == 0x08) {
#if defined(MFP_LOG)
		LOG1(Log::Normal, "Timer%c event count mode", channel + 'A');
#endif	// MFP_LOG
		// First event stop
		timer[channel].SetTime(0);

		// Timer OFF->ON, count load
		if (prev == 0) {
			mfp.tir[channel] = mfp.tdr[channel];
		}
		return;
	}

	// Broadcast mode, various timer set
#if defined(MFP_LOG)
	LOG3(Log::Normal, "Timer%c broadcast %d.%dus",
				channel + 'A', (TimerHus[now] / 2), (TimerHus[now] & 1) * 5);
#endif	// MFP_LOG

	// Timer OFF->ON, count load (_VDISPST purpose)
	if (prev == 0) {
		mfp.tir[channel] = mfp.tdr[channel];
	}

	// Event reset
	timer[channel].SetTime(TimerHus[now]);

	// Timer-C case, INFO.RAM speed change (CPU clock speed compensation)
	if (channel == 2) {
		if ((now == 3) && (mfp.sram == 0)) {
			speed = cpu->GetPC();
			if ((speed >= 0xed0100) && (speed <= 0xedffff)) {
				// CPU clock save
				speed = scheduler->GetCPUSpeed();
				mfp.sram = speed;
				speed *= 83;
				speed /= 96;
				scheduler->SetCPUSpeed(speed);
			}
		}
		if ((now != 3) && (mfp.sram != 0)) {
			// CPU clock speed
			scheduler->SetCPUSpeed(mfp.sram);
			mfp.sram = 0;
		}
	}

	// Timer-D case, CH30.SYS speed change (CPU clock speed compensation)
	if (channel == 3) {
		if ((now == 7) && (mfp.sram == 0)) {
			speed = cpu->GetPC();
			if ((speed >= 0xed0100) && (speed <= 0xedffff)) {
				// CPU clock save
				speed = scheduler->GetCPUSpeed();
				mfp.sram = speed;
				speed *= 85;
				speed /= 96;
				scheduler->SetCPUSpeed(speed);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	TCR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetTCR(int channel) const
{
 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 3));

	return mfp.tcr[channel];
}

//---------------------------------------------------------------------------
//
//	TDR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetTDR(int channel, DWORD data)
{
 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 3));
 ASSERT(data < 0x100);

	mfp.tdr[channel] = data;

	// Timer-B fixed value only
	if (channel == 1) {
		if (data != 0x0d) {
			LOG1(Log::Warning, "Timer-B load value %02X", data);
		}
	}
}

//---------------------------------------------------------------------------
//
//	TIR get
//
//---------------------------------------------------------------------------
DWORD FASTCALL MFP::GetTIR(int channel) const
{
 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 3));

	// Timer-B in XM6, output disabled (special 1us x 14 timer)
	if (channel == 1) {
		// (special)
		LOG0(Log::Warning, "Timer B data register read");
		return (DWORD)((scheduler->GetTotalTime() % 13) + 1);
	}

	return mfp.tir[channel];
}

//---------------------------------------------------------------------------
//
//	Timer proceed
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Proceed(int channel)
{
 ASSERT(this);
 ASSERT((channel >= 0) && (channel <= 3));

	// Counter decrement
	if (mfp.tir[channel] > 0) {
		mfp.tir[channel]--;
	}
	else {
		mfp.tir[channel] = 0xff;
	}

	// If 0, load and interrupt
	if (mfp.tir[channel] == 0) {
#if defined(MFP_LOG)
	LOG1(Log::Normal, "Timer%c overflow", channel + 'A');
#endif	// MFP_LOG

		// Load
		mfp.tir[channel] = mfp.tdr[channel];

		// Event count mode, not interrupt (SASI Super II)
		if (mfp.tcr[channel] == 0x08) {
			// GPIP change, make low level interrupt occur
			// Timer end wait (12 of timer, special)
			timer[channel].SetTime(36);
		}
		else {
			// Normal interrupt
			Interrupt(TimerInt[channel], TRUE);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Timer interrupt table
//
//---------------------------------------------------------------------------
const int MFP::TimerInt[4] = {
	13,									// Timer-A
	8,									// Timer-B
	5,									// Timer-C
	4									// Timer-D
};

//---------------------------------------------------------------------------
//
//	Timer time table
//
//---------------------------------------------------------------------------
const DWORD MFP::TimerHus[8] = {
	0,									// Timer stop
	2,									// 1.0us
	5,									// 2.5us
	8,									// 4us
	25,									// 12.5us
	32,									// 16us
	50,									// 25us
	100									// 50us
};

//---------------------------------------------------------------------------
//
//	GPDR set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetGPDR(DWORD data)
{
	int i;
	DWORD bit;

 ASSERT(this);
 ASSERT(data < 0x100);

	// Only bits with DDR=1 are valid
	for (i=0; i<8; i++) {
		bit = (DWORD)(1 << i);
		if (mfp.ddr & bit) {
			if (data & bit) {
				SetGPIP(i, 1);
			}
			else {
				SetGPIP(i, 0);
			}
		}
	}
}

//---------------------------------------------------------------------------
//
//	GPIP set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetGPIP(int num, int value)
{
	DWORD data;

 ASSERT(this);
 ASSERT((num >= 0) && (num < 8));
 ASSERT((value == 0) || (value == 1));

	// Back up
	data = mfp.gpdr;

	// Bit create
	mfp.gpdr &= (DWORD)(~(1 << num));
	if (value == 1) {
		mfp.gpdr |= (DWORD)(1 << num);
	}

	// If changed, interrupt check
	if (mfp.gpdr != data) {
		IntGPIP();
	}
}

//---------------------------------------------------------------------------
//
//	GPIP interrupt check
//
//---------------------------------------------------------------------------
void FASTCALL MFP::IntGPIP()
{
	DWORD data;
	int i;

 ASSERT(this);

	// Inside68k description follows original MFP data format
	// AER0 1->0 interrupt
	// AER1 0->1 interrupt

	// ~AER xor GPDR
	data = (DWORD)(~mfp.aer);
	data ^= (DWORD)mfp.gpdr;

	// From BER, when change from 1 to 0, interrupt occurs
	// (but not occur for timer from Timer-B Timer-A GPIP4,GPIP3)
	for (i=0; i<8; i++) {
		if (data & 0x80) {
			if (!(mfp.ber & 0x80)) {
				if (i == 3) {
					// GPIP4. Timer-A check
					if ((mfp.tcr[0] & 0x0f) > 0x08) {
						data <<= 1;
						mfp.ber <<= 1;
						continue;
					}
				}
				if (i == 4) {
					// GPIP3. Timer-B check
					if ((mfp.tcr[1] & 0x0f) > 0x08) {
						data <<= 1;
						mfp.ber <<= 1;
						continue;
					}
				}

				// Interrupt request (SORCERIAN X1->88)
				Interrupt(GPIPInt[i], TRUE);
			}
		}

		// Shift
		data <<= 1;
		mfp.ber <<= 1;
	}

	// BER create
	mfp.ber = (DWORD)(~mfp.aer);
	mfp.ber ^= mfp.gpdr;
}

//---------------------------------------------------------------------------
//
//	GPIP interrupt table
//
//---------------------------------------------------------------------------
const int MFP::GPIPInt[8] = {
	15,									// H-SYNC
	14,									// CIRQ
	7,									// (NoUse)
	6,									// V-DISP
	3,									// OPM
	2,									// POWER
	1,									// EXPON
	0									// ALARM
};

//---------------------------------------------------------------------------
//
//	Receiver status set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetRSR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);

	// RE only set
	data &= 0x01;

	mfp.rsr &= ~0x01;
	mfp.rsr |= (DWORD)(mfp.rsr | data);
}

//---------------------------------------------------------------------------
//
//	CPU->MFP Receive
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Receive()
{
 ASSERT(this);

	// Timer-B check
	if (mfp.tcr[1] != 0x01) {
		return;
	}
	if (mfp.tdr[1] != 0x0d) {
		return;
	}

	// USART control check
	if (mfp.ucr != 0x88) {
		return;
	}

	// Receiver buffer empty, do nothing
	if (!(mfp.rsr & 0x01)) {
		return;
	}

	// BF=1, already input data none
	if (!(mfp.rsr & 0x80)) {
		return;
	}

	// Data read, BF and OE set to 0
	mfp.rsr &= (DWORD)~0xc0;
}

//---------------------------------------------------------------------------
//
//	Transmitter status set
//
//---------------------------------------------------------------------------
void FASTCALL MFP::SetTSR(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);

	// BE,UE,END direct clear not
	mfp.tsr = (DWORD)(mfp.tsr & 0xd0);
	data &= (DWORD)~0xd0;
	mfp.tsr = (DWORD)(mfp.tsr | data);

	// TE=1, UE,END clear
	if (mfp.tsr & 0x01) {
		mfp.tsr = (DWORD)(mfp.tsr & ~0x50);
	}
}

//---------------------------------------------------------------------------
//
//	CPU->MFP Transmit
//
//---------------------------------------------------------------------------
void FASTCALL MFP::Transmit(DWORD data)
{
 ASSERT(this);

	// Timer-B check
	if (mfp.tcr[1] != 0x01) {
		return;
	}
	if (mfp.tdr[1] != 0x0d) {
		return;
	}

	// USART control check
	if (mfp.ucr != 0x88) {
		return;
	}

	// Transmitter buffer empty, do nothing
	if (!(mfp.tsr & 0x01)) {
		return;
	}

	// Status and data reset
	mfp.tsr = (DWORD)(mfp.tsr & ~0x80);
	mfp.tur = data;
#if defined(MFP_LOG)
	LOG1(Log::Normal, "USART transmit data output %02X", (BYTE)data);
#endif	// MFP_LOG
	return;
}

//---------------------------------------------------------------------------
//
//	USART
//
//---------------------------------------------------------------------------
void FASTCALL MFP::USART()
{
 ASSERT(this);
 ASSERT((mfp.readpoint >= 0) && (mfp.readpoint < 0x10));
 ASSERT((mfp.writepoint >= 0) && (mfp.writepoint < 0x10));
 ASSERT((mfp.datacount >= 0) && (mfp.datacount <= 0x10));

	// Timer and USART setting check
	if (mfp.tcr[1] != 0x01) {
		return;
	}
	if (mfp.tdr[1] != 0x0d) {
		return;
	}
	if (mfp.ucr != 0x88) {
		return;
	}

	//
	//	Transmit
	//

	if (!(mfp.tsr & 0x80)) {
		// If transmitter buffer not empty, END set
		if (!(mfp.tsr & 0x01)) {
			mfp.tsr = (DWORD)(mfp.tsr & ~0x80);
			mfp.tsr = (DWORD)(mfp.tsr | 0x10);
			LOG0(Log::Warning, "USART Transmit!Error");
			Interrupt(9, TRUE);
			return;
		}

		// Buffer empty, Interrupt accept
		mfp.tsr = (DWORD)(mfp.tsr | 0x80);
		if (mfp.tsr & 0x20) {
			mfp.tsr = (DWORD)(mfp.tsr & ~0x20);
			SetRSR((DWORD)(mfp.rsr | 0x01));
		}

		// Keyboard data output, transmit buffer interrupt
#if defined(MFP_LOG)
		LOG1(Log::Normal, "USART Transmit %02X", mfp.tur);
#endif	// MFP_LOG
		keyboard->Command(mfp.tur);
		Interrupt(10, TRUE);
	}
	else {
		if (!(mfp.tsr & 0x40)) {
			mfp.tsr = (DWORD)(mfp.tsr | 0x40);
			Interrupt(9, TRUE);
#if defined(MFP_LOG)
			LOG0(Log::Normal, "USART Transmitter error");
#endif	// MPF_LOG
		}
	}

	//
	//	Receive
	//

	// If no valid receive data, do nothing
	if (mfp.datacount == 0) {
		return;
	}

	// Receiver buffer empty, do nothing
	if (!(mfp.rsr & 0x01)) {
		return;
	}

	// Lock
	sync->Lock();

	// If receive data from overrun, overflow error
	if (mfp.rsr & 0x80) {
		mfp.rsr |= 0x40;
		mfp.readpoint = (mfp.readpoint + 1) & 0x0f;
		mfp.datacount--;
		sync->Unlock();

		LOG0(Log::Warning, "USART Overflow error");
		Interrupt(11, TRUE);
		return;
	}

	// Data read, BF reset, data save
	mfp.rsr |= 0x80;
	mfp.rur = mfp.buffer[mfp.readpoint];
	mfp.readpoint = (mfp.readpoint + 1) & 0x0f;
	mfp.datacount--;
	sync->Unlock();

	Interrupt(12, TRUE);
}

//---------------------------------------------------------------------------
//
//	Key data send
//
//---------------------------------------------------------------------------
void FASTCALL MFP::KeyData(DWORD data)
{
 ASSERT(this);
 ASSERT((mfp.readpoint >= 0) && (mfp.readpoint < 0x10));
 ASSERT((mfp.writepoint >= 0) && (mfp.writepoint < 0x10));
 ASSERT((mfp.datacount >= 0) && (mfp.datacount <= 0x10));
 ASSERT(data < 0x100);

	// Lock
	sync->Lock();

	// Write point set
	mfp.buffer[mfp.writepoint] = data;

	// Write point move, count +1
	mfp.writepoint = (mfp.writepoint + 1) & 0x0f;
	mfp.datacount++;
	if (mfp.datacount > 0x10) {
		mfp.datacount = 0x10;
	}

	// Sync unlock
	sync->Unlock();
}
