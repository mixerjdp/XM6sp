#include "os.h"
#include "xm6.h"
#include "xm6core.h"
#include "vm.h"
#include "render.h"
#include "opm.h"
#include "adpcm.h"
#include "ppi.h"
#include "config.h"
#include "midi.h"

struct XM6ContextRuntimeShim {
	VM *vm;
	Scheduler *scheduler;
	Render *render;
	Keyboard *keyboard;
	Mouse *mouse;
	FDD *fdd;
	OPMIF *opmif;
	FM::OPM *opm_engine;
	ADPCM *adpcm;
	SASI *sasi;
	SCSI *scsi;
	PPI *ppi;
	DWORD *opm_buf;
	DWORD *adpcm_buf;
	unsigned int audio_rate;
	unsigned int audio_buf_frames;
	Config runtime_config;
};

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_joy_type(XM6Handle handle, int port, int type)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (port < 0 || port >= PPI::PortMax) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	PPI *ppi = ctx->ppi;
	if (!ppi) {
		ppi = (PPI*)ctx->vm->SearchDevice(MAKEID('P', 'P', 'I', ' '));
		if (!ppi) {
			return XM6CORE_ERR_NOT_READY;
		}
		ctx->ppi = ppi;
	}

	ppi->SetJoyType(port, type);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_system_clock(XM6Handle handle, int system_clock)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (system_clock < 0 || system_clock > 5) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.system_clock = system_clock;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_ram_size(XM6Handle handle, int ram_size)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (ram_size < 0 || ram_size > 5) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.ram_size = ram_size;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_fast_floppy(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.floppy_speed = enabled ? TRUE : FALSE;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

static int clamp_volume(int volume)
{
	if (volume < 0) return 0;
	if (volume > 100) return 100;
	return volume;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_master_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.master_volume = clamp_volume(volume);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_fm_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	volume = clamp_volume(volume);
	ctx->runtime_config.fm_volume = volume;
	if (ctx->opm_engine) {
		ctx->opm_engine->SetVolume(volume);
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_adpcm_volume(XM6Handle handle, int volume)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	volume = clamp_volume(volume);
	ctx->runtime_config.adpcm_volume = volume;
	if (ctx->adpcm) {
		ctx->adpcm->SetVolume(volume);
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_mouse_speed(XM6Handle handle, int speed)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (speed < 0 || speed > 512) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.mouse_speed = speed;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_mouse_port(XM6Handle handle, int port)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (port < 0 || port > 2) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.mouse_port = port;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_mouse_swap(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.mouse_swap = enabled ? TRUE : FALSE;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_render_mode(XM6Handle handle, int mode)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (mode != XM6CORE_RENDER_MODE_ORIGINAL && mode != XM6CORE_RENDER_MODE_FAST) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->vm->SetRenderMode(mode)) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	ctx->runtime_config.alt_raster = (mode == XM6CORE_RENDER_MODE_FAST) ? TRUE : FALSE;
	ctx->vm->ApplyCfg(&ctx->runtime_config);

	if (ctx->render) {
		ctx->render->Complete();
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_get_render_mode(XM6Handle handle)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	return ctx->vm->GetRenderMode();
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_midi_enabled(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.midi_bid = enabled ? 1 : 0;
	ctx->vm->ApplyCfg(&ctx->runtime_config);

	if (!enabled) {
		MIDI *midi = (MIDI*)ctx->vm->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
		if (midi) {
			midi->ClrTransData();
		}
	}

	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_midi_read_output(
	XM6Handle handle,
	unsigned char* out_bytes,
	unsigned int capacity,
	unsigned int* out_count)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (!out_count) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}
	if ((capacity > 0) && !out_bytes) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	*out_count = 0;

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	MIDI *midi = (MIDI*)ctx->vm->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	if (!midi) {
		return XM6CORE_ERR_NOT_READY;
	}

	DWORD pending = midi->GetTransNum();
	if (pending == 0 || capacity == 0) {
		return XM6CORE_OK;
	}

	DWORD to_copy = pending;
	if (to_copy > capacity) {
		to_copy = capacity;
	}

	DWORD copied = 0;
	for (DWORD i = 0; i < to_copy; ++i) {
		const MIDI::mididata_t *entry = midi->GetTransData(i);
		if (!entry) {
			break;
		}
		out_bytes[i] = (unsigned char)(entry->data & 0xff);
		copied++;
	}

	midi->DelTransData(copied);
	*out_count = (unsigned int)copied;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_midi_write_input(
	XM6Handle handle,
	const unsigned char* bytes,
	unsigned int count)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}
	if (count > 0 && !bytes) {
		return XM6CORE_ERR_INVALID_ARGUMENT;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (count == 0) {
		return XM6CORE_OK;
	}

	MIDI *midi = (MIDI*)ctx->vm->SearchDevice(MAKEID('M', 'I', 'D', 'I'));
	if (!midi) {
		return XM6CORE_ERR_NOT_READY;
	}

	midi->SetRecvData((const BYTE*)bytes, (DWORD)count);
	return XM6CORE_OK;
}

