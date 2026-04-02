//----------------------------------------------------------------------------
//
//  Windrv POSIX stub
//  Linux/libretro build: host filesystem bridge is intentionally disabled.
//
//----------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "config.h"
#include "windrv.h"

CWindrv::CWindrv(Windrv* pWindrv, Memory* pMemory, DWORD nHandle)
	: windrv(pWindrv),
	  memory(pMemory),
	  a5(0),
	  unit(0),
	  command(0),
	  m_nHandle(nHandle),
	  m_bAlloc(FALSE),
	  m_bExecute(FALSE),
	  m_bReady(FALSE),
	  m_bTerminate(FALSE),
	  m_hThread(NULL),
	  m_hEventStart(NULL),
	  m_hEventReady(NULL)
{
}

CWindrv::~CWindrv()
{
}

BOOL FASTCALL CWindrv::Start()
{
	return TRUE;
}

BOOL FASTCALL CWindrv::Terminate()
{
	return TRUE;
}

void FASTCALL CWindrv::Execute(DWORD /*nA5*/)
{
}

#ifdef WINDRV_SUPPORT_COMPATIBLE
DWORD FASTCALL CWindrv::ExecuteCompatible(DWORD /*nA5*/)
{
	return 0;
}
#endif // WINDRV_SUPPORT_COMPATIBLE

void FASTCALL CWindrv::LockXM()
{
	if (windrv) {
		windrv->HostLockVM();
	}
}

void FASTCALL CWindrv::UnlockXM()
{
	if (windrv) {
		windrv->HostUnlockVM();
	}
}

void FASTCALL CWindrv::Ready()
{
}

DWORD __stdcall CWindrv::Run(void* /*pThis*/)
{
	return 0;
}

void FASTCALL CWindrv::Runner()
{
}

void FASTCALL CWindrvManager::Init(Windrv* /*pWindrv*/, Memory* /*pMemory*/)
{
	for (int i = 0; i < WINDRV_THREAD_MAX; ++i) {
		m_pThread[i] = NULL;
	}
}

void FASTCALL CWindrvManager::Clean()
{
}

void FASTCALL CWindrvManager::Reset()
{
}

CWindrv* FASTCALL CWindrvManager::Alloc()
{
	return NULL;
}

CWindrv* FASTCALL CWindrvManager::Search(DWORD /*nHandle*/)
{
	return NULL;
}

void FASTCALL CWindrvManager::Free(CWindrv* /*p*/)
{
}

Windrv::Windrv(VM *p) : MemDevice(p)
{
	dev.id = MAKEID('W', 'D', 'R', 'V');
	dev.desc = "Windrv";
	memdev.first = 0xe9e000;
	memdev.last = 0xe9ffff;

	windrv.enable = WINDRV_MODE_DISABLE;
	windrv.drives = 0;
	windrv.fs = NULL;
	windrv.lock_vm_cb = NULL;
	windrv.unlock_vm_cb = NULL;
	windrv.sync_user = NULL;
}

BOOL FASTCALL Windrv::Init()
{
	if (!MemDevice::Init()) {
		return FALSE;
	}
	windrv.enable = WINDRV_MODE_DISABLE;
	windrv.drives = 0;
	return TRUE;
}

void FASTCALL Windrv::Cleanup()
{
	if (windrv.fs) {
		windrv.fs->Reset();
	}
	MemDevice::Cleanup();
}

void FASTCALL Windrv::Reset()
{
	if (windrv.fs) {
		windrv.fs->Reset();
	}
	windrv.drives = 0;
}

BOOL FASTCALL Windrv::Save(Fileio* /*fio*/, int /*ver*/)
{
	return TRUE;
}

BOOL FASTCALL Windrv::Load(Fileio* /*fio*/, int /*ver*/)
{
	return TRUE;
}

void FASTCALL Windrv::ApplyCfg(const Config* pConfig)
{
	if (!pConfig) {
		windrv.enable = WINDRV_MODE_DISABLE;
		return;
	}
	(void)pConfig;
	windrv.enable = WINDRV_MODE_DISABLE;
}

DWORD FASTCALL Windrv::ReadByte(DWORD addr)
{
	return ReadOnly(addr);
}

void FASTCALL Windrv::WriteByte(DWORD /*addr*/, DWORD /*data*/)
{
}

DWORD FASTCALL Windrv::ReadOnly(DWORD addr) const
{
	if (addr == 0xE9F000) {
		return 'X';
	}
	return 0xFF;
}

void FASTCALL Windrv::SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user)
{
	windrv.lock_vm_cb = lock_vm_cb;
	windrv.unlock_vm_cb = unlock_vm_cb;
	windrv.sync_user = user;
}

void FASTCALL Windrv::HostLockVM() const
{
	if (windrv.lock_vm_cb) {
		windrv.lock_vm_cb(windrv.sync_user);
	}
}

void FASTCALL Windrv::HostUnlockVM() const
{
	if (windrv.unlock_vm_cb) {
		windrv.unlock_vm_cb(windrv.sync_user);
	}
}

void FASTCALL Windrv::SetFileSys(FileSys *fs)
{
	if (windrv.fs && windrv.fs != fs) {
		windrv.fs->Reset();
	}
	windrv.fs = fs;
	windrv.drives = 0;
}

BOOL FASTCALL Windrv::isInvalidUnit(DWORD unit) const
{
	return (unit >= windrv.drives) ? TRUE : FALSE;
}

#ifdef WINDRV_LOG
void FASTCALL Windrv::Log(DWORD /*level*/, char* /*message*/) const
{
}
#endif // WINDRV_LOG

#ifdef WINDRV_SUPPORT_COMPATIBLE
void FASTCALL Windrv::Execute()
{
}
#endif // WINDRV_SUPPORT_COMPATIBLE

void FASTCALL Windrv::ExecuteAsynchronous()
{
}

DWORD FASTCALL Windrv::StatusAsynchronous()
{
	return 0;
}

void FASTCALL Windrv::ReleaseAsynchronous()
{
}
