# XM6 vs px68k-libretro: Scrolling/Raster Effects Architecture

## Overview

Both projects emulate the Sharp X68000's video hardware, but with different architectures:

| Aspect | XM6 (this project) | px68k-libretro |
|--------|-------------------|----------------|
| Language | C++ | C |
| UI Layer | MFC (Windows) | libretro (cross-platform) |
| Video Modules | render.cpp, crtc.cpp, sprite.cpp, gvram.cpp, tvram.cpp, bg.cpp | bg.c, crtc.c, gvram.c, tvram.c |
| Architecture | Object-oriented device system | Flat C functions |

---

## XM6 Scrolling/Raster Architecture

### 1. Video Modules

**Core Components:**
- [`vm/render.cpp`](vm/render.cpp) - Main rendering pipeline
- [`vm/crtc.cpp`](vm/crtc.cpp) - CRT Controller (timing, raster interrupts)
- [`vm/sprite.cpp`](vm/sprite.cpp) - Sprite and BG controller
- [`vm/gvram.cpp`](vm/gvram.cpp) - Graphics VRAM
- [`vm/tvram.cpp`](vm/tvram.cpp) - Text VRAM
- [`vm/vc.cpp`](vm/vc.cpp) - Video Controller

### 2. Scrolling Implementation

**Text Layer Scrolling** ([`render.cpp:1617`](vm/render.cpp:1617)):
```cpp
void FASTCALL Render::TextScrl(DWORD x, DWORD y)
```
- Updates `render.textx` and `render.texty`
- Sets dirty flags for all 1024 raster lines when scroll changes

**Graphics Layer Scrolling** ([`render.cpp:1758`](vm/render.cpp:1758)):
```cpp
void FASTCALL Render::GrpScrl(int block, DWORD x, DWORD y)
```
- 4 independent graphics layers (block 0-3)
- Each layer has separate X/Y scroll registers
- Offset calculated per-raster: `offset = (raster + render.grpy[block]) & 0x3ff`

**BG Layer Scrolling** ([`render.cpp:2292`](vm/render.cpp:2292)):
```cpp
void FASTCALL Render::BGScrl(int page, DWORD x, DWORD y)
```
- 2 BG layers with 1024x1024 scroll space
- Controlled by sprite.cpp via `render->BGScrl()`

### 3. Raster Effects

**CRTC Raster Interrupt** ([`crtc.cpp`](vm/crtc.cpp)):
```cpp
crtc.raster_count     // Current raster line
crtc.raster_int      // Interrupt position
crtc.raster_copy     // Raster copy flag
crtc.raster_exec     // Raster copy execution flag
```

**Raster Copy** - Hardware-based line copy during display:
- Enabled via CRTC register write (bit 3 of address 0xE8)
- When `crtc.raster_copy && crtc.raster_exec` is true:
  ```cpp
  tvram->RasterCopy();  // Copy source line to destination
  crtc.raster_exec = FALSE;
  ```

**HSync (Raster Line Processing)** ([`render.cpp:719`](vm/render.cpp:719)):
```cpp
void FASTCALL Render::HSyncOriginal(int raster)
```
- Called every horizontal sync by CRTC
- Processes text, graphics, sprites per raster line
- Handles scroll offset application

### 4. Display Pipeline

**MFC Connection** ([`mfc/mfc_rend.cpp`](mfc/mfc_rend.cpp)):
```cpp
// Get buffer pointers from render
m_pRendBuf = render->GetTextBuf();    // Text layer
m_pRendBuf = render->GetGrpBuf(0-3);  // Graphics layers
m_pRendBuf = render->GetBGSpBuf();    // BG/Sprite composite

// Access render work structure
m_pRendWork = render->GetWorkAddr();  // Full render state
```

**Flow:**
```
CRTC (timing/raster) 
    → calls render->HSync(raster) 
    → processes each layer (Text, Grp0-3, Sprite, BG)
    → writes to internal buffers (textout, grpbuf[], bgspbuf)
    
MFC UI thread
    → calls render->GetTextBuf(), GetGrpBuf(), etc.
    → copies to display via GDI/DirectX
```

---

## px68k-libretro Scrolling/Raster Architecture

### 1. Video Modules

**Core Components:**
- [`x68k/bg.c`](C:/sw/px68k-libretro-master/x68k/bg.c) - BG and Sprite rendering
- [`x68k/crtc.c`](C:/sw/px68k-libretro-master/x68k/crtc.c) - CRT Controller
- [`x68k/gvram.c`](C:/sw/px68k-libretro-master/x68k/gvram.c) - Graphics VRAM
- [`x68k/tvram.c`](C:/sw/px68k-libretro-master/x68k/tvram.c) - Text VRAM

### 2. Scrolling Implementation

**BG Scroll Registers** ([`bg.c:28-29`](C:/sw/px68k-libretro-master/x68k/bg.c:28)):
```c
static uint32_t BG0ScrollX = 0, BG0ScrollY = 0;
static uint32_t BG1ScrollX = 0, BG1ScrollY = 0;
```

**Scroll Register Updates** ([`bg.c:160-178`](C:/sw/px68k-libretro-master/x68k/bg.c:160)):
```c
case 0x00:
case 0x01:
    BG0ScrollX = (((uint32_t)BG_Regs[0x00]<<8)+BG_Regs[0x01])&BG_AdrMask;
    TVRAM_SetAllDirty();
    break;
case 0x02:
case 0x03:
    BG0ScrollY = (((uint32_t)BG_Regs[0x02]<<8)+BG_Regs[0x03])&BG_AdrMask;
    TVRAM_SetAllDirty();
    break;
// ... BG1 similarly
```

**Scroll Applied Per-Line** ([`bg.c:373-409`](C:/sw/px68k-libretro-master/x68k/bg.c:373)):
```c
static void bg_drawline_loopx8(uint16_t BGTOP, uint32_t BGScrollX, uint32_t BGScrollY, ...)
{
    // Scroll offset calculated per raster
    uint32_t ebp = ((BGScrollY + VLINEBG - BG_VLINE) & 7) << 3;
    uint32_t edx = BGTOP + (((BGScrollY + VLINEBG - BG_VLINE) & 0x1f8) << 4);
    uint32_t edi = ((BGScrollX - adjust) & 7) ^ 15;
}
```

### 3. Raster Effects

**Raster Copy** ([`crtc.c:350-351`](C:/sw/px68k-libretro-master/x68k/crtc.c:350)):
```c
case 0x2c:  /* Turn on the raster copy of the CRTC operation port */
case 0x2d:  /* Change only Src/Dst (like Dracula) */
```

**Raster Line Tracking** ([`bg.c:31-32,40`](C:/sw/px68k-libretro-master/x68k/bg.c:31)):
```c
int32_t BG_HAdjust = 0;  // Horizontal adjustment
int32_t BG_VLINE = 0;    // Vertical line offset
uint32_t VLINEBG = 0;    // Current raster line
```

### 4. Display Pipeline

**libretro Connection** ([`libretro.c:544`](C:/sw/px68k-libretro-master/libretro.c:544)):
```c
static retro_video_refresh_t video_cb;

void retro_set_video_refresh(retro_video_refresh_t cb) { 
    video_cb = cb; 
}

// Called with framebuffer:
video_cb(buffer, width, height, pitch);
```

**Flow:**
```
CRTC (timing)
    → calls BG_DrawLine() per raster
    → bg.c processes sprites + BG0 + BG1
    → writes to BG_LineBuf[]
    
libretro frontend
    → receives pixel data via video_cb()
    → outputs to libretro display
```

---

## Comparison Summary

| Feature | XM6 | px68k-libretro |
|---------|-----|----------------|
| **Graphics Layers** | 4 (Grp0-3) + Text + BG(2) | 2 BG + Sprites + Text |
| **Scroll Registers** | Per-layer X/Y in CRTC | BG0/BG1 X/Y in BG regs |
| **Raster Effects** | CRTC raster copy, interrupts | Similar raster copy |
| **Per-line Processing** | HSync() called by CRTC | BG_DrawLine() called by CRTC |
| **Display Output** | MFC window buffers | libretro video_cb |
| **Buffer Access** | GetTextBuf(), GetGrpBuf() | Direct BG_LineBuf[] |

### Key Architectural Differences

1. **XM6** uses a device-based OOP architecture where CRTC triggers render callbacks per raster line
2. **px68k-libretro** uses a more direct approach with BG_DrawLine() called per-scanline
3. **XM6** separates render (composition) from display (MFC windows)
4. **px68k-libretro** integrates rendering directly with libretro callback system
