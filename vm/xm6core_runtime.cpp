#include "os.h"
#include "xm6.h"
#include "xm6core.h"
#include "vm.h"
#include "render.h"
#include "opm.h"
#include "adpcm.h"
#include "ppi.h"
#include "dmac.h"
#include "config.h"
#include "midi.h"
#include <cstring>
#include <new>

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
	BOOL surround_enabled;
	BOOL hq_adpcm_enabled;
	int reverb_level;
	int eq_sub_bass_level;
	int eq_bass_level;
	int eq_mid_level;
	int eq_presence_level;
	int eq_treble_level;
	int eq_air_level;
	Config runtime_config;
	int surround_prev_l;
	int surround_prev_r;
	int *reverb_buf;
	unsigned int reverb_buf_frames;
	unsigned int reverb_buf_pos;
	int reverb_lp_l;
	int reverb_lp_r;
	int eq_sub_bass_l;
	int eq_sub_bass_r;
	int eq_bass_l;
	int eq_bass_r;
	int eq_mid_l;
	int eq_mid_r;
	int eq_presence_l;
	int eq_presence_r;
	int eq_air_l;
	int eq_air_r;
	int eq_sub_bass_coeff_q14;
	int eq_bass_coeff_q14;
	int eq_mid_coeff_q14;
	int eq_presence_coeff_q14;
	int eq_air_coeff_q14;
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

static bool ensure_reverb_buffer(XM6ContextRuntimeShim *ctx)
{
	if (!ctx || ctx->audio_rate == 0) {
		return false;
	}

	const unsigned int wanted_frames = ((ctx->audio_rate * 90u) / 1000u) + 1u;
	if (wanted_frames == 0u) {
		return false;
	}

	if (ctx->reverb_buf && ctx->reverb_buf_frames == wanted_frames) {
		memset(ctx->reverb_buf, 0, ctx->reverb_buf_frames * 2u * sizeof(int));
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
		return true;
	}

	delete[] ctx->reverb_buf;
	ctx->reverb_buf = new(std::nothrow) int[wanted_frames * 2u];
	if (!ctx->reverb_buf) {
		ctx->reverb_buf_frames = 0;
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
		return false;
	}

	ctx->reverb_buf_frames = wanted_frames;
	ctx->reverb_buf_pos = 0;
	ctx->reverb_lp_l = 0;
	ctx->reverb_lp_r = 0;
	memset(ctx->reverb_buf, 0, ctx->reverb_buf_frames * 2u * sizeof(int));
	return true;
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

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_adpcm_interp(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm || !ctx->adpcm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.adpcm_interp = enabled ? TRUE : FALSE;
	ctx->adpcm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_surround_enabled(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	if (!ctx->surround_enabled && enabled) {
		ctx->surround_prev_l = 0;
		ctx->surround_prev_r = 0;
	}
	ctx->surround_enabled = enabled ? TRUE : FALSE;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_hq_adpcm_enabled(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->hq_adpcm_enabled = enabled ? TRUE : FALSE;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_reverb_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	level = clamp_volume(level);
	if (level > 100) {
		level = 100;
	}
	ctx->reverb_level = level;
	if (level <= 0) {
		delete[] ctx->reverb_buf;
		ctx->reverb_buf = NULL;
		ctx->reverb_buf_frames = 0;
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
		return XM6CORE_OK;
	}

	if (!ensure_reverb_buffer(ctx)) {
		ctx->reverb_buf_pos = 0;
		ctx->reverb_lp_l = 0;
		ctx->reverb_lp_r = 0;
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_bass_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_sub_bass_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_bass2_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_bass_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_mid_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_mid_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_presence_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_presence_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_air_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_air_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_eq_treble_level(XM6Handle handle, int level)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->eq_treble_level = clamp_volume(level);
	ctx->eq_sub_bass_l = 0;
	ctx->eq_sub_bass_r = 0;
	ctx->eq_bass_l = 0;
	ctx->eq_bass_r = 0;
	ctx->eq_mid_l = 0;
	ctx->eq_mid_r = 0;
	ctx->eq_presence_l = 0;
	ctx->eq_presence_r = 0;
	ctx->eq_air_l = 0;
	ctx->eq_air_r = 0;
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

	if (ctx->render) {
		ctx->render->Complete();
	}
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_legacy_dmac_cnt(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	DMAC *dmac = (DMAC*)ctx->vm->SearchDevice(MAKEID('D', 'M', 'A', 'C'));
	if (!dmac) {
		return XM6CORE_ERR_NOT_READY;
	}

	dmac->SetLegacyCntMode(enabled ? TRUE : FALSE);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_alt_raster(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	ctx->runtime_config.alt_raster = enabled ? TRUE : FALSE;
	ctx->vm->ApplyCfg(&ctx->runtime_config);
	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_render_bg0(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	Render *render = ctx->render;
	if (!render) {
		render = (Render*)ctx->vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
		ctx->render = render;
	}

	if (render) {
		render->SetOriginalBG0RenderEnabled(enabled ? TRUE : FALSE);
		render->ForceRecompose();
		render->Complete();
	}

	return XM6CORE_OK;
}

extern "C" XM6CORE_API int XM6CORE_CALL xm6_set_transparency_enabled(XM6Handle handle, int enabled)
{
	if (!handle) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	XM6ContextRuntimeShim *ctx = reinterpret_cast<XM6ContextRuntimeShim*>(handle);
	if (!ctx->vm) {
		return XM6CORE_ERR_INVALID_HANDLE;
	}

	Render *render = ctx->render;
	if (!render) {
		render = (Render*)ctx->vm->SearchDevice(MAKEID('R', 'E', 'N', 'D'));
		ctx->render = render;
	}

	if (render) {
		render->SetTransparencyEnabled(enabled ? TRUE : FALSE);
		// Force a full recomposite so the option takes effect immediately
		// without requiring a content reset.
		render->ForceRecompose();
		render->Complete();
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

