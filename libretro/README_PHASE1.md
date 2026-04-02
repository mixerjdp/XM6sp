# XM6 libretro bridge (Phase 2)

This folder contains a libretro bridge implementation that uses
`xm6core.dll` as backend.

## What Phase 2 includes

- Required `retro_*` API entry points.
- Dynamic loading of `xm6core.dll` (`LoadLibrary` + `GetProcAddress`).
- Multi-disk load with `.m3u` support and disk labels.
- Video: `XRGB8888` frames from `xm6_video_poll`.
- Audio: frame-driven PCM via `xm6_audio_mix`.
- Input: RetroPad -> `xm6_input_joy` (port 0).
- Keyboard callback: RetroArch keyboard -> `xm6_input_key`.
- Savestates: memory serialize/unserialize via `xm6_state_size` and
  `xm6_{save,load}_state_mem`.
- RAM exposure through `retro_get_memory_data/size`.
  - If backend `xm6core.dll` does not export `xm6_get_main_ram`, the core still
    loads and only disables RAM exposure.
- Disk control + disk control ext callbacks.
- BIOS/system path handoff from frontend system dir to
  `xm6_set_system_dir`.
- Core options:
  - `xm6_disk_drive` (`FDD0`/`FDD1`)
  - `xm6_exec_mode` (`exec_to_frame`/`legacy_exec`)
  - `xm6_audio_engine` (`XM6`/`PX68k`)
  - `xm6_legacy_dmac_cnt` (`disabled`/`enabled`)
  - `xm6_pad_start_select` (`xf_keys`/`disabled`)

## Build (Windows/MinGW)

From this directory (GNU make):

```sh
mingw32-make -f Makefile.libretro
```

Or with the included batch script:

```bat
build_libretro.bat
```

This MinGW build now links statically (`-static -static-libgcc -static-libstdc++`)
to avoid runtime DLL mismatches in RetroArch.

## Build (Windows/MSVC)

Use the MSVC helper script to generate architecture-specific DLLs:

```bat
build_libretro_msvc.bat win32
build_libretro_msvc.bat win64
```

Outputs:

- `xm6_libretro_win32.dll`
- `xm6_libretro_win64.dll`

Output:

- `xm6_libretro.dll`

## Runtime requirements

Place both files together in RetroArch `cores/`:

- `xm6_libretro.dll`
- `xm6core.dll`

Architecture must match:

- `RetroArch Win32` + `xm6_libretro_win32.dll` + `xm6core.dll` (x86)
- `RetroArch Win64` + `xm6_libretro_win64.dll` + `xm6core.dll` (x64)

Current repo backend:

- `main/Debug_DLL/xm6core.dll` is x86 (`machine 14C`), so it is compatible with
  Win32 RetroArch only.

BIOS files must be in RetroArch `system/` directory so the core can resolve:

- `IPLROM*.DAT`, `CGROM*.DAT`, etc.

The bridge queries `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` and passes it to
`xm6_set_system_dir`.

## Current limits (pending next phase)

- No legacy `libretro_core_options_v2` categories yet.
- Keyboard mapping is functional but still minimal vs PX68k full table.
- No MIDI interface bridge yet.
