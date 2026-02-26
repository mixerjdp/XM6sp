//---------------------------------------------------------------------------
//
//	EMULADOR X68000 "XM6"
//
//	Copyright (C) 2001-2006 ÇoÇhÅD(ytanaka@ipc-tokai.or.jp)
//	[ Sub-ensamblador MFC ]
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
//	Declaracion de prototipos
//
//---------------------------------------------------------------------------
BOOL IsMMXSupport(void);
										// Verificacion de soporte MMX
BOOL IsCMOVSupport(void);
										// Verificacion de soporte CMOV

void SoundMMX(DWORD *pSrc, WORD *pDst, int nBytes);
										// Redimensionamiento de muestras de sonido (MMX)
void SoundEMMS();
										// Redimensionamiento de muestras de sonido (EMMS)

void VideoText(const BYTE *pTVRAM, DWORD *pBits, int nLen, DWORD *pPalette);
										// VRAM de texto
void VideoG1024A(const BYTE *src, DWORD *dst, DWORD *plt);
										// VRAM grafica 1024x1024 (Pagina 0,1)
void VideoG1024B(const BYTE *src, DWORD *dst, DWORD *plt);
										// VRAM grafica 1024x1024 (Pagina 2,3)
void VideoG16A(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 16 colores (Pagina 0)
void VideoG16B(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 16 colores (Pagina 1)
void VideoG16C(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 16 colores (Pagina 2)
void VideoG16D(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 16 colores (Pagina 3)
void VideoG256A(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 256 colores (Pagina 0)
void VideoG256B(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 256 colores (Pagina 1)
void VideoG64K(const BYTE *src, DWORD *dst, int len, DWORD *plt);
										// VRAM grafica 65536 colores
void VideoPCG(BYTE *src, DWORD *dst, DWORD *plt);
										// PCG
void VideoBG16(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt);
										// BG 16x16
void VideoBG8(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt);
										// BG 8x8

#if defined(__cplusplus)
}
#endif	//__cplusplus

#endif	// mfc_asm_h
#endif	// _WIN32
