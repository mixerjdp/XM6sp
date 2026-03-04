//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ ������ ]
//
//---------------------------------------------------------------------------

#if !defined(memory_h)
#define memory_h

#include "device.h"

//===========================================================================
//
//	�O���֐�
//
//===========================================================================
#if defined(__cplusplus)
extern "C" {
#endif	// __cplusplus
void ReadBusErr(DWORD addr);
										// �ǂݍ��݃o�X�G���[
void WriteBusErr(DWORD addr);
										// �������݃o�X�G���[
#if defined(__cplusplus)
}
#endif	// __cplusplus

//===========================================================================
//
//	������
//
//===========================================================================
class Memory : public MemDevice
{
public:
	// ���������(=�V�X�e�����)
	enum memtype {
		None,							// ���[�h����Ă��Ȃ�
		SASI,							// v1.00-SASI(����/ACE/EXPERT/PRO)
		SCSIInt,						// v1.00-SCSI���(SUPER)
		SCSIExt,						// v1.00-SCSI�O�t�{�[�h(����/ACE/EXPERT/PRO)
		XVI,							// v1.10-SCSI���(XVI)
		Compact,						// v1.20-SCSI���(Compact)
		X68030							// v1.50-SCSI���(X68030)
	};

	// ����f�[�^��`
	typedef struct {
		MemDevice* table[0x180];		// �W�����v�e�[�u��
		int size;						// RAM�T�C�Y(2,4,6,8,10,12)
		int config;						// RAM�ݒ�l(0�`5)
		DWORD length;					// RAM�ŏI�o�C�g+1
		BYTE *ram;						// ���C��RAM
		BYTE *ipl;						// IPL ROM (128KB)
		BYTE *cg;						// CG ROM(768KB)
		BYTE *scsi;						// SCSI ROM (8KB)
		memtype type;					// ���������(���Z�b�g��)
		memtype now;					// ���������(�J�����g)
		BOOL memsw;						// �������X�C�b�`�����X�V
	} memory_t;

public:
	// ��{�t�@���N�V����
	Memory(VM *p);
										// �R���X�g���N�^
	BOOL FASTCALL Init();
										// ������
	void FASTCALL Cleanup();
										// �N���[���A�b�v
	void FASTCALL Reset();
										// ���Z�b�g
	BOOL FASTCALL Save(Fileio *fio, int ver);
										// �Z�[�u
	BOOL FASTCALL Load(Fileio *fio, int ver);
										// ���[�h
	void FASTCALL ApplyCfg(const Config *config);
										// �ݒ�K�p

	// �������f�o�C�X
	DWORD FASTCALL ReadByte(DWORD addr);
										// �o�C�g�ǂݍ���
	DWORD FASTCALL ReadWord(DWORD addr);
										// ���[�h�ǂݍ���
	void FASTCALL WriteByte(DWORD addr, DWORD data);
										// �o�C�g��������
	void FASTCALL WriteWord(DWORD addr, DWORD data);
										// ���[�h��������
	DWORD FASTCALL ReadOnly(DWORD addr) const;
										// �ǂݍ��݂̂�

	// �O��API
	void FASTCALL MakeContext(BOOL reset);
										// �������R���e�L�X�g�쐬
	BOOL FASTCALL CheckIPL() const;
										// IPL�o�[�W�����`�F�b�N
	BOOL FASTCALL CheckCG() const;
										// CG�`�F�b�N
	const BYTE* FASTCALL GetCG() const;
										// CG�擾
	const BYTE* FASTCALL GetSCSI() const;
										// SCSI�擾
	memtype FASTCALL GetMemType() const { return mem.now; }
										// ��������ʎ擾

private:
	BOOL FASTCALL LoadROM(memtype target);
										// ROM���[�h
	void FASTCALL InitTable();
										// init table

	AreaSet *areaset;
	SRAM *sram;
	memory_t mem;
};

#endif	// memory_h
