# XM6 - Emulador X68000 (versión 2.05)

## Código fuente para Win32
**Copyright (C) 2001-2006 Ytanaka**

---

## Sobre la publicación del código fuente

Este archivo contiene el código fuente completo del emulador de X68000 XM6 (versión 2.05).

XM6, hasta la versión 1.95, mantuvo todo su código fuente cerrado. Esto abarcaba la totalidad del programa, y se consideró que publicar partes importantes antes de que el emulador alcanzara cierto nivel de madurez no sería beneficioso para nadie.

En 2005, con la llegada de la versión 2.00, el diseño general finalmente tomó forma, se corrigieron varios errores de diseño menores, y el emulador llegó a un punto en que era posible publicar el código fuente.

El X68000 cuenta con una valiosa documentación técnica, comenzando por el libro *"Inside X68000"*, entre otros materiales. Sin embargo, en la actualidad, en el año 2006, han pasado ya 19 años desde el lanzamiento del X68000 en 1987. El mencionado libro *Inside* ya está fuera de impresión, las hojas de datos de cada dispositivo se vuelven cada vez más difíciles de conseguir, y lamentablemente la información disponible se está volviendo escasa.

Se espera que este código fuente sirva de ayuda en el desarrollo de hardware, software y aplicaciones relacionadas con el X68000. También puede ser útil como referencia para quienes deseen desarrollar nuevos emuladores.

---

## Condiciones de uso (Licencia)

Sobre los archivos incluidos en el archivo comprimido, a excepción de algunas partes indicadas más adelante, los derechos de autor pertenecen a Ytanaka.

Cuando se reutilice parte o la totalidad de los archivos fuente, deberán seguirse las siguientes reglas:

- Al reutilizar archivos del directorio `vm`, deberá indicarse claramente el aviso de derechos de autor originales en la documentación. Queda prohibido el uso comercial.
- La reutilización de archivos de otras partes es libre, aunque también queda prohibido el uso comercial.

> **No está permitida la redistribución de este archivo ni su copia o transferencia a otros medios.**

---

## Excepciones a las condiciones de uso

Los siguientes archivos son derivados del emulador de CPU 68000 **"Starscream"**. El autor original es **Neill Corlett** (neill@neillcorlett.com). Las reglas anteriores no se aplican a estas partes; se rigen por la licencia de Starscream.

```
cpu\star.c
vm\cpudebug.c
vm\cpudebug.h
```

Los siguientes archivos son derivados del emulador de sonido FM **"fmgen"**. El autor original es **cisc** (cisc@retropc.net). Las reglas anteriores no se aplican a estas partes; se rigen por la licencia de fmgen.

```
vm\fmgen.cpp
vm\fmgen.h
vm\fmgeninl.h
vm\fmtimer.cpp
vm\fmtimer.h
vm\opm.cpp
vm\opm.h
```

Los siguientes archivos son obra de **co** (cogood@gmail.com). Aunque técnicamente se trata de una incorporación experimental en esta versión, se tratan como parte integral de XM6.

```
vm\windrv.cpp
vm\windrv.h
mfc\mfc_host.cpp
mfc\mfc_host.h
```

---

## Entorno de desarrollo

Como compilador se requiere **Microsoft Visual C++ .NET 2003**. Se utiliza MFC 7.1, incluido en VC++ .NET 2003, como framework de aplicación.

También es posible que funcione con la versión anterior Visual C++ 6 o con la versión más reciente Visual C++ .NET 2005, aunque el autor no lo ha verificado personalmente.

Como ensamblador se requiere **NASM (Netwide Assembler)**, que se distribuye por separado. Se recomienda la versión 0.98.39 para Win32.
(URL de referencia: http://sourceforge.net/projects/nasm/)

---

## Estructura de directorios

La estructura de directorios es la siguiente:

| Directorio | Contenido |
|---|---|
| `cpu` | Núcleo de CPU (Starscream) |
| `vm` | X68000 virtual |
| `mfc` | Capa I/O (Win32) |
| `res` | Recursos Win32 |
| `00vcproj.vc6` | Archivos de proyecto para Visual C++ 6 |
| `00vcproj.vc7` | Archivos de proyecto para Visual C++ .NET 2003 |

---

## Símbolos predefinidos

Los siguientes símbolos se utilizan en todo el proyecto:

| Símbolo | Descripción |
|---|---|
| `_DEBUG` | Compilación en modo debug. Se realizan comprobaciones automáticas mediante `assert`. |
| `NDEBUG` | Compilación en modo release. Normalmente se define así. |
| `_WIN32` | Declaración de entorno Win32. (Se define automáticamente por el compilador.) |

Los siguientes símbolos se declaran en `xm6.h`:

| Símbolo | Descripción |
|---|---|
| `FALSE` | 0 |
| `TRUE` | (!FALSE) |
| `NULL` | 0 |
| `ASSERT` | En compilación `_DEBUG`, activa `assert`. |
| `FASTCALL` | Convención de llamada a funciones por registro. |

---

## Compilación

Por gentileza de Iwamori, se han preparado archivos de proyecto tanto para VC++6 como para VC++.NET.

Abra Visual C++ 6 o .NET y cargue `XM6.dsw` (VC++6) o `XM6.sln` (VC++.NET).

Seleccione la configuración activa entre "Release" o "Debug", y ejecute la compilación o la compilación de la solución.

Al compilar y enlazar, se generará `XM6.exe` en el directorio Release o Debug, respectivamente. Con esto la compilación estará completa.

> **Nota:** En el caso de la compilación Release con VC++.NET, se generarán archivos `.asm` correspondientes a los fuentes en C++. Esto se debe a la configuración del proyecto.

---

## Guía del código fuente

A continuación se describe brevemente la arquitectura y la estructura del programa de XM6.

XM6 está dividido en dos partes: la **máquina virtual X68000** (llamada VM) y la **capa Win32** que gestiona toda la emulación de I/O real. Esta separación es explícita y clara: la VM describe la máquina virtual de forma dinámica y descriptiva, mientras que la capa Win32 actúa como intermediaria entre la VM y los dispositivos físicos del sistema Windows. Además, dado que sirve como espacio compartido entre ambas, la VM abstrae los conceptos dependientes del sistema operativo como el I/O de archivos y las rutas de archivos. Por este motivo, existe una clase `Sync` como envoltura para los objetos de sincronización entre hilos, que son dependientes del sistema operativo.

Por estas razones, la VM está escrita en C++ genérico e independiente del procesador y del sistema operativo. Sin embargo, debido a que el núcleo de CPU Starscream es específico para x86 (IA-32), hay partes que son llamadas con cierta complejidad, y dado que parte del código está en ensamblador, al migrar a otro compilador, procesador o sistema operativo será necesario algún tipo de inicialización en tres archivos: `cpu`, `memory` y `schedule`.

Cada dispositivo implementado dentro de la VM hereda de la clase `Device` o de la clase `MemDevice`. Sus instancias son mantenidas por la clase `VM`, y es posible buscar y obtener cada dispositivo a través de `VM::SearchDevice`.

### Archivos de la VM

| Archivo | Contenido |
|---|---|
| `os.h` | Cabecera de inclusión dependiente del SO |
| `xm6.h` | Definiciones comunes de XM6 |
| `adpcm` | ADPCM |
| `areaset` | Area Set |
| `cisc.h` | Cabecera de conexión con fmgen |
| `config.h` | Definición de datos de configuración |
| `core_asm` | Subrutinas en ensamblador |
| `cpu` | MPU |
| `cpudebug` | Desensamblador (incluido con Starscream) |
| `crtc` | Controlador CRT |
| `device` | Dispositivo / mapa de memoria / dispositivo base |
| `disk` | Archivo de imagen de disco SASI/SCSI |
| `dmac` | Controlador DMA |
| `event` | Evento de callback |
| `fdc` | Controlador de disquete |
| `fdd` | Unidad de disquete |
| `fdi` | Archivo de imagen de disquete |
| `fileio` | I/O de archivos (independiente de Win32) |
| `filepath` | Ruta de archivos (independiente de Win32) |
| `fmgen` | Generador de sonido FM (fmgen) |
| `fmgeninl` | Generador de sonido FM (fmgen) |
| `fmtimer` | Temporizador FM (fmgen) (actualmente sin uso) |
| `gvram` | VRAM gráfica |
| `iosc` | Controlador I/O |
| `keyboard` | Teclado |
| `log` | Registro (log) |
| `memory` | Gestión de memoria completa / mapeo de memoria |
| `mercury` | Mercury-Unit |
| `mfp` | MFP |
| `midi` | MIDI |
| `mouse` | Ratón |
| `neptune` | Neptune-X |
| `opm` | YM2151 (fmgen) |
| `opmif` | Interfaz de sonido FM |
| `ppi` | Puerto paralelo / dispositivo joystick |
| `printer` | Impresora |
| `render` | Renderizador (composición de pantalla) |
| `renderin` | Renderizador inline |
| `rend_asm` | Renderizador en ensamblador |
| `rtc` | Reloj en tiempo real |
| `sasi` | Controlador SASI |
| `scc` | Comunicación en serie |
| `schedule` | Planificador (scheduler) |
| `scsi` | Controlador SCSI |
| `sprite` | Controlador BG/Sprite |
| `sram` | SRAM de respaldo de batería |
| `starcpu.h` | Conexión con Starscream |
| `sync` | Objeto de sincronización de hilos (independiente de Win32) |
| `sysport` | Puerto del sistema |
| `tvram` | VRAM de texto |
| `vc` | Controlador de vídeo |
| `vm` | Cuerpo principal de la máquina virtual |
| `windrv` | WindrvXM (WINDRV) |

> En esta versión, las clases Mercury y Neptune son implementaciones ficticias (dummy). Su implementación completa es una posibilidad futura. También existe la posibilidad de refactorizar el CRTC, el GVRAM y el Sprite.

---

La capa Win32 utiliza **MFC (Microsoft Foundation Class Library)** como framework de aplicación. No se utilizan documentos ni plantillas de documento, pero hay una fuerte dependencia de MFC en otros aspectos, por lo que la migración a otro entorno es difícil. Se recomienda usarlo como referencia para el funcionamiento de la VM y reescribirlo desde cero para otros entornos. También hay partes del código fuente que reutilizan especificaciones de código abierto.

Los módulos que operan en Win32 heredan en parte de la clase `CComponent`. Sus instancias están conectadas entre sí mediante una lista doble enlazada, por lo que es posible buscar y obtener cada componente a través de `CComponent::SearchComponent`.

### Archivos de la capa Win32

| Archivo | Contenido |
|---|---|
| `mfc.h` | Definiciones comunes de Win32 |
| `mfc_app` | Aplicación y variables globales |
| `mfc_asm` | Subrutinas en ensamblador |
| `mfc_cfg` | Gestión de datos de configuración / ajuste de opciones |
| `mfc_cmd` | Manejo de comandos de menú |
| `mfc_com` | Gestión de componentes |
| `mfc_cpu` | Subventana (procesador) |
| `mfc_dev` | Subventana (dispositivos) |
| `mfc_draw` | Ventana de dibujo y gestión de subventanas |
| `mfc_frm` | Ventana principal y control general |
| `mfc_host` | Sistema de archivos del host (WindrvXM) |
| `mfc_info` | Gestión del display de la barra de estado |
| `mfc_inp` | Driver de entrada |
| `mfc_midi` | Driver MIDI |
| `mfc_port` | Driver de puerto serie y puerto paralelo |
| `mfc_que` | Cola genérica |
| `mfc_rend` | Subventana (renderizador) |
| `mfc_res` | Recursos |
| `mfc_sch` | Planificador |
| `mfc_snd` | Driver de sonido |
| `mfc_stat` | Visor de estado (solo para pantalla completa) |
| `mfc_sub` | Control de subventanas |
| `mfc_sys` | Subventana (sistema) |
| `mfc_tkey` | TrueKey |
| `mfc_tool` | Creación de imágenes de disco, etc. |
| `mfc_ver` | Información de versión |
| `mfc_vid` | Subventana (vídeo) |
| `mfc_w32` | Subventana (Win32) |

### Flujo de procesamiento

El flujo de procesamiento real es el siguiente. Esta parte depende de la plataforma, por lo que será más fácil de entender si se tiene conocimiento de MFC.

1. `CApp::InitInstance()` — Inicialización de la instancia
2. `CFrmWnd::Init()` — Creación de la ventana principal
3. `CFrmWnd::OnCreate()` — Inicialización de la ventana principal y la ventana de dibujo / Construcción, inicialización y reset de la VM / Construcción e inicialización de los componentes / Post del mensaje de usuario `WM_KICK`
4. `CFrmWnd::OnKick()` — Procesamiento según el código de estado / Inicio del bucle de mensajes y del caption/barra de estado
5. `CScheduler::Run()` — Cuando OnKick llama a `CScheduler::Enable(TRUE)`, se inicia el hilo de la VM, que llama a `vm->Exec` según sea necesario.

---

## Contacto

- **Página web (Ytanaka Home Page):** http://www.ipc-tokai.or.jp/~ytanaka/
- **Foro de discusión:** http://cgi.ipc-tokai.or.jp/~ytanaka/cgi-bin/xm6/hyperbbs.cgi
- **Correo electrónico:** ytanaka@ipc-tokai.or.jp
