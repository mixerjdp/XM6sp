//----------------------------------------------------------------------------
//
//  XM6 Core C API
//
//----------------------------------------------------------------------------

#ifndef XM6CORE_H
#define XM6CORE_H

#if defined(_WIN32)
  #if defined(XM6CORE_EXPORTS)
    #define XM6CORE_API __declspec(dllexport)
  #elif defined(XM6CORE_STATIC)
    #define XM6CORE_API
  #else
    #define XM6CORE_API __declspec(dllimport)
  #endif
  #define XM6CORE_CALL __cdecl
#else
  #define XM6CORE_API
  #define XM6CORE_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct XM6Handle__* XM6Handle;

enum {
  XM6CORE_OK = 0,
  XM6CORE_ERR_INVALID_ARGUMENT = -1,
  XM6CORE_ERR_INVALID_HANDLE = -2,
  XM6CORE_ERR_INIT_FAILED = -3,
  XM6CORE_ERR_IO = -4,
  XM6CORE_ERR_NOT_READY = -5
};

enum {
  XM6CORE_RENDER_MODE_ORIGINAL = 0,
  XM6CORE_RENDER_MODE_FAST = 1
};

enum {
  XM6CORE_AUDIO_ENGINE_XM6 = 0,
  XM6CORE_AUDIO_ENGINE_PX68K = 1,
  XM6CORE_AUDIO_ENGINE_YMFM = 2,
  XM6CORE_AUDIO_ENGINE_YMFM_DIRECT = 3,
  XM6CORE_AUDIO_ENGINE_YMFM_RAW = XM6CORE_AUDIO_ENGINE_YMFM_DIRECT,
  XM6CORE_AUDIO_ENGINE_X68SOUND = 4
};

typedef struct xm6_video_frame_t {
  const unsigned int* pixels_argb32;
  unsigned int width;
  unsigned int height;
  unsigned int stride_pixels;
} xm6_video_frame_t;

typedef struct xm6_adpcm_telemetry_t {
  unsigned int quirk_arianshuu_fix;
  unsigned int play;
  unsigned int rec;
  unsigned int active;
  unsigned int started;
  unsigned int dmac_ch3_active;
  unsigned int buffer_samples;
  int wait_counter;
  unsigned int last_data;
  unsigned int start_events;
  unsigned int stop_events;
  unsigned int req_total;
  unsigned int req_ok;
  unsigned int req_fail;
  unsigned int decode_calls;
  unsigned int underrun_head_events;
  unsigned int underrun_interp_events;
  unsigned int underrun_linear_events;
  unsigned int silence_fill_events;
  unsigned int stale_nonzero_events;
  unsigned int max_buffer_samples;
  unsigned int dmac3_mar;
  unsigned int dmac3_mtc;
  unsigned int dmac3_btc;
  unsigned int dmac3_csr;
  unsigned int dmac3_ccr;
  unsigned int dmac3_cer;
  unsigned int x68sound_error_code;
  unsigned int x68sound_debug_value;
  unsigned int x68sound_write_value;
  unsigned int x68sound_trace_value;
} xm6_adpcm_telemetry_t;

typedef void (XM6CORE_CALL *xm6_message_callback_t)(const char* message, void* user);

XM6CORE_API const char* XM6CORE_CALL xm6_get_version(void);
XM6CORE_API const char* XM6CORE_CALL xm6_get_last_error(void);

XM6CORE_API XM6Handle XM6CORE_CALL xm6_create(void);
XM6CORE_API void XM6CORE_CALL xm6_destroy(XM6Handle handle);

XM6CORE_API int XM6CORE_CALL xm6_set_message_callback(XM6Handle handle, xm6_message_callback_t callback, void* user);
XM6CORE_API int XM6CORE_CALL xm6_set_system_dir(const char* system_dir);

XM6CORE_API int XM6CORE_CALL xm6_exec(XM6Handle handle, unsigned int hus);
XM6CORE_API int XM6CORE_CALL xm6_exec_events_only(XM6Handle handle, unsigned int hus);
XM6CORE_API int XM6CORE_CALL xm6_exec_to_frame(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_exec_mpu_nowait_step(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_reset(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_set_power(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_get_power(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_set_power_switch(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_get_power_switch(XM6Handle handle);

XM6CORE_API int XM6CORE_CALL xm6_video_poll(XM6Handle handle, xm6_video_frame_t* out_frame);
XM6CORE_API int XM6CORE_CALL xm6_video_consume(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_get_video_refresh_hz(XM6Handle handle, double* out_hz);
XM6CORE_API int XM6CORE_CALL xm6_get_video_layout(
  XM6Handle handle,
  unsigned int* out_width,
  unsigned int* out_height,
  unsigned int* out_h_mul,
  unsigned int* out_v_mul,
  int* out_lowres
);

XM6CORE_API int XM6CORE_CALL xm6_audio_configure(XM6Handle handle, unsigned int sample_rate);
XM6CORE_API int XM6CORE_CALL xm6_audio_mix(
  XM6Handle handle,
  short* out_interleaved_stereo,
  unsigned int frames,
  unsigned int* out_frames
);

XM6CORE_API int XM6CORE_CALL xm6_input_key(XM6Handle handle, unsigned int xm6_keycode, int pressed);
XM6CORE_API int XM6CORE_CALL xm6_input_mouse(XM6Handle handle, int x, int y, int left, int right);
XM6CORE_API int XM6CORE_CALL xm6_input_mouse_reset(XM6Handle handle);

XM6CORE_API int XM6CORE_CALL xm6_input_joy(
  XM6Handle handle,
  int port,
  const unsigned int axes[4],
  const int buttons[8]
);
XM6CORE_API int XM6CORE_CALL xm6_set_joy_type(XM6Handle handle, int port, int type);
XM6CORE_API int XM6CORE_CALL xm6_set_system_clock(XM6Handle handle, int system_clock);
XM6CORE_API int XM6CORE_CALL xm6_set_ram_size(XM6Handle handle, int ram_size);
XM6CORE_API int XM6CORE_CALL xm6_set_fast_floppy(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_master_volume(XM6Handle handle, int volume);
XM6CORE_API int XM6CORE_CALL xm6_set_fm_volume(XM6Handle handle, int volume);
XM6CORE_API int XM6CORE_CALL xm6_set_adpcm_volume(XM6Handle handle, int volume);
XM6CORE_API int XM6CORE_CALL xm6_set_adpcm_interp(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_hq_adpcm_enabled(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_reverb_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_bass2_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_bass_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_presence_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_mid_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_treble_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_eq_air_level(XM6Handle handle, int level);
XM6CORE_API int XM6CORE_CALL xm6_set_surround_enabled(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_audio_engine(XM6Handle handle, int audio_engine);
XM6CORE_API int XM6CORE_CALL xm6_set_legacy_dmac_cnt(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_mouse_speed(XM6Handle handle, int speed);
XM6CORE_API int XM6CORE_CALL xm6_set_mouse_port(XM6Handle handle, int port);
XM6CORE_API int XM6CORE_CALL xm6_set_mouse_swap(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_render_mode(XM6Handle handle, int mode);
XM6CORE_API int XM6CORE_CALL xm6_set_alt_raster(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_render_bg0(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_set_transparency_enabled(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_get_render_mode(XM6Handle handle);
XM6CORE_API int XM6CORE_CALL xm6_set_midi_enabled(XM6Handle handle, int enabled);
XM6CORE_API int XM6CORE_CALL xm6_midi_read_output(
  XM6Handle handle,
  unsigned char* out_bytes,
  unsigned int capacity,
  unsigned int* out_count
);
XM6CORE_API int XM6CORE_CALL xm6_midi_write_input(
  XM6Handle handle,
  const unsigned char* bytes,
  unsigned int count
);

XM6CORE_API int XM6CORE_CALL xm6_mount_fdd(XM6Handle handle, int drive, const char* image_path, int media_hint);
XM6CORE_API int XM6CORE_CALL xm6_eject_fdd(XM6Handle handle, int drive, int force);
XM6CORE_API int XM6CORE_CALL xm6_fdd_is_inserted(XM6Handle handle, int drive);
XM6CORE_API int XM6CORE_CALL xm6_fdd_get_name(
  XM6Handle handle, int drive, char* out_name, unsigned int max_len);

XM6CORE_API int XM6CORE_CALL xm6_mount_sasi_hdd(XM6Handle handle, int slot, const char* image_path);
XM6CORE_API int XM6CORE_CALL xm6_mount_scsi_hdd(XM6Handle handle, int slot, const char* image_path);

XM6CORE_API int XM6CORE_CALL xm6_save_state(XM6Handle handle, const char* state_path);
XM6CORE_API int XM6CORE_CALL xm6_load_state(XM6Handle handle, const char* state_path);
XM6CORE_API int XM6CORE_CALL xm6_state_size(XM6Handle handle, unsigned int* out_size);
XM6CORE_API int XM6CORE_CALL xm6_save_state_mem(XM6Handle handle, void* buffer, unsigned int size);
XM6CORE_API int XM6CORE_CALL xm6_load_state_mem(XM6Handle handle, const void* buffer, unsigned int size);

XM6CORE_API void* XM6CORE_CALL xm6_get_main_ram(XM6Handle handle, unsigned int* out_size);
XM6CORE_API int XM6CORE_CALL xm6_diag_init_probe(char* out_text, unsigned int out_text_size);
XM6CORE_API int XM6CORE_CALL xm6_video_attach_default_buffer(
  XM6Handle handle, unsigned int width, unsigned int height);
XM6CORE_API int XM6CORE_CALL xm6_diag_get_adpcm_telemetry(
  XM6Handle handle, xm6_adpcm_telemetry_t* out_telemetry);

XM6CORE_API void XM6CORE_CALL xm6_get_vm_version(XM6Handle handle, unsigned int* out_major, unsigned int* out_minor);

#ifdef __cplusplus
}
#endif

#endif  // XM6CORE_H

