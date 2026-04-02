//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2006 PI(ytanaka@ipc-tokai.or.jp)
//	[ Configuration ]
//
//---------------------------------------------------------------------------

#if !defined(config_h)
#define config_h

#include "filepath.h"

//===========================================================================
//
//	Configuration (version 2.00~2.01)
//
//===========================================================================
class Config200 {
public:
	// System
	int system_clock;					// System clock (0~5)
	int ram_size;						// Main RAM size (0~5)
	BOOL ram_sramsync;					// Backup switch update

	// Speed
	BOOL mpu_fullspeed;					// MPU full speed
	BOOL vm_fullspeed;					// VM full speed

	// Sound
	int sound_device;					// Sound device (0~15)
	int sample_rate;					// Sampling rate (0~4)
	int primary_buffer;					// Buffer size (2~100)
	int polling_buffer;					// Polling interval (0~99)
	BOOL adpcm_interp;					// ADPCM interpolation

	// Display
	int window_scale;					// Window scale index (0=1.0x)
	BOOL render_vsync;					// VSync (TRUE=ON)
	int render_mode;					// Renderer (0=GDI, 1=DirectX 9)
	BOOL render_shader;					// Show Shader (CRT)
	BOOL alt_raster;					// Enable pseudo-3D raster timing correction

	// Volume
	int master_volume;					// Master volume (0~100)
	BOOL fm_enable;						// FM enable
	int fm_volume;						// FM volume (0~100)
	BOOL adpcm_enable;					// ADPCM enable
	int adpcm_volume;					// ADPCM volume (0~100)

	// Keyboard
	BOOL kbd_connect;					// Connection

	// Mouse
	int mouse_speed;					// Speed
	int mouse_port;						// Connection port
	BOOL mouse_swap;					// Button swap
	BOOL mouse_mid;						// Middle button click
	BOOL mouse_trackb;					// Trackball mode

	// Joystick
	int joy_type[2];					// Joystick type
	int joy_dev[2];						// Joystick device
	int joy_button0[12];				// Joystick button (device A)
	int joy_button1[12];				// Joystick button (device B)

	// SASI
	int sasi_drives;					// SASI drives
	BOOL sasi_sramsync;					// SASI backup switch update
	TCHAR sasi_file[16][FILEPATH_MAX];	// SASI image file

	// SxSI
	int sxsi_drives;					// SxSI drives
	BOOL sxsi_mofirst;					// MO drive default assignment
	TCHAR sxsi_file[6][FILEPATH_MAX];	// SxSI image file

	// Port
	int port_com;						// COMx port
	TCHAR port_recvlog[FILEPATH_MAX];	// Serial receive log
	BOOL port_384;						// Serial 38400bps fixed
	int port_lpt;						// LPTx port
	TCHAR port_sendlog[FILEPATH_MAX];	// Parallel send log

	// MIDI
	int midi_bid;						// MIDI board ID
	int midi_ilevel;					// MIDI interrupt level
	int midi_reset;						// MIDI reset command
	int midiin_device;					// MIDI IN device
	int midiin_delay;					// MIDI IN delay (ms)
	int midiout_device;					// MIDI OUT device
	int midiout_delay;					// MIDI OUT delay (ms)

	// Options
	BOOL sram_64k;						// 64KB SRAM
	BOOL scc_clkup;						// SCC clock up
	BOOL power_led;						// Green power LED
	BOOL dual_fdd;						// 2DD/2HD compatible FDD
	BOOL sasi_parity;					// SASI parity error

	// TrueKey
	int tkey_mode;						// TrueKey mode (bit0:VM bit1:WinApp)
	int tkey_com;						// Keyboard COM port
	BOOL tkey_rts;						// RTS inverted mode

	// Others
	BOOL floppy_speed;					// Floppy speed limit
	BOOL floppy_led;					// Floppy LED blink
	BOOL popup_swnd;					// Popup sub window
	BOOL auto_mouse;					// Auto mouse mode switch
	BOOL power_off;					// Start with power OFF
	TCHAR ruta_savestate[FILEPATH_MAX];
};

//===========================================================================
//
//	Configuration (version 2.02~2.03)
//
//===========================================================================
class Config202 : public Config200 {
public:
	// System
	int mem_type;						// Memory map type

	// SCSI
	int scsi_ilevel;					// SCSI interrupt level
	int scsi_drives;					// SCSI drives
	BOOL scsi_sramsync;					// SCSI backup switch update
	BOOL scsi_mofirst;					// MO drive default assignment
	TCHAR scsi_file[5][FILEPATH_MAX];	// SCSI image file
};

//===========================================================================
//
//	Configuration
//
//===========================================================================
class Config : public Config202 {
public:
	// Resume
	BOOL resume_fd;						// FD resume
	BOOL resume_fdi[2];					// FD FDI flag
	BOOL resume_fdw[2];					// FD write protect
	int resume_fdm[2];					// FD FAD number
	BOOL resume_mo;						// MO resume
	BOOL resume_mos;					// MO FDI flag
	BOOL resume_mow;					// MO write protect
	BOOL resume_cd;						// CD resume
	BOOL resume_iso;					// CD FDI flag
	BOOL resume_state;					// State resume
	BOOL resume_xm6;					// State enable flag
	BOOL resume_screen;					// Screen mode resume
	BOOL resume_dir;					// Default directory resume
	TCHAR resume_path[FILEPATH_MAX];	// Default directory

	// Display
	BOOL caption_info;					// Caption information display

	// Display
	BOOL caption;						// Caption
	BOOL menu_bar;						// Menu bar
	BOOL status_bar;					// Status bar
	int window_left;					// Window left
	int window_top;					// Window top
	BOOL window_full;					// Fullscreen
	int window_mode;					// Custom fullscreen

	// WINDRV Resume
	DWORD windrv_enable;				// Windrv support 0:none 1:WindrvXM (2:Windrv compatible)

	// Host file system
	DWORD host_option;					// Various flags (see class CHostFilename)
	BOOL host_resume;					// Base path level retention. If FALSE, fixed host.
	DWORD host_drives;					// Valid drive numbers
	DWORD host_flag[10];				// Various flags (see class CWinFileDrv)
	TCHAR host_path[10][_MAX_PATH];		// Base path
};

#endif	// config_h
