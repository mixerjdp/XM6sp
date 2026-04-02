# AGENTS.md
#
# This file provides guidance to AI agents when working with code in this repository.

## Common commands
The build scripts have been refactored and relocated. You can build the project using the following batch files:
- 32-bit Build: `build32.bat`
- 64-bit Build: `build64.bat`
- Visual Studio Build: `main\buildvs.bat`

Run:
- Standalone executables are directed to modern output directories (e.g., `build_win32\`, `build_win64\`, or inside `main\`) depending on the build profile executed.
- Libretro cores map to `.dll` or `.so` libraries, depending on the OS target.

Tests/Lint:
- No test or lint commands were found in the repo.

## High-level architecture
The project is an XM6 (Sharp X68000) emulator. The architecture has been heavily restructured and expanded from its legacy core. It is now split into multiple focused modules, introducing a multi-platform Libretro frontend alongside the traditional MFC host.

### Layer split and modular subdirectories
- **VM layer** (`vm/`): OS-agnostic underlying emulation of X68000 hardware (CRTC, DMA, FM audio, etc.). This module owns the foundational emulated memory and register states.
- **CPU core** (`cpu/`): Isolated and optimized instructions and execution paths for the processor cores.
- **MFC host layer** (`mfc/`): The standalone Windows UI built on MFC. It continues to handle input, windowing, and bridges host services to the VM.
- **Libretro frontend** (`libretro/`): A standard Libretro core implementation. This acts as an alternative frontend that wraps the VM layer for integration into multi-platform ecosystems (e.g., RetroArch), entirely bypassing the MFC overhead.
- **Main module** (`main/`): Contains overarching project definitions, startup routines, and Visual Studio build infrastructure.

### Startup and execution loops
- **Standalone (MFC)**: Entry points reside in the `mfc/` or `main/` layers setting up host capabilities. A dedicated scheduler thread is spawned to repeatedly call the continuous `vm->Exec()` loop, managing interrupts synchronously.
- **Libretro Core**: Driven by the host frontend through standard `retro_run()` callbacks. It manages execution iteratively, advancing frames and syncing audio/video sequentially without relying on Windows-specific threads.

### Expanded Video/Audio output
- Both Audio and Video handling have been significantly expanded with enhanced performance and features.
- Video: Emulated hardware capabilities bridge internal VM video buffers (`vm/gvram.cpp`, `vm/tvram.cpp`) to rendering abstraction layers. Custom shader structures (located in `shaders/`) provide modern scaling and post-processing.
- Audio: The audio pipeline has been expanded with greater fidelity and integration flexibility to satisfy the continuous streaming demands of the new libretro frontend and the standalone client.

### Localization resources
- String resources and graphical assets (`res/`) have been expanded.
- The emulator now supports 3 distinct languages, extending its accessibility across broader regions, dynamically dispatching localized labels across the interface.
