//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ ���z�}�V�� ]
//
//---------------------------------------------------------------------------

#include <windows.h>
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
#include <vector>
#include <algorithm>

//===========================================================================
//
//	���z�}�V��
//
//===========================================================================

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
VM::VM()
{
	// ���[�N������
	status = FALSE;
	first_device = NULL;
	scheduler = NULL;

	// �f�o�C�XNULL
	scheduler = NULL;
	cpu = NULL;
	mfp = NULL;
	rtc = NULL;
	sram = NULL;
	host_message_callback = NULL;
	host_message_user = NULL;

	// �o�[�W����(���ۂ̓v���b�g�t�H�[������Đݒ肳���)
	major_ver = 0x01;
	minor_ver = 0x00;

	// �J�����g�p�X��N���A
	Clear();
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL VM::Init()
{
	Device *device;

	ASSERT(this);
	ASSERT(!first_device);
	ASSERT(!status);

	// �d���A�d���X�C�b�`on
	power = TRUE;
	power_sw = TRUE;

	// �f�o�C�X��쐬(�����ɒ���)
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

	// ���O�������
	if (!log.Init(this)) {
		return FALSE;
	}

	// �f�o�C�X�|�C���^������
	device = first_device;

	// ������(���Ԃɉ��)
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
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL VM::Cleanup()
{
	ASSERT(this);

	// �d����ON�̏�Ԃŋ����I�������ꍇ�ASRAM�̋N���J�E���^��X�V����
	if (status) {
		if (power) {
			// SRAM�X�V
			ASSERT(sram);
			sram->UpdateBoot();
		}
	}

	// �|�C���^�͕ύX�����̂ŁA�擪��������
	while (first_device) {
		first_device->Cleanup();
	}

	// ���O��N���[���A�b�v
	log.Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL VM::Reset()
{
	Device *device;

	ASSERT(this);

	// ���O����Z�b�g
	log.Reset();

	// �f�o�C�X�|�C���^������
	device = first_device;

	// ���Z�b�g(���Ԃɉ��)
	while (device) {
		device->Reset();
		device = device->GetNextDevice();
	}

	// �J�����g�p�X��N���A
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

	Windrv* pWindrv = (Windrv*)SearchDevice(MAKEID('W', 'D', 'R', 'V'));
	ASSERT(pWindrv);
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
	ASSERT(pWindrv);
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

	::OutputDebugString(message);
	::OutputDebugString(_T("\n"));
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
//	���[�h
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
//	�p�X�N���A
//
//---------------------------------------------------------------------------
void FASTCALL VM::Clear()
{
	ASSERT(this);

	current.Clear();
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
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
	ASSERT(this);

	if (mode == 0 || mode == 1) {
		return TRUE;
	}
	return FALSE;
}

int FASTCALL VM::GetRenderMode() const
{
	ASSERT(this);
	return 0;
}
//---------------------------------------------------------------------------
//
//	�f�o�C�X�ǉ�
//	���ǉ�������Device����Ăяo��
//
//---------------------------------------------------------------------------
void FASTCALL VM::AddDevice(Device *device)
{
	Device *dev;

	ASSERT(this);
	ASSERT(device);

	// �ŏ��̃f�o�C�X��
	if (!first_device) {
		// ���̃f�o�C�X���ŏ��B�o�^����
		first_device = device;
		ASSERT(!device->GetNextDevice());
		return;
	}

	// �I�[��T��
	dev = first_device;
	while (dev->GetNextDevice()) {
		dev = dev->GetNextDevice();
	}

	// dev�̌��ɒǉ�
	dev->SetNextDevice(device);
	ASSERT(!device->GetNextDevice());
}

//---------------------------------------------------------------------------
//
//	�f�o�C�X�폜
//	���폜������Device����Ăяo��
//
//---------------------------------------------------------------------------
void FASTCALL VM::DelDevice(const Device *device)
{
	Device *dev;

	ASSERT(this);
	ASSERT(device);

	// �ŏ��̃f�o�C�X��
	if (first_device == device) {
		// ��������Ȃ�A����o�^�B�Ȃ����NULL
		if (device->GetNextDevice()) {
			first_device = device->GetNextDevice();
		}
		else {
			first_device = NULL;
		}
		return;
	}

	// device��L�����Ă���T�u�E�B���h�E��T��
	dev = first_device;
	while (dev->GetNextDevice() != device) {
		ASSERT(dev->GetNextDevice());
		dev = dev->GetNextDevice();
	}

	// device->next_device��Adev�Ɍ��т��X�L�b�v������
	dev->SetNextDevice(device->GetNextDevice());
}

//---------------------------------------------------------------------------
//
//	�f�o�C�X����
//	��������Ȃ����NULL��Ԃ�
//
//---------------------------------------------------------------------------
Device* FASTCALL VM::SearchDevice(DWORD id) const
{
	Device *dev;

	ASSERT(this);

	// �f�o�C�X�������
	dev = first_device;

	// �������[�v
	while (dev) {
		// ID����v���邩�`�F�b�N
		if (dev->GetID() == id) {
			return dev;
		}

		// ����
		dev = dev->GetNextDevice();
	}

	// ������Ȃ�����
	return NULL;
}

//---------------------------------------------------------------------------
//
//	���s
//
//---------------------------------------------------------------------------
BOOL FASTCALL VM::Exec(DWORD hus)
{
	DWORD ret;

	ASSERT(this);
	ASSERT(scheduler);

	// �d���`�F�b�N
	if (power) {
		// ���s���[�v
		while (hus > 0) {
			ret = scheduler->Exec(hus);

			// ����Ȃ�A�c��^�C������炷
			if (ret < 0x80000000) {
				hus -= ret;
				continue;
			}

			// �u���[�N������AFALSE�ŏI��
			return FALSE;
		}
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�g���[�X
//
//---------------------------------------------------------------------------
void FASTCALL VM::Trace()
{
	ASSERT(this);
	ASSERT(scheduler);

	// �d���`�F�b�N
	if (!power) {
		return;
	}

	// 0�ȊO���o��܂Ŏ��s
	for (;;) {
		if (scheduler->Trace(100) != 0) {
			return;
		}
	}
}

//---------------------------------------------------------------------------
//
//	�d���X�C�b�`����
//
//---------------------------------------------------------------------------
void FASTCALL VM::PowerSW(BOOL sw)
{
	ASSERT(this);

	// ���݂̏�ԂƓ����Ȃ牽����Ȃ�
	if (power_sw == sw) {
		return;
	}

	// �L������
	power_sw = sw;

	// �d���I�t�Ȃ�A�d���I���Ń��Z�b�g
	if (sw) {
		SetPower(TRUE);
	}

	// MFP�ɑ΂��A�d������`����
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
//	�d���̏�Ԃ�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL VM::SetPower(BOOL flag)
{
	ASSERT(this);

	// ��v���Ă���Ή�����Ȃ�
	if (flag == power) {
		return;
	}

	// ��v
	power = flag;

	if (flag) {
		// �d��ON(�����A�W���X�g��s��)
		Reset();
		ASSERT(rtc);
		rtc->Adjust(FALSE);
	}
}

//---------------------------------------------------------------------------
//
//	�o�[�W�����ݒ�
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
//	�o�[�W�����擾
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




