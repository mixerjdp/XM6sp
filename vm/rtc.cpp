//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ RTC(RP5C15) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "mfp.h"
#include "schedule.h"
#include "fileio.h"
#include "rtc.h"

//===========================================================================
//
//	RTC
//
//===========================================================================
//#define RTC_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
RTC::RTC(VM *p) : MemDevice(p)
{
	// Device ID creation
	dev.id = MAKEID('R', 'T', 'C', ' ');
	dev.desc = "RTC (RP5C15)";

	// Start address, End address
	memdev.first = 0xe8a000;
	memdev.last = 0xe8bfff;

	// Null clear
	mfp = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::Init()
{
	ASSERT(this);

	// Base class
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// Get MFP
	ASSERT(!mfp);
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
	ASSERT(mfp);

	// Create event(32Hz)
	event.SetDevice(this);
	event.SetDesc("Clock 16Hz");
	event.SetUser(0);
	event.SetTime(62500);
	scheduler->AddEvent(&event);

	// Clear RTC register
	rtc.sec = 0;
	rtc.min = 0;
	rtc.hour = 0;
	rtc.week = 0;
	rtc.day = 0;
	rtc.month = 0;
	rtc.year = 0;
	rtc.carry = FALSE;

	rtc.timer_en = FALSE;
	rtc.alarm_en = FALSE;
	rtc.bank = 0;
	rtc.test = 0;
	rtc.alarm_1hz = FALSE;
	rtc.alarm_16hz = FALSE;
	rtc.under_reset = FALSE;
	rtc.alarm_reset = FALSE;

	rtc.clkout = 7;
	rtc.adjust = FALSE;

	rtc.alarm_min = 0;
	rtc.alarm_hour = 0;
	rtc.alarm_week = 0;
	rtc.alarm_day = 0;

	rtc.fullhour = TRUE;
	rtc.leap = 0;

	rtc.signal_1hz = TRUE;
	rtc.signal_16hz = TRUE;
	rtc.signal_blink = 1;
	rtc.signal_count = 0;
	rtc.alarm = FALSE;
	rtc.alarmout = FALSE;

	// Set current time
	Adjust(TRUE);

	// MFPnotification(ALARM signal output)
	mfp->SetGPIP(0, 1);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL RTC::Cleanup()
{
	ASSERT(this);

	// Base class
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	ReSet
//
//---------------------------------------------------------------------------
void FASTCALL RTC::Reset()
{
	ASSERT(this);
	LOG0(Log::Normal, "Reset");
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Size save
	sz = sizeof(rtc_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Structure save
	if (!fio->Write(&rtc, (int)sz)) {
		return FALSE;
	}

	// Event save
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
BOOL FASTCALL RTC::Load(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Size and structure verification
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(rtc_t)) {
		return FALSE;
	}

	// Structure load
	if (!fio->Read(&rtc, (int)sz)) {
		return FALSE;
	}

	// Event load
	if (!event.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Config apply
//
//---------------------------------------------------------------------------
void FASTCALL RTC::ApplyCfg(const Config* /*config*/)
{
	ASSERT(this);

	LOG0(Log::Normal, "Config apply");
}

//---------------------------------------------------------------------------
//
//	Byte read
//
//---------------------------------------------------------------------------
DWORD FASTCALL RTC::ReadByte(DWORD addr)
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Odd address is invalid, returns 0xFF
	if ((addr & 1) == 0) {
		// ROM IOCS bank read/write not supported. X680x0 is not supported
		return 0xff;
	}

	// 32-byte page mode
	addr &= 0x1f;

	// Wait
	scheduler->Wait(1);

	// Bank mapping (register 0 to 28)
	addr >>= 1;
	ASSERT((rtc.bank == 0) || (rtc.bank == 1));
	if (rtc.bank > 0) {
		// Bank1 has 13 valid bytes from beginning
		if (addr <= 0x0c) {
			addr += 0x10;
		}
	}

	switch (addr) {
		// Sec(1)
		case 0x00:
			data = rtc.sec % 10;
			return (0xf0 | data);

		// Sec(10)
		case 0x01:
			data = rtc.sec / 10;
			return (0xf0 | data);

		// Min(1)
		case 0x02:
			data = rtc.min % 10;
			return (0xf0 | data);

		// Min(10)
		case 0x03:
			data = rtc.min / 10;
			return (0xf0 | data);

		// Hour(1)
		case 0x04:
			// 12h,24h handled
			data = rtc.hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data %= 10;
			return (0xf0 | data);

		// Hour(10)
		case 0x05:
			// 12h,24h handled. 12h sets PM at bit1
			data = rtc.hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data /= 10;
			if (!rtc.fullhour && (rtc.hour >= 12)) {
				data |= 0x02;
			}
			return (0xf0 | data);

		// Week
		case 0x06:
			return (0xf0 | rtc.week);

		// Day(1)
		case 0x07:
			data = rtc.day % 10;
			return (0xf0 | data);

		// Day(10)
		case 0x08:
			data = rtc.day / 10;
			return (0xf0 | data);

		// Month(1)
		case 0x09:
			data = rtc.month % 10;
			return (0xf0 | data);

		// Month(10)
		case 0x0a:
			data = rtc.month / 10;
			return (0xf0 | data);

		// Year(1)
		case 0x0b:
			data = rtc.year % 10;
			return (0xf0 | data);

		// Year(10)
		case 0x0c:
			data = rtc.year / 10;
			return (0xf0 | data);

		// MODE register
		case 0x0d:
			data = 0xf0;
			if (rtc.timer_en) {
				data |= 0x08;
			}
			if (rtc.alarm_en) {
				data |= 0x04;
			}
			if (rtc.bank > 0) {
				data |= 0x01;
			}
			return data;

		// TEST register(Write Only)
		case 0x0e:
			return 0xf0;

		// RESET register(Write only is not HSi.x v2);
		case 0x0f:
			data = 0xf0;
			if (!rtc.alarm_1hz) {
				data |= 0x08;
			}
			if (!rtc.alarm_16hz) {
				data |= 0x04;
			}
			return data;

		// CLKOUT register
		case 0x10:
			ASSERT(rtc.clkout < 0x08);
			return (0xf0 | rtc.clkout);

		// ADJUST register
		case 0x11:
			return (0xf0 | rtc.adjust);

		// Alarm Min(1)
		case 0x12:
			data = rtc.alarm_min % 10;
			return (0xf0 | data);

		// Alarm Min(10)
		case 0x13:
			data = rtc.alarm_min / 10;
			return (0xf0 | data);

		// Alarm Hour(1)
		case 0x14:
			// 12h,24h handled
			data = rtc.alarm_hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data %= 10;
			return (0xf0 | data);

		// Alarm Hour(10)
		case 0x15:
			// 12h,24h handled. 12h sets PM at bit1
			data = rtc.alarm_hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data /= 10;
			if (!rtc.fullhour && (rtc.alarm_hour >= 12)) {
				data |= 0x02;
			}
			return (0xf0 | data);

		// Alarm Week
		case 0x16:
			return (0xf0 | rtc.alarm_week);

		// Alarm Day(1)
		case 0x17:
			data = rtc.alarm_day % 10;
			return (0xf0 | data);

		// Alarm Day(10)
		case 0x18:
			data = rtc.alarm_day / 10;
			return (0xf0 | data);

		// 12h,24h switch
		case 0x1a:
			if (rtc.fullhour) {
				return 0xf1;
			}
			return 0xf0;

		// Leap counter
		case 0x1b:
			ASSERT(rtc.leap <= 3);
			return (0xf0 | rtc.leap);
	}

	LOG1(Log::Warning, "Invalid register read R%02d", addr);
	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Byte write
//
//---------------------------------------------------------------------------
DWORD FASTCALL RTC::ReadWord(DWORD addr)
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
void FASTCALL RTC::WriteByte(DWORD addr, DWORD data)
{
	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);

	// Odd address is invalid, returns 0xFF
	if ((addr & 1) == 0) {
		// X680x0 is not supported
		return;
	}

	// 32-byte page mode
	addr &= 0x1f;

	// Wait
	scheduler->Wait(1);

	// Bank mapping (register 0 to 28)
	addr >>= 1;
	ASSERT((rtc.bank == 0) || (rtc.bank == 1));
	if (rtc.bank > 0) {
		// Bank1 has 13 valid bytes from beginning
		if (addr <= 0x0c) {
			addr += 0x10;
		}
	}

	switch (addr) {
		// Sec(1)
		case 0x00:
			data &= 0x0f;
			rtc.sec /= 10;
			rtc.sec *= 10;
			rtc.sec += data;
			return;

		// Sec(10)
		case 0x01:
			data &= 0x07;
			data *= 10;
			rtc.sec %= 10;
			rtc.sec += data;
			return;

		// Min(1)
		case 0x02:
			data &= 0x0f;
			rtc.min /= 10;
			rtc.min *= 10;
			rtc.min += data;
			return;

		// Min(10)
		case 0x03:
			data &= 0x07;
			data *= 10;
			rtc.min %= 10;
			rtc.min += data;
			return;

		// Hour(1)
		case 0x04:
			data &= 0x0f;
			// 12h,24h handled
			if (rtc.fullhour || (rtc.hour < 12)) {
				// 24h or 12h(AM)
				rtc.hour /= 10;
				rtc.hour *= 10;
				rtc.hour += data;
			}
			else {
				// 12h(PM)
				rtc.hour -= 12;
				rtc.hour /= 10;
				rtc.hour *= 10;
				rtc.hour += data;
				rtc.hour += 12;
			}
			return;

		// Hour(10)
		case 0x05:
			data &= 0x03;
			// 12h,24h handled
			if (rtc.fullhour) {
				// 24h
				data *= 10;
				rtc.hour %= 10;
				rtc.hour += data;
			}
			else {
				if (data & 0x02) {
					// 12h,PM end invalid (data conversion error)
					data &= 0x01;
					data *= 10;
					rtc.hour %= 10;
					rtc.hour += data;
					rtc.hour += 12;
				}
				else {
					// 12h,AM end invalid (data conversion error)
					data &= 0x01;
					data *= 10;
					rtc.hour %= 10;
					rtc.hour += data;
				}
			}
			return;

		// Week
		case 0x06:
			data &= 0x07;
			rtc.week = data;
			return;

		// Day(1)
		case 0x07:
			data &= 0x0f;
			rtc.day /= 10;
			rtc.day *= 10;
			rtc.day += data;
			return;

		// Day(10)
		case 0x08:
			data &= 0x03;
			data *= 10;
			rtc.day %= 10;
			rtc.day += data;
			return;

		// Month(1)
		case 0x09:
			data &= 0x0f;
			rtc.month /= 10;
			rtc.month *= 10;
			rtc.month += data;
			return;

		// Month(10)
		case 0x0a:
			data &= 0x01;
			data *= 10;
			rtc.month %= 10;
			rtc.month += data;
			return;

		// Year(1)
		case 0x0b:
			data &= 0x0f;
			rtc.year /= 10;
			rtc.year *= 10;
			rtc.year += data;
			return;

		// Year(10)
		case 0x0c:
			data &= 0x0f;
			data *= 10;
			rtc.year %= 10;
			rtc.year += data;
			return;

		// MODE register
		case 0x0d:
			// Timer clear
			if (data & 0x08) {
				rtc.timer_en = TRUE;
				// No event is registered, nothing is done
				if (rtc.carry) {
					SecUp();
				}
			}
			else {
				rtc.timer_en = FALSE;
			}

			// Alarm clear
			if (data & 0x04) {
				rtc.alarm_en = TRUE;
			}
			else {
				rtc.alarm_en = FALSE;
			}
			AlarmOut();

			// Bank select
			if (data & 0x01) {
				rtc.bank = 1;
			}
			else {
				rtc.bank = 0;
			}
			return;

		// TEST register
		case 0x0e:
			rtc.test = (data & 0x0f);
			if (rtc.test != 0) {
				LOG1(Log::Warning, "test mode set $%02X", rtc.test);
			}
			return;

		// RESET register
		case 0x0f:
			// 1Hz, 16Hz signal output
			if (data & 0x08) {
				rtc.alarm_1hz = FALSE;
			}
			else {
				rtc.alarm_1hz = TRUE;
			}
			if (data & 0x04) {
				rtc.alarm_16hz = FALSE;
			}
			else {
				rtc.alarm_16hz = TRUE;
			}
			AlarmOut();

			if (data & 0x02) {
				rtc.under_reset = TRUE;
				// Event is reset
				event.SetTime(event.GetTime());
			}
			else {
				rtc.under_reset = FALSE;
			}
			if (data & 0x01) {
				// Check if alarm signal matches current time
				rtc.alarm_reset = TRUE;
				rtc.alarm = FALSE;
				AlarmOut();
				// Register event to scheduler
				rtc.alarm_min = rtc.min;
				rtc.alarm_hour = rtc.hour;
				rtc.alarm_week = rtc.week;
				rtc.alarm_day = rtc.day;
			}
			else {
				rtc.alarm_reset = FALSE;
			}
			return;

		// CLKOUT register
		case 0x10:
			rtc.clkout = (data & 0x07);
#if defined(RTC_LOG)
			LOG1(Log::Normal, "CLKOUT reset %d", rtc.clkout);
#endif	// RTC_LOG
			return;

		// ADJ register
		case 0x11:
			rtc.adjust = (data & 0x01);
			if (data & 0x01) {
				// 1-second counter
				if (rtc.sec < 30) {
					// Round up
					rtc.sec = 0;
				}
				else {
					// Round up
					MinUp();
				}
			}
			return;

		// Alarm Min(1)
		case 0x12:
			data &= 0x0f;
			rtc.alarm_min /= 10;
			rtc.alarm_min *= 10;
			rtc.alarm_min += data;
			return;

		// Alarm Min(10)
		case 0x13:
			data &= 0x07;
			data *= 10;
			rtc.alarm_min %= 10;
			rtc.alarm_min += data;
			return;

		// Alarm Hour(1)
		case 0x14:
			data &= 0x0f;
			// 12h,24h handled
			if (rtc.fullhour || (rtc.alarm_hour < 12)) {
				// 24h or 12h(AM)
				rtc.alarm_hour /= 10;
				rtc.alarm_hour *= 10;
				rtc.alarm_hour += data;
			}
			else {
				// 12h(PM)
				rtc.alarm_hour -= 12;
				rtc.alarm_hour /= 10;
				rtc.alarm_hour *= 10;
				rtc.alarm_hour += data;
				rtc.alarm_hour += 12;
			}
			return;

		// Alarm Hour(10)
		case 0x15:
			data &= 0x03;
			// 12h,24h handled
			if (rtc.fullhour) {
				// 24h
				data *= 10;
				rtc.alarm_hour %= 10;
				rtc.alarm_hour += data;
			}
			else {
				if (data & 0x02) {
					// 12h,PM end invalid (data conversion error)
					data &= 0x01;
					data *= 10;
					rtc.alarm_hour %= 10;
					rtc.alarm_hour += data;
					rtc.alarm_hour += 12;
				}
				else {
					// 12h,AM end invalid (data conversion error)
					data &= 0x01;
					data *= 10;
					rtc.alarm_hour %= 10;
					rtc.alarm_hour += data;
				}
			}
			return;

		// Alarm Week
		case 0x16:
			data &= 0x07;
			rtc.alarm_week = data;
			return;

		// Alarm Day(1)
		case 0x17:
			data &= 0x0f;
			rtc.alarm_day /= 10;
			rtc.alarm_day *= 10;
			rtc.alarm_day += data;
			return;

		// Alarm Day(10)
		case 0x18:
			data &= 0x03;
			data *= 10;
			rtc.alarm_day %= 10;
			rtc.alarm_day += data;
			return;

		// 12h,24h switch
		case 0x1a:
			if (data & 0x01) {
				rtc.fullhour = TRUE;
#if defined(RTC_LOG)
				LOG0(Log::Normal, "24-hour mode");
#endif	// RTC_LOG
			}
			else {
				rtc.fullhour = FALSE;
#if defined(RTC_LOG)
				LOG0(Log::Normal, "12-hour mode");
#endif	// RTC_LOG
			}
			return;

		// Leap counter
		case 0x1b:
			rtc.leap = (data & 0x03);
			return;
	}

	LOG2(Log::Warning, "Invalid register write R%02d <- $%02X",
							addr, data);
}

//---------------------------------------------------------------------------
//
//	Word write
//
//---------------------------------------------------------------------------
void FASTCALL RTC::WriteWord(DWORD addr, DWORD data)
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
DWORD FASTCALL RTC::ReadOnly(DWORD addr) const
{
	DWORD data;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));

	// Odd address is invalid, returns 0xFF
	if ((addr & 1) == 0) {
		return 0xff;
	}

	// 32-byte page mode
	addr &= 0x1f;

	// Bank mapping (register 0 to 28)
	addr >>= 1;
	ASSERT((rtc.bank == 0) || (rtc.bank == 1));
	if (rtc.bank > 0) {
		// Bank1 has 13 valid bytes from beginning
		if (addr <= 0x0c) {
			addr += 0x10;
		}
	}

	switch (addr) {
		// Sec(1)
		case 0x00:
			data = rtc.sec % 10;
			return (0xf0 | data);

		// Sec(10)
		case 0x01:
			data = rtc.sec / 10;
			return (0xf0 | data);

		// Min(1)
		case 0x02:
			data = rtc.min % 10;
			return (0xf0 | data);

		// Min(10)
		case 0x03:
			data = rtc.min / 10;
			return (0xf0 | data);

		// Hour(1)
		case 0x04:
			// 12h,24h handled
			data = rtc.hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data %= 10;
			return (0xf0 | data);

		// Hour(10)
		case 0x05:
			// 12h,24h handled. 12h sets PM at bit1
			data = rtc.hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data /= 10;
			if (!rtc.fullhour && (rtc.hour >= 12)) {
				data |= 0x02;
			}
			return (0xf0 | data);

		// Week
		case 0x06:
			return (0xf0 | rtc.week);

		// Day(1)
		case 0x07:
			data = rtc.day % 10;
			return (0xf0 | data);

		// Day(10)
		case 0x08:
			data = rtc.day / 10;
			return (0xf0 | data);

		// Month(1)
		case 0x09:
			data = rtc.month % 10;
			return (0xf0 | data);

		// Month(10)
		case 0x0a:
			data = rtc.month / 10;
			return (0xf0 | data);

		// Year(1)
		case 0x0b:
			data = rtc.year % 10;
			return (0xf0 | data);

		// Year(10)
		case 0x0c:
			data = rtc.year / 10;
			return (0xf0 | data);

		// MODE register
		case 0x0d:
			data = 0xf0;
			if (rtc.timer_en) {
				data |= 0x08;
			}
			if (rtc.alarm_en) {
				data |= 0x04;
			}
			if (rtc.bank > 0) {
				data |= 0x01;
			}
			return data;

		// TEST register(Write Only)
		case 0x0e:
			return 0xf0;

		// RESET register(Write only is not HSi.x v2);
		case 0x0f:
			data = 0xf0;
			if (!rtc.alarm_1hz) {
				data |= 0x08;
			}
			if (!rtc.alarm_16hz) {
				data |= 0x04;
			}
			return data;

		// CLKOUT register
		case 0x10:
			ASSERT(rtc.clkout < 0x08);
			return (0xf0 | rtc.clkout);

		// ADJUST register
		case 0x11:
			return (0xf0 | rtc.adjust);

		// Alarm Min(1)
		case 0x12:
			data = rtc.alarm_min % 10;
			return (0xf0 | data);

		// Alarm Min(10)
		case 0x13:
			data = rtc.alarm_min / 10;
			return (0xf0 | data);

		// Alarm Hour(1)
		case 0x14:
			// 12h,24h handled
			data = rtc.alarm_hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data %= 10;
			return (0xf0 | data);

		// Alarm Hour(10)
		case 0x15:
			// 12h,24h handled. 12h sets PM at bit1
			data = rtc.alarm_hour;
			if (!rtc.fullhour) {
				data %= 12;
			}
			data /= 10;
			if (!rtc.fullhour && (rtc.alarm_hour >= 12)) {
				data |= 0x02;
			}
			return (0xf0 | data);

		// Alarm Week
		case 0x16:
			return (0xf0 | rtc.alarm_week);

		// Alarm Day(1)
		case 0x17:
			data = rtc.alarm_day % 10;
			return (0xf0 | data);

		// Alarm Day(10)
		case 0x18:
			data = rtc.alarm_day / 10;
			return (0xf0 | data);

		// 12h,24h switch
		case 0x1a:
			if (rtc.fullhour) {
				return 0xf1;
			}
			return 0xf0;

		// Leap counter
		case 0x1b:
			ASSERT(rtc.leap <= 3);
			return (0xf0 | rtc.leap);
	}

	return 0xff;
}

//---------------------------------------------------------------------------
//
//	Get RTC data
//
//---------------------------------------------------------------------------
void FASTCALL RTC::GetRTC(rtc_t *buffer)
{
	ASSERT(this);
	ASSERT(buffer);

	// ReRTC data is copied
	*buffer = rtc;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::Callback(Event* /*ev*/)
{
	ASSERT(this);

	// Event callback
	rtc.signal_blink--;
	if (rtc.signal_blink == 0) {
		rtc.signal_blink = 25;
	}

	// Time adjustment
	rtc.signal_16hz = !(rtc.signal_16hz);

	// Re16-bit units
	rtc.signal_count++;
	if (rtc.signal_count < 0x10) {
		// Output alarm register data
		AlarmOut();
		return TRUE;
	}
	rtc.signal_count = 0;

	// 1HzTime adjustment
	rtc.signal_1hz = !(rtc.signal_1hz);

	// Output alarm register data
	AlarmOut();

	// TRUE or FALSE(validation result) in callback(Si.x v2)
	if (rtc.signal_1hz) {
		return TRUE;
	}

	// If timer is disabled, set carry and exit
	if (!rtc.timer_en) {
		rtc.carry = TRUE;
		return TRUE;
	}

	// Sec up
	SecUp();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Current time is set
//
//---------------------------------------------------------------------------
void FASTCALL RTC::Adjust(BOOL alarm)
{
	time_t ltime;
	struct tm *now;
	int leap;

    ASSERT(this);

	// Get current time
	ltime = time(NULL);
	now = localtime(&ltime);

	// Conversion
	rtc.year = (now->tm_year + 20) % 100;
	rtc.month = now->tm_mon + 1;
	rtc.day = now->tm_mday;
	rtc.week = now->tm_wday;
	rtc.hour = now->tm_hour;
	rtc.min = now->tm_min;
	rtc.sec = now->tm_sec;

	// leap year calculation (not leap year in year 2100)
	leap = now->tm_year;
	leap %= 4;
	rtc.leap = leap;

	// Not supported by the 040Murder extended function
	if (alarm) {
		rtc.alarm_min = rtc.min;
		rtc.alarm_hour = rtc.hour;
		rtc.alarm_week = rtc.week;
		rtc.alarm_day = rtc.day;
	}
}

//---------------------------------------------------------------------------
//
//	Alarm output
//
//---------------------------------------------------------------------------
void FASTCALL RTC::AlarmOut()
{
	BOOL flag;

	flag = FALSE;

	// Alarm output
	if (rtc.alarm_en) {
		flag = rtc.alarm;
	}

	// 1Hz output
	if (rtc.alarm_1hz) {
		flag = rtc.signal_1hz;
	}

	// 16Hz output
	if (rtc.alarm_16hz) {
		flag = rtc.signal_16hz;
	}

	// Notify MFP
	rtc.alarmout = flag;

	// MFP notification
	if (flag) {
		mfp->SetGPIP(0, 0);
	}
	else {
		mfp->SetGPIP(0, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Get alarm output
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::GetAlarmOut() const
{
	ASSERT(this);

	return rtc.alarmout;
}

//---------------------------------------------------------------------------
//
//	Get FDD blink signal
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::GetBlink(int drive) const
{
	ASSERT(this);
	ASSERT((drive == 0) || (drive == 1));

	// Bank0, Bank1 or other included
	if (drive == 0) {
		if (rtc.signal_blink >  13) {
			return FALSE;
		}
		return TRUE;
	}

	// Cannot be 0-25, so divide by 4
	if ((rtc.signal_blink >  6) && (rtc.signal_blink < 19)) {
		return FALSE;
	}
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get timer LED
//
//---------------------------------------------------------------------------
BOOL FASTCALL RTC::GetTimerLED() const
{
	BOOL led;

	ASSERT(this);
	ASSERT(rtc.clkout <= 7);

	// 0(L level) and 3(128Hz) are treated as invalid
	led = TRUE;
	if (rtc.clkout <= 3) {
		return led;
	}

	switch (rtc.clkout) {
		// 16Hz
		case 4:
			led = rtc.signal_16hz;
			break;

		// 1Hz
		case 5:
			led = rtc.signal_1hz;
			break;

		// 1/60Hz(sec is 0-29, 30-59)
		case 6:
			if (rtc.sec < 30) {
				led = TRUE;
			}
			else {
				led = FALSE;
			}
			break;

		// L level
		case 7:
			led = FALSE;
			break;
	}

	return led;
}

//---------------------------------------------------------------------------
//
//	Sec up
//
//---------------------------------------------------------------------------
void FASTCALL RTC::SecUp()
{
	ASSERT(this);

	// Delete existing timer event
	rtc.carry = FALSE;

	// End at
	rtc.sec++;

	// Ends at 60
	if (rtc.sec < 60) {
		return;
	}

	// For alarm notification first
	MinUp();
}

//---------------------------------------------------------------------------
//
//	Min up
//
//---------------------------------------------------------------------------
void FASTCALL RTC::MinUp()
{
	ASSERT(this);

	// Sec up
	rtc.sec = 0;

	// End at
	rtc.min++;

	// If not 60, check alarm and exit
	if (rtc.min < 60) {
		AlarmCheck();
		return;
	}

	// Hour alarm
	rtc.min = 0;
	rtc.hour++;

	// 24 alarm check and exit
	if (rtc.hour < 24) {
		AlarmCheck();
		return;
	}

	// Day alarm
	rtc.hour = 0;
	rtc.day++;
	rtc.week++;
	if (rtc.week >= 7) {
		rtc.week = 0;
	}

	// Day table check
	if (rtc.day <= DayTable[rtc.month]) {
		// Subtract remaining days. Check alarm and exit
		AlarmCheck();
		return;
	}

	// 2 is leap year check
	if ((rtc.month == 2) && (rtc.day == 29)) {
		// Leap year is 0, so February is not processed
		if (rtc.leap == 0) {
			AlarmCheck();
			return;
		}
	}

	// Month up
	rtc.day = 1;
	rtc.month++;

	// 13 alarm check and exit
	if (rtc.hour < 13) {
		AlarmCheck();
		return;
	}

	// New year
	rtc.month = 1;
	rtc.year++;
	AlarmCheck();
}

//---------------------------------------------------------------------------
//
//	Alarm check
//
//---------------------------------------------------------------------------
void FASTCALL RTC::AlarmCheck()
{
	BOOL flag;

	flag = TRUE;

	// Sun,Mon,Tue,Wed,Thu,Fri,Sat and force update alarm
	if (rtc.alarm_min != rtc.min) {
		flag = FALSE;
	}
	if (rtc.alarm_hour != rtc.hour) {
		flag = FALSE;
	}
	if (rtc.alarm_week != rtc.week) {
		flag = FALSE;
	}
	if (rtc.alarm_day != rtc.day) {
		flag = FALSE;
	}
	rtc.alarm = flag;

	AlarmOut();
}

//---------------------------------------------------------------------------
//
//	Day table
//
//---------------------------------------------------------------------------
const DWORD RTC::DayTable[] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
