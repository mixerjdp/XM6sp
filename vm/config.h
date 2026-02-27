//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 �o�h�D(ytanaka@ipc-tokai.or.jp)
//	[ �R���t�B�M�����[�V���� ]
//
//---------------------------------------------------------------------------

#if !defined(config_h)
#define config_h

#include "filepath.h"

//===========================================================================
//
//	�R���t�B�M�����[�V����(version2.00�`version2.01)
//
//===========================================================================
class Config200 {
public:
	// �V�X�e��
	int system_clock;					// �V�X�e���N���b�N(0�`5)
	int ram_size;						// ���C��RAM�T�C�Y(0�`5)
	BOOL ram_sramsync;					// �������X�C�b�`�����X�V

	// �X�P�W���[��
	BOOL mpu_fullspeed;					// MPU�t���X�s�[�h
	BOOL vm_fullspeed;					// VM�t���X�s�[�h

	// �T�E���h
	int sound_device;					// �T�E���h�f�o�C�X(0�`15)
	int sample_rate;					// �T���v�����O���[�g(0�`4)
	int primary_buffer;					// �o�b�t�@�T�C�Y(2�`100)
	int polling_buffer;					// �|�[�����O�Ԋu(0�`99)
	BOOL adpcm_interp;					// ADPCM���`��Ԃ���

	// �`��
	BOOL aspect_stretch;				// �A�X�y�N�g��ɂ��킹�g��
	BOOL render_vsync;					// VSync (TRUE=ON)
	int render_mode;					// Renderizador (0=GDI, 1=DirectX 9)

	// ����
	int master_volume;					// �}�X�^����(0�`100)
	BOOL fm_enable;						// FM�L��
	int fm_volume;						// FM����(0�`100)
	BOOL adpcm_enable;					// ADPCM�L��
	int adpcm_volume;					// ADPCM����(0�`100)

	// �L�[�{�[�h
	BOOL kbd_connect;					// �ڑ�

	// �}�E�X
	int mouse_speed;					// �X�s�[�h
	int mouse_port;						// �ڑ��|�[�g
	BOOL mouse_swap;					// �{�^���X���b�v
	BOOL mouse_mid;						// ���{�^���C�l�[�u��
	BOOL mouse_trackb;					// �g���b�N�{�[�����[�h

	// �W���C�X�e�B�b�N
	int joy_type[2];					// �W���C�X�e�B�b�N�^�C�v
	int joy_dev[2];						// �W���C�X�e�B�b�N�f�o�C�X
	int joy_button0[12];				// �W���C�X�e�B�b�N�{�^��(�f�o�C�XA)
	int joy_button1[12];				// �W���C�X�e�B�b�N�{�^��(�f�o�C�XB)

	// SASI
	int sasi_drives;					// SASI�h���C�u��
	BOOL sasi_sramsync;					// SASI�������X�C�b�`�����X�V
	TCHAR sasi_file[16][FILEPATH_MAX];	// SASI�C���[�W�t�@�C��

	// SxSI
	int sxsi_drives;					// SxSI�h���C�u��
	BOOL sxsi_mofirst;					// MO�h���C�u�D�抄�蓖��
	TCHAR sxsi_file[6][FILEPATH_MAX];	// SxSI�C���[�W�t�@�C��

	// �|�[�g
	int port_com;						// COMx�|�[�g
	TCHAR port_recvlog[FILEPATH_MAX];	// �V���A����M���O
	BOOL port_384;						// �V���A��38400bps�Œ�
	int port_lpt;						// LPTx�|�[�g
	TCHAR port_sendlog[FILEPATH_MAX];	// �p���������M���O

	// MIDI
	int midi_bid;						// MIDI�{�[�hID
	int midi_ilevel;					// MIDI���荞�݃��x��
	int midi_reset;						// MIDI���Z�b�g�R�}���h
	int midiin_device;					// MIDI IN�f�o�C�X
	int midiin_delay;					// MIDI IN�f�B���C(ms)
	int midiout_device;					// MIDI OUT�f�o�C�X
	int midiout_delay;					// MIDI OUT�f�B���C(ms)

	// ����
	BOOL sram_64k;						// 64KB SRAM
	BOOL scc_clkup;						// SCC�N���b�N�A�b�v
	BOOL power_led;						// �F�d��LED
	BOOL dual_fdd;						// 2DD/2HD���pFDD
	BOOL sasi_parity;					// SASI�o�X�p���e�B

	// TrueKey
	int tkey_mode;						// TrueKey���[�h(bit0:VM bit1:WinApp)
	int tkey_com;						// �L�[�{�[�hCOM�|�[�g
	BOOL tkey_rts;						// RTS���]���[�h

	// ���̑�
	BOOL floppy_speed;					// �t���b�s�[�f�B�X�N����
	BOOL floppy_led;					// �t���b�s�[�f�B�X�NLED���[�h
	BOOL popup_swnd;					// �|�b�v�A�b�v�T�u�E�B���h�E
	BOOL auto_mouse;					// �����}�E�X���[�h����
	BOOL power_off;						// �d��OFF�ŊJ�n
	TCHAR ruta_savestate[FILEPATH_MAX];
};

//===========================================================================
//
//	�R���t�B�M�����[�V����(version2.02�`version2.03)
//
//===========================================================================
class Config202 : public Config200 {
public:
	// �V�X�e��
	int mem_type;						// �������}�b�v���

	// SCSI
	int scsi_ilevel;					// SCSI���荞�݃��x��
	int scsi_drives;					// SCSI�h���C�u��
	BOOL scsi_sramsync;					// SCSI�������X�C�b�`�����X�V
	BOOL scsi_mofirst;					// MO�h���C�u�D�抄�蓖��
	TCHAR scsi_file[5][FILEPATH_MAX];	// SCSI�C���[�W�t�@�C��
};

//===========================================================================
//
//	�R���t�B�M�����[�V����
//
//===========================================================================
class Config : public Config202 {
public:
	// ���W���[��
	BOOL resume_fd;						// FD���W���[��
	BOOL resume_fdi[2];					// FD�}���t���O
	BOOL resume_fdw[2];					// FD�������݋֎~
	int resume_fdm[2];					// FD���f�B�ANo.
	BOOL resume_mo;						// MO���W���[��
	BOOL resume_mos;					// MO�}���t���O
	BOOL resume_mow;					// MO�������݋֎~
	BOOL resume_cd;						// CD���W���[��
	BOOL resume_iso;					// CD�}���t���O
	BOOL resume_state;					// �X�e�[�g���W���[��
	BOOL resume_xm6;					// �X�e�[�g�L���t���O
	BOOL resume_screen;					// ��ʃ��[�h���W���[��
	BOOL resume_dir;					// �f�t�H���g�f�B���N�g�����W���[��
	TCHAR resume_path[FILEPATH_MAX];	// �f�t�H���g�f�B���N�g��

	// �`��
	BOOL caption_info;					// �L���v�V�������\��

	// �f�B�X�v���C
	BOOL caption;						// �L���v�V����
	BOOL menu_bar;						// ���j���[�o�[
	BOOL status_bar;					// �X�e�[�^�X�o�[
	int window_left;					// �E�B���h�E��`
	int window_top;						// �E�B���h�E��`
	BOOL window_full;					// �t���X�N���[��
	int window_mode;					// ���C�h�X�N���[��

	// WINDRV���W���[��
	DWORD windrv_enable;				// Windrv�T�|�[�g 0:���� 1:WindrvXM (2:Windrv�݊�)

	// �z�X�g���t�@�C���V�X�e��
	DWORD host_option;					// ����t���O (class CHostFilename �Q��)
	BOOL host_resume;					// �x�[�X�p�X��ԕ����L�� FALSE���Ɩ���X�L��������
	DWORD host_drives;					// �L���ȃh���C�u��
	DWORD host_flag[10];				// ����t���O (class CWinFileDrv �Q��)
	TCHAR host_path[10][_MAX_PATH];		// �x�[�X�p�X
};

#endif	// config_h
