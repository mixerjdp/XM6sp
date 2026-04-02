//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Device class ]
//
//---------------------------------------------------------------------------

#if !defined(device_h)
#define device_h

//===========================================================================
//
//	Device
//
//===========================================================================
class Device
{
public:
	// Internal data definition
	typedef struct {
		DWORD id;						// ID
		const char *desc;				// Description
		Device* next;					// Next device
	} device_t;

public:
	// Basic functions
	Device(VM *p);
										// Constructor
	virtual ~Device();
										// Destructor
	virtual BOOL FASTCALL Init();
										// Initialization
	virtual void FASTCALL Cleanup();
										// Cleanup
	virtual void FASTCALL Reset();
										// Reset
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load
	virtual void FASTCALL ApplyCfg(const Config *config);
										// Apply configuration
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
#endif	// NDEBUG

	// External API
	Device* FASTCALL GetNextDevice() const { return dev.next; }
										// Get next device
	void FASTCALL SetNextDevice(Device *p) { dev.next = p; }
										// Set next device
	DWORD FASTCALL GetID() const		{ return dev.id; }
										// Get device ID
	const char* FASTCALL GetDesc() const { return dev.desc; }
										// Get device name
	VM* FASTCALL GetVM() const			{ return vm; }
										// Get VM
	virtual BOOL FASTCALL Callback(Event *ev);
										// Event callback

protected:
	Log* FASTCALL GetLog() const		{ return log; }
										// Get log
	device_t dev;
										// Internal data
	VM *vm;
										// Virtual machine
	Log *log;
										// Log
};

//===========================================================================
//
//	Memory mapped device
//
//===========================================================================
class MemDevice : public Device
{
public:
	// Internal data definition
	typedef struct {
		DWORD first;					// Start address
		DWORD last;						// End address
	} memdev_t;

public:
	// Basic functions
	MemDevice(VM *p);
										// Constructor
	virtual BOOL FASTCALL Init();
										// Initialization

	// Memory device interface
	virtual DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	virtual DWORD FASTCALL ReadWord(DWORD addr);
										// Word read
	virtual void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	virtual void FASTCALL WriteWord(DWORD addr, DWORD data);
										// Word write
	virtual DWORD FASTCALL ReadOnly(DWORD addr) const;
										// Read only
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
										// Assert
#endif	// NDEBUG

	// External API
	DWORD FASTCALL GetFirstAddr() const	{ return memdev.first; }
										// Get first address
	DWORD FASTCALL GetLastAddr() const	{ return memdev.last; }
										// Get last address

protected:
	memdev_t memdev;
										// Internal data
	CPU *cpu;
										// CPU
	Scheduler *scheduler;
										// Scheduler
};

#endif	// device_h
