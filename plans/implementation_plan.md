# Plan de Portabilidad Incremental: XM6sp → Linux (Clang ≥3.5 / G++ ≥4.9)

## Contexto y Restricciones del Compilador

| Compilador | C++ Estándar | Notas |
|:-----------|:-------------|:------|
| G++ 4.9 | C++11 completo, C++14 parcial | **Sin** `<filesystem>` (requiere GCC 8+) |
| Clang 3.5 | C++11 completo, C++14 parcial | **Sin** `<filesystem>` |
| Ambos | No soportan `__fastcall` de MSVC | Se define como vacío |

> [!IMPORTANT]
> Al targetear G++ 4.9 / Clang 3.5, estamos limitados a **C++11**. No podemos usar `std::filesystem`, `std::optional`, `string_view`, ni otras facilidades de C++17. Las rutas de archivos se implementarán con wrappers POSIX (`<dirent.h>`, `<sys/stat.h>`, `<unistd.h>`).

## Principio Fundamental: Compilación Dual

La estrategia clave es **nunca romper la build de Windows** mientras se agrega soporte Linux. Esto se logra mediante:

```cpp
#if defined(_WIN32)
    // Implementación Windows existente (intacta)
#elif defined(__linux__)
    // Nueva implementación Linux
#endif
```

Cada fase produce un binario compilable — ya sea solo la VM como librería estática, o el emulador completo.

---

## Orden de Conversión (Bottom-Up)

```mermaid
graph BT
    P1["Fase 1: compat.h<br/>Tipos y macros portables"]
    P2["Fase 2: fileio / filepath / sync<br/>I/O y sincronización POSIX"]
    P3["Fase 3: CPU Core<br/>Star.c o Musashi"]
    P4["Fase 4: VM como librería<br/>Compilar vm/ sin mfc/"]
    P5["Fase 5: Frontend SDL2<br/>Ventana, Render, Audio, Input"]
    P6["Fase 6: Integración<br/>windrv, MIDI, config"]

    P1 --> P2 --> P3 --> P4 --> P5 --> P6

    style P1 fill:#2d5016,stroke:#4a8c28,color:#fff
    style P2 fill:#2d5016,stroke:#4a8c28,color:#fff
    style P3 fill:#8c6b0a,stroke:#d4a017,color:#fff
    style P4 fill:#8c6b0a,stroke:#d4a017,color:#fff
    style P5 fill:#8c1a0a,stroke:#d42017,color:#fff
    style P6 fill:#8c1a0a,stroke:#d42017,color:#fff
```

**Verde** = esfuerzo bajo, resultados rápidos. **Amarillo** = esfuerzo medio, riesgos técnicos. **Rojo** = esfuerzo alto, reescritura significativa.

---

## Fase 1: Header de Compatibilidad (`compat.h`)

**Objetivo:** Crear un header que mapee los tipos y macros Win32 a equivalentes portables sin modificar ningún archivo existente de la VM.

#### [NEW] [compat.h](file:///c:/sw/XM62022/vm/compat.h)

```cpp
#ifndef COMPAT_H
#define COMPAT_H

#if defined(_WIN32)
    // Windows: usar definiciones originales de mfc.h
    #include "mfc.h"
#elif defined(__linux__) || defined(__APPLE__)

    #include <cstdint>
    #include <cstring>
    #include <cassert>
    #include <climits>    // PATH_MAX

    // Tipos Win32 → estándar
    typedef int             BOOL;
    typedef uint32_t        DWORD;
    typedef uint16_t        WORD;
    typedef uint8_t         BYTE;
    typedef long            LONG;
    typedef int32_t         LONG32;
    typedef unsigned int    UINT;
    typedef const char*     LPCTSTR;
    typedef char*           LPTSTR;
    typedef char            TCHAR;

    #ifndef FALSE
    #define FALSE   0
    #endif
    #ifndef TRUE
    #define TRUE    1
    #endif

    // _T() macro — en Linux siempre char (UTF-8)
    #define _T(x)   x

    // FASTCALL — irrelevante en Linux x86-64 (ABI usa registros por defecto)
    #define FASTCALL

    // Rutas
    #define _MAX_PATH   PATH_MAX
    #define _MAX_FNAME  256
    #define _MAX_DIR    256
    #define _MAX_EXT    256
    #define _MAX_DRIVE  3
    #define MAX_PATH    PATH_MAX

    // ASSERT
    #ifndef ASSERT
    #define ASSERT(x) assert(x)
    #endif

    // Funciones MSVC → POSIX
    #define _topen      open
    #define _read       read
    #define _write      write
    #define _close      close
    #define _lseek      lseek
    #define _tell(fd)   lseek(fd, 0, SEEK_CUR)
    #define _tcscpy     strcpy
    #define _tcsncpy    strncpy
    #define _tcslen     strlen
    #define _tcscmp     strcmp
    #define _tcsicmp    strcasecmp
    #define _tcscat     strcat
    #define _stprintf   sprintf
    #define _vstprintf  vsprintf

    #include <unistd.h>
    #include <fcntl.h>
    #include <sys/stat.h>

#endif // __linux__
#endif // COMPAT_H
```

#### [MODIFY] [os.h](file:///c:/sw/XM62022/vm/os.h)

```diff
 #if defined(_WIN32)
 #include "mfc.h"
+#elif defined(__linux__) || defined(__APPLE__)
+#include "compat.h"
 #endif
```

**Verificación Fase 1:** El header debe compilar sin errores en GCC/Clang con un test mínimo:
```bash
echo '#include "compat.h"
int main() { DWORD x = 42; BOOL b = TRUE; ASSERT(b); return 0; }' > test_compat.cpp
g++ -std=c++11 -I./vm -D__linux__ test_compat.cpp -o test_compat && ./test_compat
```

---

## Fase 2: Módulos Portables de I/O y Sincronización

### [MODIFY] [fileio.cpp](file:///c:/sw/XM62022/vm/fileio.cpp)

Agregar bloque `#elif defined(__linux__)` con implementación POSIX:
- `open()` / `read()` / `write()` / `close()` / `lseek()` en lugar de `_topen` etc.
- `fstat()` para `GetFileSize()` en lugar de `_filelengthi64()`
- `lseek(fd, 0, SEEK_CUR)` para `GetFilePos()` en lugar de `_tell()`

### [MODIFY] [filepath.cpp](file:///c:/sw/XM62022/vm/filepath.cpp)

Implementación Linux sin `_splitpath` / `_makepath`:
- Parseo manual de rutas usando `/` como separador
- Sin letras de drive (`m_szDrive` siempre vacío)
- `stat()` para timestamps en lugar de `FILETIME`
- `realpath()` para rutas absolutas

### [MODIFY] [sync.cpp](file:///c:/sw/XM62022/vm/sync.cpp)

Reemplazar `CCriticalSection` con `pthread_mutex_t` (preferido sobre `std::mutex` dado que G++ 4.9 tiene bugs conocidos con `std::mutex` en algunas plataformas):

```cpp
#elif defined(__linux__)
#include <pthread.h>

Sync::Sync() {
    mtx = new pthread_mutex_t;
    pthread_mutex_init(mtx, NULL);
}
Sync::~Sync() {
    pthread_mutex_destroy(mtx);
    delete mtx;
}
void FASTCALL Sync::Lock()   { pthread_mutex_lock(mtx); }
void FASTCALL Sync::Unlock() { pthread_mutex_unlock(mtx); }
#endif
```

### [MODIFY] [sync.h](file:///c:/sw/XM62022/vm/sync.h)

Agregar branch `#elif defined(__linux__)` con `pthread_mutex_t*` en lugar de `CCriticalSection*`.

**Verificación Fase 2:** Compilar cada módulo individualmente como object file:
```bash
g++ -std=c++11 -c -I./vm -D__linux__ vm/fileio.cpp -o fileio.o
g++ -std=c++11 -c -I./vm -D__linux__ vm/filepath.cpp -o filepath.o
g++ -std=c++11 -c -I./vm -D__linux__ -lpthread vm/sync.cpp -o sync.o
```

---

## Fase 3: Núcleo CPU

### Opción A: Usar [Star.c](file:///c:/sw/XM62022/cpu/Star.c) existente (recomendado como primer intento)

[cpu/Star.c](file:///c:/sw/XM62022/cpu/Star.c) (156KB) es una versión C del core Starscream. Ventajas: ya existe, es C portable.

1. Verificar que [Star.c](file:///c:/sw/XM62022/cpu/Star.c) compila con GCC: `gcc -std=c99 -c cpu/Star.c -o star.o`
2. Si hay errores, probablemente sean por `__cdecl` / `__stdcall` u otros MSVC-ismos → parchear con `#ifdef`
3. Verificar que la interfaz ([starcpu.h](file:///c:/sw/XM62022/vm/starcpu.h)) es compatible con el resto de la VM

### Opción B: Reemplazar con Musashi (backup plan)

Si [Star.c](file:///c:/sw/XM62022/cpu/Star.c) no funciona correctamente:
1. Integrar [Musashi](https://github.com/kstenerud/Musashi) — emulador M68000 en C puro
2. Crear un adapter header que mapee la API de Starscream a la de Musashi
3. Riesgo: diferencias sutiles de timing y comportamiento entre implementaciones

### ASM auxiliar ([core_asm.asm](file:///c:/sw/XM62022/vm/core_asm.asm), [rend_asm.asm](file:///c:/sw/XM62022/vm/rend_asm.asm))

Para Linux: **desactivar** las rutinas ASM y usar equivalentes C.
- [rend_asm.asm](file:///c:/sw/XM62022/vm/rend_asm.asm) (119KB) contiene rutinas optimizadas de rendering → reimplementar en C los hot paths o convertir a NASM
- [core_asm.asm](file:///c:/sw/XM62022/vm/core_asm.asm) (7KB) → pequeño, verificar si es prescindible o convertir a C inline ASM con sintaxis GAS

**Verificación Fase 3:**
```bash
gcc -std=c99 -c -I./vm cpu/Star.c -o star.o 2>&1 | head -50
# Si compila: linkear con un test mínimo que inicialice el contexto s68000
```

---

## Fase 4: Compilar la VM como Librería Estática

**Objetivo:** Compilar todos los [.cpp](file:///c:/sw/XM62022/vm/vm.cpp) de `vm/` (excepto [windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp) temporalmente) como una librería estática `libxm6vm.a`.

### [NEW] [CMakeLists.txt](file:///c:/sw/XM62022/CMakeLists.txt)

```cmake
cmake_minimum_required(VERSION 2.8.12)
project(XM6sp)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)

# Archivos VM (sin windrv por ahora)
file(GLOB VM_SOURCES
    vm/*.cpp
)
list(REMOVE_ITEM VM_SOURCES
    ${CMAKE_SOURCE_DIR}/vm/windrv.cpp  # Fase 6
)

# CPU core
set(CPU_SOURCES cpu/Star.c)

# Librería estática de la VM
add_library(xm6vm STATIC ${VM_SOURCES} ${CPU_SOURCES})
target_include_directories(xm6vm PUBLIC vm/)
target_compile_definitions(xm6vm PRIVATE $<$<PLATFORM_ID:Linux>:__linux__>)
if(UNIX)
    target_link_libraries(xm6vm pthread)
endif()
```

**Verificación Fase 4:**
```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=g++
make -j$(nproc) 2>&1 | tee build_log.txt
# Contar errores y resolverlos iterativamente
```

> [!WARNING]
> Esta fase será la más iterativa. Es probable que aparezcan 50-100+ errores de compilación derivados de includes transitivos, tipos faltantes, y funciones Win32 que se colaron en la capa VM. Cada error se resuelve con un `#ifdef` adicional en `compat.h` o en los archivos individuales.

---

## Fase 5: Frontend SDL2

**Objetivo:** Crear un frontend mínimo funcional usando SDL2 que reemplace la capa `mfc/`.

### Dependencias
```bash
sudo apt-get install libsdl2-dev libsdl2-ttf-dev
```

### [NEW] [sdl/sdl_main.cpp](file:///c:/sw/XM62022/sdl/sdl_main.cpp)
Entry point Linux: `main()` → SDL_Init → crear ventana → crear VM → bucle principal

### [NEW] [sdl/sdl_render.cpp](file:///c:/sw/XM62022/sdl/sdl_render.cpp)
Rendering: SDL_Texture para mostrar el framebuffer de la VM

### [NEW] [sdl/sdl_audio.cpp](file:///c:/sw/XM62022/sdl/sdl_audio.cpp)
Audio: SDL_AudioSpec con callback para alimentar samples desde la VM

### [NEW] [sdl/sdl_input.cpp](file:///c:/sw/XM62022/sdl/sdl_input.cpp)
Input: SDL_Event → mapeo a teclas X68000 y joystick

### [NEW] [sdl/sdl_config.cpp](file:///c:/sw/XM62022/sdl/sdl_config.cpp)
Configuración: parser INI portable (sin Win32 API)

### Arquitectura del frontend SDL2

```mermaid
graph LR
    subgraph "sdl_main.cpp"
        INIT["SDL_Init()"]
        LOOP["Main Loop:<br/>PollEvents → VM.Exec() → Present"]
    end

    subgraph "sdl_render.cpp"
        TEX["SDL_Texture<br/>(framebuffer VM)"]
        REND["SDL_Renderer<br/>Present()"]
    end

    subgraph "sdl_audio.cpp"
        ACALLBACK["SDL Audio Callback<br/>Leer samples de OPM/ADPCM"]
    end

    subgraph "sdl_input.cpp"
        KEYS["SDL_Event → Keyboard VM"]
        JOY["SDL_GameController → Joystick VM"]
    end

    subgraph "VM (libxm6vm.a)"
        VM_CORE["VM::Exec()"]
        RENDER["Render Buffer"]
        OPM["OPM/ADPCM Audio"]
    end

    INIT --> LOOP
    LOOP --> VM_CORE
    VM_CORE --> RENDER --> TEX --> REND
    VM_CORE --> OPM --> ACALLBACK
    LOOP --> KEYS
    LOOP --> JOY
```

**Verificación Fase 5:**
```bash
cmake .. -DCMAKE_CXX_COMPILER=g++ -DSDL2_FRONTEND=ON
make -j$(nproc)
./xm6sp  # Debe abrir una ventana SDL2 y arrancar la VM
```

---

## Fase 6: Adaptaciones Específicas

### [windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp) → POSIX filesystem bridge
- `FindFirstFile`/`FindNextFile` → `opendir()`/`readdir()`
- `CreateFile` → `open()`
- `GetFileAttributes` → `stat()`
- `FILETIME` → `struct timespec`
- Mapeo de paths Win32 (`\`) → POSIX (`/`)

### MIDI
- Win32 MIDI API → RtMidi o ALSA sequencer
- O desactivar con `#ifdef` inicialmente

### Shaders
- HLSL ([crt.hlsl](file:///c:/sw/XM62022/crt.hlsl)) → GLSL si se usa OpenGL
- O simplemente desactivar shaders en la versión SDL2 Renderer inicial

---

## Resumen del Orden

| Fase | Qué se modifica | Qué se obtiene | Esfuerzo |
|:-----|:----------------|:---------------|:---------|
| 1 | `compat.h` (nuevo), [os.h](file:///c:/sw/XM62022/vm/os.h) | Tipos portables compilables | 1-2 días |
| 2 | [fileio.cpp](file:///c:/sw/XM62022/vm/fileio.cpp), [filepath.cpp](file:///c:/sw/XM62022/vm/filepath.cpp), `sync.cpp/h` | I/O y sync portable | 2-3 días |
| 3 | [Star.c](file:///c:/sw/XM62022/cpu/Star.c) o Musashi, evaluar ASM | CPU core compilable en GCC | 2-5 días |
| 4 | CMakeLists.txt, fixes iterativos en VM | `libxm6vm.a` compilable | 1-2 semanas |
| 5 | `sdl/` (nuevo directorio completo) | Emulador funcional en Linux | 3-4 semanas |
| 6 | [windrv.cpp](file:///c:/sw/XM62022/vm/windrv.cpp), MIDI, shaders | Funcionalidad completa | 2-3 semanas |

**Estimación total: 2-3 meses** para un desarrollador dedicado.

---

## Verificación General

> [!NOTE]
> No existen tests unitarios en el proyecto actual. La verificación se basa en:

1. **Compilación limpia:** 0 errores, mínimas warnings en cada fase
2. **Test funcional (Fase 5+):** Cargar una ROM del X68000 y verificar que arranca Human68k
3. **Comparación visual:** Capturar screenshots del emulador en Windows y Linux, comparar rendering
4. **Audio:** Verificar que se reproduce sonido FM/ADPCM sin glitches

### Tests manuales propuestos
- **Fase 1-4:** `cmake --build . 2>&1 | grep -c "error:"` → debe ser 0
- **Fase 5:** Ejecutar `./xm6sp` con IPL ROM → debe mostrar la pantalla de boot del X68000
- **Fase 6:** Cargar un disco con juego → verificar audio, input de teclado, y filesystem bridge

## User Review Required

> [!IMPORTANT]
> **Decisión requerida sobre el CPU core:** ¿Prefieres intentar primero con [Star.c](file:///c:/sw/XM62022/cpu/Star.c) (rápido pero incierto) o ir directamente a integrar Musashi (más trabajo pero resultado garantizado)?

> [!IMPORTANT]
> **Sobre G++ 4.9 como mínimo:** Este compilador tiene 12 años. Si pudieras subir el mínimo a G++ 7+ o 8+, ganaríamos acceso a `std::filesystem`, `std::optional`, structured bindings, y mejores optimizaciones. ¿Es un requisito estricto G++ 4.9?
