//----------------------------------------------------------------------------
//
//	X68Sound bridge implementation
//
//----------------------------------------------------------------------------

#include "x68sound_bridge.h"

#include <cstdio>
#include <cstdint>

#if defined(XM6CORE_ENABLE_X68SOUND)

#include "vm.h"
#include "memory.h"
#include "dmac.h"

namespace {

static Memory *g_memory = nullptr;
static bool g_started = false;
static unsigned int g_last_write_value = 0;
static unsigned int g_write_dma_count = 0;
static unsigned int g_write_adpcm_count = 0;
static const char *g_trace_source = "vm";

static int CALLBACK x68sound_mem_read(unsigned char *adrs)
{
	if (!g_memory) {
		return 0;
	}

	const uintptr_t raw = reinterpret_cast<uintptr_t>(adrs);
	const DWORD addr = static_cast<DWORD>(raw & 0x00ffffffu);
	return static_cast<int>(g_memory->ReadByte(addr) & 0xffu);
}

static inline unsigned char compose_dcr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.xrm & 0x03u) << 6);
	data |= static_cast<unsigned char>((dma.dtyp & 0x03u) << 4);
	if (dma.dps) {
		data |= 0x08;
	}
	data |= static_cast<unsigned char>(dma.pcl & 0x03u);
	return data;
}

static inline unsigned char compose_ocr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.dir) {
		data |= 0x80;
	}
	if (dma.btd) {
		data |= 0x40;
	}
	data |= static_cast<unsigned char>((dma.size & 0x03u) << 4);
	data |= static_cast<unsigned char>((dma.chain & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.reqg & 0x03u);
	return data;
}

static inline unsigned char compose_scr(const DMAC::dma_t &dma)
{
	unsigned char data = static_cast<unsigned char>((dma.mac & 0x03u) << 2);
	data |= static_cast<unsigned char>(dma.dac & 0x03u);
	return data;
}

static inline unsigned char compose_ccr(const DMAC::dma_t &dma)
{
	unsigned char data = 0;
	if (dma.intr) {
		data |= 0x08;
	}
	if (dma.hlt) {
		data |= 0x20;
	}
	if (dma.str) {
		data |= 0x80;
	}
	if (dma.cnt) {
		data |= 0x40;
	}
	if (dma.sab) {
		data |= 0x10;
	}
	return data;
}

} // namespace

namespace Xm6X68Sound {

bool Available()
{
	return true;
}

void SetMemory(Memory *memory)
{
	g_memory = memory;
	X68Sound_MemReadFunc(memory ? x68sound_mem_read : nullptr);
}

void Start(unsigned int sample_rate)
{
	X68Sound_Free();
	X68Sound_Reset();
	X68Sound_OpmClock(4000000);
	X68Sound_TotalVolume(256);
	const int rc = X68Sound_StartPcm(static_cast<int>(sample_rate), 1, 0, 5);
	X68Sound_MemReadFunc(g_memory ? x68sound_mem_read : nullptr);
	g_last_write_value = 0;
	g_write_dma_count = 0;
	g_write_adpcm_count = 0;
	g_started = (rc >= 0);
	if (!g_started) {
		X68Sound_MemReadFunc(nullptr);
		X68Sound_Free();
	}
}

void Shutdown()
{
	X68Sound_MemReadFunc(nullptr);
	if (!g_started) {
		g_memory = nullptr;
		return;
	}
	X68Sound_Free();
	g_started = false;
	g_memory = nullptr;
}

void Reset()
{
	if (g_started) {
		X68Sound_Reset();
		X68Sound_MemReadFunc(g_memory ? x68sound_mem_read : nullptr);
		g_last_write_value = 0;
		g_write_dma_count = 0;
		g_write_adpcm_count = 0;
	}
}

void SetTraceSource(const char *source)
{
	g_trace_source = (source && source[0]) ? source : "vm";
}

int GetPcm(void *buffer, int frames)
{
	if (!g_started) {
		return X68SNDERR_NOTACTIVE;
	}
	return X68Sound_GetPcm(buffer, frames);
}

void WriteOpm(unsigned char reg, unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_OpmReg(reg);
	X68Sound_OpmPoke(data);
}

void WriteAdpcm(unsigned char data)
{
	if (!g_started) {
		return;
	}
	if (data & 0x06u) {
		const unsigned char backend_csr = X68Sound_DmaPeek(0x00);
		std::fprintf(stderr,
		             "[xm6-x68sound][%s] ADPCM start csr=$%02X\n",
		             g_trace_source,
		             static_cast<unsigned>(backend_csr));
	}
	++g_write_adpcm_count;
	std::fprintf(stderr, "[xm6-x68sound][%s] WriteAdpcm #%u data=$%02X\n",
	             g_trace_source,
	             g_write_adpcm_count, static_cast<unsigned>(data));
	g_last_write_value = 0xC0000000u | static_cast<unsigned int>(data);
	X68Sound_AdpcmPoke(data);
}

void WritePpi(unsigned char data)
{
	if (!g_started) {
		return;
	}
	static unsigned int g_write_ppi_count = 0;
	if (g_write_ppi_count < 24u) {
		std::fprintf(stderr, "[xm6-x68sound][%s] WritePpi #%u data=$%02X\n",
		             g_trace_source,
		             g_write_ppi_count + 1u,
		             static_cast<unsigned>(data));
	}
	++g_write_ppi_count;
	X68Sound_PpiPoke(data);
}

void WritePpiCtrl(unsigned char data)
{
	if (!g_started) {
		return;
	}
	X68Sound_PpiCtrl(data);
}

void WriteDma(unsigned char adrs, unsigned char data)
{
	if (!g_started) {
		return;
	}
	const unsigned char original_data = data;
	++g_write_dma_count;
	if (adrs == 0x04u || adrs == 0x05u || adrs == 0x06u || adrs == 0x07u) {
		std::fprintf(stderr, "[xm6-x68sound][%s] WriteDma #%u adrs=$%02X data=$%02X\n",
		             g_trace_source,
		             g_write_dma_count,
		             static_cast<unsigned>(adrs),
		             static_cast<unsigned>(original_data));
	}
	g_last_write_value = 0xB0000000u | (static_cast<unsigned int>(adrs) << 8) | static_cast<unsigned int>(original_data);
	X68Sound_DmaPoke(adrs, original_data);
}

void TimerA()
{
	if (!g_started) {
		return;
	}
	X68Sound_TimerA();
}

int GetErrorCode()
{
	return X68Sound_ErrorCode();
}

int GetDebugValue()
{
	return X68Sound_DebugValue();
}

int GetTraceValue()
{
	return X68Sound_TraceValue();
}

int GetWriteValue()
{
	return static_cast<int>(g_last_write_value);
}

} // namespace Xm6X68Sound

#else

namespace Xm6X68Sound {

bool Available()
{
	return false;
}

void SetMemory(Memory * /*memory*/)
{
}

void Start(unsigned int /*sample_rate*/)
{
}

void Shutdown()
{
}

void Reset()
{
}

int GetPcm(void * /*buffer*/, int /*frames*/)
{
	return -1;
}

void WriteOpm(unsigned char /*reg*/, unsigned char /*data*/)
{
}

void WriteAdpcm(unsigned char /*data*/)
{
}

void WritePpi(unsigned char /*data*/)
{
}

void WritePpiCtrl(unsigned char /*data*/)
{
}

void WriteDma(unsigned char /*adrs*/, unsigned char /*data*/)
{
}

void TimerA()
{
}

int GetErrorCode()
{
	return 0;
}

int GetDebugValue()
{
	return 0;
}

int GetTraceValue()
{
	return 0;
}

int GetWriteValue()
{
	return 0;
}

} // namespace Xm6X68Sound

#endif
