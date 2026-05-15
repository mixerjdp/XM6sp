//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ Virtual Machine ]
//
//---------------------------------------------------------------------------

#if !defined(vm_h)
#define vm_h

#include "log.h"
#include "schedule.h"
#include "cpu.h"
#include "filepath.h"
#include "host_services.h"

class Fileio;

//===========================================================================
//
//	Virtual machine
//
//===========================================================================
class VM
{
public:

	// Basic methods
	VM();
										// Constructor
	BOOL FASTCALL Init();
										// Initialization
	void FASTCALL Cleanup();
										// Cleanup
	void FASTCALL Reset();
										// Reset
	void FASTCALL ApplyCfg(const Config *config);
										// Apply settings
	BOOL FASTCALL SetRenderMode(int mode);
	int FASTCALL GetRenderMode() const;

	// State save/load
	DWORD FASTCALL OriginalSave(const Filepath& path);
	DWORD FASTCALL OriginalSave(Fileio& fio);
	
	DWORD FASTCALL Save(const Filepath& path);
	DWORD FASTCALL Save(Fileio& fio);
										// Save
	DWORD FASTCALL Load(const Filepath& path);
	DWORD FASTCALL Load(Fileio& fio);
										// Load
	void FASTCALL GetPath(Filepath& path) const;
										// Get path
	void FASTCALL Clear();
										// Clear path

	// Device management
	void FASTCALL AddDevice(Device *device);
										// Add device (called recursively)
	void FASTCALL DelDevice(const Device *device);
										// Delete device (called recursively)
	Device* FASTCALL GetFirstDevice() const	{ return first_device; }
										// Get first device
	Device* FASTCALL SearchDevice(DWORD id) const;
										// Get device by ID

	// Execution
	BOOL FASTCALL Exec(DWORD hus);
										// Execute
	void FASTCALL Trace();
										// Trace
	void FASTCALL Break();
										// Stop execution

	// Version
	void FASTCALL SetVersion(DWORD major, DWORD minor);
										// Set version
	void FASTCALL GetVersion(DWORD& major, DWORD& minor);
										// Get version

	// System functions
	void FASTCALL PowerSW(BOOL sw);
										// Power switch
	BOOL FASTCALL IsPowerSW() const		{ return power_sw; }
										// Get power switch state
	void FASTCALL SetPower(BOOL flag);
										// Set power
	BOOL FASTCALL IsPower() const		{ return power; }
										// Get power state
	void FASTCALL Interrupt() const;
										// NMI interrupt
	void FASTCALL SetHostMessageCallback(host_message_callback_t callback, void *user);
	void FASTCALL SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user);
	void FASTCALL SetHostServices(const host_services_t *services);
	void FASTCALL SetHostFileSystem(FileSys *fs);
						// Host callback for non-fatal messages

	Log log;
										// Log

private:
	BOOL status;
										// Current status
	Device *first_device;
										// First device
	Scheduler *scheduler;
										// Scheduler
	CPU *cpu;
										// CPU
	MFP *mfp;
										// MFP
	RTC *rtc;
										// RTC
	SRAM *sram;
										// SRAM
	BOOL power_sw;
										// Power switch
	BOOL power;
										// Power
	DWORD major_ver;
										// Major version
	DWORD minor_ver;
										// Minor version
	void FASTCALL NotifyHostMessage(const TCHAR* message) const;
	host_message_callback_t host_message_callback;
	void *host_message_user;
	host_sync_callback_t host_lock_vm_callback;
	host_sync_callback_t host_unlock_vm_callback;
	void *host_sync_user;

	Filepath current;
										// Current path
};

#endif	// vm_h






