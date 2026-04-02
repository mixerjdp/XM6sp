//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Musashi Memory Access Implementation
//	Provides m68k_read/write_memory functions required by Musashi core
//	[ musashi_mem.cpp ]
//
//---------------------------------------------------------------------------

// Use the XM6-specific Musashi configuration
#define MUSASHI_CNF "m68kconf_xm6.h"

#include <string.h>
#include "os.h"
#include "xm6.h"

// Minimal type compatibility (from xm6.h)
#ifndef FASTCALL
#define FASTCALL __fastcall
#endif

#include "musashi_adapter.h"
#include "m68k.h"

// Forward declarations for XM6 memory system
class Memory;
class MemDevice;

// External references to the XM6 memory system
// These are set up in memory.cpp
extern Memory *pMemory;
extern MemDevice *pMemDeviceTable[];

//---------------------------------------------------------------------------
//
//	Memory access helpers
//	These use the Memory class dispatch mechanism
//
//---------------------------------------------------------------------------

// The memory class provides ReadByte, ReadWord, WriteByte, WriteWord
// We call these through the global pMemory pointer

extern "C" {

//---------------------------------------------------------------------------
//
//	m68k_read_memory_8 - Read a byte from memory
//
//---------------------------------------------------------------------------
unsigned int m68k_read_memory_8(unsigned int address)
{
	extern DWORD FASTCALL MusashiReadByte(DWORD addr);
	return MusashiReadByte(address & 0xFFFFFF);
}

//---------------------------------------------------------------------------
//
//	m68k_read_memory_16 - Read a word from memory
//
//---------------------------------------------------------------------------
unsigned int m68k_read_memory_16(unsigned int address)
{
	extern DWORD FASTCALL MusashiReadWord(DWORD addr);
	return MusashiReadWord(address & 0xFFFFFF);
}

//---------------------------------------------------------------------------
//
//	m68k_read_memory_32 - Read a long from memory (two word reads)
//
//---------------------------------------------------------------------------
unsigned int m68k_read_memory_32(unsigned int address)
{
	unsigned int hi, lo;
	address &= 0xFFFFFF;
	hi = m68k_read_memory_16(address);
	lo = m68k_read_memory_16(address + 2);
	return (hi << 16) | lo;
}

//---------------------------------------------------------------------------
//
//	m68k_write_memory_8 - Write a byte to memory
//
//---------------------------------------------------------------------------
void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	extern void FASTCALL MusashiWriteByte(DWORD addr, DWORD data);
	MusashiWriteByte(address & 0xFFFFFF, value & 0xFF);
}

//---------------------------------------------------------------------------
//
//	m68k_write_memory_16 - Write a word to memory
//
//---------------------------------------------------------------------------
void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	extern void FASTCALL MusashiWriteWord(DWORD addr, DWORD data);
	MusashiWriteWord(address & 0xFFFFFF, value & 0xFFFF);
}

//---------------------------------------------------------------------------
//
//	m68k_write_memory_32 - Write a long to memory (two word writes)
//
//---------------------------------------------------------------------------
void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	address &= 0xFFFFFF;
	m68k_write_memory_16(address, (value >> 16) & 0xFFFF);
	m68k_write_memory_16(address + 2, value & 0xFFFF);
}

//---------------------------------------------------------------------------
//
//	Disassembler memory access (uses same read functions)
//
//---------------------------------------------------------------------------
unsigned int m68k_read_disassembler_8(unsigned int address)
{
	return m68k_read_memory_8(address);
}

unsigned int m68k_read_disassembler_16(unsigned int address)
{
	return m68k_read_memory_16(address);
}

unsigned int m68k_read_disassembler_32(unsigned int address)
{
	return m68k_read_memory_32(address);
}

} /* extern "C" */
