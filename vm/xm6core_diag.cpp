#include "os.h"
#include "xm6.h"
#include "xm6core.h"
#include "vm.h"
#include "device.h"
#include "schedule.h"
#include "cpu.h"
#include "memory.h"
#include "render.h"
#include "keyboard.h"
#include "mouse.h"
#include "fdd.h"
#include "ppi.h"
#include "opmif.h"
#include "adpcm.h"
#include "sasi.h"
#include "scsi.h"
#include "sram.h"
#include "tvram.h"
#include "gvram.h"
#include "crtc.h"
#include "vc.h"
#include "dmac.h"
#include "areaset.h"
#include "mfp.h"
#include "rtc.h"
#include "printer.h"
#include "sysport.h"
#include "fdc.h"
#include "iosc.h"
#include "scc.h"
#include "windrv.h"
#include "midi.h"
#include "sprite.h"
#include "mercury.h"
#include "neptune.h"
#include <new>
#include <cstdarg>
#include <cstdio>
#include <cstring>

static int diag_set_text(char* out_text, unsigned int out_text_size, const char* fmt, ...)
{
	if (!out_text || out_text_size == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	va_list args;
	va_start(args, fmt);
	_vsnprintf(out_text, out_text_size - 1, fmt, args);
	va_end(args);
	out_text[out_text_size - 1] = '\0';
	return XM6CORE_OK;
}

struct XM6ContextShim {
	VM *vm;
	Scheduler *scheduler;
	Render *render;
};

static DWORD* g_default_mixbuf = NULL;
static unsigned int g_default_mixbuf_w = 0;
static unsigned int g_default_mixbuf_h = 0;

extern "C" XM6CORE_API int XM6CORE_CALL xm6_diag_init_probe(char* out_text, unsigned int out_text_size)
{
	if (!out_text || out_text_size == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}
	out_text[0] = '\0';

	VM* vm = new(std::nothrow) VM();
	if (!vm) {
		return diag_set_text(out_text, out_text_size, "probe: failed allocating VM");
	}

	if (!vm->Init()) {
		delete vm;
		return diag_set_text(out_text, out_text_size,
							 "probe: vm->Init failed (check BIOS files/system dir)");
	}

	vm->Cleanup();
	delete vm;
	return diag_set_text(out_text, out_text_size, "probe: init sequence OK");
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_video_attach_default_buffer(
	XM6Handle handle, unsigned int width, unsigned int height)
{
	if (!handle || width == 0 || height == 0) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextShim *ctx = reinterpret_cast<XM6ContextShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	Render *render = ctx->render;
	if (!render) {
		render = (Render*)ctx->vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
		if (!render) {
			return XM6CORE_ERR_NOT_READY;
		}
		ctx->render = render;
	}

	if (!g_default_mixbuf ||
		g_default_mixbuf_w < width ||
		g_default_mixbuf_h < height) {
		const size_t pixels = (size_t)width * (size_t)height;
		DWORD *buf = new(std::nothrow) DWORD[pixels];
		if (!buf) {
			return XM6CORE_ERR_INIT_FAILED;
		}
		delete[] g_default_mixbuf;
		g_default_mixbuf = buf;
		g_default_mixbuf_w = width;
		g_default_mixbuf_h = height;
	}

	std::memset(g_default_mixbuf, 0, (size_t)g_default_mixbuf_w * (size_t)g_default_mixbuf_h * sizeof(DWORD));
	render->SetMixBuf(g_default_mixbuf, (int)width, (int)height);
	return XM6CORE_OK;
}
