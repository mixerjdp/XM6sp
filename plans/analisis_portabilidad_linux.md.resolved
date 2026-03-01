# Análisis de Portabilidad: XM6sp → Linux

## Resumen Ejecutivo

El emulador XM6sp presenta un **acoplamiento profundo con la plataforma Windows** en todas sus capas. Portar a Linux requeriría **reescribir prácticamente toda la capa host** (`mfc/`) y adaptar secciones significativas de la capa VM (`vm/`). La capa de emulación de hardware (VM) es conceptualmente agnóstica al OS, pero en la práctica contiene dependencias Win32 embebidas en tipos de datos, I/O de archivos y sincronización.

El esfuerzo estimado es **alto** — equivalente a un proyecto de meses para un desarrollador experimentado.

---

## Arquitectura Actual (3 Capas)

```mermaid
graph TD
    subgraph "Capa HOST (mfc/) — 100% Windows"
        MFC["MFC Framework<br/>CApp, CFrmWnd, CComponent"]
        DX9["DirectX 9<br/>IDirect3D9, D3D Textures"]
        DS["DirectSound<br/>Audio Output"]
        DI["DirectInput<br/>Joystick/Gamepad"]
        GDI["Win32 GDI<br/>Overlay, Bitmaps"]
        W32["Win32 API<br/>Hilos, Mensajes, Registry"]
        ASM_MFC["mfc_asm.asm<br/>MASM x86"]
    end

    subgraph "Capa VM (vm/) — Parcialmente acoplada"
        VM_CORE["Emulación Hardware<br/>CPU, CRTC, DMA, FDC, SCSI..."]
        FILEIO["fileio.cpp<br/>_topen, _read, _write (CRT MSVC)"]
        FILEPATH["filepath.cpp<br/>_MAX_PATH, FILETIME, TCHAR"]
        SYNC["sync.cpp<br/>CCriticalSection (MFC)"]
        WINDRV["windrv.cpp<br/>Bridge filesystem Win32↔Human68k"]
        ASM_VM["core_asm.asm, rend_asm.asm<br/>MASM x86"]
    end

    subgraph "Capa CPU (cpu/)"
        STAR["Starscream 68000<br/>star.asm (1.8MB MASM x86)"]
        STAR_C["Star.c<br/>156KB, generado"]
    end

    MFC --> VM_CORE
    DX9 --> VM_CORE
    DS --> VM_CORE
    SYNC --> VM_CORE
    VM_CORE --> STAR
    VM_CORE --> FILEIO
    VM_CORE --> FILEPATH
```

---

## Categorías de Dependencias Windows

### 1. Framework MFC (Microsoft Foundation Classes)

| Impacto | Archivos | Descripción |
|:--------|:---------|:------------|
| **Crítico** | Todos los `mfc_*.cpp/h` (65 archivos) | Jerarquía de clases `CApp`, `CFrmWnd`, `CComponent`, message maps, diálogos, menús |
| **Crítico** | `sync.cpp/h` (en `vm/`) | Usa `CCriticalSection` de MFC para sincronización de hilos |

**Cambio requerido:** Reescribir toda la UI y el ciclo de vida de la aplicación. [sync.cpp](file:///c:/sw/XM62022/vm/sync.cpp) requiere reimplementación con `std::mutex` o `pthread_mutex_t`.

---

### 2. DirectX 9 (Renderizado)

| Impacto | Archivos | APIs utilizadas |
|:--------|:---------|:----------------|
| **Crítico** | `mfc_dx9.cpp/h` | `IDirect3D9`, `IDirect3DDevice9`, `IDirect3DTexture9`, `IDirect3DPixelShader9`, HLSL shaders |
| **Crítico** | `mfc_draw.cpp/h` | Renderizado de framebuffer del emulador a la ventana |
| **Crítico** | `mfc_rend.cpp/h` | Pipeline de rendering |

**Cambio requerido:** Reemplazar D3D9 con OpenGL, Vulkan, o SDL2 Renderer. Los shaders HLSL (ej. [crt.hlsl](file:///c:/sw/XM62022/crt.hlsl)) necesitan conversión a GLSL.

---

### 3. DirectSound / DirectInput (Audio e Input)

| Impacto | Archivos | APIs utilizadas |
|:--------|:---------|:----------------|
| **Crítico** | `mfc_snd.cpp/h` | DirectSound para output de audio |
| **Crítico** | `mfc_inp.cpp/h` | DirectInput para joystick/gamepad |
| **Crítico** | `mfc_midi.cpp/h` | Win32 MIDI API (`midiOutOpen`, etc.) |

**Cambio requerido:** SDL2 Audio / PortAudio para sonido. SDL2 GameController / evdev para input. ALSA/PulseAudio para MIDI o librerías como RtMidi.

---

### 4. Win32 API Nativa

| Impacto | Archivos | APIs utilizadas |
|:--------|:---------|:----------------|
| **Alto** | `mfc_sch.cpp/h` | `CreateThread`, `WaitForSingleObject`, `SetEvent` |
| **Alto** | `mfc_frm.cpp/h` | `HWND`, `SendMessage`, `PostMessage`, message loop |
| **Alto** | `mfc_w32.cpp/h` | APIs Win32 diversas, COM port, etc. |
| **Alto** | `mfc_cfg.cpp/h` | INI files via Win32 `GetPrivateProfileString`, registry |
| **Medio** | `mfc_host.cpp/h` | File system host, diálogos de archivos |

**Cambio requerido:** `std::thread` / pthreads para hilos. GTK+/Qt/SDL2 para ventanas y diálogos. Parseo manual de INI o librería portable (ej. `inih`).

---

### 5. Ensamblador x86 MASM

| Impacto | Archivos | Tamaño | Descripción |
|:--------|:---------|:-------|:------------|
| **Crítico** | [cpu/star.asm](file:///c:/sw/XM62022/cpu/star.asm) | 1.8 MB | Núcleo CPU Motorola 68000 (Starscream) |
| **Alto** | [vm/rend_asm.asm](file:///c:/sw/XM62022/vm/rend_asm.asm) | 119 KB | Rutinas optimizadas de renderizado |
| **Alto** | [vm/core_asm.asm](file:///c:/sw/XM62022/vm/core_asm.asm) | 7 KB | Rutinas core de la VM |
| **Medio** | [mfc/mfc_asm.asm](file:///c:/sw/XM62022/mfc/mfc_asm.asm) | 34 KB | Rutinas de la capa host |

**Cambio requerido:** Starscream es el problema **más grave**. Opciones:
1. **Convertir de MASM a NASM/GAS** — factible pero laborioso (~1.8MB de ASM x86-32)
2. **Usar [Star.c](file:///c:/sw/XM62022/cpu/Star.c) (156KB)** — ya existe una versión C generada, pero podría tener diferencias de rendimiento o comportamiento
3. **Reemplazar Starscream por Musashi** — emulador 68000 portable en C puro, ampliamente usado en proyectos similares

> [!IMPORTANT]
> [Star.c](file:///c:/sw/XM62022/cpu/Star.c) es la opción más pragmática si compila limpiamente. Musashi es el camino más seguro si [Star.c](file:///c:/sw/XM62022/cpu/Star.c) no funciona correctamente.

---

### 6. Tipos y Macros Win32

| Macro/Tipo | Uso | Reemplazo Linux |
|:-----------|:----|:----------------|
| `BOOL`, `TRUE`, `FALSE` | Ubiquo | `int` o `bool` (C++) |
| `DWORD` | Ubiquo | `uint32_t` |
| `TCHAR`, `_T()`, `LPCTSTR` | Strings | `char` / `const char*` (UTF-8) |
| `FASTCALL` | Casi todas las funciones | `__attribute__((fastcall))` en GCC (x86) o eliminar |
| `_MAX_PATH` | Rutas de archivos | `PATH_MAX` de `<limits.h>` |
| `FILETIME` | Timestamps | `struct timespec` |
| `ASSERT` | Debug | `assert()` de `<cassert>` |
| `WINVER` | Versionado | Eliminar |

**Cambio requerido:** Crear un header de compatibilidad (`compat.h`) con `typedef` y `#define` para mapear estos tipos.

---

### 7. Filesystem Bridge (`windrv.cpp/h`)

| Impacto | Archivos | Tamaño |
|:--------|:---------|:-------|
| **Alto** | [vm/windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp) | 74 KB |
| **Alto** | [vm/windrv.h](file:///c:/sw/XM62022/vm/windrv.h) | 21 KB |

Este módulo traduce llamadas del sistema de archivos de Human68k (el OS del X68000) a llamadas Win32 (`FindFirstFile`, `CreateFile`, `GetFileAttributes`, etc.).

**Cambio requerido:** Reimplementar usando POSIX (`opendir`, `readdir`, `stat`, `open`, etc.).

---

## Estrategia de Portabilidad Recomendada

### Fase 1: Capa de Abstracción de Plataforma
1. Crear `compat.h` con typedefs portables (`DWORD` → `uint32_t`, etc.)
2. Reimplementar [fileio.cpp](file:///c:/sw/XM62022/vm/fileio.cpp) con `open()`/`read()`/`write()` POSIX
3. Reimplementar [filepath.cpp](file:///c:/sw/XM62022/vm/filepath.cpp) con `<filesystem>` (C++17) o rutas POSIX
4. Reimplementar [sync.cpp](file:///c:/sw/XM62022/vm/sync.cpp) con `std::mutex`
5. Definir `FASTCALL` como vacío o `__attribute__((fastcall))` en x86

### Fase 2: Núcleo CPU
1. Evaluar si [Star.c](file:///c:/sw/XM62022/cpu/Star.c) compila y funciona correctamente en GCC/Linux
2. Si no: convertir [star.asm](file:///c:/sw/XM62022/cpu/star.asm) a NASM, o reemplazar Starscream por Musashi
3. Convertir [core_asm.asm](file:///c:/sw/XM62022/vm/core_asm.asm) y [rend_asm.asm](file:///c:/sw/XM62022/vm/rend_asm.asm) a NASM o reimplementar en C

### Fase 3: Frontend SDL2 (Reemplazo de mfc/)
1. Adoptar **SDL2** como framework base (ventana, eventos, rendering, audio, input)
2. Reimplementar el ciclo de vida de la aplicación (`CApp` → `main()` + SDL_Init)
3. Reimplementar el rendering (`CDX9Renderer` → SDL2_Renderer u OpenGL)
4. Reimplementar audio (`CSound` → SDL_Audio o PortAudio)
5. Reimplementar input (`CInput` → SDL_GameController)
6. Reimplementar la configuración (INI parser portable)
7. Reimplementar diálogos de archivos (GTK file chooser o nativefiledialog)

### Fase 4: Adaptaciones Específicas
1. Reimplementar [windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp) con POSIX filesystem
2. Convertir shaders HLSL a GLSL (si se usa OpenGL)
3. Adaptar [mfc_midi.cpp](file:///c:/sw/XM62022/mfc/mfc_midi.cpp) con ALSA/RtMidi
4. Adaptar sistema de build: `CMakeLists.txt` o `Makefile`

---

## Matriz de Esfuerzo Estimado

| Componente | Esfuerzo | Riesgo | Estrategia |
|:-----------|:---------|:-------|:-----------|
| typedefs/macros (`compat.h`) | Bajo | Bajo | Header de compatibilidad |
| [fileio.cpp](file:///c:/sw/XM62022/vm/fileio.cpp) / [filepath.cpp](file:///c:/sw/XM62022/vm/filepath.cpp) | Bajo | Bajo | Reescritura POSIX |
| [sync.cpp](file:///c:/sw/XM62022/vm/sync.cpp) | Bajo | Bajo | `std::mutex` |
| [core_asm.asm](file:///c:/sw/XM62022/vm/core_asm.asm) / [rend_asm.asm](file:///c:/sw/XM62022/vm/rend_asm.asm) | Medio | Medio | NASM o reescritura en C |
| [star.asm](file:///c:/sw/XM62022/cpu/star.asm) (CPU core) | Alto | Alto | Evaluar [Star.c](file:///c:/sw/XM62022/cpu/Star.c), alternativamente Musashi |
| [windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp) | Alto | Medio | Reescritura POSIX completa |
| Toda la capa `mfc/` (UI, render, audio, input) | **Muy Alto** | **Alto** | SDL2 + reescritura completa |
| Shaders HLSL → GLSL | Bajo | Bajo | Conversión manual |
| Sistema de build | Bajo | Bajo | CMake |

---

## Conclusión

El porteo a Linux es **técnicamente factible** pero constituye un esfuerzo de **ingeniería inversa e integración significativo**. La capa VM es razonablemente limpia conceptualmente, pero la capa host MFC/DirectX es un monolito que requiere reescritura completa.

> [!CAUTION]
> No es una tarea de "ajustes menores". Estamos hablando de reescribir ~50-60% del código fuente, con el riesgo inherente de introducir bugs de sincronización, timing y rendering que son difíciles de depurar en un emulador.

La decisión pragmática sería usar **SDL2** como framework base por su excelente soporte multi-plataforma, lo que permitiría que el resultado funcione tanto en Linux como en Windows, eliminando la dependencia de MFC para futuras versiones.
