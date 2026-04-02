//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2004 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ ���z�}�V���R�A �A�Z���u���T�u ]
//
//---------------------------------------------------------------------------

#if !defined (core_asm_h)
#define core_asm_h

#include <stdint.h>

//#if _MSC_VER >= 1200

#if defined(__cplusplus)
extern "C" {
#endif	//__cplusplus

//---------------------------------------------------------------------------
//
//	�v���g�^�C�v�錾
//
//---------------------------------------------------------------------------
void MemInitDecode(Memory *mem, MemDevice* list[]);
										// �������f�R�[�_������
DWORD ReadByteC(DWORD addr);
										// �o�C�g�ǂݍ���
DWORD ReadWordC(DWORD addr);
										// ���[�h�ǂݍ���
void WriteByteC(DWORD addr, DWORD data);
										// �o�C�g��������
void WriteWordC(DWORD addr, DWORD data);
										// ���[�h��������
void ReadErrC(DWORD addr);
										// �o�X�G���[�ǂݍ���
void WriteErrC(DWORD addr, DWORD data);
										// �o�X�G���[��������
void NotifyEvent(Event *first);
										// �C�x���g�Q �w��
DWORD GetMinEvent(DWORD hus);
										// �C�x���g�Q �ŏ��̂��̂�T��
BOOL SubExecEvent(DWORD hus);
										// �C�x���g�Q ���Z�����s
extern uintptr_t MemDecodeTable[];
										// �������f�R�[�h�e�[�u��

#if defined(__cplusplus)
}
#endif	//__cplusplus

//#endif	// _MSC_VER
#endif	// mem_asm_h
