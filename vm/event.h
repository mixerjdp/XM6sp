//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Event ]
//
//---------------------------------------------------------------------------

#if !defined(event_h)
#define event_h

//===========================================================================
//
//	Event
//
//===========================================================================
class Event
{
public:
	// Internal data definition
#if defined(_WIN32)
#pragma pack(push, 8)
#endif	// _WIN32
	typedef struct {
		DWORD remain;					// Remaining time
		DWORD time;						// Total time
		DWORD user;						// User defined data
		Device *device;					// Parent device
		Scheduler *scheduler;			// Scheduler
		Event *next;					// Next event
		char desc[0x20];				// Description
	} event_t;
#if defined(_WIN32)
#pragma pack(pop)
#endif	// _WIN32

public:
	// Basic functions
	Event();
										// Constructor
	virtual ~Event();
										// Destructor
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Diagnosis
#endif	// NDEBUG

	// Load/Save
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

	// Properties
	void FASTCALL SetDevice(Device *p);
										// Set parent device
	Device* FASTCALL GetDevice() const	{ return ev.device; }
										// Get parent device
	void FASTCALL SetDesc(const char *desc);
										// Set description
	const char* FASTCALL GetDesc() const;
										// Get description
	void FASTCALL SetUser(DWORD data)	{ ev.user = data; }
										// Set user defined data
	DWORD FASTCALL GetUser() const		{ return ev.user; }
										// Get user defined data

	// Time management
	void FASTCALL SetTime(DWORD hus);
										// Set time period
	DWORD FASTCALL GetTime() const		{ return ev.time; }
										// Get time period
	DWORD FASTCALL GetRemain() const	{ return ev.remain; }
										// Get remaining time
	void FASTCALL Exec(DWORD hus);
										// Advance time

	// Link setting/removal
	void FASTCALL SetNextEvent(Event *p) { ev.next = p; }
										// Set next event
	Event* FASTCALL GetNextEvent() const { return ev.next; }
										// Get next event

private:
	// Internal data definition (Ver 2.01 and earlier. has enable)
	typedef struct {
		Device *device;					// Parent device
		Scheduler *scheduler;			// Scheduler
		Event *next;					// Next event
		char desc[0x20];				// Description
		DWORD user;						// User defined data
		BOOL enable;					// Enable time
		DWORD time;						// Total time
		DWORD remain;					// Remaining time
	} event201_t;

	BOOL FASTCALL Load201(Fileio *fio);
										// Load (version 2.01 and earlier)
public:
	event_t ev;
										// Internal work area
};

#endif	// event_h
