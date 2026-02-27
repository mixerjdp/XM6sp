# AGENTS.md
#
# This file provides guidance to WARP (warp.dev) when working with code in this repository.

## Common commands
Build (VS2012 toolchain as configured in `.vscode/build.bat`):
- Debug: `.vscode\build.bat Debug`
- Release: `.vscode\build.bat Release`
  - This runs: `msbuild 00proj.vc7\XM6.sln /p:Configuration=<Debug|Release> /p:Platform=Win32 /m`
  - `build.bat` calls `C:\Program Files (x86)\Microsoft Visual Studio 11.0\VC\bin\vcvars32.bat` first.

Run:
- Debug exe: `00proj.vc7\Debug\XM6.exe`
- Release exe: `00proj.vc7\Release\XM6.exe`

Tests/Lint:
- No test or lint commands were found in the repo.

## High-level architecture
The project is an XM6 (Sharp X68000) emulator split into a host UI layer and a portable VM layer, with a scheduler thread driving emulation. The following overview is distilled from `Leeme.MD`.

### Two-layer split
- **VM layer** (`vm/`): OS-agnostic emulation of X68000 hardware (CPU, CRTC, DMA, FM audio, etc.). Owns the emulated memory/register state.
- **MFC host layer** (`mfc/`): Windows UI built on MFC; handles input, audio/video output, and bridges host services to the VM.

### Startup and main loop
- Entry point: `CApp::InitInstance` in `mfc/mfc_app.cpp` sets up host capabilities/paths and creates the main frame window.
- VM creation: `CFrmWnd::OnCreate` in `mfc/mfc_frm.cpp` instantiates the global VM, initializes devices, and connects host-side components.
- Emulation loop: a `WM_KICK` message starts `CScheduler::Run` in `mfc/mfc_sch.cpp`, which spins a dedicated thread that repeatedly calls `vm->Exec()` (in `vm/vm.cpp`) to execute instructions and process interrupts/events.

### Video/audio output
- VM video devices write to internal buffers (`vm/gvram.cpp`, `vm/tvram.cpp`).
- `mfc/mfc_draw.cpp` consumes those buffers and draws via GDI/DirectX on the UI thread.

### MFC component system and command handling
- MFC-side components inherit `CComponent` (`mfc/mfc_com.h`), are linked in a list, and are discoverable via `CFrmWnd::SearchComponent(ID)`.
- UI commands are wired via MFC message maps in `mfc/mfc_frm.cpp`, with command logic mostly in `mfc/mfc_cmd.cpp`.

### Localization resources
- `mfc/mfc_app.cpp` determines Japanese vs. non‑Japanese environments.
- String resources are loaded via `GetMsg(UINT, CString&)` against `mfc/mfc_res.rc` and IDs in `mfc/mfc_res.h`; this repo is the Spanish‑translated variant.
