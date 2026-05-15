//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI (ytanaka@ipc-tokai.or.jp)
//	[ Floppy Disk Image ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "filepath.h"
#include "fileio.h"
#include "fdd.h"
#include "fdi.h"

//===========================================================================
//
//	FDI Sector
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDISector::FDISector(BOOL mfm, const DWORD *chrn)
{
	int i;

	ASSERT(chrn);

	// Store CHRN, MFM
	for (i=0; i<4; i++) {
		sec.chrn[i] = chrn[i];
	}
	sec.mfm = mfm;

	// Initialize other fields
	sec.error = FDD_NODATA;
	sec.length = 0;
	sec.gap3 = 0;
	sec.buffer = NULL;
	sec.pos = 0;
	sec.changed = FALSE;
	sec.next = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDISector::~FDISector()
{
	// Free memory
	if (sec.buffer) {
		delete[] sec.buffer;
		sec.buffer = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Initial load
//
//---------------------------------------------------------------------------
void FASTCALL FDISector::Load(const BYTE *buf, int len, int gap, int err)
{
	ASSERT(this);
	ASSERT(!sec.buffer);
	ASSERT(sec.length == 0);
	ASSERT(buf);
	ASSERT(len > 0);
	ASSERT(gap > 0);

	// Set length first
	sec.length = len;

	// Allocate buffer
	try {
		sec.buffer = new BYTE[len];
	}
	catch (...) {
		sec.buffer = NULL;
		sec.length = 0;
	}
	if (!sec.buffer) {
		sec.length = 0;
	}

	// Transfer
	memcpy(sec.buffer, buf, sec.length);

	// Work settings
	sec.gap3 = gap;
	sec.error = err;
	sec.changed = FALSE;
}

//---------------------------------------------------------------------------
//
//	Check if sector matches
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDISector::IsMatch(BOOL mfm, const DWORD *chrn) const
{
	int i;

	ASSERT(this);
	ASSERT(chrn);

	// Compare MFM
	if (sec.mfm != mfm) {
		return FALSE;
	}

	// Compare CHR
	for (i=0; i<3; i++) {
		if (chrn[i] != sec.chrn[i]) {
			return FALSE;
		}
	}

	// Compare N only if argument N is != 0
	if (chrn[3] != 0) {
		if (chrn[3] != sec.chrn[3]) {
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get CHRN
//
//---------------------------------------------------------------------------
void FASTCALL FDISector::GetCHRN(DWORD *chrn) const
{
	int i;

	ASSERT(this);
	ASSERT(chrn);

	for (i=0; i<4; i++) {
		chrn[i] = sec.chrn[i];
	}
}

//---------------------------------------------------------------------------
//
//	Read
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Read(BYTE *buf) const
{
	ASSERT(this);
	ASSERT(buf);

	// Do nothing if no sector buffer
	if (!sec.buffer) {
		return sec.error;
	}

	// Transfer and return error
	memcpy(buf, sec.buffer, sec.length);
	return sec.error;
}

//---------------------------------------------------------------------------
//
//	Write
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Write(const BYTE *buf, BOOL deleted)
{
	ASSERT(this);
	ASSERT(buf);

	// Do nothing if no sector buffer
	if (!sec.buffer) {
		return sec.error;
	}

	// Process errors first
	sec.error &= ~FDD_DATACRC;
	sec.error &= ~FDD_DDAM;
	if (deleted) {
		sec.error |= FDD_DDAM;
	}

	// Do nothing if they match
	if (memcmp(sec.buffer, buf, sec.length) == 0) {
		return sec.error;
	}

	// Transfer
	memcpy(sec.buffer, buf, sec.length);

	// Set update flag and return error
	sec.changed = TRUE;
	return sec.error;
}

//---------------------------------------------------------------------------
//
//	Fill
//
//---------------------------------------------------------------------------
int FASTCALL FDISector::Fill(DWORD d)
{
	int i;
	BOOL changed;

	ASSERT(this);

	// Do nothing if no sector buffer
	if (!sec.buffer) {
		return sec.error;
	}

	// Write while comparing
	changed = FALSE;
	for (i=0; i<sec.length; i++) {
		if (sec.buffer[i] != (BYTE)d) {
			// If different even once, fill and break
			memset(sec.buffer, d, sec.length);
			changed = TRUE;
			break;
		}
	}

	// Assume data CRC does not occur on write
	sec.error &= ~FDD_DATACRC;

	// Set update flag and return error
	if (changed) {
		sec.changed = TRUE;
	}
	return sec.error;
}

//===========================================================================
//
//	FDI Track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrack::FDITrack(FDIDisk *disk, int track, BOOL hd)
{
	ASSERT(disk);
	ASSERT((track >= 0) && (track <= 163));

	// Store disk, track
	trk.disk = disk;
	trk.track = track;

	// Other work area
	trk.init = FALSE;
	trk.sectors[0] = 0;
	trk.sectors[1] = 0;
	trk.sectors[2] = 0;
	trk.hd = hd;
	trk.mfm = TRUE;
	trk.first = NULL;
	trk.next = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDITrack::~FDITrack()
{
	// Clear
	ClrSector();
}

//---------------------------------------------------------------------------
//
//	Save
//	Note: For 2HD, DIM sector continuous write types
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::Save(const Filepath& path, DWORD offset)
{
	Fileio fio;
	FDISector *sector;
	BOOL changed;

	ASSERT(this);

	// No need to write if not initialized
	if (!IsInit()) {
		return TRUE;
	}

	// Check sectors for any that are written
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// Do nothing if none are written
	if (!changed) {
		return TRUE;
	}

	// Open file
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// Loop
	sector = GetFirst();
	while (sector) {
		// If not changed, go to next
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// Seek
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}

		// Write
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// Clear flag
		sector->ClrChanged();

		// Next
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// End
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//	Note: For 2HD, DIM sector continuous write types
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::Save(Fileio *fio, DWORD offset)
{
	FDISector *sector;
	BOOL changed;

	ASSERT(this);
	ASSERT(fio);

	// No need to write if not initialized
	if (!IsInit()) {
		return TRUE;
	}

	// Check sectors for any that are written
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// Do nothing if none are written
	if (!changed) {
		return TRUE;
	}

	// Loop
	sector = GetFirst();
	while (sector) {
		// If not changed, go to next
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// Seek
		if (!fio->Seek(offset)) {
			return FALSE;
		}

		// Write
		if (!fio->Write(sector->GetSector(), sector->GetLength())) {
			return FALSE;
		}

		// Clear flag
		sector->ClrChanged();

		// Next
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// End
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Physical format
//	Note: Save separately
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create(DWORD phyfmt)
{
	ASSERT(this);

	// Delete all sectors
	ClrSector();

	// By physical format type
	switch (phyfmt) {
		// Standard 2HD
		case FDI_2HD:
			Create2HD(153);
			break;

		// Over-track 2HD
		case FDI_2HDA:
			Create2HD(159);
			break;

		// 2HS
		case FDI_2HS:
			Create2HS();
			break;

		// 2HC
		case FDI_2HC:
			Create2HC();
			break;

		// 2HDE
		case FDI_2HDE:
			Create2HDE();
			break;

		// 2HQ
		case FDI_2HQ:
			Create2HQ();
			break;

		// N88-BASIC
		case FDI_N88B:
			CreateN88B();
			break;

		// OS-9/68000
		case FDI_OS9:
			CreateOS9();
			break;

		// 2DD
		case FDI_2DD:
			Create2DD();
			break;

		// Other
		default:
			ASSERT(FALSE);
			return;
	}

	// If sectors exist, initialize and mark all changed to force save later
	if (GetAllSectors() > 0) {
		trk.init = TRUE;
		ForceChanged();
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2HD, 2HDA)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HD(int lim)
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to specified number
	if (trk.track > lim) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x03;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 3x8 sectors (MFM)
	for (i=0; i<8; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x400, 0x74, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2HS)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HS()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 159
	if (trk.track > 159) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x03;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 3x9 sectors (MFM)
	for (i=0; i<9; i++) {
		// Create R (special case exists)
		if ((trk.track == 0) && (i == 0)) {
			chrn[2] = 0x01;
		}
		else {
			chrn[2] = 10 + i;
		}

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x400, 0x39, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2HC)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HC()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 159
	if (trk.track > 159) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 2x15 sectors (MFM)
	for (i=0; i<15; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2HDE)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HDE()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x400];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 159
	if (trk.track > 159) {
		return;
	}

	// C,NCreate
	chrn[0] = trk.track >> 1;
	chrn[3] = 0x03;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 3x9 sectors (MFM)
	for (i=0; i<9; i++) {
		// Create H (special case exists)
		chrn[1] = 0x80 + (trk.track & 1);
		if ((trk.track == 0) && (i == 0)) {
			chrn[1] = 0x00;
		}

		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x400, 0x39, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2HQ)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2HQ()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 159
	if (trk.track > 159) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 2x18 sectors (MFM)
	for (i=0; i<18; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(N88-BASIC)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CreateN88B()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x100];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 153
	if (trk.track > 153) {
		return;
	}

	// C,HCreate
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Track 0 is exception
	if (trk.track == 0) {
		// Length 0x26 sectors (FM)
		chrn[3] = 0;
		for (i=0; i<26; i++) {
			// Create R
			chrn[2] = i + 1;

			// Create sector
			sector = new FDISector(FALSE, chrn);

			// Load data
			sector->Load(buf, 0x80, 0x1a, FDD_NOERROR);

			// Add
			AddSector(sector);
		}
		return;
	}

	// Length 1x26 sectors (MFM)
	chrn[3] = 1;
	for (i=0; i<26; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x100, 0x33, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(OS-9/68000)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CreateOS9()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x100];

	ASSERT(this);
	ASSERT(trk.hd);

	// Track up to 153
	if (trk.track > 153) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 1;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 1x26 sectors (MFM)
	for (i=0; i<26; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x100, 0x33, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	Physical format(2DD)
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::Create2DD()
{
	int i;
	FDISector *sector;
	DWORD chrn[4];
	BYTE buf[0x200];

	ASSERT(this);
	ASSERT(!trk.hd);

	// Track up to 159
	if (trk.track > 159) {
		return;
	}

	// Create C, H, N
	chrn[0] = trk.track >> 1;
	chrn[1] = trk.track & 0x01;
	chrn[3] = 0x02;

	// Initialize buffer
	memset(buf, 0xe5, sizeof(buf));

	// Length 2x9 sectors (MFM)
	for (i=0; i<9; i++) {
		// Create R
		chrn[2] = i + 1;

		// Create sector
		sector = new FDISector(TRUE, chrn);

		// Load data
		sector->Load(buf, 0x200, 0x54, FDD_NOERROR);

		// Add
		AddSector(sector);
	}
}

//---------------------------------------------------------------------------
//
//	ReadID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or all valid sectors have ID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadID(DWORD *buf, BOOL mfm)
{
	FDISector *sector;
	DWORD pos;
	int status;
	int num;
	int match;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(trk.disk);

	// Initialize status
	status = FDD_NOERROR;

	// If HD flag does not match, process error
	if (GetDisk()->IsHD() != trk.hd) {
		status |= FDD_MAM;
		status |= FDD_NODATA;
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// Count sectors with matching density and no ID CRC
	num = 0;
	match = 0;
	sector = GetFirst();
	while (sector) {
		// Check if density matches
		if (sector->IsMFM() == mfm) {
			match++;

			// Check if no ID CRC error
			if (!(sector->GetError() & FDD_IDCRC)) {
				num++;
			}
		}
		sector = sector->GetNext();
	}

	// No data with matching density
	if (match == 0) {
		status |= FDD_MAM;
	}

	// No sectors without ID CRC
	if (num == 0) {
		status |= FDD_NODATA;
	}

	// Both failed. Search time is 2 index hole detections
	if (status != FDD_NOERROR) {
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// Get first sector after current position. Must match density
	sector = GetCurSector();
	ASSERT(sector);
	for (;;) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// Skip if density does not match
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// Skip if ID CRC
		if (sector->GetError() & FDD_IDCRC) {
			sector = sector->GetNext();
			continue;
		}

		// End
		break;
	}

	// Get CHRN
	sector->GetCHRN(buf);

	// Set search time
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// No error
	return status;
}

//---------------------------------------------------------------------------
//
//	Read sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;
	DWORD work[4];
	DWORD pos;
	int status;
	int i;
	int num;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// Get sector count with matching density
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// Force sector count to 0 if HD flag does not match
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// If 0, MAM, NODATA (no sectors)
	if (num == 0) {
		// Search time is 2 index hole detections
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// Get first sector after current position
	sector = GetCurSector();

	// Loop by Number only, sector search (only check R)
	status = FDD_NOERROR;
	num = GetAllSectors();
	for (i=0; i<num; i++) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// Repeat if density does not match
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// Get CHRN, check C
		sector->GetCHRN(work);
		if (work[0] != chrn[0]) {
			if (work[0] == 0xff) {
				status |= FDD_BADCYL;
			}
			else {
				status |= FDD_NOCYL;
			}
		}

		// Break on R match
		if (work[2] == chrn[2]) {
			break;
		}

		// Go to next sector
		sector = sector->GetNext();
	}

	// If R does not match, return NODATA
	if (work[2] != chrn[2]) {
		status |= FDD_NODATA;

		// Search time is 2 index hole detections
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// Set search time
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// Put data in buffer only if buf is specified. If NULL, status only
	*len = sector->GetLength();
	if (buf) {
		status = sector->Read(buf);
	}
	else {
		status = sector->GetError();
	}

	// Mask status
	status &= (FDD_IDCRC | FDD_DATACRC | FDD_DDAM);
	return status;
}

//---------------------------------------------------------------------------
//
//	Write sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or CHRN does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, BOOL deleted)
{
	FDISector *sector;
	DWORD work[4];
	DWORD pos;
	int status;
	int i;
	int num;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// Get sector count with matching density
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// Force sector count to 0 if HD flag does not match
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// If 0, MAM, NODATA (no sectors)
	if (num == 0) {
		// Search time is 2 index hole detections
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// Get first sector after current position
	sector = GetCurSector();

	// Loop by Number only, sector search (check CHRN)
	status = FDD_NOERROR;
	num = GetAllSectors();
	for (i=0; i<num; i++) {
		if (!sector) {
			sector = GetFirst();
		}
		ASSERT(sector);

		// Repeat if density does not match
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// Get CHRN, check C
		sector->GetCHRN(work);
		if (work[0] != chrn[0]) {
			if (work[0] == 0xff) {
				status |= FDD_BADCYL;
			}
			else {
				status |= FDD_NOCYL;
			}
		}

		// Break on CHRN match
		if (sector->IsMatch(mfm, chrn)) {
			break;
		}

		// Go to next sector
		sector = sector->GetNext();
	}

	// If sector not found, NODATA
	if (i >= num) {
		status |= FDD_NODATA;

		// Search time is 2 index hole detections
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return status;
	}

	// Set search time
	pos = sector->GetPos();
	GetDisk()->CalcSearch(pos);

	// Write only if buf is specified. If NULL, status only
	*len = sector->GetLength();
	if (buf) {
		status = sector->Write(buf, deleted);
	}
	else {
		status = sector->GetError();
	}

	// Mask status
	status &= (FDD_IDCRC | FDD_DDAM);
	return status;
}

//---------------------------------------------------------------------------
//
//	Read diag
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;
	DWORD work[4];
	int num;
	int total;
	int start;
	int length;
	int status;
	int error;
	BYTE *ptr;
	DWORD pos;
	BOOL found;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);

	// Get sector count with matching density
	if (mfm) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// Force sector count to 0 if HD flag does not match
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// If 0, MAM, NODATA (no sectors)
	if (num == 0) {
		// Search time is 2 index hole detections
		pos = GetDisk()->GetRotationTime();
		pos = (pos * 2) - GetDisk()->GetRotationPos();
		GetDisk()->SetSearch(pos);
		return FDD_NODATA | FDD_MAM;
	}

	// Allocate work
	try {
		ptr = new BYTE[0x4000];
	}
	catch (...) {
		return FDD_NODATA | FDD_MAM;
	}
	if (!ptr) {
		return FDD_NODATA | FDD_MAM;
	}

	// Determine length. Max N=7 (4000h)
	length = chrn[3];
	if (length > 7) {
		length = 7;
	}
	length = 1 << (length + 7);

	// Search time is first sector acquisition time
	sector = GetFirst();
	pos = sector->GetPos();
	GetDisk()->SetSearch(pos);

	// Initialize status
	status = FDD_NOERROR;

	// Create GAP1
	total = MakeGAP1(ptr, 0);

	// Loop
	found = FALSE;
	while (sector) {
		// If sector MFM does not match, continue
		if (sector->IsMFM() != mfm) {
			sector = sector->GetNext();
			continue;
		}

		// Create sector data
		total = MakeSector(ptr, total, sector);

		// Get CHRN
		sector->GetCHRN(work);

		// found only if both R and N match
		if (work[2] == chrn[2]) {
			if (work[3] == chrn[3]) {
				found = TRUE;
			}
		}

		// IDCRC, DATACRC, DDAM
		error = sector->GetError();
		error &= (FDD_IDCRC | FDD_DATACRC | FDD_DDAM);
		status |= error;

		// Next
		sector = sector->GetNext();
	}

	// Check result
	if (!found) {
		// (GLODEA type)
		status |= (FDD_NODATA | FDD_DATAERR);
	}

	// Create GAP4
	total = MakeGAP4(ptr, total);

	// End here if no buffer given
	if (!buf) {
		*len = 0;
		delete[] ptr;
		return status;
	}

	// Determine start position (skip to just before first sector data)
	if (mfm) {
		start = 60 + GetGAP1();
	}
	else {
		start = 31 + GetGAP1();
	}

	// If ends in one pass
	if (length <= (total - start)) {
		memcpy(buf, &ptr[start], length);
		*len = length;
		delete[] ptr;
		return status;
	}

	// Process first pass
	memcpy(buf, &ptr[start], (total - start));
	*len = (total - start);
	length -= (total - start);
	buf += (total - start);

	// Next loop
	while (length > 0) {
		if (length <= total) {
			// Fits
			memcpy(buf, ptr, length);
			*len += length;
			break;
		}
		// Put all
		memcpy(buf, ptr, total);
		*len += total;
		length -= total;
		buf += total;
	}

	// Free ptr
	delete[] ptr;

	// End
	return status;
}

//---------------------------------------------------------------------------
//
//	WriteID
//	Note: For 2HD, DIM fixed sector types
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTWRITE	Write protected
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int /*gpl*/)
{
	FDISector *sector;
	DWORD chrn[4];
	int num;
	int i;
	DWORD pos;

	ASSERT(this);
	ASSERT(sc > 0);

	// Get current sector
	if (IsMFM()) {
		num = GetMFMSectors();
	}
	else {
		num = GetFMSectors();
	}

	// Force sector count to 0 if HD flag does not match
	if (GetDisk()->IsHD() != trk.hd) {
		num = 0;
	}

	// Sector count must match
	if (num != sc) {
		return FDD_NOTWRITE;
	}

	// Single existence not allowed
	if (GetAllSectors() != num) {
		return FDD_NOTWRITE;
	}

	// Set time (until index)
	pos = GetDisk()->GetRotationTime();
	pos -= GetDisk()->GetRotationPos();
	GetDisk()->SetSearch(pos);

	// End here if no buf given
	if (!buf) {
		return FDD_NOERROR;
	}

	// CHRN must match without duplicates
	sector = GetFirst();
	while (sector) {
		// Check inside buf
		for (i=0; i<sc; i++) {
			chrn[0] = (DWORD)buf[i * 4 + 0];
			chrn[1] = (DWORD)buf[i * 4 + 1];
			chrn[2] = (DWORD)buf[i * 4 + 2];
			chrn[3] = (DWORD)buf[i * 4 + 3];
			if (sector->IsMatch(mfm, chrn)) {
				break;
			}
		}

		// No match found
		if (i >= sc) {
			ASSERT(i == sc);
			return FDD_NOTWRITE;
		}

		// Next
		sector = sector->GetNext();
	}

	// Write loop (fill all sectors)
	sector = GetFirst();
	while (sector) {
		sector->Fill(d);
		sector = sector->GetNext();
	}

	return FDD_NOERROR;
}

//---------------------------------------------------------------------------
//
//	Check for changes
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack::IsChanged() const
{
	BOOL changed;
	FDISector *sector;

	ASSERT(this);

	// Initialize
	changed = FALSE;
	sector = GetFirst();

	// Using OR
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	return changed;
}

//---------------------------------------------------------------------------
//
//	Calculate total sector length
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrack::GetTotalLength() const
{
	DWORD total;
	FDISector *sector;

	ASSERT(this);

	// Initialize
	total = 0;
	sector = GetFirst();

	// Loop
	while (sector) {
		total += sector->GetLength();
		sector = sector->GetNext();
	}

	return total;
}

//---------------------------------------------------------------------------
//
//	Force change
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::ForceChanged()
{
	FDISector *sector;

	ASSERT(this);

	// Initialize
	sector = GetFirst();

	// Loop
	while (sector) {
		sector->ForceChanged();
		sector = sector->GetNext();
	}
}

//---------------------------------------------------------------------------
//
//	Add sector
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::AddSector(FDISector *sector)
{
	FDISector *ptr;

	ASSERT(this);
	ASSERT(sector);

	// If no sectors, add as is
	if (!trk.first) {
		trk.first = sector;
		sector->SetNext(NULL);

		// Determine MFM
		trk.mfm = sector->IsMFM();
	}
	else {
		// Get last sector
		ptr = trk.first;
			while (ptr->GetNext()) {
			ptr = ptr->GetNext();
		}

		// Add to last sector
		ptr->SetNext(sector);
		sector->SetNext(NULL);
	}

	// Increment count
	trk.sectors[0]++;
	if (sector->IsMFM()) {
		trk.sectors[1]++;
	}
	else {
		trk.sectors[2]++;
	}
}

//---------------------------------------------------------------------------
//
//	Delete all sectors
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::ClrSector()
{
	FDISector *sector;

	ASSERT(this);

	// Delete all sectors
	while (trk.first) {
		sector = trk.first->GetNext();
		delete trk.first;
		trk.first = sector;
	}

	// Count 0
	trk.sectors[0] = 0;
	trk.sectors[1] = 0;
	trk.sectors[2] = 0;
}

//---------------------------------------------------------------------------
//
//	Search sector
//
//---------------------------------------------------------------------------
FDISector* FASTCALL FDITrack::Search(BOOL mfm, const DWORD *chrn)
{
	FDISector *sector;

	ASSERT(this);
	ASSERT(chrn);

	// Get first sector
	sector = GetFirst();

	// Loop
	while (sector) {
		// If match, return that sector
		if (sector->IsMatch(mfm, chrn)) {
			return sector;
		}

		sector = sector->GetNext();
	}

	// No match
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Get GAP1 length
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::GetGAP1() const
{
	ASSERT(this);

	if (IsMFM()) {
		// GAP4a 80 bytes, SYNC 12 bytes, IAM 4 bytes, GAP1 50 bytes
		return 146;
	}
	else {
		// GAP4a 40 bytes, SYNC 6 bytes, IAM 1 byte, GAP1 26 bytes
		return 73;
	}
}

//---------------------------------------------------------------------------
//
//	Get total length
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::GetTotal() const
{
	ASSERT(this);

	// 2DD is handled separately
	if (!trk.hd) {
		// Measured value on PC-9801BX4
		if (IsMFM()) {
			return 6034 + GetGAP1() + 60;
		}
		else {
			return 3016 + GetGAP1() + 31;
		}
	}

	// Measured value on XVI
	if (IsMFM()) {
		return 10193 + GetGAP1() + 60;
	}
	else {
		return 5095 + GetGAP1() + 31;
	}
}

//---------------------------------------------------------------------------
//
//	Calculate position of each sector start
//
//---------------------------------------------------------------------------
void FASTCALL FDITrack::CalcPos()
{
	FDISector *sector;
	DWORD total;
	DWORD prev;
	DWORD hus;
	FDISector *p;

	ASSERT(this);

	// Set first sector
	sector = GetFirst();

	// Loop
	while (sector) {
		// GAP1
		prev = GetGAP1();

		// Get size by iterating all sectors
		p = GetFirst();
		while (p) {
			if (p == sector) {
				break;
			}

			// Add to prev if not yet appeared
			prev += GetSize(p);
			p = p->GetNext();
		}

		// Add ID field and SYNC portion
		if (sector->IsMFM()) {
			prev += 60;
		}
		else {
			prev += 31;
		}

		// Get total value including GAP4
		total = GetTotal();

		// Calculate ratio of prev to total. Should equal GetDisk()->GetRotationTime() in 1 revolution
		if (prev >= total) {
			prev = total;
		}
		ASSERT(total <= 0x5000);
		hus = GetDisk()->GetRotationTime();
		prev >>= 1;
		total >>= 1;
		prev *= hus;
		prev /= total;
		if (prev >= hus) {
			prev = hus - 1;
		}

		// Store
		sector->SetPos(prev);

		// Next
		sector = sector->GetNext();
	}
}

//---------------------------------------------------------------------------
//
//	Get sector size
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrack::GetSize(FDISector *sector) const
{
	DWORD len;

	ASSERT(this);
	ASSERT(sector);

	// First, sector actual data area + CRC + GAP3
	len = sector->GetLength();
	len += 2;
	len += sector->GetGAP3();

	if (sector->IsMFM()) {
		// SYNC 12 bytes
		len += 12;

		// ID address mark, CHRN, CRC 2 bytes
		len += 10;

		// GAP2, SYNC, data mark
		len += 38;
	}
	else {
		// SYNC 6 bytes
		len += 6;

		// ID address mark, CHRN, CRC 2 bytes
		len += 7;

		// GAP2, SYNC, data mark
		len += 18;
	}

	return len;
}

//---------------------------------------------------------------------------
//
//	Get sector after current position
//
//---------------------------------------------------------------------------
FDISector* FASTCALL FDITrack::GetCurSector() const
{
	DWORD cur;
	DWORD pos;
	FDISector *sector;

	ASSERT(this);

	// Get current position
	cur = GetDisk()->GetRotationPos();

	// Get first sector
	sector = GetFirst();
	if (!sector) {
		return NULL;
	}

	// Check sectors from head, ok if above
	while (sector) {
		pos = sector->GetPos();
		if (pos >= cur) {
			return sector;
		}
		sector = sector->GetNext();
	}

	// Specified position exceeds last sector position, return to head
	return GetFirst();
}

//---------------------------------------------------------------------------
//
//	Create GAP1
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeGAP1(BYTE *buf, int offset) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);

	// Is MFM
	if (IsMFM()) {
		ASSERT(GetMFMSectors() > 0);

		// Create GAP1
		offset = MakeData(buf, offset, 0x4e, 80);
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xc2, 3);
		offset = MakeData(buf, offset, 0xfc, 1);
		offset = MakeData(buf, offset, 0x4e, 50);
		return offset;
	}

	// FM
	ASSERT(GetFMSectors() > 0);

	// Create GAP1
	offset = MakeData(buf, offset, 0xff, 40);
	offset = MakeData(buf, offset, 0x00, 6);
	offset = MakeData(buf, offset, 0xfc, 1);
	offset = MakeData(buf, offset, 0xff, 26);
	return offset;
}

//---------------------------------------------------------------------------
//
//	Create sector data
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeSector(BYTE *buf, int offset, FDISector *sector) const
{
	DWORD chrn[4];
	int i;
	WORD crc;
	const BYTE *ptr;
	int len;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);
	ASSERT(sector);

	// Get CHRN, sector data, length
	sector->GetCHRN(chrn);
	ptr = sector->GetSector();
	len = sector->GetLength();

	// Is MFM
	if (sector->IsMFM()) {
		// MFM (ID part)
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xa1, 3);
		offset = MakeData(buf, offset, 0xfe, 1);
		for (i=0; i<4; i++) {
			buf[offset + i] = (BYTE)chrn[i];
		}
		offset += 4;
		crc = CalcCRC(&buf[offset - 8], 8);
		buf[offset + 0] = (BYTE)(crc >> 8);
		buf[offset + 1] = (BYTE)crc;
		offset += 2;
		offset = MakeData(buf, offset, 0x4e, 22);

		// MFM (data part)
		offset = MakeData(buf, offset, 0x00, 12);
		offset = MakeData(buf, offset, 0xa1, 3);
		if (sector->GetError() & FDD_DDAM) {
			offset = MakeData(buf, offset, 0xf8, 1);
		}
		else {
			offset = MakeData(buf, offset, 0xfb, 1);
		}
		memcpy(&buf[offset], ptr, len);
		crc = CalcCRC(&buf[offset - 4], len + 4);
		offset += len;
		buf[offset + 0] = (BYTE)(crc >> 8);
		buf[offset + 1] = (BYTE)crc;
		offset += 2;
		offset = MakeData(buf, offset, 0x4e, sector->GetGAP3());
		return offset;
	}

	// FM (ID part)
	offset = MakeData(buf, offset, 0x00, 6);
	offset = MakeData(buf, offset, 0xfe, 1);
	for (i=0; i<4; i++) {
		buf[offset + i] = (BYTE)chrn[i];
	}
	offset += 4;
	crc = CalcCRC(&buf[offset - 5], 5);
	buf[offset + 0] = (BYTE)(crc >> 8);
	buf[offset + 1] = (BYTE)crc;
	offset += 2;
	offset = MakeData(buf, offset, 0xff, 11);

	// FM (data part)
	offset = MakeData(buf, offset, 0x00, 6);
	if (sector->GetError() & FDD_DDAM) {
		offset = MakeData(buf, offset, 0xf8, 1);
	}
	else {
		offset = MakeData(buf, offset, 0xfb, 1);
	}
	memcpy(&buf[offset], ptr, len);
	crc = CalcCRC(&buf[offset - 1], len + 1);
	offset += len;
	buf[offset + 0] = (BYTE)(crc >> 8);
	buf[offset + 1] = (BYTE)crc;
	offset += 2;
	offset = MakeData(buf, offset, 0xff, sector->GetGAP3());

	return offset;
}

//---------------------------------------------------------------------------
//
//	Create GAP4
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeGAP4(BYTE *buf, int offset) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);

	if (IsMFM()) {
		return MakeData(buf, offset, 0x4e, GetTotal() - offset);
	}
	else {
		return MakeData(buf, offset, 0xff, GetTotal() - offset);
	}
}

//---------------------------------------------------------------------------
//
//	Calculate CRC
//
//---------------------------------------------------------------------------
WORD FASTCALL FDITrack::CalcCRC(BYTE *ptr, int len) const
{
	WORD crc;
	int i;

	ASSERT(this);
	ASSERT(ptr);
	ASSERT(len >= 0);

	// Initialize
	crc = 0xffff;

	// Loop
	for (i=0; i<len; i++) {
		crc = (WORD)((crc << 8) ^ CRCTable[ (BYTE)(crc >> 8) ^ (BYTE)*ptr++ ]);
	}

	return crc;
}

//---------------------------------------------------------------------------
//
//	CRC calculation table
//
//---------------------------------------------------------------------------
const WORD FDITrack::CRCTable[0x100] = {
	0x0000,  0x1021,  0x2042,  0x3063,  0x4084,  0x50a5,  0x60c6,  0x70e7,
	0x8108,  0x9129,  0xa14a,  0xb16b,  0xc18c,  0xd1ad,  0xe1ce,  0xf1ef,
	0x1231,  0x0210,  0x3273,  0x2252,  0x52b5,  0x4294,  0x72f7,  0x62d6,
	0x9339,  0x8318,  0xb37b,  0xa35a,  0xd3bd,  0xc39c,  0xf3ff,  0xe3de,
	0x2462,  0x3443,  0x0420,  0x1401,  0x64e6,  0x74c7,  0x44a4,  0x5485,
	0xa56a,  0xb54b,  0x8528,  0x9509,  0xe5ee,  0xf5cf,  0xc5ac,  0xd58d,
	0x3653,  0x2672,  0x1611,  0x0630,  0x76d7,  0x66f6,  0x5695,  0x46b4,
	0xb75b,  0xa77a,  0x9719,  0x8738,  0xf7df,  0xe7fe,  0xd79d,  0xc7bc,
	0x48c4,  0x58e5,  0x6886,  0x78a7,  0x0840,  0x1861,  0x2802,  0x3823,
	0xc9cc,  0xd9ed,  0xe98e,  0xf9af,  0x8948,  0x9969,  0xa90a,  0xb92b,
	0x5af5,  0x4ad4,  0x7ab7,  0x6a96,  0x1a71,  0x0a50,  0x3a33,  0x2a12,
	0xdbfd,  0xcbdc,  0xfbbf,  0xeb9e,  0x9b79,  0x8b58,  0xbb3b,  0xab1a,
	0x6ca6,  0x7c87,  0x4ce4,  0x5cc5,  0x2c22,  0x3c03,  0x0c60,  0x1c41,
	0xedae,  0xfd8f,  0xcdec,  0xddcd,  0xad2a,  0xbd0b,  0x8d68,  0x9d49,
	0x7e97,  0x6eb6,  0x5ed5,  0x4ef4,  0x3e13,  0x2e32,  0x1e51,  0x0e70,
	0xff9f,  0xefbe,  0xdfdd,  0xcffc,  0xbf1b,  0xaf3a,  0x9f59,  0x8f78,
	0x9188,  0x81a9,  0xb1ca,  0xa1eb,  0xd10c,  0xc12d,  0xf14e,  0xe16f,
	0x1080,  0x00a1,  0x30c2,  0x20e3,  0x5004,  0x4025,  0x7046,  0x6067,
	0x83b9,  0x9398,  0xa3fb,  0xb3da,  0xc33d,  0xd31c,  0xe37f,  0xf35e,
	0x02b1,  0x1290,  0x22f3,  0x32d2,  0x4235,  0x5214,  0x6277,  0x7256,
	0xb5ea,  0xa5cb,  0x95a8,  0x8589,  0xf56e,  0xe54f,  0xd52c,  0xc50d,
	0x34e2,  0x24c3,  0x14a0,  0x0481,  0x7466,  0x6447,  0x5424,  0x4405,
	0xa7db,  0xb7fa,  0x8799,  0x97b8,  0xe75f,  0xf77e,  0xc71d,  0xd73c,
	0x26d3,  0x36f2,  0x0691,  0x16b0,  0x6657,  0x7676,  0x4615,  0x5634,
	0xd94c,  0xc96d,  0xf90e,  0xe92f,  0x99c8,  0x89e9,  0xb98a,  0xa9ab,
	0x5844,  0x4865,  0x7806,  0x6827,  0x18c0,  0x08e1,  0x3882,  0x28a3,
	0xcb7d,  0xdb5c,  0xeb3f,  0xfb1e,  0x8bf9,  0x9bd8,  0xabbb,  0xbb9a,
	0x4a75,  0x5a54,  0x6a37,  0x7a16,  0x0af1,  0x1ad0,  0x2ab3,  0x3a92,
	0xfd2e,  0xed0f,  0xdd6c,  0xcd4d,  0xbdaa,  0xad8b,  0x9de8,  0x8dc9,
	0x7c26,  0x6c07,  0x5c64,  0x4c45,  0x3ca2,  0x2c83,  0x1ce0,  0x0cc1,
	0xef1f,  0xff3e,  0xcf5d,  0xdf7c,  0xaf9b,  0xbfba,  0x8fd9,  0x9ff8,
	0x6e17,  0x7e36,  0x4e55,  0x5e74,  0x2e93,  0x3eb2,  0x0ed1,  0x1ef0
};

//---------------------------------------------------------------------------
//
//	Create Diag data
//
//---------------------------------------------------------------------------
int FASTCALL FDITrack::MakeData(BYTE *buf, int offset, BYTE data, int length) const
{
	int i;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(offset >= 0);
	ASSERT((length > 0) && (length < 0x400));

	for (i=0; i<length; i++) {
		buf[offset + i] = data;
	}

	return (offset + length);
}

//===========================================================================
//
//	FDI Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDisk::FDIDisk(int index, FDI *fdi)
{
	ASSERT((index >= 0) && (index < 0x10));

	// Specify index, ID
	disk.index = index;
	disk.fdi = fdi;
	disk.id = MAKEID('I', 'N', 'I', 'T');

	// State
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// No name
	disk.name[0] = '\0';
	disk.offset = 0;

	// No held tracks
	disk.first = NULL;
	disk.head[0] = NULL;
	disk.head[1] = NULL;

	// Position
	disk.search = 0;

	// No link
	disk.next = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//	Note for derived classes:
//		Write current head[] back to file
//
//---------------------------------------------------------------------------
FDIDisk::~FDIDisk()
{
	// Clear
	ClrTrack();
}

//---------------------------------------------------------------------------
//
//	Create
//	Note for derived classes:
//		Perform physical format (call here at end of virtual function)
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Create(const Filepath& /*path*/, const option_t *opt)
{
	ASSERT(this);
	ASSERT(opt);

	// End if logical format not needed
	if (!opt->logfmt) {
		return TRUE;
	}

	// Perform logical format by physical format type
	switch (opt->phyfmt) {
		// 2HD
		case FDI_2HD:
			Create2HD(TRUE);
			break;

		// 2HDA
		case FDI_2HDA:
			Create2HD(FALSE);
			break;

		// 2HS
		case FDI_2HS:
			Create2HS();
			break;

		// 2HC
		case FDI_2HC:
			Create2HC();
			break;

		// 2HDE
		case FDI_2HDE:
			Create2HDE();
			break;

		// 2HQ
		case FDI_2HQ:
			Create2HQ();
			break;

		// 2DD
		case FDI_2DD:
			Create2DD();
			break;

		// Other
		default:
			return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Logical format (2HD, 2HDA)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HD(BOOL flag2hd)
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to all of track 0 sectors 1-8
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 3;
	for (i=1; i<=8; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Write to all of track 1 sectors 1-3
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 3;
	for (i=1; i<=3; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// IPLWrite
	memcpy(buf, IPL2HD, 0x200);
	if (!flag2hd) {
		// 2HDA has logical sector count = 1280 sectors
		buf[0x13] = 0x00;
		buf[0x14] = 0x05;
	}
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Initialize FAT first sector
	memset(buf, 0, sizeof(buf));
	buf[0] = 0xfe;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// Write 1st FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Write 2nd FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 4;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL (2HD, 2HDA)
//	Obtained from FORMAT.x v2.31
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HD[0x200] = {
	0x60,0x3c,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x04,0x01,0x01,0x00,
	0x02,0xc0,0x00,0xd0,0x04,0xfe,0x02,0x00,
	0x08,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x04,0x00,0x03,0x00,
	0x00,0x06,0x00,0x08,0x00,0x1f,0x00,0x09,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	Logical format (2HS)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HS()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to total 10 sectors (9 sectors per track)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 3;
	for (i=11; i<=18; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[2] = 10;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// IPLWrite(1)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HS[0x000], FALSE);

	// IPLWrite(2)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 13;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HS[0x400], FALSE);

	// Initialize FAT first sector
	buf[0] = 0xfb;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// FATWrite
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 11;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL (2HS)
//	Obtained from 9scdrv.x v3.00
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HS[0x800] = {
	0x60,0x1e,0x39,0x53,0x43,0x46,0x4d,0x54,
	0x20,0x49,0x50,0x4c,0x20,0x76,0x31,0x2e,
	0x30,0x32,0x04,0x00,0x01,0x03,0x00,0x01,
	0x00,0xc0,0x05,0xa0,0xfb,0x01,0x90,0x70,
	0x60,0x00,0x03,0x5a,0x08,0x01,0x00,0x0c,
	0x66,0x08,0x4d,0xfa,0xff,0xd4,0x2c,0x56,
	0x4e,0xd6,0x61,0x00,0x00,0xba,0x48,0xe7,
	0x4f,0x00,0x61,0x00,0x02,0xf0,0x61,0x00,
	0x00,0xc4,0x08,0x00,0x00,0x1b,0x66,0x4e,
	0xc2,0x3c,0x00,0xc0,0x82,0x3c,0x00,0x06,
	0x61,0x00,0x00,0xd0,0xe1,0x9a,0x54,0x88,
	0x20,0xc2,0xe0,0x9a,0x10,0xc2,0x10,0xc7,
	0x10,0x86,0x61,0x00,0x00,0xf0,0x41,0xf8,
	0x09,0xee,0x70,0x08,0x61,0x00,0x01,0x0c,
	0x61,0x00,0x01,0x42,0x61,0x00,0x01,0x60,
	0x61,0x00,0x01,0x7a,0x08,0x00,0x00,0x0e,
	0x66,0x0c,0x08,0x00,0x00,0x1e,0x67,0x26,
	0x08,0x00,0x00,0x1b,0x66,0x08,0x61,0x00,
	0x01,0x7a,0x51,0xcc,0xff,0xbc,0x4c,0xdf,
	0x00,0xf2,0x4a,0x38,0x09,0xe1,0x67,0x0c,
	0x31,0xf8,0x09,0xc2,0x09,0xc4,0x11,0xfc,
	0x00,0x40,0x09,0xe1,0x4e,0x75,0x08,0x00,
	0x00,0x1f,0x66,0xe2,0xd3,0xc5,0x96,0x85,
	0x63,0xdc,0x20,0x04,0x48,0x40,0x38,0x00,
	0x30,0x3c,0x00,0x12,0x52,0x02,0xb0,0x02,
	0x64,0x86,0x14,0x3c,0x00,0x0a,0x0a,0x42,
	0x01,0x00,0x08,0x02,0x00,0x08,0x66,0x00,
	0xff,0x78,0xd4,0xbc,0x00,0x01,0x00,0x00,
	0x61,0x00,0x01,0xb8,0x08,0x00,0x00,0x1b,
	0x66,0xac,0x60,0x00,0xff,0x64,0x08,0x38,
	0x00,0x07,0x09,0xe1,0x66,0x0c,0x48,0xe7,
	0xc0,0x00,0x61,0x00,0x01,0x46,0x4c,0xdf,
	0x00,0x03,0x4e,0x75,0x70,0x00,0x78,0x00,
	0x08,0x01,0x00,0x05,0x67,0x08,0x78,0x09,
	0x48,0x44,0x38,0x3c,0x00,0x09,0x08,0x01,
	0x00,0x04,0x67,0x04,0x61,0x00,0x01,0x7c,
	0x4e,0x75,0x2f,0x01,0x41,0xf8,0x09,0xee,
	0x10,0x81,0xe0,0x99,0xc2,0x3c,0x00,0x03,
	0x08,0x02,0x00,0x08,0x67,0x04,0x08,0xc1,
	0x00,0x02,0x11,0x41,0x00,0x01,0x22,0x1f,
	0x4e,0x75,0x13,0xfc,0x00,0xff,0x00,0xe8,
	0x40,0x00,0x13,0xfc,0x00,0x32,0x00,0xe8,
	0x40,0x05,0x60,0x10,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x13,0xfc,0x00,0xb2,
	0x00,0xe8,0x40,0x05,0x23,0xc9,0x00,0xe8,
	0x40,0x0c,0x33,0xc5,0x00,0xe8,0x40,0x0a,
	0x13,0xfc,0x00,0x80,0x00,0xe8,0x40,0x07,
	0x4e,0x75,0x48,0xe7,0x40,0x60,0x43,0xf9,
	0x00,0xe9,0x40,0x01,0x45,0xf9,0x00,0xe9,
	0x40,0x03,0x40,0xe7,0x00,0x7c,0x07,0x00,
	0x12,0x11,0x08,0x01,0x00,0x04,0x66,0xf8,
	0x12,0x11,0x08,0x01,0x00,0x07,0x67,0xf8,
	0x08,0x01,0x00,0x06,0x66,0xf2,0x14,0x98,
	0x51,0xc8,0xff,0xee,0x46,0xdf,0x4c,0xdf,
	0x06,0x02,0x4e,0x75,0x10,0x39,0x00,0xe8,
	0x40,0x00,0x08,0x00,0x00,0x04,0x66,0x0e,
	0x10,0x39,0x00,0xe9,0x40,0x01,0xc0,0x3c,
	0x00,0x1f,0x66,0xf4,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x01,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x00,0x08,0x00,0x00,0x07,
	0x66,0x08,0x13,0xfc,0x00,0x10,0x00,0xe8,
	0x40,0x07,0x13,0xfc,0x00,0xff,0x00,0xe8,
	0x40,0x00,0x4e,0x75,0x30,0x01,0xe0,0x48,
	0xc0,0xbc,0x00,0x00,0x00,0x03,0xe7,0x40,
	0x41,0xf8,0x0c,0x90,0xd1,0xc0,0x20,0x10,
	0x4e,0x75,0x2f,0x00,0xc0,0xbc,0x00,0x35,
	0xff,0x00,0x67,0x2a,0xb8,0x3c,0x00,0x05,
	0x64,0x24,0x2f,0x38,0x09,0xee,0x2f,0x38,
	0x09,0xf2,0x3f,0x38,0x09,0xf6,0x61,0x00,
	0x00,0xc4,0x70,0x64,0x51,0xc8,0xff,0xfe,
	0x61,0x68,0x31,0xdf,0x09,0xf6,0x21,0xdf,
	0x09,0xf2,0x21,0xdf,0x09,0xee,0x20,0x1f,
	0x4e,0x75,0x30,0x01,0xe0,0x48,0x4a,0x00,
	0x67,0x3c,0xc0,0x3c,0x00,0x03,0x80,0x3c,
	0x00,0x80,0x08,0xf8,0x00,0x07,0x09,0xe1,
	0x13,0xc0,0x00,0xe9,0x40,0x07,0x08,0xf8,
	0x00,0x06,0x09,0xe1,0x66,0x18,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x61,0x00,0x00,0x90,
	0x08,0x00,0x00,0x1d,0x66,0x08,0x0c,0x78,
	0x00,0x64,0x09,0xc4,0x64,0xee,0x08,0xb8,
	0x00,0x06,0x09,0xe1,0x4e,0x75,0x4a,0x38,
	0x09,0xe1,0x67,0x0c,0x31,0xf8,0x09,0xc2,
	0x09,0xc4,0x11,0xfc,0x00,0x40,0x09,0xe1,
	0x4e,0x75,0x61,0x12,0x08,0x00,0x00,0x1b,
	0x66,0x26,0x48,0x40,0x48,0x42,0xb4,0x00,
	0x67,0x1a,0x48,0x42,0x61,0x3e,0x2f,0x01,
	0x12,0x3c,0x00,0x0f,0x61,0x00,0xfe,0x6c,
	0x48,0x42,0x11,0x42,0x00,0x02,0x48,0x42,
	0x70,0x02,0x60,0x08,0x48,0x42,0x48,0x40,
	0x4e,0x75,0x2f,0x01,0x61,0x00,0xfe,0xac,
	0x61,0x00,0xfe,0xee,0x22,0x1f,0x30,0x01,
	0xe0,0x48,0xc0,0xbc,0x00,0x00,0x00,0x03,
	0xe7,0x40,0x41,0xf8,0x0c,0x90,0xd1,0xc0,
	0x20,0x10,0x4e,0x75,0x2f,0x01,0x12,0x3c,
	0x00,0x07,0x61,0x00,0xfe,0x2e,0x70,0x01,
	0x61,0xd0,0x22,0x1f,0x4e,0x75,0x2f,0x01,
	0x12,0x3c,0x00,0x04,0x61,0x00,0xfe,0x1c,
	0x22,0x1f,0x70,0x01,0x61,0x00,0xfe,0x6c,
	0x10,0x39,0x00,0xe9,0x40,0x01,0xc0,0x3c,
	0x00,0xd0,0xb0,0x3c,0x00,0xd0,0x66,0xf0,
	0x70,0x00,0x10,0x39,0x00,0xe9,0x40,0x03,
	0xe0,0x98,0x4e,0x75,0x53,0x02,0x7e,0x00,
	0x3a,0x02,0xe0,0x5d,0x4a,0x05,0x67,0x04,
	0x06,0x45,0x08,0x00,0xe0,0x4d,0x48,0x42,
	0x02,0x82,0x00,0x00,0x00,0xff,0xe9,0x8a,
	0xd4,0x45,0x0c,0x42,0x00,0x04,0x65,0x02,
	0x53,0x42,0x84,0xfc,0x00,0x12,0x48,0x42,
	0x3e,0x02,0x8e,0xfc,0x00,0x09,0x48,0x47,
	0xe1,0x4f,0xe0,0x8f,0x34,0x07,0x06,0x82,
	0x03,0x00,0x00,0x0a,0x2a,0x3c,0x00,0x00,
	0x04,0x00,0x3c,0x3c,0x00,0xff,0x3e,0x3c,
	0x09,0x28,0x4e,0x75,0x4f,0xfa,0xfc,0x82,
	0x43,0xfa,0xfc,0xa2,0x4d,0xfa,0xfc,0x7a,
	0x2c,0xb9,0x00,0x00,0x05,0x18,0x23,0xc9,
	0x00,0x00,0x05,0x18,0x43,0xfa,0x00,0xda,
	0x4d,0xfa,0xfc,0x6a,0x2c,0xb9,0x00,0x00,
	0x05,0x14,0x23,0xc9,0x00,0x00,0x05,0x14,
	0x43,0xfa,0x01,0x6e,0x4d,0xfa,0xfc,0x5a,
	0x2c,0xb9,0x00,0x00,0x05,0x04,0x23,0xc9,
	0x00,0x00,0x05,0x04,0x24,0x3c,0x03,0x00,
	0x00,0x04,0x20,0x3c,0x00,0x00,0x00,0x8e,
	0x4e,0x4f,0x12,0x00,0xe1,0x41,0x12,0x3c,
	0x00,0x70,0x33,0xc1,0x00,0x00,0x00,0x64,
	0x26,0x3c,0x00,0x00,0x04,0x00,0x43,0xfa,
	0x00,0x20,0x61,0x04,0x60,0x00,0x01,0xf0,
	0x48,0xe7,0x78,0x40,0x70,0x46,0x4e,0x4f,
	0x08,0x00,0x00,0x1e,0x66,0x02,0x70,0x00,
	0x4c,0xdf,0x02,0x1e,0x4e,0x75,0x4e,0x75,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfb,0x8c,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfc,0x6e,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfe,0xa4,0x61,0x00,0xfc,0x78,0x08,0x00,
	0x00,0x1b,0x66,0x30,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x05,0x60,0x08,0x30,0x3c,
	0x01,0xac,0x51,0xc8,0xff,0xfe,0x61,0x00,
	0x01,0x00,0x08,0x00,0x00,0x1e,0x67,0x2c,
	0x08,0x00,0x00,0x1b,0x66,0x0e,0x08,0x00,
	0x00,0x11,0x66,0x08,0x61,0x00,0xfd,0x4c,
	0x51,0xcc,0xff,0xe4,0x4c,0xdf,0x00,0xf2,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x08,0x00,0x00,0x1f,
	0x66,0xe2,0xd3,0xc5,0x96,0x85,0x63,0xdc,
	0x20,0x04,0x48,0x40,0x38,0x00,0x30,0x3c,
	0x00,0x12,0x52,0x02,0xb0,0x02,0x64,0xae,
	0x14,0x3c,0x00,0x0a,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0x98,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfd,0x8c,
	0x08,0x00,0x00,0x1b,0x66,0xae,0x60,0x8e,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfa,0xe0,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfb,0xc6,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfd,0xfc,0x61,0x00,0xfb,0xd0,0x08,0x00,
	0x00,0x1b,0x66,0x24,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x11,0x61,0x62,0x08,0x00,
	0x00,0x0a,0x66,0x14,0x08,0x00,0x00,0x1e,
	0x67,0x16,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0xfc,0xb0,0x51,0xcc,0xff,0xe6,
	0x4c,0xdf,0x00,0xf2,0x60,0x00,0xfb,0x34,
	0x08,0x00,0x00,0x1f,0x66,0xf2,0xd3,0xc5,
	0x96,0x85,0x63,0xec,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x12,0x30,0x3c,
	0x00,0x12,0x52,0x02,0xb0,0x02,0x64,0xbc,
	0x14,0x3c,0x00,0x0a,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0xae,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfc,0xfc,
	0x08,0x00,0x00,0x1b,0x66,0xba,0x60,0x9c,
	0x61,0x00,0xfb,0x78,0xe1,0x9a,0x54,0x88,
	0x20,0xc2,0xe0,0x9a,0x10,0xc2,0x10,0xc7,
	0x10,0x86,0x61,0x00,0xfb,0x86,0x41,0xf8,
	0x09,0xee,0x70,0x08,0x61,0x00,0xfb,0xb4,
	0x61,0x00,0xfb,0xea,0x61,0x00,0xfc,0x08,
	0x61,0x00,0xfc,0x22,0x4e,0x75,0x43,0xfa,
	0x01,0x8c,0x61,0x00,0x01,0x76,0x24,0x3c,
	0x03,0x00,0x00,0x06,0x32,0x39,0x00,0x00,
	0x00,0x64,0x26,0x3c,0x00,0x00,0x04,0x00,
	0x43,0xf8,0x28,0x00,0x61,0x00,0xfd,0xf2,
	0x4a,0x80,0x66,0x00,0x01,0x20,0x43,0xf8,
	0x28,0x00,0x49,0xfa,0x01,0x54,0x78,0x1f,
	0x24,0x49,0x26,0x4c,0x7a,0x0a,0x10,0x1a,
	0x80,0x3c,0x00,0x20,0xb0,0x1b,0x66,0x06,
	0x51,0xcd,0xff,0xf4,0x60,0x0c,0x43,0xe9,
	0x00,0x20,0x51,0xcc,0xff,0xe4,0x66,0x00,
	0x00,0xf4,0x30,0x29,0x00,0x1a,0xe1,0x58,
	0x55,0x40,0xd0,0x7c,0x00,0x0b,0x34,0x00,
	0xc4,0x7c,0x00,0x07,0x52,0x02,0xe8,0x48,
	0x64,0x04,0x84,0x7c,0x01,0x00,0x48,0x42,
	0x34,0x3c,0x03,0x00,0x14,0x00,0x48,0x42,
	0x26,0x29,0x00,0x1c,0xe1,0x5b,0x48,0x43,
	0xe1,0x5b,0x43,0xf8,0x67,0xc0,0x61,0x00,
	0xfd,0x88,0x0c,0x51,0x48,0x55,0x66,0x00,
	0x00,0xb4,0x4b,0xf8,0x68,0x00,0x49,0xfa,
	0x00,0x4c,0x22,0x4d,0x43,0xf1,0x38,0xc0,
	0x2c,0x3c,0x00,0x04,0x00,0x00,0x0c,0x69,
	0x4e,0xd4,0xff,0xd2,0x66,0x36,0x0c,0xad,
	0x4c,0x5a,0x58,0x20,0x00,0x04,0x66,0x16,
	0x2b,0x46,0x00,0x04,0x2b,0x4d,0x00,0x08,
	0x42,0xad,0x00,0x20,0x51,0xf9,0x00,0x00,
	0x07,0x9e,0x4e,0xed,0x00,0x02,0x0c,0x6d,
	0x4e,0xec,0x00,0x1a,0x66,0x0e,0x0c,0x6d,
	0x4e,0xea,0x00,0x2a,0x66,0x06,0x43,0xfa,
	0x01,0x1f,0x60,0x64,0x10,0x3c,0x00,0xc0,
	0x41,0xf8,0x68,0x00,0x36,0x3c,0xff,0xff,
	0xb0,0x18,0x67,0x26,0x51,0xcb,0xff,0xfa,
	0x43,0xf8,0x68,0x00,0x4a,0x39,0x00,0x00,
	0x07,0x9e,0x67,0x14,0x41,0xf8,0x67,0xcc,
	0x24,0x18,0xd4,0x98,0x22,0x10,0xd1,0xc2,
	0x53,0x81,0x65,0x04,0x42,0x18,0x60,0xf8,
	0x4e,0xd1,0x0c,0x10,0x00,0x04,0x66,0xd0,
	0x52,0x88,0x0c,0x10,0x00,0xd0,0x66,0xc8,
	0x52,0x88,0x0c,0x10,0x00,0xfe,0x66,0xc0,
	0x52,0x88,0x0c,0x10,0x00,0x02,0x66,0xb8,
	0x57,0x88,0x30,0xfc,0x05,0xa1,0x10,0xbc,
	0x00,0xfb,0x60,0xac,0x43,0xfa,0x00,0x92,
	0x2f,0x09,0x43,0xfa,0x00,0x47,0x61,0x2a,
	0x43,0xfa,0x00,0x46,0x61,0x24,0x43,0xfa,
	0x00,0x52,0x61,0x1e,0x43,0xfa,0x00,0x43,
	0x61,0x18,0x43,0xfa,0x00,0x46,0x61,0x12,
	0x22,0x5f,0x61,0x0e,0x32,0x39,0x00,0x00,
	0x00,0x64,0x70,0x4f,0x4e,0x4f,0x70,0xfe,
	0x4e,0x4f,0x70,0x21,0x4e,0x4f,0x4e,0x75,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x39,0x53,0x43,0x49,
	0x50,0x4c,0x00,0x1b,0x5b,0x34,0x37,0x6d,
	0x1b,0x5b,0x31,0x33,0x3b,0x32,0x36,0x48,
	0x00,0x1b,0x5b,0x31,0x34,0x3b,0x32,0x36,
	0x48,0x00,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x00,
	0x1b,0x5b,0x31,0x34,0x3b,0x33,0x34,0x48,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x93,0xc7,0x82,0xdd,
	0x8d,0x9e,0x82,0xdd,0x83,0x47,0x83,0x89,
	0x81,0x5b,0x82,0xc5,0x82,0xb7,0x00,0x1b,
	0x5b,0x31,0x34,0x3b,0x33,0x34,0x48,0x4c,
	0x5a,0x58,0x2e,0x58,0x20,0x82,0xcc,0x83,
	0x6f,0x81,0x5b,0x83,0x57,0x83,0x87,0x83,
	0x93,0x82,0xaa,0x8c,0xc3,0x82,0xb7,0x82,
	0xac,0x82,0xdc,0x82,0xb7,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	Logical format (2HC)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HC()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to total 29 sectors (15 sectors per track)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=15; i++) {
		chrn[2] = (BYTE)i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=14; i++) {
		chrn[2] = (BYTE)i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// IPLWrite
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(IPL2HC, FALSE);

	// Initialize FAT first sector
	buf[0] = 0xf9;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// Write 1st FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Write 2nd FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 9;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL (2HC)
//	Obtained from FORMAT.x v2.31
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HC[0x200] = {
	0x60,0x3c,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x02,0x01,0x01,0x00,
	0x02,0xe0,0x00,0x60,0x09,0xf9,0x07,0x00,
	0x0f,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x02,0x00,0x02,0x00,
	0x01,0x01,0x00,0x0f,0x00,0x0f,0x00,0x1b,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	Logical format (2HDE)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HDE()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to total 13 sectors (9 sectors per track)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[3] = 3;
	for (i=2; i<=9; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0x81;
	chrn[3] = 3;
	for (i=1; i<=4; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// IPLWrite(1)
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HDE[0x000], FALSE);

	// IPLWrite(2)
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 4;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(&IPL2HDE[0x400], FALSE);

	// Initialize FAT first sector
	buf[0] = 0xf8;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// Write 1st FAT
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 2;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Write 2nd FAT
	chrn[0] = 0;
	chrn[1] = 0x80;
	chrn[2] = 5;
	chrn[3] = 3;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL (2HDE)
//	Obtained from 9scdrv.x v3.00
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HDE[0x800] = {
	0x60,0x20,0x32,0x48,0x44,0x45,0x20,0x76,
	0x31,0x2e,0x31,0x00,0x00,0x04,0x01,0x01,
	0x00,0x02,0xc0,0x00,0xa0,0x05,0x03,0x03,
	0x00,0x09,0x00,0x02,0x00,0x00,0x00,0x00,
	0x90,0x70,0x60,0x00,0x03,0x5a,0x08,0x01,
	0x00,0x0c,0x66,0x08,0x4d,0xfa,0xff,0xd2,
	0x2c,0x56,0x4e,0xd6,0x61,0x00,0x00,0xba,
	0x48,0xe7,0x4f,0x00,0x61,0x00,0x02,0xf0,
	0x61,0x00,0x00,0xc4,0x08,0x00,0x00,0x1b,
	0x66,0x4e,0xc2,0x3c,0x00,0xc0,0x82,0x3c,
	0x00,0x06,0x61,0x00,0x00,0xd0,0xe1,0x9a,
	0x54,0x88,0x20,0xc2,0xe0,0x9a,0x10,0xc2,
	0x10,0xc7,0x10,0x86,0x61,0x00,0x00,0xf0,
	0x41,0xf8,0x09,0xee,0x70,0x08,0x61,0x00,
	0x01,0x0c,0x61,0x00,0x01,0x42,0x61,0x00,
	0x01,0x60,0x61,0x00,0x01,0x7a,0x08,0x00,
	0x00,0x0e,0x66,0x0c,0x08,0x00,0x00,0x1e,
	0x67,0x26,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0x01,0x7a,0x51,0xcc,0xff,0xbc,
	0x4c,0xdf,0x00,0xf2,0x4a,0x38,0x09,0xe1,
	0x67,0x0c,0x31,0xf8,0x09,0xc2,0x09,0xc4,
	0x11,0xfc,0x00,0x40,0x09,0xe1,0x4e,0x75,
	0x08,0x00,0x00,0x1f,0x66,0xe2,0xd3,0xc5,
	0x96,0x85,0x63,0xdc,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x09,0x52,0x02,
	0xb0,0x02,0x64,0x86,0x14,0x3c,0x00,0x01,
	0x0a,0x42,0x01,0x00,0x08,0x02,0x00,0x08,
	0x66,0x00,0xff,0x78,0xd4,0xbc,0x00,0x01,
	0x00,0x00,0x61,0x00,0x01,0xb8,0x08,0x00,
	0x00,0x1b,0x66,0xac,0x60,0x00,0xff,0x64,
	0x08,0x38,0x00,0x07,0x09,0xe1,0x66,0x0c,
	0x48,0xe7,0xc0,0x00,0x61,0x00,0x01,0x46,
	0x4c,0xdf,0x00,0x03,0x4e,0x75,0x70,0x00,
	0x78,0x00,0x08,0x01,0x00,0x05,0x67,0x08,
	0x78,0x09,0x48,0x44,0x38,0x3c,0x00,0x09,
	0x08,0x01,0x00,0x04,0x67,0x04,0x61,0x00,
	0x01,0x7c,0x4e,0x75,0x2f,0x01,0x41,0xf8,
	0x09,0xee,0x10,0x81,0xe0,0x99,0xc2,0x3c,
	0x00,0x03,0x08,0x02,0x00,0x08,0x67,0x04,
	0x08,0xc1,0x00,0x02,0x11,0x41,0x00,0x01,
	0x22,0x1f,0x4e,0x75,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x13,0xfc,0x00,0x32,
	0x00,0xe8,0x40,0x05,0x60,0x10,0x13,0xfc,
	0x00,0xff,0x00,0xe8,0x40,0x00,0x13,0xfc,
	0x00,0xb2,0x00,0xe8,0x40,0x05,0x23,0xc9,
	0x00,0xe8,0x40,0x0c,0x33,0xc5,0x00,0xe8,
	0x40,0x0a,0x13,0xfc,0x00,0x80,0x00,0xe8,
	0x40,0x07,0x4e,0x75,0x48,0xe7,0x40,0x60,
	0x43,0xf9,0x00,0xe9,0x40,0x01,0x45,0xf9,
	0x00,0xe9,0x40,0x03,0x40,0xe7,0x00,0x7c,
	0x07,0x00,0x12,0x11,0x08,0x01,0x00,0x04,
	0x66,0xf8,0x12,0x11,0x08,0x01,0x00,0x07,
	0x67,0xf8,0x08,0x01,0x00,0x06,0x66,0xf2,
	0x14,0x98,0x51,0xc8,0xff,0xee,0x46,0xdf,
	0x4c,0xdf,0x06,0x02,0x4e,0x75,0x10,0x39,
	0x00,0xe8,0x40,0x00,0x08,0x00,0x00,0x04,
	0x66,0x0e,0x10,0x39,0x00,0xe9,0x40,0x01,
	0xc0,0x3c,0x00,0x1f,0x66,0xf4,0x4e,0x75,
	0x10,0x39,0x00,0xe8,0x40,0x01,0x4e,0x75,
	0x10,0x39,0x00,0xe8,0x40,0x00,0x08,0x00,
	0x00,0x07,0x66,0x08,0x13,0xfc,0x00,0x10,
	0x00,0xe8,0x40,0x07,0x13,0xfc,0x00,0xff,
	0x00,0xe8,0x40,0x00,0x4e,0x75,0x30,0x01,
	0xe0,0x48,0xc0,0xbc,0x00,0x00,0x00,0x03,
	0xe7,0x40,0x41,0xf8,0x0c,0x90,0xd1,0xc0,
	0x20,0x10,0x4e,0x75,0x2f,0x00,0xc0,0xbc,
	0x00,0x35,0xff,0x00,0x67,0x2a,0xb8,0x3c,
	0x00,0x05,0x64,0x24,0x2f,0x38,0x09,0xee,
	0x2f,0x38,0x09,0xf2,0x3f,0x38,0x09,0xf6,
	0x61,0x00,0x00,0xc4,0x70,0x64,0x51,0xc8,
	0xff,0xfe,0x61,0x68,0x31,0xdf,0x09,0xf6,
	0x21,0xdf,0x09,0xf2,0x21,0xdf,0x09,0xee,
	0x20,0x1f,0x4e,0x75,0x30,0x01,0xe0,0x48,
	0x4a,0x00,0x67,0x3c,0xc0,0x3c,0x00,0x03,
	0x80,0x3c,0x00,0x80,0x08,0xf8,0x00,0x07,
	0x09,0xe1,0x13,0xc0,0x00,0xe9,0x40,0x07,
	0x08,0xf8,0x00,0x06,0x09,0xe1,0x66,0x18,
	0x31,0xf8,0x09,0xc2,0x09,0xc4,0x61,0x00,
	0x00,0x90,0x08,0x00,0x00,0x1d,0x66,0x08,
	0x0c,0x78,0x00,0x64,0x09,0xc4,0x64,0xee,
	0x08,0xb8,0x00,0x06,0x09,0xe1,0x4e,0x75,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x61,0x12,0x08,0x00,
	0x00,0x1b,0x66,0x26,0x48,0x40,0x48,0x42,
	0xb4,0x00,0x67,0x1a,0x48,0x42,0x61,0x3e,
	0x2f,0x01,0x12,0x3c,0x00,0x0f,0x61,0x00,
	0xfe,0x6c,0x48,0x42,0x11,0x42,0x00,0x02,
	0x48,0x42,0x70,0x02,0x60,0x08,0x48,0x42,
	0x48,0x40,0x4e,0x75,0x2f,0x01,0x61,0x00,
	0xfe,0xac,0x61,0x00,0xfe,0xee,0x22,0x1f,
	0x30,0x01,0xe0,0x48,0xc0,0xbc,0x00,0x00,
	0x00,0x03,0xe7,0x40,0x41,0xf8,0x0c,0x90,
	0xd1,0xc0,0x20,0x10,0x4e,0x75,0x2f,0x01,
	0x12,0x3c,0x00,0x07,0x61,0x00,0xfe,0x2e,
	0x70,0x01,0x61,0xd0,0x22,0x1f,0x4e,0x75,
	0x2f,0x01,0x12,0x3c,0x00,0x04,0x61,0x00,
	0xfe,0x1c,0x22,0x1f,0x70,0x01,0x61,0x00,
	0xfe,0x6c,0x10,0x39,0x00,0xe9,0x40,0x01,
	0xc0,0x3c,0x00,0xd0,0xb0,0x3c,0x00,0xd0,
	0x66,0xf0,0x70,0x00,0x10,0x39,0x00,0xe9,
	0x40,0x03,0xe0,0x98,0x4e,0x75,0x53,0x02,
	0x7e,0x00,0x3a,0x02,0xe0,0x5d,0x4a,0x05,
	0x67,0x04,0x06,0x45,0x08,0x00,0xe0,0x4d,
	0x48,0x42,0x02,0x82,0x00,0x00,0x00,0xff,
	0xe9,0x8a,0xd4,0x45,0x0c,0x42,0x00,0x04,
	0x65,0x02,0x54,0x42,0x84,0xfc,0x00,0x12,
	0x48,0x42,0x3e,0x02,0x8e,0xfc,0x00,0x09,
	0x48,0x47,0xe1,0x4f,0xe0,0x8f,0x34,0x07,
	0x06,0x82,0x03,0x00,0x80,0x01,0x2a,0x3c,
	0x00,0x00,0x04,0x00,0x3c,0x3c,0x00,0xff,
	0x3e,0x3c,0x09,0x28,0x4e,0x75,0x4f,0xfa,
	0xfc,0x80,0x43,0xfa,0xfc,0xa2,0x4d,0xfa,
	0xfc,0x78,0x2c,0xb9,0x00,0x00,0x05,0x18,
	0x23,0xc9,0x00,0x00,0x05,0x18,0x43,0xfa,
	0x00,0xda,0x4d,0xfa,0xfc,0x68,0x2c,0xb9,
	0x00,0x00,0x05,0x14,0x23,0xc9,0x00,0x00,
	0x05,0x14,0x43,0xfa,0x01,0x6e,0x4d,0xfa,
	0xfc,0x58,0x2c,0xb9,0x00,0x00,0x05,0x04,
	0x23,0xc9,0x00,0x00,0x05,0x04,0x24,0x3c,
	0x03,0x00,0x00,0x04,0x20,0x3c,0x00,0x00,
	0x00,0x8e,0x4e,0x4f,0x12,0x00,0xe1,0x41,
	0x12,0x3c,0x00,0x70,0x33,0xc1,0x00,0x00,
	0x00,0x66,0x26,0x3c,0x00,0x00,0x04,0x00,
	0x43,0xfa,0x00,0x20,0x61,0x04,0x60,0x00,
	0x01,0xec,0x48,0xe7,0x78,0x40,0x70,0x46,
	0x4e,0x4f,0x08,0x00,0x00,0x1e,0x66,0x02,
	0x70,0x00,0x4c,0xdf,0x02,0x1e,0x4e,0x75,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfb,0x8a,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfc,0x6e,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfe,0xa4,0x61,0x00,0xfc,0x78,0x08,0x00,
	0x00,0x1b,0x66,0x30,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x05,0x60,0x08,0x30,0x3c,
	0x01,0xac,0x51,0xc8,0xff,0xfe,0x61,0x00,
	0x00,0xfc,0x08,0x00,0x00,0x1e,0x67,0x2c,
	0x08,0x00,0x00,0x1b,0x66,0x0e,0x08,0x00,
	0x00,0x11,0x66,0x08,0x61,0x00,0xfd,0x4c,
	0x51,0xcc,0xff,0xe4,0x4c,0xdf,0x00,0xf2,
	0x4a,0x38,0x09,0xe1,0x67,0x0c,0x31,0xf8,
	0x09,0xc2,0x09,0xc4,0x11,0xfc,0x00,0x40,
	0x09,0xe1,0x4e,0x75,0x08,0x00,0x00,0x1f,
	0x66,0xe2,0xd3,0xc5,0x96,0x85,0x63,0xdc,
	0x20,0x04,0x48,0x40,0x38,0x00,0x30,0x3c,
	0x00,0x09,0x52,0x02,0xb0,0x02,0x64,0xae,
	0x14,0x3c,0x00,0x01,0x0a,0x42,0x01,0x00,
	0x08,0x02,0x00,0x08,0x66,0x98,0xd4,0xbc,
	0x00,0x01,0x00,0x00,0x61,0x00,0xfd,0x8c,
	0x08,0x00,0x00,0x1b,0x66,0xae,0x60,0x8e,
	0x08,0x01,0x00,0x0c,0x66,0x08,0x4d,0xfa,
	0xfa,0xde,0x2c,0x56,0x4e,0xd6,0x61,0x00,
	0xfb,0xc6,0x48,0xe7,0x4f,0x00,0x61,0x00,
	0xfd,0xfc,0x61,0x00,0xfb,0xd0,0x08,0x00,
	0x00,0x1b,0x66,0x24,0xc2,0x3c,0x00,0xc0,
	0x82,0x3c,0x00,0x11,0x61,0x5e,0x08,0x00,
	0x00,0x0a,0x66,0x14,0x08,0x00,0x00,0x1e,
	0x67,0x16,0x08,0x00,0x00,0x1b,0x66,0x08,
	0x61,0x00,0xfc,0xb0,0x51,0xcc,0xff,0xe6,
	0x4c,0xdf,0x00,0xf2,0x60,0x00,0xfb,0x34,
	0x08,0x00,0x00,0x1f,0x66,0xf2,0xd3,0xc5,
	0x96,0x85,0x63,0xec,0x20,0x04,0x48,0x40,
	0x38,0x00,0x30,0x3c,0x00,0x09,0x52,0x02,
	0xb0,0x02,0x64,0xc0,0x14,0x3c,0x00,0x01,
	0x0a,0x42,0x01,0x00,0x08,0x02,0x00,0x08,
	0x66,0xb2,0xd4,0xbc,0x00,0x01,0x00,0x00,
	0x61,0x00,0xfd,0x00,0x08,0x00,0x00,0x1b,
	0x66,0xbe,0x60,0xa0,0x61,0x00,0xfb,0x7c,
	0xe1,0x9a,0x54,0x88,0x20,0xc2,0xe0,0x9a,
	0x10,0xc2,0x10,0xc7,0x10,0x86,0x61,0x00,
	0xfb,0x8a,0x41,0xf8,0x09,0xee,0x70,0x08,
	0x61,0x00,0xfb,0xb8,0x61,0x00,0xfb,0xee,
	0x61,0x00,0xfc,0x0c,0x61,0x00,0xfc,0x26,
	0x4e,0x75,0x43,0xfa,0x01,0x8c,0x61,0x00,
	0x01,0x76,0x24,0x3c,0x03,0x00,0x00,0x06,
	0x32,0x39,0x00,0x00,0x00,0x66,0x26,0x3c,
	0x00,0x00,0x04,0x00,0x43,0xf8,0x28,0x00,
	0x61,0x00,0xfd,0xf6,0x4a,0x80,0x66,0x00,
	0x01,0x20,0x43,0xf8,0x28,0x00,0x49,0xfa,
	0x01,0x54,0x78,0x1f,0x24,0x49,0x26,0x4c,
	0x7a,0x0a,0x10,0x1a,0x80,0x3c,0x00,0x20,
	0xb0,0x1b,0x66,0x06,0x51,0xcd,0xff,0xf4,
	0x60,0x0c,0x43,0xe9,0x00,0x20,0x51,0xcc,
	0xff,0xe4,0x66,0x00,0x00,0xf4,0x30,0x29,
	0x00,0x1a,0xe1,0x58,0x55,0x40,0xd0,0x7c,
	0x00,0x0b,0x34,0x00,0xc4,0x7c,0x00,0x07,
	0x52,0x02,0xe8,0x48,0x64,0x04,0x84,0x7c,
	0x01,0x00,0x48,0x42,0x34,0x3c,0x03,0x00,
	0x14,0x00,0x48,0x42,0x26,0x29,0x00,0x1c,
	0xe1,0x5b,0x48,0x43,0xe1,0x5b,0x43,0xf8,
	0x67,0xc0,0x61,0x00,0xfd,0x8c,0x0c,0x51,
	0x48,0x55,0x66,0x00,0x00,0xb4,0x4b,0xf8,
	0x68,0x00,0x49,0xfa,0x00,0x4c,0x22,0x4d,
	0x43,0xf1,0x38,0xc0,0x2c,0x3c,0x00,0x04,
	0x00,0x00,0x0c,0x69,0x4e,0xd4,0xff,0xd2,
	0x66,0x36,0x0c,0xad,0x4c,0x5a,0x58,0x20,
	0x00,0x04,0x66,0x16,0x2b,0x46,0x00,0x04,
	0x2b,0x4d,0x00,0x08,0x42,0xad,0x00,0x20,
	0x51,0xf9,0x00,0x00,0x07,0x9c,0x4e,0xed,
	0x00,0x02,0x0c,0x6d,0x4e,0xec,0x00,0x1a,
	0x66,0x0e,0x0c,0x6d,0x4e,0xea,0x00,0x2a,
	0x66,0x06,0x43,0xfa,0x01,0x20,0x60,0x64,
	0x10,0x3c,0x00,0xc0,0x41,0xf8,0x68,0x00,
	0x36,0x3c,0xff,0xff,0xb0,0x18,0x67,0x26,
	0x51,0xcb,0xff,0xfa,0x43,0xf8,0x68,0x00,
	0x4a,0x39,0x00,0x00,0x07,0x9c,0x67,0x14,
	0x41,0xf8,0x67,0xcc,0x24,0x18,0xd4,0x98,
	0x22,0x10,0xd1,0xc2,0x53,0x81,0x65,0x04,
	0x42,0x18,0x60,0xf8,0x4e,0xd1,0x0c,0x10,
	0x00,0x04,0x66,0xd0,0x52,0x88,0x0c,0x10,
	0x00,0xd0,0x66,0xc8,0x52,0x88,0x0c,0x10,
	0x00,0xfe,0x66,0xc0,0x52,0x88,0x0c,0x10,
	0x00,0x02,0x66,0xb8,0x57,0x88,0x30,0xfc,
	0x05,0x9e,0x10,0xbc,0x00,0xfb,0x60,0xac,
	0x43,0xfa,0x00,0x93,0x2f,0x09,0x43,0xfa,
	0x00,0x48,0x61,0x2a,0x43,0xfa,0x00,0x47,
	0x61,0x24,0x43,0xfa,0x00,0x53,0x61,0x1e,
	0x43,0xfa,0x00,0x44,0x61,0x18,0x43,0xfa,
	0x00,0x47,0x61,0x12,0x22,0x5f,0x61,0x0e,
	0x32,0x39,0x00,0x00,0x00,0x66,0x70,0x4f,
	0x4e,0x4f,0x70,0xfe,0x4e,0x4f,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x68,0x75,0x6d,0x61,
	0x6e,0x20,0x20,0x20,0x73,0x79,0x73,0x00,
	0x32,0x48,0x44,0x45,0x49,0x50,0x4c,0x00,
	0x1b,0x5b,0x34,0x37,0x6d,0x1b,0x5b,0x31,
	0x33,0x3b,0x32,0x36,0x48,0x00,0x1b,0x5b,
	0x31,0x34,0x3b,0x32,0x36,0x48,0x00,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x00,0x1b,0x5b,0x31,
	0x34,0x3b,0x33,0x34,0x48,0x48,0x75,0x6d,
	0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,0x82,
	0xcc,0x93,0xc7,0x82,0xdd,0x8d,0x9e,0x82,
	0xdd,0x83,0x47,0x83,0x89,0x81,0x5b,0x82,
	0xc5,0x82,0xb7,0x00,0x1b,0x5b,0x31,0x34,
	0x3b,0x33,0x34,0x48,0x4c,0x5a,0x58,0x2e,
	0x58,0x20,0x82,0xcc,0x83,0x6f,0x81,0x5b,
	0x83,0x57,0x83,0x87,0x83,0x93,0x82,0xaa,
	0x8c,0xc3,0x82,0xb7,0x82,0xac,0x82,0xdc,
	0x82,0xb7,0x00,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	Logical format (2HQ)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2HQ()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to total 33 sectors (18 sectors per track)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=18; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=15; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// IPLWrite
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(IPL2HQ, FALSE);

	// Initialize FAT first sector
	buf[0] = 0xf0;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// Write 1st FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Write 2nd FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 11;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	IPL (2HQ)
//	Obtained from FORMAT.x v2.31
//
//---------------------------------------------------------------------------
const BYTE FDIDisk::IPL2HQ[0x200] = {
	0xeb,0xfe,0x90,0x58,0x36,0x38,0x49,0x50,
	0x4c,0x33,0x30,0x00,0x02,0x01,0x01,0x00,
	0x02,0xe0,0x00,0x40,0x0b,0xf0,0x09,0x00,
	0x12,0x00,0x02,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x20,0x20,0x20,0x20,0x20,
	0x20,0x20,0x20,0x20,0x20,0x20,0x46,0x41,
	0x54,0x31,0x32,0x20,0x20,0x20,0x4f,0xfa,
	0xff,0xc0,0x4d,0xfa,0x01,0xb8,0x4b,0xfa,
	0x00,0xe0,0x49,0xfa,0x00,0xea,0x43,0xfa,
	0x01,0x20,0x4e,0x94,0x70,0x8e,0x4e,0x4f,
	0x7e,0x70,0xe1,0x48,0x8e,0x40,0x26,0x3a,
	0x01,0x02,0x22,0x4e,0x24,0x3a,0x01,0x00,
	0x32,0x07,0x4e,0x95,0x66,0x28,0x22,0x4e,
	0x32,0x3a,0x00,0xfa,0x20,0x49,0x45,0xfa,
	0x01,0x78,0x70,0x0a,0x00,0x10,0x00,0x20,
	0xb1,0x0a,0x56,0xc8,0xff,0xf8,0x67,0x38,
	0xd2,0xfc,0x00,0x20,0x51,0xc9,0xff,0xe6,
	0x45,0xfa,0x00,0xe0,0x60,0x10,0x45,0xfa,
	0x00,0xfa,0x60,0x0a,0x45,0xfa,0x01,0x10,
	0x60,0x04,0x45,0xfa,0x01,0x28,0x61,0x00,
	0x00,0x94,0x22,0x4a,0x4c,0x99,0x00,0x06,
	0x70,0x23,0x4e,0x4f,0x4e,0x94,0x32,0x07,
	0x70,0x4f,0x4e,0x4f,0x70,0xfe,0x4e,0x4f,
	0x74,0x00,0x34,0x29,0x00,0x1a,0xe1,0x5a,
	0xd4,0x7a,0x00,0xa4,0x84,0xfa,0x00,0x9c,
	0x84,0x7a,0x00,0x94,0xe2,0x0a,0x64,0x04,
	0x08,0xc2,0x00,0x18,0x48,0x42,0x52,0x02,
	0x22,0x4e,0x26,0x3a,0x00,0x7e,0x32,0x07,
	0x4e,0x95,0x34,0x7c,0x68,0x00,0x22,0x4e,
	0x0c,0x59,0x48,0x55,0x66,0xa6,0x54,0x89,
	0xb5,0xd9,0x66,0xa6,0x2f,0x19,0x20,0x59,
	0xd1,0xd9,0x2f,0x08,0x2f,0x11,0x32,0x7c,
	0x67,0xc0,0x76,0x40,0xd6,0x88,0x4e,0x95,
	0x22,0x1f,0x24,0x1f,0x22,0x5f,0x4a,0x80,
	0x66,0x00,0xff,0x7c,0xd5,0xc2,0x53,0x81,
	0x65,0x04,0x42,0x1a,0x60,0xf8,0x4e,0xd1,
	0x70,0x46,0x4e,0x4f,0x08,0x00,0x00,0x1e,
	0x66,0x02,0x70,0x00,0x4e,0x75,0x70,0x21,
	0x4e,0x4f,0x4e,0x75,0x72,0x0f,0x70,0x22,
	0x4e,0x4f,0x72,0x19,0x74,0x0c,0x70,0x23,
	0x4e,0x4f,0x61,0x08,0x72,0x19,0x74,0x0d,
	0x70,0x23,0x4e,0x4f,0x76,0x2c,0x72,0x20,
	0x70,0x20,0x4e,0x4f,0x51,0xcb,0xff,0xf8,
	0x4e,0x75,0x00,0x00,0x02,0x00,0x02,0x00,
	0x01,0x02,0x00,0x12,0x00,0x0f,0x00,0x1f,
	0x1a,0x00,0x00,0x22,0x00,0x0d,0x48,0x75,
	0x6d,0x61,0x6e,0x2e,0x73,0x79,0x73,0x20,
	0x82,0xaa,0x20,0x8c,0xa9,0x82,0xc2,0x82,
	0xa9,0x82,0xe8,0x82,0xdc,0x82,0xb9,0x82,
	0xf1,0x00,0x00,0x25,0x00,0x0d,0x83,0x66,
	0x83,0x42,0x83,0x58,0x83,0x4e,0x82,0xaa,
	0x81,0x40,0x93,0xc7,0x82,0xdf,0x82,0xdc,
	0x82,0xb9,0x82,0xf1,0x00,0x00,0x00,0x23,
	0x00,0x0d,0x48,0x75,0x6d,0x61,0x6e,0x2e,
	0x73,0x79,0x73,0x20,0x82,0xaa,0x20,0x89,
	0xf3,0x82,0xea,0x82,0xc4,0x82,0xa2,0x82,
	0xdc,0x82,0xb7,0x00,0x00,0x20,0x00,0x0d,
	0x48,0x75,0x6d,0x61,0x6e,0x2e,0x73,0x79,
	0x73,0x20,0x82,0xcc,0x20,0x83,0x41,0x83,
	0x68,0x83,0x8c,0x83,0x58,0x82,0xaa,0x88,
	0xd9,0x8f,0xed,0x82,0xc5,0x82,0xb7,0x00,
	0x68,0x75,0x6d,0x61,0x6e,0x20,0x20,0x20,
	0x73,0x79,0x73,0x00,0x00,0x00,0x00,0x00
};

//---------------------------------------------------------------------------
//
//	Logical format (2DD)
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Create2DD()
{
	FDITrack *track;
	FDISector *sector;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;

	ASSERT(this);

	// Normal initialization
	memset(buf, 0, sizeof(buf));

	// Write to total 14 sectors (18 sectors per track)
	track = Search(0);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[3] = 2;
	for (i=1; i<=9; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}
	track = Search(1);
	ASSERT(track);
	chrn[0] = 0;
	chrn[1] = 1;
	chrn[3] = 2;
	for (i=1; i<=5; i++) {
		chrn[2] = i;
		sector = track->Search(TRUE, chrn);
		ASSERT(sector);
		sector->Write(buf, FALSE);
	}

	// Seek to track 0
	track = Search(0);
	ASSERT(track);

	// Process IPL (created based on 2HQ version)
	memcpy(buf, IPL2HQ, sizeof(buf));
	buf[0] = 0x60;
	buf[1] = 0x3c;
	buf[13] = 0x02;
	buf[17] = 0x70;
	buf[19] = 0xa0;
	buf[20] = 0x05;
	buf[21] = 0xf9;
	buf[22] = 0x03;
	buf[24] = 0x09;
	buf[0x168] = 0x00;
	buf[0x169] = 0x08;
	buf[0x16b] = 0x09;
	buf[0x16f] = 0x0c;

	// IPLWrite
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 1;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
	memset(buf, 0, sizeof(buf));

	// Initialize FAT first sector
	buf[0] = 0xf9;
	buf[1] = 0xff;
	buf[2] = 0xff;

	// Write 1st FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 2;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);

	// Write 2nd FAT
	chrn[0] = 0;
	chrn[1] = 0;
	chrn[2] = 5;
	chrn[3] = 2;
	sector = track->Search(TRUE, chrn);
	ASSERT(sector);
	sector->Write(buf, FALSE);
}

//---------------------------------------------------------------------------
//
//	Open
//	Note for derived classes:
//		Set writep, readonly appropriately
//		Set path, name appropriately
//	Note for parent class:
//		Seek to current cylinder after calling
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Open(const Filepath& /*path*/, DWORD /*offset*/)
{
	// Pure virtual class style
	ASSERT(FALSE);
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Write protectedSet
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::WriteP(BOOL flag)
{
	ASSERT(this);

	// If ReadOnly, always write protected
	if (IsReadOnly()) {
		disk.writep = TRUE;
		return;
	}

	// Set
	disk.writep = flag;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::Flush()
{
	ASSERT(this);

	// Do nothing
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get disk name
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::GetName(char *buf) const
{
	ASSERT(this);
	ASSERT(buf);

	strcpy(buf, disk.name);
}

//---------------------------------------------------------------------------
//
//	Get path name
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::GetPath(Filepath& path) const
{
	ASSERT(this);

	// Assign
	path = disk.path;
}

//---------------------------------------------------------------------------
//
//	Seek
//	Note for derived classes:
//		Write current head[] back to file, load new track, set to head[]
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::Seek(int /*c*/)
{
	ASSERT(this);
}

//---------------------------------------------------------------------------
//
//	ReadID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or all valid sectors have ID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadID(DWORD *buf, BOOL mfm, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(buf);
	ASSERT((hd == 0) || (hd == 4));

	// Get track
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// If NULL, NODATA
	if (!track) {
		// Set search time
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// Delegate to track
	return track->ReadID(buf, mfm);
}

//---------------------------------------------------------------------------
//
//	Read sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Get track
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// If NULL, NODATA
	if (!track) {
		// Set search time
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// Delegate to track
	return track->ReadSector(buf, len, mfm, chrn);
}

//---------------------------------------------------------------------------
//
//	Write sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTWRITE	Media is write protected
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Write check
	if (IsWriteP()) {
		return FDD_NOTWRITE;
	}

	// Get track
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// If NULL, NODATA
	if (!track) {
		// Set search time
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// Delegate to track
	return track->WriteSector(buf, len, mfm, chrn, deleted);
}

//---------------------------------------------------------------------------
//
//	Read diag
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	FDITrack *track;
	DWORD pos;

	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Get track
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// If NULL, NODATA
	if (!track) {
		// Set search time
		pos = GetRotationTime();
		pos = (pos * 2) - GetRotationPos();
		SetSearch(pos);
		return FDD_MAM | FDD_NODATA;
	}

	// Delegate to track
	return track->ReadDiag(buf, len, mfm, chrn);
}

//---------------------------------------------------------------------------
//
//	WriteID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTWRITE	Media is write protected
//
//---------------------------------------------------------------------------
int FASTCALL FDIDisk::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl)
{
	FDITrack *track;

	ASSERT(this);
	ASSERT(sc > 0);

	// Write check
	if (IsWriteP()) {
		return FDD_NOTWRITE;
	}

	// Get track
	if (hd == 0) {
		track = GetHead(0);
	}
	else {
		track = GetHead(1);
	}

	// If NULL, treat as No Error (Write ID to 154 tracks in format.x v2.20)
	if (!track) {
		return FDD_NOERROR;
	}

	// Delegate to track
	return track->WriteID(buf, d, sc, mfm, gpl);
}

//---------------------------------------------------------------------------
//
//	Get rotation position
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDisk::GetRotationPos() const
{
	ASSERT(this);
	ASSERT(GetFDI());

	// Ask parent
	return GetFDI()->GetRotationPos();
}

//---------------------------------------------------------------------------
//
//	Get rotation time
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDisk::GetRotationTime() const
{
	ASSERT(this);

	ASSERT(GetFDI());

	// Ask parent
	return GetFDI()->GetRotationTime();
}

//---------------------------------------------------------------------------
//
//	Calculate search length
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::CalcSearch(DWORD pos)
{
	DWORD cur;
	DWORD hus;

	ASSERT(this);

	// Get
	cur = GetRotationPos();
	hus = GetRotationTime();

	// If current < position, output difference only
	if (cur < pos) {
		SetSearch(pos - cur);
		return;
	}

	// If position < current, exceeds 1 revolution
	ASSERT(cur <= hus);
	pos += (hus - cur);
	SetSearch(pos);
}

//---------------------------------------------------------------------------
//
//	Get HD flag
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk::IsHD() const
{
	ASSERT(this);
	ASSERT(GetFDI());

	// Ask parent
	return GetFDI()->IsHD();
}

//---------------------------------------------------------------------------
//
//	Add track
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::AddTrack(FDITrack *track)
{
	FDITrack *ptr;

	ASSERT(this);
	ASSERT(track);

	// If no tracks, add as is
	if (!disk.first) {
		disk.first = track;
		disk.first->SetNext(NULL);
		return;
	}

	// Get last track
	ptr = disk.first;
	while (ptr->GetNext()) {
		ptr = ptr->GetNext();
	}

	// Add to last track
	ptr->SetNext(track);
	track->SetNext(NULL);
}

//---------------------------------------------------------------------------
//
//	Delete all tracks
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk::ClrTrack()
{
	FDITrack *track;

	ASSERT(this);

	// Delete all tracks
	while (disk.first) {
		track = disk.first->GetNext();
		delete disk.first;
		disk.first = track;
	}
}

//---------------------------------------------------------------------------
//
//	Search track
//
//---------------------------------------------------------------------------
FDITrack* FASTCALL FDIDisk::Search(int track) const
{
	FDITrack *p;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));

	// Get first track
	p = GetFirst();

	// Loop
	while (p) {
		if (p->GetTrack() == track) {
			return p;
		}
		p = p->GetNext();
	}

	// Not found
	return NULL;
}

//===========================================================================
//
//	FDI
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDI::FDI(FDD *fdd)
{
	ASSERT(fdd);

	// Initialize work
	fdi.fdd = fdd;
	fdi.disks = 0;
	fdi.first = NULL;
	fdi.disk = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDI::~FDI()
{
	ClrDisk();
}

//---------------------------------------------------------------------------
//
//	Open
//	Note for parent class:
//		Seek to current cylinder after calling
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Open(const Filepath& path, int media = 0)
{
	FDIDisk2HD *disk2hd;
	FDIDiskDIM *diskdim;
	FDIDiskD68 *diskd68;
	FDIDisk2DD *disk2dd;
	FDIDisk2HQ *disk2hq;
	FDIDiskBAD *diskbad;
	int i;
	int num;
	DWORD offset[0x10];

	ASSERT(this);
	ASSERT((media >= 0) && (media < 0x10));

	// Already open, this is wrong
	ASSERT(!GetDisk());
	ASSERT(!GetFirst());
	ASSERT(fdi.disks == 0);

	// Try as DIM file
	diskdim = new FDIDiskDIM(0, this);
	if (diskdim->Open(path, 0)) {
		AddDisk(diskdim);
		fdi.disk = diskdim;
		return TRUE;
	}
	// Failed
	delete diskdim;

	// Try as D68 file (count disks)
	num = FDIDiskD68::CheckDisks(path, offset);
	if (num > 0) {
		// D68 disk creation loop (add for disk count)
		for (i=0; i<num; i++) {
			diskd68 = new FDIDiskD68(i, this);
			if (!diskd68->Open(path, offset[i])) {
				// Failed
				delete diskd68;
				ClrDisk();
				return FALSE;
			}
			AddDisk(diskd68);
		}
		// Media select
		fdi.disk = Search(media);
		if (!fdi.disk) {
			ClrDisk();
			return FALSE;
		}
		return TRUE;
	}

	// Try as 2HD file
	disk2hd = new FDIDisk2HD(0, this);
	if (disk2hd->Open(path, 0)) {
		AddDisk(disk2hd);
		fdi.disk = disk2hd;
		return TRUE;
	}
	// Failed
	delete disk2hd;

	// Try as 2DD file
	disk2dd = new FDIDisk2DD(0, this);
	if (disk2dd->Open(path, 0)) {
		AddDisk(disk2dd);
		fdi.disk = disk2dd;
		return TRUE;
	}
	// Failed
	delete disk2dd;

	// Try as 2HQ file
	disk2hq = new FDIDisk2HQ(0, this);
	if (disk2hq->Open(path, 0)) {
		AddDisk(disk2hq);
		fdi.disk = disk2hq;
		return TRUE;
	}
	// Failed
	delete disk2hq;

	// Try as BAD file
	diskbad = new FDIDiskBAD(0, this);
	if (diskbad->Open(path, 0)) {
		AddDisk(diskbad);
		fdi.disk = diskbad;
		return TRUE;
	}
	// Failed
	delete diskbad;

	// Error
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	IDGet
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetID() const
{
	ASSERT(this);

	// If not ready, NULL
	if (!IsReady()) {
		return MAKEID('N', 'U', 'L', 'L');
	}

	// Ask disk
	return GetDisk()->GetID();
}

//---------------------------------------------------------------------------
//
//	Is multi-disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsMulti() const
{
	ASSERT(this);

	// TRUE if 2 or more disks
	if (GetDisks() >= 2) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get media number
//
//---------------------------------------------------------------------------
int FASTCALL FDI::GetMedia() const
{
	ASSERT(this);

	// If no disk, 0
	if (!GetDisk()) {
		return 0;
	}

	// Just ask disk
	return GetDisk()->GetIndex();
}

//---------------------------------------------------------------------------
//
//	Get disk name
//
//---------------------------------------------------------------------------
void FASTCALL FDI::GetName(char *buf, int index) const
{
	FDIDisk *disk;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(index >= -1);
	ASSERT(index < GetDisks());

	// When -1, means current
	if (index < 0) {
		// If not ready, empty string
		if (!IsReady()) {
			buf[0] = '\0';
			return;
		}

		// Ask current
		GetDisk()->GetName(buf);
		return;
	}

	// Has index, so search
	disk = Search(index);
	if (!disk) {
		buf[0] = '\0';
		return;
	}
	disk->GetName(buf);
}

//---------------------------------------------------------------------------
//
//	Get path
//
//---------------------------------------------------------------------------
void FASTCALL FDI::GetPath(Filepath& path) const
{
	ASSERT(this);

	// If not ready, empty string
	if (!IsReady()) {
		path.Clear();
		return;
	}

	// Ask disk
	GetDisk()->GetPath(path);
}

//---------------------------------------------------------------------------
//
//	Check ready
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsReady() const
{
	ASSERT(this);

	// TRUE if current media exists, FALSE otherwise
	if (GetDisk()) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Is write protected
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsWriteP() const
{
	// If not ready, FALSE
	if (!IsReady()) {
		return FALSE;
	}

	// Ask disk
	return GetDisk()->IsWriteP();
}

//---------------------------------------------------------------------------
//
//	Is read-only disk image
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsReadOnly() const
{
	// If not ready, FALSE
	if (!IsReady()) {
		return FALSE;
	}

	// Ask disk
	return GetDisk()->IsReadOnly();
}

//---------------------------------------------------------------------------
//
//	Set write protection
//
//---------------------------------------------------------------------------
void FASTCALL FDI::WriteP(BOOL flag)
{
	ASSERT(this);

	// If ready, instruct
	if (IsReady()) {
		GetDisk()->WriteP(flag);
	}
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDI::Seek(int c)
{
	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// If not ready, do nothing
	if (!IsReady()) {
		return;
	}

	// Notify disk
	GetDisk()->Seek(c);
}

//---------------------------------------------------------------------------
//
//	ReadID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or all valid sectors have ID CRC
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadID(DWORD *buf, BOOL mfm, int hd)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((hd == 0) || (hd == 4));

	// Determine not ready
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// Delegate to disk
	return GetDisk()->ReadID(buf, mfm, hd);
}

//---------------------------------------------------------------------------
//
//	Read sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadSector(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Determine not ready
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// Delegate to disk
	return GetDisk()->ReadSector(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	Write sector
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_NOTWRITE	Media is write protected
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_NOCYL		Found sector where C in ID does not match and is not FF during search
//		FDD_BADCYL		Found sector where C in ID does not match and is FF during search
//		FDD_IDCRC		CRC error in ID field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDI::WriteSector(const BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd, BOOL deleted)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Determine not ready
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// Delegate to disk
	return GetDisk()->WriteSector(buf, len, mfm, chrn, hd, deleted);
}

//---------------------------------------------------------------------------
//
//	Read diag
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_MAM			Unformatted at specified density
//		FDD_NODATA		Unformatted at specified density, or
//						or R does not match after searching all valid sectors
//		FDD_IDCRC		CRC error in ID field
//		FDD_DATACRC		CRC error in DATA field
//		FDD_DDAM		Is deleted sector
//
//---------------------------------------------------------------------------
int FASTCALL FDI::ReadDiag(BYTE *buf, int *len, BOOL mfm, const DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

	// Determine not ready
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// Delegate to disk
	return GetDisk()->ReadDiag(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	WriteID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_NOTWRITE	Media is write protected
//
//---------------------------------------------------------------------------
int FASTCALL FDI::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl)
{
	ASSERT(this);
	ASSERT(sc > 0);
	ASSERT((hd == 0) || (hd == 4));

	// Determine not ready
	if (!IsReady()) {
		return FDD_NOTREADY;
	}

	// Delegate to disk
	return GetDisk()->WriteID(buf, d, sc, mfm, hd, gpl);
}

//---------------------------------------------------------------------------
//
//	Get rotation position
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetRotationPos() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// Ask parent
	return GetFDD()->GetRotationPos();
}

//---------------------------------------------------------------------------
//
//	Get rotation time
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetRotationTime() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// Ask parent
	return GetFDD()->GetRotationTime();
}

//---------------------------------------------------------------------------
//
//	Get search time
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDI::GetSearch() const
{
	FDIDisk *disk;

	ASSERT(this);

	// If not ready, always 0
	disk = GetDisk();
	if (!disk) {
		return 0;
	}

	// Ask disk
	return disk->GetSearch();
}

//---------------------------------------------------------------------------
//
//	Get HD flag
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::IsHD() const
{
	ASSERT(this);
	ASSERT(GetFDD());

	// Ask parent
	return GetFDD()->IsHD();
}

//---------------------------------------------------------------------------
//
//	Add disk
//
//---------------------------------------------------------------------------
void FASTCALL FDI::AddDisk(FDIDisk *disk)
{
	FDIDisk *ptr;

	ASSERT(this);
	ASSERT(disk);

	// If no disks, add as is
	if (!fdi.first) {
		fdi.first = disk;
		fdi.first->SetNext(NULL);

		// Increment counter
		fdi.disks++;
		return;
	}

	// Get last disk
	ptr = fdi.first;
	while (ptr->GetNext()) {
		ptr = ptr->GetNext();
	}

	// Add to last disk
	ptr->SetNext(disk);
	disk->SetNext(NULL);

	// Increment counter
	fdi.disks++;
}

//---------------------------------------------------------------------------
//
//	Delete all disks
//
//---------------------------------------------------------------------------
void FASTCALL FDI::ClrDisk()
{
	FDIDisk *disk;

	ASSERT(this);

	// Delete all disks
	while (fdi.first) {
		disk = fdi.first->GetNext();
		delete fdi.first;
		fdi.first = disk;
	}

	// Counter 0
	fdi.disks = 0;
}

//---------------------------------------------------------------------------
//
//	Search disk
//
//---------------------------------------------------------------------------
FDIDisk* FASTCALL FDI::Search(int index) const
{
	FDIDisk *disk;

	ASSERT(this);
	ASSERT(index >= 0);
	ASSERT(index < GetDisks());

	// Get first disk
	disk = GetFirst();

	// Comparison loop
	while (disk) {
		if (disk->GetIndex() == index) {
			return disk;
		}

		// Next
		disk = disk->GetNext();
	}

	// Not found
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Save(Fileio *fio, int ver)
{
	BOOL ready;
	FDIDisk *disk;
	Filepath path;
	int i;
	int disks;
	int media;

	ASSERT(this);
	ASSERT(fio);

	// Write ready flag
	ready = IsReady();
	if (!fio->Write(&ready, sizeof(ready))) {
		return FALSE;
	}

	// If not ready, end
	if (!ready) {
		return TRUE;
	}

	// Flush all media
	disks = GetDisks();
	for (i=0; i<disks; i++) {
		disk = Search(i);
		ASSERT(disk);
		if (!disk->Flush()) {
			return FALSE;
		}
	}

	// Write media
	media = GetMedia();
	if (!fio->Write(&media, sizeof(media))) {
		return FALSE;
	}

	// Write path
	GetPath(path);
	if (!path.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDI::Load(Fileio *fio, int ver, BOOL *ready, int *media, Filepath& path)
{
	ASSERT(this);
	ASSERT(fio);
	ASSERT(ready);
	ASSERT(media);
	ASSERT(!IsReady());
	ASSERT(GetDisks() == 0);

	// Read ready flag
	if (!fio->Read(ready, sizeof(BOOL))) {
		return FALSE;
	}

	// If not ready, end
	if (!(*ready)) {
		return TRUE;
	}

	// Read media
	if (!fio->Read(media, sizeof(int))) {
		return FALSE;
	}

	// Read path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Adjust (special)
//
//---------------------------------------------------------------------------
void FASTCALL FDI::Adjust()
{
	FDIDisk *disk;
	FDIDiskD68 *disk68;

	ASSERT(this);

	// Only for multiple images
	if (!IsMulti()) {
		return;
	}

	disk = GetFirst();
	while (disk) {
		// Only for D68
		if (disk->GetID() == MAKEID('D', '6', '8', ' ')) {
			disk68 = (FDIDiskD68*)disk;
			disk68->AdjustOffset();
		}

		disk = disk->GetNext();
	}
}

//===========================================================================
//
//	FDI Track(2HD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrack2HD::FDITrack2HD(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2HD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset & 0x1fff) == 0);
	ASSERT(offset < 0x134000);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// Determine C, H, N
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 3;

	// Open for reading
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Seek
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// Loop
	for (i=0; i<8; i++) {
		// Read data
		if (!fio.Read(buf, sizeof(buf))) {
			// Delete partially added sectors
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// Create sector
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Close
	fio.Close();

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDI Disk(2HD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDisk2HD::FDIDisk2HD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('2', 'H', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDisk2HD::~FDIDisk2HD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2HD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Verify file size is 1261568
	size = fio.GetFileSize();
	if (size != 1261568) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Store path, offset
	disk.path = path;
	disk.offset = offset;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create tracks (0-76 cylinders, 77*2 tracks)
	for (i=0; i<154; i++) {
		track = new FDITrack2HD(this, i);
		AddTrack(track);
	}

	// End
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2HD::Seek(int c)
{
	int i;
	FDITrack2HD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// Write track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = (FDITrack2HD*)GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		track->Save(disk.path, offset);
	}

	// Allow c up to 75. If out of range, set head[i]=NULL
	if ((c < 0) || (c > 76)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrack2HD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Load
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	Create new disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2HD *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// Physical format only allows 2HD
	if (opt->phyfmt != FDI_2HD) {
		return FALSE;
	}

	// Try to create file
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Store path, offset
	disk.path = path;
	disk.offset = 0;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create tracks and physical format only for 0-153
	for (i=0; i<154; i++) {
		track = new FDITrack2HD(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// Logical format
	FDIDisk::Create(path, opt);

	// WriteLoop
	offset = 0;
	for (i=0; i<154; i++) {
		// Get track
		track = (FDITrack2HD*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// Write
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// Next
		offset += (0x400 * 8);
	}

	// Success
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HD::Flush()
{
	int i;
	DWORD offset;
	FDITrack *track;

	ASSERT(this);

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDI Track(DIM)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrackDIM::FDITrackDIM(FDIDisk *disk, int track, int type) : FDITrack(disk, track)
{
	// Determine dim_mfm, dim_secs, dim_n based on type
	switch (type) {
		// 2HD (N=3, 8 sectors)
		case 0:
			dim_mfm = TRUE;
			dim_secs = 8;
			dim_n = 3;
			break;

		// 2HS (N=3, 9 sectors)
		case 1:
			dim_mfm = TRUE;
			dim_secs = 9;
			dim_n = 3;
			break;

		// 2HC (N=2, 15 sectors)
		case 2:
			dim_mfm = TRUE;
			dim_secs = 15;
			dim_n = 2;
			break;

		// 2HDE (N=3, 9 sectors)
		case 3:
			dim_mfm = TRUE;
			dim_secs = 9;
			dim_n = 3;
			break;

		// 2HQ (N=2, 18 sectors)
		case 9:
			dim_mfm = TRUE;
			dim_secs = 18;
			dim_n = 2;
			break;

		// N88-BASIC (26 sectors, track 0 only single density)
		case 17:
			dim_secs = 26;
			if (track == 0) {
				dim_mfm = FALSE;
				dim_n = 0;
			}
			else {
				dim_mfm = TRUE;
				dim_n = 1;
			}
			break;

		// Other
		default:
			ASSERT(FALSE);
			break;
	}

	dim_type = type;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackDIM::Load(const Filepath& path, DWORD offset, BOOL load)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	int num;
	int len;
	int gap;
	FDISector *sector;

	ASSERT(this);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// Determine C, H, N
	chrn[0] = GetTrack() >> 1;
	chrn[3] = GetDIMN();

	// Open for reading
	if (load) {
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Seek
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
	}

	// Prepare for load
	num = GetDIMSectors();
	len = 1 << (GetDIMN() + 7);
	ASSERT(len <= sizeof(buf));
	switch (GetDIMN()) {
		case 0:
			gap = 0x14;
			break;
		case 1:
			gap = 0x33;
			break;
		case 2:
			gap = 0x54;
			break;
		case 3:
			if (GetDIMSectors() == 8) {
				gap = 0x74;
			}
			else {
				gap = 0x39;
			}
			break;
		default:
			ASSERT(FALSE);
			fio.Close();
			return FALSE;
	}

	// Create initial data (if load==FALSE)
	memset(buf, 0xe5, len);

	// Loop
	for (i=0; i<num; i++) {
		// Read data
		if (load) {
			if (!fio.Read(buf, len)) {
				// Delete partially added sectors
				ClrSector();
				fio.Close();
				return FALSE;
			}
		}

		// Determine H and R (special cases for 2HS, 2HDE)
		chrn[1] = GetTrack() & 1;
		chrn[2] = i + 1;
		if (dim_type == 1) {
			chrn[2] = i + 10;
			if ((GetTrack() == 0) && (i == 0)) {
				chrn[2] = 1;
			}
		}
		if (dim_type == 3) {
			chrn[1] = chrn[1] + 0x80;
			if ((GetTrack() == 0) && (i == 0)) {
				chrn[1] = 0;
			}
		}

		// Create sector
		sector = new FDISector(IsDIMMFM(), chrn);
		sector->Load(buf, len, gap, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Close
	if (load) {
		fio.Close();
	}

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDI Disk(DIM)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDiskDIM::FDIDiskDIM(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('D', 'I', 'M', ' ');

	// Clear header
	memset(dim_hdr, 0, sizeof(dim_hdr));

	// No load
	dim_load = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDiskDIM::~FDIDiskDIM()
{
	// Write if loaded
	if (dim_load) {
		Save();
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrackDIM *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Verify file size is at least 256
	size = fio.GetFileSize();
	if (size < 0x100) {
		fio.Close();
		return FALSE;
	}

	// Read header, check recognition string
	fio.Read(dim_hdr, sizeof(dim_hdr));
	fio.Close();
	if (strcmp((char*)&dim_hdr[171], "DIFC HEADER  ") != 0) {
		return FALSE;
	}

	// Store path + offset
	disk.path = path;
	disk.offset = offset;

	// Is there a comment
	if (dim_hdr[0xc2] != '\0') {
		// Use comment as disk name (truncate to 60 chars)
		dim_hdr[0xc2 + 60] = '\0';
		strcpy(disk.name, (char*)&dim_hdr[0xc2]);
	}
	else {
		// Disk name is filename + extension
		strcpy(disk.name, path.GetShort());
	}

	// Create tracks (0-81 cylinders, 82*2 tracks)
	for (i=0; i<164; i++) {
		track = new FDITrackDIM(this, i, dim_hdr[0]);
		AddTrack(track);
	}

	// Set flag, end
	dim_load = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskDIM::Seek(int c)
{
	int i;
	FDITrackDIM *track;
	DWORD offset;
	BOOL flag;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));
	ASSERT(dim_load);

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrackDIM*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Check map, load if valid track
		flag = FALSE;
		offset = 0;
		if (GetDIMMap(c * 2 + i)) {
			// Calculate offset
			offset = GetDIMOffset(c * 2 + i);
			flag = TRUE;
		}

		// Load or create
		track->Load(disk.path, offset, flag);
	}
}

//---------------------------------------------------------------------------
//
//	Get DIM track map
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::GetDIMMap(int track) const
{
	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(dim_load);

	if (dim_hdr[track + 1] != 0) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Get DIM track offset
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDiskDIM::GetDIMOffset(int track) const
{
	int i;
	DWORD offset;
	int length;
	FDITrackDIM *dim;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(dim_load);

	// Base is 256
	offset = 0x100;

	// Sum up to previous sector
	for (i=0; i<track; i++) {
		// Sum if valid track
		if (GetDIMMap(i)) {
			dim = (FDITrackDIM*)Search(track);
			ASSERT(dim);
			length = 1 << (dim->GetDIMN() + 7);
			length *= dim->GetDIMSectors();
			offset += length;
		}
	}

	return offset;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Save()
{
	BOOL changed;
	int i;
	FDITrackDIM *track;
	DWORD offset;
	Fileio fio;
	DWORD total;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(dim_load);

	// First check if any tracks outside map are changed
	changed = FALSE;
	for (i=0; i<164; i++) {
		if (!GetDIMMap(i)) {
			// Track not in map. Any changes?
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			if (track->IsChanged()) {
				changed = TRUE;
			}
		}
	}

	// If only map changed, handle individually
	if (!changed) {
		for (i=0; i<164; i++) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			if (track->IsChanged()) {
				// Mapped track
				ASSERT(GetDIMMap(i));
				offset = GetDIMOffset(i);
				if (!track->Save(disk.path, offset)) {
					return FALSE;
				}
			}
		}
		// End all tracks
		return TRUE;
	}

	// Load all mapped tracks
	for (i=0; i<164; i++) {
		if (GetDIMMap(i)) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			offset = GetDIMOffset(i);
			if (!track->Load(disk.path, offset, TRUE)) {
				return FALSE;
			}
		}
	}

	// Map newly added unmapped tracks. Also get total size
	total = 0;
	for (i=0; i<164; i++) {
		track = (FDITrackDIM*)Search(i);
		ASSERT(track);
		if (!GetDIMMap(i)) {
			if (track->IsChanged()) {
				// Add to map
				dim_hdr[i + 1] = 0x01;
				total += track->GetTotalLength();
			}
		}
		else {
			// Already mapped
			total += track->GetTotalLength();
		}
	}

	// Set OverTrack flag if 2HD uses track 154 or later
	dim_hdr[0xff] = 0x00;
	if (dim_hdr[0] == 0x00) {
		for (i=154; i<164; i++) {
			if (GetDIMMap(i)) {
				dim_hdr[0xff] = 0x01;
			}
		}
	}

	// New save (create size)
	if (!fio.Open(disk.path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(dim_hdr, sizeof(dim_hdr))) {
		fio.Close();
		return FALSE;
	}

	// After 256-byte header, save E5 data first
	try {
		ptr = new BYTE[total];
	}
	catch (...) {
		fio.Close();
		return FALSE;
	}
	if (!ptr) {
		fio.Close();
		return FALSE;
	}
	memset(ptr, 0xe5, total);
	if (!fio.Write(ptr, total)) {
		fio.Close();
		delete[] ptr;
		return FALSE;
	}
	delete[] ptr;

	// Write all (keep file open)
	for (i=0; i<164; i++) {
		if (GetDIMMap(i)) {
			track = (FDITrackDIM*)Search(i);
			ASSERT(track);
			offset = GetDIMOffset(i);
			track->ForceChanged();
			if (!track->Save(&fio, offset)) {
				return FALSE;
			}
		}
	}

	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Create new disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrackDIM *track;
	Fileio fio;
	static BYTE iocsdata[] = {
		0x04, 0x21, 0x03, 0x22, 0x01, 0x00, 0x00, 0x00
	};

	ASSERT(this);
	ASSERT(opt);

	// Clear header
	memset(dim_hdr, 0, sizeof(dim_hdr));

	// Check format and write type
	switch (opt->phyfmt) {
		// 2HD (including over-track usage)
		case FDI_2HD:
		case FDI_2HDA:
			dim_hdr[0] = 0x00;
			break;

		// 2HS
		case FDI_2HS:
			dim_hdr[0] = 0x01;
			break;

		// 2HC
		case FDI_2HC:
			dim_hdr[0] = 0x02;
			break;

		// 2HDE(68)
		case FDI_2HDE:
			dim_hdr[0] = 0x03;
			break;

		// 2HQ
		case FDI_2HQ:
			dim_hdr[0] = 0x09;
			break;

		// N88-BASIC
		case FDI_N88B:
			dim_hdr[0] = 0x11;
			break;

		// Unsupported physical format
		default:
			return FALSE;
	}

	// Remaining header (date is 2001-03-22 00:00:00; XM6 development start date)
	strcpy((char*)&dim_hdr[0xab], "DIFC HEADER  ");
	dim_hdr[0xfe] = 0x19;
	if (opt->phyfmt == FDI_2HDA) {
		dim_hdr[0xff] = 0x01;
	}
	memcpy(&dim_hdr[0xba], iocsdata, 8);
	ASSERT(strlen(opt->name) < 60);
	strcpy((char*)&dim_hdr[0xc2], opt->name);

	// Write header
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(&dim_hdr[0], sizeof(dim_hdr))) {
		return FALSE;
	}
	fio.Close();

	// Set flag
	disk.writep = FALSE;
	disk.readonly = FALSE;
	dim_load = TRUE;

	// Store path + offset
	disk.path = path;
	disk.offset = 0;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create track and physical format
	for (i=0; i<164; i++) {
		track = new FDITrackDIM(this, i, dim_hdr[0x00]);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// Logical format
	FDIDisk::Create(path, opt);

	// Save
	if (!Save()) {
		return FALSE;
	}

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskDIM::Flush()
{
	ASSERT(this);

	// Write if loaded
	if (dim_load) {
		return Save();
	}

	// Not loaded
	return TRUE;
}

//===========================================================================
//
//	FDI Track(D68)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrackD68::FDITrackD68(FDIDisk *disk, int track, BOOL hd) : FDITrack(disk, track, hd)
{
	// No format change
	d68_format = FALSE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE header[0x10];
	DWORD chrn[4];
	BYTE buf[0x2000];
	BOOL mfm;
	int len;
	int i;
	int num;
	int gap;
	int stat;
	FDISector *sector;
	BYTE *ptr;
	const int *table;

	ASSERT(this);
	ASSERT(offset > 0);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// Open and seek
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	if (!fio.Seek(offset)) {
		return FALSE;
	}

	// Assume at least one sector exists (offset != 0)
	i = 0;
	num = 1;
	while (i < num) {
		// Read header
		if (!fio.Read(header, sizeof(header))) {
			break;
		}

		// Get sector count on first pass
		if (i == 0) {
			ptr = &header[0x04];
			num = (int)ptr[1];
			num <<= 8;
			num |= (int)ptr[0];
		}

		// MFM
		mfm = TRUE;
		if (header[0x06] != 0) {
			mfm = FALSE;
		}

		// Length
		ptr = &header[0x0e];
		len = (int)ptr[1];
		len <<= 8;
		len |= (int)ptr[0];

		// GAP3 (D68 file has no info, use common pattern)
		gap = 0x12;
		table = &Gap3Table[0];
		while (table[0] != 0) {
			// Search GAP table
			if ((table[0] == num) && (table[1] == (int)header[3])) {
				gap = table[2];
				break;
			}
			table += 3;
		}

		// Status including deleted sector
		stat = FDD_NOERROR;
		if (header[0x07] != 0) {
			stat |= FDD_DDAM;
		}
		if ((header[0x08] & 0xf0) == 0xa0) {
			stat |= FDD_DATACRC;
		}

		// Load data to buffer
		if (sizeof(buf) < len) {
			break;
		}
		if (!fio.Read(buf, len)) {
			break;
		}

		// Create sector
		chrn[0] = (DWORD)header[0];
		chrn[1] = (DWORD)header[1];
		chrn[2] = (DWORD)header[2];
		chrn[3] = (DWORD)header[3];
		sector = new FDISector(mfm, chrn);
		sector->Load(buf, len, gap, stat);

		// Next
		AddSector(sector);
		i++;
	}

	fio.Close();

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Save(const Filepath& path, DWORD offset)
{
	FDISector *sector;
	BYTE header[0x10];
	DWORD chrn[4];
	Fileio fio;
	int secs;
	int len;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(offset > 0);

	// Initialize sector
	sector = GetFirst();

	while (sector) {
		if (!sector->IsChanged()) {
			// Skip if not changed
			offset += 0x10;
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// Changed. Create header
		memset(header, 0, sizeof(header));
		sector->GetCHRN(chrn);
		header[0] = (BYTE)chrn[0];
		header[1] = (BYTE)chrn[1];
		header[2] = (BYTE)chrn[2];
		header[3] = (BYTE)chrn[3];
		secs = GetAllSectors();
		ptr = &header[0x04];
		ptr[1] = (BYTE)(secs >> 8);
		ptr[0] = (BYTE)secs;
		if (!sector->IsMFM()) {
			header[0x06] = 0x40;
		}
		if (sector->GetError() & FDD_DDAM) {
			header[0x07] = 0x10;
		}
		if (sector->GetError() & FDD_IDCRC) {
			header[0x08] = 0xa0;
		}
		if (sector->GetError() & FDD_DATACRC) {
			header[0x08] = 0xa0;
		}
		ptr = &header[0x0e];
		len = sector->GetLength();
		ptr[1] = (BYTE)(len >> 8);
		ptr[0] = (BYTE)len;

		// Write
		if (!fio.IsValid()) {
			// Open file if first time
			if (!fio.Open(path, Fileio::ReadWrite)) {
				return FALSE;
			}
		}
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Write(header, sizeof(header))) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// Write complete
		sector->ClrChanged();

		// Next
		offset += 0x10;
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// Close file if valid
	if (fio.IsValid()) {
		fio.Close();
	}

	// Also clear format flag
	d68_format = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackD68::Save(Fileio *fio, DWORD offset)
{
	FDISector *sector;
	BYTE header[0x10];
	DWORD chrn[4];
	int secs;
	int len;
	BYTE *ptr;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(offset > 0);

	// Initialize sector
	sector = GetFirst();

	while (sector) {
		if (!sector->IsChanged()) {
			// Skip if not changed
			offset += 0x10;
			offset += sector->GetLength();
			sector = sector->GetNext();
			continue;
		}

		// Changed. Create header
		memset(header, 0, sizeof(header));
		sector->GetCHRN(chrn);
		header[0] = (BYTE)chrn[0];
		header[1] = (BYTE)chrn[1];
		header[2] = (BYTE)chrn[2];
		header[3] = (BYTE)chrn[3];
		secs = GetAllSectors();
		ptr = &header[0x04];
		ptr[1] = (BYTE)(secs >> 8);
		ptr[0] = (BYTE)secs;
		if (!sector->IsMFM()) {
			header[0x06] = 0x40;
		}
		if (sector->GetError() & FDD_DDAM) {
			header[0x07] = 0x10;
		}
		if (sector->GetError() & FDD_IDCRC) {
			header[0x08] = 0xa0;
		}
		if (sector->GetError() & FDD_DATACRC) {
			header[0x08] = 0xa0;
		}
		ptr = &header[0x0e];
		len = sector->GetLength();
		ptr[1] = (BYTE)(len >> 8);
		ptr[0] = (BYTE)len;

		// Write
		fio->Seek(offset);
		if (!fio->Write(header, sizeof(header))) {
			return FALSE;
		}
		if (!fio->Write(sector->GetSector(), sector->GetLength())) {
			return FALSE;
		}

		// Write complete
		sector->ClrChanged();

		// Next
		offset += 0x10;
		offset += sector->GetLength();
		sector = sector->GetNext();
	}

	// Also clear format flag
	d68_format = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	WriteID
//	Note: Returns the following status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTWRITE	Write protected
//
//---------------------------------------------------------------------------
int FASTCALL FDITrackD68::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int gpl)
{
	int stat;
	FDISector *sector;
	DWORD pos;
	int i;
	BYTE fillbuf[0x2000];
	DWORD chrn[4];

	ASSERT(this);
	ASSERT(sc > 0);

	// Call original (write protection check already done in FDIDisk)
	stat = FDITrack::WriteID(buf, d, sc, mfm, gpl);
	if (stat == FDD_NOERROR) {
		// Format success (same physical format as before)
		return stat;
	}

	// Different format
	d68_format = TRUE;

	// Set time (until index)
	pos = GetDisk()->GetRotationTime();
	pos -= GetDisk()->GetRotationPos();
	GetDisk()->SetSearch(pos);

	// End here if no buf given
	if (!buf) {
		return FDD_NOERROR;
	}

	// Clear sector
	ClrSector();
	memset(fillbuf, d, sizeof(fillbuf));

	// Create sectors in order
	for (i=0; i<sc; i++) {
		// Length >= 7 is unformatted
		if (buf[i * 4 + 3] >= 0x07) {
			ClrSector();
			return FDD_NOERROR;
		}

		// Create sector
		chrn[0] = (DWORD)buf[i * 4 + 0];
		chrn[1] = (DWORD)buf[i * 4 + 1];
		chrn[2] = (DWORD)buf[i * 4 + 2];
		chrn[3] = (DWORD)buf[i * 4 + 3];
		sector = new FDISector(mfm, chrn);
		sector->Load(fillbuf, 1 << (buf[i * 4 + 3] + 7), gpl, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Calculate position
	CalcPos();

	return FDD_NOERROR;
}

//---------------------------------------------------------------------------
//
//	Get length in D68 format
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDITrackD68::GetD68Length() const
{
	DWORD length;
	FDISector *sector;

	ASSERT(this);

	// Initialize
	length = 0;
	sector = GetFirst();

	// Loop
	while (sector) {
		length += 0x10;
		length += sector->GetLength();
		sector = sector->GetNext();
	}

	return length;
}

//---------------------------------------------------------------------------
//
//	GAP3 table
//
//---------------------------------------------------------------------------
const int FDITrackD68::Gap3Table[] = {
	// SEC, N, GAP3
	 8, 3, 0x74,						// 2HD
	 9, 3, 0x39,						// 2HS, 2HDE
	15, 2, 0x54,						// 2HC
	18, 2, 0x54,						// 2HQ
	26, 1, 0x33,						// OS-9/68000, N88-BASIC
	26, 0, 0x1a,						// N88-BASIC
	 9, 2, 0x54,						// 2DD(720KB)
	 8, 2, 0x54,						// 2DD(640KB)
	 5, 3, 0x74,						// 2D(Falcom)
	16, 1, 0x33,						// 2D(BASIC)
	 0, 0, 0
};

//===========================================================================
//
//	FDI Disk(D68)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDiskD68::FDIDiskD68(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('D', '6', '8', ' ');

	// Clear header
	memset(d68_hdr, 0, sizeof(d68_hdr));

	// No load
	d68_load = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDiskD68::~FDIDiskD68()
{
	// Write if loaded
	if (d68_load) {
		Save();
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	int i;
	FDITrackD68 *track;
	BOOL hd;

	ASSERT(this);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Including seek and read header
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}
	if (!fio.Read(d68_hdr, sizeof(d68_hdr))) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Store path + offset
	disk.path = path;
	disk.offset = offset;

	// Disk name (truncate to 16 chars)
	d68_hdr[0x10] = 0;
	strcpy(disk.name, (char*)d68_hdr);
	// But for single disk, if NULL or default, use filename + extension
	if (!GetFDI()->IsMulti()) {
		if (strcmp(disk.name, "Default") == 0) {
			strcpy(disk.name, path.GetShort());
		}
		if (strlen(disk.name) == 0) {
			strcpy(disk.name, path.GetShort());
		}
	}

	// Write protected
	if (d68_hdr[0x1a] != 0) {
		disk.writep = TRUE;
	}

	// HD flag
	switch (d68_hdr[0x1b]) {
		// 2D,2DD
		case 0x00:
		case 0x10:
			hd = FALSE;
			break;
		// 2HD
		case 0x20:
			hd = TRUE;
			break;
		default:
			return FALSE;
	}

	// Create tracks (0-81 cylinders, 82*2 tracks)
	for (i=0; i<164; i++) {
		track = new FDITrackD68(this, i, hd);
		AddTrack(track);
	}

	// Set flag, end
	d68_load = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskD68::Seek(int c)
{
	int i;
	FDITrackD68 *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));
	ASSERT(d68_load);

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrackD68*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Get offset, load if valid track
		if (d68_hdr[0x1b] == 0x00) {
			// 2D
			if (c == 0) {
				offset = GetD68Offset(i);
			}
			else {
				if (c & 1) {
					// 1,3,5... odd cylinders are 1,2,3 cylinders respectively
					offset = GetD68Offset(c + 1 + i);
				}
				else {
					// 2,4,6... even cylinders are unformatted
					offset = 0;
				}
			}
		}
		else {
			// 2DD,2HD
			offset = GetD68Offset(c * 2 + i);
		}
		if (offset > 0) {
			track->Load(disk.path, disk.offset + offset);
		}
	}
}

//---------------------------------------------------------------------------
//
//	Get D68 track offset
//	Note: Invalid track is 0
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDIDiskD68::GetD68Offset(int track) const
{
	DWORD offset;
	const BYTE *ptr;

	ASSERT(this);
	ASSERT((track >= 0) && (track <= 163));
	ASSERT(d68_load);

	// Get pointer
	ptr = &d68_hdr[0x20 + (track << 2)];

	// Get offset (little endian)
	offset = (DWORD)ptr[2];
	offset <<= 8;
	offset |= (DWORD)ptr[1];
	offset <<= 8;
	offset |= (DWORD)ptr[0];

	return offset;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Save()
{
	DWORD diskoff[16 + 1];
	BOOL format;
	int i;
	FDITrackD68 *track;
	DWORD offset;
	DWORD length;
	BYTE *fileptr;
	DWORD filelen;
	Fileio fio;
	BYTE *ptr;

	ASSERT(this);

	// Do nothing if not loaded
	if (!d68_load) {
		return TRUE;
	}

	// Re-get offset
	memset(diskoff, 0, sizeof(diskoff));
	CheckDisks(disk.path, diskoff);
	disk.offset = diskoff[disk.index];

	// Check if format has changed
	format = FALSE;
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		if (track->IsFormated()) {
			format = TRUE;
		}
	}

	// If not changed, save per track
	if (!format) {
		for (i=0; i<164; i++) {
			track = (FDITrackD68*)Search(i);
			ASSERT(track);
			offset = GetD68Offset(i);
			if (offset > 0) {
				if (!track->Save(disk.path, disk.offset + offset)) {
					return FALSE;
				}
			}
		}

		// If write protection differs, save only that
		i = 0;
		if (IsWriteP()) {
			i = 0x10;
		}
		if (d68_hdr[0x1a] != (BYTE)i) {
			d68_hdr[0x1a] = (BYTE)i;
			if (!fio.Open(disk.path, Fileio::ReadWrite)) {
				return FALSE;
			}
			if (!fio.Seek(diskoff[disk.index])) {
				fio.Close();
				return FALSE;
			}
			if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
				fio.Close();
				return FALSE;
			}
			fio.Close();
		}
		return TRUE;
	}

	// Load entire file into memory
	if (!fio.Open(disk.path, Fileio::ReadOnly)) {
		return FALSE;
	}
	filelen = fio.GetFileSize();
	try {
		fileptr = new BYTE[filelen];
	}
	catch (...) {
		fio.Close();
		return FALSE;
	}
	if (!fileptr) {
		fio.Close();
		return FALSE;
	}
	if (!fio.Read(fileptr, filelen)) {
		delete[] fileptr;
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Rebuild header
	offset = sizeof(d68_hdr);
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		length = track->GetD68Length();
		ptr = &d68_hdr[0x20 + (i << 2)];
		if (length == 0) {
			memset(ptr, 0, 4);
		}
		else {
			ptr[3] = (BYTE)(offset >> 24);
			ptr[2] = (BYTE)(offset >> 16);
			ptr[1] = (BYTE)(offset >> 8);
			ptr[0] = (BYTE)offset;
			offset += length;
		}
	}
	d68_hdr[0x1a] = 0;
	if (IsWriteP()) {
		d68_hdr[0x1a] = 0x10;
	}
	ptr = &d68_hdr[0x1c];
	ptr[3] = (BYTE)(offset >> 24);
	ptr[2] = (BYTE)(offset >> 16);
	ptr[1] = (BYTE)(offset >> 8);
	ptr[0] = (BYTE)offset;

	// Save front of file
	if (!fio.Open(disk.path, Fileio::WriteOnly)) {
		delete[] fileptr;
		return FALSE;
	}
	if (diskoff[disk.index] != 0) {
		if (!fio.Write(fileptr, diskoff[disk.index])) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
	}

	// Save header
	if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
		delete[] fileptr;
		fio.Close();
		return FALSE;
	}
	offset -= sizeof(d68_hdr);

	// Create size portion
	while (offset > 0) {
		// If write completes in one pass
		if (offset < filelen) {
			if (!fio.Write(fileptr, offset)) {
				delete[] fileptr;
				fio.Close();
				return FALSE;
			}
			break;
		}

		// If multiple passes needed
		if (!fio.Write(fileptr, filelen)) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
		offset -= filelen;
	}

	// Save rest of file
	if (diskoff[disk.index + 1] != 0) {
		ASSERT(filelen >= diskoff[disk.index + 1]);
		if (!fio.Write(&fileptr[ diskoff[disk.index + 1] ],
				filelen - diskoff[disk.index + 1])) {
			delete[] fileptr;
			fio.Close();
			return FALSE;
		}
	}
	delete[] fileptr;

	// Save all with Length != 0 (force change, keep file open)
	for (i=0; i<164; i++) {
		track = (FDITrackD68*)Search(i);
		ASSERT(track);
		length = track->GetD68Length();
		if (length != 0) {
			offset = GetD68Offset(i);
			ASSERT(offset != 0);
			track->ForceChanged();
			if (!track->Save(&fio, disk.offset + offset)) {
				return FALSE;
			}
		}
	}

	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Create new disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Create(const Filepath& path, const option_t *opt)
{
	Fileio fio;
	int i;
	FDITrackD68 *track;
	BOOL hd;

	ASSERT(this);
	ASSERT(opt);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Create header
	memset(&d68_hdr, 0, sizeof(d68_hdr));
	ASSERT(strlen(opt->name) <= 16);
	strcpy((char*)d68_hdr, opt->name);
	if (opt->phyfmt == FDI_2DD) {
		hd = FALSE;
		d68_hdr[0x1b] = 0x10;
	}
	else {
		hd = TRUE;
		d68_hdr[0x1b] = 0x20;
	}

	// Write header
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}
	if (!fio.Write(d68_hdr, sizeof(d68_hdr))) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Path, disk name, offset
	disk.path = path;
	strcpy(disk.name, opt->name);
	disk.offset = 0;

	// Create tracks (0-81 cylinders, 82*2 tracks)
	for (i=0; i<164; i++) {
		track = new FDITrackD68(this, i, hd);
		track->Create(opt->phyfmt);
		track->ForceFormat();
		AddTrack(track);
	}

	// Set flag
	disk.writep = FALSE;
	disk.readonly = FALSE;
	d68_load = TRUE;

	// Logical format
	FDIDisk::Create(path, opt);

	// Save
	if (!Save()) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	D68 file inspection
//
//---------------------------------------------------------------------------
int FASTCALL FDIDiskD68::CheckDisks(const Filepath& path, DWORD *offbuf)
{
	Fileio fio;
	DWORD fsize;
	DWORD dsize;
	DWORD base;
	DWORD prev;
	DWORD offset;
	int disks;
	BYTE header[0x2b0];
	BYTE *ptr;
	int i;

	ASSERT(offbuf);

	// Initialize
	disks = 0;
	base = 0;

	// Get file size
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return 0;
	}
	fsize = fio.GetFileSize();
	fio.Close();
	if (fsize < sizeof(header)) {
		return 0;
	}

	// Disk loop
	while (disks < 16) {
		// End if size over
		if (base >= fsize) {
			break;
		}

		// Read header
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return 0;
		}
		if (!fio.Seek(base)) {
			fio.Close();
			break;
		}
		if (!fio.Read(header, sizeof(header))) {
			fio.Close();
			break;
		}
		fio.Close();

		// Check density
		switch (header[0x1b]) {
			case 0x00:
			case 0x10:
			case 0x20:
				break;
			default:
				return 0;
		}

		// Get this disk size (limited to 0x200 or more, 1.92MB or less)
		ptr = &header[0x1c];
		dsize = (DWORD)ptr[3];
		dsize <<= 8;
		dsize |= (DWORD)ptr[2];
		dsize <<= 8;
		dsize |= (DWORD)ptr[1];
		dsize <<= 8;
		dsize |= (DWORD)ptr[0];

		if ((dsize + base) > fsize) {
			return 0;
		}
		if (dsize < 0x200) {
			return 0;
		}
		if (dsize > 1920 * 1024) {
			return 0;
		}

		// Check offset. Must not exceed dsize and be monotonically increasing
		prev = 0;
		for (i=0; i<164; i++) {
			// For 2D image, do not check 84 or more tracks (strange values may be written)
			if (header[0x1b] == 0x00) {
				if (i >= 84) {
					break;
				}
			}

			// Get this track offset
			ptr = &header[0x20 + (i << 2)];
			offset = (DWORD)ptr[3];
			offset <<= 8;
			offset |= (DWORD)ptr[2];
			offset <<= 8;
			offset |= (DWORD)ptr[1];
			offset <<= 8;
			offset |= (DWORD)ptr[0];

			// Error if offset not divisible by 0x10
			if (offset & 0x0f) {
				return 0;
			}

			// 0 indicates that track is unformatted
			if (offset != 0) {
				// If not 0
				if (prev == 0) {
					// First track must start with 2X0
					if ((offset & 0xffffff0f) != 0x200) {
						return 0;
					}
				}
				else {
					// Monotonically increasing
					if (offset <= prev) {
						return 0;
					}
					// Must not exceed disk size
					if (offset > dsize) {
						return 0;
					}
				}
				prev = offset;
			}
		}

		// Increment count, register in buffer, next
		offbuf[disks] = base;
		disks++;
		base += dsize;
	}

	return disks;
}

//---------------------------------------------------------------------------
//
//	Update offset
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskD68::AdjustOffset()
{
	DWORD offset[0x10];

	ASSERT(this);

	memset(offset, 0, sizeof(offset));
	CheckDisks(disk.path, offset);
	disk.offset = offset[disk.index];
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskD68::Flush()
{
	ASSERT(this);

	// Write if loaded
	if (d68_load) {
		return Save();
	}

	// Not loaded
	return TRUE;
}

//===========================================================================
//
//	FDI Track(BAD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrackBAD::FDITrackBAD(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);

	// File valid sector count 0
	bad_secs = 0;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackBAD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x400];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// File valid sector count 0
	bad_secs = 0;

	// Determine C, H, N
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 3;

	// Open for reading
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Seek (failure is OK)
	if (!fio.Seek(offset)) {
		// Seek failed, fill with E5
		memset(buf, 0xe5, sizeof(buf));

		// Sector loop
		for (i=0; i<8; i++) {
			chrn[2] = i + 1;
			sector = new FDISector(TRUE, chrn);
			sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);
			AddSector(sector);
		}

		// CloseÅAInitializeOK
		fio.Close();
		trk.init = TRUE;
		return TRUE;
	}

	// Loop
	for (i=0; i<8; i++) {
		// Fill buffer with E5 each time
		memset(buf, 0xe5, sizeof(buf));

		// Read data (failure is OK)
		if (fio.Read(buf, sizeof(buf))) {
			// Increment file valid sector (0-8)
			bad_secs++;
		}

		// Create sector
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x74, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Close
	fio.Close();

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrackBAD::Save(const Filepath& path, DWORD offset)
{
	Fileio fio;
	FDISector *sector;
	BOOL changed;
	int index;

	ASSERT(this);

	// No need to write if not initialized
	if (!IsInit()) {
		return TRUE;
	}

	// Check sectors for any that are written
	sector = GetFirst();
	changed = FALSE;
	while (sector) {
		if (sector->IsChanged()) {
			changed = TRUE;
		}
		sector = sector->GetNext();
	}

	// Do nothing if none are written
	if (!changed) {
		return TRUE;
	}

	// Open file
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// Loop
	sector = GetFirst();
	index = 1;
	while (sector) {
		// If not changed, go to next
		if (!sector->IsChanged()) {
			offset += sector->GetLength();
			sector = sector->GetNext();
			index++;
			continue;
		}

		// Is within valid range
		if (index > bad_secs) {
			// Exceeds file, dummy processing
			sector->ClrChanged();
			offset += sector->GetLength();
			sector = sector->GetNext();
			index++;
			continue;
		}

		// Seek
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}

		// Write
		if (!fio.Write(sector->GetSector(), sector->GetLength())) {
			fio.Close();
			return FALSE;
		}

		// Clear flag
		sector->ClrChanged();

		// Next
		offset += sector->GetLength();
		sector = sector->GetNext();
		index++;
	}

	// End
	fio.Close();
	return TRUE;
}

//===========================================================================
//
//	FDI Disk(BAD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDiskBAD::FDIDiskBAD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('B', 'A', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDiskBAD::~FDIDiskBAD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskBAD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrackBAD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Verify file size is divisible by 1024 and 1280KB or less
	size = fio.GetFileSize();
	if (size & 0x3ff) {
		fio.Close();
		return FALSE;
	}
	if (size > 1310720) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Store path, offset
	disk.path = path;
	disk.offset = offset;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create tracks (0-76 cylinders, 77*2 tracks)
	for (i=0; i<154; i++) {
		track = new FDITrackBAD(this, i);
		AddTrack(track);
	}

	// End
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDiskBAD::Seek(int c)
{
	int i;
	FDITrackBAD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// Write track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = (FDITrackBAD*)GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		track->Save(disk.path, offset);
	}

	// Allow c up to 76. If out of range, set head[i]=NULL
	if ((c < 0) || (c > 76)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrackBAD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Load
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDiskBAD::Flush()
{
	ASSERT(this);

	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x8KB)
		offset = track->GetTrack();
		offset <<= 13;

		// Write
		if(!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDI Track(2DD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrack2DD::FDITrack2DD(FDIDisk *disk, int track) : FDITrack(disk, track, FALSE)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2DD::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset % 0x1200) == 0);
	ASSERT(offset < 0xb4000);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// Determine C, H, N
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 2;

	// Open for reading
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Seek
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// Loop
	for (i=0; i<9; i++) {
		// Read data
		if (!fio.Read(buf, sizeof(buf))) {
			// Delete partially added sectors
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// Create sector
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x54, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Close
	fio.Close();

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDI Disk(2DD)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDisk2DD::FDIDisk2DD(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('2', 'D', 'D', ' ');
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDisk2DD::~FDIDisk2DD()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// Write
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2DD *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Verify file size is 737280
	size = fio.GetFileSize();
	if (size != 0xb4000) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Store path, offset
	disk.path = path;
	disk.offset = offset;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create tracks (0-79 cylinders, 80*2 tracks)
	for (i=0; i<160; i++) {
		track = new FDITrack2DD(this, i);
		AddTrack(track);
	}

	// End
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2DD::Seek(int c)
{
	int i;
	FDITrack2DD *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// Write track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = (FDITrack2DD*)GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// Write
		track->Save(disk.path, offset);
	}

	// Allow c up to 79. If out of range, set head[i]=NULL
	if ((c < 0) || (c > 79)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrack2DD*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Calculate offset from track number (x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// Load
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	Create new disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2DD *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// Physical format only allows 2DD
	if (opt->phyfmt != FDI_2DD) {
		return FALSE;
	}

	// Try to create file
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Store path, offset
	disk.path = path;
	disk.offset = 0;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create track and physical format only for 0-159
	for (i=0; i<160; i++) {
		track = new FDITrack2DD(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// Logical format
	FDIDisk::Create(path, opt);

	// WriteLoop
	offset = 0;
	for (i=0; i<160; i++) {
		// Get track
		track = (FDITrack2DD*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// Write
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// Next
		offset += (0x200 * 9);
	}

	// Success
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2DD::Flush()
{
	int i;
	DWORD offset;
	FDITrack *track;

	ASSERT(this);

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x1200)
		offset = track->GetTrack();
		offset *= 0x1200;

		// Write
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}

//===========================================================================
//
//	FDI Track(2HQ)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDITrack2HQ::FDITrack2HQ(FDIDisk *disk, int track) : FDITrack(disk, track)
{
	ASSERT(disk);
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDITrack2HQ::Load(const Filepath& path, DWORD offset)
{
	Fileio fio;
	BYTE buf[0x200];
	DWORD chrn[4];
	int i;
	FDISector *sector;

	ASSERT(this);
	ASSERT((offset % 0x2400) == 0);
	ASSERT(offset < 0x168000);

	// Not needed if already initialized (called each seek, read once and cache)
	if (IsInit()) {
		return TRUE;
	}

	// Sector does not exist
	ASSERT(!GetFirst());
	ASSERT(GetAllSectors() == 0);
	ASSERT(GetMFMSectors() == 0);
	ASSERT(GetFMSectors() == 0);

	// Determine C, H, N
	chrn[0] = GetTrack() >> 1;
	chrn[1] = GetTrack() & 1;
	chrn[3] = 2;

	// Open for reading
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Seek
	if (!fio.Seek(offset)) {
		fio.Close();
		return FALSE;
	}

	// Loop
	for (i=0; i<18; i++) {
		// Read data
		if (!fio.Read(buf, sizeof(buf))) {
			// Delete partially added sectors
			ClrSector();
			fio.Close();
			return FALSE;
		}

		// Create sector
		chrn[2] = i + 1;
		sector = new FDISector(TRUE, chrn);
		sector->Load(buf, sizeof(buf), 0x54, FDD_NOERROR);

		// Add sector
		AddSector(sector);
	}

	// Close
	fio.Close();

	// Calculate position
	CalcPos();

	// Initializeok
	trk.init = TRUE;
	return TRUE;
}

//===========================================================================
//
//	FDI Disk(2HQ)
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDIDisk2HQ::FDIDisk2HQ(int index, FDI *fdi) : FDIDisk(index, fdi)
{
	// IDSet
	disk.id = MAKEID('2', 'H', 'Q', ' ');
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
FDIDisk2HQ::~FDIDisk2HQ()
{
	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// Write
		track->Save(disk.path, offset);
		disk.head[i] = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Open(const Filepath& path, DWORD offset)
{
	Fileio fio;
	DWORD size;
	FDITrack2HQ *track;
	int i;

	ASSERT(this);
	ASSERT(offset == 0);
	ASSERT(!GetFirst());
	ASSERT(!GetHead(0));
	ASSERT(!GetHead(1));

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Verify can open
	if (!fio.Open(path, Fileio::ReadWrite)) {
		// Try to open for reading
		if (!fio.Open(path, Fileio::ReadOnly)) {
			return FALSE;
		}

		// Reading is allowed
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Verify file size is 1474560
	size = fio.GetFileSize();
	if (size != 0x168000) {
		fio.Close();
		return FALSE;
	}
	fio.Close();

	// Store path, offset
	disk.path = path;
	disk.offset = offset;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create tracks (0-79 cylinders, 80*2 tracks)
	for (i=0; i<160; i++) {
		track = new FDITrack2HQ(this, i);
		AddTrack(track);
	}

	// End
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Seek
//
//---------------------------------------------------------------------------
void FASTCALL FDIDisk2HQ::Seek(int c)
{
	int i;
	FDITrack2HQ *track;
	DWORD offset;

	ASSERT(this);
	ASSERT((c >= 0) && (c < 82));

	// Write track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = (FDITrack2HQ*)GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// Write
		track->Save(disk.path, offset);
	}

	// Allow c up to 79. If out of range, set head[i]=NULL
	if ((c < 0) || (c > 79)) {
		disk.head[0] = NULL;
		disk.head[1] = NULL;
		return;
	}

	// Search and load corresponding track
	for (i=0; i<2; i++) {
		// Search track
		track = (FDITrack2HQ*)Search(c * 2 + i);
		ASSERT(track);
		disk.head[i] = track;

		// Calculate offset from track number (x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// Load
		track->Load(disk.path, offset);
	}
}

//---------------------------------------------------------------------------
//
//	Create new disk
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Create(const Filepath& path, const option_t *opt)
{
	int i;
	FDITrack2HQ *track;
	DWORD offset;
	Fileio fio;

	ASSERT(this);
	ASSERT(opt);

	// Physical format only allows 2HQ
	if (opt->phyfmt != FDI_2HQ) {
		return FALSE;
	}

	// Try to create file
	if (!fio.Open(path, Fileio::WriteOnly)) {
		return FALSE;
	}

	// Initialize as writable
	disk.writep = FALSE;
	disk.readonly = FALSE;

	// Store path, offset
	disk.path = path;
	disk.offset = 0;

	// Disk name is filename + extension
	strcpy(disk.name, path.GetShort());

	// Create track and physical format only for 0-159
	for (i=0; i<160; i++) {
		track = new FDITrack2HQ(this, i);
		track->Create(opt->phyfmt);
		AddTrack(track);
	}

	// Logical format
	FDIDisk::Create(path, opt);

	// WriteLoop
	offset = 0;
	for (i=0; i<160; i++) {
		// Get track
		track = (FDITrack2HQ*)Search(i);
		ASSERT(track);
		ASSERT(track->IsChanged());

		// Write
		if (!track->Save(&fio, offset)) {
			fio.Close();
			return FALSE;
		}

		// Next
		offset += (0x200 * 18);
	}

	// Success
	fio.Close();
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDIDisk2HQ::Flush()
{
	ASSERT(this);

	int i;
	DWORD offset;
	FDITrack *track;

	// Write last track data
	for (i=0; i<2; i++) {
		// Is there a track
		track = GetHead(i);
		if (!track) {
			continue;
		}

		// Calculate offset from track number (x0x2400)
		offset = track->GetTrack();
		offset *= 0x2400;

		// Write
		if (!track->Save(disk.path, offset)) {
			return FALSE;
		}
	}

	return TRUE;
}
