//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Keyboard ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "mfp.h"
#include "mouse.h"
#include "sync.h"
#include "fileio.h"
#include "config.h"
#include "keyboard.h"

//===========================================================================
//
//	Keyboard
//
//===========================================================================
//#define KEYBOARD_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Keyboard::Keyboard(VM *p) : Device(p)
{
	// Device ID creation
	dev.id = MAKEID('K', 'E', 'Y', 'B');
	dev.desc = "Keyboard";

	// Objects
	sync = NULL;
	mfp = NULL;
	mouse = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL Keyboard::Init()
{
	int i;
	Scheduler *scheduler;

 ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// Sync creation
	sync = new Sync;

	// MFP get
	mfp = (MFP*)vm->SearchDevice(MAKEID('M', 'F', 'P', ' '));
 ASSERT(mfp);

	// Scheduler get
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(scheduler);

	// Mouse get
	mouse = (Mouse*)vm->SearchDevice(MAKEID('M', 'O', 'U', 'S'));
 ASSERT(mouse);

	// Add event
	event.SetDevice(this);
	event.SetDesc("Key Repeat");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	// Internal state clear (No reset message sent. Only power-on reset)
	keyboard.connect = TRUE;
	for (i=0; i<0x80; i++) {
		keyboard.status[i] = FALSE;
	}
	keyboard.rep_code = 0;
	keyboard.rep_count = 0;
	keyboard.rep_start = 500 * 1000 * 2;
	keyboard.rep_next = 110 * 1000 * 2;
	keyboard.send_en = TRUE;
	keyboard.send_wait = FALSE;
	keyboard.msctrl = 0;
	keyboard.tv_mode = TRUE;
	keyboard.tv_ctrl = TRUE;
	keyboard.opt2_ctrl = TRUE;
	keyboard.bright = 0;
	keyboard.led = 0;

	// TrueKey command list
	sync->Lock();
	keyboard.cmdnum = 0;
	keyboard.cmdread = 0;
	keyboard.cmdwrite = 0;
	sync->Unlock();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::Cleanup()
{
 ASSERT(this);

	// Sync delete
	if (sync) {
		delete sync;
		sync = NULL;
	}

	// Base class cleanup
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::Reset()
{
 ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Since system port resets, send_wait is cleared
	keyboard.send_wait = FALSE;

	// Command all clear
	ClrCommand();

	// Send 0xff
	if (keyboard.send_en && !keyboard.send_wait && keyboard.connect) {
		mfp->KeyData(0xff);
	}
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Keyboard::Save(Fileio *fio, int ver)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(keyboard_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save main
	if (!fio->Write(&keyboard, (int)sz)) {
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
BOOL FASTCALL Keyboard::Load(Fileio *fio, int ver)
{
	size_t sz;

 ASSERT(this);
 ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Load size and verify
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(keyboard_t)) {
		return FALSE;
	}

	// Load main
	if (!fio->Read(&keyboard, (int)sz)) {
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
//	Apply config
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::ApplyCfg(const Config *config)
{
 ASSERT(config);
	LOG0(Log::Normal, "Apply config");

	// Connect
	Connect(config->kbd_connect);
}

//---------------------------------------------------------------------------
//
//	Connect
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::Connect(BOOL connect)
{
	int i;

 ASSERT(this);

	// If already same, do nothing
	if (keyboard.connect == connect) {
		return;
	}

#if defined(KEYBOARD_LOG)
	if (connect) {
		LOG0(Log::Normal, "Keyboard connect");
	}
	else {
		LOG0(Log::Normal, "Keyboard disconnect");
	}
#endif	// KEYBOARD_LOG

	// Save connect state
	keyboard.connect = connect;

	// After key off, stop event
	for (i=0; i<0x80; i++) {
		keyboard.status[i] = FALSE;
	}
	keyboard.rep_code = 0;
	event.SetTime(0);

	// If connected, send 0xff
	if (keyboard.connect) {
		mfp->KeyData(0xff);
	}
}

//---------------------------------------------------------------------------
//
//	Get keyboard
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::GetKeyboard(keyboard_t *buffer) const
{
 ASSERT(this);
 ASSERT(buffer);

	// Copy to buffer
	*buffer = keyboard;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL Keyboard::Callback(Event *ev)
{
 ASSERT(this);
 ASSERT(ev);

	// If repeat code not active, stop
	if (keyboard.rep_code == 0) {
		ev->SetTime(0);
		return FALSE;
	}

	// If keyboard disconnected, stop
	if (!keyboard.connect) {
		ev->SetTime(0);
		return FALSE;
	}

	// Repeat count up
	keyboard.rep_count++;

	// Only when idle (Don't output burst: maxim.x)
	if (keyboard.send_en && !keyboard.send_wait) {
#if defined(KEYBOARD_LOG)
		LOG2(Log::Normal, "Repeat$%02X (%d times)", keyboard.rep_code, keyboard.rep_count);
#endif	// KEYBOARD_LOG

		mfp->KeyData(keyboard.rep_code);
	}

	// Set next event time
	ev->SetTime(keyboard.rep_next);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Make
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::MakeKey(DWORD code)
{
 ASSERT(this);
 ASSERT((code >= 0x01) && (code <= 0x73));

	// If keyboard disconnected, do nothing
	if (!keyboard.connect) {
		return;
	}

	// If already make, do nothing
	if (keyboard.status[code]) {
		return;
	}

#if defined(KEYBOARD_LOG)
	LOG1(Log::Normal, "Make $%02X", code);
#endif	// KEYBOARD_LOG

	// Set status
	keyboard.status[code] = TRUE;

	// Start repeat
	keyboard.rep_code = code;
	keyboard.rep_count = 0;
	event.SetTime(keyboard.rep_start);

	// Send Make data to MFP
	if (keyboard.send_en && !keyboard.send_wait) {
		mfp->KeyData(keyboard.rep_code);
	}
}

//---------------------------------------------------------------------------
//
//	Break
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::BreakKey(DWORD code)
{
 ASSERT(this);
 ASSERT((code >= 0x01) && (code <= 0x73));

	// If keyboard disconnected, do nothing
	if (!keyboard.connect) {
		return;
	}

	// If already break, do nothing
	if (!keyboard.status[code]) {
		return;
	}

#if defined(KEYBOARD_LOG)
	LOG1(Log::Normal, "Break $%02X", code);
#endif	// KEYBOARD_LOG

	// Set status
	keyboard.status[code] = FALSE;

	// If repeat key, stop repeat
	if (keyboard.rep_code == (DWORD)code) {
		keyboard.rep_code = 0;
		event.SetTime(0);
	}

	// Send data to MFP
	code |= 0x80;
	if (keyboard.send_en && !keyboard.send_wait) {
		mfp->KeyData(code);
	}
}

//---------------------------------------------------------------------------
//
//	Key data send wait
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::SendWait(BOOL flag)
{
 ASSERT(this);

	keyboard.send_wait = flag;
}

//---------------------------------------------------------------------------
//
//	Command
//
//---------------------------------------------------------------------------
void FASTCALL Keyboard::Command(DWORD data)
{
 ASSERT(this);
 ASSERT(data < 0x100);

	// Queue check
 ASSERT(sync);
 ASSERT(keyboard.cmdnum <= 0x10);
 ASSERT(keyboard.cmdread < 0x10);
 ASSERT(keyboard.cmdwrite < 0x10);

	// Add to queue (Sync exclusive)
	sync->Lock();
	keyboard.cmdbuf[keyboard.cmdwrite] = data;
	keyboard.cmdwrite = (keyboard.cmdwrite + 1) & 0x0f;
	keyboard.cmdnum++;
	if (keyboard.cmdnum > 0x10) {
	 ASSERT(keyboard.cmdnum == 0x11);
		keyboard.cmdnum = 0x10;
		keyboard.cmdread = (keyboard.cmdwrite + 1) & 0x0f;
	}
	sync->Unlock();

	// If keyboard disconnected, do nothing
	if (!keyboard.connect) {
		return;
	}

#if defined(KEYBOARD_LOG)
	LOG1(Log::Normal, "Command output $%02X", data);
#endif	// KEYBOARD_LOG

	// Dispatcher
	if (data < 0x40) {
		return;
	}

	// Key LED
	if (data >= 0x80) {
		keyboard.led = ~data;
		return;
	}

	// Key repeat start
	if ((data & 0xf0) == 0x60) {
		keyboard.rep_start = data & 0x0f;
		keyboard.rep_start *= 100 * 1000 * 2;
		keyboard.rep_start += 200 * 1000 * 2;
		return;
	}

	// Key repeat interval
	if ((data & 0xf0) == 0x70) {
		keyboard.rep_next = data & 0x0f;
		keyboard.rep_next *= (data & 0x0f);
		keyboard.rep_next *= 5 * 1000 * 2;
		keyboard.rep_next += 30 * 1000 * 2;
		return;
	}

	if ((data & 0xf0) == 0x40) {
		data &= 0x0f;
		if (data < 0x08) {
			// MSCTRL setting
			keyboard.msctrl = data & 0x01;
			if (keyboard.msctrl) {
				mouse->MSCtrl(TRUE, 2);
			}
			else {
				mouse->MSCtrl(FALSE, 2);
			}
			return;
		}
		else {
			// Key data output enable/disable
			if (data & 0x01) {
				keyboard.send_en = TRUE;
			}
			else {
				keyboard.send_en = FALSE;
			}
			return;
		}
	}

	// Normal, others
	data &= 0x0f;

	switch (data >> 2) {
		// Display mode key latch switch
		case 0:
			if (data & 0x01) {
				keyboard.tv_mode = TRUE;
			}
			else {
				keyboard.tv_mode = FALSE;
			}
			return;

		// Key LED brightness
		case 1:
			keyboard.bright = data & 0x03;
			return;

		// Display controller on/off
		case 2:
			if (data & 0x01) {
				keyboard.tv_ctrl = TRUE;
			}
			else {
				keyboard.tv_ctrl = FALSE;
			}
			return;

		// OPT2 controller on/off
		case 3:
			if (data & 0x01) {
				keyboard.opt2_ctrl = TRUE;
			}
			else {
				keyboard.opt2_ctrl = FALSE;
			}
			return;
	}

	// Normal, others
 ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Command get
//
//---------------------------------------------------------------------------
BOOL Keyboard::GetCommand(DWORD& data)
{
 ASSERT(this);
 ASSERT(sync);
 ASSERT(keyboard.cmdnum <= 0x10);
 ASSERT(keyboard.cmdread < 0x10);
 ASSERT(keyboard.cmdwrite < 0x10);

	// Lock
	sync->Lock();

	// If no data, FALSE
	if (keyboard.cmdnum == 0) {
		sync->Unlock();
		return FALSE;
	}

	// Get data
	data = keyboard.cmdbuf[keyboard.cmdread];
	keyboard.cmdread = (keyboard.cmdread + 1) & 0x0f;
 ASSERT(keyboard.cmdnum > 0);
	keyboard.cmdnum--;

	// Unlock
	sync->Unlock();

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Command clear
//
//---------------------------------------------------------------------------
void Keyboard::ClrCommand()
{
 ASSERT(this);
 ASSERT(sync);
 ASSERT(keyboard.cmdnum <= 0x10);
 ASSERT(keyboard.cmdread < 0x10);
 ASSERT(keyboard.cmdwrite < 0x10);

	// Lock
	sync->Lock();

	// Clear
	keyboard.cmdnum = 0;
	keyboard.cmdread = 0;
	keyboard.cmdwrite = 0;

	// Unlock
	sync->Unlock();
}
