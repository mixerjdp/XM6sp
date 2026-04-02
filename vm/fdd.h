//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ FDD(FD55GFR) ]
//
//---------------------------------------------------------------------------

#if !defined(fdd_h)
#define fdd_h

#include "device.h"
#include "event.h"
#include "filepath.h"

//---------------------------------------------------------------------------
//
//	Error definition
//	Partially uses ST1, ST2, and abnormal bits
//
//---------------------------------------------------------------------------
#define FDD_NOERROR			0x0000		// No error
#define FDD_EOT				0x8000		// EOT overrun
#define FDD_DDAM			0x4000		// DDAM sector
#define FDD_DATAERR			0x2000		// IDCRC or data CRC (ReadId exception)
#define FDD_OVERRUN			0x1000		// Overrun (sense data)
#define FDD_IDCRC			0x0800		// ID field CRC
#define FDD_NODATA			0x0400		// No valid sector
#define FDD_NOTWRITE		0x0200		// Write protect and write error
#define FDD_MAM				0x0100		// Address mark not found
#define FDD_NOTREADY		0x0080		// Drive not ready
#define FDD_CM				0x0040		// DDAM or DAM detected
#define FDD_DATACRC			0x0020		// Data CRC
#define FDD_NOCYL			0x0010		// Cylinder not found
#define FDD_SCANEQ			0x0008		// SCAN equal
#define FDD_SCANNOT			0x0004		// SCAN not equal
#define FDD_BADCYL			0x0002		// Cylinder ID error
#define FDD_MDAM			0x0001		// DAM not found

//---------------------------------------------------------------------------
//
//	Drive status definition
//
//---------------------------------------------------------------------------
#define FDST_INSERT			0x80		// Inserted (or ejected)
#define FDST_INVALID		0x40		// Invalid
#define FDST_EJECT			0x20		// Eject possible
#define FDST_BLINK			0x10		// Blink
#define FDST_CURRENT		0x08		// Current blink status
#define FDST_MOTOR			0x04		// Motor rotation
#define FDST_SELECT			0x02		// Selected
#define FDST_ACCESS			0x01		// Access LED

//===========================================================================
//
//	FDD
//
//===========================================================================
class FDD : public Device
{
public:
	// Drive data definition
	typedef struct {
		FDI *fdi;						// Floppy disk image
		FDI *next;						// Next image to be inserted
		BOOL seeking;					// Seeking
		int cylinder;					// Cylinder
		BOOL insert;					// Inserted
		BOOL invalid;					// Invalid
		BOOL eject;					// Eject possible
		BOOL blink;					// Blinking while not inserted
		BOOL access;					// Access LED
	} drv_t;

	// Local data definition
	typedef struct {
		BOOL motor;					// Motor flag
		BOOL settle;					// Settling time
		BOOL force;					// Force ready flag
		int selected;					// Selected drive
		BOOL first;					// First seek after motor ON
		BOOL hd;					// HD flag

		BOOL fast;					// Fast mode
	} fdd_t;

public:
	// Basic constructor
	FDD(VM *p);
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
										// Apply config

	// External API
	void FASTCALL GetDrive(int drive, drv_t *buffer) const;
										// Get drive structure
	void FASTCALL GetFDD(fdd_t *buffer) const;
										// Get local structure
	FDI* FASTCALL GetFDI(int drive);
										// Get FDI
	BOOL FASTCALL Callback(Event *ev);
										// Event callback
	void FASTCALL ForceReady(BOOL flag);
										// Force ready
	DWORD FASTCALL GetRotationPos() const;
										// Get rotation position
	DWORD FASTCALL GetRotationTime() const;
										// Get rotation time
	DWORD FASTCALL GetSearch();
										// Get search time
	void FASTCALL SetHD(BOOL hd);
										// Set HD flag
	BOOL FASTCALL IsHD() const;
										// Get HD flag
	void FASTCALL Access(BOOL flag);
										// Set access LED

	// Drive control
	BOOL FASTCALL Open(int drive, const Filepath& path, int media = 0);
										// Image open
	void FASTCALL Insert(int drive);
										// Media insert
	void FASTCALL Eject(int drive, BOOL force);
										// Eject
	void FASTCALL Invalid(int drive);
										// Invalid
	void FASTCALL Control(int drive, DWORD func);
										// Drive control
	BOOL FASTCALL IsReady(int drive, BOOL fdc = TRUE) const;
										// Ready check
	BOOL FASTCALL IsWriteP(int drive) const;
										// Write protect check
	BOOL FASTCALL IsReadOnly(int drive) const;
										// Read Only check
	void FASTCALL WriteP(int drive, BOOL flag);
										// Write protect setting
	int FASTCALL GetStatus(int drive) const;
										// Get drive status
	void FASTCALL SetMotor(int drive, BOOL flag);
										// Motor setting + drive select
	int FASTCALL GetCylinder(int drive) const;
										// Get cylinder
	void FASTCALL GetName(int drive, char *buf, int media = -1) const;
										// Get disk name
	void FASTCALL GetPath(int drive, Filepath& path) const;
										// Get path
	int FASTCALL GetDisks(int drive) const;
										// Get disk count
	int FASTCALL GetMedia(int drive) const;
										// Get media type

	// Seek
	void FASTCALL Recalibrate(DWORD srt);
										// Restore to track 0
	void FASTCALL StepIn(int step, DWORD srt);
										// Step in
	void FASTCALL StepOut(int step, DWORD srt);
										// Step out

	// Read/Write
	int FASTCALL ReadID(DWORD *buf, BOOL mfm, int hd);
										// Read ID
	int FASTCALL ReadSector(BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd);
										// Read sector
	int FASTCALL WriteSector(const BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd, BOOL deleted);
										// Write sector
	int FASTCALL ReadDiag(BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd);
										// Read diagonal
	int FASTCALL WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl);
										// Write ID

private:
	void FASTCALL SeekInOut(int cylinder, DWORD srt);
										// Seek processing
	void FASTCALL Rotation();
										// Motor rotation
	FDC *fdc;
										// FDC
	IOSC *iosc;
										// IOSC
	RTC *rtc;
										// RTC
	drv_t drv[4];
										// Drive data
	fdd_t fdd;
										// Local data
	Event eject;
										// Eject event
	Event seek;
										// Seek event
	Event rotation;
										// Rotation event
};

#endif	// fdd_h
