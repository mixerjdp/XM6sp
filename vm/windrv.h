//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 PI(ytanaka@ipc-tokai.or.jp)
//	Modified (C) 2006 co (cogood@gmail.com)
//	[ Windrv ]
//
//---------------------------------------------------------------------------

#if !defined(windrv_h)
#define windrv_h

#include "os.h"
#include "device.h"
#include "host_fs.h"
#include "host_services.h"

// Maximum threads
#define WINDRV_THREAD_MAX	3

// WINDRV compatible support
#define WINDRV_SUPPORT_COMPATIBLE

// Debug output enable
#ifdef _DEBUG
#define WINDRV_LOG
#endif // _DEBUG


class CWindrv : public WindrvContext {
public:
	// Member function
	CWindrv(Windrv* pWindrv, Memory* pMemory, DWORD nHandle = 0);
	// Constructor
	virtual ~CWindrv();
		// Destructor

// Thread functions
	BOOL FASTCALL Start();
// Thread start
	BOOL FASTCALL Terminate();
// Thread terminate

// Windrv legacy API
	void FASTCALL Execute(DWORD nA5);
// Command execution
#ifdef WINDRV_SUPPORT_COMPATIBLE
	DWORD FASTCALL ExecuteCompatible(DWORD nA5);
	// Command execution (WINDRV compatible)
#endif // WINDRV_SUPPORT_COMPATIBLE
	DWORD FASTCALL GetHandle() const { ASSERT(this); return m_nHandle; }
// Get own handle (buffer number)
	BOOL FASTCALL isExecute() const { ASSERT(this); return m_bExecute; }
// Command execution - check if executing

// CWindrvManager legacy API
	void FASTCALL SetAlloc(BOOL bAlloc) { ASSERT(this); m_bAlloc = bAlloc; }
	BOOL FASTCALL isAlloc() const { ASSERT(this); return m_bAlloc; }
// Check if handle is in use - check if in use

// FileSys legacy API
	DWORD FASTCALL GetUnit() const { ASSERT(this); return unit; }
// Get unit number
	Memory* FASTCALL GetMemory() const { ASSERT(this); return memory; }
// Get unit number
	void FASTCALL LockXM();
// Start VM access
	void FASTCALL UnlockXM();
// End VM access
	void FASTCALL Ready();
// Wait for command to complete, run VM thread

// Thread functions
	static DWORD __stdcall Run(void* pThis);
// Thread execution start point
	void FASTCALL Runner();
									// Thread functions

private:
// Command handler
	void FASTCALL ExecuteCommand();
									// Command handler

	void FASTCALL InitDrive();
										// $40 - Init drive
	void FASTCALL CheckDir();
										// $41 - Check directory
	void FASTCALL MakeDir();
										// $42 - Make directory
	void FASTCALL RemoveDir();
										// $43 - Remove directory
	void FASTCALL Rename();
										// $44 - Rename file
	void FASTCALL Delete();
										// $45 - Delete file
	void FASTCALL Attribute();
										// $46 - Get/set file attributes
	void FASTCALL Files();
										// $47 - Find file (First)
	void FASTCALL NFiles();
										// $48 - Find file (NFiles)
	void FASTCALL Create();
										// $49 - Create file
	void FASTCALL Open();
										// $4A - Open file
	void FASTCALL Close();
										// $4B - Close file
	void FASTCALL Read();
										// $4C - Read file
	void FASTCALL Write();
										// $4D - Write file
	void FASTCALL Seek();
										// $4E - Seek file
	void FASTCALL TimeStamp();
										// $4F - Get/set file timestamp
	void FASTCALL GetCapacity();
										// $50 - Get capacity
	void FASTCALL CtrlDrive();
										// $51 - Drive control/info
	void FASTCALL GetDPB();
										// $52 - Get DPB
	void FASTCALL DiskRead();
										// $53 - Read sector
	void FASTCALL DiskWrite();
										// $54 - Write sector
	void FASTCALL IoControl();
										// $55 - IOCTRL
	void FASTCALL Flush();
										// $56 - Flush
	void FASTCALL CheckMedia();
										// $57 - Check media
	void FASTCALL Lock();
										// $58 - Lock

// Result value
	void FASTCALL SetResult(DWORD result);
									// Result value

// Memory access
	DWORD FASTCALL GetByte(DWORD addr) const;
										// Byte read
	void FASTCALL SetByte(DWORD addr, DWORD data);
										// Byte write
	DWORD FASTCALL GetWord(DWORD addr) const;
										// Word
	void FASTCALL SetWord(DWORD addr, DWORD data);
										// Word
	DWORD FASTCALL GetLong(DWORD addr) const;
										// Long read
	void FASTCALL SetLong(DWORD addr, DWORD data);
										// Long write
	DWORD FASTCALL GetAddr(DWORD addr) const;
										// Address
	void FASTCALL SetAddr(DWORD addr, DWORD data);
										// Address

// Structure conversion
	void FASTCALL GetNameStsPath(DWORD addr, Human68k::namests_t* pNamests) const;
										// NAMESTS path
	void FASTCALL GetNameSts(DWORD addr, Human68k::namests_t* pNamests) const;
										// NAMESTS
	void FASTCALL GetFiles(DWORD addr, Human68k::files_t* pFiles) const;
										// FILES
	void FASTCALL SetFiles(DWORD addr, const Human68k::files_t* pFiles);
										// FILES
	void FASTCALL GetFcb(DWORD addr, Human68k::fcb_t* pFcb) const;
										// FCB
	void FASTCALL SetFcb(DWORD addr, const Human68k::fcb_t* pFcb);
										// FCB
	void FASTCALL SetCapacity(DWORD addr, const Human68k::capacity_t* pCapacity);
										// CAPACITY
	void FASTCALL SetDpb(DWORD addr, const Human68k::dpb_t* pDpb);
										// DPB
	void FASTCALL GetIoctrl(DWORD param, DWORD func, Human68k::ioctrl_t* pIoctrl);
										// IOCTRL
	void FASTCALL SetIoctrl(DWORD param, DWORD func, const Human68k::ioctrl_t* pIoctrl);
										// IOCTRL
	void FASTCALL GetParameter(DWORD addr, BYTE* pOption, DWORD nSize);
										// Read parameter

#ifdef WINDRV_LOG
// Log
	void Log(DWORD level, char* format, ...) const;
									// Log output
#endif // WINDRV_LOG

// Command handler
	Windrv* windrv;
										// Settings
	Memory* memory;
									// Settings
	DWORD a5;
										// Request header
	DWORD unit;
										// Unit number
	DWORD command;
										// Execute

// For thread management
	DWORD m_nHandle;
										// Buffer number (also serves as handle) g(0`THREAD_MAX - 1)
	BOOL m_bAlloc;
										// in usepTRUE
	BOOL m_bExecute;
// Command executionTRUE
	BOOL m_bReady;
										// VMs startTRUE
	BOOL m_bTerminate;
										// terminationTRUE
	void* m_hThread;
										// Statushandler
	void* m_hEventStart;
// Command executionJn notification
	void* m_hEventReady;
										// VMs start notification
};

//===========================================================================
//
// Command handler management
//
//===========================================================================
class CWindrvManager {
public:
	void FASTCALL Init(Windrv* pWindrv, Memory* pMemory);
									// Settings
	void FASTCALL Clean();
										// Terminate
	void FASTCALL Reset();
										// Zbg

	CWindrv* FASTCALL Alloc();
										// free threadbhm
	CWindrv* FASTCALL Search(DWORD nHandle);
									// Thread functions
	void FASTCALL Free(CWindrv* p);
										// StatusbhJ

private:
	CWindrv* m_pThread[WINDRV_THREAD_MAX];
									// Command handler
};

//===========================================================================
//
//	Windrv
//
//===========================================================================
class Windrv : public MemDevice
{
public:
// Windrv specific
	enum {
		WINDRV_MODE_NONE = 0,
		WINDRV_MODE_ENABLE = 1,
		WINDRV_MODE_COMPATIBLE = 2,
		WINDRV_MODE_DUAL = 3,
		WINDRV_MODE_DISABLE = 255,
	};

// Configuration data structure`
	typedef struct {
		// RtBO
		DWORD enable;					// WindrvT|[g 0: 1:WindrvXMM (2:Windrv compatible)

		// hCuEt@C
		DWORD drives;					// Number assigned hCu (FileSysANZXE`FbNp)

		// ProcessZX
		FileSys *fs;					// t@CVXe

		// host callback
		host_sync_callback_t lock_vm_cb;
		host_sync_callback_t unlock_vm_cb;
		void *sync_user;
	} windrv_t;

public:
	// Member function
	Windrv(VM *p);
	// Constructor
	BOOL FASTCALL Init();
									// Settings
	void FASTCALL Cleanup();
										// N[Abv
	void FASTCALL Reset();
										// Zbg
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Reset[u
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Word
	void FASTCALL ApplyCfg(const Config *config);
										// ApplyKp

// SettingsfoCX
	DWORD FASTCALL ReadByte(DWORD addr);
										// Byte read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// Byte write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// 

// External API
	void FASTCALL SetFileSys(FileSys *fs);
										// t@CVXe
	void FASTCALL SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user);
	void FASTCALL HostLockVM() const;
	void FASTCALL HostUnlockVM() const;

// Command handler OAPI
	void FASTCALL SetUnitMax(DWORD nUnitMax) { ASSERT(this); windrv.drives = nUnitMax; }
										// Unit number
	BOOL FASTCALL isInvalidUnit(DWORD unit) const;
										// Unit number`FbN
	BOOL FASTCALL isValidUnit(DWORD unit) const { ASSERT(this); return unit < windrv.drives; }
										// Unit number`FbN (ASSERT version)p)
	FileSys* FASTCALL GetFilesystem() const { ASSERT(this); return windrv.fs; }
										// t@CVXe l

#ifdef WINDRV_LOG
// Log
	void FASTCALL Log(DWORD level, char* message) const;
									// Log output
#endif // WINDRV_LOG

private:
#ifdef WINDRV_SUPPORT_COMPATIBLE
	void FASTCALL Execute();
// Command execution
#endif // WINDRV_SUPPORT_COMPATIBLE
	void FASTCALL ExecuteAsynchronous();
// Command executionJn async
	DWORD FASTCALL StatusAsynchronous();
										// Statuse[^Xl async
	void FASTCALL ReleaseAsynchronous();
										// J async

	windrv_t windrv;
										// f[^
	CWindrvManager m_cThread;
									// Command handler
};


#endif // windrv_h






