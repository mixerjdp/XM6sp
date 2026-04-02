//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ Disk ]
//
//---------------------------------------------------------------------------

#if !defined(disk_h)
#define disk_h

//---------------------------------------------------------------------------
//
//	Class forward declaration
//
//---------------------------------------------------------------------------
class DiskTrack;
class DiskCache;
class Disk;
class SASIHD;
class SCSIHD;
class SCSIMO;
class SCSICDTrack;
class SCSICD;

//---------------------------------------------------------------------------
//
//	Error definition (returned by REQUEST SENSE)
//
//	MSB		Reserved(0x00)
//			Device type
//			Additional Sense Code (ASC)
//	LSB		Additional Sense Code Qualifier (ASCQ)
//
//---------------------------------------------------------------------------
#define DISK_NOERROR		0x00000000	// NO ADDITIONAL SENSE INFO.
#define DISK_DEVRESET		0x00062900	// POWER ON OR RESET OCCURED
#define DISK_NOTREADY		0x00023a00	// MEDIUM NOT PRESENT
#define DISK_ATTENTION		0x00062800	// MEDIUIM MAY HAVE CHANGED
#define DISK_PREVENT		0x00045302	// MEDIUM REMOVAL PREVENTED
#define DISK_READFAULT		0x00031100	// UNRECOVERED READ ERROR
#define DISK_WRITEFAULT		0x00030300	// PERIPHERAL DEVICE WRITE FAULT
#define DISK_WRITEPROTECT	0x00042700	// WRITE PROTECTED
#define DISK_MISCOMPARE		0x000e1d00	// MISCOMPARE DURING VERIFY
#define DISK_INVALIDCMD		0x00052000	// INVALID COMMAND OPERATION CODE
#define DISK_INVALIDLBA		0x00052100	// LOGICAL BLOCK ADDR. OUT OF RANGE
#define DISK_INVALIDCDB		0x00052400	// INVALID FIELD IN CDB
#define DISK_INVALIDLUN		0x00052500	// LOGICAL UNIT NOT SUPPORTED
#define DISK_INVALIDPRM		0x00052600	// INVALID FIELD IN PARAMETER LIST
#define DISK_INVALIDMSG		0x00054900	// INVALID MESSAGE ERROR
#define DISK_PARAMLEN		0x00051a00	// PARAMETERS LIST LENGTH ERROR
#define DISK_PARAMNOT		0x00052601	// PARAMETERS NOT SUPPORTED
#define DISK_PARAMVALUE		0x00052602	// PARAMETERS VALUE INVALID
#define DISK_PARAMSAVE		0x00053900	// SAVING PARAMETERS NOT SUPPORTED

#if 0
#define DISK_AUDIOPROGRESS	0x00??0011	// AUDIO PLAY IN PROGRESS
#define DISK_AUDIOPAUSED	0x00??0012	// AUDIO PLAY PAUSED
#define DISK_AUDIOSTOPPED	0x00??0014	// AUDIO PLAY STOPPED DUE TO ERROR
#define DISK_AUDIOCOMPLETE	0x00??0013	// AUDIO PLAY SUCCESSFULLY COMPLETED
#endif

//===========================================================================
//
//	Disk track
//
//===========================================================================
class DiskTrack
{
public:
	// Member data definition
	typedef struct {
		int track;						// Track number
		int size;						// Sector size (8 or 9)
		int sectors;					// Number of sectors (<=0x100)
		BYTE *buffer;					// Data buffer
		BOOL init;						// Loaded flag
		BOOL changed;					// Changed flag
		BOOL *changemap;				// Change map
		BOOL raw;						// RAW mode
	} disktrk_t;

public:
	// Basic constructor
	DiskTrack(int track, int size, int sectors, BOOL raw = FALSE);
										// Constructor
	virtual ~DiskTrack();
										// Destructor
	BOOL FASTCALL Load(const Filepath& path);
										// Load
	BOOL FASTCALL Save(const Filepath& path);
										// Save

	// Read/Write
	BOOL FASTCALL Read(BYTE *buf, int sec) const;
										// Sector read
	BOOL FASTCALL Write(const BYTE *buf, int sec);
										// Sector write

	// Others
	int FASTCALL GetTrack() const		{ return dt.track; }
										// Get track
	BOOL FASTCALL IsChanged() const		{ return dt.changed; }
										// Check changed flag

private:
	// Member data
	disktrk_t dt;
										// Member data
};

//===========================================================================
//
//	Disk cache
//
//===========================================================================
class DiskCache
{
public:
	// Member data definition
	typedef struct {
		DiskTrack *disktrk;				// Attached track
		DWORD serial;					// Last access serial
	} cache_t;

	// Cache constants
	enum {
		CacheMax = 16					// Maximum cache track count
	};

public:
	// Basic constructor
	DiskCache(const Filepath& path, int size, int blocks);
										// Constructor
	virtual ~DiskCache();
										// Destructor
	void FASTCALL SetRawMode(BOOL raw);
										// Set CD-ROM raw mode

	// Access
	BOOL FASTCALL Save();
										// Save all
	BOOL FASTCALL Read(BYTE *buf, int block);
										// Sector read
	BOOL FASTCALL Write(const BYTE *buf, int block);
										// Sector write
	BOOL FASTCALL GetCache(int index, int& track, DWORD& serial) const;
										// Get cache

private:
	// Cache management
	void FASTCALL Clear();
										// Clear all tracks
	DiskTrack* FASTCALL Assign(int track);
										// Assign track
	BOOL FASTCALL Load(int index, int track);
										// Load track
	void FASTCALL Update();
										// Update serial number

	// Member data
	cache_t cache[CacheMax];
										// Cache management
	DWORD serial;
										// Last access serial number
	Filepath sec_path;
										// Path
	int sec_size;
										// Sector size (8 or 9 or 11)
	int sec_blocks;
										// Sector blocks
	BOOL cd_raw;
										// CD-ROM RAW mode
};

//===========================================================================
//
//	Disk
//
//===========================================================================
class Disk
{
public:
	// Member structure
	typedef struct {
		DWORD id;						// Device ID
		BOOL ready;						// Ready disk
		BOOL writep;					// Write protect
		BOOL readonly;					// Read only
		BOOL removable;					// Removable
		BOOL lock;						// Lock
		BOOL attn;						// Attention
		BOOL reset;						// Reset
		int size;						// Sector size
		int blocks;						// Number of sectors
		DWORD lun;						// LUN
		DWORD code;						// Status code
		DiskCache *dcache;				// Disk cache
	} disk_t;

public:
	// Basic constructor
	Disk(Device *dev);
										// Constructor
	virtual ~Disk();
										// Destructor
	virtual void FASTCALL Reset();
										// Device reset
	virtual BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	virtual BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

	// ID
	DWORD FASTCALL GetID() const		{ return disk.id; }
										// Get device ID
	BOOL FASTCALL IsNULL() const;
										// NULL check
	BOOL FASTCALL IsSASI() const;
										// SASI check

	// Device control
	virtual BOOL FASTCALL Open(const Filepath& path);
										// Open
	void FASTCALL GetPath(Filepath& path) const;
										// Get path
	void FASTCALL Eject(BOOL force);
										// Eject
	BOOL FASTCALL IsReady() const		{ return disk.ready; }
										// Ready check
	void FASTCALL WriteP(BOOL flag);
										// Write protect
	BOOL FASTCALL IsWriteP() const		{ return disk.writep; }
										// Write protect check
	BOOL FASTCALL IsReadOnly() const	{ return disk.readonly; }
										// Read Only check
	BOOL FASTCALL IsRemovable() const	{ return disk.removable; }
										// Removable check
	BOOL FASTCALL IsLocked() const		{ return disk.lock; }
										// Lock check
	BOOL FASTCALL IsAttn() const		{ return disk.attn; }
										// Attn check
	BOOL FASTCALL Flush();
										// Flush cache
	void FASTCALL GetDisk(disk_t *buffer) const;
										// Get member structure

	// Properties
	void FASTCALL SetLUN(DWORD lun)		{ disk.lun = lun; }
										// Set LUN
	DWORD FASTCALL GetLUN()				{ return disk.lun; }
										// Get LUN

	// Commands
	virtual int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf);
										// INQUIRY command
	virtual int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);
										// REQUEST SENSE command
	int FASTCALL SelectCheck(const DWORD *cdb);
										// SELECT check
	BOOL FASTCALL ModeSelect(const BYTE *buf, int size);
										// MODE SELECT command
	int FASTCALL ModeSense(const DWORD *cdb, BYTE *buf);
										// MODE SENSE command
	BOOL FASTCALL TestUnitReady(const DWORD *cdb);
										// TEST UNIT READY command
	BOOL FASTCALL Rezero(const DWORD *cdb);
										// REZERO command
	BOOL FASTCALL Format(const DWORD *cdb);
										// FORMAT UNIT command
	BOOL FASTCALL Reassign(const DWORD *cdb);
										// REASSIGN command
	virtual int FASTCALL Read(BYTE *buf, int block);
										// READ command
	int FASTCALL WriteCheck(int block);
										// WRITE check
	BOOL FASTCALL Write(const BYTE *buf, int block);
										// WRITE command
	BOOL FASTCALL Seek(const DWORD *cdb);
										// SEEK command
	BOOL FASTCALL StartStop(const DWORD *cdb);
										// START STOP UNIT command
	BOOL FASTCALL SendDiag(const DWORD *cdb);
										// SEND DIAGNOSTIC command
	BOOL FASTCALL Removal(const DWORD *cdb);
										// PREVENT/ALLOW MEDIUM REMOVAL command
	int FASTCALL ReadCapacity(const DWORD *cdb, BYTE *buf);
										// READ CAPACITY command
	BOOL FASTCALL Verify(const DWORD *cdb);
										// VERIFY command
	virtual int FASTCALL ReadToc(const DWORD *cdb, BYTE *buf);
										// READ TOC command
	virtual BOOL FASTCALL PlayAudio(const DWORD *cdb);
										// PLAY AUDIO command
	virtual BOOL FASTCALL PlayAudioMSF(const DWORD *cdb);
										// PLAY AUDIO MSF command
	virtual BOOL FASTCALL PlayAudioTrack(const DWORD *cdb);
										// PLAY AUDIO TRACK command
	void FASTCALL InvalidCmd()			{ disk.code = DISK_INVALIDCMD; }
										// Unsupported command

protected:
	// Subroutines
	int FASTCALL AddError(BOOL change, BYTE *buf);
										// Add error message
	int FASTCALL AddFormat(BOOL change, BYTE *buf);
										// Add format page
	int FASTCALL AddOpt(BOOL change, BYTE *buf);
										// Add optical page
	int FASTCALL AddCache(BOOL change, BYTE *buf);
										// Add cache page
	int FASTCALL AddCDROM(BOOL change, BYTE *buf);
										// Add CD-ROM page
	int FASTCALL AddCDDA(BOOL change, BYTE *buf);
										// Add CD-DA page
	BOOL FASTCALL CheckReady();
										// Disk check

	// Member data
	disk_t disk;
										// Disk member data
	Device *ctrl;
										// Controller device
	Filepath diskpath;
										// Path (for GetPath)
};

//===========================================================================
//
//	SASI hard disk
//
//===========================================================================
class SASIHD : public Disk
{
public:
	// Basic constructor
	SASIHD(Device *dev);
										// Constructor
	BOOL FASTCALL Open(const Filepath& path);
										// Open

	// Device control
	void FASTCALL Reset();
										// Device reset

	// Commands
	int FASTCALL RequestSense(const DWORD *cdb, BYTE *buf);
										// REQUEST SENSE command
};

//===========================================================================
//
//	SCSI hard disk
//
//===========================================================================
class SCSIHD : public Disk
{
public:
	// Basic constructor
	SCSIHD(Device *dev);
										// Constructor
	BOOL FASTCALL Open(const Filepath& path);
										// Open

	// Commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf);
										// INQUIRY command
};

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================
class SCSIMO : public Disk
{
public:
	// Basic constructor
	SCSIMO(Device *dev);
										// Constructor
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

	// Commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf);
										// INQUIRY command
};

//===========================================================================
//
//	CD-ROM track
//
//===========================================================================
class CDTrack
{
public:
	// Basic constructor
	CDTrack(SCSICD *scsicd);
										// Constructor
	virtual ~CDTrack();
										// Destructor
	BOOL FASTCALL Init(int track, DWORD first, DWORD last);
										// Initialize

	// Properties
	void FASTCALL SetPath(BOOL cdda, const Filepath& path);
										// Set path
	void FASTCALL GetPath(Filepath& path) const;
										// Get path
	void FASTCALL AddIndex(int index, DWORD lba);
										// Add index
	DWORD FASTCALL GetFirst() const;
										// Get first LBA
	DWORD FASTCALL GetLast() const;
										// Get last LBA
	DWORD FASTCALL GetBlocks() const;
										// Get blocks
	int FASTCALL GetTrackNo() const;
										// Get track number
	BOOL FASTCALL IsValid(DWORD lba) const;
										// Valid LBA
	BOOL FASTCALL IsAudio() const;
										// Audio track

private:
	SCSICD *cdrom;
										// Parent device
	BOOL valid;
										// Valid track
	int track_no;
										// Track number
	DWORD first_lba;
										// First LBA
	DWORD last_lba;
										// Last LBA
	BOOL audio;
										// Audio track flag
	BOOL raw;
										// RAW data flag
	Filepath imgpath;
										// Image file path
};

//===========================================================================
//
//	CD-DA buffer
//
//===========================================================================
class CDDABuf
{
public:
	// Basic constructor
	CDDABuf();
										// Constructor
	virtual ~CDDABuf();
										// Destructor
#if 0
	BOOL Init();
										// Initialize
	BOOL FASTCALL Load(const Filepath& path);
										// Load
	BOOL FASTCALL Save(const Filepath& path);
										// Save

	// API
	void FASTCALL Clear();
										// Clear buffer
	BOOL FASTCALL Open(Filepath& path);
										// File open
	BOOL FASTCALL GetBuf(DWORD *buffer, int frames);
										// Get buffer
	BOOL FASTCALL IsValid();
										// Valid check
	BOOL FASTCALL ReadReq();
										// Read request
	BOOL FASTCALL IsEnd() const;
										// End check

private:
	Filepath wavepath;
										// Wave path
	BOOL valid;
										// Opened
	DWORD *buf;
										// Data buffer
	DWORD read;
										// Read pointer
	DWORD write;
										// Write pointer
	DWORD num;
										// Data count
	DWORD rest;
										// File rest size
#endif
};

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================
class SCSICD : public Disk
{
public:
	// Track constants
	enum {
		TrackMax = 96					// Maximum tracks
	};

public:
	// Basic constructor
	SCSICD(Device *dev);
										// Constructor
	virtual ~SCSICD();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, BOOL attn = TRUE);
										// Open
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// Load

	// Commands
	int FASTCALL Inquiry(const DWORD *cdb, BYTE *buf);
										// INQUIRY command
	int FASTCALL Read(BYTE *buf, int block);
										// READ command
	int FASTCALL ReadToc(const DWORD *cdb, BYTE *buf);
										// READ TOC command
	BOOL FASTCALL PlayAudio(const DWORD *cdb);
										// PLAY AUDIO command
	BOOL FASTCALL PlayAudioMSF(const DWORD *cdb);
										// PLAY AUDIO MSF command
	BOOL FASTCALL PlayAudioTrack(const DWORD *cdb);
										// PLAY AUDIO TRACK command

	// CD-DA
	BOOL FASTCALL NextFrame();
										// Frame advance
	void FASTCALL GetBuf(DWORD *buffer, int samples, DWORD rate);
										// Get CD-DA buffer

	// LBA-MSF conversion
	void FASTCALL LBAtoMSF(DWORD lba, BYTE *msf) const;
										// LBA to MSF
	DWORD FASTCALL MSFtoLBA(const BYTE *msf) const;
										// MSF to LBA

private:
	// Open
	BOOL FASTCALL OpenCue(const Filepath& path);
										// Open (CUE)
	BOOL FASTCALL OpenIso(const Filepath& path);
										// Open (ISO)
	BOOL rawfile;
										// RAW flag

	// Track management
	void FASTCALL ClearTrack();
										// Clear tracks
	int FASTCALL SearchTrack(DWORD lba) const;
										// Search track
	CDTrack* track[TrackMax];
										// Track object
	int tracks;
										// Number of valid tracks
	int dataindex;
										// Current data track
	int audioindex;
										// Current audio track

	int frame;
										// Frame number

#if 0
	CDDABuf da_buf;
										// CD-DA buffer
	int da_num;
										// CD-DA track count
	int da_cur;
										// CD-DA current track
	int da_next;
										// CD-DA next track
	BOOL da_req;
										// CD-DA data request
#endif
};

#endif	// disk_h
