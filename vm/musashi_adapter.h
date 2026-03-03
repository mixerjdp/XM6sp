//---------------------------------------------------------------------------
//
//  X68000 EMULATOR "XM6"
//
//  Musashi adapter compatibility declarations
//
//---------------------------------------------------------------------------

#ifndef musashi_adapter_h
#define musashi_adapter_h

#include "starcpu.h"

#ifdef __cplusplus
extern "C" {
#endif

extern unsigned int s68000iocycle;
extern unsigned int musashi_current_fc;
extern bool musashi_is_resetting;

void musashi_adjust_timeslice(int cycles);
unsigned int s68000getcounter(void);
void s68000setcounter(unsigned int c);
void musashi_fc_callback(unsigned int new_fc);

#ifdef __cplusplus
}
#endif

#endif
