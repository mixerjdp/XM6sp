//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Copyright (C) 2001-2006 Ytanaka (ytanaka@ipc-tokai.or.jp)
//	[MFC sub-assembler]
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#if !defined (mfc_asm_h)
#define mfc_asm_h

#if defined(__cplusplus)
extern "C" {
#endif	//__cplusplus

//---------------------------------------------------------------------------
//
//	Prototype declarations
//
//---------------------------------------------------------------------------
BOOL IsMMXSupport(void);
										// MMX support check
BOOL IsCMOVSupport(void);
										// CMOV support check

// Portable C/C++ replacements (used selectively during the ASM migration)
BOOL IsMMXSupportPortable(void);
BOOL IsCMOVSupportPortable(void);
void SoundMMXPortable(DWORD *pSrc, WORD *pDst, int nBytes);
void SoundEMMSPortable(void);

void SoundMMX(DWORD *pSrc, WORD *pDst, int nBytes);
										// Resample audio samples (MMX)
void SoundEMMS();
										// Resample audio samples (EMMS)

void VideoText(const BYTE *pTVRAM, DWORD *pBits, int nLen, DWORD *pPalette);
										// Text VRAM
void VideoG1024A(const BYTE *src, DWORD *dst, DWORD *plt);
										// Graphics VRAM 1024x1024 (pages 0,1)
void VideoG1024B(const BYTE *src, DWORD *dst, DWORD *plt);
										// Graphics VRAM 1024x1024 (pages 2,3)
void VideoG16A(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 16 colors (page 0)
void VideoG16B(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 16 colors (page 1)
void VideoG16C(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 16 colors (page 2)
void VideoG16D(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 16 colors (page 3)
void VideoG256A(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 256 colors (page 0)
void VideoG256B(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 256 colors (page 1)
void VideoG64K(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// Graphics VRAM 65536 colors
void VideoPCG(BYTE *src, DWORD *dst, DWORD *plt);
										// PCG
void VideoBG16(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt);
										// Background 16x16
void VideoBG8(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt);
										// Background 8x8

#if defined(__cplusplus)
}
#endif	//__cplusplus

#endif	// mfc_asm_h
#endif	// _WIN32
