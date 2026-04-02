//----------------------------------------------------------------------------
//
//	X68Sound bridge
//
//	Minimal wrapper used by XM6 core to mirror live VM writes into the
//	external X68Sound backend when that engine is selected.
//
//----------------------------------------------------------------------------

#ifndef X68SOUND_BRIDGE_H
#define X68SOUND_BRIDGE_H

#include <cstddef>

class Memory;

#if defined(XM6CORE_ENABLE_X68SOUND)
#include "x68sound.h"
#endif

namespace Xm6X68Sound {

bool Available();
void SetMemory(Memory *memory);
void Start(unsigned int sample_rate);
void Shutdown();
void Reset();
void SetTraceSource(const char *source);
int GetPcm(void *buffer, int frames);
void WriteOpm(unsigned char reg, unsigned char data);
void WriteAdpcm(unsigned char data);
void WritePpi(unsigned char data);
void WritePpiCtrl(unsigned char data);
void WriteDma(unsigned char adrs, unsigned char data);
void TimerA();
int GetErrorCode();
int GetDebugValue();
int GetTraceValue();
int GetWriteValue();

} // namespace Xm6X68Sound

#endif // X68SOUND_BRIDGE_H
