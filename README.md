# XM6_Libretro

XM6_Libretro is a Sharp X68000 emulator with two front ends:

- a classic Windows/MFC desktop app
- a Libretro core for RetroArch and other Libretro front ends

This branch keeps the original XM6 code usable, easier to build, and easier to run on more than one platform.

## Quick links

- [Latest release](https://github.com/mixerjdp/XM6_Libretro/releases/latest)
- [Source code](https://github.com/mixerjdp/XM6_Libretro)

## What this project includes

- Windows MFC version for the desktop
- Libretro core for RetroArch
- Linux build of the Libretro core
- separate FM and ADPCM volume controls
- optional shaders and resource files

## Repository layout

| Path | Purpose |
| --- | --- |
| `vm/` | Main X68000 emulation layer: memory, DMA, video, audio, devices, and system timing |
| `cpu/` | Motorola 68000 CPU core and support code |
| `mfc/` | Windows desktop user interface |
| `libretro/` | Libretro bridge and core entry points |
| `main/` | Visual Studio solution and Windows build files |
| `shaders/` | Optional video shaders |
| `res/` | UI resources and language assets |

## Build targets

| Target | Script | Output |
| --- | --- | --- |
| MFC Win32 | `build32.bat` | Windows desktop build |
| Libretro Win64 | `build64.bat` | 64-bit Libretro core |
| Main MFC solution | `main\buildvs.bat` | Visual Studio Win32 solution |
| Libretro Linux | `make -f Makefile.libretro -j4` inside `libretro/` | `xm6_libretro.so` |

The build scripts are the easiest way to compile the project from Windows.

On Linux, the Libretro core can be built directly from the `libretro/` directory:

```sh
make -f Makefile.libretro -j4
```

## Runtime files

The emulator expects the normal X68000 BIOS and system files to be present.

Common files used by this project include:

- `IPLROM.DAT`
- `CGROM.DAT`
- `CGROM.TMP`
- `SCSIINROM.DAT`
- `SCSIEXROM.DAT`
- `SRAM.DAT`

For RetroArch, point the system directory to the folder that contains those files.

## Libretro usage

The Libretro core is designed to run inside RetroArch and similar front ends.

Typical setup:

1. Copy the core to RetroArch `cores/`
2. Put the BIOS and system files in RetroArch `system/`
3. Load the game image or disk set from the front end

Where supported, the core also handles disk playlists and the usual RetroArch callbacks.

## Audio

XM6_Libretro keeps FM and ADPCM as separate controls.

That makes it easier to tune sound per game and per backend without forcing a single global volume.

## Releases

The initial public release includes:

- MFC Win32 build
- Libretro Win32 build
- Libretro Win64 build
- Linux Libretro `.so`

## License and credits

XM6 is the original Sharp X68000 emulator project by Ytanaka.

This repository also includes third-party code and ideas from projects such as:

- Musashi 68000 CPU core
- fmgen / FM synthesis support
- windrv-style host/device bridging

Each component keeps its own license and notice requirements. Check the source headers for details.

## Notes

This repository focuses on keeping the classic XM6 codebase practical to build while adding modern Libretro support and cleaner release packaging.
