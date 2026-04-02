//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Core Soft (replaces core_asm) - Event Dispatcher
//
//---------------------------------------------------------------------------

#pragma once

#if !defined (core_soft_h)
#define core_soft_h

#include <stdint.h>
#include "event.h"

//---------------------------------------------------------------------------
//
//	プロトタイプ宣言
//
//---------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif

void NotifyEvent(Event *first);
DWORD GetMinEvent(DWORD hus);
BOOL SubExecEvent(DWORD hus);

// Memory related
void MemInitDecode(Device *mem, MemDevice **devarray);
extern uintptr_t MemDecodeTable[0x180];

#ifdef __cplusplus
}
#endif

#endif	// core_soft_h
