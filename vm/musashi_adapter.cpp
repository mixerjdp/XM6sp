//---------------------------------------------------------------------------
//
//  X68000 EMULATOR "XM6"
//
//  Musashi adapter
//
//---------------------------------------------------------------------------

#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#include "os.h"
#include "xm6.h"
#include "memory.h"
#include "musashi_adapter.h"
#include "../cpu/musashi/m68k.h"

class Memory;
Memory* g_musashi_memory = NULL;

struct S68000CONTEXT s68000context;
unsigned int s68000iocycle = (unsigned int)-1;
unsigned int musashi_current_fc = 0;
bool musashi_is_resetting = false;

static int musashi_executing = 0;
static int musashi_wait_cycles = 0;
static int musashi_exec_calls = 0;

static int GetHighestPendingLegacyIRQ(void)
{
	unsigned char mask = s68000context.interrupts[0];
	for (int level = 7; level >= 1; level--) {
		if (mask & (1u << level)) {
			return level;
		}
	}
	return 0;
}

static int MusashiTraceEnabled(void)
{
	static int enabled = -1;
	if (enabled == -1) {
		char v[8];
		DWORD n = ::GetEnvironmentVariableA("XM6_TRACE_MUSASHI", v, sizeof(v));
		enabled = (n > 0) ? 1 : 0;
	}
	return enabled;
}

static void MusashiTrace(const char* fmt, ...)
{
	static FILE* f = NULL;
	if (!MusashiTraceEnabled()) {
		return;
	}

	if (!f) {
		char module[_MAX_PATH];
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char path[_MAX_PATH];
		::GetModuleFileNameA(NULL, module, _MAX_PATH);
		_splitpath(module, drive, dir, NULL, NULL);
		_makepath(path, drive, dir, "xm6_musashi_trace", ".log");
		f = fopen(path, "a");
		if (!f) {
			return;
		}
	}

	SYSTEMTIME st;
	::GetLocalTime(&st);
	fprintf(f, "[%02u:%02u:%02u.%03u] ", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

	va_list ap;
	va_start(ap, fmt);
	vfprintf(f, fmt, ap);
	va_end(ap);

	fprintf(f, "\n");
	fflush(f);
}

static void SyncContextFromMusashi(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		s68000context.dreg[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_D0 + i));
		s68000context.areg[i] = m68k_get_reg(NULL, (m68k_register_t)(M68K_REG_A0 + i));
	}

	s68000context.pc = m68k_get_reg(NULL, M68K_REG_PC);
	s68000context.sr = (unsigned short)m68k_get_reg(NULL, M68K_REG_SR);
	s68000context.asp = m68k_get_reg(NULL, M68K_REG_USP);

	if (musashi_executing) {
		int raw = (int)m68k_cycles_run();
		int adjusted = raw - musashi_wait_cycles;
		if (adjusted < 0) {
			adjusted = 0;
		}
		s68000context.odometer = (unsigned)adjusted;
	}
}

static void SyncContextToMusashi(void)
{
	int i;

	for (i = 0; i < 8; i++) {
		m68k_set_reg((m68k_register_t)(M68K_REG_D0 + i), s68000context.dreg[i]);
		m68k_set_reg((m68k_register_t)(M68K_REG_A0 + i), s68000context.areg[i]);
	}

	m68k_set_reg(M68K_REG_PC, s68000context.pc);
	m68k_set_reg(M68K_REG_SR, (unsigned)s68000context.sr);
	m68k_set_reg(M68K_REG_USP, s68000context.asp);
}

extern "C" {

int s68000init(void)
{
	m68k_init();
	m68k_set_cpu_type(M68K_CPU_TYPE_68000);
	memset(&s68000context, 0, sizeof(s68000context));
	return 0;
}

unsigned s68000reset(void)
{
	musashi_is_resetting = true;
	m68k_pulse_reset();
	musashi_is_resetting = false;
	SyncContextFromMusashi();
	s68000context.odometer = 0;
	MusashiTrace("reset pc=%08X sp=%08X sr=%04X", m68k_get_reg(NULL, M68K_REG_PC), m68k_get_reg(NULL, M68K_REG_A7), m68k_get_reg(NULL, M68K_REG_SR));
	return 0;
}

unsigned s68000exec(int n)
{
	int cycles_used;

	musashi_executing = 1;
	musashi_wait_cycles = 0;
	s68000iocycle = (unsigned)n;

	cycles_used = m68k_execute(n);
	cycles_used -= musashi_wait_cycles;
	if (cycles_used < 0) {
		cycles_used = 0;
	}

	musashi_executing = 0;
	SyncContextFromMusashi();
	s68000context.odometer = (unsigned)cycles_used;
	musashi_exec_calls++;
	if (musashi_exec_calls <= 300 || (musashi_exec_calls % 20000) == 0) {
		MusashiTrace("exec #%d req=%d used=%d wait=%d remain=%d pc=%08X", musashi_exec_calls, n, cycles_used, musashi_wait_cycles, m68k_cycles_remaining(), m68k_get_reg(NULL, M68K_REG_PC));
	}
	return (unsigned)cycles_used;
}

int s68000interrupt(int level, int vector)
{
	if (level < 1 || level > 7) {
		return 2; // bad input (Starscream contract)
	}
	if (vector > 255 || vector < -2) {
		return 2; // bad input (Starscream contract)
	}

	{
		unsigned char bit = (unsigned char)(1u << level);
		if (s68000context.interrupts[0] & bit) {
			return 1; // duplicate level request rejected
		}

		s68000context.interrupts[0] |= bit;
		if (vector == -2) {
			s68000context.interrupts[level] = 0x18; // spurious
		}
		else if (vector < 0) {
			s68000context.interrupts[level] = (unsigned char)(0x18 + level); // autovector
		}
		else {
			s68000context.interrupts[level] = (unsigned char)(vector & 0xFF);
		}
	}

	m68k_set_irq(GetHighestPendingLegacyIRQ());
	return 0;
}

void s68000flushInterrupts(void)
{
}

int s68000GetContextSize(void)
{
	return sizeof(struct S68000CONTEXT);
}

void s68000GetContext(void* context)
{
	SyncContextFromMusashi();
	memcpy(context, &s68000context, sizeof(struct S68000CONTEXT));
}

void s68000SetContext(void* context)
{
	memcpy(&s68000context, context, sizeof(struct S68000CONTEXT));
	SyncContextToMusashi();
}

int s68000fetch(unsigned address)
{
	return (int)m68k_read_memory_16(address);
}

unsigned s68000readOdometer(void)
{
	if (musashi_executing) {
		int raw = (int)m68k_cycles_run();
		int adjusted = raw - musashi_wait_cycles;
		if (adjusted < 0) {
			adjusted = 0;
		}
		return (unsigned)adjusted;
	}
	return s68000context.odometer;
}

unsigned s68000tripOdometer(void)
{
	unsigned odo = s68000readOdometer();
	s68000context.odometer = 0;
	return odo;
}

unsigned s68000controlOdometer(int n)
{
	unsigned odo = s68000readOdometer();
	if (n >= 0) {
		s68000context.odometer = (unsigned)n;
	}
	return odo;
}

void s68000releaseTimeslice(void)
{
	if (musashi_executing) {
		m68k_end_timeslice();
	}
}

unsigned s68000readPC(void)
{
	return m68k_get_reg(NULL, M68K_REG_PC);
}

unsigned s68000wait(unsigned cycle)
{
	if (musashi_executing) {
		m68k_modify_timeslice(-(int)cycle);
		musashi_wait_cycles += (int)cycle;
		if (musashi_exec_calls <= 300) {
			MusashiTrace("wait cycle=%u total_wait=%d", cycle, musashi_wait_cycles);
		}
	}
	return 0;
}

void musashi_adjust_timeslice(int cycles)
{
	if (musashi_executing) {
		m68k_modify_timeslice(cycles);
		if (cycles < 0) {
			musashi_wait_cycles += (-cycles);
			if (musashi_exec_calls <= 300) {
				MusashiTrace("adjust cycles=%d total_wait=%d", cycles, musashi_wait_cycles);
			}
		}
	}
}

unsigned int s68000getcounter(void)
{
	if (musashi_executing) {
		return (unsigned int)m68k_cycles_remaining();
	}
	return s68000iocycle;
}

void s68000setcounter(unsigned int c)
{
	if (musashi_executing) {
		int current = m68k_cycles_remaining();
		int diff = (int)c - current;
		if (diff != 0) {
			m68k_modify_timeslice(diff);
		}
	}
	s68000iocycle = c;
}

void musashi_fc_callback(unsigned int new_fc)
{
	musashi_current_fc = new_fc;
	if (musashi_exec_calls <= 120) {
		MusashiTrace("fc=%u", new_fc);
	}
}

unsigned int m68k_read_memory_8(unsigned int address)
{
	if (!g_musashi_memory) {
		return 0xFF;
	}
	return g_musashi_memory->ReadByte(address & 0x00FFFFFF);
}

unsigned int m68k_read_memory_16(unsigned int address)
{
	if (!g_musashi_memory) {
		return 0xFFFF;
	}
	return g_musashi_memory->ReadWord(address & 0x00FFFFFF);
}

unsigned int m68k_read_memory_32(unsigned int address)
{
	unsigned int addr = address & 0x00FFFFFF;
	unsigned int word_high = m68k_read_memory_16(addr);
	unsigned int word_low = m68k_read_memory_16((addr + 2) & 0x00FFFFFF);
	return (word_high << 16) | word_low;
}

unsigned int m68k_read_immediate_16(unsigned int address)
{
	return m68k_read_memory_16(address);
}

unsigned int m68k_read_immediate_32(unsigned int address)
{
	return m68k_read_memory_32(address);
}

unsigned int m68k_read_pcrelative_8(unsigned int address)
{
	return m68k_read_memory_8(address);
}

unsigned int m68k_read_pcrelative_16(unsigned int address)
{
	return m68k_read_memory_16(address);
}

unsigned int m68k_read_pcrelative_32(unsigned int address)
{
	return m68k_read_memory_32(address);
}

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

void m68k_write_memory_8(unsigned int address, unsigned int value)
{
	if (!g_musashi_memory) {
		return;
	}
	g_musashi_memory->WriteByte(address & 0x00FFFFFF, value & 0xFF);
}

void m68k_write_memory_16(unsigned int address, unsigned int value)
{
	if (!g_musashi_memory) {
		return;
	}
	g_musashi_memory->WriteWord(address & 0x00FFFFFF, value & 0xFFFF);
}

void m68k_write_memory_32(unsigned int address, unsigned int value)
{
	unsigned int addr = address & 0x00FFFFFF;
	m68k_write_memory_16(addr, (value >> 16) & 0xFFFF);
	m68k_write_memory_16((addr + 2) & 0x00FFFFFF, value & 0xFFFF);
}

void m68k_write_memory_32_pd(unsigned int address, unsigned int value)
{
	m68k_write_memory_32(address, value);
}

} // extern "C"
