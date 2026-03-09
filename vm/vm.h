//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ ���z�}�V�� ]
//
//---------------------------------------------------------------------------

#if !defined(vm_h)
#define vm_h

#include "log.h"
#include "schedule.h"
#include "cpu.h"
#include "filepath.h"
#include "host_services.h"

class Fileio;

//===========================================================================
//
//	���z�}�V��
//
//===========================================================================
class VM
{
public:

	// ��{�t�@���N�V����
	VM();
										// �R���X�g���N�^
	BOOL FASTCALL Init();
										// ������
	void FASTCALL Cleanup();
										// �N���[���A�b�v
	void FASTCALL Reset();
										// ���Z�b�g
	void FASTCALL ApplyCfg(const Config *config);
	BOOL FASTCALL SetRenderMode(int mode);
	int FASTCALL GetRenderMode() const;
										// �ݒ�K�p

	// �X�e�[�g�ۑ�
	DWORD FASTCALL OriginalSave(const Filepath& path);
	DWORD FASTCALL OriginalSave(Fileio& fio);
	
	DWORD FASTCALL Save(const Filepath& path);
	DWORD FASTCALL Save(Fileio& fio);
										// �Z�[�u
	DWORD FASTCALL Load(const Filepath& path);
	DWORD FASTCALL Load(Fileio& fio);
										// ���[�h
	void FASTCALL GetPath(Filepath& path) const;
										// �p�X�擾
	void FASTCALL Clear();
										// �p�X��N���A

	// �f�o�C�X�Ǘ�
	void FASTCALL AddDevice(Device *device);
										// �f�o�C�X�ǉ�(�q����Ă΂��)
	void FASTCALL DelDevice(const Device *device);
										// �f�o�C�X�폜(�q����Ă΂��)
	Device* FASTCALL GetFirstDevice() const	{ return first_device; }
										// �ŏ��̃f�o�C�X��擾
	Device* FASTCALL SearchDevice(DWORD id) const;
										// �C��ID�̃f�o�C�X��擾

	// ���s
	BOOL FASTCALL Exec(DWORD hus);
										// ���s
	void FASTCALL Trace();
										// �g���[�X
	void FASTCALL Break();
										// ���s���~

	// �o�[�W����
	void FASTCALL SetVersion(DWORD major, DWORD minor);
										// �o�[�W�����ݒ�
	void FASTCALL GetVersion(DWORD& major, DWORD& minor);
										// �o�[�W�����擾

	// �V�X�e������
	void FASTCALL PowerSW(BOOL sw);
										// �d���X�C�b�`����
	BOOL FASTCALL IsPowerSW() const		{ return power_sw; }
										// �d���X�C�b�`��Ԏ擾
	void FASTCALL SetPower(BOOL flag);
										// �d������
	BOOL FASTCALL IsPower() const		{ return power; }
										// �d����Ԏ擾
	void FASTCALL Interrupt() const;
										// NMI���荞��
	void FASTCALL SetHostMessageCallback(host_message_callback_t callback, void *user);
	void FASTCALL SetHostSyncCallbacks(host_sync_callback_t lock_vm_cb, host_sync_callback_t unlock_vm_cb, void *user);
	void FASTCALL SetHostServices(const host_services_t *services);
	void FASTCALL SetHostFileSystem(FileSys *fs);
						// Host callback for non-fatal messages

	Log log;
										// ���O

private:
	BOOL status;
										// �������X�e�[�^�X
	Device *first_device;
										// �ŏ��̃f�o�C�X
	Scheduler *scheduler;
										// �X�P�W���[��
	CPU *cpu;
										// CPU
	MFP *mfp;
										// MFP
	RTC *rtc;
										// RTC
	SRAM *sram;
										// SRAM
	BOOL power_sw;
										// �d���X�C�b�`
	BOOL power;
										// �d��
	DWORD major_ver;
										// ���W���[�o�[�W����
	DWORD minor_ver;
										// �}�C�i�[�o�[�W����
	void FASTCALL NotifyHostMessage(const TCHAR* message) const;
	host_message_callback_t host_message_callback;
	void *host_message_user;

	Filepath current;
										// �J�����g�f�[�^
};

#endif	// vm_h






