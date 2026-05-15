//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI. (ytanaka@ipc-tokai.or.jp)
//	[ SCSI(MB89352) ]
//
//---------------------------------------------------------------------------

#if !defined(scsi_h)
#define scsi_h

#include "device.h"
#include "event.h"

class Disk;
class SCSIMO;
class SCSICD;
//===========================================================================
//
//	SCSI
//
//===========================================================================
class SCSI : public MemDevice
{
public:
	// Maximum counts
	enum {
		DeviceMax = 8,					// Maximum SCSI devices
		HDMax = 5						// Maximum SCSI HDs
	};

	// Phase definitions
	enum phase_t {
		busfree,						// Bus free phase
		arbitration,					// Arbitration phase
		selection,						// Selection phase
		reselection,					// Reselection phase
		command,						// Command phase
		execute,						// Execute phase
		msgin,							// Message in phase
		msgout,							// Message out phase
		datain,							// Data in phase
		dataout,						// Data out phase
		status							// Status phase
	};

	// Internal data definition
	typedef struct {
		// General
		int type;						// SCSI type (0:none 1:external 2:internal)
		phase_t phase;					// Phase
		int id;							// Current ID (0-7)

		// Interrupt
		int vector;						// Request vector (-1 if no request)
		int ilevel;						// Interrupt level

		// Signals
		BOOL bsy;						// Busy signal
		BOOL sel;						// Select signal
		BOOL atn;						// Attention signal
		BOOL msg;						// Message signal
		BOOL cd;						// Command/Data signal
		BOOL io;						// Input/Output signal
		BOOL req;						// Request signal
		BOOL ack;						// Ack signal
		BOOL rst;						// Reset signal

		// Registers
		DWORD bdid;						// BDID register (bit representation)
		DWORD sctl;						// SCTL register
		DWORD scmd;						// SCMD register
		DWORD ints;						// INTS register
		DWORD sdgc;						// SDGC register
		DWORD pctl;						// PCTL register
		DWORD mbc;						// MBC register
		DWORD temp;						// TEMP register
		DWORD tc;						// TCH, TCM, TCL registers

		// Command
		DWORD cmd[10];					// Command data
		DWORD status;					// Status data
		DWORD message;					// Message data

		// Transfer
		BOOL trans;						// Transfer flag
		BYTE buffer[0x800];				// Transfer buffer
		DWORD blocks;					// Transfer block count
		DWORD next;						// Next record
		DWORD offset;					// Transfer offset
		DWORD length;					// Transfer remaining length

		// Configuration
		int scsi_drives;				// SCSI drive count
		BOOL memsw;						// Memory switch update
		BOOL mo_first;					// MO priority flag (SxSI)

		// Disk
		Disk *disk[DeviceMax];			// Devices
		Disk *hd[HDMax];				// HD
		SCSIMO *mo;						// MO
		SCSICD *cdrom;					// CD-ROM
	} scsi_t;

public:
	// Basic functions
	SCSI(VM *p);
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
#if !defined(NDEBUG)
	void FASTCALL AssertDiag() const;
									// Diagnostics
#endif	// NDEBUG

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
	void FASTCALL GetSCSI(scsi_t *buffer) const;
									// Get internal data
	BOOL FASTCALL Callback(Event *ev);
									// Event callback
	void FASTCALL IntAck(int level);
									// Interrupt ACK
	int FASTCALL GetSCSIID() const;
									// Get SCSI-ID
	BOOL FASTCALL IsBusy() const;
									// Is BUSY
	DWORD FASTCALL GetBusyDevice() const;
									// Get BUSY device

	// MO/CD access
	BOOL FASTCALL Open(const Filepath& path, BOOL mo = TRUE);
									// MO/CD open
	void FASTCALL Eject(BOOL force, BOOL mo = TRUE);
									// MO/CD eject
	void FASTCALL WriteP(BOOL writep);
									// MO write protect
	BOOL FASTCALL IsWriteP() const;
									// MO write protect check
	BOOL FASTCALL IsReadOnly() const;
									// MO read-only check
	BOOL FASTCALL IsLocked(BOOL mo = TRUE) const;
									// MO/CD lock check
	BOOL FASTCALL IsReady(BOOL mo = TRUE) const;
									// MO/CD ready check
	BOOL FASTCALL IsValid(BOOL mo = TRUE) const;
									// MO/CD valid check
	void FASTCALL GetPath(Filepath &path, BOOL mo = TRUE) const;
									// Get MO/CD path

	// CD-DA
	void FASTCALL GetBuf(DWORD *buffer, int samples, DWORD rate);
									// Get CD-DA buffer

private:
	// Registers
	void FASTCALL ResetReg();
									// Reset registers
	void FASTCALL ResetCtrl();
									// Reset transfer
	void FASTCALL ResetBus(BOOL reset);
									// Reset bus
	void FASTCALL SetBDID(DWORD data);
									// Set BDID
	void FASTCALL SetSCTL(DWORD data);
									// Set SCTL
	void FASTCALL SetSCMD(DWORD data);
									// Set SCMD
	void FASTCALL SetINTS(DWORD data);
									// Set INTS
	DWORD FASTCALL GetPSNS() const;
									// Get PSNS
	void FASTCALL SetSDGC(DWORD data);
									// Set SDGC
	DWORD FASTCALL GetSSTS() const;
									// Get SSTS
	DWORD FASTCALL GetSERR() const;
									// Get SERR
	void FASTCALL SetPCTL(DWORD data);
									// Set PCTL
	DWORD FASTCALL GetDREG();
									// Get DREG
	void FASTCALL SetDREG(DWORD data);
									// Set DREG
	void FASTCALL SetTEMP(DWORD data);
									// Set TEMP
	void FASTCALL SetTCH(DWORD data);
									// Set TCH
	void FASTCALL SetTCM(DWORD data);
									// Set TCM
	void FASTCALL SetTCL(DWORD data);
									// Set TCL

	// SPC commands
	void FASTCALL BusRelease();
									// Bus release
	void FASTCALL Select();
									// Selection/Reselection
	void FASTCALL ResetATN();
									// ATN line = 0
	void FASTCALL SetATN();
									// ATN line = 1
	void FASTCALL Transfer();
									// Transfer
	void FASTCALL TransPause();
									// Transfer pause
	void FASTCALL TransComplete();
									// Transfer complete
	void FASTCALL ResetACKREQ();
									// ACK/REQ line = 0
	void FASTCALL SetACKREQ();
									// ACK/REQ line = 1

	// Data transfer
	void FASTCALL Xfer(DWORD *reg);
									// Data transfer
	void FASTCALL XferNext();
									// Continue data transfer
	BOOL FASTCALL XferIn();
									// Data transfer IN
	BOOL FASTCALL XferOut(BOOL cont);
									// Data transfer OUT
	BOOL FASTCALL XferMsg(DWORD msg);
									// Data transfer MSG

	// Phases
	void FASTCALL BusFree();
									// Bus free phase
	void FASTCALL Arbitration();
									// Arbitration phase
	void FASTCALL Selection();
									// Selection phase
	void FASTCALL SelectTime();
									// Selection phase (time setting)
	void FASTCALL Command();
									// Command phase
	void FASTCALL Execute();
									// Execute phase
	void FASTCALL MsgIn();
									// Message in phase
	void FASTCALL MsgOut();
									// Message out phase
	void FASTCALL DataIn();
									// Data in phase
	void FASTCALL DataOut();
									// Data out phase
	void FASTCALL Status();
									// Status phase

	// Interrupt
	void FASTCALL Interrupt(int type, BOOL flag);
									// Interrupt request
	void FASTCALL IntCheck();
									// Interrupt check

	// SCSI command common
	void FASTCALL Error();
									// Common error

	// SCSI commands
	void FASTCALL TestUnitReady();
									// TEST UNIT READY command
	void FASTCALL Rezero();
									// REZERO UNIT command
	void FASTCALL RequestSense();
									// REQUEST SENSE command
	void FASTCALL Format();
									// FORMAT UNIT command
	void FASTCALL Reassign();
									// REASSIGN BLOCKS command
	void FASTCALL Read6();
									// READ(6) command
	void FASTCALL Write6();
									// WRITE(6) command
	void FASTCALL Seek6();
									// SEEK(6) command
	void FASTCALL Inquiry();
									// INQUIRY command
	void FASTCALL ModeSelect();
									// MODE SELECT command
	void FASTCALL ModeSense();
									// MODE SENSE command
	void FASTCALL StartStop();
									// START STOP UNIT command
	void FASTCALL SendDiag();
									// SEND DIAGNOSTIC command
	void FASTCALL Removal();
									// PREVENT/ALLOW MEDIUM REMOVAL command
	void FASTCALL ReadCapacity();
									// READ CAPACITY command
	void FASTCALL Read10();
									// READ(10) command
	void FASTCALL Write10();
									// WRITE(10) command
	void FASTCALL Seek10();
									// SEEK(10) command
	void FASTCALL Verify();
									// VERIFY command
	void FASTCALL ReadToc();
									// READ TOC command
	void FASTCALL PlayAudio10();
									// PLAY AUDIO(10) command
	void FASTCALL PlayAudioMSF();
									// PLAY AUDIO MSF command
	void FASTCALL PlayAudioTrack();
									// PLAY AUDIO TRACK INDEX command

	// CD-ROM/CD-DA
	Event cdda;
									// Frame event

	// Drive/FilePath
	void FASTCALL Construct();
									// Build drives
	Filepath scsihd[DeviceMax];
									// SCSI-HD file path

	// Other
	scsi_t scsi;
									// Internal data
	BYTE verifybuf[0x800];
									// Verify buffer
	Event event;
									// Event
	Memory *memory;
									// Memory
	SRAM *sram;
									// SRAM
};

#endif	// scsi_h
