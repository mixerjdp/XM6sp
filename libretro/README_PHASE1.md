# XM6 libretro bridge (Phase 1)

This folder contains a first libretro bridge implementation that uses
`xm6core.dll` as backend.

## What Phase 1 includes

- Required `retro_*` API entry points.
- Dynamic loading of `xm6core.dll` (`LoadLibrary` + `GetProcAddress`).
- Single-disk load to FDD0 (`retro_load_game`).
- Video: `XRGB8888` frames from `xm6_video_poll`.
- Audio: frame-driven PCM via `xm6_audio_mix`.
- Input: RetroPad -> `xm6_input_joy` (port 0).
- Savestates: memory serialize/unserialize via `xm6_state_size` and
  `xm6_{save,load}_state_mem`.
- RAM exposure through `retro_get_memory_data/size`.
- Basic disk control callbacks for one image.
- BIOS/system path handoff from frontend system dir to
  `xm6_set_system_dir`.

## Build (Windows/MinGW)

From this directory (GNU make):

```sh
mingw32-make -f Makefile.libretro
```

Or with the included batch script:

```bat
build_libretro.bat
```

Output:

- `xm6_libretro.dll`

## Runtime requirements

Place both files together in RetroArch `cores/`:

- `xm6_libretro.dll`
- `xm6core.dll`

BIOS files must be in RetroArch `system/` directory so the core can resolve:

- `IPLROM*.DAT`, `CGROM*.DAT`, etc.

The bridge queries `RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY` and passes it to
`xm6_set_system_dir`.

## Current limits (expected in Phase 1)

- Single-disk path flow (FDD0-first boot behavior).
- No core options yet.
- No `.m3u` playlist parser yet.
- No keyboard map table yet (joypad-first path).
