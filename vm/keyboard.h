//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI (ytanaka@ipc-tokai.or.jp)
//	[ Keyboard ]
//
//---------------------------------------------------------------------------

#if !defined(keyboard_h)
#define keyboard_h

#include "device.h"
#include "event.h"

class MFP;
class Mouse;
class Sync;
//===========================================================================
//
//	Keyboard
//
//===========================================================================
class Keyboard : public Device
{
public:
	// Internal data structure
	typedef struct {
		BOOL connect;					// Connect flag
		BOOL status[0x80];				// Key status
		DWORD rep_code;					// Repeat code
		DWORD rep_count;				// Repeat counter
		DWORD rep_start;				// Repeat start(hus unit)
		DWORD rep_next;					// Repeat interval(hus unit)
		BOOL send_en;					// Key data send enable
		BOOL send_wait;				// Key data send wait
		DWORD msctrl;					// Mouse send enable
		BOOL tv_mode;					// X68000 display mode
		BOOL tv_ctrl;					// Display controller on/off by command
		BOOL opt2_ctrl;					// Display controller on/off by OPT2
		DWORD bright;					// Key LED brightness
		DWORD led;						// Key LED(1=off)
		DWORD cmdbuf[0x10];				// Command buffer
		DWORD cmdread;					// Command read pointer
		DWORD cmdwrite;					// Command write pointer
		DWORD cmdnum;					// Command number
	} keyboard_t;

public:
	// Basic constructor
	Keyboard(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
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

	// External API
	void FASTCALL Connect(BOOL connect);
										// Connect
	BOOL FASTCALL IsConnect() const		{ return keyboard.connect; }
										// Connect check
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL MakeKey(DWORD code);
										// Make
	void FASTCALL BreakKey(DWORD code);
										// Break
	void FASTCALL Command(DWORD data);
										// Command
	BOOL FASTCALL GetCommand(DWORD& data);
										// Command get
	void FASTCALL ClrCommand();
										// Command clear
	void FASTCALL SendWait(BOOL flag);
										// Key data send wait
	BOOL FASTCALL IsSendWait() const	{ return keyboard.send_wait; }
										// Key data send wait get
	void FASTCALL GetKeyboard(keyboard_t *buffer) const;
										// Get keyboard
	keyboard_t keyboard;
										// Internal data

private:
	MFP *mfp;
										// MFP
	Mouse *mouse;
										// Mouse
	
	Event event;
										// Event
	Sync *sync;
										// Command sync
};

#endif	// keyboard_h
