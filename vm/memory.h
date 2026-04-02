//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
//	[ Memory ]
//
//---------------------------------------------------------------------------

#if !defined(memory_h)
#define memory_h

#include "device.h"
#include "musashi_adapter.h"

//===========================================================================
//
//	External functions
//
//===========================================================================
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus
void ReadBusErr(DWORD addr);
										// Read bus error
void WriteBusErr(DWORD addr);
										// Write bus error
#if defined(__cplusplus)
}
#endif	// __cplusplus

//===========================================================================
//
//	Memory
//
//===========================================================================
class Memory : public MemDevice
{
public:
	// Memory type (= system type)
	enum memtype {
		None,							// Not loaded
		SASI,							// v1.00-SASI(first/ACE/EXPERT/PRO)
		SCSIInt,						// v1.00-SCSI internal(SUPER)
		SCSIExt,						// v1.00-SCSI external board(first/ACE/EXPERT/PRO)
		XVI,							// v1.10-SCSI internal(XVI)
		Compact,						// v1.20-SCSI internal(Compact)
		X68030							// v1.50-SCSI internal(X68030)
	};

	// Internal data definition
	typedef struct {
		MemDevice* table[0x180];		// Jump table
		int size;						// RAM size(2,4,6,8,10,12)
		int config;						// RAM setting value(0~5)
		DWORD length;					// RAM last byte+1
		BYTE *ram;						// Main RAM
		BYTE *ipl;						// IPL ROM (128KB)
		BYTE *cg;						// CG ROM(768KB)
		BYTE *scsi;						// SCSI ROM (8KB)
		memtype type;					// Memory type(after reset)
		memtype now;					// Memory type(current)
		BOOL memsw;						// Memory switch auto update
	} memory_t;

public:
	// Basic functions
	Memory(VM *p);
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
	void FASTCALL MakeContext(BOOL reset);
										// Memory context create
	BOOL FASTCALL CheckIPL() const;
										// IPL version check
	BOOL FASTCALL CheckCG() const;
										// CG check
	const BYTE* FASTCALL GetCG() const;
										// CG get
	const BYTE* FASTCALL GetSCSI() const;
										// SCSI get
	memtype FASTCALL GetMemType() const { return mem.now; }
										// Memory type get
	BYTE* FASTCALL GetRAM() const { return mem.ram; }
	DWORD FASTCALL GetRAMSize() const { return mem.length; }

private:
	BOOL FASTCALL LoadROM(memtype target);
										// ROM load
	void FASTCALL InitTable();
										// Decode table init
	void FASTCALL TerminateProgramRegion(int index, STARSCREAM_PROGRAMREGION *spr);
										// Program region terminate
	void FASTCALL TerminateDataRegion(int index, STARSCREAM_DATAREGION *sdr);
										// Data region terminate
	AreaSet *areaset;
										// Area set
	SRAM *sram;
										// SRAM
	memory_t mem;
										// Internal data

	// Region (Starscream specific)
	STARSCREAM_PROGRAMREGION u_pgr[10];
										// Program region(User)
	STARSCREAM_PROGRAMREGION s_pgr[10];
										// Program region(Super)
	STARSCREAM_DATAREGION u_rbr[10];
										// Read Byte region(User)
	STARSCREAM_DATAREGION s_rbr[10];
										// Read Byte region(Super)
	STARSCREAM_DATAREGION u_rwr[10];
										// Read Word region(User)
	STARSCREAM_DATAREGION s_rwr[10];
										// Read Word region(Super)
	STARSCREAM_DATAREGION u_wbr[10];
										// Write Byte region(User)
	STARSCREAM_DATAREGION s_wbr[10];
										// Write Byte region(Super)
	STARSCREAM_DATAREGION u_wwr[10];
										// Write Word region(User)
	STARSCREAM_DATAREGION s_wwr[10];
										// Write Word region(Super)
};

#endif	// memory_h
