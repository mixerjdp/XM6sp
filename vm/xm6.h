//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Copyright (C) 2001-2005 PI (ytanaka@ipc-tokai.or.jp)
//	[ Main header ]
//
//---------------------------------------------------------------------------

#if !defined(xm6_h)
#define xm6_h

#include <assert.h>
#include <stdint.h>
#include "win_compat.h"

#if !defined(FASTCALL)
#if defined(_MSC_VER) && defined(_M_IX86)
#define FASTCALL __fastcall
#else
#define FASTCALL
#endif
#endif

//---------------------------------------------------------------------------
//
//	Basic constants
//
//---------------------------------------------------------------------------
#if !defined(FALSE)
#define FALSE		0
#define TRUE		(!FALSE)
#endif	// FALSE
#if !defined(NULL)
#define NULL		0
#endif	// NULL

//---------------------------------------------------------------------------
//
//	Basic macros
//
//---------------------------------------------------------------------------
#if !defined(ASSERT)
#if !defined(NDEBUG)
#define ASSERT(cond)	assert(cond)
#else
#define ASSERT(cond)	((void)0)
#endif	// NDEBUG
#endif	// ASSERT

#if !defined(ASSERT_DIAG)
#if !defined(NDEBUG)
#define ASSERT_DIAG()	AssertDiag()
#else
#define ASSERT_DIAG()	((void)0)
#endif	// NDEBUG
#endif	// ASSERT_DIAG

//---------------------------------------------------------------------------
//
//	Basic types
//
//---------------------------------------------------------------------------
#if !defined(XM6_BASIC_TYPES_DEFINED)
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int BOOL;
#define XM6_BASIC_TYPES_DEFINED 1
#endif

static_assert(sizeof(BYTE) == 1, "BYTE must be 8-bit");
static_assert(sizeof(WORD) == 2, "WORD must be 16-bit");
static_assert(sizeof(DWORD) == 4, "DWORD must be 32-bit");

//---------------------------------------------------------------------------
//
//	ID macros
//
//---------------------------------------------------------------------------
#define MAKEID(a, b, c, d)	((DWORD)((a<<24) | (b<<16) | (c<<8) | d))

//---------------------------------------------------------------------------
//
//	Class declarations
//
//---------------------------------------------------------------------------
class VM;
										// Virtual Machine
class Config;
										// Configuration
class Device;
										// Device common
class MemDevice;
										// Memory mapped device common
class Log;
										// Log
class Event;
										// Event
class Scheduler;
										// Scheduler
class CPU;
										// CPU MC68000
class Memory;
										// Address space OHM2
class Fileio;
										// File I/O
class SRAM;
										// SRAM
class SysPort;
										// System port MESSIAH
class TVRAM;
										// Text VRAM
class VC;
										// Video controller VIPS&CATHY
class CRTC;
										// CRTC VICON
class RTC;
										// RTC RP5C15
class PPI;
										// PPI i8255A
class DMAC;
										// DMAC HD63450
class MFP;
										// MFP MC68901
class FDC;
										// FDC uPD72065
class FDD;
										// FDD FD55GFR
class IOSC;
										// I/O controller IOSC-2
class SASI;
										// SASI
class Sync;
										// Sync object
class OPMIF;
										// OPM YM2151
class Keyboard;
										// Keyboard
class ADPCM;
										// ADPCM MSM6258V
class GVRAM;
										// Graphics VRAM
class Sprite;
										// Sprite RAM
class SCC;
										// SCC Z8530
class Mouse;
										// Mouse
class Printer;
										// Printer
class AreaSet;
										// Area set
class Render;
										// Renderer
class Windrv;
										// Windrv
class FDI;
										// FDCImages floppy disk image
class Disk;
										// SASI/SCSI disk
class MIDI;
										// MIDI YM3802
class Filepath;
										// File path
class JoyDevice;
										// Joystick device
class FileSys;
										// File system
class SCSI;
										// SCSI MB89352
class Mercury;
										// Mercury-Unit

#endif	// xm6_h