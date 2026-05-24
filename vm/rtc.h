//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ RTC(RP5C15) ]
//
//---------------------------------------------------------------------------

#if !defined(rtc_h)
#define rtc_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	RTC
//
//===========================================================================
class RTC : public MemDevice
{
public:
	typedef struct {
		DWORD sec;						// second
		DWORD min;						// minute
		DWORD hour;						// hour
		DWORD week;						// day of week
		DWORD day;						// day
		DWORD month;					// month
		DWORD year;						// year
		BOOL carry;						// second carry

		BOOL timer_en;					// timer enable
		BOOL alarm_en;					// alarm enable
		DWORD bank;						// bank number
		DWORD test;						// TEST register
		BOOL alarm_1hz;					// 1Hz pulse output enable
		BOOL alarm_16hz;				// 16Hz pulse output enable
		BOOL under_reset;				// timer reset
		BOOL alarm_reset;				// alarm reset

		DWORD clkout;					// CLKOUT register
		BOOL adjust;					// adjust

		DWORD alarm_min;				// minute
		DWORD alarm_hour;				// hour
		DWORD alarm_week;				// day of week
		DWORD alarm_day;				// day

		BOOL fullhour;					// 24h flag
		DWORD leap;						// leap year

		BOOL signal_1hz;				// 1Hz signal (toggle every 500ms)
		BOOL signal_16hz;				// 16Hz signal (toggle every 31.25ms)
		DWORD signal_count;				// 16Hz counter (0~15)
		DWORD signal_blink;				// blink signal (toggle every 781.25ms)
		BOOL alarm;						// alarm signal
		BOOL alarmout;					// ALARM OUT
	} rtc_t;

public:
	// Basic functions
	RTC(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Init
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply config

	// Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only

	// External API
	void FASTCALL GetRTC(rtc_t *buffer);
										// Get RTC data
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	BOOL FASTCALL GetTimerLED() const;
										// Get timer LED
	BOOL FASTCALL GetAlarmOut() const;
										// Get ALARM signal
	BOOL FASTCALL GetBlink(int drive) const;
										// Get FDD blink signal
	void FASTCALL Adjust(BOOL alarm);
										// Current time set

private:
	void FASTCALL AlarmOut();
										// Alarm signal output
	void FASTCALL SecUp();
										// Second up
	void FASTCALL MinUp();
										// Minute up
	void FASTCALL AlarmCheck();
										// Alarm check
	MFP *mfp;
										// MFP
	rtc_t rtc;
										// RTC data
	Event event;
										// Event
	static const DWORD DayTable[];
										// Day table
};

#endif	// rtc_h
