//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ Disk Control ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "fileio.h"
#include "vm.h"
#include "sasi.h"
#include "disk.h"

//===========================================================================
//
//	Disk track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
DiskTrack::DiskTrack(int track, int size, int sectors, BOOL raw)
{
	ASSERT(track >= 0);
	ASSERT((size == 8) || (size == 9) || (size == 11));
	ASSERT((sectors > 0) && (sectors <= 0x100));

	// Set parameters
	dt.track = track;
	dt.size = size;
	dt.sectors = sectors;
	dt.raw = raw;

	// Not loaded (need to load)
	dt.init = FALSE;

	// Not changed
	dt.changed = FALSE;

	// Buffer is not allocated yet
	dt.buffer = NULL;
	dt.changemap = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
DiskTrack::~DiskTrack()
{
	// Deallocate buffer, do not save
	if (dt.buffer) {
		delete[] dt.buffer;
		dt.buffer = NULL;
	}
	if (dt.changemap) {
		delete[] dt.changemap;
		dt.changemap = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Load(const Filepath& path)
{
	Fileio fio;
	DWORD offset;
	int i;
	int length;

	ASSERT(this);

	// If already loaded, skip
	if (dt.init) {
		ASSERT(dt.buffer);
		ASSERT(dt.changemap);
		return TRUE;
	}

	ASSERT(!dt.buffer);
	ASSERT(!dt.changemap);

	// Calculate offset (previous track stores 256 sectors)
	offset = (dt.track << 8);
	if (dt.raw) {
		ASSERT(dt.size == 11);
		offset *= 0x930;
		offset += 0x10;
	}
	else {
		offset <<= dt.size;
	}

	// Calculate length (next track's data size)
	length = dt.sectors << dt.size;

	// Ensure buffer allocation size
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	try {
		dt.buffer = new BYTE[ length ];
	}
	catch (...) {
		dt.buffer = NULL;
		return FALSE;
	}
	if (!dt.buffer) {
		return FALSE;
	}

	// Ensure changemap allocation
	try {
		dt.changemap = new BOOL[dt.sectors];
	}
	catch (...) {
		dt.changemap = NULL;
		return FALSE;
	}
	if (!dt.changemap) {
		return FALSE;
	}

	// Clear changemap
	for (i=0; i<dt.sectors; i++) {
		dt.changemap[i] = FALSE;
	}

	// Open file and read
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}
	if (dt.raw) {
		// RAW mode
		for (i=0; i<dt.sectors; i++) {
			// Seek
			if (!fio.Seek(offset)) {
				fio.Close();
				return FALSE;
			}

			// Read
			if (!fio.Read(&dt.buffer[i << dt.size], 1 << dt.size)) {
				fio.Close();
				return FALSE;
			}

			// Next offset
			offset += 0x930;
		}
	}
	else {
		// Normal mode
		if (!fio.Seek(offset)) {
			fio.Close();
			return FALSE;
		}
		if (!fio.Read(dt.buffer, length)) {
			fio.Close();
			return FALSE;
		}
	}
	fio.Close();

	// Set flag, clear change
	dt.init = TRUE;
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Save(const Filepath& path)
{
	DWORD offset;
	int i;
	Fileio fio;
	int length;

	ASSERT(this);

	// If not loaded, skip
	if (!dt.init) {
		return TRUE;
	}

	// If not changed, skip
	if (!dt.changed) {
		return TRUE;
	}

	// Need to save
	ASSERT(dt.buffer);
	ASSERT(dt.changemap);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));

	// Write is not supported in RAW mode
	ASSERT(!dt.raw);

	// Calculate offset (previous track stores 256 sectors)
	offset = (dt.track << 8);
	offset <<= dt.size;

	// Calculate sector data length
	length = 1 << dt.size;

	// Open file
	if (!fio.Open(path, Fileio::ReadWrite)) {
		return FALSE;
	}

	// Write loop
	for (i=0; i<dt.sectors; i++) {
		// If changed
		if (dt.changemap[i]) {
			// Seek, write
			if (!fio.Seek(offset + (i << dt.size))) {
				fio.Close();
				return FALSE;
			}
			if (!fio.Write(&dt.buffer[i << dt.size], length)) {
				fio.Close();
				return FALSE;
			}

			// Clear change flag
			dt.changemap[i] = FALSE;
		}
	}

	// Close
	fio.Close();

	// Clear change flag and exit
	dt.changed = FALSE;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Read sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Read(BYTE *buf, int sec) const
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));

	// Error if not loaded
	if (!dt.init) {
		return FALSE;
	}

	// Error if sector exceeds number
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// Copy
	ASSERT(dt.buffer);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	memcpy(buf, &dt.buffer[sec << dt.size], 1 << dt.size);

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Write sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskTrack::Write(const BYTE *buf, int sec)
{
	int offset;
	int length;

	ASSERT(this);
	ASSERT(buf);
	ASSERT((sec >= 0) & (sec < 0x100));
	ASSERT(!dt.raw);

	// Error if not loaded
	if (!dt.init) {
		return FALSE;
	}

	// Error if sector exceeds number
	if (sec >= dt.sectors) {
		return FALSE;
	}

	// Calculate offset and length
	offset = sec << dt.size;
	length = 1 << dt.size;

	// Compare
	ASSERT(dt.buffer);
	ASSERT((dt.size == 8) || (dt.size == 9) || (dt.size == 11));
	ASSERT((dt.sectors > 0) && (dt.sectors <= 0x100));
	if (memcmp(buf, &dt.buffer[offset], length) == 0) {
		// Same data, so skip
		return TRUE;
	}

	// Copy and change
	memcpy(&dt.buffer[offset], buf, length);
	dt.changemap[sec] = TRUE;
	dt.changed = TRUE;

	// Success
	return TRUE;
}

//===========================================================================
//
//	Disk cache
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
DiskCache::DiskCache(const Filepath& path, int size, int blocks)
{
	int i;

	ASSERT((size == 8) || (size == 9) || (size == 11));
	ASSERT(blocks > 0);

	// Initialize cache
	for (i=0; i<CacheMax; i++) {
		cache[i].disktrk = NULL;
		cache[i].serial = 0;
	}

	// Others
	serial = 0;
	sec_path = path;
	sec_size = size;
	sec_blocks = blocks;
	cd_raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
DiskCache::~DiskCache()
{
	// Clear tracks
	Clear();
}

//---------------------------------------------------------------------------
//
//	RAW mode setting
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::SetRawMode(BOOL raw)
{
	ASSERT(this);
	ASSERT(sec_size == 11);

	// Set
	cd_raw = raw;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Save()
{
	int i;

	ASSERT(this);

	// Save tracks
	for (i=0; i<CacheMax; i++) {
		// Valid track
		if (cache[i].disktrk) {
			// Save
			if (!cache[i].disktrk->Save(sec_path)) {
				return FALSE;
			}
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Get disk cache
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::GetCache(int index, int& track, DWORD& serial) const
{
	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));

	// If not used, return FALSE
	if (!cache[index].disktrk) {
		return FALSE;
	}

	// Set track and serial
	track = cache[index].disktrk->GetTrack();
	serial = cache[index].serial;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Clear
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Clear()
{
	int i;

	ASSERT(this);

	// Clear cache entries
	for (i=0; i<CacheMax; i++) {
		if (cache[i].disktrk) {
			delete cache[i].disktrk;
			cache[i].disktrk = NULL;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Read sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Read(BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// Update first
	Update();

	// Calculate track (fixed to 256 sectors per track)
	track = block >> 8;

	// Get track data
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// Read from track
	return disktrk->Read(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	Write sector
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Write(const BYTE *buf, int block)
{
	int track;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT(sec_size != 0);

	// Update first
	Update();

	// Calculate track (fixed to 256 sectors per track)
	track = block >> 8;

	// Get track data
	disktrk = Assign(track);
	if (!disktrk) {
		return FALSE;
	}

	// Write to track
	return disktrk->Write(buf, (BYTE)block);
}

//---------------------------------------------------------------------------
//
//	Assign track
//
//---------------------------------------------------------------------------
DiskTrack* FASTCALL DiskCache::Assign(int track)
{
	int i;
	int c;
	DWORD s;

	ASSERT(this);
	ASSERT(sec_size != 0);
	ASSERT(track >= 0);

	// First, check if already assigned
	for (i=0; i<CacheMax; i++) {
		if (cache[i].disktrk) {
			if (cache[i].disktrk->GetTrack() == track) {
				// Track found
				cache[i].serial = serial;
				return cache[i].disktrk;
			}
		}
	}

	// Next, check for empty slot
	for (i=0; i<CacheMax; i++) {
		if (!cache[i].disktrk) {
			// Try to load
			if (Load(i, track)) {
				// Load success
				cache[i].serial = serial;
				return cache[i].disktrk;
			}

			// Load failed
			return NULL;
		}
	}

	// Finally, find the least recently used and remove it

	// Start with index 0
	s = cache[0].serial;
	c = 0;

	// Compare serial numbers and find oldest
	for (i=0; i<CacheMax; i++) {
		ASSERT(cache[i].disktrk);

		// Compare with existing serial and update
		if (cache[i].serial < s) {
			s = cache[i].serial;
			c = i;
		}
	}

	// Save this track
	if (!cache[c].disktrk->Save(sec_path)) {
		return NULL;
	}

	// Delete this track
	delete cache[c].disktrk;
	cache[c].disktrk = NULL;

	// Load
	if (Load(c, track)) {
		// Load success
		cache[c].serial = serial;
		return cache[c].disktrk;
	}

	// Load failed
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Load track
//
//---------------------------------------------------------------------------
BOOL FASTCALL DiskCache::Load(int index, int track)
{
	int sectors;
	DiskTrack *disktrk;

	ASSERT(this);
	ASSERT((index >= 0) && (index < CacheMax));
	ASSERT(track >= 0);
	ASSERT(!cache[index].disktrk);

	// Get this track's sector count
	sectors = sec_blocks - (track << 8);
	ASSERT(sectors > 0);
	if (sectors > 0x100) {
		sectors = 0x100;
	}

	// Create disk track
	disktrk = new DiskTrack(track, sec_size, sectors, cd_raw);

	// Try loading
	if (!disktrk->Load(sec_path)) {
		// Failed
		delete disktrk;
		return FALSE;
	}

	// Assign and set
	cache[index].disktrk = disktrk;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Update serial number
//
//---------------------------------------------------------------------------
void FASTCALL DiskCache::Update()
{
	int i;

	ASSERT(this);

	// Update, ignore if not 0
	serial++;
	if (serial != 0) {
		return;
	}

	// Clear all cache serial numbers (32bit overflow occurred)
	for (i=0; i<CacheMax; i++) {
		cache[i].serial = 0;
	}
}

//===========================================================================
//
//	Disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
Disk::Disk(Device *dev)
{
	// Remember device as controller
	ctrl = dev;

	// Initialize
	disk.id = MAKEID('N', 'U', 'L', 'L');
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.removable = FALSE;
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = FALSE;
	disk.size = 0;
	disk.blocks = 0;
	disk.lun = 0;
	disk.code = 0;
	disk.dcache = NULL;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
Disk::~Disk()
{
	// Save disk cache
	if (disk.ready) {
		// Only if disk present
		ASSERT(disk.dcache);
		disk.dcache->Save();
	}

	// Delete disk cache
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Reset()
{
	ASSERT(this);

	// Not locked, no attention, reset done
	disk.lock = FALSE;
	disk.attn = FALSE;
	disk.reset = TRUE;
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Save(Fileio *fio, int ver)
{
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	// Save size
	sz = sizeof(disk_t);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}

	// Save self
	if (!fio->Write(&disk, (int)sz)) {
		return FALSE;
	}

	// Save path
	if (!diskpath.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);

	// Before version2.03, disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// Delete current disk cache
	if (disk.dcache) {
		disk.dcache->Save();
		delete disk.dcache;
		disk.dcache = NULL;
	}

	// Read size and check
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// Read to buffer
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// Read path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Only move if ID matches
	if (disk.id == buf.id) {
		// NULL device is excluded
		if (IsNULL()) {
			return TRUE;
		}

		// Open the same device as saved
		disk.ready = FALSE;
		if (Open(path)) {
			// Open creates disk cache
			// Copy properties directly
			if (!disk.readonly) {
				disk.writep = buf.writep;
			}
			disk.lock = buf.lock;
			disk.attn = buf.attn;
			disk.reset = buf.reset;
			disk.lun = buf.lun;
			disk.code = buf.code;

			// Loadable
			return TRUE;
		}
	}

	// Create disk cache again
	if (!IsReady()) {
		disk.dcache = NULL;
	}
	else {
		disk.dcache = new DiskCache(diskpath, disk.size, disk.blocks);
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	NULL check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsNULL() const
{
	ASSERT(this);

	if (disk.id == MAKEID('N', 'U', 'L', 'L')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	SASI check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::IsSASI() const
{
	ASSERT(this);

	if (disk.id == MAKEID('S', 'A', 'H', 'D')) {
		return TRUE;
	}
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Open
//	The base class opens, and the derived class does special processing
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Open(const Filepath& path)
{
	Fileio fio;

	ASSERT(this);
	ASSERT((disk.size == 8) || (disk.size == 9) || (disk.size == 11));
	ASSERT(disk.blocks > 0);

	// Disk ready
	disk.ready = TRUE;

	// Create cache
	ASSERT(!disk.dcache);
	disk.dcache = new DiskCache(path, disk.size, disk.blocks);

	// If write open is possible
	if (fio.Open(path, Fileio::ReadWrite)) {
		// Write enabled, not read only
		disk.writep = FALSE;
		disk.readonly = FALSE;
		fio.Close();
	}
	else {
		// Write disabled, read only
		disk.writep = TRUE;
		disk.readonly = TRUE;
	}

	// Not locked
	disk.lock = FALSE;

	// Save path
	diskpath = path;

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Eject
//
//---------------------------------------------------------------------------
void FASTCALL Disk::Eject(BOOL force)
{
	ASSERT(this);

	// Cannot eject if not removable
	if (!disk.removable) {
		return;
	}

	// Cannot eject if not ready
	if (!disk.ready) {
		return;
	}

	// If force flag is off, need to check if not locked
	if (!force) {
		if (disk.lock) {
			return;
		}
	}

	// Delete disk cache
	disk.dcache->Save();
	delete disk.dcache;
	disk.dcache = NULL;

	// Disk not ready, no attention
	disk.ready = FALSE;
	disk.writep = FALSE;
	disk.readonly = FALSE;
	disk.attn = FALSE;
}

//---------------------------------------------------------------------------
//
//	Write protect
//
//---------------------------------------------------------------------------
void FASTCALL Disk::WriteP(BOOL writep)
{
	ASSERT(this);

	// If not ready, do nothing
	if (!disk.ready) {
		return;
	}

	// If Read Only, return as is
	if (disk.readonly) {
		ASSERT(disk.writep);
		return;
	}

	// Set flag
	disk.writep = writep;
}

//---------------------------------------------------------------------------
//
//	Get member structure
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetDisk(disk_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// Copy member structure
	*buffer = disk;
}

//---------------------------------------------------------------------------
//
//	Get path
//
//---------------------------------------------------------------------------
void FASTCALL Disk::GetPath(Filepath& path) const
{
	path = diskpath;
}

//---------------------------------------------------------------------------
//
//	Flush
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Flush()
{
	ASSERT(this);

	// If no cache, return success
	if (!disk.dcache) {
		return TRUE;
	}

	// Save cache
	return disk.dcache->Save();
}

//---------------------------------------------------------------------------
//
//	Disk check
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::CheckReady()
{
	ASSERT(this);

	// If reset, return status
	if (disk.reset) {
		disk.code = DISK_DEVRESET;
		disk.reset = FALSE;
		return FALSE;
	}

	// If attention, return status
	if (disk.attn) {
		disk.code = DISK_ATTENTION;
		disk.attn = FALSE;
		return FALSE;
	}

	// If disk not ready, return status
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// No error
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//	The derived class does special processing as needed
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Inquiry(const DWORD* /*cdb*/, BYTE* /*buf*/)
{
	ASSERT(this);

	// Default is INQUIRY failure
	disk.code = DISK_INVALIDCMD;
	return 0;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//	Used exclusively by SASI
//
//---------------------------------------------------------------------------
int FASTCALL Disk::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// If no error, check disk not ready
	if (disk.code == DISK_NOERROR) {
		if (!disk.ready) {
			disk.code = DISK_NOTREADY;
		}
	}

	// Get size (follows allocation length specification)
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// SCSI-1, if size is 0, transfer 4 bytes (this rule removed in SCSI-2)
	if (size == 0) {
		size = 4;
	}

	// Clear buffer
	memset(buf, 0, size);

	// Set sense data, 18 bytes
	buf[0] = 0x70;
	buf[2] = (BYTE)(disk.code >> 16);
	buf[7] = 10;
	buf[12] = (BYTE)(disk.code >> 8);
	buf[13] = (BYTE)disk.code;

	// Clear code
	disk.code = 0x00;

	return size;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT check
//	Does not receive effect of disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::SelectCheck(const DWORD *cdb)
{
	int length;

	ASSERT(this);
	ASSERT(cdb);

	// Error if save parameters are set
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// Receive data specified by parameter length
	length = (int)cdb[4];
	return length;
}

//---------------------------------------------------------------------------
//
//	MODE SELECT
//	Does not receive effect of disk.code
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::ModeSelect(const BYTE *buf, int size)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(size >= 0);

	// Not supported
	disk.code = DISK_INVALIDPRM;

	printf("%p %d", (const void*)buf, size);

	return FALSE;
}

//---------------------------------------------------------------------------
//
//	MODE SENSE
//	Does not receive effect of disk.code
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ModeSense(const DWORD *cdb, BYTE *buf)
{
	int page;
	int length;
	int size;
	BOOL valid;
	BOOL change;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x1a);

	// Get allocation length, clear buffer
	length = (int)cdb[4];
	ASSERT((length >= 0) && (length < 0x100));
	memset(buf, 0, length);

	// Get changeable flag
	if ((cdb[2] & 0xc0) == 0x40) {
		change = TRUE;
	}
	else {
		change = FALSE;
	}

	// Get page code (0x00 is the smallest valid)
	page = cdb[2] & 0x3f;
	if (page == 0x00) {
		valid = TRUE;
	}
	else {
		valid = FALSE;
	}

	// Basic settings
	size = 4;
	if (disk.writep) {
		buf[2] = 0x80;
	}

	// If DBD is 0, add block descriptor
	if ((cdb[1] & 0x08) == 0) {
		// Read parameters page
		buf[ 3] = 0x08;

		// Return if no disk
		if (disk.ready) {
			// Block descriptor (number of blocks)
			buf[ 5] = (BYTE)(disk.blocks >> 16);
			buf[ 6] = (BYTE)(disk.blocks >> 8);
			buf[ 7] = (BYTE)disk.blocks;

			// Block descriptor (block length)
			size = 1 << disk.size;
			buf[ 9] = (BYTE)(size >> 16);
			buf[10] = (BYTE)(size >> 8);
			buf[11] = (BYTE)size;
		}

		// Size reset
		size = 12;
	}

	// Page code 1 (read-write error recovery)
	if ((page == 0x01) || (page == 0x3f)) {
		size += AddError(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 3 (format device)
	if ((page == 0x03) || (page == 0x3f)) {
		size += AddFormat(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 6 (optical)
	if (disk.id == MAKEID('S', 'C', 'M', 'O')) {
		if ((page == 0x06) || (page == 0x3f)) {
			size += AddOpt(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 8 (caching)
	if ((page == 0x08) || (page == 0x3f)) {
		size += AddCache(change, &buf[size]);
		valid = TRUE;
	}

	// Page code 13 (CD-ROM)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0d) || (page == 0x3f)) {
			size += AddCDROM(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Page code 14 (CD-DA)
	if (disk.id == MAKEID('S', 'C', 'C', 'D')) {
		if ((page == 0x0e) || (page == 0x3f)) {
			size += AddCDDA(change, &buf[size]);
			valid = TRUE;
		}
	}

	// Set last position with allocation length
	buf[0] = (BYTE)(size - 1);

	// Unsupported page
	if (!valid) {
		disk.code = DISK_INVALIDCDB;
		return 0;
	}

	// Saved values are not supported
	if ((cdb[2] & 0xc0) == 0xc0) {
		disk.code = DISK_PARAMSAVE;
		return 0;
	}

	// MODE SENSE success
	disk.code = DISK_NOERROR;
	return length;
}

//---------------------------------------------------------------------------
//
//	Add error page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddError(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x01;
	buf[1] = 0x0a;

	// If changeable, return
	if (change) {
		return 12;
	}

	// Use default values: retry count 0, recovered error stays silent
	return 12;
}

//---------------------------------------------------------------------------
//
//	Add format page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddFormat(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x03;
	buf[1] = 0x16;

	// If changeable, return
	if (change) {
		return 24;
	}

	// Set removable format
	if (disk.removable) {
		buf[20] = 0x20;
	}

	return 24;
}

//---------------------------------------------------------------------------
//
//	Add optical page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddOpt(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x06;
	buf[1] = 0x02;

	// If changeable, return
	if (change) {
		return 4;
	}

	// Update block transfer does not occur
	return 4;
}

//---------------------------------------------------------------------------
//
//	Add cache page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCache(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x08;
	buf[1] = 0x0a;

	// If changeable, return
	if (change) {
		return 12;
	}

	// Read cache only valid, write cache does not occur
	return 12;
}

//---------------------------------------------------------------------------
//
//	Add CD-ROM page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDROM(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x0d;
	buf[1] = 0x06;

	// If changeable, return
	if (change) {
		return 8;
	}

	// Inactivity timeout is 2 sec
	buf[3] = 0x05;

	// MSF format is 60, 75 respectively
	buf[5] = 60;
	buf[7] = 75;

	return 8;
}

//---------------------------------------------------------------------------
//
//	Add CD-DA page
//
//---------------------------------------------------------------------------
int FASTCALL Disk::AddCDDA(BOOL change, BYTE *buf)
{
	ASSERT(this);
	ASSERT(buf);

	// Set code and length
	buf[0] = 0x0e;
	buf[1] = 0x0e;

	// If changeable, return
	if (change) {
		return 16;
	}

	// Audio plays silently until the next track is reached
	return 16;
}

//---------------------------------------------------------------------------
//
//	TEST UNIT READY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::TestUnitReady(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// TEST UNIT READY success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REZERO UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Rezero(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// REZERO success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	FORMAT UNIT
//	For SASI is format code $06, for SCSI is format code $04
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Format(const DWORD *cdb)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// FMTDATA=1 is not supported
	if (cdb[1] & 0x10) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// FORMAT success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	REASSIGN BLOCKS
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Reassign(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// REASSIGN BLOCKS success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL Disk::Read(BYTE *buf, int block)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Error if logical block number exceeds
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}

	// Read from cache
	if (!disk.dcache->Read(buf, block)) {
		disk.code = DISK_READFAULT;
		return 0;
	}

	// Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE check
//
//---------------------------------------------------------------------------
int FASTCALL Disk::WriteCheck(int block)
{
	ASSERT(this);
	ASSERT(block >= 0);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Error if logical block number exceeds
	if (block >= disk.blocks) {
		return 0;
	}

	// Error if write disabled
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return 0;
	}

	// Success
	return (1 << disk.size);
}

//---------------------------------------------------------------------------
//
//	WRITE
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Write(const BYTE *buf, int block)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// Error if not ready
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// Error if logical block number exceeds
	if (block >= disk.blocks) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	// Error if write disabled
	if (disk.writep) {
		disk.code = DISK_WRITEPROTECT;
		return FALSE;
	}

	// Write to cache
	if (!disk.dcache->Write(buf, block)) {
		disk.code = DISK_WRITEFAULT;
		return FALSE;
	}

	// Success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEEK
//	LBA check is not performed (SASI IOCS)
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Seek(const DWORD* /*cdb*/)
{
	ASSERT(this);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// SEEK success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	START STOP UNIT
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::StartStop(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1b);

	// Check eject bit, eject if necessary
	if (cdb[4] & 0x02) {
		if (disk.lock) {
			// Locked, cannot eject
			disk.code = DISK_PREVENT;
			return FALSE;
		}

		// Eject
		Eject(FALSE);
	}

	// OK
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	SEND DIAGNOSTIC
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::SendDiag(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1d);

	// PF bit is not supported
	if (cdb[1] & 0x10) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Parameter list is not supported
	if ((cdb[3] != 0) || (cdb[4] != 0)) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Always success
	disk.code = DISK_NOERROR;
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	PREVENT/ALLOW MEDIUM REMOVAL
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Removal(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x1e);

	// Status check
	if (!CheckReady()) {
		return FALSE;
	}

	// Set lock flag
	if (cdb[4] & 0x01) {
		disk.lock = TRUE;
	}
	else {
		disk.lock = FALSE;
	}

	// REMOVAL success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ CAPACITY
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadCapacity(const DWORD* /*cdb*/, BYTE *buf)
{
	DWORD blocks;
	DWORD length;

	ASSERT(this);
	ASSERT(buf);

	// Clear buffer
	memset(buf, 0, 8);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Create last block address (disk.blocks - 1)
	ASSERT(disk.blocks > 0);
	blocks = disk.blocks - 1;
	buf[0] = (BYTE)(blocks >> 24);
	buf[1] = (BYTE)(blocks >> 16);
	buf[2] = (BYTE)(blocks >>  8);
	buf[3] = (BYTE)blocks;

	// Create block length (1 << disk.size)
	length = 1 << disk.size;
	buf[4] = (BYTE)(length >> 24);
	buf[5] = (BYTE)(length >> 16);
	buf[6] = (BYTE)(length >> 8);
	buf[7] = (BYTE)length;

	// Return buffer size
	return 8;
}

//---------------------------------------------------------------------------
//
//	VERIFY
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::Verify(const DWORD *cdb)
{
	int record;
    int blocks;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x2f);

	// Get parameters
	record = cdb[2];
	record <<= 8;
	record |= cdb[3];
	record <<= 8;
	record |= cdb[4];
	record <<= 8;
	record |= cdb[5];
	blocks = cdb[7];
	blocks <<= 8;
	blocks |= cdb[8];

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Parameter check
	if (disk.blocks < (record + blocks)) {
		disk.code = DISK_INVALIDLBA;
		return FALSE;
	}

	// Success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL Disk::ReadToc(const DWORD *cdb, BYTE *buf)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	printf("%p %p", (const void*)buf, (const void*)cdb);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudio(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x45);

	printf("%p", (const void*)cdb);

	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioMSF(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x47);

	printf("%p", (const void*)cdb);
	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL Disk::PlayAudioTrack(const DWORD *cdb)
{
	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x48);
	printf("%p", (const void*)cdb);
	// This command is not supported
	disk.code = DISK_INVALIDCMD;
	return FALSE;
}

//===========================================================================
//
//	SASI hard disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SASIHD::SASIHD(Device *dev) : Disk(dev)
{
	// SASI hard disk
	disk.id = MAKEID('S', 'A', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SASIHD::Open(const Filepath& path)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Need read open
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	// Only 10MB, 20MB, 40MB
	switch (size) {
		// 10MB
		case 0x9f5400:
			break;

		// 20MB
		case 0x13c9800:
			break;

		// 40MB
		case 0x2793000:
			break;

		// Others (not supported)
		default:
			return FALSE;
	}

	// Set sector size and blocks
	disk.size = 8;
	disk.blocks = size >> 8;

	// Base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	Device reset
//
//---------------------------------------------------------------------------
void FASTCALL SASIHD::Reset()
{
	ASSERT(this);

	// Unlock and no attention
	disk.lock = FALSE;
	disk.attn = FALSE;

	// Reset, clear code
	disk.reset = TRUE;
	disk.code = 0x00;
}

//---------------------------------------------------------------------------
//
//	REQUEST SENSE
//
//---------------------------------------------------------------------------
int FASTCALL SASIHD::RequestSense(const DWORD *cdb, BYTE *buf)
{
	int size;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);

	// Get size
	size = (int)cdb[4];
	ASSERT((size >= 0) && (size < 0x100));

	// SASI is limited to non-formatted
	memset(buf, 0, size);
	buf[0] = (BYTE)(disk.code >> 16);
	buf[1] = (BYTE)(disk.lun << 5);

	// Clear code
	disk.code = 0x00;

	return size;
}

//===========================================================================
//
//	SCSI hard disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIHD::SCSIHD(Device *dev) : Disk(dev)
{
	// SCSI hard disk
	disk.id = MAKEID('S', 'C', 'H', 'D');
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIHD::Open(const Filepath& path)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Need read open
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	// Must be 512 byte unit
	if (size & 0x1ff) {
		return FALSE;
	}

	// From 10MB to 4GB
	if (size < 0x9f5400) {
		return FALSE;
	}
	if (size > 0xfff00000) {
		return FALSE;
	}

	// Set sector size and blocks
	disk.size = 9;
	disk.blocks = size >> 9;

	// Base class
	return Disk::Open(path);
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIHD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Disk check (if no image file, error)
	if (!disk.ready) {
		disk.code = DISK_NOTREADY;
		return FALSE;
	}

	// Basic data
	// buf[0] ... Direct Access Device
	// buf[2] ... SCSI-2 command system type
	// buf[3] ... SCSI-2 response data format for Inquiry
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// Fill space
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// Product name
	size = disk.blocks >> 11;
	if (size < 300)
		sprintf(string, "PRODRIVE LPS%dS", size);
	else if (size < 600)
		sprintf(string, "MAVERICK%dS", size);
	else if (size < 800)
		sprintf(string, "LIGHTNING%dS", size);
	else if (size < 1000)
		sprintf(string, "TRAILBRAZER%dS", size);
	else if (size < 2000)
		sprintf(string, "FIREBALL%dS", size);
	else
		sprintf(string, "FBSE%d.%dS", size / 1000, (size % 1000) / 100);
	memcpy(&buf[16], string, strlen(string));

	// Revision (XM6 version No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// Return size 36 bytes, but limit to allocation length
	size = 36;
	len = (int)cdb[4];
	if (len < size) {
		size = len;
	}

	// Success
	disk.code = DISK_NOERROR;
	return size;
}

//===========================================================================
//
//	SCSI magneto-optical disk
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSIMO::SCSIMO(Device *dev) : Disk(dev)
{
	// SCSI magneto-optical disk
	disk.id = MAKEID('S', 'C', 'M', 'O');

	// Removable
	disk.removable = TRUE;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	DWORD size;

	ASSERT(this);
	ASSERT(!disk.ready);

	// Need read open
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get file size
	size = fio.GetFileSize();
	fio.Close();

	switch (size) {
		// 128MB
		case 0x797f400:
			disk.size = 9;
			disk.blocks = 248826;
			break;

		// 230MB
		case 0xd9eea00:
			disk.size = 9;
			disk.blocks = 446325;
			break;

		// 540MB
		case 0x1fc8b800:
			disk.size = 9;
			disk.blocks = 1041500;
			break;

		// 640MB
		case 0x25e28000:
			disk.size = 11;
			disk.blocks = 310352;
			break;

		// Others (error)
		default:
			return FALSE;
	}

	// Base class
	Disk::Open(path);

	// If disk, attention
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSIMO::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Before version2.03, disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// Read size and check
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// Read to buffer
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// Read path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Force eject
	Eject(TRUE);

	// Only move if ID matches
	if (disk.id != buf.id) {
		// Saved MO is different. Keep eject state
		return TRUE;
	}

	// Try re-open
	if (!Open(path, FALSE)) {
		// Cannot re-open. Keep eject state
		return TRUE;
	}

	// Open creates disk cache. Copy properties directly
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// Loadable
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSIMO::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... Optical Memory Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 command system type
	// buf[3] ... SCSI-2 response data format for Inquiry
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x07;
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// Fill space
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// Product name
	memcpy(&buf[16], "M2513A", 6);

	// Revision (XM6 version No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// Return size 36 bytes, but limit to allocation length
	size = 36;
	len = cdb[4];
	if (len < size) {
		size = len;
	}

	// Success
	disk.code = DISK_NOERROR;
	return size;
}

//===========================================================================
//
//	CD track
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDTrack::CDTrack(SCSICD *scsicd)
{
	ASSERT(scsicd);

	// Set parent CD-ROM device
	cdrom = scsicd;

	// Track invalid
	valid = FALSE;

	// Initialize other data
	track_no = -1;
	first_lba = 0;
	last_lba = 0;
	audio = FALSE;
	raw = FALSE;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDTrack::~CDTrack()
{
}

//---------------------------------------------------------------------------
//
//	Initialize
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::Init(int track, DWORD first, DWORD last)
{
	ASSERT(this);
	ASSERT(!valid);
	ASSERT(track >= 1);
	ASSERT(first < last);

	// Set and validate track number
	track_no = track;
	valid = TRUE;

	// LBA is valid
	first_lba = first;
	last_lba = last;

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Set path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::SetPath(BOOL cdda, const Filepath& path)
{
	ASSERT(this);
	ASSERT(valid);

	// CD-DA or data track
	audio = cdda;

	// Path valid
	imgpath = path;
}

//---------------------------------------------------------------------------
//
//	Get path
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::GetPath(Filepath& path) const
{
	ASSERT(this);
	ASSERT(valid);

	// Return path
	path = imgpath;
}

//---------------------------------------------------------------------------
//
//	Add index
//
//---------------------------------------------------------------------------
void FASTCALL CDTrack::AddIndex(int index, DWORD lba)
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(index > 0);
	ASSERT(first_lba <= lba);
	ASSERT(lba <= last_lba);

	printf("%d %d", lba, index);

	// Current index is not supported
	ASSERT(FALSE);
}

//---------------------------------------------------------------------------
//
//	Get first LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetFirst() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return first_lba;
}

//---------------------------------------------------------------------------
//
//	Get last LBA
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetLast() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	return last_lba;
}

//---------------------------------------------------------------------------
//
//	Get blocks
//
//---------------------------------------------------------------------------
DWORD FASTCALL CDTrack::GetBlocks() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(first_lba < last_lba);

	// Calculate from first to last LBA
	return (DWORD)(last_lba - first_lba + 1);
}

//---------------------------------------------------------------------------
//
//	Get track number
//
//---------------------------------------------------------------------------
int FASTCALL CDTrack::GetTrackNo() const
{
	ASSERT(this);
	ASSERT(valid);
	ASSERT(track_no >= 1);

	return track_no;
}

//---------------------------------------------------------------------------
//
//	Valid block
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsValid(DWORD lba) const
{
	ASSERT(this);

	// Track is invalid, return FALSE
	if (!valid) {
		return FALSE;
	}

	// If before first, return FALSE
	if (lba < first_lba) {
		return FALSE;
	}

	// If after last, return FALSE
	if (last_lba < lba) {
		return FALSE;
	}

	// Valid track
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Audio track
//
//---------------------------------------------------------------------------
BOOL FASTCALL CDTrack::IsAudio() const
{
	ASSERT(this);
	ASSERT(valid);

	return audio;
}

//===========================================================================
//
//	CD-DA buffer
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
CDDABuf::CDDABuf()
{
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
CDDABuf::~CDDABuf()
{
}

//===========================================================================
//
//	SCSI CD-ROM
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
SCSICD::SCSICD(Device *dev) : Disk(dev)
{
	int i;

	// SCSI CD-ROM
	disk.id = MAKEID('S', 'C', 'C', 'D');

	// Removable, write disabled
	disk.removable = TRUE;
	disk.writep = TRUE;

	// Not RAW format
	rawfile = FALSE;

	// Initialize frame
	frame = 0;

	// Track management
	for (i=0; i<TrackMax; i++) {
		track[i] = NULL;
	}
	tracks = 0;
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Destructor
//
//---------------------------------------------------------------------------
SCSICD::~SCSICD()
{
	// Clear tracks
	ClearTrack();
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Load(Fileio *fio, int ver)
{
	size_t sz;
	disk_t buf;
	Filepath path;

	ASSERT(this);
	ASSERT(fio);
	ASSERT(ver >= 0x0200);

	// Before version2.03, disk was not saved
	if (ver <= 0x0202) {
		return TRUE;
	}

	// Read size and check
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(disk_t)) {
		return FALSE;
	}

	// Read to buffer
	if (!fio->Read(&buf, (int)sz)) {
		return FALSE;
	}

	// Read path
	if (!path.Load(fio, ver)) {
		return FALSE;
	}

	// Force eject
	Eject(TRUE);

	// Only move if ID matches
	if (disk.id != buf.id) {
		// Saved CD-ROM is different. Keep eject state
		return TRUE;
	}

	// Try re-open
	if (!Open(path, FALSE)) {
		// Cannot re-open. Keep eject state
		return TRUE;
	}

	// Open creates disk cache. Copy properties directly
	if (!disk.readonly) {
		disk.writep = buf.writep;
	}
	disk.lock = buf.lock;
	disk.attn = buf.attn;
	disk.reset = buf.reset;
	disk.lun = buf.lun;
	disk.code = buf.code;

	// Delete disk cache again
	if (disk.dcache) {
		delete disk.dcache;
		disk.dcache = NULL;
	}
	disk.dcache = NULL;

	// Calculate
	disk.blocks = track[0]->GetBlocks();
	if (disk.blocks > 0) {
		// Create disk cache
		track[0]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = 0;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::Open(const Filepath& path, BOOL attn)
{
	Fileio fio;
	DWORD size;
	char file[5];

	ASSERT(this);
	ASSERT(!disk.ready);

	// Reset, clear tracks
	disk.blocks = 0;
	rawfile = FALSE;
	ClearTrack();

	// Need read open
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get size
	size = fio.GetFileSize();
	if (size <= 4) {
		fio.Close();
		return FALSE;
	}

	// Read CUE sheet or ISO file to distinguish
	fio.Read(file, 4);
	file[4] = '\0';
	fio.Close();

	// If starts with FILE, it is CUE sheet
	if (_strnicmp(file, "FILE", 4) == 0) {
		// Open as CUE
		if (!OpenCue(path)) {
			return FALSE;
		}
	}
	else {
		// Open as ISO
		if (!OpenIso(path)) {
			return FALSE;
		}
	}

	// Open success
	ASSERT(disk.blocks > 0);
	disk.size = 11;

	// Base class
	Disk::Open(path);

	// Set RAW flag
	ASSERT(disk.dcache);
	disk.dcache->SetRawMode(rawfile);

	// ROM disk, write is not possible
	disk.writep = TRUE;

	// If disk, attention
	if (disk.ready && attn) {
		disk.attn = TRUE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Open (CUE)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenCue(const Filepath& path)
{
	ASSERT(this);

	printf("%p", (const void*)&path);
	// Currently fails
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Open (ISO)
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::OpenIso(const Filepath& path)
{
	Fileio fio;
	DWORD size;
	BYTE header[12];
	BYTE sync[12];

	ASSERT(this);

	// Need read open
	if (!fio.Open(path, Fileio::ReadOnly)) {
		return FALSE;
	}

	// Get size
	size = fio.GetFileSize();
	if (size < 0x800) {
		fio.Close();
		return FALSE;
	}

	// Read first 12 bytes, close
	if (!fio.Read(header, sizeof(header))) {
		fio.Close();
		return FALSE;
	}

	// Check RAW format
	memset(sync, 0xff, sizeof(sync));
	sync[0] = 0x00;
	sync[11] = 0x00;
	rawfile = FALSE;
	if (memcmp(header, sync, sizeof(sync)) == 0) {
		// 00,FFx10,00 so RAW format
		if (!fio.Read(header, 4)) {
			fio.Close();
			return FALSE;
		}

		// Only MODE1/2048 or MODE1/2352 are supported
		if (header[3] != 0x01) {
			// Load different
			fio.Close();
			return FALSE;
		}

		// Set as RAW file
		rawfile = TRUE;
	}
	fio.Close();

	if (rawfile) {
		// Size must be multiple of 2536, up to 700MB
		if (size % 0x930) {
			return FALSE;
		}
		if (size > 912579600) {
			return FALSE;
		}

		// Set block count
		disk.blocks = size / 0x930;
	}
	else {
		// Size must be multiple of 2048, up to 700MB
		if (size & 0x7ff) {
			return FALSE;
		}
		if (size > 0x2bed5000) {
			return FALSE;
		}

		// Set block count
		disk.blocks = size >> 11;
	}

	// Create only one data track
	ASSERT(!track[0]);
	track[0] = new CDTrack(this);
	track[0]->Init(1, 0, disk.blocks - 1);
	track[0]->SetPath(FALSE, path);
	tracks = 1;
	dataindex = 0;

	// Open success
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	INQUIRY
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Inquiry(const DWORD *cdb, BYTE *buf)
{
	DWORD major;
	DWORD minor;
	char string[32];
	int size;
	int len;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(buf);
	ASSERT(cdb[0] == 0x12);

	// EVPD check
	if (cdb[1] & 0x01) {
		disk.code = DISK_INVALIDCDB;
		return FALSE;
	}

	// Basic data
	// buf[0] ... CD-ROM Device
	// buf[1] ... Removable
	// buf[2] ... SCSI-2 command system type
	// buf[3] ... SCSI-2 response data format for Inquiry
	// buf[4] ... Inquiry additional data
	memset(buf, 0, 8);
	buf[0] = 0x05;
	buf[1] = 0x80;
	buf[2] = 0x02;
	buf[3] = 0x02;
	buf[4] = 0x1f;

	// Fill space
	memset(&buf[8], 0x20, 28);
	memcpy(&buf[8], "XM6", 3);

	// Product name
	memcpy(&buf[16], "CDU-55S", 7);

	// Revision (XM6 version No)
	ctrl->GetVM()->GetVersion(major, minor);
	sprintf(string, "0%01d%01d%01d",
				major, (minor >> 4), (minor & 0x0f));
	memcpy((char*)&buf[32], string, 4);

	// Return size 36 bytes, but limit to allocation length
	size = 36;
	len = cdb[4];
	if (len < size) {
		size = len;
	}

	// Success
	disk.code = DISK_NOERROR;
	return size;
}

//---------------------------------------------------------------------------
//
//	READ
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::Read(BYTE *buf, int block)
{
	int index;
	Filepath path;

	ASSERT(this);
	ASSERT(buf);
	ASSERT(block >= 0);

	// Status check
	if (!CheckReady()) {
		return 0;
	}

	// Track search
	index = SearchTrack(block);

	// Invalid, out of range
	if (index < 0) {
		disk.code = DISK_INVALIDLBA;
		return 0;
	}
	ASSERT(track[index]);

	// If different from current data track
	if (dataindex != index) {
		// Delete current disk cache (no need to Save)
		delete disk.dcache;
		disk.dcache = NULL;

		// Reset block count
		disk.blocks = track[index]->GetBlocks();
		ASSERT(disk.blocks > 0);

		// Create disk cache
		track[index]->GetPath(path);
		disk.dcache = new DiskCache(path, disk.size, disk.blocks);
		disk.dcache->SetRawMode(rawfile);

		// Reset data index
		dataindex = index;
	}

	// Base class
	ASSERT(dataindex >= 0);
	return Disk::Read(buf, block);
}

//---------------------------------------------------------------------------
//
//	READ TOC
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::ReadToc(const DWORD *cdb, BYTE *buf)
{
	int last;
	int index;
	int length;
	int loop;
	int i;
	BOOL msf;
	DWORD lba;

	ASSERT(this);
	ASSERT(cdb);
	ASSERT(cdb[0] == 0x43);
	ASSERT(buf);

	// Disk check
	if (!CheckReady()) {
		return 0;
	}

	// If disk, tracks are at least 1
	ASSERT(tracks > 0);
	ASSERT(track[0]);

	// Get allocation length, clear buffer
	length = cdb[7] << 8;
	length |= cdb[8];
	memset(buf, 0, length);

	// Get MSF flag
	if (cdb[1] & 0x02) {
		msf = TRUE;
	}
	else {
		msf = FALSE;
	}

	// Get last track number and check
	last = track[tracks - 1]->GetTrackNo();
	if ((int)cdb[6] > last) {
		// If not AA, out of range
		if (cdb[6] != 0xaa) {
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Check starting index
	index = 0;
	if (cdb[6] != 0x00) {
		// Track number was specified, search track
		while (track[index]) {
			if ((int)cdb[6] == track[index]->GetTrackNo()) {
				break;
			}
			index++;
		}

		// If not found, if AA, error
		if (!track[index]) {
			if (cdb[6] == 0xaa) {
				// Since AA, return last LBA+1
				buf[0] = 0x00;
				buf[1] = 0x0a;
				buf[2] = (BYTE)track[0]->GetTrackNo();
				buf[3] = (BYTE)last;
				buf[6] = 0xaa;
				lba = track[tracks -1]->GetLast() + 1;
				if (msf) {
					LBAtoMSF(lba, &buf[8]);
				}
				else {
					buf[10] = (BYTE)(lba >> 8);
					buf[11] = (BYTE)lba;
				}
				return length;
			}

			// Others are error
			disk.code = DISK_INVALIDCDB;
			return 0;
		}
	}

	// Calculate number of track descriptor entries (loop count)
	loop = last - track[index]->GetTrackNo() + 1;
	ASSERT(loop >= 1);

	// Create header
	buf[0] = (BYTE)(((loop << 3) + 2) >> 8);
	buf[1] = (BYTE)((loop << 3) + 2);
	buf[2] = (BYTE)track[0]->GetTrackNo();
	buf[3] = (BYTE)last;
	buf += 4;

	// Loop
	for (i=0; i<loop; i++) {
		// ADR and Control
		if (track[index]->IsAudio()) {
			// Audio track
			buf[1] = 0x10;
		}
		else {
			// Data track
			buf[1] = 0x14;
		}

		// Track number
		buf[2] = (BYTE)track[index]->GetTrackNo();

		// Track address
		if (msf) {
			LBAtoMSF(track[index]->GetFirst(), &buf[4]);
		}
		else {
			buf[6] = (BYTE)(track[index]->GetFirst() >> 8);
			buf[7] = (BYTE)track[index]->GetFirst();
		}

		// Advance buffer and index
		buf += 8;
		index++;
	}

	// Return allocation length
	return length;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudio(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO MSF
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioMSF(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	PLAY AUDIO TRACK
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::PlayAudioTrack(const DWORD *cdb)
{
	ASSERT(this);
	printf("%p", (const void*)cdb);
	disk.code = DISK_INVALIDCDB;
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	LBA to MSF conversion
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::LBAtoMSF(DWORD lba, BYTE *msf) const
{
	DWORD m;
	DWORD s;
	DWORD f;

	ASSERT(this);

	// Convert by 75, 75*60 respectively
	m = lba / (75 * 60);
	s = lba % (75 * 60);
	f = s % 75;
	s /= 75;

	// Offset: M=0,S=2,F=0
	s += 2;
	if (s >= 60) {
		s -= 60;
		m++;
	}

	// Store
	ASSERT(m < 0x100);
	ASSERT(s < 60);
	ASSERT(f < 75);
	msf[0] = 0x00;
	msf[1] = (BYTE)m;
	msf[2] = (BYTE)s;
	msf[3] = (BYTE)f;
}

//---------------------------------------------------------------------------
//
//	MSF to LBA conversion
//
//---------------------------------------------------------------------------
DWORD FASTCALL SCSICD::MSFtoLBA(const BYTE *msf) const
{
	DWORD lba;

	ASSERT(this);
	ASSERT(msf[2] < 60);
	ASSERT(msf[3] < 75);

	// Calculate by multiples of 1, 75, 75*60
	lba = msf[1];
	lba *= 60;
	lba += msf[2];
	lba *= 75;
	lba += msf[3];

	// Since offset is M=0,S=2,F=0, subtract 150
	lba -= 150;

	return lba;
}

//---------------------------------------------------------------------------
//
//	Clear tracks
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::ClearTrack()
{
	int i;

	ASSERT(this);

	// Delete track objects
	for (i=0; i<TrackMax; i++) {
		if (track[i]) {
			delete track[i];
			track[i] = NULL;
		}
	}

	// Tracks is 0
	tracks = 0;

	// Set data and audio index invalid
	dataindex = -1;
	audioindex = -1;
}

//---------------------------------------------------------------------------
//
//	Search track
//	Returns -1 if not found
//
//---------------------------------------------------------------------------
int FASTCALL SCSICD::SearchTrack(DWORD lba) const
{
	int i;

	ASSERT(this);

	// Track loop
	for (i=0; i<tracks; i++) {
		// Compare with track
		ASSERT(track[i]);
		if (track[i]->IsValid(lba)) {
			return i;
		}
	}

	// Not found
	return -1;
}

//---------------------------------------------------------------------------
//
//	Frame advance
//
//---------------------------------------------------------------------------
BOOL FASTCALL SCSICD::NextFrame()
{
	ASSERT(this);
	ASSERT((frame >= 0) && (frame < 75));

	// Set frame in range 0-74
	frame = (frame + 1) % 75;

	// Returns FALSE when 1 arrives
	if (frame != 0) {
		return TRUE;
	}
	else {
		return FALSE;
	}
}

//---------------------------------------------------------------------------
//
//	Get CD-DA buffer
//
//---------------------------------------------------------------------------
void FASTCALL SCSICD::GetBuf(DWORD *buffer, int samples, DWORD rate)
{
	ASSERT(this);
	printf("%d", samples);
	printf("%p %lu", (const void*)buffer, (unsigned long)rate);
}
