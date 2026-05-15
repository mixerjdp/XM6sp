# AGENTS.md

## CRITICAL RULE

**NO DEBES HACER COMMIT NI PUSH AL REMOTO A MENOS QUE EL USUARIO LO ORDENE EXPLICITAMENTE**

## Common commands

- 64-bit Build: `build64.bat`
- 32-bit Build: `build32.bat`
- Visual Studio Build: `main\buildvs.bat`
- Deploy (Libretro core): copy `libretro\Release_x64\xm6_libretro.dll` to RetroArch cores
- No test or lint commands exist

## Architecture

Dual-frontend X68000 emulator. The `vm/` layer is OS-agnostic hardware emulation. Two render backends share `vm/` devices:

1. **Original (MFC/render)**: Legacy scanline compositor in `vm/render.cpp`, `vm/renderfast_compositor.cpp`. Uses `GraphicEngine` interface via `OriginalGraphicEngine`.

2. **PX68k (Libretro)**: Port of the px68k-libretro video engine (`C:\sw\px68k-libretro-master\`). Uses `GraphicEngine` interface via `Px68kGraphicEngine` which delegates to `Px68kRenderAdapter` ? `Px68kVideoEngine`. Entry: `vm/px68k_video_engine.cpp`, `vm/px68k_render_adapter.cpp`.

**When modifying the PX68k path, always cross-reference `C:\sw\px68k-libretro-master\`** � the authoritative original. Key files:
- `libretro/windraw.c` ? `px68k_video_engine.cpp` (compositor, WinDraw_DrawLine)
- `x68k/bg.c` ? BG/sprite functions (BG_Write, Sprite_DrawLineMcr, BG_DrawLine)
- `x68k/crtc.c` ? CRTC register handling (CRTC_Write, CRTC_Read)
- `x68k/gvram.c`, `x68k/tvram.c` ? GVRAM/TVRAM per-write handlers
- `core/px68k_timing.cpp` ? VLINE/VLINEBG calculation and draw scheduling
- `core/px68k_vm_runner.cpp` ? Frame loop (VStep==1 odd-only, VStep==4 double)

## Critical invariants for the PX68k video path

### Per-write handler wiring

The original px68k calls handler functions on every CPU write/read. The XM6 original render path uses bulk sync once per HSync. The PX68k path must receive per-write updates for correctness:

- **CRTC registers** ($E800xx): `CRTC::WriteByte` ? `render->CRTCRegWrite(0xE80000+addr, data)` ? `px68k_adapter->CRTCRegWrite()` ? `engine_->CRTCWrite()`. Updates derived state immediately (bg_hadjust, bg_vline, textdotx/y, UpdateScreenMode, TVRAMSetAllDirty).

- **Sprite/BG** ($EBxxxx): `Sprite::WriteByte/WriteWord` ? `render->SpriteBGWrite(0xEB0000+offset, data)` ? `px68k_adapter->BGWrite()` ? `engine_->BGWrite()`. Updates sprite_regs, bg_regs, bg_chr*, TextDirtyLine in real-time.

Adding new per-write handlers: add method to `Px68kRenderAdapter`, add forwarding method to `Render`, call from the appropriate `vm/` device's write function using the CPU bus address.

### VLINE / visible_vline

Must match the original formula exactly: `VLINE = ((vline - CRTC_VSTART) * CRTC_VStep) / 2` with VLINE=(uint32_t)-1 outside visible range. **No extra shifts, no effective_vstart overrides, no fallback branches.** The raw CRTC VSTART/VEND register values are always used � never XM6's computed `v_back+5` effective values.

### Bulk sync is supplementary

`SyncCRTCState`, `SyncSpriteState`, `SyncGVRAMState`, `SyncTVRAMState`, `SyncPaletteState` still run (via `SyncDynamicState` per HSync or `SyncAllState` per frame). They copy bulk state and can overwrite per-write state � this is acceptable because they set the same values. The per-write handlers provide immediate reactivity between HSync intervals.

## Render backend selection

`Render::backend` points to a `GraphicEngine` subclass. When the PX68k backend is active, `render.px68k_adapter` is non-NULL and the Original render methods (`HSyncOriginal`, `ProcessCommon`, `Mix`, `SpriteReg`, `BGMem`, `PCGMem`) are bypassed. When `px68k_adapter` is NULL, the Original path runs � all per-write methods (CRTCRegWrite, SpriteBGWrite) are no-ops.

## Build notes

- Windows-only (MSVC). The `vm/` layer uses `FASTCALL`, Windows headers, and MFC types in places.
- `build64.bat` calls `libretro\build_libretro_msvc.bat win64` internally.
- No CMake or cross-platform build system � batch files only.
- The `.gitignore` excludes `*.dll`, `*.lib`, `*.exe`, `*.obj` and VS artifacts.
