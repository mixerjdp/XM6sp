//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ FDD(FD55GFR) ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "schedule.h"
#include "filepath.h"
#include "fileio.h"
#include "rtc.h"
#include "iosc.h"
#include "config.h"
#include "fdc.h"
#include "fdi.h"
#include "fdd.h"

//===========================================================================
//
//	FDD
//
//===========================================================================
//#define FDD_LOG

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
FDD::FDD(VM *p) : Device(p)
{
	// Device ID generation
	dev.id = MAKEID('F', 'D', 'D', ' ');
	dev.desc = "Floppy Drive";

	// Objects
	fdc = NULL;
	iosc = NULL;
	rtc = NULL;
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::Init()
{
	int i;
	Scheduler *scheduler;

	ASSERT(this);

	// Base class
	if (!Device::Init()) {
		return FALSE;
	}

	// Get FDC
	fdc = (FDC*)vm->SearchDevice(MAKEID('F', 'D', 'C', ' '));
	ASSERT(fdc);

	// Get IOSC
	iosc = (IOSC*)vm->SearchDevice(MAKEID('I', 'O', 'S', 'C'));
	ASSERT(iosc);

	// Get scheduler
	scheduler = (Scheduler*)vm->SearchDevice(MAKEID('S', 'C', 'H', 'E'));
	ASSERT(scheduler);

	// Get RTC
	rtc = (RTC*)vm->SearchDevice(MAKEID('R', 'T', 'C', ' '));
	ASSERT(rtc);

	// Drive unit initialization
	for (i=0; i<4; i++) {
		drv[i].fdi = new FDI(this);
		drv[i].next = NULL;
		drv[i].seeking = FALSE;
		drv[i].cylinder = 0;
		drv[i].insert = FALSE;
		drv[i].invalid = FALSE;
		drv[i].eject = TRUE;
		drv[i].blink = FALSE;
		drv[i].access = FALSE;
	}

	// ApplyCfg area
	fdd.fast = FALSE;

	// Common variable initialization
	fdd.motor = FALSE;
	fdd.settle = FALSE;
	fdd.force = FALSE;
	fdd.first = TRUE;
	fdd.selected = 0;
	fdd.hd = TRUE;

	// Seek event initialization
	seek.SetDevice(this);
	seek.SetDesc("Seek");
	seek.SetUser(0);
	seek.SetTime(0);
	scheduler->AddEvent(&seek);

	// Rotation event initialization (spin-down)
	rotation.SetDevice(this);
	rotation.SetDesc("Rotation Stopped");
	rotation.SetUser(1);
	rotation.SetTime(0);
	scheduler->AddEvent(&rotation);

	// Eject event initialization (media change)
	eject.SetDevice(this);
	eject.SetDesc("Eject");
	eject.SetUser(2);
	eject.SetTime(0);
	scheduler->AddEvent(&eject);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Cleanup()
{
	int i;

	ASSERT(this);

	// Delete FDI files
	for (i=0; i<4; i++) {
		if (drv[i].fdi) {
			delete drv[i].fdi;
			drv[i].fdi = NULL;
		}
		if (drv[i].next) {
			delete drv[i].next;
			drv[i].next = NULL;
		}
	}

	// Base class
	Device::Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Reset()
{
	int i;

	ASSERT(this);
	LOG0(Log::Normal, "Reset");

	// Drive unit reset
	for (i=0; i<4; i++) {
		drv[i].seeking = FALSE;
		drv[i].eject = TRUE;
		drv[i].blink = FALSE;
		drv[i].access = FALSE;

		// If next exists, swap
		if (drv[i].next) {
			delete drv[i].fdi;
			drv[i].fdi = drv[i].next;
			drv[0].fdi->Adjust();
			drv[1].fdi->Adjust();
			drv[i].next = NULL;
			drv[i].insert = TRUE;
			drv[i].invalid = FALSE;
		}

		// If invalid, recreate
		if (drv[i].invalid) {
			delete drv[i].fdi;
			drv[i].fdi = new FDI(this);
			drv[i].insert = FALSE;
			drv[i].invalid = FALSE;
		}

		// Seek to cylinder 0
		drv[i].cylinder = 0;
		drv[i].fdi->Seek(0);
	}

	// Common variable reset (selected matches FDC's DSR mapping)
	fdd.motor = FALSE;
	fdd.settle = FALSE;
	fdd.force = FALSE;
	fdd.first = TRUE;
	fdd.selected = 0;
	fdd.hd = TRUE;

	// Clear seek event (seeking=FALSE)
	seek.SetTime(0);

	// Clear rotation/spindown event (motor=FALSE, settle=FALSE)
	rotation.SetDesc("Rotation Stopped");
	rotation.SetTime(0);

	// Clear eject event (swap invalid)
	eject.SetTime(0);
}

//---------------------------------------------------------------------------
//
//	Save
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::Save(Fileio *fio, int ver)
{
	int i;
	size_t sz;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Save");

	// Save drive unit variable
	for (i=0; i<4; i++) {
		// Save size
		sz = sizeof(drv_t);
		if (!fio->Write(&sz, sizeof(sz))) {
			return FALSE;
		}

		// Save body
		if (!fio->Write(&drv[i], (int)sz)) {
			return FALSE;
		}

		// Image data is variable length
		if (drv[i].fdi) {
			if (!drv[i].fdi->Save(fio, ver)) {
				return FALSE;
			}
		}
		if (drv[i].next) {
			if (!drv[i].next->Save(fio, ver)) {
				return FALSE;
			}
		}
	}

	// Save common variable
	sz = sizeof(fdd);
	if (!fio->Write(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (!fio->Write(&fdd, (int)sz)) {
		return FALSE;
	}

	// Save event variable
	if (!seek.Save(fio, ver)) {
		return FALSE;
	}
	if (!rotation.Save(fio, ver)) {
		return FALSE;
	}
	if (!eject.Save(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::Load(Fileio *fio, int ver)
{
	int i;
	size_t sz;
	drv_t work;
	BOOL ready;
	Filepath path;
	int media;
	BOOL success;
	BOOL failed;

	ASSERT(this);
	ASSERT(fio);

	LOG0(Log::Normal, "Load");

	// Save drive unit variable
	for (i=0; i<4; i++) {
		// Load size
		if (!fio->Read(&sz, sizeof(sz))) {
			return FALSE;
		}
		if (sz != sizeof(drv_t)) {
			return FALSE;
		}

		// Load body
		if (!fio->Read(&work, (int)sz)) {
			return FALSE;
		}

		// Delete current image and reconstruct
		if (drv[i].fdi) {
			delete drv[i].fdi;
			drv[i].fdi = NULL;
		}
		if (drv[i].next) {
			delete drv[i].next;
			drv[i].next = NULL;
		}

		// Assignment
		drv[i] = work;

		// Reconstruct image and reconstruct
		failed = FALSE;
		if (drv[i].fdi) {
			// Reconstruct (current drv[i].fdi has meaningless data)
			drv[i].fdi = new FDI(this);

			// Load result
			success = FALSE;
			if (drv[i].fdi->Load(fio, ver, &ready, &media, path)) {
				// If ready
				if (ready) {
					// Open result
					if (drv[i].fdi->Open(path, media)) {
						// Seek
						drv[i].fdi->Seek(drv[i].cylinder);
						success = TRUE;
					}
					else {
						// Failure (since save time point was ready, eject)
						failed = TRUE;
					}
				}
				else {
					// If not ready, load OK
					success = TRUE;
				}
			}

			// If failed, create new one
			if (!success) {
				delete drv[i].fdi;
				drv[i].fdi = new FDI(this);
			}
		}
		if (drv[i].next) {
			// Reconstruct (current drv[i].next has meaningless data)
			drv[i].next = new FDI(this);

			// Load result
			success = FALSE;
			if (drv[i].next->Load(fio, ver, &ready, &media, path)) {
				// If ready
				if (ready) {
					// Open result
					if (drv[i].next->Open(path, media)) {
						// Seek
						drv[i].next->Seek(drv[i].cylinder);
						success = TRUE;
					}
				}
			}

			// If failed, delete and clear next
			if (!success) {
				delete drv[i].next;
				drv[i].next = NULL;
			}
		}

		// If main FDI's open failed due to inconsistency, start eject
		if (failed) {
			Eject(i, TRUE);
		}
	}

	// Load common variable
	if (!fio->Read(&sz, sizeof(sz))) {
		return FALSE;
	}
	if (sz != sizeof(fdd)) {
		return FALSE;
	}
	if (!fio->Read(&fdd, (int)sz)) {
		return FALSE;
	}

	// Load event variable
	if (!seek.Load(fio, ver)) {
		return FALSE;
	}
	if (!rotation.Load(fio, ver)) {
		return FALSE;
	}
	if (!eject.Load(fio, ver)) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Apply configuration
//
//---------------------------------------------------------------------------
void FASTCALL FDD::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	LOG0(Log::Normal, "Apply configuration");

	// Fast mode
	fdd.fast = config->floppy_speed;
#if defined(FDD_LOG)
	if (fdd.fast) {
		LOG0(Log::Normal, "Fast mode ON");
	}
	else {
		LOG0(Log::Normal, "Fast mode OFF");
	}
#endif	// FDD_LOG
}

//---------------------------------------------------------------------------
//
//	Drive unit reference
//
//---------------------------------------------------------------------------
void FASTCALL FDD::GetDrive(int drive, drv_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);
	ASSERT((drive >= 0) && (drive <= 3));

	// Copy drive unit reference
	*buffer = drv[drive];
}

//---------------------------------------------------------------------------
//
//	FDD unit reference
//
//---------------------------------------------------------------------------
void FASTCALL FDD::GetFDD(fdd_t *buffer) const
{
	ASSERT(this);
	ASSERT(buffer);

	// Copy FDD unit reference
	*buffer = fdd;
}

//---------------------------------------------------------------------------
//
//	FDI reference
//
//---------------------------------------------------------------------------
FDI* FASTCALL FDD::GetFDI(int drive)
{
	ASSERT(this);
	ASSERT((drive == 0) || (drive == 1));

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next;
	}

	// fdi is always valid
	ASSERT(drv[drive].fdi);
	return drv[drive].fdi;
}

//---------------------------------------------------------------------------
//
//	Event callback
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::Callback(Event *ev)
{
	DWORD user;
	int i;

	ASSERT(this);
	ASSERT(ev);

	// Receive user data
	user = ev->GetUser();

	switch (user) {
		// 0:Seek
		case 0:
			for (i=0; i<4; i++) {
				// If seeking
				if (drv[i].seeking) {
					// Seek completion
					drv[i].seeking = FALSE;
#if defined(FDD_LOG)
					LOG1(Log::Normal, "Seek completion drive%d", i);
#endif	// FDD_LOG

					// Set ready and proceed
					fdc->CompleteSeek(i, IsReady(i));
				}
			}
			// Simple, so break
			break;

		// 1:Rotation/spindown
		case 1:
			// Spindle not started or OFF
			if (!fdd.settle && !fdd.motor) {
				return FALSE;
			}

			// Spindle startup
			if (fdd.settle) {
				fdd.settle = FALSE;
				fdd.motor = TRUE;
				fdd.first = TRUE;

				// Rotation
				Rotation();
			}
			// Resume
			return TRUE;

		// 2:Eject/media change
		case 2:
			for (i=0; i<4; i++) {
				// If next exists, swap
				if (drv[i].next) {
					delete drv[i].fdi;
					drv[i].fdi = drv[i].next;
					drv[0].fdi->Adjust();
					drv[1].fdi->Adjust();
					drv[i].next = NULL;
					Insert(i);
				}

				// Eject with invalid
				if (drv[i].invalid) {
					Eject(i, TRUE);
				}
			}
			// Simple, so break
			break;

		// Others (unreachable)
		default:
			ASSERT(FALSE);
			break;
	}

	// Done
	return FALSE;
}

//---------------------------------------------------------------------------
//
//	Open
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::Open(int drive, const Filepath& path, int media)
{
	FDI *fdi;

	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));
	ASSERT((media >= 0) && (media < 0x10));

#if defined(FDD_LOG)
	LOG2(Log::Normal, "Open media drive%d media,%d", drive, media);
#endif	// FDD_LOG

	// Create FDI and open
	fdi = new FDI(this);
	if (!fdi->Open(path, media)) {
		delete fdi;
		return FALSE;
	}

	// Seek current image to set cylinder
	fdi->Seek(drv[drive].cylinder);

	// If current image is set pending
	if (drv[drive].next) {
		// Delete old image and set new one (300ms)
		delete drv[drive].next;
		drv[drive].next = fdi;
		eject.SetTime(300 * 1000 * 2);
		return TRUE;
	}

	// If ready, change
	if (drv[drive].insert && !drv[drive].invalid) {
		// Replace with current image and set eject event (300ms)
		Eject(drive, FALSE);
		drv[drive].next = fdi;
		eject.SetTime(300 * 1000 * 2);
		return TRUE;
	}

	// Normal uses current
	delete drv[drive].fdi;
	drv[drive].fdi = fdi;
	drv[0].fdi->Adjust();
	drv[1].fdi->Adjust();
	Insert(drive);
	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Media insertion
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Insert(int drive)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

#if defined(FDD_LOG)
	LOG1(Log::Normal, "Media insertion drive%d", drive);
#endif	// FDD_LOG

	// If seeking, cancel
	if (drv[drive].seeking) {
		drv[drive].seeking = FALSE;
		fdc->CompleteSeek(drive, FALSE);
	}

	// Media inserted, not invalid
	drv[drive].insert = TRUE;
	drv[drive].invalid = FALSE;

	// Seek to current cylinder
	ASSERT(drv[drive].fdi);
	drv[drive].fdi->Seek(drv[drive].cylinder);

	// Generate interrupt
	iosc->IntFDD(TRUE);
}

//---------------------------------------------------------------------------
//
//	Media eject
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Eject(int drive, BOOL force)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If not set, reject eject
	if (!force && !drv[drive].eject) {
		return;
	}

	// If already ejected, invalid
	if (!drv[drive].insert && !drv[drive].invalid) {
		return;
	}

#if defined(FDD_LOG)
	LOG1(Log::Normal, "Media eject drive%d", drive);
#endif	// FDD_LOG

	// Cancel seeking
	if (drv[drive].seeking) {
		drv[drive].seeking = FALSE;
		fdc->CompleteSeek(drive, FALSE);
	}

	// Replace image file with empty FDI
	ASSERT(drv[drive].fdi);
	delete drv[drive].fdi;
	drv[drive].fdi = new FDI(this);

	// Not inserted, not invalid
	drv[drive].insert = FALSE;
	drv[drive].invalid = FALSE;

	// Not accessible (LED off)
	drv[drive].access = FALSE;

	// Clear next
	if (drv[drive].next) {
		delete drv[drive].next;
		drv[drive].next = NULL;
	}

	// Generate interrupt
	iosc->IntFDD(TRUE);
}

//---------------------------------------------------------------------------
//
//	Media invalidation
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Invalid(int drive)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

#if defined(FDD_LOG)
	LOG1(Log::Normal, "Media invalidation drive%d", drive);
#endif	// FDD_LOG

	// If already invalidated, invalid
	if (drv[drive].insert && drv[drive].invalid) {
		return;
	}

	// Cancel seeking
	if (drv[drive].seeking) {
		drv[drive].seeking = FALSE;
		fdc->CompleteSeek(drive, FALSE);
	}

	// Media inserted, invalidated
	drv[drive].insert = TRUE;
	drv[drive].invalid = TRUE;

	// Clear next
	if (drv[drive].next) {
		delete drv[drive].next;
		drv[drive].next = NULL;
	}

	// Generate interrupt
	iosc->IntFDD(TRUE);

	// Set event (300ms)
	eject.SetTime(300 * 1000 * 2);
}

//---------------------------------------------------------------------------
//
//	Drive control
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Control(int drive, DWORD func)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// bit7 - LED blink
	if (func & 0x80) {
		if (!drv[drive].blink) {
			// Blink on
#if defined(FDD_LOG)
			LOG1(Log::Normal, "LED blink on drive%d", drive);
#endif	// FDD_LOG
			drv[drive].blink = TRUE;
		}
	}
	else {
		drv[drive].blink = FALSE;
	}

	// bit6 - Eject inhibit
	if (func & 0x40) {
		if (drv[drive].eject) {
#if defined(FDD_LOG)
			LOG1(Log::Normal, "Eject inhibit drive%d", drive);
#endif	// FDD_LOG
			drv[drive].eject = FALSE;
		}
	}
	else {
		drv[drive].eject = TRUE;
	}

	// bit5 - Eject
	if (func & 0x20) {
		Eject(drive, TRUE);
	}
}

//---------------------------------------------------------------------------
//
//	Force ready
//
//---------------------------------------------------------------------------
void FASTCALL FDD::ForceReady(BOOL flag)
{
	ASSERT(this);

#if defined(FDD_LOG)
	if (flag) {
		LOG0(Log::Normal, "Force ready ON");
	}
	else {
		LOG0(Log::Normal, "Force ready OFF");
	}
#endif	// FDD_LOG

	// Reset only
	fdd.force = flag;
}

//---------------------------------------------------------------------------
//
//	Get rotation position
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDD::GetRotationPos() const
{
	DWORD remain;
	DWORD hus;

	ASSERT(this);

	// If stopped, 0
	if (rotation.GetTime() == 0) {
		return 0;
	}

	// Get rotation time
	hus = GetRotationTime();

	// Get countdown counter
	remain = rotation.GetRemain();
	if (remain > hus) {
		remain = 0;
	}

	// Return difference
	return (DWORD)(hus - remain);
}

//---------------------------------------------------------------------------
//
//	Get rotation time
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDD::GetRotationTime() const
{
	ASSERT(this);

	// 2HD is 360rpm, 2DD is 300rpm
	if (fdd.hd) {
		return 333333;
	}
	else {
		return 400000;
	}
}

//---------------------------------------------------------------------------
//
//	Rotation
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Rotation()
{
	DWORD rpm;
	DWORD hus;
	char desc[0x20];

	ASSERT(this);
	ASSERT(!fdd.settle);
	ASSERT(fdd.motor);

	rpm = 2000 * 1000 * 60;
	hus = GetRotationTime();
	rpm /= hus;
	sprintf(desc, "Rotation %drpm", rpm);
	rotation.SetDesc(desc);
	rotation.SetTime(hus);
}

//---------------------------------------------------------------------------
//
//	Get seek time
//
//---------------------------------------------------------------------------
DWORD FASTCALL FDD::GetSearch()
{
	DWORD schtime;

	ASSERT(this);

	// If fast mode, 64us fixed
	if (fdd.fast) {
		return 128;
	}

	// Ask image file
	schtime = drv[ fdd.selected ].fdi->GetSearch();

	return schtime;
}

//---------------------------------------------------------------------------
//
//	HD setting
//
//---------------------------------------------------------------------------
void FASTCALL FDD::SetHD(BOOL hd)
{

	ASSERT(this);

	if (hd) {
		if (fdd.hd) {
			return;
		}
#if defined(FDD_LOG)
		LOG0(Log::Normal, "Drive speed change 2HD");
#endif	// FDD_LOG
		fdd.hd = TRUE;
	}
	else {
		if (!fdd.hd) {
			return;
		}
#if defined(FDD_LOG)
		LOG0(Log::Normal, "Drive speed change 2DD");
#endif	// FDD_LOG
		fdd.hd = FALSE;
	}

	// If motor off, change speed
	if (!fdd.motor || fdd.settle) {
		return;
	}

	// Rotate and set
	Rotation();
}

//---------------------------------------------------------------------------
//
//	HD reference
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::IsHD() const
{
	ASSERT(this);

	return fdd.hd;
}

//---------------------------------------------------------------------------
//
//	Access LED setting
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Access(BOOL flag)
{
	int i;

	ASSERT(this);

	// All off
	for (i=0; i<4; i++) {
		drv[i].access = FALSE;
	}

	// If flag is on, set selected drive
	if (flag) {
		drv[ fdd.selected ].access = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	Ready check
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::IsReady(int drive, BOOL fdc) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If FDC side access
	if (fdc) {
		// Force ready overrides ready
		if (fdd.force) {
			return TRUE;
		}

		// Motor off means not ready
		if (!fdd.motor) {
			return FALSE;
		}
	}

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next->IsReady();
	}

	// Ask image file
	return drv[drive].fdi->IsReady();
}

//---------------------------------------------------------------------------
//
//	Write protect check
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::IsWriteP(int drive) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next->IsWriteP();
	}

	// Ask image file
	return drv[drive].fdi->IsWriteP();
}

//---------------------------------------------------------------------------
//
//	Read only check
//
//---------------------------------------------------------------------------
BOOL FASTCALL FDD::IsReadOnly(int drive) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next->IsReadOnly();
	}

	// Ask image file
	return drv[drive].fdi->IsReadOnly();
}

//---------------------------------------------------------------------------
//
//	Write protect
//
//---------------------------------------------------------------------------
void FASTCALL FDD::WriteP(int drive, BOOL flag)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// Set in image file
	drv[drive].fdi->WriteP(flag);
}

//---------------------------------------------------------------------------
//
//	Drive status reference
//
//	b7 : Insert
//	b6 : Invalid Insert
//	b5 : Inhibit Eject
//	b4 : Blink (Global)
//	b3 : Blink (Current)
//	b2 : Motor
//	b1 : Select
//	b0 : Access
//---------------------------------------------------------------------------
int FASTCALL FDD::GetStatus(int drive) const
{
	int st;

	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// Clear
	st = 0;

	// bit7 - Inserted
	if (drv[drive].insert) {
		st |= FDST_INSERT;
	}

	// bit6 - Invalid (inserted though)
	if (drv[drive].invalid) {
		st |= FDST_INVALID;
	}

	// bit5 - Can eject
	if (drv[drive].eject) {
		st |= FDST_EJECT;
	}

	// bit4 - Blink (global, not inserted)
	if (drv[drive].blink && !drv[drive].insert) {
		st |= FDST_BLINK;

		// bit3 - Blink (current)
		if (rtc->GetBlink(drive)) {
			st |= FDST_CURRENT;
		}
	}

	// bit2 - Motor
	if (fdd.motor) {
		st |= FDST_MOTOR;
	}

	// bit1 - Select
	if (drive == fdd.selected) {
		st |= FDST_SELECT;
	}

	// bit0 - Access
	if (drv[drive].access) {
		st |= FDST_ACCESS;
	}

	return st;
}

//---------------------------------------------------------------------------
//
//	Motor setting function and drive select
//
//---------------------------------------------------------------------------
void FASTCALL FDD::SetMotor(int drive, BOOL flag)
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// flag=FALSE is simple
	if (!flag) {
		// If already OFF, unnecessary
		if (!fdd.motor) {
			return;
		}
#if defined(FDD_LOG)
		LOG0(Log::Normal, "Motor OFF");
#endif	// FDD_LOG

		// Motor stop
		fdd.motor = FALSE;
		fdd.settle = FALSE;

		// Set selected drive
		fdd.selected = drive;

		// Set spindown event
		rotation.SetDesc("Standby 54000ms");
		rotation.SetTime(54 * 1000 * 2 * 1000);
		return;
	}

	// Set selected drive
	fdd.selected = drive;

	// If motor ON or settling, unnecessary
	if (fdd.motor || fdd.settle) {
		return;
	}

#if defined(FDD_LOG)
	LOG1(Log::Normal, "Motor ON drive%d select", drive);
#endif	// FDD_LOG

	// If event already running, spindle start
	if (rotation.GetTime() != 0) {
		// Motor ON
		fdd.settle = FALSE;
		fdd.motor = TRUE;
		fdd.first = TRUE;

		// Rotation
		Rotation();
		return;
	}

	// Motor OFF, settling
	fdd.motor = FALSE;
	fdd.settle = TRUE;

	// Set spindown event (settle mode 64us, normal mode 384ms)
	rotation.SetDesc("Settle 384ms");
	if (fdd.fast) {
		rotation.SetTime(128);
	}
	else {
		rotation.SetTime(384 * 1000 * 2);
	}
}

//---------------------------------------------------------------------------
//
//	Get cylinder
//
//---------------------------------------------------------------------------
int FASTCALL FDD::GetCylinder(int drive) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// Return cylinder
	return drv[drive].cylinder;
}

//---------------------------------------------------------------------------
//
//	Get media name
//
//---------------------------------------------------------------------------
void FASTCALL FDD::GetName(int drive, char *buf, int media) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));
	ASSERT(buf);
	ASSERT((media >= -1) && (media < 0x10));

	// If next exists, use next
	if (drv[drive].next) {
		drv[drive].next->GetName(buf, media);
		return;
	}

	// Ask image
	drv[drive].fdi->GetName(buf, media);
}

//---------------------------------------------------------------------------
//
//	Get path
//
//---------------------------------------------------------------------------
void FASTCALL FDD::GetPath(int drive, Filepath& path) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If next exists, use next
	if (drv[drive].next) {
		drv[drive].next->GetPath(path);
		return;
	}

	// Ask image
	drv[drive].fdi->GetPath(path);
}

//---------------------------------------------------------------------------
//
//	Get number of sides
//
//---------------------------------------------------------------------------
int FASTCALL FDD::GetDisks(int drive) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next->GetDisks();
	}

	// Ask image
	return drv[drive].fdi->GetDisks();
}

//---------------------------------------------------------------------------
//
//	Get current media type
//
//---------------------------------------------------------------------------
int FASTCALL FDD::GetMedia(int drive) const
{
	ASSERT(this);
	ASSERT((drive >= 0) && (drive <= 3));

	// If next exists, use next
	if (drv[drive].next) {
		return drv[drive].next->GetMedia();
	}

	// Ask image
	return drv[drive].fdi->GetMedia();
}

//---------------------------------------------------------------------------
//
//	Recalibrate
//
//---------------------------------------------------------------------------
void FASTCALL FDD::Recalibrate(DWORD srt)
{
	ASSERT(this);

	// Validity check
	if (drv[ fdd.selected ].cylinder == 0) {
		fdc->CompleteSeek(fdd.selected, IsReady(fdd.selected));
		return;
	}

#if defined(FDD_LOG)
	LOG1(Log::Normal, "Recalibrate drive%d", fdd.selected);
#endif	// FDD_LOG

	// fdd[drive].cylinder from step-out
	StepOut(drv[ fdd.selected ].cylinder, srt);
}

//---------------------------------------------------------------------------
//
//	Step in
//
//---------------------------------------------------------------------------
void FASTCALL FDD::StepIn(int step, DWORD srt)
{
	int cylinder;

	ASSERT(this);
	ASSERT((step >= 0) && (step < 256));

	// Step in
	cylinder = drv[ fdd.selected ].cylinder;
	cylinder += step;
	if (cylinder >= 82) {
		cylinder = 81;
	}

	// Validity check
	if (drv[ fdd.selected ].cylinder == cylinder) {
		// Generate seek interrupt
		fdc->CompleteSeek(fdd.selected, IsReady(fdd.selected));
		return;
	}

#if defined(FDD_LOG)
	LOG2(Log::Normal, "Step in drive%d step%d", fdd.selected, step);
#endif	// FDD_LOG

	// Seek execution
	SeekInOut(cylinder, srt);
}

//---------------------------------------------------------------------------
//
//	Step out
//
//---------------------------------------------------------------------------
void FASTCALL FDD::StepOut(int step, DWORD srt)
{
	int cylinder;

	ASSERT(this);
	ASSERT((step >= 0) && (step < 256));

	// Step out
	cylinder = drv[ fdd.selected ].cylinder;
	cylinder -= step;
	if (cylinder < 0) {
		cylinder = 0;
	}

	// Validity check
	if (drv[ fdd.selected ].cylinder == cylinder) {
		fdc->CompleteSeek(fdd.selected, IsReady(fdd.selected));
		return;
	}

#if defined(FDD_LOG)
	LOG2(Log::Normal, "Step out drive%d step%d", fdd.selected, step);
#endif	// FDD_LOG

	// Seek execution
	SeekInOut(cylinder, srt);
}

//---------------------------------------------------------------------------
//
//	Seek execution
//
//---------------------------------------------------------------------------
void FASTCALL FDD::SeekInOut(int cylinder, DWORD srt)
{
	int step;

	ASSERT(this);
	ASSERT((cylinder >= 0) && (cylinder < 82));

	// Calculate step count
	ASSERT(drv[ fdd.selected ].cylinder != cylinder);
	if (drv[ fdd.selected ].cylinder < cylinder) {
		step = cylinder - drv[ fdd.selected ].cylinder;
	}
	else {
		step = drv[ fdd.selected ].cylinder - cylinder;
	}

	// Seek now (seek flag is left as is)
	drv[ fdd.selected ].cylinder = cylinder;
	drv[ fdd.selected ].fdi->Seek(cylinder);
	drv[ fdd.selected ].seeking = TRUE;

	// Set event (SRT * step count + 0.64ms)
	if (fdd.fast) {
		// Fast mode fixes at 64us
		seek.SetTime(128);
	}
	else {
		srt *= step;
		srt += 1280;

		// Motor ON additional time is 128ms
		if (fdd.first) {
			srt += (128 * 0x800);
			fdd.first = FALSE;
		}
		seek.SetTime(srt);
	}
}

//---------------------------------------------------------------------------
//
//	Read ID
//	Returns status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_MAM			Address mark not found at specified track
//		FDD_NODATA		Address mark not found at specified track,
//						or no valid sector ID found
//
//---------------------------------------------------------------------------
int FASTCALL FDD::ReadID(DWORD *buf, BOOL mfm, int hd)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT((hd == 0) || (hd == 4));

#if defined(FDD_LOG)
	LOG2(Log::Normal, "ReadID drive%d head%d", fdd.selected, hd);
#endif	// FDD_LOG

	// Delegate to image
	return drv[ fdd.selected ].fdi->ReadID(buf, mfm, hd);
}

//---------------------------------------------------------------------------
//
//	Read sector
//	Returns status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_NOTWRITE	Read-only, write disabled
//		FDD_MAM			Address mark not found at specified track
//		FDD_NODATA		Address mark not found at specified track,
//						or no valid sector and CRC error
//		FDD_NOCYL		Read ID CYL field different from target, and FFh (non-existent) sector not found
//		FDD_BADCYL		Read ID CYL field different from target, and not FFh (non-existent) sector not found
//		FDD_IDCRC		ID field CRC error
//		FDD_DATACRC		DATA field CRC error
//		FDD_DDAM		Deleted dam sector
//
//---------------------------------------------------------------------------
int FASTCALL FDD::ReadSector(BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(buf);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

#if defined(FDD_LOG)
	LOG4(Log::Normal, "ReadSector C:%02X H:%02X R:%02X N:%02X",
						chrn[0], chrn[1], chrn[2], chrn[3]);
#endif	// FDD_LOG

	// Delegate to image
	return drv[ fdd.selected ].fdi->ReadSector(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	Write sector
//	Returns status (errors are ORed)
//		FDD_NOERROR		No error
//		FDD_NOTREADY	Not ready
//		FDD_NOTWRITE	Read-only, write disabled
//		FDD_MAM			Address mark not found at specified track
//		FDD_NODATA		Address mark not found at specified track,
//						or no valid sector and CRC error
//		FDD_NOCYL		Read ID CYL field different from target, and FFh (non-existent) sector not found
//		FDD_BADCYL		Read ID CYL field different from target, and not FFh (non-existent) sector not found
//		FDD_IDCRC		ID field CRC error
//		FDD_DDAM		Deleted dam sector
//
//---------------------------------------------------------------------------
int FASTCALL FDD::WriteSector(const BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd, BOOL deleted)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

#if defined(FDD_LOG)
	LOG4(Log::Normal, "WriteSector C:%02X H:%02X R:%02X N:%02X",
						chrn[0], chrn[1], chrn[2], chrn[3]);
#endif	// FDD_LOG

	// Delegate to image
	return drv[ fdd.selected ].fdi->WriteSector(buf, len, mfm, chrn, hd, deleted);
}

//---------------------------------------------------------------------------
//
//	Read diag
//
//---------------------------------------------------------------------------
int FASTCALL FDD::ReadDiag(BYTE *buf, int *len, BOOL mfm, DWORD *chrn, int hd)
{
	ASSERT(this);
	ASSERT(len);
	ASSERT(chrn);
	ASSERT((hd == 0) || (hd == 4));

#if defined(FDD_LOG)
	LOG4(Log::Normal, "ReadDiag C:%02X H:%02X R:%02X N:%02X",
						chrn[0], chrn[1], chrn[2], chrn[3]);
#endif	// FDD_LOG

	// Delegate to image
	return drv[ fdd.selected ].fdi->ReadDiag(buf, len, mfm, chrn, hd);
}

//---------------------------------------------------------------------------
//
//	Write ID
//
//---------------------------------------------------------------------------
int FASTCALL FDD::WriteID(const BYTE *buf, DWORD d, int sc, BOOL mfm, int hd, int gpl)
{
	ASSERT(this);
	ASSERT(sc > 0);
	ASSERT((hd == 0) || (hd == 4));

#if defined(FDD_LOG)
	LOG3(Log::Normal, "WriteID drive%d cylinder%d head%d",
					fdd.selected, drv[ fdd.selected ].cylinder, hd);
#endif	// FDD_LOG

	// Delegate to image
	return drv[ fdd.selected ].fdi->WriteID(buf, d, sc, mfm, hd, gpl);
}
