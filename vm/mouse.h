//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2003 PI(ytanaka@ipc-tokai.or.jp)
//	[ Mouse ]
//
//---------------------------------------------------------------------------

#if !defined(mouse_h)
#define mouse_h

#include "device.h"
#include "event.h"

//===========================================================================
//
//	Mouse
//
//===========================================================================
class Mouse : public Device
{
public:
	// Internal data structure
	typedef struct {
		BOOL msctrl;					// MSCTRL receive
		BOOL reset;						// Reset flag
		BOOL left;						// Left button
		BOOL right;						// Right button
		int x;							// x coordinate
		int y;							// y coordinate
		int px;							// Previous x coordinate
		int py;							// Previous y coordinate
		int mul;						// Mouse speed (256 is scale)
		BOOL rev;						// Left/right swap flag
		int port;						// Connection port
	} mouse_t;

public:
	// Basic operations
	Mouse(VM *p);
										// Constructor
	BOOL FASTCALL Init();
										// Initialize
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	void FASTCALL ApplyCfg(const Config *config);
										// Apply configuration

	// External API
	void FASTCALL GetMouse(mouse_t *buffer);
										// Get mouse data
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL MSCtrl(BOOL flag, int port = 1);
										// MSCTRL processing
	void FASTCALL SetMouse(int x, int y, BOOL left, BOOL right);
										// Set mouse data
	void FASTCALL ResetMouse();
										// Reset mouse data

private:
	mouse_t mouse;
										// Mouse data
	Event event;
										// Event
	SCC *scc;
										// SCC
};

#endif	// mouse_h