//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI(ytanaka@ipc-tokai.or.jp)
//	[ Mouse ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "schedule.h"
#include "scc.h"
#include "log.h"
#include "fileio.h"
#include "config.h"
#include "mouse.h"

//===========================================================================
//
//	Mouse
//
//===========================================================================
//#define MOUSE_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Mouse::Mouse(VM *p) : Device(p)
{
	// Set device ID
	dev.id = MAKEID('M', 'O', 'U', 'S');
	dev.desc = "Mouse";
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mouse::Init()
{
	Scheduler *scheduler;

	ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// Get SCC
	scc = (SCC*)vm->SearchDevice(MAKEID('S', 'C', 'C', ' '));
 ASSERT(scc);

	// Get scheduler
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
 ASSERT(scheduler);

	// Event setup
	event.SetDevice(this);
	event.SetDesc("Latency 725us");
	event.SetUser(0);
	event.SetTime(0);
	scheduler->AddEvent(&event);

	// Mouse speed 205/256, no swap, port 1
	mouse.mul = 205;
	mouse.rev = FALSE;
	mouse.port = 1;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::Cleanup()
{
 ASSERT(this);

	// Base class
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::Reset()
{
 ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Set MSCTRL receive to 'L'
	mouse.msctrl = FALSE;

	// Set reset flag
	mouse.reset = TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mouse::Save(Fileio *fio, int ver)
{
	size_t sz;

 ASSERT(this);
	LOG0(Log::Normal, "Save");

	// Save size
	sz = sizeof(mouse_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&mouse, sizeof(mouse))) {
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
BOOL FASTCALL Mouse::Load(Fileio *fio, int ver)
{
	size_t sz;

 ASSERT(this);
	LOG0(Log::Normal, "Load");

	// Load body
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(mouse_t)) {
		return FALSE;
	}
	if (!fio->Read(&mouse, sizeof(mouse_t))) {
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
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::ApplyCfg(const Config *config)
{
 ASSERT(this);
 ASSERT(config);
	LOG0(Log::Normal, "Apply configuration");

	// Speed
	if (mouse.mul != config->mouse_speed) {
		// Update speed value
		mouse.mul = config->mouse_speed;
		mouse.reset = TRUE;
	}

	// Swap flag
	mouse.rev = config->mouse_swap;

	// Port
	mouse.port = config->mouse_port;
	if (mouse.port == 0) {
		// Not connected, so stop event
		mouse.reset = TRUE;
		event.SetTime(0);
	}
}

//---------------------------------------------------------------------------
//
//	Get mouse data
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::GetMouse(mouse_t *buffer)
{
 ASSERT(this);
 ASSERT(buffer);

	// Copy structure
	*buffer = mouse;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL Mouse::Callback(Event* /*ev*/)
{
	DWORD status;
	int dx;
	int dy;

 ASSERT(this);
 ASSERT(scc);

	// Create status
	status = 0;

	// Create button
	if (mouse.left) {
		status |= 0x01;
	}
	if (mouse.right) {
		status |= 0x02;
	}

	// Create X delta
	dx = mouse.x - mouse.px;
	dx *= mouse.mul;
	dx >>= 8;
	if (dx > 0x7f) {
		dx = 0x7f;
		status |= 0x10;
	}
	if (dx < -0x80) {
		dx = -0x80;
		status |= 0x20;
	}

	// Create Y delta
	dy = mouse.y - mouse.py;
	dy *= mouse.mul;
	dy >>= 8;
	if (dy > 0x7f) {
		dy = 0x7f;
		status |= 0x40;
	}
	if (dy < -0x80) {
		dy = -0x80;
		status |= 0x80;
	}

	// If SCC not receiving, ignore
	if (!scc->IsRxEnable(1)) {
		return FALSE;
	}

	// If not 4800bps, framing error
	if (!scc->IsBaudRate(1, 4800)) {
#if defined(MOUSE_LOG)
		LOG0(Log::Normal, "SCC baudrate error");
#endif	// MOUSE_LOG
		scc->FramingErr(1);
		return FALSE;
	}

	// If not 8bit data, framing error
	if (scc->GetRxBit(1) != 8) {
#if defined(MOUSE_LOG)
		LOG0(Log::Normal, "SCC data bit error");
#endif	// MOUSE_LOG
		scc->FramingErr(1);
		return FALSE;
	}

	// If not 2 stop bits, framing error
	if (scc->GetStopBit(1) != 3) {
#if defined(MOUSE_LOG)
		LOG0(Log::Normal, "SCC stop bit error");
#endif	// MOUSE_LOG
		scc->FramingErr(1);
		return FALSE;
	}

	// If not no parity, parity error
	if (scc->GetParity(1) != 0) {
#if defined(MOUSE_LOG)
		LOG0(Log::Normal, "SCC parity error");
#endif	// MOUSE_LOG
		scc->ParityErr(1);
		return FALSE;
	}

	// If SCC Rx buffer has data, cannot receive
	if (!scc->IsRxBufEmpty(1)) {
#if defined(MOUSE_LOG)
		LOG0(Log::Normal, "SCC Rx buffer has data");
#endif	// MOUSE_LOG
		return FALSE;
	}

	// Send data (3 bytes)
#if defined(MOUSE_LOG)
	LOG3(Log::Normal, "Send data St:$%02X X:$%02X Y:$%02X", status, dx & 0xff, dy & 0xff);
#endif	// MOUSE_LOG
	scc->Send(1, status);
	scc->Send(1, dx);
	scc->Send(1, dy);

	// Update previous data
	mouse.px = mouse.x;
	mouse.py = mouse.y;

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	MSCTRL processing
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::MSCtrl(BOOL flag, int port)
{
 ASSERT(this);
 ASSERT((port == 1) || (port == 2));

#if defined(MOUSE_LOG)
	LOG2(Log::Normal, "PORT=%d MSCTRL=%d", port, flag);
#endif	// MOUSE_LOG

	// If port does not match, ignore
	if (port != mouse.port) {
		return;
	}

	// Set event from 'H' to 'L'
	if (flag) {
		mouse.msctrl = TRUE;
		return;
	}

	// Check previous 'H'
	if (!mouse.msctrl) {
		return;
	}

	// Clear
	mouse.msctrl = FALSE;

	// If event is not zero
	if (event.GetTime() != 0) {
		return;
	}

	// Set event (725us)
	event.SetTime(725 * 2);
}

//---------------------------------------------------------------------------
//
//	Set mouse data
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::SetMouse(int x, int y, BOOL left, BOOL right)
{
 ASSERT(this);

	// Set data
	mouse.x = x;
	mouse.y = y;
	if (mouse.rev) {
		mouse.left = right;
		mouse.right = left;
	}
	else {
		mouse.left = left;
		mouse.right = right;
	}

	// If reset flag is set, reset position
	if (mouse.reset) {
		mouse.reset = FALSE;
		mouse.px = x;
		mouse.py = y;
	}
}

//---------------------------------------------------------------------------
//
//	Reset mouse data
//
//---------------------------------------------------------------------------
void FASTCALL Mouse::ResetMouse()
{
 ASSERT(this);

	mouse.reset = TRUE;
}