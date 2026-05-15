//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Floppy Disk Image ]
//
//---------------------------------------------------------------------------

#if !defined(fdi_h)
#define fdi_h

#include "filepath.h"

//---------------------------------------------------------------------------
//
//	Forward class declarations
//
//---------------------------------------------------------------------------
class FDI;
class FDIDisk;
class FDITrack;
class FDISector;

class FDIDisk2HD;
class FDITrack2HD;
class FDIDiskDIM;
class FDITrackDIM;
class FDIDiskD68;
class FDITrackD68;
class FDIDiskBAD;
class FDITrackBAD;
class FDIDisk2DD;
class FDITrack2DD;
class FDIDisk2HQ;
class FDITrack2HQ;

//---------------------------------------------------------------------------
//
//	Physical format definitions
//
//---------------------------------------------------------------------------
#define FDI_2HD			0x00			// 2HD
#define FDI_2HDA		0x01			// 2HDA
#define FDI_2HS			0x02			// 2HS
#define FDI_2HC			0x03			// 2HC
#define FDI_2HDE		0x04			// 2HDE
#define FDI_2HQ			0x05			// 2HQ
#define FDI_N88B		0x06			// N88-BASIC
#define FDI_OS9			0x07			// OS-9/68000
#define FDI_2DD			0x08			// 2DD

//===========================================================================
//
//	FDI Sector
//
//===========================================================================
class FDISector
{
public:
	// Internal data definition
	typedef struct {
		DWORD chrn[4];					// CHRN
		BOOL mfm;						// MFM flag
		int error;						// Error code
		int length;						// Data length
		int gap3;						// GAP3
		BYTE *buffer;					// Data buffer
		DWORD pos;						// Position
		BOOL changed;					// Modified flag
		FDISector *next;				// Next sector
	} sector_t;

public:
	// Basic functions
	FDISector(BOOL mfm, const DWORD *chrn);
										// Constructor
	virtual ~FDISector();
										// Destructor
	void FASTCALL Load(const BYTE *buf, int len, int gap, int err);
										// Initial load

	// Read/Write
	BOOL FASTCALL IsMatch(BOOL mfm, const DWORD *chrn) const;
										// Check if sector matches
	void FASTCALL GetCHRN(DWORD *chrn)	const;
										// Get CHRN
	BOOL FASTCALL IsMFM() const			{ return sec.mfm; }
										// Is MFM
	int FASTCALL Read(BYTE *buf) const;
										// Read
	int FASTCALL Write(const BYTE *buf, BOOL deleted);
										// Write
	int FASTCALL Fill(DWORD d);
										// Fill

	// Properties
	const BYTE* FASTCALL GetSector() const	{ return sec.buffer; }
										// Get sector data
	int FASTCALL GetLength() const		{ return sec.length; }
										// Get data length
	int FASTCALL GetError() const		{ return sec.error; }
										// Get error code
	int FASTCALL GetGAP3() const		{ return sec.gap3; }
										// Get GAP3 byte count

	// Position
	void FASTCALL SetPos(DWORD pos)		{  sec.pos = pos; }
										// Set position
	DWORD FASTCALL GetPos() const		{ return sec.pos; }
										// Get position

	// Change flag
	BOOL FASTCALL IsChanged() const		{ return sec.changed; }
										// Check change flag
	void FASTCALL ClrChanged()			{ sec.changed = FALSE; }
										// Clear change flag
	void FASTCALL ForceChanged()		{ sec.changed = TRUE; }
										// Set change flag

	// Index/Link
	void FASTCALL SetNext(FDISector *next)	{ sec.next = next; }
										// Set next sector
	FDISector* FASTCALL GetNext() const	{ return sec.next; }
										// Get next sector

private:
	// Internal data
	sector_t sec;
										// SectorInternal data
};

//===========================================================================
//
//	FDI Track
//
//===========================================================================
class FDITrack
{
public:
	// Internal work
	typedef struct {
		FDIDisk *disk;					// Parent disk
		int track;						// Track
		BOOL init;						// Is loaded
		int sectors[3];					// Owned sector count(ALL/FM/MFM)
		BOOL hd;						// Density flag
		BOOL mfm;						// First sector MFM flag
		FDISector *first;				// First sector
		FDITrack *next;					// Next track
	} track_t;

public:
	// Basic functions
	FDITrack(FDIDisk *disk, int track, BOOL hd = TRUE);
										// Constructor
	virtual ~FDITrack();
										// Destructor
	virtual BOOL FASTCALL Save(Fileio *fio, DWORD offset);
										// Save
	virtual BOOL FASTCALL Save(const Filepath& path, DWORD offset);
										// Save
	void FASTCALL Create(DWORD phyfmt);
										// Physical format
	BOOL FASTCALL IsHD() const			{ return trk.hd; }
										// Get HD flag

	// Read/Write
	int FASTCALL ReadID(DWORD *buf, BOOL mfm);
										// ReadID
	int FASTCALL ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn);
										// ReadSector
	int FASTCALL WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, BOOL deleted);
										// WriteSector
	int FASTCALL ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn);
										// ReadDiag
	virtual int FASTCALL WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int gpl);
										// WriteID

	// Index/Link
	int FASTCALL GetTrack() const		{ return trk.track; }
										// Get this track
	void FASTCALL SetNext(FDITrack *p)	{ trk.next = p; }
										// Set next track
	FDITrack* FASTCALL GetNext() const	{ return trk.next; }
										// Get next track

	// Sector
	BOOL FASTCALL IsChanged() const;
										// Check change flag
	DWORD FASTCALL GetTotalLength() const;
										// Calculate total sector length
	void FASTCALL ForceChanged();
										// Force change
	FDISector* FASTCALL Search(BOOL mfm, const DWORD *chrn);
										// Search sector matching conditions
	FDISector* FASTCALL GetFirst() const{ return trk.first; }
										// Get first sector

protected:
	// Physical format
	void FASTCALL Create2HD(int lim);
										// Physical format(2HD)
	void FASTCALL Create2HS();
										// Physical format(2HS)
	void FASTCALL Create2HC();
										// Physical format(2HC)
	void FASTCALL Create2HDE();
										// Physical format(2HDE)
	void FASTCALL Create2HQ();
										// Physical format(2HQ)
	void FASTCALL CreateN88B();
										// Physical format(N88-BASIC)
	void FASTCALL CreateOS9();
										// Physical format(OS-9)
	void FASTCALL Create2DD();
										// Physical format(2DD)

	// Disk, rotation management
	FDIDisk* FASTCALL GetDisk() const	{ return trk.disk; }
										// Get disk
	BOOL FASTCALL IsMFM() const			{ return trk.mfm; }
										// Is first sector MFM
	int FASTCALL GetGAP1() const;
										// Get GAP1 length
	int FASTCALL GetTotal() const;
										// Get total length
	void FASTCALL CalcPos();
										// Calculate sector start position
	DWORD FASTCALL GetSize(FDISector *sector) const;
										// Get sector size (including ID, GAP3)
	FDISector* FASTCALL GetCurSector() const;
										// Get first sector after current position

	// Track
	BOOL IsInit() const					{ return trk.init; }

	// Sector
	void FASTCALL AddSector(FDISector *sector);
										// Add sector
	void FASTCALL ClrSector();
										// Delete all sectors
	int FASTCALL GetAllSectors() const	{ return trk. sectors[0]; }
										// Get sector count (All)
	int FASTCALL GetMFMSectors() const	{ return trk.sectors[1]; }
										// Get sector count (MFM)
	int FASTCALL GetFMSectors() const	{ return trk.sectors[2]; }
										// Get sector count (FM)

	// Diag
	int FASTCALL MakeGAP1(BYTE *buf, int offset) const;
										// Create GAP1
	int FASTCALL MakeSector(BYTE *buf, int offset, FDISector *sector) const;
										// Create sector data
	int FASTCALL MakeGAP4(BYTE *buf, int offset) const;
										// Create GAP4
	int FASTCALL MakeData(BYTE *buf, int offset, BYTE data, int length) const;
										// Create Diag data
	WORD FASTCALL CalcCRC(BYTE *buf, int length) const;
										// Calculate CRC
	static const WORD CRCTable[0x100];
										// CRC calculation table

	// Internal data
	track_t trk;
										// TrackInternal data
};

//===========================================================================
//
//	FDI Disk
//
//===========================================================================
class FDIDisk
{
public:
	// New option definition
	typedef struct {
		DWORD phyfmt;					// Physical format type
		BOOL logfmt;					// Logical format presence
		char name[60];					// Comment (DIM)/Disk name (D68)
	} option_t;

	// Internal data definition
	typedef struct {
		int index;						// Index
		FDI *fdi;						// Parent FDI
		DWORD id;						// ID
		BOOL writep;					// Write protected
		BOOL readonly;					// Read only
		char name[60];					// Disk name
		Filepath path;					// Path
		DWORD offset;					// File offset
		FDITrack *first;				// First track
		FDITrack *head[2];				// Track corresponding to head
		int search;						// Search time (1 revolution = 0x10000)
		FDIDisk *next;					// Next disk
	} disk_t;

public:
	FDIDisk(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDisk();
										// Destructor
	DWORD FASTCALL GetID() const		{ return disk.id; }
										// Get ID

	// Media operations
	virtual BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	virtual BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL GetName(char *buf) const;
										// Get disk name
	void FASTCALL GetPath(Filepath& path) const;
										// Get path
	BOOL FASTCALL IsWriteP() const		{ return disk.writep; }
										// Is write protected
	BOOL FASTCALL IsReadOnly() const	{ return disk.readonly; }
										// Is read-only disk image
	void FASTCALL WriteP(BOOL flag);
										// Set write protection
	virtual BOOL FASTCALL Flush();
										// Flush buffer

	// Access
	virtual void FASTCALL Seek(int c);
										// Seek
	int FASTCALL ReadID(DWORD *buf, BOOL mfm, int hd);
										// ReadID
	int FASTCALL ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd);
										// ReadSector
	int FASTCALL WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted);
										// WriteSector
	int FASTCALL ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd);
										// ReadDiag
	int FASTCALL WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl);
										// WriteID

	// Rotation management
	DWORD FASTCALL GetRotationPos() const;
										// Get rotation position
	DWORD FASTCALL GetRotationTime() const;
										// Get rotation time
	DWORD FASTCALL GetSearch() const	{ return disk.search; }
										// Get search length
	void FASTCALL SetSearch(DWORD len)	{ disk.search = len; }
										// Set search length
	void FASTCALL CalcSearch(DWORD pos);
										// Calculate search length
	BOOL FASTCALL IsHD() const;
										// Get drive HD status
	FDITrack* FASTCALL Search(int track) const;
										// Search track

	// Index/Link
	int FASTCALL GetIndex() const		{ return disk.index; }
										// Get index
	void FASTCALL SetNext(FDIDisk *p)	{ disk.next = p; }
										// Set next track
	FDIDisk* FASTCALL GetNext() const	{ return disk.next; }
										// Get next track

protected:
	// Logical format
	void FASTCALL Create2HD(BOOL flag2hd);
										// Logical format(2HD, 2HDA)
	static const BYTE IPL2HD[0x200];
										// IPL(2HD, 2HDA)
	void FASTCALL Create2HS();
										// Logical format(2HS)
	static const BYTE IPL2HS[0x800];
										// IPL(2HS)
	void FASTCALL Create2HC();
										// Logical format(2HC)
	static const BYTE IPL2HC[0x200];
										// IPL(2HC)
	void FASTCALL Create2HDE();
										// Logical format(2HDE)
	static const BYTE IPL2HDE[0x800];
										// IPL(2HDE)
	void FASTCALL Create2HQ();
										// Logical format(2HQ)
	static const BYTE IPL2HQ[0x200];
										// IPL(2HQ)
	void FASTCALL Create2DD();
										// Logical format(2DD)

	// Image
	FDI* FASTCALL GetFDI() const		{ return disk.fdi; }
										// Get parent image

	// Track
	void FASTCALL AddTrack(FDITrack *track);
										// Add track
	void FASTCALL ClrTrack();
										// Delete all tracks
	FDITrack* FASTCALL GetFirst() const	{ return disk.first; }
										// Get first track
	FDITrack* FASTCALL GetHead(int idx) { ASSERT((idx==0)||(idx==1)); return disk.head[idx]; }
										// Get track corresponding to head

	// Internal data
	disk_t disk;
										// DiskInternal data
};

//===========================================================================
//
//	FDI
//
//===========================================================================
class FDI
{
public:
	// Internal data definition
	typedef struct {
		FDD *fdd;						// FDD
		int disks;						// Disk count
		FDIDisk *first;					// First disk
		FDIDisk *disk;					// Current disk
	} fdi_t;

public:
	FDI(FDD *fdd);
										// Constructor
	virtual ~FDI();
										// Destructor

	// Media operations
	BOOL FASTCALL Open(const Filepath& path, int media);
										// Open
	DWORD FASTCALL GetID() const;
										// Get ID
	BOOL FASTCALL IsMulti() const;
										// Is multi-disk image
	FDIDisk* GetDisk() const			{ return fdi.disk; }
										// Get current disk
	int FASTCALL GetDisks() const		{ return fdi.disks; }
										// Get disk count
	int FASTCALL GetMedia() const;
										// Get multi-disk index
	void FASTCALL GetName(char *buf, int index = -1) const;
										// Get disk name
	void FASTCALL GetPath(Filepath& path) const;
										// Get path
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// Save
	BOOL FASTCALL Load(Fileio *fio, int ver, BOOL *ready, int *media, Filepath& path);
										// Load
	void FASTCALL Adjust();
										// Adjust (special)

	// Media state
	BOOL FASTCALL IsReady() const;
										// Is media set
	BOOL FASTCALL IsWriteP() const;
										// Is write protected
	BOOL FASTCALL IsReadOnly() const;
										// Is read-only disk image
	void FASTCALL WriteP(BOOL flag);
										// Set write protection

	// Access
	void FASTCALL Seek(int c);
										// Seek
	int FASTCALL ReadID(DWORD *buf, BOOL mfm, int hd);
										// ReadID
	int FASTCALL ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd);
										// ReadSector
	int FASTCALL WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted);
										// WriteSector
	int FASTCALL ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd);
										// ReadDiag
	int FASTCALL WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl);
										// WriteID

	// Rotation management
	DWORD FASTCALL GetRotationPos() const;
										// Get rotation position
	DWORD FASTCALL GetRotationTime() const;
										// Get rotation time
	DWORD FASTCALL GetSearch() const;
										// Get search time
	BOOL FASTCALL IsHD() const;
										// Get drive HD status

private:
	// Drive
	FDD* FASTCALL GetFDD() const		{ return fdi.fdd; }

	// Disk
	void FASTCALL AddDisk(FDIDisk *disk);
										// Add disk
	void FASTCALL ClrDisk();
										// Delete all disks
	FDIDisk* GetFirst() const			{ return fdi.first; }
										// Get first disk
	FDIDisk* FASTCALL Search(int index) const;
										// Search disk

	// Internal data
	fdi_t fdi;
										// FDIInternal data
};

//===========================================================================
//
//	FDI Track(2HD)
//
//===========================================================================
class FDITrack2HD : public FDITrack
{
public:
	// Basic functions
	FDITrack2HD(FDIDisk *disk, int track);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset);
										// Load
};

//===========================================================================
//
//	FDI Disk(2HD)
//
//===========================================================================
class FDIDisk2HD : public FDIDisk
{
public:
	FDIDisk2HD(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDisk2HD();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	BOOL FASTCALL Flush();
										// Flush buffer
};

//===========================================================================
//
//	FDI Track(DIM)
//
//===========================================================================
class FDITrackDIM : public FDITrack
{
public:
	// Basic functions
	FDITrackDIM(FDIDisk *disk, int track, int type);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset, BOOL load);
										// Load
	BOOL FASTCALL IsDIMMFM() const		{ return dim_mfm; }
										// Get DIM MFM flag
	int FASTCALL GetDIMSectors() const	{ return dim_secs; }
										// Get DIM sector count
	int FASTCALL GetDIMN() const		{ return dim_n; }
										// Get DIM length

private:
	BOOL dim_mfm;
										// DIM MFM flag
	int dim_secs;
										// DIM sector count
	int dim_n;
										// DIM length
	int dim_type;
										// DIM type
};

//===========================================================================
//
//	FDI Disk(DIM)
//
//===========================================================================
class FDIDiskDIM : public FDIDisk
{
public:
	FDIDiskDIM(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDiskDIM();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	BOOL FASTCALL Flush();
										// Flush buffer

private:
	BOOL FASTCALL GetDIMMap(int track) const;
										// Get track map
	DWORD FASTCALL GetDIMOffset(int track) const;
										// Get track offset
	BOOL FASTCALL Save();
										// Save before deletion
	BYTE dim_hdr[0x100];
										// DIM header
	BOOL dim_load;
										// Header verification flag
};

//===========================================================================
//
//	FDI Track(D68)
//
//===========================================================================
class FDITrackD68 : public FDITrack
{
public:
	// Basic functions
	FDITrackD68(FDIDisk *disk, int track, BOOL hd);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset);
										// Load
	BOOL FASTCALL Save(const Filepath& path, DWORD offset);
										// Save
	BOOL FASTCALL Save(Fileio *fio, DWORD offset);
										// Save
	int FASTCALL WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int gpl);
										// WriteID
	void FASTCALL ForceFormat()			{ d68_format = TRUE; }
										// Force format
	BOOL FASTCALL IsFormated() const	{ return d68_format; }
										// Is format changed
	DWORD FASTCALL GetD68Length() const;
										// Get length in D68 format

private:
	BOOL d68_format;
										// Format change flag
	static const int Gap3Table[];
										// GAP3 table
};

//===========================================================================
//
//	FDI Disk(D68)
//
//===========================================================================
class FDIDiskD68 : public FDIDisk
{
public:
	FDIDiskD68(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDiskD68();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	void FASTCALL AdjustOffset();
										// Update offset
	static int FASTCALL CheckDisks(const Filepath& path, DWORD *offbuf);
										// D68 header check
	BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	BOOL FASTCALL Flush();
										// Flush buffer

private:
	DWORD FASTCALL GetD68Offset(int track) const;
										// Get track offset
	BOOL FASTCALL Save();
										// Save before deletion
	BYTE d68_hdr[0x2b0];
										// D68 header
	BOOL d68_load;
										// Header verification flag
};

//===========================================================================
//
//	FDI Track(BAD)
//
//===========================================================================
class FDITrackBAD : public FDITrack
{
public:
	// Basic functions
	FDITrackBAD(FDIDisk *disk, int track);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset);
										// Load
	BOOL FASTCALL Save(const Filepath& path, DWORD offset);
										// Save

private:
	int bad_secs;
										// Sector count
};

//===========================================================================
//
//	FDI Disk(BAD)
//
//===========================================================================
class FDIDiskBAD : public FDIDisk
{
public:
	FDIDiskBAD(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDiskBAD();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	BOOL FASTCALL Flush();
										// Flush buffer
};

//===========================================================================
//
//	FDI Track(2DD)
//
//===========================================================================
class FDITrack2DD : public FDITrack
{
public:
	// Basic functions
	FDITrack2DD(FDIDisk *disk, int track);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset);
										// Load
};

//===========================================================================
//
//	FDI Disk(2DD)
//
//===========================================================================
class FDIDisk2DD : public FDIDisk
{
public:
	FDIDisk2DD(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDisk2DD();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	BOOL FASTCALL Flush();
										// Flush buffer
};

//===========================================================================
//
//	FDI Track(2HQ)
//
//===========================================================================
class FDITrack2HQ : public FDITrack
{
public:
	// Basic functions
	FDITrack2HQ(FDIDisk *disk, int track);
										// Constructor
	BOOL FASTCALL Load(const Filepath& path, DWORD offset);
										// Load
};

//===========================================================================
//
//	FDI Disk(2HQ)
//
//===========================================================================
class FDIDisk2HQ : public FDIDisk
{
public:
	FDIDisk2HQ(int index, FDI *fdi);
										// Constructor
	virtual ~FDIDisk2HQ();
										// Destructor
	BOOL FASTCALL Open(const Filepath& path, DWORD offset = 0);
										// Open
	void FASTCALL Seek(int c);
										// Seek
	BOOL FASTCALL Create(const Filepath& path, const option_t *opt);
										// Create new disk
	BOOL FASTCALL Flush();
										// Flush buffer
};

#endif	// FDI_h
