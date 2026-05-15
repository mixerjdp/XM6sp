//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[ Virtual Machine ]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)
#include <windows.h>
#endif
#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "device.h"
#include "schedule.h"
#include "cpu.h"
#include "memory.h"
#include "sram.h"
#include "sysport.h"
#include "tvram.h"
#include "vc.h"
#include "crtc.h"
#include "rtc.h"
#include "ppi.h"
#include "dmac.h"
#include "mfp.h"
#include "fdc.h"
#include "iosc.h"
#include "sasi.h"
#include "opmif.h"
#include "keyboard.h"
#include "adpcm.h"
#include "gvram.h"
#include "sprite.h"
#include "fdd.h"
#include "scc.h"
#include "mouse.h"
#include "printer.h"
#include "areaset.h"
#include "windrv.h"
#include "render.h"
#include "midi.h"
#include "scsi.h"
#include "mercury.h"
#include "neptune.h"
#include "filepath.h"
#include "fileio.h"
#include "musashi_adapter.h"
#include <vector>
#include <algorithm>

//===========================================================================
//
//	Virtual machine
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	Constructor
//
//---------------------------------------------------------------------------
VM::VM()
{
	// Initialize the runtime state
	status = FALSE;
	first_device = NULL;
	scheduler = NULL;

	// Device pointers
	scheduler = NULL;
	cpu = NULL;
	mfp = NULL;
	rtc = NULL;
	sram = NULL;
	host_message_callback = NULL;
	host_message_user = NULL;
	host_lock_vm_callback = NULL;
	host_unlock_vm_callback = NULL;
	host_sync_user = NULL;

	// Version number (preinitialized to the platform default)
	major_ver = 0x01;
	minor_ver = 0x00;

	// Clear the current state
	Clear();
}

//---------------------------------------------------------------------------
//
//	Initialization
//
//---------------------------------------------------------------------------
BOOL FASTCALL VM::Init()
{
	Device *device;

	ASSERT(this);
	ASSERT(!first_device);
	ASSERT(!status);

	// Power switch on
	power = TRUE;
	power_sw = TRUE;

	// Create devices in dependency order
	scheduler = new Scheduler(this);
	cpu = new CPU(this);
	new Keyboard(this);
	new Mouse(this);
	new FDD(this);
	new Render(this);
	new Memory(this);
	new GVRAM(this);
	new TVRAM(this);
	new CRTC(this);
	new VC(this);
	new DMAC(this);
	new AreaSet(this);
	mfp = new MFP(this);
	rtc = new RTC(this);
	new Printer(this);
	new SysPort(this);
	new OPMIF(this);
	new ADPCM(this);
	new FDC(this);
	new SASI(this);
	new SCC(this);
	new PPI(this);
	new IOSC(this);
	new Windrv(this);
	new SCSI(this);
	new MIDI(this);
	new Sprite(this);
	new Mercury(this);
	new Neptune(this);
	sram = new SRAM(this);

	// Host callbacks may have been configured before VM::Init().
	SetHostSyncCallbacks(host_lock_vm_callback, host_unlock_vm_callback, host_sync_user);

	// Initialize logging
	if (!log.Init(this)) {
		return FALSE;
	}

	// Get the device pointer list head
	device = first_device;

	// Initialize each device in sequence
	status = TRUE;
	while (device) {
		if (!device->Init()) {
			status = FALSE;
		}
		device = device->GetNextDevice();
	}

	return status;
}

//---------------------------------------------------------------------------
//
//	Cleanup
//
//---------------------------------------------------------------------------
void FASTCALL VM::Cleanup()
{
	ASSERT(this);

	// If shutdown happens while power is on, update the SRAM boot state
	if (status) {
		if (power) {
			// Save SRAM
			ASSERT(sram);
			sram->UpdateBoot();
		}
	}

	// Device pointers change as objects are destroyed, so restart from the head
	while (first_device) {
		first_device->Cleanup();
	}

	// Cleanup logging
	log.Cleanup();
}

//---------------------------------------------------------------------------
//
//	Reset
//
//---------------------------------------------------------------------------
void FASTCALL VM::Reset()
{
	Device *device;

	ASSERT(this);

	// Reset logging
	log.Reset();

	// Get the device pointer list head
	device = first_device;

	// Reset each device in sequence
	while (device) {
		device->Reset();
		device = device->GetNextDevice();
	}

	// Clear the current state
	Clear();
}


//---------------------------------------------------------------------------
//
//  Execution stop
//
//---------------------------------------------------------------------------
void FASTCALL VM::Break()
{
	ASSERT(this);
	ASSERT(scheduler);

	scheduler->Break();
}

//---------------------------------------------------------------------------
//
//  NMI interrupt
//
//---------------------------------------------------------------------------
void FASTCALL VM::Interrupt() const
{
	ASSERT(this);
	ASSERT(cpu);

	cpu->Interrupt(7, -1);
}
void FASTCALL VM::SetHostMessageCallback(host_message_callback_t callback, void *user)
{
	ASSERT(this);

	host_message_callback = callback;
	host_message_user = user;
}

void FASTCALL VM::SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user)
{
	ASSERT(this);

	host_lock_vm_callback = lock_vm_cb;
	host_unlock_vm_callback = unlock_vm_cb;
	host_sync_user = user;

	Windrv* pWindrv = (Windrv*)SearchDevice(MAKEID('W', 'D', 'R', 'V'));
	if (!pWindrv) {
		return;
	}

	pWindrv->SetHostSyncCallbacks(lock_vm_cb, unlock_vm_cb, user);
}


void FASTCALL VM::SetHostServices(const host_services_t *services)
{
	ASSERT(this);

	if (!services) {
		SetHostMessageCallback(NULL, NULL);
		SetHostSyncCallbacks(NULL, NULL, NULL);
		return;
	}

	SetHostMessageCallback(services->message, services->user);
	SetHostSyncCallbacks(services->lock_vm, services->unlock_vm, services->user);
}

void FASTCALL VM::SetHostFileSystem(FileSys *fs)
{
	ASSERT(this);

	Windrv* pWindrv = (Windrv*)SearchDevice(MAKEID('W', 'D', 'R', 'V'));
	if (!pWindrv) {
		return;
	}

	pWindrv->SetFileSys(fs);
}
void FASTCALL VM::NotifyHostMessage(const TCHAR* message) const
{
	ASSERT(this);
	ASSERT(message);

	if (host_message_callback) {
		host_message_callback(message, host_message_user);
		return;
	}

#if defined(_WIN32)
	::OutputDebugStringA(message);
	::OutputDebugStringA(_T("\n"));
#else
	fprintf(stderr, "%s\n", message);
#endif
}

DWORD FASTCALL VM::Save(const Filepath& path)
{
    return OriginalSave(path);
}

DWORD FASTCALL VM::Save(Fileio& fio)
{
    return OriginalSave(fio);
}

DWORD FASTCALL VM::OriginalSave(const Filepath& path)
{
    Fileio fio;
    DWORD pos;

    ASSERT(this);
    current.Clear();

    if (!fio.Open(path, Fileio::WriteOnly)) {
        return 0;
    }
    pos = OriginalSave(fio);
    fio.Close();
    if (pos != 0) {
        current = path;
    }
    return pos;
}

DWORD FASTCALL VM::OriginalSave(Fileio& fio)
{
    char header[0x10];
    int ver;
    Device *device;
    DWORD id;
    DWORD pos;

    ASSERT(this);
    ASSERT(fio.IsValid());

    device = first_device;
    ver = (int)((major_ver << 8) | minor_ver);

    sprintf(header, "XM6 DATA %1X.%02X", major_ver, minor_ver);
    header[0x0d] = 0x0d;
    header[0x0e] = 0x0a;
    header[0x0f] = 0x1a;

    if (!fio.Write(header, 0x10)) {
        return 0;
    }

    while (device) {
        id = device->GetID();
        if (!fio.Write(&id, sizeof(id))) {
            return 0;
        }
        if (!device->Save(&fio, ver)) {
            return 0;
        }
        device = device->GetNextDevice();
    }

    id = MAKEID('E', 'N', 'D', ' ');
    if (!fio.Write(&id, sizeof(id))) {
        return 0;
    }

    pos = fio.GetFilePos();
    return pos;
}

//---------------------------------------------------------------------------
//
//	Load
//
//---------------------------------------------------------------------------
DWORD FASTCALL VM::Load(const Filepath& path)
{
    Fileio fio;
    DWORD pos;

    ASSERT(this);
    current.Clear();

    if (!fio.Open(path, Fileio::ReadOnly)) {
        return 0;
    }
    pos = Load(fio);
    fio.Close();
    if (pos != 0) {
        current = path;
    }
    return pos;
}

DWORD FASTCALL VM::Load(Fileio& fio)
{
    char buf[0x10];
    int rec;
    int ver;
    Device *device;
    DWORD id;
    DWORD pos;

    ASSERT(this);
    ASSERT(fio.IsValid());
    current.Clear();

    if (!fio.Read(buf, 0x10)) {
        NotifyHostMessage(_T("savestate load failed: short header"));
        return 0;
    }

    buf[0x0a] = '\0';
    rec = ::strtoul(&buf[0x09], NULL, 16);
    rec <<= 8;
    buf[0x0d] = '\0';
    rec |= ::strtoul(&buf[0x0b], NULL, 16);

    ver = (int)((major_ver << 8) | minor_ver);

    buf[0x09] = '\0';
    if (strcmp(buf, "XM6 DATA ") != 0) {
        NotifyHostMessage(_T("savestate load failed: invalid header"));
        return 0;
    }
    if (ver < rec) {
        NotifyHostMessage(_T("savestate load failed: unsupported version"));
        return 0;
    }

    for (;;) {
        if (!fio.Read(&id, sizeof(id))) {
            NotifyHostMessage(_T("savestate load failed: truncated device list"));
            return 0;
        }
        if (id == MAKEID('E', 'N', 'D', ' ')) {
            break;
        }
        device = SearchDevice(id);
        if (!device) {
            NotifyHostMessage(_T("savestate load failed: unknown device"));
            return 0;
        }
        if (!device->Load(&fio, rec)) {
            NotifyHostMessage(_T("savestate load failed: device load error"));
            return 0;
        }
    }
    pos = fio.GetFilePos();
    return pos;
}
void FASTCALL VM::GetPath(Filepath& path) const
{
	ASSERT(this);

	path = current;
}

//---------------------------------------------------------------------------
//
//	Path clear
//
//---------------------------------------------------------------------------
void FASTCALL VM::Clear()
{
	ASSERT(this);

	current.Clear();
}

//---------------------------------------------------------------------------
//
//	Apply settings
//
//---------------------------------------------------------------------------

void FASTCALL VM::ApplyCfg(const Config *config)
{
	Device *device;

	ASSERT(this);
	ASSERT(config);

	device = first_device;
	while (device) {
		device->ApplyCfg(config);
		device = device->GetNextDevice();
	}
}
BOOL FASTCALL VM::SetRenderMode(int mode)
{
	Render *render;

	ASSERT(this);
	render = (Render*)SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		return FALSE;
	}
	if (!render->SetCompositorMode(mode)) {
		return FALSE;
	}
	musashi_set_render_mode(mode);
	return TRUE;
}

int FASTCALL VM::GetRenderMode() const
{
	Render *render;

	ASSERT(this);
	render = (Render*)SearchDevice(MAKEID('R', 'E', 'N', 'D'));
	if (!render) {
		return Render::compositor_original;
	}
	return render->GetCompositorMode();
}
//---------------------------------------------------------------------------
//
//	Device add
//	The Device is called when added
//
//---------------------------------------------------------------------------
void FASTCALL VM::AddDevice(Device *device)
{
	Device *dev;

	ASSERT(this);
	ASSERT(device);

	// First device
	if (!first_device) {
		// This device becomes the first registered
		first_device = device;
		ASSERT(!device->GetNextDevice());
		return;
	}

	// Search for terminal
	dev = first_device;
	while (dev->GetNextDevice()) {
		dev = dev->GetNextDevice();
	}

	// Add after dev
	dev->SetNextDevice(device);
	ASSERT(!device->GetNextDevice());
}

//---------------------------------------------------------------------------
//
//	Device delete
//	The Device is called when deleted
//
//---------------------------------------------------------------------------
void FASTCALL VM::DelDevice(const Device *device)
{
	Device *dev;

	ASSERT(this);
	ASSERT(device);

	// First device
	if (first_device == device) {
		// If present, unlink and set to NULL if not registered
		if (device->GetNextDevice()) {
			first_device = device->GetNextDevice();
		}
		else {
			first_device = NULL;
		}
		return;
	}

	// Search for the sub-device containing device
	dev = first_device;
	while (dev->GetNextDevice() != device) {
		ASSERT(dev->GetNextDevice());
		dev = dev->GetNextDevice();
	}

	// Skip device->next_device and link to dev
	dev->SetNextDevice(device->GetNextDevice());
}

//---------------------------------------------------------------------------
//
//	Device search
//	Returns NULL if not found
//
//---------------------------------------------------------------------------
Device* FASTCALL VM::SearchDevice(DWORD id) const
{
	Device *dev;

	ASSERT(this);

	// Search from first device
	dev = first_device;

	// Linear search
	while (dev) {
		// Check if ID matches
		if (dev->GetID() == id) {
			return dev;
		}

		// Next
		dev = dev->GetNextDevice();
	}

	// Not found
	return NULL;
}

//---------------------------------------------------------------------------
//
//	Execute
//
//---------------------------------------------------------------------------
BOOL FASTCALL VM::Exec(DWORD hus)
{
	DWORD ret;

	ASSERT(this);
	ASSERT(scheduler);

	// Power check
	if (power) {
		// Execution loop
		while (hus > 0) {
			ret = scheduler->Exec(hus);

			// If valid, advance the virtual time
			if (ret < 0x80000000) {
				hus -= ret;
				continue;
			}

			// Return FALSE if stopped by break
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	Trace
//
//---------------------------------------------------------------------------
void FASTCALL VM::Trace()
{
	ASSERT(this);
	ASSERT(scheduler);

	// Power check
	if (!power) {
		return;
	}

	// Execute until break
	for (;;) {
		if (scheduler->Trace(100) != 0) {
			return;
		}
	}
}

//---------------------------------------------------------------------------
//
//	Power switch processing
//
//---------------------------------------------------------------------------
void FASTCALL VM::PowerSW(BOOL sw)
{
	ASSERT(this);

	// If the current state and switch are the same, do nothing
	if (power_sw == sw) {
		return;
	}

	// Save
	power_sw = sw;

	// When power is off/on, reset with power on/off
	if (sw) {
		SetPower(TRUE);
	}

	// For MFP, set the power signal
	ASSERT(mfp);
	if (sw) {
		mfp->SetGPIP(2, 0);
	}
	else {
		mfp->SetGPIP(2, 1);
	}
}

//---------------------------------------------------------------------------
//
//	Power state setting
//
//---------------------------------------------------------------------------
void FASTCALL VM::SetPower(BOOL flag)
{
 ASSERT(this);

 // If it matches the power already, do nothing
 if (flag == power) {
  return;
 }

 // Power
 power = flag;

 if (flag) {
  // Power ON (performs a reset)
  Reset();
  ASSERT(rtc);
  rtc->Adjust(FALSE);
 }
}

//---------------------------------------------------------------------------
//
//	Version setting
//
//---------------------------------------------------------------------------
void FASTCALL VM::SetVersion(DWORD major, DWORD minor)
{
ASSERT(this);
ASSERT(major < 0x100);
ASSERT(minor < 0x100);

major_ver = major;
minor_ver = minor;
}

//---------------------------------------------------------------------------
//
//	Version get
//
//---------------------------------------------------------------------------
void FASTCALL VM::GetVersion(DWORD& major, DWORD& minor)
{
ASSERT(this);
ASSERT(major_ver < 0x100);
ASSERT(minor_ver < 0x100);

major = major_ver;
minor = minor_ver;
}




