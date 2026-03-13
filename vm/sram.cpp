//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �X�^�e�B�b�NRAM ]
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "vm.h"
#include "log.h"
#include "filepath.h"
#include "fileio.h"
#include "cpu.h"
#include "schedule.h"
#include "memory.h"
#include "config.h"
#include "sram.h"

#include "sram_default.h"

//===========================================================================
//
//	�X�^�e�B�b�NRAM
//
//===========================================================================
//#define SRAM_LOG

//---------------------------------------------------------------------------
//
//	�R���X�g���N�^
//
//---------------------------------------------------------------------------
SRAM::SRAM(VM *p) : MemDevice(p)
{
	// �f�o�C�XID��������
	dev.id = MAKEID('S', 'R', 'A', 'M');
	dev.desc = "Static RAM";

	// �J�n�A�h���X�A�I���A�h���X
	memdev.first = 0xed0000;
	memdev.last = 0xedffff;

	// ���̑�������
	sram_size = 16;
	write_en = FALSE;
	mem_sync = TRUE;
	changed = FALSE;
}

//---------------------------------------------------------------------------
//
//	������
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Init()
{
	Fileio fio;
	int i;
	BYTE data;

	ASSERT(this);

	// ��{�N���X
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// ������

	ASSERT(this);

	// {NX
	if (!MemDevice::Init()) {
		return FALSE;
	}

	// 
	memset(sram, 0xff, sizeof(sram));

	// pX쐬
	sram_path.SysFile(Filepath::SRAM);

#if defined(XM6_FORCE_DEFAULT_SRAM)
	// libretro用: セッションの初回起動時のみデフォルトを適用してSRAM.DATを初期化
	// 以降のリセット時などは以前の値を保持するため読込を試行する
	static bool session_initialized = false;
	if (!session_initialized) {
		memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
		fio.Save(sram_path, (void *)kDefaultSramFile, sizeof(kDefaultSramFile));
		session_initialized = true;
		LOG0(Log::Normal, "SRAM: Initial session initialization (Template applied)");
	} else {
		// リセット時は保存されている設定の復元を試みる
		if (!fio.Load(sram_path, sram, sizeof(sram))) {
			memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
			LOG0(Log::Warning, "SRAM: SRAM.DAT not found during reset, using defaults");
		} else {
			LOG0(Log::Normal, "SRAM: Persistence restored after reset");
		}
	}
#else
	// MFC用: SRAM.DATが存在すればその値を使用、存在しない場合のみデフォルトで初期化
	if (!fio.Load(sram_path, sram, sizeof(sram))) {
		memcpy(sram, kDefaultSramFile, sizeof(kDefaultSramFile));
		fio.Save(sram_path, (void *)kDefaultSramFile, sizeof(kDefaultSramFile));
	}
#endif

	// �G���f�B�A�����]
	for (i=0; i<sizeof(sram); i+=2) {
		data = sram[i];
		sram[i] = sram[i + 1];
		sram[i + 1] = data;
	}

	// �ύX�Ȃ�
	ASSERT(!changed);

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�N���[���A�b�v
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::Cleanup()
{
	Fileio fio;
	int i;
	BYTE data;

	ASSERT(this);

	// �ύX�����
	if (changed) {
		// �G���f�B�A�����]
		for (i=0; i<sizeof(sram); i+=2) {
			data = sram[i];
			sram[i] = sram[i + 1];
			sram[i + 1] = data;
		}

		// ��������
		fio.Save(sram_path, sram, sram_size << 10);
	}

	// ��{�N���X��
	MemDevice::Cleanup();
}

//---------------------------------------------------------------------------
//
//	���Z�b�g
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::Reset()
{
	ASSERT(this);
	ASSERT_DIAG();

	LOG0(Log::Normal, "���Z�b�g");

	// �������݋֎~�ɏ�����
	write_en = FALSE;
}

//---------------------------------------------------------------------------
//
//	�Z�[�u
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Save(Fileio *fio, int /*ver*/)
{
	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "���[�h");

	// SRAM�T�C�Y
	if (!fio->Write(&sram_size, sizeof(sram_size))) {
		return FALSE;
	}

	// SRAM�{��(64KB�܂Ƃ߂�)
	if (!fio->Write(&sram, sizeof(sram))) {
		return FALSE;
	}

	// �������݋��t���O
	if (!fio->Write(&write_en, sizeof(write_en))) {
		return FALSE;
	}

	// �����t���O
	if (!fio->Write(&mem_sync, sizeof(mem_sync))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	���[�h
//
//---------------------------------------------------------------------------
BOOL FASTCALL SRAM::Load(Fileio *fio, int /*ver*/)
{
	BYTE *buf;

	ASSERT(this);
	ASSERT(fio);
	ASSERT_DIAG();

	LOG0(Log::Normal, "���[�h");

	// �o�b�t�@�m��
	try {
		buf = new BYTE[sizeof(sram)];
	}
	catch (...) {
		buf = NULL;
	}
	if (!buf) {
		return FALSE;
	}

	// SRAM�T�C�Y
	if (!fio->Read(&sram_size, sizeof(sram_size))) {
		delete[] buf;
		return FALSE;
	}

	// SRAM�{��(64KB�܂Ƃ߂�)
	if (!fio->Read(buf, sizeof(sram))) {
		delete[] buf;
		return FALSE;
	}

	// ��r�Ɠ]��
	if (memcmp(sram, buf, sizeof(sram)) != 0) {
		memcpy(sram, buf, sizeof(sram));
		changed = TRUE;
	}

	// ��Ƀo�b�t�@���
	delete[] buf;
	buf = NULL;

	// �������݋��t���O
	if (!fio->Read(&write_en, sizeof(write_en))) {
		return FALSE;
	}

	// �����t���O
	if (!fio->Read(&mem_sync, sizeof(mem_sync))) {
		return FALSE;
	}

	return TRUE;
}

//---------------------------------------------------------------------------
//
//	�ݒ�K�p
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::ApplyCfg(const Config *config)
{
	ASSERT(this);
	ASSERT(config);
	ASSERT_DIAG();

	LOG0(Log::Normal, "�ݒ�K�p");

	// SRAM�T�C�Y
	if (config->sram_64k) {
		sram_size = 64;
#if defined(SRAM_LOG)
		LOG0(Log::Detail, "�������T�C�Y 64KB");
#endif	// SRAM_LOG
	}
	else {
		sram_size = 16;
	}

	// �������X�C�b�`��������
	mem_sync = config->ram_sramsync;
}

#if !defined(NDEBUG)
//---------------------------------------------------------------------------
//
//	�f�f
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::AssertDiag() const
{
	// ��{�N���X
	MemDevice::AssertDiag();

	ASSERT(this);
	ASSERT(GetID() == MAKEID('S', 'R', 'A', 'M'));
	ASSERT(memdev.first == 0xed0000);
	ASSERT(memdev.last == 0xedffff);
	ASSERT((sram_size == 16) || (sram_size == 32) || (sram_size == 48) || (sram_size == 64));
	ASSERT((write_en == TRUE) || (write_en == FALSE));
	ASSERT((mem_sync == TRUE) || (mem_sync == FALSE));
	ASSERT((changed == TRUE) || (changed == FALSE));
}
#endif	// NDEBUG

//---------------------------------------------------------------------------
//
//	�o�C�g�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadByte(DWORD addr)
{
	DWORD size;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// �I�t�Z�b�g�Z�o
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// �������`�F�b�N
	if (size <= addr) {
		// �o�X�G���[
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// �E�F�C�g
	scheduler->Wait(1);

	// �ǂݍ���(�G���f�B�A���𔽓]������)
	return (DWORD)sram[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	���[�h�ǂݍ���
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadWord(DWORD addr)
{
	DWORD size;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT_DIAG();

	// �I�t�Z�b�g�Z�o
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// �������`�F�b�N
	if (size <= addr) {
		// �o�X�G���[
		cpu->BusErr(memdev.first + addr, TRUE);
		return 0xff;
	}

	// �E�F�C�g
	scheduler->Wait(1);

	// �ǂݍ���(�G���f�B�A���ɒ���)
	return (DWORD)(*(WORD *)&sram[addr]);
}

//---------------------------------------------------------------------------
//
//	�o�C�g��������
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteByte(DWORD addr, DWORD data)
{
	DWORD size;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	// �I�t�Z�b�g�Z�o
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// �������`�F�b�N
	if (size <= addr) {
		// �o�X�G���[
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// �������݉\�`�F�b�N
	if (!write_en) {
		LOG1(Log::Warning, "�������݋֎~ $%06X", memdev.first + addr);
		return;
	}

	// �E�F�C�g
	scheduler->Wait(1);

	// �A�h���X$09��$00 or $10���������܂��ꍇ�A�������X�C�b�`�����X�V�ł���΃X�L�b�v������
	// (���Z�b�g����Memory::Reset���珑�����܂�Ă��邽�߁A�㏑���ɂ��j���h��)
	if ((addr == 0x09) && (data == 0x10)) {
		if (cpu->GetPC() == 0xff03a8) {
			if (mem_sync) {
				LOG2(Log::Warning, "�X�C�b�`�ύX�}�� $%06X <- $%02X", memdev.first + addr, data);
				return;
			}
		}
	}

	// ��������(�G���f�B�A���𔽓]������)
	if (addr < 0x100) {
		LOG2(Log::Detail, "�������X�C�b�`�ύX $%06X <- $%02X", memdev.first + addr, data);
	}
	if (sram[addr ^ 1] != (BYTE)data) {
		sram[addr ^ 1] = (BYTE)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	���[�h��������
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteWord(DWORD addr, DWORD data)
{
	DWORD size;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT((addr & 1) == 0);
	ASSERT(data < 0x10000);
	ASSERT_DIAG();

	// �I�t�Z�b�g�Z�o
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// �������`�F�b�N
	if (size <= addr) {
		// �o�X�G���[
		cpu->BusErr(memdev.first + addr, FALSE);
		return;
	}

	// �������݉\�`�F�b�N
	if (!write_en) {
		LOG1(Log::Warning, "�������݋֎~ $%06X", memdev.first + addr);
		return;
	}

	// �E�F�C�g
	scheduler->Wait(1);

	// ��������(�G���f�B�A���ɒ���)
	if (addr < 0x100) {
		LOG2(Log::Detail, "�������X�C�b�`�ύX $%06X <- $%04X", memdev.first + addr, data);
	}
	if (*(WORD *)&sram[addr] != (WORD)data) {
		*(WORD *)&sram[addr] = (WORD)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	�ǂݍ��݂̂�
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::ReadOnly(DWORD addr) const
{
	DWORD size;

	ASSERT(this);
	ASSERT((addr >= memdev.first) && (addr <= memdev.last));
	ASSERT_DIAG();

	// �I�t�Z�b�g�Z�o
	addr -= memdev.first;
	size = (DWORD)(sram_size << 10);

	// �������`�F�b�N
	if (size <= addr) {
		return 0xff;
	}

	// �ǂݍ���(�G���f�B�A���𔽓]������)
	return (DWORD)sram[addr ^ 1];
}

//---------------------------------------------------------------------------
//
//	SRAM�A�h���X�擾
//
//---------------------------------------------------------------------------
const BYTE* FASTCALL SRAM::GetSRAM() const
{
	ASSERT(this);
	ASSERT_DIAG();

	return sram;
}

//---------------------------------------------------------------------------
//
//	SRAM�T�C�Y�擾
//
//---------------------------------------------------------------------------
int FASTCALL SRAM::GetSize() const
{
	ASSERT(this);
	ASSERT_DIAG();

	return sram_size;
}

//---------------------------------------------------------------------------
//
//	�������݋���
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::WriteEnable(BOOL enable)
{
	ASSERT(this);
	ASSERT_DIAG();

	write_en = enable;

	if (write_en) {
		LOG0(Log::Detail, "SRAM�������݋���");
	}
	else {
		LOG0(Log::Detail, "SRAM�������݋֎~");
	}
}

//---------------------------------------------------------------------------
//
//	�������X�C�b�`�ݒ�
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::SetMemSw(DWORD offset, DWORD data)
{
	ASSERT(this);
	ASSERT(offset < 0x100);
	ASSERT(data < 0x100);
	ASSERT_DIAG();

	LOG2(Log::Detail, "�������X�C�b�`�ݒ� $%06X <- $%02X", memdev.first + offset, data);
	if (sram[offset ^ 1] != (BYTE)data) {
		sram[offset ^ 1] = (BYTE)data;
		changed = TRUE;
	}
}

//---------------------------------------------------------------------------
//
//	�������X�C�b�`�擾
//
//---------------------------------------------------------------------------
DWORD FASTCALL SRAM::GetMemSw(DWORD offset) const
{
	ASSERT(this);
	ASSERT(offset < 0x100);
	ASSERT_DIAG();

	return (DWORD)sram[offset ^ 1];
}

//---------------------------------------------------------------------------
//
//	�N���J�E���^�X�V
//
//---------------------------------------------------------------------------
void FASTCALL SRAM::UpdateBoot()
{
	WORD *ptr;

	ASSERT(this);
	ASSERT_DIAG();

	// ��ɕύX����
	changed = TRUE;

	// �|�C���^�ݒ�($ED0044)
	ptr = (WORD *)&sram[0x0044];

	// �C���N�������g(Low)
	if (ptr[1] != 0xffff) {
		ptr[1] = ptr[1] + 1;
		return;
	}

	// �C���N�������g(High)
	ptr[1] = 0x0000;
	ptr[0] = ptr[0] + 1;
}
