#include <cstdio>
#include <cstdarg>
#include <cctype>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

#include "libretro.h"

#define XM6CORE_STATIC
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

static const double k_fps = 55.0;
static const unsigned k_sample_rate = 44100;
static const unsigned k_default_width = 768;
static const unsigned k_default_height = 512;

static unsigned g_frame_width = k_default_width;
static unsigned g_frame_height = k_default_height;
static double g_audio_fraction = 0.0;
static bool g_game_loaded = false;

static std::vector<std::string> g_disk_paths;
static std::vector<std::string> g_disk_labels;
static unsigned g_disk_index = 0;
static int g_disk_drive = 0;
static bool g_disk_ejected = false;

static bool g_use_exec_to_frame = true;
static bool g_pad_start_select_as_xf = true;

static bool g_prev_start = false;
static bool g_prev_select = false;

static std::vector<int16_t> g_audio_buffer;

struct xm6_api_t {
  HMODULE module = nullptr;

  XM6Handle (XM6CORE_CALL *create)(void) = nullptr;
  void (XM6CORE_CALL *destroy)(XM6Handle) = nullptr;
  int (XM6CORE_CALL *set_system_dir)(const char *system_dir) = nullptr;

  int (XM6CORE_CALL *exec)(XM6Handle handle, unsigned int hus) = nullptr;
  int (XM6CORE_CALL *exec_to_frame)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *reset)(XM6Handle handle) = nullptr;
  int (XM6CORE_CALL *set_power)(XM6Handle handle, int enabled) = nullptr;

  int (XM6CORE_CALL *video_poll)(XM6Handle handle, xm6_video_frame_t *out_frame) = nullptr;
  int (XM6CORE_CALL *video_consume)(XM6Handle handle) = nullptr;

  int (XM6CORE_CALL *audio_configure)(XM6Handle handle, unsigned int sample_rate) = nullptr;
  int (XM6CORE_CALL *audio_mix)(XM6Handle handle, short *out_interleaved_stereo,
                                unsigned int frames, unsigned int *out_frames) = nullptr;

  int (XM6CORE_CALL *input_joy)(XM6Handle handle, int port,
                                const unsigned int axes[4], const int buttons[8]) = nullptr;
  int (XM6CORE_CALL *input_key)(XM6Handle handle, unsigned int xm6_keycode, int pressed) = nullptr;

  int (XM6CORE_CALL *mount_fdd)(XM6Handle handle, int drive, const char *image_path, int media_hint) = nullptr;
  int (XM6CORE_CALL *eject_fdd)(XM6Handle handle, int drive, int force) = nullptr;

  int (XM6CORE_CALL *state_size)(XM6Handle handle, unsigned int *out_size) = nullptr;
  int (XM6CORE_CALL *save_state_mem)(XM6Handle handle, void *buffer, unsigned int size) = nullptr;
  int (XM6CORE_CALL *load_state_mem)(XM6Handle handle, const void *buffer, unsigned int size) = nullptr;

  void *(XM6CORE_CALL *get_main_ram)(XM6Handle handle, unsigned int *out_size) = nullptr;
};

static xm6_api_t g_xm6;
static XM6Handle g_xm6_handle = nullptr;

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
    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
  }
}

static bool get_core_module_dir(char *out_dir, size_t out_dir_size)
{
  if (!out_dir || out_dir_size == 0) {
    return false;
  }

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
}

template <typename FnType>
static bool load_required_symbol(FnType *fn, const char *symbol)
{
  *fn = reinterpret_cast<FnType>(GetProcAddress(g_xm6.module, symbol));
  if (!*fn) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Missing required symbol: %s", symbol);
    return false;
  }
  return true;
}

template <typename FnType>
static void load_optional_symbol(FnType *fn, const char *symbol)
{
  *fn = reinterpret_cast<FnType>(GetProcAddress(g_xm6.module, symbol));
}

static void unload_xm6_api()
{
  if (g_xm6.module) {
    FreeLibrary(g_xm6.module);
  }
  g_xm6 = xm6_api_t();
}

static bool load_xm6_api()
{
  if (g_xm6.module) {
    return true;
  }

  char core_dir[MAX_PATH] = {};
  char dll_path[MAX_PATH] = {};
  if (get_core_module_dir(core_dir, sizeof(core_dir))) {
    std::snprintf(dll_path, sizeof(dll_path), "%s%s", core_dir, "xm6core.dll");
    g_xm6.module = LoadLibraryA(dll_path);
  }

  if (!g_xm6.module) {
    g_xm6.module = LoadLibraryA("xm6core.dll");
  }

  if (!g_xm6.module) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Could not load xm6core.dll");
    return false;
  }

  if (!load_required_symbol(&g_xm6.create, "xm6_create") ||
      !load_required_symbol(&g_xm6.destroy, "xm6_destroy") ||
      !load_required_symbol(&g_xm6.exec, "xm6_exec") ||
      !load_required_symbol(&g_xm6.reset, "xm6_reset") ||
      !load_required_symbol(&g_xm6.set_power, "xm6_set_power") ||
      !load_required_symbol(&g_xm6.video_poll, "xm6_video_poll") ||
      !load_required_symbol(&g_xm6.video_consume, "xm6_video_consume") ||
      !load_required_symbol(&g_xm6.audio_configure, "xm6_audio_configure") ||
      !load_required_symbol(&g_xm6.audio_mix, "xm6_audio_mix") ||
      !load_required_symbol(&g_xm6.input_joy, "xm6_input_joy") ||
      !load_required_symbol(&g_xm6.input_key, "xm6_input_key") ||
      !load_required_symbol(&g_xm6.mount_fdd, "xm6_mount_fdd") ||
      !load_required_symbol(&g_xm6.eject_fdd, "xm6_eject_fdd") ||
      !load_required_symbol(&g_xm6.state_size, "xm6_state_size") ||
      !load_required_symbol(&g_xm6.save_state_mem, "xm6_save_state_mem") ||
      !load_required_symbol(&g_xm6.load_state_mem, "xm6_load_state_mem") ||
      !load_required_symbol(&g_xm6.get_main_ram, "xm6_get_main_ram")) {
    unload_xm6_api();
    return false;
  }

  load_optional_symbol(&g_xm6.exec_to_frame, "xm6_exec_to_frame");
  load_optional_symbol(&g_xm6.set_system_dir, "xm6_set_system_dir");

  core_log(RETRO_LOG_INFO, "[xm6-libretro] Loaded xm6core.dll");
  return true;
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
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] xm6_create failed");
    return false;
  }

  if (g_xm6.audio_configure(g_xm6_handle, k_sample_rate) != XM6CORE_OK) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] audio configure failed");
  }

  return true;
}

static void destroy_xm6_handle()
{
  if (g_xm6_handle && g_xm6.destroy) {
    g_xm6.destroy(g_xm6_handle);
  }
  g_xm6_handle = nullptr;
  g_game_loaded = false;
  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_index = 0;
  g_disk_ejected = false;
  g_prev_start = false;
  g_prev_select = false;
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

  return g_xm6.set_system_dir(sys_dir) == XM6CORE_OK;
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

static bool parse_m3u_playlist(const char *m3u_path, std::vector<std::string> *out_paths)
{
  if (!m3u_path || !out_paths) {
    return false;
  }

  std::ifstream input(m3u_path, std::ios::in | std::ios::binary);
  if (!input.is_open()) {
    return false;
  }

  std::vector<std::string> entries;
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
    entries.push_back(path_join(base_dir, line));
  }

  if (entries.empty()) {
    return false;
  }
  *out_paths = entries;
  return true;
}

static void build_disk_labels()
{
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

static bool mount_current_disk()
{
  if (!g_xm6_handle || g_disk_paths.empty() || g_disk_index >= g_disk_paths.size()) {
    return false;
  }
  const std::string &path = g_disk_paths[g_disk_index];
  if (path.empty()) {
    return false;
  }
  const int rc = g_xm6.mount_fdd(g_xm6_handle, g_disk_drive, path.c_str(), 0);
  if (rc != XM6CORE_OK) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Failed to mount disk[%u]: %s",
             g_disk_index, path.c_str());
    return false;
  }
  return true;
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
  } else {
    g_disk_drive = 0;
  }

  var.key = "xm6_exec_mode";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_use_exec_to_frame = std::strcmp(var.value, "legacy_exec") != 0;
  } else {
    g_use_exec_to_frame = true;
  }

  var.key = "xm6_pad_start_select";
  if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value) {
    g_pad_start_select_as_xf = std::strcmp(var.value, "xf_keys") == 0;
  } else {
    g_pad_start_select_as_xf = true;
  }
}

static void register_core_options()
{
  if (!g_environ_cb) {
    return;
  }

  static const retro_variable vars[] = {
    { "xm6_disk_drive",
      "Disk drive for content swap; FDD0|FDD1" },
    { "xm6_exec_mode",
      "Frame execution mode; exec_to_frame|legacy_exec" },
    { "xm6_pad_start_select",
      "Map Start/Select to XF keys; xf_keys|disabled" },
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
    case RETROK_COLON: return 0x28;
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

static unsigned calc_audio_frames_for_run()
{
  const double samples = (static_cast<double>(k_sample_rate) / k_fps) + g_audio_fraction;
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

static bool joy_pressed(unsigned id)
{
  if (!g_input_state_cb) {
    return false;
  }

  if (g_supports_input_bitmasks) {
    const unsigned mask = static_cast<unsigned>(
      g_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK));
    return (mask & (1u << id)) != 0;
  }

  return g_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, id) != 0;
}

static void poll_and_push_input()
{
  if (!g_xm6_handle || !g_xm6.input_joy || !g_input_poll_cb) {
    return;
  }

  g_input_poll_cb();

  const bool left  = joy_pressed(RETRO_DEVICE_ID_JOYPAD_LEFT);
  const bool right = joy_pressed(RETRO_DEVICE_ID_JOYPAD_RIGHT);
  const bool up    = joy_pressed(RETRO_DEVICE_ID_JOYPAD_UP);
  const bool down  = joy_pressed(RETRO_DEVICE_ID_JOYPAD_DOWN);

  unsigned int axes[4] = {};
  axes[0] = left ? 0xFFFFFA00u : (right ? 0x00000600u : 0);
  axes[1] = up   ? 0xFFFFFA00u : (down  ? 0x00000600u : 0);
  axes[2] = 0;
  axes[3] = 0;

  int buttons[8] = {};
  buttons[0] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_B) ? 1 : 0;
  buttons[1] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_A) ? 1 : 0;
  buttons[2] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_Y) ? 1 : 0;
  buttons[3] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_X) ? 1 : 0;
  buttons[4] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_L) ? 1 : 0;
  buttons[5] = joy_pressed(RETRO_DEVICE_ID_JOYPAD_R) ? 1 : 0;
  const bool select_pressed = joy_pressed(RETRO_DEVICE_ID_JOYPAD_SELECT);
  const bool start_pressed = joy_pressed(RETRO_DEVICE_ID_JOYPAD_START);
  buttons[6] = select_pressed ? 1 : 0;
  buttons[7] = start_pressed ? 1 : 0;

  g_xm6.input_joy(g_xm6_handle, 0, axes, buttons);

  if (g_pad_start_select_as_xf && g_xm6.input_key) {
    if (start_pressed != g_prev_start) {
      g_xm6.input_key(g_xm6_handle, 0x55, start_pressed ? 1 : 0);  // XF1
      g_prev_start = start_pressed;
    }
    if (select_pressed != g_prev_select) {
      g_xm6.input_key(g_xm6_handle, 0x57, select_pressed ? 1 : 0); // XF3
      g_prev_select = select_pressed;
    }
  } else {
    g_prev_start = start_pressed;
    g_prev_select = select_pressed;
  }
}

static bool disk_set_eject_state(bool ejected)
{
  if (!g_xm6_handle) {
    return false;
  }

  if (ejected) {
    g_xm6.eject_fdd(g_xm6_handle, g_disk_drive, 1);
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
    }
    return true;
  }

  if (g_disk_paths.empty()) {
    g_disk_paths.resize(1);
    g_disk_labels.resize(1);
  }
  g_disk_paths[index] = info->path;
  g_disk_labels[index] = path_basename_no_ext(g_disk_paths[index]);
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
  g_audio_fraction = 0.0;
  g_game_loaded = false;
  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_index = 0;
  g_disk_ejected = false;
  g_prev_start = false;
  g_prev_select = false;
  g_audio_buffer.clear();

  load_xm6_api();
}

void retro_deinit(void)
{
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
  info->library_name = "XM6 (DLL Bridge)";
  info->library_version = "Phase2";
  info->need_fullpath = true;
  info->block_extract = false;
  info->valid_extensions = "dim|xdf|d88|88d|hdm|dup|2hd|img|hdf|m3u";
}

void retro_get_system_av_info(struct retro_system_av_info *info)
{
  if (!info) {
    return;
  }

  std::memset(info, 0, sizeof(*info));
  info->timing.fps = k_fps;
  info->timing.sample_rate = static_cast<double>(k_sample_rate);
  info->geometry.base_width = g_frame_width;
  info->geometry.base_height = g_frame_height;
  info->geometry.max_width = 1024;
  info->geometry.max_height = 1024;
  info->geometry.aspect_ratio = 4.0f / 3.0f;
}

void retro_set_controller_port_device(unsigned, unsigned) {}

void retro_reset(void)
{
  if (g_xm6_handle && g_xm6.reset) {
    g_xm6.reset(g_xm6_handle);
  }
}

bool retro_load_game(const struct retro_game_info *info)
{
  if (!info || !info->path || !*info->path) {
    return false;
  }
  if (!ensure_xm6_handle()) {
    return false;
  }

  if (!set_system_directory_from_frontend()) {
    core_log(RETRO_LOG_WARN, "[xm6-libretro] Could not set system directory");
  }

  apply_core_option_values();

  enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
  if (!g_environ_cb || !g_environ_cb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt)) {
    core_log(RETRO_LOG_ERROR, "[xm6-libretro] Frontend does not support XRGB8888");
    return false;
  }

  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_index = 0;

  if (path_has_extension(info->path, ".m3u")) {
    if (!parse_m3u_playlist(info->path, &g_disk_paths)) {
      core_log(RETRO_LOG_ERROR, "[xm6-libretro] Failed to parse M3U: %s", info->path);
      return false;
    }
  } else {
    g_disk_paths.push_back(info->path);
  }
  build_disk_labels();

  if (!mount_current_disk()) {
    return false;
  }
  g_disk_ejected = false;
  register_disk_interface();

  g_xm6.set_power(g_xm6_handle, 1);
  g_xm6.reset(g_xm6_handle);

  g_game_loaded = true;
  return true;
}

bool retro_load_game_special(unsigned, const struct retro_game_info *, size_t)
{
  return false;
}

void retro_unload_game(void)
{
  if (g_xm6_handle && g_xm6.eject_fdd) {
    g_xm6.eject_fdd(g_xm6_handle, g_disk_drive, 1);
  }
  g_game_loaded = false;
  g_disk_paths.clear();
  g_disk_labels.clear();
  g_disk_index = 0;
  g_disk_ejected = false;
  g_prev_start = false;
  g_prev_select = false;
}

unsigned retro_get_region(void)
{
  return RETRO_REGION_NTSC;
}

void retro_run(void)
{
  if (!g_xm6_handle || !g_game_loaded) {
    if (g_video_cb) {
      g_video_cb(nullptr, g_frame_width, g_frame_height, g_frame_width * sizeof(uint32_t));
    }
    return;
  }

  if (g_environ_cb) {
    bool updated = false;
    if (g_environ_cb(RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE, &updated) && updated) {
      const int old_drive = g_disk_drive;
      apply_core_option_values();
      if (old_drive != g_disk_drive && !g_disk_ejected) {
        mount_current_disk();
      }
    }
  }

  poll_and_push_input();

  if (g_use_exec_to_frame && g_xm6.exec_to_frame) {
    g_xm6.exec_to_frame(g_xm6_handle);
  } else {
    g_xm6.exec(g_xm6_handle, 36000);
  }

  xm6_video_frame_t frame = {};
  if (g_xm6.video_poll(g_xm6_handle, &frame) == XM6CORE_OK && frame.pixels_argb32 &&
      frame.width > 0 && frame.height > 0) {
    g_frame_width = frame.width;
    g_frame_height = frame.height;
    if (g_video_cb) {
      g_video_cb(frame.pixels_argb32, frame.width, frame.height,
                 frame.stride_pixels * sizeof(uint32_t));
    }
    g_xm6.video_consume(g_xm6_handle);
  } else if (g_video_cb) {
    g_video_cb(nullptr, g_frame_width, g_frame_height, g_frame_width * sizeof(uint32_t));
  }

  if (g_audio_batch_cb) {
    const unsigned want_frames = calc_audio_frames_for_run();
    g_audio_buffer.resize(want_frames * 2);
    unsigned out_frames = 0;
    if (g_xm6.audio_mix(g_xm6_handle, g_audio_buffer.data(), want_frames, &out_frames) == XM6CORE_OK &&
        out_frames > 0) {
      g_audio_batch_cb(g_audio_buffer.data(), out_frames);
    }
  } else if (g_audio_cb) {
    g_audio_cb(0, 0);
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
  return g_xm6.save_state_mem(g_xm6_handle, data, static_cast<unsigned int>(size)) == XM6CORE_OK;
}

bool retro_unserialize(const void *data, size_t size)
{
  if (!g_xm6_handle || !data) {
    return false;
  }
  return g_xm6.load_state_mem(g_xm6_handle, data, static_cast<unsigned int>(size)) == XM6CORE_OK;
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
