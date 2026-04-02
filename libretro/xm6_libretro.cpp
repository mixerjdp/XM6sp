#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <cerrno>
#include <sys/stat.h>

#if defined(_WIN32)
#include <windows.h>
#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)
#endif
#else
#include <unistd.h>
#include <limits.h>
#include <strings.h>
#include <dlfcn.h>
#endif

#include "libretro.h"

#if !defined(_WIN32) && !defined(XM6CORE_MONOLITHIC)
#define XM6CORE_MONOLITHIC
#endif

#ifndef XM6CORE_STATIC
#define XM6CORE_STATIC
#endif
#include "../vm/xm6core.h"

namespace {

static retro_environment_t g_environ_cb = nullptr;
static retro_video_refresh_t g_video_cb = nullptr;
static retro_audio_sample_t g_audio_cb = nullptr;
static retro_audio_sample_batch_t g_audio_batch_cb = nullptr;
static retro_input_poll_t g_input_poll_cb = nullptr;
static retro_input_state_t g_input_state_cb = nullptr;

static retro_log_printf_t g_log_cb = nullptr;
static bool g_supports_input_bitmasks = false;
static struct retro_midi_interface g_midi_cb = {};
static bool g_supports_midi_interface = false;

static const double k_default_fps = 55.0;
static const double k_default_aspect = 4.0 / 3.0;
static const unsigned k_default_sample_rate = 44100;
static const unsigned k_ymfm_sample_rate = 62500;
static const unsigned k_default_width = 768;
static const unsigned k_default_height = 512;
static const unsigned k_savestate_guard_frames_default = 300;
static const unsigned k_savestate_guard_frames_floppy_load = 420;
static const unsigned k_savestate_guard_frames_hdd_load = 720;
static const unsigned k_savestate_guard_frames_post_load = 180;
static const unsigned k_video_probe_frames_after_mode_change = 12;

static uint64_t now_usec()
{
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::microseconds>(now).count());
}

static unsigned g_frame_width = k_default_width;
static unsigned g_frame_height = k_default_height;
static float g_frame_aspect = static_cast<float>(k_default_aspect);
static double g_current_fps = k_default_fps;
static double g_audio_fraction = 0.0;
static unsigned g_sample_rate = k_default_sample_rate;
static bool g_game_loaded = false;

static std::vector<std::string> g_disk_paths;
static std::vector<std::string> g_disk_labels;
static std::vector<int> g_disk_target_drives;
static unsigned g_disk_index = 0;
static int g_inserted_disk_index[2] = { -1, -1 };
static int g_disk_drive = 0;
static bool g_disk_ejected = false;
static bool g_content_is_hdd = false;
static bool g_hdd_is_scsi = false;
static int g_hdd_slot = 0;
static bool g_hdd_target_auto = true;

static bool g_use_exec_to_frame = true;
enum start_select_mode_t {
  START_SELECT_DISABLED = 0,
  START_SELECT_XF_KEYS = 1,
  START_SELECT_F_KEYS = 2,
  START_SELECT_OPT_KEYS = 3
};
static int g_pad_start_select_mode = START_SELECT_XF_KEYS;
static unsigned g_video_not_ready_count = 0;
static int g_joy_type[2] = { 1, 0 };
static int g_system_clock = 0;
static int g_ram_size = 5;
static bool g_fast_floppy = false;
static int g_render_mode = XM6CORE_RENDER_MODE_ORIGINAL;
static bool g_alt_raster_enabled = true;
static bool g_render_bg0_enabled = true;
static bool g_transparency_enabled = true;
static enum retro_pixel_format g_frontend_pixel_format = RETRO_PIXEL_FORMAT_UNKNOWN;
static int g_fm_volume = 50;
static int g_adpcm_volume = 50;
static bool g_hq_adpcm_enabled = false;
static int g_reverb_level = 0;
static int g_eq_sub_bass_level = 50;
static int g_eq_bass_level = 50;
static int g_eq_mid_level = 50;
static int g_eq_presence_level = 50;
static int g_eq_treble_level = 50;
static int g_eq_air_level = 50;
static bool g_surround_enabled = false;
static int g_audio_engine = XM6CORE_AUDIO_ENGINE_XM6;
static bool g_legacy_dmac_cnt = false;
static bool g_midi_output_enabled = false;
enum midi_output_type_t {
  MIDI_OUTPUT_LA = 0,
  MIDI_OUTPUT_GM = 1,
  MIDI_OUTPUT_GS = 2,
  MIDI_OUTPUT_XG = 3
};
static int g_midi_output_type = MIDI_OUTPUT_GM;
static bool g_midi_reset_pending = false;
enum pointer_device_mode_t {
  POINTER_DEVICE_DISABLED = 0,
  POINTER_DEVICE_MOUSE = 1,
  POINTER_DEVICE_JOYPAD_MOUSE = 2
};
static int g_pointer_device_mode = POINTER_DEVICE_DISABLED;
static int g_mouse_port = 0;
static int g_mouse_speed = 205;
static bool g_mouse_swap = false;
static int g_mouse_x = 0;
static int g_mouse_y = 0;
static bool g_mouse_left = false;
static bool g_mouse_right = false;
static bool g_mpu_nowait = false;
static unsigned g_hdd_boot_reset_countdown = 0;
static unsigned g_savestate_guard_countdown = 0;
static const unsigned k_hdd_boot_warmup_frames = 8;

static bool g_prev_start = false;
static bool g_prev_select = false;
static unsigned g_prev_start_keycode = 0;
static unsigned g_prev_select_keycode = 0;
static bool g_prev_midi_hotkey = false;

static std::vector<int16_t> g_audio_buffer;
static std::vector<unsigned int> g_video_buffer;
static std::vector<uint16_t> g_video_buffer_rgb565;

static inline unsigned current_video_bpp_bytes()
{
  return (g_frontend_pixel_format == RETRO_PIXEL_FORMAT_RGB565) ? 2u : 4u;
}

static inline unsigned current_video_pitch_bytes(unsigned width_pixels)
{
  // px68k libretro always reports a pitch of 800 for RGB565 output, even when the
  // visible width is smaller (e.g. 512/768). Match that behavior in Fast mode
  // to minimize border/backdrop and shader edge-case differences.
  if (g_frontend_pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
    const unsigned stride_pixels = (width_pixels <= 800) ? 800u : width_pixels;
    return stride_pixels * sizeof(uint16_t);
  }
  return width_pixels * sizeof(uint32_t);
}

static inline uint16_t px68k_pack_rgb565i(uint32_t pixel)
{
  // XM6 uses REND_COLOR0 (bit31) as "transparent". px68k uses 0 in 565 buffers
  // as a "no write" value in some blend paths; mapping REND_COLOR0 -> 0 keeps
  // behavior consistent when a transparent pixel ever escapes to the output.
  if (pixel & 0x80000000u) {
    return 0;
  }

  const uint32_t rgb = pixel & 0x00ffffffu;
  const uint16_t r5 = (uint16_t)((rgb >> 19) & 0x1fu);
  const uint16_t g5 = (uint16_t)((rgb >> 11) & 0x1fu);
  const uint16_t b5 = (uint16_t)((rgb >> 3) & 0x1fu);

  // px68k uses a 565 layout where bit 0x0020 is repurposed as the "I" bit.
  // Keep it to match its TR half-color mixing behavior.
  uint16_t out = (uint16_t)((r5 << 11) | (g5 << 6) | b5);
  if (pixel & 0x40000000u) {
    out |= 0x0020;
  }
  return out;
}

struct xm6_video_layout_t {
  bool valid = false;
  unsigned int width = 0;
  unsigned int height = 0;
  unsigned int h_mul = 1;
  unsigned int v_mul = 1;
  int lowres = 0;
};

struct xm6_video_mode_log_t {
  bool valid = false;
  unsigned int raw_w = 0;
  unsigned int raw_h = 0;
  unsigned int layout_w = 0;
  unsigned int layout_h = 0;
  unsigned int h_mul = 0;
  unsigned int v_mul = 0;
  int lowres = 0;
  unsigned int h_scale = 0;
  unsigned int v_scale = 0;
  unsigned int out_w = 0;
  unsigned int out_h = 0;
};

static xm6_video_mode_log_t g_video_mode_log = {};
static unsigned g_video_probe_frames_remaining = 0;
static unsigned g_video_probe_frame_index = 0;

struct xm6_api_t {
#ifndef XM6CORE_MONOLITHIC
#if defined(_WIN32)
  HMODULE module = nullptr;
#else
  void* module = nullptr;
#endif
#endif

  XM6Handle (XM6CORE_CALL *create)(void) = nullptr;
  void (XM6CORE_CALL *destroy)(XM6Handle) = nullptr;
  int (XM6CORE_CALL *set_message_callback)(XM6Handle handle, xm6_message_callback_t callback, void *user) = nullptr;
  int (XM6CORE_CALL *set_system_dir)(const char *system_dir) = nullptr;

  int (XM6CORE_CALL *exec)(XM6Handle handle, unsigned int hus) = nullptr;
  int (XM6CORE_CALL *exec_events_only)(XM6Handle handle, unsigned int hus) = nullptr;
  int (XM6CORE_CALL *exec_to_frame)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *exec_mpu_nowait_step)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *reset)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *set_power)(XM6Handle handle, int enabled) = nullptr;

  int (XM6CORE_CALL *video_poll)(XM6Handle handle, xm6_video_frame_t *out_frame) = nullptr;
  int (XM6CORE_CALL *video_consume)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *get_video_refresh_hz)(XM6Handle handle, double *out_hz) = nullptr;
  int (XM6CORE_CALL *get_video_layout)(XM6Handle handle,
                                       unsigned int *out_width,
                                       unsigned int *out_height,
                                       unsigned int *out_h_mul,
                                       unsigned int *out_v_mul,
                                       int *out_lowres) = nullptr;

  int (XM6CORE_CALL *audio_configure)(XM6Handle handle, unsigned int sample_rate) = nullptr;
  int (XM6CORE_CALL *audio_mix)(XM6Handle handle, short *out_interleaved_stereo,
                                unsigned int frames, unsigned int *out_frames) = nullptr;

  int (XM6CORE_CALL *input_joy)(XM6Handle handle, int port,
                                const unsigned int axes[4], const int buttons[8]) = nullptr;
  int (XM6CORE_CALL *input_key)(XM6Handle handle, unsigned int xm6_keycode, int pressed) = nullptr;
  int (XM6CORE_CALL *input_mouse)(XM6Handle handle, int x, int y, int left, int right) = nullptr;
  int (XM6CORE_CALL *input_mouse_reset)(XM6Handle handle) = nullptr;

  int (XM6CORE_CALL *mount_fdd)(XM6Handle handle, int drive, const char *image_path, int media_hint) = nullptr;
  int (XM6CORE_CALL *mount_sasi_hdd)(XM6Handle handle, int slot, const char *image_path) = nullptr;
  int (XM6CORE_CALL *mount_scsi_hdd)(XM6Handle handle, int slot, const char *image_path) = nullptr;
  int (XM6CORE_CALL *eject_fdd)(XM6Handle handle, int drive, int force) = nullptr;
  int (XM6CORE_CALL *set_joy_type)(XM6Handle handle, int port, int type) = nullptr;
  int (XM6CORE_CALL *set_system_clock)(XM6Handle handle, int system_clock) = nullptr;
  int (XM6CORE_CALL *set_ram_size)(XM6Handle handle, int ram_size) = nullptr;
  int (XM6CORE_CALL *set_fast_floppy)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_master_volume)(XM6Handle handle, int volume) = nullptr;
  int (XM6CORE_CALL *set_fm_volume)(XM6Handle handle, int volume) = nullptr;
  int (XM6CORE_CALL *set_adpcm_volume)(XM6Handle handle, int volume) = nullptr;
  int (XM6CORE_CALL *set_adpcm_interp)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_hq_adpcm_enabled)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_reverb_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_bass2_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_bass_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_presence_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_mid_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_treble_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_eq_air_level)(XM6Handle handle, int level) = nullptr;
  int (XM6CORE_CALL *set_surround_enabled)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_audio_engine)(XM6Handle handle, int audio_engine) = nullptr;
  int (XM6CORE_CALL *set_legacy_dmac_cnt)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_mouse_speed)(XM6Handle handle, int speed) = nullptr;
  int (XM6CORE_CALL *set_mouse_port)(XM6Handle handle, int port) = nullptr;
  int (XM6CORE_CALL *set_mouse_swap)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_render_mode)(XM6Handle handle, int mode) = nullptr;
  int (XM6CORE_CALL *set_alt_raster)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_render_bg0)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *set_transparency_enabled)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *get_render_mode)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *set_midi_enabled)(XM6Handle handle, int enabled) = nullptr;
  int (XM6CORE_CALL *midi_read_output)(XM6Handle handle,
                                       unsigned char *out_bytes,
                                       unsigned int capacity,
                                       unsigned int *out_count) = nullptr;
  int (XM6CORE_CALL *midi_write_input)(XM6Handle handle,
                                       const unsigned char *bytes,
                                       unsigned int count) = nullptr;

  int (XM6CORE_CALL *state_size)(XM6Handle handle, unsigned int *out_size) = nullptr;
  int (XM6CORE_CALL *save_state_mem)(XM6Handle handle, void *buffer, unsigned int size) = nullptr;
  int (XM6CORE_CALL *load_state_mem)(XM6Handle handle, const void *buffer, unsigned int size) = nullptr;

  void *(XM6CORE_CALL *get_main_ram)(XM6Handle handle, unsigned int *out_size) = nullptr;
  int (XM6CORE_CALL *diag_init_probe)(char *out_text, unsigned int out_text_size) = nullptr;
  int (XM6CORE_CALL *video_attach_default_buffer)(XM6Handle handle, unsigned int width,
                                                  unsigned int height) = nullptr;
};

static xm6_api_t g_xm6;
static XM6Handle g_xm6_handle = nullptr;

static void core_log(enum retro_log_level level, const char *fmt, ...);
static void reset_video_mode_log_state();
static void fill_av_info(struct retro_system_av_info *info);
static unsigned desired_audio_sample_rate_for_engine(int engine)
{
  return (engine == XM6CORE_AUDIO_ENGINE_YMFM ||
          engine == XM6CORE_AUDIO_ENGINE_YMFM_DIRECT) ? k_ymfm_sample_rate : k_default_sample_rate;
}
static const char* audio_engine_label(int engine)
{
  switch (engine) {
    case XM6CORE_AUDIO_ENGINE_PX68K: return "PX68k";
    case XM6CORE_AUDIO_ENGINE_YMFM: return "YMFM";
    case XM6CORE_AUDIO_ENGINE_YMFM_DIRECT: return "YMFM";
    case XM6CORE_AUDIO_ENGINE_X68SOUND: return "X68Sound";
    default: return "XM6";
  }
}

static void XM6CORE_CALL xm6_host_message_cb(const char *message, void * /*user*/)
{
  if (message && *message) {
    core_log(RETRO_LOG_INFO, "%s", message);
  }
}

static void core_log(enum retro_log_level level, const char *fmt, ...)
{
  char buffer[1024];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buffer, sizeof(buffer), fmt, args);
  va_end(args);

  if (g_log_cb) {
    g_log_cb(level, "%s", buffer);
  } else {
#if defined(_WIN32)
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
#else
    std::fprintf(stderr, "%s\n", buffer);
#endif
  }
}

static void core_log_last_error(const char *prefix)
{
#if defined(_WIN32)
  const DWORD err = GetLastError();
  char *msg = nullptr;
  const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, err, 0,
                             reinterpret_cast<LPSTR>(&msg), 0, nullptr);
  if (len > 0 && msg) {
    while (len > 0 && (msg[len - 1] == '\r' || msg[len - 1] == '\n')) {
      msg[len - 1] = '\0';
      --len;
    }
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] %s (winerr=%lu: %s)",
             prefix, static_cast<unsigned long>(err), msg);
    LocalFree(msg);
    return;
  }
  core_log(RETRO_LOG_ERROR, "[xm6-libretro] %s (winerr=%lu)",
           prefix, static_cast<unsigned long>(err));
#else
  const int err = errno;
  const char* dyn = dlerror();
  if (dyn && *dyn) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] %s (dlerror=%s)", prefix, dyn);
    return;
  }
  core_log(RETRO_LOG_ERROR, "[xm6-libretro] %s (errno=%d: %s)",
           prefix, err, std::strerror(err));
#endif
}

static bool set_frontend_pixel_format(enum retro_pixel_format fmt)
{
  if (g_frontend_pixel_format == fmt) {
    return true;
  }
  if (!g_environ_cb) {
    return false;
  }

  enum retro_pixel_format req = fmt;
  if (!g_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &req)) {
    return false;
  }
  g_frontend_pixel_format = fmt;
  return true;
}

static void sync_frontend_pixel_format_for_render_mode()
{
  // Keep the frontend pixel format stable across compositor changes.
  // RetroArch can display corrupted colors/geometry when switching between
  // RGB565 and XRGB8888 mid-run; the core always produces ARGB32 anyway.
  const enum retro_pixel_format desired = RETRO_PIXEL_FORMAT_XRGB8888;

  if (desired == g_frontend_pixel_format && g_frontend_pixel_format != RETRO_PIXEL_FORMAT_UNKNOWN) {
    return;
  }

  if (set_frontend_pixel_format(desired)) {
    core_log(RETRO_LOG_INFO,
             "[xm6-libretro] Video output pixel format set to %s",
             "XRGB8888");
    return;
  }

  core_log(RETRO_LOG_WARN, "[xm6-libretro] Frontend does not support XRGB8888 (falling back to RGB565)");
  if (set_frontend_pixel_format(RETRO_PIXEL_FORMAT_RGB565)) {
    core_log(RETRO_LOG_INFO,
             "[xm6-libretro] Video output pixel format set to %s",
             "RGB565");
    return;
  }

  core_log(RETRO_LOG_ERROR, "[xm6-libretro] Frontend does not support %s",
           "XRGB8888 or RGB565");
}

static bool get_core_module_dir(char *out_dir, size_t out_dir_size)
{
  if (!out_dir || out_dir_size == 0) {
    return false;
  }

#if defined(_WIN32)
  HMODULE self = nullptr;
  if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                          GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCSTR>(&get_core_module_dir), &self)) {
    return false;
  }

  char module_path[MAX_PATH] = {};
  if (GetModuleFileNameA(self, module_path, MAX_PATH) == 0) {
    return false;
  }

  const char *slash = std::strrchr(module_path, '\\');
  if (!slash) {
    slash = std::strrchr(module_path, '/');
  }
  if (!slash) {
    return false;
  }

  const size_t dir_len = static_cast<size_t>(slash - module_path + 1);
  if (dir_len + 1 > out_dir_size) {
    return false;
  }

  std::memcpy(out_dir, module_path, dir_len);
  out_dir[dir_len] = '\0';
  return true;
#else
  char module_path[PATH_MAX] = {};
  ssize_t len = readlink("/proc/self/exe", module_path, sizeof(module_path) - 1);
  if (len <= 0 || static_cast<size_t>(len) >= sizeof(module_path)) {
    return false;
  }
  module_path[len] = '\0';

  const char *slash = std::strrchr(module_path, '/');
  if (!slash) {
    return false;
  }
  const size_t dir_len = static_cast<size_t>(slash - module_path + 1);
  if (dir_len + 1 > out_dir_size) {
    return false;
  }

  std::memcpy(out_dir, module_path, dir_len);
  out_dir[dir_len] = '\0';
  return true;
#endif
}

template <typename FnType>
static bool load_required_symbol(FnType *fn, const char *symbol)
{
#ifdef XM6CORE_MONOLITHIC
  (void)symbol;
  if (!*fn) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Missing required built-in symbol: %s", symbol);
    return false;
  }
  return true;
#else
#if defined(_WIN32)
  *fn = reinterpret_cast<FnType>(GetProcAddress(g_xm6.module, symbol));
#else
  *fn = reinterpret_cast<FnType>(dlsym(g_xm6.module, symbol));
#endif
  if (!*fn) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Missing required symbol: %s", symbol);
    return false;
  }
  return true;
#endif
}

template <typename FnType>
static void load_optional_symbol(FnType *fn, const char *symbol)
{
#ifdef XM6CORE_MONOLITHIC
  (void)symbol;
#else
#if defined(_WIN32)
  *fn = reinterpret_cast<FnType>(GetProcAddress(g_xm6.module, symbol));
#else
  *fn = reinterpret_cast<FnType>(dlsym(g_xm6.module, symbol));
#endif
#endif
}

static void unload_xm6_api()
{
#ifndef XM6CORE_MONOLITHIC
  if (g_xm6.module) {
#if defined(_WIN32)
    FreeLibrary(g_xm6.module);
#else
    dlclose(g_xm6.module);
#endif
  }
#endif
  g_xm6 = xm6_api_t();
}

static bool load_xm6_api()
{
#ifdef XM6CORE_MONOLITHIC
  if (g_xm6.create) {
    return true;
  }

  g_xm6.create = xm6_create;
  g_xm6.destroy = xm6_destroy;
  g_xm6.set_message_callback = xm6_set_message_callback;
  g_xm6.set_system_dir = xm6_set_system_dir;
  g_xm6.exec = xm6_exec;
  g_xm6.exec_events_only = xm6_exec_events_only;
  g_xm6.exec_to_frame = xm6_exec_to_frame;
  g_xm6.exec_mpu_nowait_step = xm6_exec_mpu_nowait_step;
  g_xm6.reset = xm6_reset;
  g_xm6.set_power = xm6_set_power;
  g_xm6.video_poll = xm6_video_poll;
  g_xm6.video_consume = xm6_video_consume;
  g_xm6.get_video_refresh_hz = xm6_get_video_refresh_hz;
  g_xm6.get_video_layout = xm6_get_video_layout;
  g_xm6.audio_configure = xm6_audio_configure;
  g_xm6.audio_mix = xm6_audio_mix;
  g_xm6.input_joy = xm6_input_joy;
  g_xm6.input_key = xm6_input_key;
  g_xm6.input_mouse = xm6_input_mouse;
  g_xm6.input_mouse_reset = xm6_input_mouse_reset;
  g_xm6.mount_fdd = xm6_mount_fdd;
  g_xm6.mount_sasi_hdd = xm6_mount_sasi_hdd;
  g_xm6.mount_scsi_hdd = xm6_mount_scsi_hdd;
  g_xm6.eject_fdd = xm6_eject_fdd;
  g_xm6.set_joy_type = xm6_set_joy_type;
  g_xm6.set_system_clock = xm6_set_system_clock;
  g_xm6.set_ram_size = xm6_set_ram_size;
  g_xm6.set_fast_floppy = xm6_set_fast_floppy;
  g_xm6.set_master_volume = xm6_set_master_volume;
  g_xm6.set_fm_volume = xm6_set_fm_volume;
  g_xm6.set_adpcm_volume = xm6_set_adpcm_volume;
  g_xm6.set_adpcm_interp = xm6_set_adpcm_interp;
  g_xm6.set_hq_adpcm_enabled = xm6_set_hq_adpcm_enabled;
  g_xm6.set_reverb_level = xm6_set_reverb_level;
  g_xm6.set_eq_bass2_level = xm6_set_eq_bass2_level;
  g_xm6.set_eq_bass_level = xm6_set_eq_bass_level;
  g_xm6.set_eq_presence_level = xm6_set_eq_presence_level;
  g_xm6.set_eq_mid_level = xm6_set_eq_mid_level;
  g_xm6.set_eq_treble_level = xm6_set_eq_treble_level;
  g_xm6.set_eq_air_level = xm6_set_eq_air_level;
  g_xm6.set_surround_enabled = xm6_set_surround_enabled;
  g_xm6.set_audio_engine = xm6_set_audio_engine;
  g_xm6.set_legacy_dmac_cnt = xm6_set_legacy_dmac_cnt;
  g_xm6.set_mouse_speed = xm6_set_mouse_speed;
  g_xm6.set_mouse_port = xm6_set_mouse_port;
  g_xm6.set_mouse_swap = xm6_set_mouse_swap;
  g_xm6.set_render_mode = xm6_set_render_mode;
  g_xm6.set_alt_raster = xm6_set_alt_raster;
  g_xm6.set_render_bg0 = xm6_set_render_bg0;
  g_xm6.set_transparency_enabled = xm6_set_transparency_enabled;
  g_xm6.get_render_mode = xm6_get_render_mode;
  g_xm6.set_midi_enabled = xm6_set_midi_enabled;
  g_xm6.midi_read_output = xm6_midi_read_output;
  g_xm6.midi_write_input = xm6_midi_write_input;
  g_xm6.state_size = xm6_state_size;
  g_xm6.save_state_mem = xm6_save_state_mem;
  g_xm6.load_state_mem = xm6_load_state_mem;
  g_xm6.get_main_ram = xm6_get_main_ram;
  g_xm6.diag_init_probe = xm6_diag_init_probe;
  g_xm6.video_attach_default_buffer = xm6_video_attach_default_buffer;

  if (!load_required_symbol(&g_xm6.create, "xm6_create") ||
      !load_required_symbol(&g_xm6.destroy, "xm6_destroy") ||
      !load_required_symbol(&g_xm6.set_message_callback, "xm6_set_message_callback") ||
      !load_required_symbol(&g_xm6.exec, "xm6_exec") ||
      !load_required_symbol(&g_xm6.reset, "xm6_reset") ||
      !load_required_symbol(&g_xm6.set_power, "xm6_set_power") ||
      !load_required_symbol(&g_xm6.video_poll, "xm6_video_poll") ||
      !load_required_symbol(&g_xm6.video_consume, "xm6_video_consume") ||
      !load_required_symbol(&g_xm6.audio_configure, "xm6_audio_configure") ||
      !load_required_symbol(&g_xm6.audio_mix, "xm6_audio_mix") ||
      !load_required_symbol(&g_xm6.input_joy, "xm6_input_joy") ||
      !load_required_symbol(&g_xm6.input_key, "xm6_input_key") ||
      !load_required_symbol(&g_xm6.input_mouse, "xm6_input_mouse") ||
      !load_required_symbol(&g_xm6.input_mouse_reset, "xm6_input_mouse_reset") ||
      !load_required_symbol(&g_xm6.mount_fdd, "xm6_mount_fdd") ||
      !load_required_symbol(&g_xm6.eject_fdd, "xm6_eject_fdd") ||
      !load_required_symbol(&g_xm6.state_size, "xm6_state_size") ||
      !load_required_symbol(&g_xm6.save_state_mem, "xm6_save_state_mem") ||
      !load_required_symbol(&g_xm6.load_state_mem, "xm6_load_state_mem")) {
    unload_xm6_api();
    return false;
  }

  core_log(RETRO_LOG_INFO, "[xm6-libretro] Loaded built-in xm6 core");
  return true;
#else
  if (g_xm6.module) {
    return true;
  }

#if defined(_WIN32)
  char core_dir[MAX_PATH] = {};
  char dll_path[MAX_PATH] = {};
  if (get_core_module_dir(core_dir, sizeof(core_dir))) {
    std::snprintf(dll_path, sizeof(dll_path), "%s%s", core_dir, "xm6core.dll");
    g_xm6.module = LoadLibraryA(dll_path);
  }

  if (!g_xm6.module) {
    g_xm6.module = LoadLibraryA("xm6core.dll");
  }
#else
  char core_dir[PATH_MAX] = {};
  char so_path[PATH_MAX] = {};
  if (get_core_module_dir(core_dir, sizeof(core_dir))) {
    std::snprintf(so_path, sizeof(so_path), "%s%s", core_dir, "xm6core.so");
    g_xm6.module = dlopen(so_path, RTLD_NOW | RTLD_LOCAL);
  }
  if (!g_xm6.module) {
    g_xm6.module = dlopen("xm6core.so", RTLD_NOW | RTLD_LOCAL);
  }
#endif

  if (!g_xm6.module) {
#if defined(_WIN32)
    core_log_last_error("Could not load xm6core.dll");
#else
    core_log_last_error("Could not load xm6core.so");
#endif
    return false;
  }

  if (!load_required_symbol(&g_xm6.create, "xm6_create") ||
      !load_required_symbol(&g_xm6.destroy, "xm6_destroy") ||
      !load_required_symbol(&g_xm6.set_message_callback, "xm6_set_message_callback") ||
      !load_required_symbol(&g_xm6.exec, "xm6_exec") ||
      !load_required_symbol(&g_xm6.reset, "xm6_reset") ||
      !load_required_symbol(&g_xm6.set_power, "xm6_set_power") ||
      !load_required_symbol(&g_xm6.video_poll, "xm6_video_poll") ||
      !load_required_symbol(&g_xm6.video_consume, "xm6_video_consume") ||
      !load_required_symbol(&g_xm6.audio_configure, "xm6_audio_configure") ||
      !load_required_symbol(&g_xm6.audio_mix, "xm6_audio_mix") ||
      !load_required_symbol(&g_xm6.input_joy, "xm6_input_joy") ||
      !load_required_symbol(&g_xm6.input_key, "xm6_input_key") ||
      !load_required_symbol(&g_xm6.input_mouse, "xm6_input_mouse") ||
      !load_required_symbol(&g_xm6.input_mouse_reset, "xm6_input_mouse_reset") ||
      !load_required_symbol(&g_xm6.mount_fdd, "xm6_mount_fdd") ||
      !load_required_symbol(&g_xm6.eject_fdd, "xm6_eject_fdd") ||
      !load_required_symbol(&g_xm6.state_size, "xm6_state_size") ||
      !load_required_symbol(&g_xm6.save_state_mem, "xm6_save_state_mem") ||
      !load_required_symbol(&g_xm6.load_state_mem, "xm6_load_state_mem")) {
    unload_xm6_api();
    return false;
  }

  load_optional_symbol(&g_xm6.get_main_ram, "xm6_get_main_ram");
  if (!g_xm6.get_main_ram) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] Optional symbol not found: xm6_get_main_ram (RAM exposure disabled)");
  }

  load_optional_symbol(&g_xm6.diag_init_probe, "xm6_diag_init_probe");
  load_optional_symbol(&g_xm6.video_attach_default_buffer, "xm6_video_attach_default_buffer");
  load_optional_symbol(&g_xm6.get_video_refresh_hz, "xm6_get_video_refresh_hz");
  load_optional_symbol(&g_xm6.get_video_layout, "xm6_get_video_layout");
  load_optional_symbol(&g_xm6.exec_to_frame, "xm6_exec_to_frame");
  load_optional_symbol(&g_xm6.exec_events_only, "xm6_exec_events_only");
  load_optional_symbol(&g_xm6.exec_mpu_nowait_step, "xm6_exec_mpu_nowait_step");
  load_optional_symbol(&g_xm6.set_system_dir, "xm6_set_system_dir");
  load_optional_symbol(&g_xm6.mount_sasi_hdd, "xm6_mount_sasi_hdd");
  load_optional_symbol(&g_xm6.mount_scsi_hdd, "xm6_mount_scsi_hdd");
  load_optional_symbol(&g_xm6.set_joy_type, "xm6_set_joy_type");
  load_optional_symbol(&g_xm6.set_system_clock, "xm6_set_system_clock");
  load_optional_symbol(&g_xm6.set_ram_size, "xm6_set_ram_size");
  load_optional_symbol(&g_xm6.set_fast_floppy, "xm6_set_fast_floppy");
  load_optional_symbol(&g_xm6.set_master_volume, "xm6_set_master_volume");
  load_optional_symbol(&g_xm6.set_fm_volume, "xm6_set_fm_volume");
  load_optional_symbol(&g_xm6.set_adpcm_volume, "xm6_set_adpcm_volume");
  load_optional_symbol(&g_xm6.set_adpcm_interp, "xm6_set_adpcm_interp");
  load_optional_symbol(&g_xm6.set_hq_adpcm_enabled, "xm6_set_hq_adpcm_enabled");
  load_optional_symbol(&g_xm6.set_reverb_level, "xm6_set_reverb_level");
  load_optional_symbol(&g_xm6.set_eq_bass2_level, "xm6_set_eq_bass2_level");
  load_optional_symbol(&g_xm6.set_eq_bass_level, "xm6_set_eq_bass_level");
  load_optional_symbol(&g_xm6.set_eq_presence_level, "xm6_set_eq_presence_level");
  load_optional_symbol(&g_xm6.set_eq_mid_level, "xm6_set_eq_mid_level");
  load_optional_symbol(&g_xm6.set_eq_treble_level, "xm6_set_eq_treble_level");
  load_optional_symbol(&g_xm6.set_eq_air_level, "xm6_set_eq_air_level");
  load_optional_symbol(&g_xm6.set_surround_enabled, "xm6_set_surround_enabled");
  load_optional_symbol(&g_xm6.set_audio_engine, "xm6_set_audio_engine");
  load_optional_symbol(&g_xm6.set_legacy_dmac_cnt, "xm6_set_legacy_dmac_cnt");
  load_optional_symbol(&g_xm6.set_mouse_speed, "xm6_set_mouse_speed");
  load_optional_symbol(&g_xm6.set_mouse_port, "xm6_set_mouse_port");
  load_optional_symbol(&g_xm6.set_mouse_swap, "xm6_set_mouse_swap");
  load_optional_symbol(&g_xm6.set_render_mode, "xm6_set_render_mode");
  load_optional_symbol(&g_xm6.set_alt_raster, "xm6_set_alt_raster");
  load_optional_symbol(&g_xm6.set_render_bg0, "xm6_set_render_bg0");
  load_optional_symbol(&g_xm6.set_transparency_enabled, "xm6_set_transparency_enabled");
  load_optional_symbol(&g_xm6.get_render_mode, "xm6_get_render_mode");
  load_optional_symbol(&g_xm6.set_midi_enabled, "xm6_set_midi_enabled");
  load_optional_symbol(&g_xm6.midi_read_output, "xm6_midi_read_output");
  load_optional_symbol(&g_xm6.midi_write_input, "xm6_midi_write_input");

  core_log(RETRO_LOG_INFO, "[xm6-libretro] Loaded xm6core.dll");
  return true;
#endif
}

static bool ensure_xm6_handle()
{
  if (g_xm6_handle) {
    return true;
  }
  if (!load_xm6_api()) {
    return false;
  }

  g_xm6_handle = g_xm6.create();
  if (!g_xm6_handle) {
    if (g_xm6.diag_init_probe) {
      char diag_msg[512] = {};
      if (g_xm6.diag_init_probe(diag_msg, static_cast<unsigned int>(sizeof(diag_msg))) == XM6CORE_OK &&
          diag_msg[0] != '\0') {
        core_log(RETRO_LOG_ERROR, "[xm6-libretro] %s", diag_msg);
      }
    }
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] xm6_create failed");
    return false;
  }

  if (g_xm6.video_attach_default_buffer) {
    const int rc = g_xm6.video_attach_default_buffer(g_xm6_handle, 1024, 1024);
    if (rc != XM6CORE_OK) {
      core_log(RETRO_LOG_WARN, "[xm6-libretro] video_attach_default_buffer failed rc=%d", rc);
    }
  }

  if (g_xm6.set_message_callback) {
    g_xm6.set_message_callback(g_xm6_handle, xm6_host_message_cb, nullptr);
  }
  if (g_xm6.set_surround_enabled) {
    g_xm6.set_surround_enabled(g_xm6_handle, g_surround_enabled ? 1 : 0);
  }

  g_sample_rate = desired_audio_sample_rate_for_engine(g_audio_engine);
  if (g_xm6.audio_configure(g_xm6_handle, g_sample_rate) != XM6CORE_OK) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] audio configure failed");
  } else {
    core_log(RETRO_LOG_INFO,
             "[xm6-libretro] Audio runtime: engine=%s sample_rate=%u Hz",
             audio_engine_label(g_audio_engine),
             g_sample_rate);
  }

  return true;
}

static void log_memory_exposure_status()
{
  if (!g_xm6_handle) {
    return;
  }

  if (!g_xm6.get_main_ram) {
    core_log(RETRO_LOG_INFO,
             "[xm6-libretro] Memory exposure: SYSTEM_RAM unavailable");
    return;
  }

  unsigned int ram_size = 0;
  void *ram = g_xm6.get_main_ram(g_xm6_handle, &ram_size);
  if (!ram || ram_size == 0) {
    core_log(RETRO_LOG_WARN,
             "[xm6-libretro] Memory exposure: SYSTEM_RAM returned null/empty");
    return;
  }

  core_log(RETRO_LOG_INFO,
           "[xm6-libretro] Memory exposure: SYSTEM_RAM ptr=%p size=%u bytes",
           ram, ram_size);
}

static void begin_hdd_boot_warmup(const char *kind)
{
  if (!g_xm6_handle) {
    return;
  }

  g_hdd_boot_reset_countdown = k_hdd_boot_warmup_frames;
  g_video_not_ready_count = 0;
}

static bool run_pending_hdd_boot_warmup_step()
{
  if (!g_xm6_handle || g_hdd_boot_reset_countdown == 0) {
    return false;
  }

  if (g_xm6.exec_events_only) {
    g_xm6.exec_events_only(g_xm6_handle, 36000);
  } else if (g_xm6.exec) {
    g_xm6.exec(g_xm6_handle, 36000);
  }

  if (g_xm6.video_poll && g_xm6.video_consume) {
    xm6_video_frame_t frame = {};
    if (g_xm6.video_poll(g_xm6_handle, &frame) == XM6CORE_OK) {
      g_xm6.video_consume(g_xm6_handle);
    }
  }

  --g_hdd_boot_reset_countdown;
  if (g_hdd_boot_reset_countdown == 0) {
      g_xm6.reset(g_xm6_handle);
    g_video_not_ready_count = 0;
  }

  return true;
}

static void reset_mouse_state();
static void apply_runtime_core_options();
static void apply_joy_type_options();
static bool mount_current_disk();
static bool set_system_directory_from_frontend();

static void clear_reset_input_latches()
{
  g_prev_start = false;
  g_prev_select = false;
  g_prev_start_keycode = 0;
  g_prev_select_keycode = 0;
  g_prev_midi_hotkey = false;
}

static void sanitize_runtime_for_reset()
{
  g_hdd_boot_reset_countdown = 0;
  g_video_not_ready_count = 0;
  clear_reset_input_latches();
  reset_mouse_state();
}

static void hdd_manual_reset_cold_style()
{
  if (!g_xm6_handle || !g_xm6.set_power) {
    return;
  }

  core_log(RETRO_LOG_INFO,
           "[xm6-libretro] Manual reset while HDD mounted: full cold-style reboot (recreate VM + remount + deferred warm-up reset)");

  // Recreate VM instance so manual reset matches content-load cold boot behavior.
  if (g_xm6.destroy) {
    g_xm6.destroy(g_xm6_handle);
    g_xm6_handle = nullptr;
  }

  if (!ensure_xm6_handle()) {
    core_log(RETRO_LOG_ERROR,
             "[xm6-libretro] HDD manual reset failed: could not recreate VM handle");
    g_game_loaded = false;
    return;
  }

  if (!set_system_directory_from_frontend()) {
    core_log(RETRO_LOG_WARN,
             "[xm6-libretro] HDD manual reset: could not set system directory");
  }

  // Re-apply runtime options before remount so VM-side config mirrors content-load
  // cold boot behavior as closely as possible.
  apply_runtime_core_options();
  apply_joy_type_options();
  reset_mouse_state();

  if (!mount_current_disk()) {
    core_log(RETRO_LOG_ERROR,
             "[xm6-libretro] HDD manual reset failed: remount failed");
    g_game_loaded = false;
    return;
  }

  g_xm6.set_power(g_xm6_handle, 0);
  g_xm6.set_power(g_xm6_handle, 1);
  begin_hdd_boot_warmup("manual reset (HDD full cold-style)");
  g_game_loaded = true;
}

static void arm_savestate_guard_frames(const char *reason, unsigned frames)
{
  g_savestate_guard_countdown = frames;
  core_log(RETRO_LOG_INFO,
           "[xm6-libretro] Savestate guard armed for %u frames (%s)",
           g_savestate_guard_countdown,
           reason ? reason : "unspecified");
}

static void reset_mouse_state()
{
  g_mouse_x = 0;
  g_mouse_y = 0;
  g_mouse_left = false;
  g_mouse_right = false;
  if (g_xm6_handle && g_xm6.input_mouse_reset) {
    g_xm6.input_mouse_reset(g_xm6_handle);
  }
}

static void destroy_xm6_handle()
{
  if (g_xm6_handle && g_xm6.destroy) {
    g_xm6.destroy(g_xm6_handle);
  }
  g_xm6_handle = nullptr;
  g_game_loaded = false;
  g_current_fps = k_default_fps;
  g_frame_aspect = static_cast<float>(k_default_aspect);
  g_audio_fraction = 0.0;
  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_target_drives.clear();
  g_disk_index = 0;
  g_inserted_disk_index[0] = -1;
  g_inserted_disk_index[1] = -1;
  g_disk_ejected = false;
  g_content_is_hdd = false;
  g_hdd_is_scsi = false;
  g_hdd_slot = 0;
  g_hdd_target_auto = true;
  g_hdd_boot_reset_countdown = 0;
  g_prev_start = false;
  g_prev_select = false;
  g_prev_start_keycode = 0;
  g_prev_select_keycode = 0;
  g_prev_midi_hotkey = false;
  g_midi_reset_pending = false;
  reset_video_mode_log_state();
  g_video_buffer.clear();
  reset_mouse_state();
}

static bool set_system_directory_from_frontend()
{
  if (!g_environ_cb || !g_xm6.set_system_dir) {
    return true;
  }

  const char *sys_dir = nullptr;
  if (!g_environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sys_dir)) {
    return true;
  }
  if (!sys_dir || !*sys_dir) {
    return true;
  }

  std::string normalized = sys_dir;
  const char last = normalized.empty() ? '\0' : normalized[normalized.size() - 1];
  if (last != '\\' && last != '/') {
#if defined(_WIN32)
    normalized.push_back('\\');
#else
    normalized.push_back('/');
#endif
  }

  core_log(RETRO_LOG_INFO, "[xm6-libretro] set_system_dir: %s", normalized.c_str());
  return g_xm6.set_system_dir(normalized.c_str()) == XM6CORE_OK;
}

static bool get_frontend_system_directory(std::string *out_dir)
{
  if (!out_dir || !g_environ_cb) {
    return false;
  }
  const char *sys_dir = nullptr;
  if (!g_environ_cb(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY, &sys_dir) ||
      !sys_dir || !*sys_dir) {
    return false;
  }
  *out_dir = sys_dir;
  return true;
}

static bool file_exists(const std::string &path)
{
  if (path.empty()) {
    return false;
  }
  struct stat st = {};
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }
  return !S_ISDIR(st.st_mode);
}

static bool get_file_size_bytes(const std::string &path, unsigned long long *out_size)
{
  if (out_size) {
    *out_size = 0;
  }
  if (path.empty()) {
    return false;
  }

  struct stat st = {};
  if (stat(path.c_str(), &st) != 0) {
    return false;
  }
  if (S_ISDIR(st.st_mode)) {
    return false;
  }

  const unsigned long long size = static_cast<unsigned long long>(st.st_size);
  if (out_size) {
    *out_size = size;
  }
  return true;
}

static std::string join_path(const std::string &dir, const char *name)
{
  if (!name || !*name) {
    return dir;
  }
  if (dir.empty()) {
    return std::string(name);
  }
  const char last = dir[dir.size() - 1];
  if (last == '\\' || last == '/') {
    return dir + name;
  }
#if defined(_WIN32)
  return dir + "\\" + name;
#else
  return dir + "/" + name;
#endif
}

static bool system_file_exists(const char *name, unsigned long long *out_size = nullptr)
{
  std::string sys_dir;
  if (!get_frontend_system_directory(&sys_dir)) {
    if (out_size) {
      *out_size = 0;
    }
    return false;
  }

  return get_file_size_bytes(join_path(sys_dir, name), out_size);
}

static bool have_scsi_ext_rom()
{
  unsigned long long size = 0;
  return system_file_exists("SCSIEXROM.DAT", &size) &&
         (size == 8192ull || size == 8160ull || size == 131072ull);
}

static bool is_classic_sasi_hdf_size(unsigned long long size)
{
  return size == 0x9f5400ull || size == 0x13c9800ull || size == 0x2793000ull;
}

static bool path_has_extension(const std::string &path, const char *ext);

static bool is_hdd_content_path(const std::string &path)
{
  return path_has_extension(path, ".hdf") || path_has_extension(path, ".hds");
}

static void resolve_hdd_target_for_path(const std::string &path, bool *out_is_scsi,
                                        int *out_slot, const char **out_reason)
{
  bool is_scsi = g_hdd_is_scsi;
  int slot = g_hdd_slot;
  const char *reason = g_hdd_target_auto ? "auto fallback" : "manual override";

  if (path_has_extension(path, ".hds")) {
    is_scsi = true;
    slot = 0;
    reason = "HDS content";
  } else if (g_hdd_target_auto) {
    unsigned long long size = 0;
    if (get_file_size_bytes(path, &size)) {
      if (is_classic_sasi_hdf_size(size)) {
        is_scsi = false;
        slot = 0;
        reason = "classic SASI size";
      } else if ((size & 0x1ffull) == 0 && size >= 0x9f5400ull && size <= 0xfff00000ull) {
        is_scsi = true;
        slot = 0;
        reason = "512-byte aligned HDD size";
      }
    }
  }

  if (out_is_scsi) {
    *out_is_scsi = is_scsi;
  }
  if (out_slot) {
    *out_slot = slot;
  }
  if (out_reason) {
    *out_reason = reason;
  }
}

static void log_system_bios_probe()
{
  std::string sys_dir;
  if (!get_frontend_system_directory(&sys_dir)) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] Frontend system dir is empty/unavailable");
    return;
  }

  const std::string ipl = join_path(sys_dir, "IPLROM.DAT");
  const std::string cg = join_path(sys_dir, "CGROM.DAT");
  const std::string cgtmp = join_path(sys_dir, "CGROM.TMP");
  const std::string scsiint = join_path(sys_dir, "SCSIINROM.DAT");
  const std::string scsiext = join_path(sys_dir, "SCSIEXROM.DAT");

  const bool have_ipl = file_exists(ipl);
  const bool have_cg = file_exists(cg) || file_exists(cgtmp);
  const bool have_scsiint = file_exists(scsiint);
  const bool have_scsiext = file_exists(scsiext);
  unsigned long long ipl_size = 0;
  unsigned long long cg_size = 0;
  unsigned long long cgtmp_size = 0;
  unsigned long long scsiint_size = 0;
  unsigned long long scsiext_size = 0;
  const bool have_ipl_size = get_file_size_bytes(ipl, &ipl_size);
  const bool have_cg_size = get_file_size_bytes(cg, &cg_size);
  const bool have_cgtmp_size = get_file_size_bytes(cgtmp, &cgtmp_size);
  const bool have_scsiint_size = get_file_size_bytes(scsiint, &scsiint_size);
  const bool have_scsiext_size = get_file_size_bytes(scsiext, &scsiext_size);

  core_log(RETRO_LOG_INFO, "[xm6-libretro] system dir: %s", sys_dir.c_str());
  if (!have_ipl) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Missing BIOS file: %s", ipl.c_str());
  }
  if (!have_cg) {
    core_log(RETRO_LOG_ERROR,
             "[xm6-libretro] Missing BIOS file: %s (or %s)", cg.c_str(), cgtmp.c_str());
  }
  if (have_scsiint_size) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] SCSIINROM.DAT size=%llu bytes",
             scsiint_size);
  } else if (have_scsiint) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] SCSIINROM.DAT exists but size probe failed");
  }
  if (have_scsiext_size) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] SCSIEXROM.DAT size=%llu bytes",
             scsiext_size);
  } else if (have_scsiext) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] SCSIEXROM.DAT exists but size probe failed");
  }
  if (have_ipl_size) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] IPLROM.DAT size=%llu bytes (expected 131072)",
             ipl_size);
    if (ipl_size != 131072ull) {
      core_log(RETRO_LOG_ERROR, "[xm6-libretro] IPLROM.DAT invalid size: %llu", ipl_size);
    }
  }
  if (have_cg_size) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] CGROM.DAT size=%llu bytes (expected 786432)",
             cg_size);
    if (cg_size != 786432ull) {
      core_log(RETRO_LOG_ERROR, "[xm6-libretro] CGROM.DAT invalid size: %llu", cg_size);
    }
  }
  if (have_cgtmp_size) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] CGROM.TMP size=%llu bytes (expected 786432)",
             cgtmp_size);
    if (cgtmp_size != 786432ull) {
      core_log(RETRO_LOG_ERROR, "[xm6-libretro] CGROM.TMP invalid size: %llu", cgtmp_size);
    }
  }
  if (have_scsiext_size &&
      scsiext_size != 8192ull &&
      scsiext_size != 8160ull &&
      scsiext_size != 131072ull) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] SCSIEXROM.DAT unexpected size: %llu", scsiext_size);
  }
  if (have_scsiint_size &&
      scsiint_size != 8192ull &&
      scsiint_size != 8160ull &&
      scsiint_size != 131072ull) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] SCSIINROM.DAT unexpected size: %llu", scsiint_size);
  }
}

static std::string trim_copy(const std::string &s)
{
  size_t begin = 0;
  while (begin < s.size() && std::isspace(static_cast<unsigned char>(s[begin]))) {
    ++begin;
  }
  size_t end = s.size();
  while (end > begin && std::isspace(static_cast<unsigned char>(s[end - 1]))) {
    --end;
  }
  return s.substr(begin, end - begin);
}

static std::string lower_copy(std::string s)
{
  for (char &ch : s) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return s;
}

static std::string path_dirname(const std::string &path)
{
  const size_t slash = path.find_last_of("\\/");
  if (slash == std::string::npos) {
    return std::string();
  }
  return path.substr(0, slash + 1);
}

static bool path_is_absolute(const std::string &path)
{
  if (path.size() >= 2 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':') {
    return true;
  }
  if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
    return true;
  }
  return false;
}

static std::string path_join(const std::string &base_dir, const std::string &relative)
{
  if (relative.empty() || path_is_absolute(relative) || base_dir.empty()) {
    return relative;
  }
  return base_dir + relative;
}

static std::string path_basename_no_ext(const std::string &path)
{
  size_t start = path.find_last_of("\\/");
  if (start == std::string::npos) {
    start = 0;
  } else {
    ++start;
  }
  size_t end = path.find_last_of('.');
  if (end == std::string::npos || end < start) {
    end = path.size();
  }
  return path.substr(start, end - start);
}

static bool path_has_extension(const std::string &path, const char *ext)
{
  if (!ext) {
    return false;
  }
  const std::string lower = lower_copy(path);
  const std::string wanted = lower_copy(std::string(ext));
  if (lower.size() < wanted.size()) {
    return false;
  }
  return lower.compare(lower.size() - wanted.size(), wanted.size(), wanted) == 0;
}

static std::string strip_wrapping_quotes(std::string s)
{
  if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
    return s.substr(1, s.size() - 2);
  }
  return s;
}

static bool parse_m3u_playlist(const char *m3u_path,
                               std::vector<std::string> *out_paths,
                               std::vector<std::string> *out_labels,
                               std::vector<int> *out_drives)
{
  if (!m3u_path || !out_paths || !out_labels || !out_drives) {
    return false;
  }

  std::ifstream input(m3u_path, std::ios::in | std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::vector<std::string> entries;
  std::vector<std::string> labels;
  std::vector<int> drives;
  bool have_explicit_drive = false;
  const std::string base_dir = path_dirname(m3u_path);

  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    line = trim_copy(line);
    if (line.empty() || line[0] == '#') {
      continue;
    }

    line = strip_wrapping_quotes(line);

    std::string custom_label;
    int target_drive = -1;
    const size_t first_sep = line.find('|');
    if (first_sep != std::string::npos) {
      std::string field1 = trim_copy(line.substr(first_sep + 1));
      line = trim_copy(line.substr(0, first_sep));

      const size_t second_sep = field1.find('|');
      std::string field2;
      if (second_sep != std::string::npos) {
        field2 = trim_copy(field1.substr(second_sep + 1));
        field1 = trim_copy(field1.substr(0, second_sep));
      }

      const std::string lower1 = lower_copy(field1);
      const std::string lower2 = lower_copy(field2);
      if (lower1 == "fdd0") {
        target_drive = 0;
        have_explicit_drive = true;
        custom_label = field2;
      } else if (lower1 == "fdd1") {
        target_drive = 1;
        have_explicit_drive = true;
        custom_label = field2;
      } else if (lower2 == "fdd0") {
        target_drive = 0;
        have_explicit_drive = true;
        custom_label = field1;
      } else if (lower2 == "fdd1") {
        target_drive = 1;
        have_explicit_drive = true;
        custom_label = field1;
      } else {
        custom_label = field1;
      }
    }

    if (line.empty()) {
      continue;
    }

    std::string full_path = path_join(base_dir, line);
    entries.push_back(full_path);
    if (!custom_label.empty()) {
      labels.push_back(custom_label);
    } else {
      labels.push_back(path_basename_no_ext(full_path));
    }
    drives.push_back(target_drive);
  }

  if (entries.empty()) {
    return false;
  }

  if (!have_explicit_drive && entries.size() == 2 && drives.size() == 2) {
    drives[0] = 0;
    drives[1] = 1;
  }

  *out_paths = entries;
  *out_labels = labels;
  *out_drives = drives;
  return true;
}

static void build_disk_labels()
{
  if (g_disk_labels.size() == g_disk_paths.size()) {
    bool all_filled = true;
    for (size_t i = 0; i < g_disk_labels.size(); ++i) {
      if (g_disk_labels[i].empty()) {
        all_filled = false;
        break;
      }
    }
    if (all_filled) {
      return;
    }
  }

  g_disk_labels.clear();
  g_disk_labels.reserve(g_disk_paths.size());
  for (size_t i = 0; i < g_disk_paths.size(); ++i) {
    std::string label = path_basename_no_ext(g_disk_paths[i]);
    if (label.empty()) {
      char fallback[32] = {};
      std::snprintf(fallback, sizeof(fallback), "Disk %u", static_cast<unsigned>(i + 1));
      label = fallback;
    }
    g_disk_labels.push_back(label);
  }
}

static int get_disk_target_drive(unsigned index)
{
  if (index < g_disk_target_drives.size()) {
    const int drive = g_disk_target_drives[index];
    if (drive == 0 || drive == 1) {
      return drive;
    }
  }
  return g_disk_drive;
}

static bool mount_disk_index(unsigned index, bool update_selected = true)
{
  if (!g_xm6_handle || g_disk_paths.empty() || index >= g_disk_paths.size()) {
    return false;
  }

  const std::string &path = g_disk_paths[index];
  if (path.empty()) {
    return false;
  }

  int rc = XM6CORE_ERR_INVALID_ARGUMENT;
  bool mount_as_scsi = g_hdd_is_scsi;
  int mount_slot = g_hdd_slot;
  int target_drive = g_disk_drive;

  if (g_content_is_hdd) {
    const char *reason = nullptr;
    resolve_hdd_target_for_path(path, &mount_as_scsi, &mount_slot, &reason);
    if (mount_as_scsi && !have_scsi_ext_rom()) {
      core_log(RETRO_LOG_ERROR,
               "[xm6-libretro] HDF resolved to SCSI%d (%s) but SCSIEXROM.DAT is missing/invalid",
               mount_slot, reason ? reason : "unknown");
      return false;
    }
    core_log(RETRO_LOG_INFO, "[xm6-libretro] HDF target resolved to %s%d (%s)",
             mount_as_scsi ? "SCSI" : "SASI", mount_slot, reason ? reason : "unknown");
    if (mount_as_scsi) {
      if (!g_xm6.mount_scsi_hdd) {
        core_log(RETRO_LOG_ERROR, "[xm6-libretro] Backend does not export xm6_mount_scsi_hdd");
        return false;
      }
      rc = g_xm6.mount_scsi_hdd(g_xm6_handle, mount_slot, path.c_str());
    } else {
      if (!g_xm6.mount_sasi_hdd) {
        core_log(RETRO_LOG_ERROR, "[xm6-libretro] Backend does not export xm6_mount_sasi_hdd");
        return false;
      }
      rc = g_xm6.mount_sasi_hdd(g_xm6_handle, mount_slot, path.c_str());
    }
  } else {
    target_drive = get_disk_target_drive(index);
    rc = g_xm6.mount_fdd(g_xm6_handle, target_drive, path.c_str(), 0);
  }

  if (rc != XM6CORE_OK) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Failed to mount disk[%u]: %s",
             index, path.c_str());
    return false;
  }

  if (update_selected) {
    g_disk_index = index;
  }

  if (g_content_is_hdd) {
    core_log(RETRO_LOG_INFO, "[xm6-libretro] Mounted HDD[%u] on %s%d: %s",
             index, mount_as_scsi ? "SCSI" : "SASI", mount_slot, path.c_str());
  } else {
    g_inserted_disk_index[target_drive] = static_cast<int>(index);
    core_log(RETRO_LOG_INFO, "[xm6-libretro] Mounted disk[%u] on FDD%d: %s",
             index, target_drive, path.c_str());
  }

  return true;
}

static bool mount_initial_playlist_disks()
{
  if (g_content_is_hdd) {
    return mount_disk_index(g_disk_index);
  }
  if (!g_xm6_handle || g_disk_paths.empty()) {
    return false;
  }

  g_inserted_disk_index[0] = -1;
  g_inserted_disk_index[1] = -1;

  bool mounted_any = false;
  bool drive_done[2] = { false, false };
  for (unsigned i = 0; i < g_disk_paths.size(); ++i) {
    const int drive = get_disk_target_drive(i);
    if (drive < 0 || drive > 1 || drive_done[drive]) {
      continue;
    }
    if (!mount_disk_index(i, !mounted_any)) {
      return false;
    }
    drive_done[drive] = true;
    mounted_any = true;
  }

  return mounted_any;
}

static bool mount_current_disk()
{
  return mount_disk_index(g_disk_index);
}

static void init_midi_interface()
{
  std::memset(&g_midi_cb, 0, sizeof(g_midi_cb));
  g_supports_midi_interface = false;
  if (g_environ_cb) {
    g_supports_midi_interface =
      g_environ_cb(RETRO_ENVIRONMENT_GET_MIDI_INTERFACE, &g_midi_cb);
  }
}

static void apply_joy_type_options()
{
  if (!g_xm6_handle || !g_xm6.set_joy_type) {
    return;
  }

  for (unsigned port = 0; port < 2; ++port) {
    g_xm6.set_joy_type(g_xm6_handle, static_cast<int>(port), g_joy_type[port]);
  }
}

static bool midi_output_ready()
{
  return g_midi_output_enabled &&
         g_supports_midi_interface &&
         g_midi_cb.write &&
         g_midi_cb.output_enabled &&
         g_midi_cb.output_enabled();
}

static void midi_send_bytes(const unsigned char *bytes, unsigned int length)
{
  if (!bytes || length == 0 || !midi_output_ready()) {
    return;
  }

  for (unsigned int i = 0; i < length; ++i) {
    g_midi_cb.write(bytes[i], 0);
  }
}

static void midi_send_cc_all_channels(unsigned char controller)
{
  if (!midi_output_ready()) {
    return;
  }

  for (unsigned int ch = 0; ch < 16; ++ch) {
    g_midi_cb.write((unsigned char)(0xB0 + ch), 0);
    g_midi_cb.write(controller, 0);
    g_midi_cb.write(0x00, 0);
  }
}

static void midi_send_reset_sequence()
{
  static const unsigned char kResetGM[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
  static const unsigned char kResetGS[] = { 0xF0, 0x41, 0x10, 0x42, 0x12, 0x40, 0x00, 0x7F, 0x00, 0x41, 0xF7 };
  static const unsigned char kResetXG[] = { 0xF0, 0x43, 0x10, 0x4C, 0x00, 0x00, 0x7E, 0x00, 0xF7 };
  static const unsigned char kResetLA[] = { 0xF0, 0x41, 0x10, 0x16, 0x12, 0x7F, 0x00, 0x00, 0x00, 0x01, 0xF7 };

  if (!midi_output_ready()) {
    return;
  }

  midi_send_cc_all_channels(0x78); // All Sound Off
  midi_send_cc_all_channels(0x7B); // All Notes Off
  midi_send_cc_all_channels(0x7D); // Omni Off
  midi_send_cc_all_channels(0x7F); // Poly On
  midi_send_cc_all_channels(0x79); // Reset All Controllers

  switch (g_midi_output_type) {
    case MIDI_OUTPUT_LA:
      midi_send_bytes(kResetLA, static_cast<unsigned int>(sizeof(kResetLA)));
      break;
    case MIDI_OUTPUT_GS:
      midi_send_bytes(kResetGM, static_cast<unsigned int>(sizeof(kResetGM)));
      midi_send_bytes(kResetGS, static_cast<unsigned int>(sizeof(kResetGS)));
      break;
    case MIDI_OUTPUT_XG:
      midi_send_bytes(kResetGM, static_cast<unsigned int>(sizeof(kResetGM)));
      midi_send_bytes(kResetXG, static_cast<unsigned int>(sizeof(kResetXG)));
      break;
    case MIDI_OUTPUT_GM:
    default:
      midi_send_bytes(kResetGM, static_cast<unsigned int>(sizeof(kResetGM)));
      break;
  }

  if (g_midi_cb.flush) {
    g_midi_cb.flush();
  }
}

static void midi_queue_reset_if_enabled()
{
  g_midi_reset_pending = g_midi_output_enabled;
}

static void pump_frontend_midi_input()
{
  if (!g_xm6_handle || !g_xm6.midi_write_input || !g_midi_output_enabled ||
      !g_supports_midi_interface || !g_midi_cb.input_enabled || !g_midi_cb.read) {
    return;
  }

  if (!g_midi_cb.input_enabled()) {
    return;
  }

  unsigned char inbuf[256];
  unsigned int count = 0;
  while (count < static_cast<unsigned int>(sizeof(inbuf)) && g_midi_cb.read(&inbuf[count])) {
    ++count;
  }

  if (count > 0) {
    g_xm6.midi_write_input(g_xm6_handle, inbuf, count);
  }
}

static void pump_core_midi_output()
{
  if (!g_xm6_handle || !g_xm6.midi_read_output || !midi_output_ready()) {
    return;
  }

  unsigned char outbuf[512];
  while (true) {
    unsigned int count = 0;
    if (g_xm6.midi_read_output(g_xm6_handle, outbuf,
                               static_cast<unsigned int>(sizeof(outbuf)),
                               &count) != XM6CORE_OK) {
      break;
    }
    if (count == 0) {
      break;
    }
    for (unsigned int i = 0; i < count; ++i) {
      g_midi_cb.write(outbuf[i], 0);
    }
  }

  if (g_midi_cb.flush) {
    g_midi_cb.flush();
  }
}

static void apply_runtime_core_options()
{
  if (!g_xm6_handle) {
    return;
  }

  const unsigned desired_sample_rate = desired_audio_sample_rate_for_engine(g_audio_engine);
  const bool sample_rate_changed = (g_sample_rate != desired_sample_rate);
  if (sample_rate_changed) {
    g_sample_rate = desired_sample_rate;
    g_audio_fraction = 0.0;
  }

  if (g_xm6.set_system_clock) {
    g_xm6.set_system_clock(g_xm6_handle, g_system_clock);
  }
  if (g_xm6.set_ram_size) {
    g_xm6.set_ram_size(g_xm6_handle, g_ram_size);
  }
  if (g_xm6.set_fast_floppy) {
    g_xm6.set_fast_floppy(g_xm6_handle, g_fast_floppy ? 1 : 0);
  }
  if (g_xm6.set_render_mode) {
    g_xm6.set_render_mode(g_xm6_handle, g_render_mode);
  }
  if (g_xm6.set_alt_raster) {
    g_xm6.set_alt_raster(g_xm6_handle, g_alt_raster_enabled ? 1 : 0);
  }
  if (g_xm6.set_render_bg0) {
    g_xm6.set_render_bg0(g_xm6_handle, g_render_bg0_enabled ? 1 : 0);
  }
  if (g_xm6.set_transparency_enabled) {
    g_xm6.set_transparency_enabled(g_xm6_handle, g_transparency_enabled ? 1 : 0);
  }
  if (g_xm6.set_master_volume) {
    g_xm6.set_master_volume(g_xm6_handle, 100);
  }
  if (g_xm6.set_fm_volume) {
    g_xm6.set_fm_volume(g_xm6_handle, g_fm_volume);
  }
  if (g_xm6.set_adpcm_volume) {
    g_xm6.set_adpcm_volume(g_xm6_handle, g_adpcm_volume);
  }
  if (g_xm6.set_adpcm_interp) {
    g_xm6.set_adpcm_interp(g_xm6_handle, 1);
  }
  if (g_xm6.set_hq_adpcm_enabled) {
    g_xm6.set_hq_adpcm_enabled(g_xm6_handle, g_hq_adpcm_enabled ? 1 : 0);
  }
  if (g_xm6.set_reverb_level) {
    g_xm6.set_reverb_level(g_xm6_handle, g_reverb_level);
  }
  if (g_xm6.set_eq_bass2_level) {
    g_xm6.set_eq_bass2_level(g_xm6_handle, g_eq_bass_level);
  }
  if (g_xm6.set_eq_bass_level) {
    g_xm6.set_eq_bass_level(g_xm6_handle, g_eq_sub_bass_level);
  }
  if (g_xm6.set_eq_presence_level) {
    g_xm6.set_eq_presence_level(g_xm6_handle, g_eq_presence_level);
  }
  if (g_xm6.set_eq_mid_level) {
    g_xm6.set_eq_mid_level(g_xm6_handle, g_eq_mid_level);
  }
  if (g_xm6.set_eq_treble_level) {
    g_xm6.set_eq_treble_level(g_xm6_handle, g_eq_treble_level);
  }
  if (g_xm6.set_eq_air_level) {
    g_xm6.set_eq_air_level(g_xm6_handle, g_eq_air_level);
  }
  if (g_xm6.set_surround_enabled) {
    g_xm6.set_surround_enabled(g_xm6_handle, g_surround_enabled ? 1 : 0);
  }
  if (g_xm6.set_audio_engine) {
    g_xm6.set_audio_engine(g_xm6_handle, g_audio_engine);
  }
  if (g_xm6.audio_configure && sample_rate_changed) {
    if (g_xm6.audio_configure(g_xm6_handle, g_sample_rate) != XM6CORE_OK) {
      core_log(RETRO_LOG_WARN,
               "[xm6-libretro] audio reconfigure failed for %s at %u Hz",
               audio_engine_label(g_audio_engine),
               g_sample_rate);
    } else if (g_environ_cb) {
      retro_system_av_info av_info = {};
      fill_av_info(&av_info);
      g_environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
      core_log(RETRO_LOG_INFO,
               "[xm6-libretro] Audio runtime: engine=%s sample_rate=%u Hz",
               audio_engine_label(g_audio_engine),
               g_sample_rate);
    }
  }
  if (g_xm6.set_legacy_dmac_cnt) {
    g_xm6.set_legacy_dmac_cnt(g_xm6_handle, g_legacy_dmac_cnt ? 1 : 0);
  }
  if (g_xm6.set_midi_enabled) {
    g_xm6.set_midi_enabled(g_xm6_handle, g_midi_output_enabled ? 1 : 0);
  }
  if (g_xm6.set_mouse_speed) {
    g_xm6.set_mouse_speed(g_xm6_handle, g_mouse_speed);
  }
  if (g_xm6.set_mouse_port) {
    g_xm6.set_mouse_port(g_xm6_handle, g_mouse_port);
  }
  if (g_xm6.set_mouse_swap) {
    g_xm6.set_mouse_swap(g_xm6_handle, g_mouse_swap ? 1 : 0);
  }
}

static bool equals_ci(const char *lhs, const char *rhs)
{
  if (!lhs || !rhs) {
    return false;
  }
#if defined(_WIN32)
  return _stricmp(lhs, rhs) == 0;
#else
  return strcasecmp(lhs, rhs) == 0;
#endif
}

static int parse_joy_type_value(const char *value, int default_type)
{
  if (!value) {
    return default_type;
  }

  if (equals_ci(value, "cpsf_sfc") ||
      equals_ci(value, "cpsf-sfc") ||
      equals_ci(value, "CPSF-SFC (8 Buttons)") ||
      equals_ci(value, "cpsf sfc")) {
    return 7;
  }
  if (equals_ci(value, "cpsf_md") ||
      equals_ci(value, "cpsf-md") ||
      equals_ci(value, "CPSF-MD (8 Buttons)") ||
      equals_ci(value, "cpsf md")) {
    return 8;
  }
  if (equals_ci(value, "atari_ss") ||
      equals_ci(value, "atari-ss") ||
      equals_ci(value, "atari + start/select")) {
    return 2;
  }
  if (equals_ci(value, "atari") || equals_ci(value, "2button") || equals_ci(value, "2-button")) {
    return 1;
  }
  if (equals_ci(value, "disabled") || equals_ci(value, "none")) {
    return 0;
  }

  return default_type;
}

static void apply_core_option_values()
{
  if (!g_environ_cb) {
    return;
  }

  retro_variable var = {};

  var.key = "xm6_disk_drive";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_disk_drive = (std::strcmp(var.value, "FDD1") == 0) ? 1 : 0;
  }

  var.key = "xm6_exec_mode";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_use_exec_to_frame = std::strcmp(var.value, "legacy_exec") != 0;
  }

  var.key = "xm6_pad_start_select";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "f_keys") == 0) {
      g_pad_start_select_mode = START_SELECT_F_KEYS;
    } else if (std::strcmp(var.value, "opt_keys") == 0) {
      g_pad_start_select_mode = START_SELECT_OPT_KEYS;
    } else if (std::strcmp(var.value, "disabled") == 0) {
      g_pad_start_select_mode = START_SELECT_DISABLED;
    } else {
      g_pad_start_select_mode = START_SELECT_XF_KEYS;
    }
  }

  var.key = "xm6_cpu_clock";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "12mhz") == 0) {
      g_system_clock = 1;
    } else if (std::strcmp(var.value, "16mhz") == 0) {
      g_system_clock = 3;
    } else if (std::strcmp(var.value, "22mhz") == 0) {
      g_system_clock = 5;
    } else {
      g_system_clock = 0;
    }
  }

  var.key = "xm6_joy1_type";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_joy_type[0] = parse_joy_type_value(var.value, 1);
  }

  var.key = "xm6_joy2_type";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_joy_type[1] = parse_joy_type_value(var.value, 0);
  }

  var.key = "xm6_ram_size";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "2mb") == 0) {
      g_ram_size = 0;
    } else if (std::strcmp(var.value, "4mb") == 0) {
      g_ram_size = 1;
    } else if (std::strcmp(var.value, "6mb") == 0) {
      g_ram_size = 2;
    } else if (std::strcmp(var.value, "8mb") == 0) {
      g_ram_size = 3;
    } else if (std::strcmp(var.value, "10mb") == 0) {
      g_ram_size = 4;
    } else {
      g_ram_size = 5;
    }
  }

  var.key = "xm6_fast_floppy";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_fast_floppy = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_render_mode";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "fast") == 0) {
      g_render_mode = XM6CORE_RENDER_MODE_FAST;
    } else {
      g_render_mode = XM6CORE_RENDER_MODE_ORIGINAL;
    }
  }

  var.key = "xm6_alt_raster";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_alt_raster_enabled = (std::strcmp(var.value, "disabled") != 0);
  }

  var.key = "xm6_transparency";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_transparency_enabled = (std::strcmp(var.value, "disabled") != 0);
  }

  var.key = "xm6_fm_volume";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_fm_volume = std::atoi(var.value);
  }

  var.key = "xm6_adpcm_volume";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_adpcm_volume = std::atoi(var.value);
  }

  var.key = "xm6_hq_adpcm";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_hq_adpcm_enabled = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_reverb";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_reverb_level = std::atoi(var.value);
    if (g_reverb_level < 0) {
      g_reverb_level = 0;
    } else if (g_reverb_level > 100) {
      g_reverb_level = 100;
    }
  }

  var.key = "xm6_eq_sub_bass";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_sub_bass_level = std::atoi(var.value);
    if (g_eq_sub_bass_level < 0) {
      g_eq_sub_bass_level = 0;
    } else if (g_eq_sub_bass_level > 100) {
      g_eq_sub_bass_level = 100;
    }
  }

  var.key = "xm6_eq_bass";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_bass_level = std::atoi(var.value);
    if (g_eq_bass_level < 0) {
      g_eq_bass_level = 0;
    } else if (g_eq_bass_level > 100) {
      g_eq_bass_level = 100;
    }
  }

  var.key = "xm6_eq_mid";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_mid_level = std::atoi(var.value);
    if (g_eq_mid_level < 0) {
      g_eq_mid_level = 0;
    } else if (g_eq_mid_level > 100) {
      g_eq_mid_level = 100;
    }
  }

  var.key = "xm6_eq_presence";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_presence_level = std::atoi(var.value);
    if (g_eq_presence_level < 0) {
      g_eq_presence_level = 0;
    } else if (g_eq_presence_level > 100) {
      g_eq_presence_level = 100;
    }
  }

  var.key = "xm6_eq_treble";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_treble_level = std::atoi(var.value);
    if (g_eq_treble_level < 0) {
      g_eq_treble_level = 0;
    } else if (g_eq_treble_level > 100) {
      g_eq_treble_level = 100;
    }
  }

  var.key = "xm6_eq_air";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_eq_air_level = std::atoi(var.value);
    if (g_eq_air_level < 0) {
      g_eq_air_level = 0;
    } else if (g_eq_air_level > 100) {
      g_eq_air_level = 100;
    }
  }

  var.key = "xm6_surround";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_surround_enabled = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_audio_engine";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "PX68k") == 0) {
      g_audio_engine = XM6CORE_AUDIO_ENGINE_PX68K;
    } else if (std::strcmp(var.value, "YMFM") == 0) {
      g_audio_engine = XM6CORE_AUDIO_ENGINE_YMFM;
    } else if (std::strcmp(var.value, "YMFM direct") == 0 || std::strcmp(var.value, "YMFM raw") == 0) {
      g_audio_engine = XM6CORE_AUDIO_ENGINE_YMFM_DIRECT;
    } else if (std::strcmp(var.value, "X68Sound") == 0) {
      g_audio_engine = XM6CORE_AUDIO_ENGINE_X68SOUND;
    } else {
      g_audio_engine = XM6CORE_AUDIO_ENGINE_XM6;
    }
  }

  var.key = "xm6_legacy_dmac_cnt";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_legacy_dmac_cnt = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_midi_output";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_midi_output_enabled = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_midi_output_type";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "LA") == 0) {
      g_midi_output_type = MIDI_OUTPUT_LA;
    } else if (std::strcmp(var.value, "GS") == 0) {
      g_midi_output_type = MIDI_OUTPUT_GS;
    } else if (std::strcmp(var.value, "XG") == 0) {
      g_midi_output_type = MIDI_OUTPUT_XG;
    } else {
      g_midi_output_type = MIDI_OUTPUT_GM;
    }
  }

  var.key = "xm6_pointer_device";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "mouse") == 0) {
      g_pointer_device_mode = POINTER_DEVICE_MOUSE;
    } else {
      g_pointer_device_mode = POINTER_DEVICE_DISABLED;
    }
  }

  var.key = "xm6_mouse_port";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "scc") == 0) {
      g_mouse_port = 1;
    } else if (std::strcmp(var.value, "keyboard") == 0) {
      g_mouse_port = 2;
    } else {
      g_mouse_port = 0;
    }
  }

  var.key = "xm6_mouse_speed";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_mouse_speed = std::atoi(var.value);
    if (g_mouse_speed < 0) g_mouse_speed = 0;
    if (g_mouse_speed > 512) g_mouse_speed = 512;
  }

  var.key = "xm6_mouse_swap";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_mouse_swap = (std::strcmp(var.value, "enabled") == 0);
  }

  var.key = "xm6_hdd_target";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    if (std::strcmp(var.value, "auto") == 0) {
      g_hdd_target_auto = true;
      g_hdd_is_scsi = false;
      g_hdd_slot = 0;
    } else if (std::strcmp(var.value, "scsi0") == 0) {
      g_hdd_target_auto = false;
      g_hdd_is_scsi = true;
      g_hdd_slot = 0;
    } else if (std::strcmp(var.value, "scsi1") == 0) {
      g_hdd_target_auto = false;
      g_hdd_is_scsi = true;
      g_hdd_slot = 1;
    } else if (std::strcmp(var.value, "sasi1") == 0) {
      g_hdd_target_auto = false;
      g_hdd_is_scsi = false;
      g_hdd_slot = 1;
    } else {
      g_hdd_target_auto = false;
      g_hdd_is_scsi = false;
      g_hdd_slot = 0;
    }
  }

  var.key = "xm6_mpu_nowait";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_mpu_nowait = (std::strcmp(var.value, "enabled") == 0);
  }

  core_log(RETRO_LOG_INFO, "[xm6-libretro] options: drive=FDD%d exec_mode=%s start_select=%s clock=%s joy1=%d joy2=%d ram=%dmb fast_floppy=%s render=%s alt_raster=%s render_bg0=%s vol(master=100 fm=%d adpcm=%d hq_adpcm=%s reverb=%d eq(sub_bass=%d bass=%d mid=%d presence=%d treble=%d air=%d) surround=%s) audio=%s dmac_cnt=%s midi=%s/%s mouse=%s port=%d speed=%d swap=%s hdd=%s mpu_nowait=%s",
           g_disk_drive,
           g_use_exec_to_frame ? "exec_to_frame" : "legacy_exec",
           (g_pad_start_select_mode == START_SELECT_F_KEYS) ? "f_keys" :
           (g_pad_start_select_mode == START_SELECT_OPT_KEYS) ? "opt_keys" :
           (g_pad_start_select_mode == START_SELECT_XF_KEYS) ? "xf_keys" : "disabled",
           (g_system_clock == 1) ? "12mhz" :
           (g_system_clock == 3) ? "16mhz" :
           (g_system_clock == 5) ? "22mhz" : "10mhz",
           g_joy_type[0], g_joy_type[1], (g_ram_size + 1) * 2,
           g_fast_floppy ? "enabled" : "disabled",
           (g_render_mode == XM6CORE_RENDER_MODE_FAST) ? "fast" : "original",
           g_alt_raster_enabled ? "enabled" : "disabled",
           g_render_bg0_enabled ? "enabled" : "disabled",
           g_fm_volume, g_adpcm_volume,
           g_hq_adpcm_enabled ? "enabled" : "disabled",
           g_reverb_level,
           g_eq_sub_bass_level, g_eq_bass_level, g_eq_mid_level, g_eq_presence_level, g_eq_treble_level, g_eq_air_level,
           g_surround_enabled ? "enabled" : "disabled",
           audio_engine_label(g_audio_engine),
           g_legacy_dmac_cnt ? "legacy" : "fixed",
           g_midi_output_enabled ? "enabled" : "disabled",
           (g_midi_output_type == MIDI_OUTPUT_LA) ? "LA" :
           (g_midi_output_type == MIDI_OUTPUT_GS) ? "GS" :
           (g_midi_output_type == MIDI_OUTPUT_XG) ? "XG" : "GM",
           (g_pointer_device_mode == POINTER_DEVICE_MOUSE) ? "mouse" : "disabled",
           g_mouse_port, g_mouse_speed, g_mouse_swap ? "enabled" : "disabled",
           g_hdd_target_auto ? "AUTO" : (g_hdd_is_scsi ? (g_hdd_slot ? "SCSI1" : "SCSI0")
                                                        : (g_hdd_slot ? "SASI1" : "SASI0")),
           g_mpu_nowait ? "enabled" : "disabled");
}

static void register_core_options()
{
  if (!g_environ_cb) {
    return;
  }

  unsigned version = 0;
  g_environ_cb(RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION, &version);

  if (version >= 2) {
    static retro_core_option_v2_category cats[] = {
      { "system",  "System",  "System-level hardware and boot settings." },
      { "video",   "Video",   "Rendering, transparency and raster timing." },
      { "sound",   "Sound",   "Volumes and audio processing." },
      { "input",   "Input",   "Controller, pointer and mouse mapping." },
      { "media",   "Media",   "Disk drive and HDD target selection." },
      { "advanced","Advanced","Low-level compatibility and performance tweaks." },
      { nullptr, nullptr, nullptr }
    };

    static retro_core_option_v2_definition defs[] = {
      {
        "xm6_exec_mode",
        "Frame execution mode",
        nullptr,
        "Choose the execution mode used to drive the emulation loop.",
        nullptr,
        "system",
        {
          { "exec_to_frame", nullptr },
          { "legacy_exec", nullptr },
          { nullptr, nullptr }
        },
        "exec_to_frame"
      },
      {
        "xm6_cpu_clock",
        "System clock",
        nullptr,
        "Select the emulated CPU clock speed.",
        nullptr,
        "system",
        {
          { "10mhz", nullptr },
          { "12mhz", nullptr },
          { "16mhz", nullptr },
          { "22mhz", nullptr },
          { nullptr, nullptr }
        },
        "10mhz"
      },
      {
        "xm6_ram_size",
        "Main RAM size (Reset)",
        nullptr,
        "Sets the amount of main RAM. Requires a reset.",
        nullptr,
        "system",
        {
          { "12mb", nullptr },
          { "10mb", nullptr },
          { "8mb", nullptr },
          { "6mb", nullptr },
          { "4mb", nullptr },
          { "2mb", nullptr },
          { nullptr, nullptr }
        },
        "12mb"
      },
      {
        "xm6_fast_floppy",
        "Fast floppy",
        nullptr,
        "Speed up floppy handling.",
        nullptr,
        "system",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_disk_drive",
        "Disk drive for content swap",
        nullptr,
        "Select which floppy drive receives the mounted content.",
        nullptr,
        "media",
        {
          { "FDD0", nullptr },
          { "FDD1", nullptr },
          { nullptr, nullptr }
        },
        "FDD0"
      },
      {
        "xm6_hdd_target",
        "HDD target",
        nullptr,
        "Choose the active HDD slot and bus.",
        nullptr,
        "media",
        {
          { "auto", nullptr },
          { "sasi0", nullptr },
          { "sasi1", nullptr },
          { "scsi0", nullptr },
          { "scsi1", nullptr },
          { nullptr, nullptr }
        },
        "auto"
      },
      {
        "xm6_render_mode",
        "Video compositor",
        nullptr,
        "Choose the video compositor path.",
        nullptr,
        "video",
        {
          { "original", nullptr },
          { "fast", nullptr },
          { nullptr, nullptr }
        },
        "original"
      },
      {
        "xm6_alt_raster",
        "Alternative raster timing",
        nullptr,
        "Enable alternate raster timing for compatibility tweaks.",
        nullptr,
        "video",
        {
          { "enabled", nullptr },
          { "disabled", nullptr },
          { nullptr, nullptr }
        },
        "enabled"
      },
      {
        "xm6_render_bg0",
        "BG tile transparency rule",
        nullptr,
        "Select the background transparency rule used by the compositor.",
        nullptr,
        "video",
        {
          { "modern", nullptr },
          { "legacy (XM62022Nuevo)", nullptr },
          { nullptr, nullptr }
        },
        "modern"
      },
      {
        "xm6_transparency",
        "Transparency (TR/half-fill)",
        nullptr,
        "Enable or disable TR/half-fill transparency handling.",
        nullptr,
        "video",
        {
          { "enabled", nullptr },
          { "disabled", nullptr },
          { nullptr, nullptr }
        },
        "enabled"
      },
      {
        "xm6_audio_engine",
        "Audio engine",
        nullptr,
        "Select the FM synthesis backend.",
        nullptr,
        "sound",
        {
          { "XM6", nullptr },
          { "PX68k", nullptr },
          { "YMFM", nullptr },
          { "X68Sound", nullptr },
          { nullptr, nullptr }
        },
        "XM6"
      },
      {
        "xm6_fm_volume",
        "FM volume",
        nullptr,
        "Adjust the FM channel mix.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_adpcm_volume",
        "ADPCM volume",
        nullptr,
        "Adjust the ADPCM channel mix.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_hq_adpcm",
        "HQ ADPCM",
        nullptr,
        "Apply the higher quality ADPCM post-processing path.",
        nullptr,
        "sound",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_reverb",
        "Reverb",
        nullptr,
        "Adjust the global reverb intensity. 0 is bypass; 100 is the fullest space.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "0"
      },
      {
        "xm6_eq_sub_bass",
        "Sub-bass EQ",
        nullptr,
        "Adjust the 60 Hz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_eq_bass",
        "Bass EQ",
        nullptr,
        "Adjust the 200 Hz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_eq_mid",
        "Mid EQ",
        nullptr,
        "Adjust the 800 Hz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_eq_presence",
        "Presence EQ",
        nullptr,
        "Adjust the 3 kHz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_eq_treble",
        "Treble EQ",
        nullptr,
        "Adjust the 8 kHz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_eq_air",
        "Air EQ",
        nullptr,
        "Adjust the 16 kHz band of the global EQ. 50 is flat; 0 cuts and 100 boosts.",
        nullptr,
        "sound",
        {
          { "0", nullptr },
          { "10", nullptr },
          { "20", nullptr },
          { "30", nullptr },
          { "40", nullptr },
          { "50", nullptr },
          { "60", nullptr },
          { "70", nullptr },
          { "80", nullptr },
          { "90", nullptr },
          { "100", nullptr },
          { nullptr, nullptr }
        },
        "50"
      },
      {
        "xm6_surround",
        "Surround",
        nullptr,
        "Apply stereo widening to FM and ADPCM.",
        nullptr,
        "sound",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_midi_output",
        "MIDI output",
        nullptr,
        "Enable or disable MIDI output.",
        nullptr,
        "sound",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_midi_output_type",
        "MIDI output type",
        nullptr,
        "Choose the MIDI output profile.",
        nullptr,
        "sound",
        {
          { "GM", nullptr },
          { "LA", nullptr },
          { "GS", nullptr },
          { "XG", nullptr },
          { nullptr, nullptr }
        },
        "GM"
      },
      {
        "xm6_pad_start_select",
        "Map Start/Select to keys",
        nullptr,
        "Select how Start and Select are mapped to X68000 keys.",
        nullptr,
        "input",
        {
          { "xf_keys", nullptr },
          { "f_keys", nullptr },
          { "opt_keys", nullptr },
          { "disabled", nullptr },
          { nullptr, nullptr }
        },
        "xf_keys"
      },
      {
        "xm6_joy1_type",
        "Joystick Port 1 Type",
        nullptr,
        "Set the type used by joystick port 1.",
        nullptr,
        "input",
        {
          { "atari", nullptr },
          { "atari_ss", nullptr },
          { "cpsf_sfc", nullptr },
          { "cpsf_md", nullptr },
          { "disabled", nullptr },
          { nullptr, nullptr }
        },
        "atari"
      },
      {
        "xm6_joy2_type",
        "Joystick Port 2 Type",
        nullptr,
        "Set the type used by joystick port 2.",
        nullptr,
        "input",
        {
          { "disabled", nullptr },
          { "atari", nullptr },
          { "atari_ss", nullptr },
          { "cpsf_sfc", nullptr },
          { "cpsf_md", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_pointer_device",
        "Pointer device",
        nullptr,
        "Choose the pointer device emulation mode.",
        nullptr,
        "input",
        {
          { "disabled", nullptr },
          { "mouse", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_mouse_port",
        "Mouse port",
        nullptr,
        "Select which port receives mouse input.",
        nullptr,
        "input",
        {
          { "off", nullptr },
          { "scc", nullptr },
          { "keyboard", nullptr },
          { nullptr, nullptr }
        },
        "off"
      },
      {
        "xm6_mouse_speed",
        "Mouse speed",
        nullptr,
        "Choose the mouse pointer speed.",
        nullptr,
        "input",
        {
          { "205", nullptr },
          { "256", nullptr },
          { "128", nullptr },
          { "384", nullptr },
          { "512", nullptr },
          { "64", nullptr },
          { "0", nullptr },
          { nullptr, nullptr }
        },
        "205"
      },
      {
        "xm6_mouse_swap",
        "Swap mouse buttons",
        nullptr,
        "Swap left and right mouse buttons.",
        nullptr,
        "input",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_legacy_dmac_cnt",
        "Legacy DMAC CNT behavior",
        nullptr,
        "Use the legacy CNT behavior for compatibility.",
        nullptr,
        "advanced",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      {
        "xm6_mpu_nowait",
        "No Wait Operation with MPU",
        nullptr,
        "Enable MPU no-wait operation for compatibility.",
        nullptr,
        "advanced",
        {
          { "disabled", nullptr },
          { "enabled", nullptr },
          { nullptr, nullptr }
        },
        "disabled"
      },
      { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, { { nullptr, nullptr } }, nullptr }
    };

    static retro_core_options_v2 options_v2 = {
      cats,
      defs
    };

    g_environ_cb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2, &options_v2);
    return;
  }

  static const retro_variable vars[] = {
    { "xm6_audio_engine",
      "Audio engine (legacy); XM6|PX68k|YMFM|X68Sound" },
    { nullptr, nullptr }
  };
  g_environ_cb(RETRO_ENVIRONMENT_SET_VARIABLES, const_cast<retro_variable *>(vars));
}

static unsigned map_retro_key_to_xm6(unsigned keycode)
{
  switch (keycode) {
    case RETROK_ESCAPE: return 0x01;
    case RETROK_1: return 0x02;
    case RETROK_2: return 0x03;
    case RETROK_3: return 0x04;
    case RETROK_4: return 0x05;
    case RETROK_5: return 0x06;
    case RETROK_6: return 0x07;
    case RETROK_7: return 0x08;
    case RETROK_8: return 0x09;
    case RETROK_9: return 0x0A;
    case RETROK_0: return 0x0B;
    case RETROK_MINUS: return 0x0C;
    case RETROK_CARET: return 0x0D;
    case RETROK_BACKSLASH: return 0x0E;
    case RETROK_BACKSPACE: return 0x0F;
    case RETROK_TAB: return 0x10;
    case RETROK_q: return 0x11;
    case RETROK_w: return 0x12;
    case RETROK_e: return 0x13;
    case RETROK_r: return 0x14;
    case RETROK_t: return 0x15;
    case RETROK_y: return 0x16;
    case RETROK_u: return 0x17;
    case RETROK_i: return 0x18;
    case RETROK_o: return 0x19;
    case RETROK_p: return 0x1A;
    case RETROK_AT: return 0x1B;
    case RETROK_LEFTBRACKET: return 0x1C;
    case RETROK_RETURN: return 0x1D;
    case RETROK_a: return 0x1E;
    case RETROK_s: return 0x1F;
    case RETROK_d: return 0x20;
    case RETROK_f: return 0x21;
    case RETROK_g: return 0x22;
    case RETROK_h: return 0x23;
    case RETROK_j: return 0x24;
    case RETROK_k: return 0x25;
    case RETROK_l: return 0x26;
    case RETROK_SEMICOLON: return 0x27;
    case RETROK_COLON:
    case RETROK_QUOTE: return 0x28;
    case RETROK_RIGHTBRACKET: return 0x29;
    case RETROK_z: return 0x2A;
    case RETROK_x: return 0x2B;
    case RETROK_c: return 0x2C;
    case RETROK_v: return 0x2D;
    case RETROK_b: return 0x2E;
    case RETROK_n: return 0x2F;
    case RETROK_m: return 0x30;
    case RETROK_COMMA: return 0x31;
    case RETROK_PERIOD: return 0x32;
    case RETROK_SLASH: return 0x33;
    case RETROK_UNDERSCORE: return 0x34;
    case RETROK_SPACE: return 0x35;
    case RETROK_HOME: return 0x36;
    case RETROK_DELETE: return 0x37;
    case RETROK_PAGEDOWN: return 0x38;
    case RETROK_PAGEUP: return 0x39;
    case RETROK_END:
    case RETROK_UNDO: return 0x3A;
    case RETROK_LEFT: return 0x3B;
    case RETROK_UP: return 0x3C;
    case RETROK_RIGHT: return 0x3D;
    case RETROK_DOWN: return 0x3E;
    case RETROK_NUMLOCK: return 0x3F;
    case RETROK_KP_DIVIDE: return 0x40;
    case RETROK_KP_MULTIPLY: return 0x41;
    case RETROK_KP_MINUS: return 0x42;
    case RETROK_KP7: return 0x43;
    case RETROK_KP8: return 0x44;
    case RETROK_KP9: return 0x45;
    case RETROK_KP_PLUS: return 0x46;
    case RETROK_KP4: return 0x47;
    case RETROK_KP5: return 0x48;
    case RETROK_KP6: return 0x49;
    case RETROK_KP1: return 0x4B;
    case RETROK_KP2: return 0x4C;
    case RETROK_KP3: return 0x4D;
    case RETROK_KP_ENTER: return 0x4E;
    case RETROK_KP0: return 0x4F;
    case RETROK_KP_PERIOD: return 0x51;
    case RETROK_CAPSLOCK: return 0x5D;
    case RETROK_INSERT: return 0x5E;
    case RETROK_PAUSE:
    case RETROK_BREAK: return 0x61;
    case RETROK_F1: return 0x63;
    case RETROK_F2: return 0x64;
    case RETROK_F3: return 0x65;
    case RETROK_F4: return 0x66;
    case RETROK_F5: return 0x67;
    case RETROK_F6: return 0x68;
    case RETROK_F7: return 0x69;
    case RETROK_F8: return 0x6A;
    case RETROK_F9: return 0x6B;
    case RETROK_F10: return 0x6C;
    case RETROK_LSHIFT:
    case RETROK_RSHIFT: return 0x70;
    case RETROK_LCTRL:
    case RETROK_RCTRL: return 0x71;
    case RETROK_LALT: return 0x72;
    case RETROK_RALT: return 0x73;
    default: return 0;
  }
}

static void RETRO_CALLCONV keyboard_event_cb(bool down, unsigned keycode,
                                             uint32_t, uint16_t)
{
  if (!g_xm6_handle || !g_xm6.input_key) {
    return;
  }

  const unsigned xm6_code = map_retro_key_to_xm6(keycode);
  if (xm6_code == 0) {
    return;
  }

  g_xm6.input_key(g_xm6_handle, xm6_code, down ? 1 : 0);
}

static void fill_av_info(struct retro_system_av_info *info)
{
  if (!info) {
    return;
  }

  std::memset(info, 0, sizeof(*info));
  info->timing.fps = g_current_fps;
  info->timing.sample_rate = static_cast<double>(g_sample_rate);
  info->geometry.base_width = g_frame_width;
  info->geometry.base_height = g_frame_height;
  info->geometry.max_width = 1024;
  info->geometry.max_height = 1024;
  info->geometry.aspect_ratio = static_cast<float>(k_default_aspect);
}

static double sanitize_refresh_hz(double hz)
{
  if (hz < 20.0 || hz > 240.0) {
    return k_default_fps;
  }
  return hz;
}

static float sanitize_aspect_ratio(float aspect)
{
  if (aspect < 0.5f || aspect > 3.0f) {
    return static_cast<float>(k_default_aspect);
  }
  return aspect;
}

static unsigned int vertical_scale_from_layout(const xm6_video_layout_t &layout,
                                               unsigned int frame_height)
{
  if (layout.valid && (layout.v_mul == 0 || layout.v_mul == 2)) {
    return 2;
  }

  // Fallback: en algunos modos 31kHz el core reporta mitad de alto visible.
  if (layout.valid && layout.lowres == 0 && frame_height > 0 && frame_height <= 300) {
    return 2;
  }

  return 1;
}

static void compute_video_scale_factors(const xm6_video_layout_t &layout,
                                        unsigned int frame_width,
                                        unsigned int frame_height,
                                        unsigned int *out_h_scale,
                                        unsigned int *out_v_scale)
{
  unsigned int h_scale = 1;
  unsigned int v_scale = 1;

  // Fast compositor path: keep core-provided geometry as-is,
  // matching px68k's direct line output style.
  if (g_render_mode == XM6CORE_RENDER_MODE_FAST) {
    if (out_h_scale) {
      *out_h_scale = 1;
    }
    if (out_v_scale) {
      *out_v_scale = 1;
    }
    return;
  }

  if (layout.valid) {
    if (layout.h_mul > 0) {
      h_scale = layout.h_mul;
    }
    v_scale = vertical_scale_from_layout(layout, frame_height);
  } else if (frame_width > 0 && frame_width <= 320) {
    h_scale = 2;
  }

  if (out_h_scale) {
    *out_h_scale = h_scale;
  }
  if (out_v_scale) {
    *out_v_scale = v_scale;
  }
}

static void reset_video_mode_log_state()
{
  g_video_mode_log = {};
  g_video_probe_frames_remaining = 0;
  g_video_probe_frame_index = 0;
}

static void log_video_mode_if_changed(unsigned int raw_w,
                                      unsigned int raw_h,
                                      const xm6_video_layout_t &layout,
                                      unsigned int h_scale,
                                      unsigned int v_scale,
                                      unsigned int out_w,
                                      unsigned int out_h)
{
  const unsigned int layout_w = layout.valid ? layout.width : 0;
  const unsigned int layout_h = layout.valid ? layout.height : 0;
  const unsigned int h_mul = layout.valid ? layout.h_mul : 0;
  const unsigned int v_mul = layout.valid ? layout.v_mul : 0;
  const int lowres = layout.valid ? layout.lowres : -1;

  if (g_video_mode_log.valid &&
      g_video_mode_log.raw_w == raw_w &&
      g_video_mode_log.raw_h == raw_h &&
      g_video_mode_log.layout_w == layout_w &&
      g_video_mode_log.layout_h == layout_h &&
      g_video_mode_log.h_mul == h_mul &&
      g_video_mode_log.v_mul == v_mul &&
      g_video_mode_log.lowres == lowres &&
      g_video_mode_log.h_scale == h_scale &&
      g_video_mode_log.v_scale == v_scale &&
      g_video_mode_log.out_w == out_w &&
      g_video_mode_log.out_h == out_h) {
    return;
  }

  g_video_mode_log.valid = true;
  g_video_mode_log.raw_w = raw_w;
  g_video_mode_log.raw_h = raw_h;
  g_video_mode_log.layout_w = layout_w;
  g_video_mode_log.layout_h = layout_h;
  g_video_mode_log.h_mul = h_mul;
  g_video_mode_log.v_mul = v_mul;
  g_video_mode_log.lowres = lowres;
  g_video_mode_log.h_scale = h_scale;
  g_video_mode_log.v_scale = v_scale;
  g_video_mode_log.out_w = out_w;
  g_video_mode_log.out_h = out_h;

  core_log(RETRO_LOG_INFO,
           "[xm6-libretro] Video mode raw=%ux%u layout=%ux%u hm=%u vm=%u low=%d scale=%ux%u out=%ux%u",
           raw_w,
           raw_h,
           layout_w,
           layout_h,
           h_mul,
           v_mul,
           lowres,
           h_scale,
           v_scale,
           out_w,
           out_h);


  g_video_probe_frames_remaining = k_video_probe_frames_after_mode_change;
  g_video_probe_frame_index = 0;
}

static xm6_video_layout_t query_video_layout()
{
  xm6_video_layout_t layout;
  if (!g_xm6_handle || !g_xm6.get_video_layout) {
    return layout;
  }

  if (g_xm6.get_video_layout(g_xm6_handle,
                             &layout.width,
                             &layout.height,
                             &layout.h_mul,
                             &layout.v_mul,
                             &layout.lowres) != XM6CORE_OK) {
    return layout;
  }

  if (layout.h_mul == 0) {
    layout.h_mul = 1;
  }
  layout.valid = true;
  return layout;
}

static void log_video_probe_frame(const void *video_pixels,
                                  unsigned int out_w,
                                  unsigned int out_h,
                                  unsigned int out_stride_pixels,
                                  unsigned int raw_w,
                                  unsigned int raw_h,
                                  const xm6_video_layout_t &layout,
                                  unsigned int h_scale,
                                  unsigned int v_scale)
{
  (void)video_pixels;
  (void)out_w;
  (void)out_h;
  (void)out_stride_pixels;
  (void)raw_w;
  (void)raw_h;
  (void)layout;
  (void)h_scale;
  (void)v_scale;

  g_video_probe_frames_remaining = 0;
  g_video_probe_frame_index = 0;
}

static float query_frame_aspect(unsigned frame_width,
                                unsigned frame_height,
                                const xm6_video_layout_t &layout)
{
  (void)frame_width;
  (void)frame_height;
  (void)layout;
  return static_cast<float>(k_default_aspect);
}

static bool build_scaled_video_frame(
  const xm6_video_frame_t &frame,
  const xm6_video_layout_t &layout,
  const void **out_pixels,
  unsigned *out_width,
  unsigned *out_height,
  unsigned *out_stride_pixels)
{
  if (!out_pixels || !out_width || !out_height || !out_stride_pixels) {
    return false;
  }
  if (!frame.pixels_argb32 || frame.width == 0 || frame.height == 0 || frame.stride_pixels == 0) {
    return false;
  }

  unsigned int h_factor = 1;
  unsigned int v_factor = 1;
  compute_video_scale_factors(layout, frame.width, frame.height, &h_factor, &v_factor);

  const bool use_native_double_height =
    (g_render_mode != XM6CORE_RENDER_MODE_FAST) && layout.valid && layout.lowres == 1 && layout.v_mul == 0;

  if (h_factor == 1 && v_factor == 1 && !use_native_double_height) {
    *out_pixels = frame.pixels_argb32;
    *out_width = frame.width;
    *out_height = frame.height;
    *out_stride_pixels = frame.stride_pixels;
    return true;
  }

  const unsigned src_w = frame.width;
  const unsigned src_h = frame.height;
  const unsigned dst_w = src_w * h_factor;
  const unsigned dst_h = use_native_double_height ? (src_h * 2) : (src_h * v_factor);
  if (dst_w == 0 || dst_h == 0 || dst_w > 4096 || dst_h > 4096) {
    return false;
  }

  g_video_buffer.resize(static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h));
  if (use_native_double_height) {
    for (unsigned y = 0; y < dst_h; ++y) {
      const unsigned int *src = reinterpret_cast<const unsigned int*>(frame.pixels_argb32) +
                                static_cast<size_t>(y) * static_cast<size_t>(frame.stride_pixels);
      unsigned int *dst = g_video_buffer.data() + static_cast<size_t>(y) * static_cast<size_t>(dst_w);
      if (h_factor == 1) {
        std::memcpy(dst, src, static_cast<size_t>(src_w) * sizeof(unsigned int));
      } else {
        for (unsigned x = 0; x < src_w; ++x) {
          const unsigned int pixel = src[x];
          for (unsigned hx = 0; hx < h_factor; ++hx) {
            dst[x * h_factor + hx] = pixel;
          }
        }
      }
    }
  } else {
    for (unsigned y = 0; y < src_h; ++y) {
      const unsigned int *src = reinterpret_cast<const unsigned int*>(frame.pixels_argb32) +
                                static_cast<size_t>(y) * static_cast<size_t>(frame.stride_pixels);
      for (unsigned vy = 0; vy < v_factor; ++vy) {
        unsigned int *dst = g_video_buffer.data() +
                            static_cast<size_t>(y * v_factor + vy) * static_cast<size_t>(dst_w);
        if (h_factor == 1) {
          std::memcpy(dst, src, static_cast<size_t>(src_w) * sizeof(unsigned int));
        } else {
          for (unsigned x = 0; x < src_w; ++x) {
            const unsigned int pixel = src[x];
            for (unsigned hx = 0; hx < h_factor; ++hx) {
              dst[x * h_factor + hx] = pixel;
            }
          }
        }
      }
    }
  }

  *out_pixels = g_video_buffer.data();
  *out_width = dst_w;
  *out_height = dst_h;
  *out_stride_pixels = dst_w;
  return true;
}

static void update_refresh_rate_from_core()
{
  if (!g_xm6_handle || !g_xm6.get_video_refresh_hz) {
    return;
  }

  double measured_hz = 0.0;
  if (g_xm6.get_video_refresh_hz(g_xm6_handle, &measured_hz) != XM6CORE_OK) {
    return;
  }

  measured_hz = sanitize_refresh_hz(measured_hz);
  if (std::fabs(measured_hz - g_current_fps) < 0.01) {
    return;
  }

  g_current_fps = measured_hz;
  g_audio_fraction = 0.0;

  if (g_environ_cb) {
    retro_system_av_info av_info = {};
    fill_av_info(&av_info);
    g_environ_cb(RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO, &av_info);
  }

  core_log(RETRO_LOG_INFO, "[xm6-libretro] Timing updated to %.2f Hz", g_current_fps);
}

static unsigned calc_audio_frames_for_run()
{
  const double fps = (g_current_fps > 1.0) ? g_current_fps : k_default_fps;
  const double samples = (static_cast<double>(g_sample_rate) / fps) + g_audio_fraction;
  unsigned frames = static_cast<unsigned>(samples);
  g_audio_fraction = samples - static_cast<double>(frames);

  if (frames == 0) {
    frames = 1;
  }
  if (frames > 2048) {
    frames = 2048;
  }
  return frames;
}

static void push_geometry_if_changed(unsigned width, unsigned height, float aspect)
{
  if (!g_environ_cb || width == 0 || height == 0) {
    return;
  }

  aspect = sanitize_aspect_ratio(aspect);
  if (width == g_frame_width && height == g_frame_height) {
    return;
  }

  core_log(RETRO_LOG_INFO,
           "[xm6-libretro] Resolution change: %ux%u -> %ux%u",
           g_frame_width,
           g_frame_height,
           width,
           height);

  g_frame_width = width;
  g_frame_height = height;
  g_frame_aspect = static_cast<float>(k_default_aspect);

  retro_game_geometry geom = {};
  geom.base_width = g_frame_width;
  geom.base_height = g_frame_height;
  geom.max_width = 1024;
  geom.max_height = 1024;
  geom.aspect_ratio = static_cast<float>(k_default_aspect);
  g_environ_cb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
}

static bool joy_pressed(unsigned port, unsigned id)
{
  if (!g_input_state_cb) {
    return false;
  }

  if (g_supports_input_bitmasks) {
    const unsigned mask = static_cast<unsigned>(
      g_input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));
    return (mask & (1u << id)) != 0;
  }

  return g_input_state_cb(port, RETRO_DEVICE_JOYPAD, 0, id) != 0;
}

static int16_t joy_analog_axis(unsigned port, unsigned index, unsigned id)
{
  if (!g_input_state_cb) {
    return 0;
  }

  return g_input_state_cb(port, RETRO_DEVICE_ANALOG, index, id);
}

static void build_joy_state(unsigned port,
                            unsigned int axes[4],
                            int buttons[8],
                            bool *out_select_pressed,
                            bool *out_start_pressed)
{
  const int16_t analog_x = joy_analog_axis(port,
                                           RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                           RETRO_DEVICE_ID_ANALOG_X);
  const int16_t analog_y = joy_analog_axis(port,
                                           RETRO_DEVICE_INDEX_ANALOG_LEFT,
                                           RETRO_DEVICE_ID_ANALOG_Y);
  const int16_t analog_threshold = 0x4000;

  const bool left  = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_LEFT)  || analog_x <= -analog_threshold;
  const bool right = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_RIGHT) || analog_x >=  analog_threshold;
  const bool up    = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_UP)    || analog_y <= -analog_threshold;
  const bool down  = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_DOWN)  || analog_y >=  analog_threshold;
  const unsigned int axis_neg = static_cast<unsigned int>(-2048);
  const unsigned int axis_pos = 2047u;

  axes[0] = left ? axis_neg : (right ? axis_pos : 0);
  axes[1] = up   ? axis_neg : (down  ? axis_pos : 0);
  axes[2] = 0;
  axes[3] = 0;

  const bool select_pressed = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_SELECT);
  const bool start_pressed = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_START);

  const int joy_type = (port < 2) ? g_joy_type[port] : 0;
  switch (joy_type) {
    case 2: // atari_ss
      buttons[0] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
      buttons[1] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
      buttons[2] = start_pressed ? 1 : 0;
      buttons[3] = select_pressed ? 1 : 0;
      break;
    case 7: // cpsf_sfc
      // Match px68k/libretro trigger layout.
      buttons[0] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0;
      buttons[1] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0;
      buttons[2] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
      buttons[3] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
      buttons[4] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0;
      buttons[5] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0;
      break;
    case 8: // cpsf_md
      buttons[0] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
      buttons[1] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
      buttons[2] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0;
      buttons[3] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0;
      buttons[4] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0;
      buttons[5] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0;
      buttons[7] = select_pressed ? 1 : 0; // MODE
      break;
    default:
      buttons[0] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
      buttons[1] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
      buttons[2] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0;
      buttons[3] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0;
      buttons[4] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0;
      buttons[5] = joy_pressed(port, RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0;
      break;
  }

  buttons[6] = start_pressed ? 1 : 0;
  if (joy_type != 8) {
    buttons[7] = select_pressed ? 1 : 0;
  }

  if (out_select_pressed) {
    *out_select_pressed = select_pressed;
  }
  if (out_start_pressed) {
    *out_start_pressed = start_pressed;
  }
}

static void poll_and_push_input()
{
  if (!g_xm6_handle || !g_xm6.input_joy || !g_input_poll_cb) {
    return;
  }

  g_input_poll_cb();

  unsigned int axes_p1[4] = {};
  unsigned int axes_p2[4] = {};
  int buttons_p1[8] = {};
  int buttons_p2[8] = {};
  bool select_pressed_p1 = false;
  bool start_pressed_p1 = false;

  build_joy_state(0, axes_p1, buttons_p1, &select_pressed_p1, &start_pressed_p1);
  build_joy_state(1, axes_p2, buttons_p2, NULL, NULL);

  g_xm6.input_joy(g_xm6_handle, 0, axes_p1, buttons_p1);
  g_xm6.input_joy(g_xm6_handle, 1, axes_p2, buttons_p2);

  if (g_xm6.input_key) {
    unsigned start_key = 0;
    unsigned select_key = 0;
    switch (g_pad_start_select_mode) {
      case START_SELECT_F_KEYS:
        start_key = 0x63;   // F1
        select_key = 0x64;  // F2
        break;
      case START_SELECT_OPT_KEYS:
        start_key = 0x73;   // OPT1
        select_key = 0x72;  // OPT2
        break;
      case START_SELECT_XF_KEYS:
        start_key = 0x55;   // XF1
        select_key = 0x57;  // XF2
        break;
      default:
        break;
    }

    const bool prev_start_pressed = g_prev_start;
    const bool prev_select_pressed = g_prev_select;

    if (g_prev_start_keycode != start_key) {
      if (g_prev_start_keycode && prev_start_pressed) {
        g_xm6.input_key(g_xm6_handle, g_prev_start_keycode, 0);
      }
      if (start_key && start_pressed_p1) {
        g_xm6.input_key(g_xm6_handle, start_key, 1);
      }
      g_prev_start_keycode = start_key;
      g_prev_start = start_pressed_p1;
    } else if (start_key && start_pressed_p1 != prev_start_pressed) {
      g_xm6.input_key(g_xm6_handle, start_key, start_pressed_p1 ? 1 : 0);
      g_prev_start = start_pressed_p1;
    } else {
      g_prev_start = start_pressed_p1;
    }

    if (g_prev_select_keycode != select_key) {
      if (g_prev_select_keycode && prev_select_pressed) {
        g_xm6.input_key(g_xm6_handle, g_prev_select_keycode, 0);
      }
      if (select_key && select_pressed_p1) {
        g_xm6.input_key(g_xm6_handle, select_key, 1);
      }
      g_prev_select_keycode = select_key;
      g_prev_select = select_pressed_p1;
    } else if (select_key && select_pressed_p1 != prev_select_pressed) {
      g_xm6.input_key(g_xm6_handle, select_key, select_pressed_p1 ? 1 : 0);
      g_prev_select = select_pressed_p1;
    } else {
      g_prev_select = select_pressed_p1;
    }
  } else {
    g_prev_start = start_pressed_p1;
    g_prev_select = select_pressed_p1;
  }

  if (g_xm6.input_key) {
    const bool midi_hotkey = joy_pressed(0, RETRO_DEVICE_ID_JOYPAD_R2);
    if (midi_hotkey != g_prev_midi_hotkey) {
      // ScrollLock/登録: useful for games that require MIDI enable on boot.
      g_xm6.input_key(g_xm6_handle, 0x53, midi_hotkey ? 1 : 0);
      g_prev_midi_hotkey = midi_hotkey;
    }
  }

  if (g_pointer_device_mode == POINTER_DEVICE_MOUSE &&
      g_mouse_port != 0 &&
      g_xm6.input_mouse &&
      g_input_state_cb) {
    const int dx = static_cast<int>(
      g_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_X));
    const int dy = static_cast<int>(
      g_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_Y));
    const bool left_button =
      g_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_LEFT) != 0;
    const bool right_button =
      g_input_state_cb(0, RETRO_DEVICE_MOUSE, 0, RETRO_DEVICE_ID_MOUSE_RIGHT) != 0;

    g_mouse_x += dx;
    g_mouse_y += dy;
    g_mouse_left = left_button;
    g_mouse_right = right_button;
    g_xm6.input_mouse(g_xm6_handle,
                      g_mouse_x,
                      g_mouse_y,
                      g_mouse_left ? 1 : 0,
                      g_mouse_right ? 1 : 0);
  }
}

static bool disk_set_eject_state(bool ejected)
{
  if (!g_xm6_handle) {
    return false;
  }

  if (ejected) {
    const int target_drive = get_disk_target_drive(g_disk_index);
    g_xm6.eject_fdd(g_xm6_handle, target_drive, 1);
    if (target_drive >= 0 && target_drive < 2) {
      g_inserted_disk_index[target_drive] = -1;
    }
    g_disk_ejected = true;
    return true;
  }

  if (g_disk_paths.empty() || g_disk_index >= g_disk_paths.size()) {
    return false;
  }

  if (!mount_current_disk()) {
    return false;
  }
  g_disk_ejected = false;
  return true;
}

static bool disk_get_eject_state()
{
  return g_disk_ejected;
}

static unsigned disk_get_image_index()
{
  return g_disk_index;
}

static bool disk_set_image_index(unsigned index)
{
  if (index >= g_disk_paths.size()) {
    return false;
  }
  g_disk_index = index;
  if (!g_disk_ejected && g_xm6_handle) {
    return mount_current_disk();
  }
  return true;
}

static unsigned disk_get_num_images()
{
  return static_cast<unsigned>(g_disk_paths.size());
}

static bool disk_replace_image_index(unsigned index, const struct retro_game_info *info)
{
  if (index >= g_disk_paths.size() && !(index == 0 && g_disk_paths.empty())) {
    return false;
  }

  if (!info || !info->path || !*info->path) {
    if (index < g_disk_paths.size()) {
      g_disk_paths[index].clear();
      if (index < g_disk_labels.size()) {
        g_disk_labels[index].clear();
      }
      if (index < g_disk_target_drives.size()) {
        g_disk_target_drives[index] = -1;
      }
    }
    return true;
  }

  if (g_disk_paths.empty()) {
    g_disk_paths.resize(1);
    g_disk_labels.resize(1);
    g_disk_target_drives.resize(1, -1);
  }
  g_disk_paths[index] = info->path;
  g_disk_labels[index] = path_basename_no_ext(g_disk_paths[index]);
  if (index >= g_disk_target_drives.size()) {
    g_disk_target_drives.resize(index + 1, -1);
  }
  if (!g_disk_ejected && g_xm6_handle) {
    g_disk_index = index;
    return mount_current_disk();
  }
  return true;
}

static bool disk_add_image_index()
{
  g_disk_paths.push_back(std::string());
  g_disk_labels.push_back(std::string());
  g_disk_target_drives.push_back(-1);
  return true;
}

static bool disk_set_initial_image(unsigned index, const char *)
{
  if (index >= g_disk_paths.size()) {
    return false;
  }
  g_disk_index = index;
  return true;
}

static bool disk_get_image_path(unsigned index, char *s, size_t len)
{
  if (!s || len == 0 || index >= g_disk_paths.size()) {
    return false;
  }
  std::snprintf(s, len, "%s", g_disk_paths[index].c_str());
  return true;
}

static bool disk_get_image_label(unsigned index, char *s, size_t len)
{
  if (!s || len == 0 || index >= g_disk_labels.size()) {
    return false;
  }
  const std::string &label = g_disk_labels[index];
  std::snprintf(s, len, "%s", label.c_str());
  return true;
}

static void register_disk_interface()
{
  if (!g_environ_cb) {
    return;
  }

  retro_disk_control_ext_callback disk_ext_cb = {};
  disk_ext_cb.set_eject_state = disk_set_eject_state;
  disk_ext_cb.get_eject_state = disk_get_eject_state;
  disk_ext_cb.get_image_index = disk_get_image_index;
  disk_ext_cb.set_image_index = disk_set_image_index;
  disk_ext_cb.get_num_images = disk_get_num_images;
  disk_ext_cb.replace_image_index = disk_replace_image_index;
  disk_ext_cb.add_image_index = disk_add_image_index;
  disk_ext_cb.set_initial_image = disk_set_initial_image;
  disk_ext_cb.get_image_path = disk_get_image_path;
  disk_ext_cb.get_image_label = disk_get_image_label;
  g_environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE, &disk_ext_cb);

  retro_disk_control_callback disk_cb = {};
  disk_cb.set_eject_state = disk_set_eject_state;
  disk_cb.get_eject_state = disk_get_eject_state;
  disk_cb.get_image_index = disk_get_image_index;
  disk_cb.set_image_index = disk_set_image_index;
  disk_cb.get_num_images = disk_get_num_images;
  disk_cb.replace_image_index = disk_replace_image_index;
  disk_cb.add_image_index = disk_add_image_index;
  g_environ_cb(RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE, &disk_cb);
}

} // namespace

extern "C" {

void retro_set_environment(retro_environment_t cb)
{
  g_environ_cb = cb;

  if (!g_environ_cb) {
    return;
  }

  retro_log_callback logging = {};
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging)) {
    g_log_cb = logging.log;
  }

  bool no_game = false;
  g_environ_cb(RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME, &no_game);

  bool input_bitmasks = false;
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_INPUT_BITMASKS, &input_bitmasks)) {
    g_supports_input_bitmasks = input_bitmasks;
  }

  init_midi_interface();

  retro_keyboard_callback key_cb = {};
  key_cb.callback = keyboard_event_cb;
  g_environ_cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &key_cb);

  register_core_options();
  apply_core_option_values();
}

void retro_set_video_refresh(retro_video_refresh_t cb) { g_video_cb = cb; }
void retro_set_audio_sample(retro_audio_sample_t cb) { g_audio_cb = cb; }
void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb) { g_audio_batch_cb = cb; }
void retro_set_input_poll(retro_input_poll_t cb) { g_input_poll_cb = cb; }
void retro_set_input_state(retro_input_state_t cb) { g_input_state_cb = cb; }

void retro_init(void)
{
  g_frame_width = k_default_width;
  g_frame_height = k_default_height;
  g_frame_aspect = static_cast<float>(k_default_aspect);
  g_current_fps = k_default_fps;
  g_audio_fraction = 0.0;
  g_sample_rate = k_default_sample_rate;
  g_game_loaded = false;
  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_index = 0;
  g_disk_ejected = false;
  g_content_is_hdd = false;
  g_hdd_is_scsi = false;
  g_hdd_slot = 0;
  g_hdd_target_auto = true;
  g_hdd_boot_reset_countdown = 0;
  g_prev_start = false;
  g_prev_select = false;
  g_prev_start_keycode = 0;
  g_prev_select_keycode = 0;
  g_pad_start_select_mode = START_SELECT_XF_KEYS;
  g_video_not_ready_count = 0;
  g_system_clock = 0;
  g_ram_size = 5;
  g_fast_floppy = false;
  g_alt_raster_enabled = true;
  g_render_bg0_enabled = true;
  g_transparency_enabled = true;
  g_fm_volume = 50;
  g_adpcm_volume = 50;
  g_hq_adpcm_enabled = false;
  g_reverb_level = 0;
  g_eq_sub_bass_level = 50;
  g_eq_bass_level = 50;
  g_eq_mid_level = 50;
  g_eq_presence_level = 50;
  g_eq_treble_level = 50;
  g_eq_air_level = 50;
  g_surround_enabled = false;
  g_audio_engine = XM6CORE_AUDIO_ENGINE_XM6;
  g_legacy_dmac_cnt = false;
  g_midi_output_enabled = false;
  g_midi_output_type = MIDI_OUTPUT_GM;
  g_midi_reset_pending = false;
  g_pointer_device_mode = POINTER_DEVICE_DISABLED;
  g_mouse_port = 0;
  g_mouse_speed = 205;
  g_mouse_swap = false;
  g_mouse_x = 0;
  g_mouse_y = 0;
  g_mouse_left = false;
  g_mouse_right = false;
  g_mpu_nowait = false;
  g_savestate_guard_countdown = 0;
  reset_video_mode_log_state();
  g_audio_buffer.clear();
  g_video_buffer.clear();

  init_midi_interface();

  load_xm6_api();
}

void retro_deinit(void)
{
  g_midi_reset_pending = false;
  destroy_xm6_handle();
  unload_xm6_api();
}

unsigned retro_api_version(void)
{
  return RETRO_API_VERSION;
}

void retro_get_system_info(struct retro_system_info *info)
{
  if (!info) {
    return;
  }

  std::memset(info, 0, sizeof(*info));
  info->library_name = "XM6";
  info->library_version = "2.06 Libretro";
  info->need_fullpath = true;
  info->block_extract = false;
  info->valid_extensions = "dim|xdf|d88|88d|hdm|dup|2hd|img|hdf|hds|m3u";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  fill_av_info(info);
}

void retro_set_controller_port_device(unsigned, unsigned) {}

void retro_reset(void)
{
  if (g_xm6_handle && g_xm6.reset) {
    sanitize_runtime_for_reset();

    if (g_content_is_hdd && g_xm6.set_power) {
      hdd_manual_reset_cold_style();
    } else {
      g_xm6.reset(g_xm6_handle);
    }

    apply_joy_type_options();

    midi_queue_reset_if_enabled();
    arm_savestate_guard_frames(
      g_content_is_hdd ? "manual reset (HDD full cold-style)" : "manual reset",
      g_content_is_hdd ? k_savestate_guard_frames_hdd_load : k_savestate_guard_frames_default);
  }
}

bool retro_load_game(const struct retro_game_info *info)
{
  if (!info || !info->path || !*info->path) {
    return false;
  }
  if (!load_xm6_api()) {
    return false;
  }

  apply_core_option_values();

  if (!set_system_directory_from_frontend()) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] Could not set system directory");
  }
  log_system_bios_probe();

  // Recreate the VM per content load so HDD boot always starts from a clean
  // device topology, matching the standalone frontend more closely.
  destroy_xm6_handle();

  if (!ensure_xm6_handle()) {
    // If VM init failed, this probe makes missing BIOS/path issues explicit.
    log_system_bios_probe();
    return false;
  }
  apply_runtime_core_options();
  apply_joy_type_options();
  if (g_midi_output_enabled && !g_supports_midi_interface) {
    core_log(RETRO_LOG_WARN,
             "[xm6-libretro] MIDI output enabled but frontend MIDI interface is unavailable");
  }
  reset_mouse_state();
  g_content_is_hdd = is_hdd_content_path(info->path);

  if (!g_content_is_hdd) {
    core_log(RETRO_LOG_INFO,
             "[xm6-libretro] Powering off before floppy mount for cold boot");
    g_xm6.set_power(g_xm6_handle, 0);
    g_video_not_ready_count = 0;
  }

  sync_frontend_pixel_format_for_render_mode();
  if (g_frontend_pixel_format != RETRO_PIXEL_FORMAT_XRGB8888 &&
      g_frontend_pixel_format != RETRO_PIXEL_FORMAT_RGB565) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Failed to set a supported pixel format");
    return false;
  }

  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_target_drives.clear();
  g_disk_index = 0;
  g_inserted_disk_index[0] = -1;
  g_inserted_disk_index[1] = -1;

  if (path_has_extension(info->path, ".m3u")) {
    if (!parse_m3u_playlist(info->path, &g_disk_paths, &g_disk_labels, &g_disk_target_drives)) {
      core_log(RETRO_LOG_ERROR, "[xm6-libretro] Failed to parse M3U: %s", info->path);
      return false;
    }
    core_log(RETRO_LOG_INFO, "[xm6-libretro] Parsed M3U with %u entries",
             static_cast<unsigned>(g_disk_paths.size()));
  } else {
    g_disk_paths.push_back(info->path);
  }
  build_disk_labels();

  if (!(g_content_is_hdd ? mount_current_disk() : mount_initial_playlist_disks())) {
    return false;
  }
  arm_savestate_guard_frames(
    g_content_is_hdd ? "content load (HDD)" : "content load (floppy)",
    g_content_is_hdd ? k_savestate_guard_frames_hdd_load : k_savestate_guard_frames_floppy_load);
  g_disk_ejected = false;
  if (!g_content_is_hdd) {
    register_disk_interface();
  }

  if (g_content_is_hdd) {
    // HDF boot is more reliable with a full power cycle, not just a soft reset.
    g_xm6.set_power(g_xm6_handle, 0);
    g_xm6.set_power(g_xm6_handle, 1);
    apply_joy_type_options();
    begin_hdd_boot_warmup("HDD");
  } else {
    g_xm6.set_power(g_xm6_handle, 1);
    g_video_not_ready_count = 0;
    core_log(RETRO_LOG_INFO, "[xm6-libretro] Performing floppy post-mount reset");
    g_xm6.reset(g_xm6_handle);
    apply_joy_type_options();
  }

  midi_queue_reset_if_enabled();

  log_memory_exposure_status();
  g_game_loaded = true;
  return true;
}

bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
{
  return false;
}

void retro_unload_game(void)
{
  if (g_xm6_handle && g_xm6.eject_fdd && !g_content_is_hdd) {
    g_xm6.eject_fdd(g_xm6_handle, g_disk_drive, 1);
  }
  g_midi_reset_pending = false;
  destroy_xm6_handle();
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

void retro_run(void)
{
  if (!g_xm6_handle || !g_game_loaded) {
    if (g_video_cb) {
      const unsigned pitch = current_video_pitch_bytes(g_frame_width);
      g_video_cb(nullptr, g_frame_width, g_frame_height, pitch);
    }
    return;
  }

  if (!g_supports_midi_interface) {
    init_midi_interface();
  }

  const uint64_t frame_start_usec = now_usec();

  if (g_environ_cb) {
    bool updated = false;
    if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
      const int old_drive = g_disk_drive;
      const bool old_hdd_auto = g_hdd_target_auto;
      const bool old_hdd_is_scsi = g_hdd_is_scsi;
      const int old_hdd_slot = g_hdd_slot;
      const int old_joy0 = g_joy_type[0];
      const int old_joy1 = g_joy_type[1];
      const int old_system_clock = g_system_clock;
	      const int old_ram_size = g_ram_size;
	      const bool old_fast_floppy = g_fast_floppy;
	      const int old_render_mode = g_render_mode;
	      const bool old_alt_raster_enabled = g_alt_raster_enabled;
	      const bool old_render_bg0_enabled = g_render_bg0_enabled;
	      const bool old_transparency_enabled = g_transparency_enabled;
      const int old_fm_volume = g_fm_volume;
      const int old_adpcm_volume = g_adpcm_volume;
	      const bool old_hq_adpcm_enabled = g_hq_adpcm_enabled;
	      const int old_reverb_level = g_reverb_level;
	      const int old_eq_sub_bass_level = g_eq_sub_bass_level;
	      const int old_eq_bass_level = g_eq_bass_level;
	      const int old_eq_mid_level = g_eq_mid_level;
	      const int old_eq_presence_level = g_eq_presence_level;
	      const int old_eq_treble_level = g_eq_treble_level;
	      const bool old_surround_enabled = g_surround_enabled;
	      const int old_audio_engine = g_audio_engine;
	      const bool old_legacy_dmac_cnt = g_legacy_dmac_cnt;
      const bool old_midi_output_enabled = g_midi_output_enabled;
      const int old_midi_output_type = g_midi_output_type;
      const int old_pointer_device_mode = g_pointer_device_mode;
      const int old_mouse_port = g_mouse_port;
      const int old_mouse_speed = g_mouse_speed;
      const bool old_mouse_swap = g_mouse_swap;
      apply_core_option_values();
      if (old_system_clock != g_system_clock || old_ram_size != g_ram_size) {
        apply_runtime_core_options();
        if (old_system_clock != g_system_clock && old_ram_size != g_ram_size) {
          core_log(RETRO_LOG_INFO,
                   "[xm6-libretro] Clock changed to %s. RAM changed to %dMB and will apply after manual reset.",
                   (g_system_clock == 1) ? "12mhz" :
                   (g_system_clock == 3) ? "16mhz" :
                   (g_system_clock == 5) ? "22mhz" : "10mhz",
                   (g_ram_size + 1) * 2);
        } else if (old_system_clock != g_system_clock) {
          core_log(RETRO_LOG_INFO,
                   "[xm6-libretro] System clock changed to %s.",
                   (g_system_clock == 1) ? "12mhz" :
                   (g_system_clock == 3) ? "16mhz" :
                   (g_system_clock == 5) ? "22mhz" : "10mhz");
        } else {
          core_log(RETRO_LOG_INFO,
                   "[xm6-libretro] RAM size changed to %dMB and will apply after manual reset.",
                   (g_ram_size + 1) * 2);
        }
      }
      if (old_fast_floppy != g_fast_floppy &&
          !(old_system_clock != g_system_clock || old_ram_size != g_ram_size)) {
        apply_runtime_core_options();
        core_log(RETRO_LOG_INFO, "[xm6-libretro] Fast floppy %s",
                 g_fast_floppy ? "enabled" : "disabled");
      }
	      if (old_render_mode != g_render_mode) {
	        apply_runtime_core_options();
	        core_log(RETRO_LOG_INFO, "[xm6-libretro] Video compositor changed to %s",
	                 (g_render_mode == XM6CORE_RENDER_MODE_FAST) ? "fast" : "original");
	      }
	      if (old_render_mode != g_render_mode) {
	        sync_frontend_pixel_format_for_render_mode();
	        g_video_probe_frames_remaining = k_video_probe_frames_after_mode_change;
	        g_video_probe_frame_index = 0;
	      }
	      if (old_alt_raster_enabled != g_alt_raster_enabled) {
	        apply_runtime_core_options();
	        core_log(RETRO_LOG_INFO, "[xm6-libretro] Alternative raster timing %s",
	                 g_alt_raster_enabled ? "enabled" : "disabled");
	      }
	      if (old_render_bg0_enabled != g_render_bg0_enabled) {
	        apply_runtime_core_options();
	        core_log(RETRO_LOG_INFO, "[xm6-libretro] BG tile transparency rule %s",
	                 g_render_bg0_enabled ? "modern" : "legacy (XM62022Nuevo)");
	      }
	      if (old_transparency_enabled != g_transparency_enabled) {
	        apply_runtime_core_options();
	        core_log(RETRO_LOG_INFO, "[xm6-libretro] Transparency (TR/half-fill) %s",
	                 g_transparency_enabled ? "enabled" : "disabled");
	      }
      if (old_audio_engine != g_audio_engine) {
        apply_runtime_core_options();
        g_audio_fraction = 0.0;
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] Audio engine changed to %s",
                 audio_engine_label(g_audio_engine));
      }
      if (old_reverb_level != g_reverb_level) {
        apply_runtime_core_options();
        g_audio_fraction = 0.0;
        if (g_xm6.audio_configure && g_xm6_handle && g_sample_rate > 0) {
          if (g_xm6.audio_configure(g_xm6_handle, g_sample_rate) == XM6CORE_OK) {
            core_log(RETRO_LOG_INFO,
                     "[xm6-libretro] Reverb audio state refreshed at %u Hz",
                     g_sample_rate);
          } else {
            core_log(RETRO_LOG_WARN,
                     "[xm6-libretro] Reverb refresh failed at %u Hz",
                     g_sample_rate);
          }
        }
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] Reverb level changed to %d",
                 g_reverb_level);
      }
      if (old_eq_sub_bass_level != g_eq_sub_bass_level ||
          old_eq_bass_level != g_eq_bass_level ||
          old_eq_mid_level != g_eq_mid_level ||
          old_eq_presence_level != g_eq_presence_level ||
          old_eq_treble_level != g_eq_treble_level) {
        apply_runtime_core_options();
      }
      if (old_surround_enabled != g_surround_enabled) {
        apply_runtime_core_options();
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] Surround %s",
                 g_surround_enabled ? "enabled" : "disabled");
      }
      if (old_legacy_dmac_cnt != g_legacy_dmac_cnt) {
        apply_runtime_core_options();
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] Legacy DMAC CNT behavior %s",
                 g_legacy_dmac_cnt ? "enabled" : "disabled");
      }
      if (old_fm_volume != g_fm_volume ||
          old_adpcm_volume != g_adpcm_volume) {
        apply_runtime_core_options();
      }
      if (old_hq_adpcm_enabled != g_hq_adpcm_enabled) {
        apply_runtime_core_options();
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] HQ ADPCM %s",
                 g_hq_adpcm_enabled ? "enabled" : "disabled");
      }
      if (old_midi_output_enabled != g_midi_output_enabled ||
          old_midi_output_type != g_midi_output_type) {
        apply_runtime_core_options();
        midi_queue_reset_if_enabled();
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] MIDI output %s (type=%s)",
                 g_midi_output_enabled ? "enabled" : "disabled",
                 (g_midi_output_type == MIDI_OUTPUT_LA) ? "LA" :
                 (g_midi_output_type == MIDI_OUTPUT_GS) ? "GS" :
                 (g_midi_output_type == MIDI_OUTPUT_XG) ? "XG" : "GM");
      }
      if (old_pointer_device_mode != g_pointer_device_mode ||
          old_mouse_port != g_mouse_port ||
          old_mouse_speed != g_mouse_speed ||
          old_mouse_swap != g_mouse_swap) {
        apply_runtime_core_options();
        reset_mouse_state();
        core_log(RETRO_LOG_INFO,
                 "[xm6-libretro] Mouse config changed (device=%s port=%d speed=%d swap=%s)",
                 (g_pointer_device_mode == POINTER_DEVICE_MOUSE) ? "mouse" : "disabled",
                 g_mouse_port,
                 g_mouse_speed,
                 g_mouse_swap ? "enabled" : "disabled");
      }
      if (old_joy0 != g_joy_type[0] || old_joy1 != g_joy_type[1]) {
        apply_joy_type_options();
      }
      if (((old_drive != g_disk_drive) && !g_content_is_hdd && !g_disk_ejected) ||
          ((old_hdd_auto != g_hdd_target_auto ||
            old_hdd_is_scsi != g_hdd_is_scsi ||
            old_hdd_slot != g_hdd_slot) && g_content_is_hdd)) {
        mount_current_disk();
        arm_savestate_guard_frames(
          g_content_is_hdd ? "disk remount (HDD)" : "disk remount (floppy)",
          g_content_is_hdd ? k_savestate_guard_frames_hdd_load : k_savestate_guard_frames_floppy_load);
        if (g_content_is_hdd) {
          begin_hdd_boot_warmup("HDD");
          midi_queue_reset_if_enabled();
        } else {
          g_video_not_ready_count = 0;
          core_log(RETRO_LOG_INFO, "[xm6-libretro] Performing floppy post-mount reset");
          g_xm6.reset(g_xm6_handle);
          midi_queue_reset_if_enabled();
        }
      }
    }
  }

  if (g_savestate_guard_countdown > 0) {
    --g_savestate_guard_countdown;
  }

  if (g_midi_reset_pending && midi_output_ready()) {
    midi_send_reset_sequence();
    g_midi_reset_pending = false;
  }

  pump_frontend_midi_input();

  poll_and_push_input();

  if (run_pending_hdd_boot_warmup_step()) {
    update_refresh_rate_from_core();

    if (g_video_cb) {
      const unsigned pitch = g_frame_width * current_video_bpp_bytes();
      g_video_cb(nullptr, g_frame_width, g_frame_height, pitch);
    }

    if (g_audio_batch_cb) {
      const unsigned want_frames = calc_audio_frames_for_run();
      g_audio_buffer.resize(want_frames * 2);
      if (want_frames > 0) {
        std::memset(g_audio_buffer.data(), 0, want_frames * 2 * sizeof(int16_t));
      }
      g_audio_batch_cb(g_audio_buffer.data(), want_frames);
    } else if (g_audio_cb) {
      g_audio_cb(0, 0);
    }
    return;
  }

  if (g_use_exec_to_frame && g_xm6.exec_to_frame) {
    g_xm6.exec_to_frame(g_xm6_handle);
  } else {
    g_xm6.exec(g_xm6_handle, 36000);
  }

  update_refresh_rate_from_core();

  xm6_video_frame_t frame = {};
  int vrc = g_xm6.video_poll(g_xm6_handle, &frame);
  if (vrc != XM6CORE_OK && g_use_exec_to_frame) {
    // Fallback for backends where exec_to_frame does not expose a ready frame reliably.
    g_xm6.exec(g_xm6_handle, 36000);
    vrc = g_xm6.video_poll(g_xm6_handle, &frame);
  }

  pump_core_midi_output();

  if (vrc == XM6CORE_OK && frame.pixels_argb32 && frame.width > 0 && frame.height > 0) {
    g_video_not_ready_count = 0;
    const xm6_video_layout_t layout = query_video_layout();
    const float frame_aspect = query_frame_aspect(frame.width, frame.height, layout);
    unsigned int h_scale = 1;
    unsigned int v_scale = 1;
    compute_video_scale_factors(layout, frame.width, frame.height, &h_scale, &v_scale);

    const void *video_pixels = frame.pixels_argb32;
    unsigned video_width = frame.width;
    unsigned video_height = frame.height;
    unsigned video_stride_pixels = frame.stride_pixels;
    if (!build_scaled_video_frame(frame,
                                  layout,
                                  &video_pixels,
                                  &video_width,
                                  &video_height,
                                  &video_stride_pixels)) {
      video_pixels = frame.pixels_argb32;
      video_width = frame.width;
      video_height = frame.height;
      video_stride_pixels = frame.stride_pixels;
    }

    log_video_mode_if_changed(frame.width,
                              frame.height,
                              layout,
                              h_scale,
                              v_scale,
                              video_width,
                              video_height);

    log_video_probe_frame(video_pixels,
                          video_width,
                          video_height,
                          video_stride_pixels,
                          frame.width,
                          frame.height,
                          layout,
                          h_scale,
                          v_scale);

	    push_geometry_if_changed(video_width, video_height, frame_aspect);
	    if (g_video_cb) {
        const void *cb_pixels = video_pixels;
        unsigned cb_pitch = video_stride_pixels * sizeof(uint32_t);

        if (g_frontend_pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
          const uint32_t *src = static_cast<const uint32_t *>(video_pixels);
          const unsigned out_stride_pixels = (video_width <= 800) ? 800u : video_width;
          g_video_buffer_rgb565.resize(static_cast<size_t>(out_stride_pixels) * static_cast<size_t>(video_height));
          for (unsigned y = 0; y < video_height; ++y) {
            const uint32_t *src_row = src + static_cast<size_t>(y) * static_cast<size_t>(video_stride_pixels);
            uint16_t *dst_row = g_video_buffer_rgb565.data() + static_cast<size_t>(y) * static_cast<size_t>(out_stride_pixels);
            for (unsigned x = 0; x < video_width; ++x) {
              dst_row[x] = px68k_pack_rgb565i(src_row[x]);
            }
          }
          cb_pixels = g_video_buffer_rgb565.data();
          cb_pitch = out_stride_pixels * sizeof(uint16_t);
        }

	      g_video_cb(cb_pixels, video_width, video_height, cb_pitch);
	    }
    g_xm6.video_consume(g_xm6_handle);
  } else if (g_video_cb) {
    ++g_video_not_ready_count;
    const unsigned pitch = current_video_pitch_bytes(g_frame_width);
    g_video_cb(nullptr, g_frame_width, g_frame_height, pitch);
  }

  if (g_audio_batch_cb) {
    const unsigned want_frames = calc_audio_frames_for_run();
    g_audio_buffer.resize(want_frames * 2);
    unsigned out_frames = 0;
    if (g_xm6.audio_mix(g_xm6_handle, g_audio_buffer.data(), want_frames, &out_frames) == XM6CORE_OK &&
        out_frames > 0) {
      g_audio_batch_cb(g_audio_buffer.data(), out_frames);
    } else if (want_frames > 0) {
      std::memset(g_audio_buffer.data(), 0, want_frames * 2 * sizeof(int16_t));
      g_audio_batch_cb(g_audio_buffer.data(), want_frames);
    }
  } else if (g_audio_cb) {
    g_audio_cb(0, 0);
  }

  if (g_mpu_nowait && g_xm6.exec_mpu_nowait_step) {
    const double fps = (g_current_fps > 1.0) ? g_current_fps : k_default_fps;
    const uint64_t budget_us = static_cast<uint64_t>(1000000.0 / fps);
    const uint64_t safety_us = 150;
    const uint64_t stop_usec =
      frame_start_usec + ((budget_us > safety_us) ? (budget_us - safety_us) : 0u);

    for (unsigned int i = 0; i < 512; ++i) {
      if (now_usec() >= stop_usec) {
        break;
      }
      if (g_xm6.exec_mpu_nowait_step(g_xm6_handle) != XM6CORE_OK) {
        break;
      }
    }
  }
}

size_t retro_serialize_size(void)
{
  if (!g_xm6_handle || !g_xm6.state_size) {
    return 0;
  }
  unsigned int size = 0;
  if (g_xm6.state_size(g_xm6_handle, &size) != XM6CORE_OK) {
    return 0;
  }
  return static_cast<size_t>(size);
}

bool retro_serialize(void *data, size_t size)
{
  if (!g_xm6_handle || !data) {
    return false;
  }
  if (g_savestate_guard_countdown > 0) {
    core_log(RETRO_LOG_WARN,
             "[xm6-libretro] Savestate denied during boot/disk stabilization (%u frames remaining)",
             g_savestate_guard_countdown);
    return false;
  }
  return g_xm6.save_state_mem(g_xm6_handle, data, static_cast<unsigned int>(size)) == XM6CORE_OK;
}

bool retro_unserialize(const void *data, size_t size)
{
  if (!g_xm6_handle || !data) {
    return false;
  }

  bool ok = false;
#if defined(_WIN32)
  __try {
#endif
    ok = g_xm6.load_state_mem(g_xm6_handle, data, static_cast<unsigned int>(size)) == XM6CORE_OK;
#if defined(_WIN32)
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    ok = false;
  }
#endif
  if (ok) {
    apply_runtime_core_options();
    apply_joy_type_options();
    g_video_not_ready_count = 0;
    arm_savestate_guard_frames("post state load", k_savestate_guard_frames_post_load);
  }
  return ok;
}

void retro_cheat_reset(void) {}

void retro_cheat_set(unsigned, bool, const char *) {}

void *retro_get_memory_data(unsigned id)
{
  if (id != RETRO_MEMORY_SYSTEM_RAM || !g_xm6_handle || !g_xm6.get_main_ram) {
    return nullptr;
  }
  unsigned int ram_size = 0;
  return g_xm6.get_main_ram(g_xm6_handle, &ram_size);
}

size_t retro_get_memory_size(unsigned id)
{
  if (id != RETRO_MEMORY_SYSTEM_RAM || !g_xm6_handle || !g_xm6.get_main_ram) {
    return 0;
  }
  unsigned int ram_size = 0;
  g_xm6.get_main_ram(g_xm6_handle, &ram_size);
  return static_cast<size_t>(ram_size);
}

} // extern "C"













