//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//Copyright (C) 2001-2006  PI.(ytanaka@ipc-tokai.or.jp)
//	[ SASI ]
//
//---------------------------------------------------------------------------

#if !defined(sasi_h)
#define sasi_h

#include "device.h"
#include "event.h"

class Disk;
class SCSIMO;
class SCSI;
//===========================================================================
//
//	SASI
//
//===========================================================================
class SASI : public MemDevice
{
public:
	//Maximum count
	enum {
		SASIMax = 16,					//Maximum SASI disk count (including LUN)
		SCSIMax = 6						//Maximum SCSI hard disk count
	};

	//Phase definition
	enum phase_t {
		busfree,						//Bus free phase
		selection,						//Selection phase
		command,						//Command phase
		execute,						//Execution phase
		read,							//Read phase
		write,							//Write phase
		status,							//Status phase
		message							//Message phase
	};

	//Internal data definition
	typedef struct {
		//SASI controller
		phase_t phase;					//Phase
		BOOL sel;						//Select signal
		BOOL msg;						//Message signal
		BOOL cd;						//Command/Data signal
		BOOL io;						//Input/Output signal
		BOOL bsy;						//Busy signal
		BOOL req;						//Request signal
		DWORD ctrl;						//Selected controller
		DWORD cmd[10];					//Command data
		DWORD status;					//Status data
		DWORD message;					//Message data
		BYTE buffer[0x800];				//Transfer buffer
		DWORD blocks;					//Transfer block count
		DWORD next;						//Next record
		DWORD offset;					//Transfer offset
		DWORD length;					//Transfer remaining length

		//Disk
		Disk *disk[SASIMax];			//Disk
		Disk *current;					//Disk (current)
		SCSIMO *mo;						//Disk (MO)

		//Config
		int sasi_drives;				//Drive count (SASI)
		BOOL memsw;						//Memory switch update
		BOOL parity;					//Add parity
		int sxsi_drives;				//Drive count (SxSI)
		BOOL mo_first;					//MO priority flag (SxSI)
		int scsi_type;					//SCSI type

		//MO parameter
		BOOL writep;					//MO write protect flag
	} sasi_t;

public:
	//Basic functions
	SASI(VM *p);
										//Constructor
	BOOL FASTCALL Init();
										//Initialization
	void FASTCALL Cleanup();
										//Cleanup
	void FASTCALL Reset();
										//Reset
	BOOL FASTCALL Save(Fileio *fio, int ver);
										//Save
	BOOL FASTCALL Load(Fileio *fio, int ver);
										//Load
	void FASTCALL ApplyCfg(const Config *config);
										//Apply settings

	//Memory device
	DWORD FASTCALL ReadByte(DWORD addr);
										//Byte read
	DWORD FASTCALL ReadWord(DWORD addr);
										//Word read
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										//Byte write
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										//Word write
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										//Read only

	//MO access
	BOOL FASTCALL Open(const Filepath& path);
										//MO open
	void FASTCALL Eject(BOOL force);
										//MO eject
	void FASTCALL WriteP(BOOL writep);
										//MO write protect
	BOOL FASTCALL IsWriteP() const;
										//Check MO write protect
	BOOL FASTCALL IsReadOnly() const;
										//Check MO ReadOnly
	BOOL FASTCALL IsLocked() const;
										//Check MO Lock
	BOOL FASTCALL IsReady() const;
										//Check MO Ready
	BOOL FASTCALL IsValid() const;
										//Check MO valid
	void FASTCALL GetPath(Filepath &path) const;
										//Get MO path

	//External API
	void FASTCALL GetSASI(sasi_t *buffer) const;
										//Get internal data
	BOOL FASTCALL Callback(Event *ev);
										//Event callback
	void FASTCALL Construct();
										//Rebuild disk
	BOOL FASTCALL IsBusy() const;
										//Get HD BUSY
	DWORD FASTCALL GetBusyDevice() const;
										//Get BUSY device

private:
	DWORD FASTCALL ReadData();
										//Data read
	void FASTCALL WriteData(DWORD data);
										//Data write

	//Phase processing
	void FASTCALL BusFree();
										//Bus free phase
	void FASTCALL Selection(DWORD data);
										//Selection phase
	void FASTCALL Command();
										//Command phase
	void FASTCALL Execute();
										//Execution phase
	void FASTCALL Status();
										//Status phase
	void FASTCALL Message();
										//Message phase
	void FASTCALL Error();
										//Common error processing

	//Command
	void FASTCALL TestUnitReady();
										//TEST UNIT READY command
	void FASTCALL Rezero();
										//REZERO UNIT command
	void FASTCALL RequestSense();
										//REQUEST SENSE command
	void FASTCALL Format();
										//FORMAT command
	void FASTCALL Reassign();
										//REASSIGN BLOCKS command
	void FASTCALL Read6();
										//READ(6) command
	void FASTCALL Write6();
										//WRITE(6) command
	void FASTCALL Seek6();
										//SEEK(6) command
	void FASTCALL Assign();
										//ASSIGN command
	void FASTCALL Inquiry();
										//INQUIRY command
	void FASTCALL ModeSense();
										//MODE SENSE command
	void FASTCALL StartStop();
										//START STOP UNIT command
	void FASTCALL Removal();
										//PREVENT/ALLOW MEDIUM REMOVAL command
	void FASTCALL ReadCapacity();
										//READ CAPACITY command
	void FASTCALL Read10();
										//READ(10) command
	void FASTCALL Write10();
										//WRITE(10) command
	void FASTCALL Seek10();
										//SEEK(10) command
	void FASTCALL Verify();
										//VERIFY command
	void FASTCALL Specify();
										//SPECIFY command

	//Work area
	sasi_t sasi;
										//Internal data
	Event event;
										//Event
	Filepath sasihd[SASIMax];
										//SASI-HD file path
	Filepath scsihd[SCSIMax];
										//SCSI-HD file path
	Filepath scsimo;
										//SCSI-MO file path
	DMAC *dmac;
										// DMAC
	IOSC *iosc;
										// IOSC
	SRAM *sram;
										// SRAM
	SCSI *scsi;
										// SCSI
	BOOL sxsicpu;
										//SxSI CPU transfer flag
};

#endif	// sasi_h
