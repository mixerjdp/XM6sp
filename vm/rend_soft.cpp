#include "os.h"
#include "xm6.h"
#include "rend_asm.h"
#include <string.h>

#ifndef FASTCALL
#define FASTCALL __fastcall
#endif

extern "C" {

void RendTextMem(const BYTE *tvrm, BOOL *flag, BYTE *buf)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i] != FALSE) {
			const BYTE* p0 = tvrm + i * 4;
			const BYTE* p1 = p0 + 0x20000;
			const BYTE* p2 = p0 + 0x40000;
			const BYTE* p3 = p0 + 0x60000;

			DWORD v0 = *(DWORD*)p0;
			DWORD v1 = *(DWORD*)p1;
			DWORD v2 = *(DWORD*)p2;
			DWORD v3 = *(DWORD*)p3;

			// Endianness swap equivalent to `rol reg, 16`
			v0 = (v0 << 16) | (v0 >> 16);
			v1 = (v1 << 16) | (v1 >> 16);
			v2 = (v2 << 16) | (v2 >> 16);
			v3 = (v3 << 16) | (v3 >> 16);

			DWORD* out = (DWORD*)(buf + i * 16);
			for (int chunk = 0; chunk < 4; chunk++) {
				DWORD ebp1 = 0;
				for (int b = 0; b < 8; b++) {
					ebp1 = (ebp1 << 1) | ((v3 >> 31) & 1); v3 <<= 1;
					ebp1 = (ebp1 << 1) | ((v2 >> 31) & 1); v2 <<= 1;
					ebp1 = (ebp1 << 1) | ((v1 >> 31) & 1); v1 <<= 1;
					ebp1 = (ebp1 << 1) | ((v0 >> 31) & 1); v0 <<= 1;
				}
				out[chunk] = ebp1;
			}
		}
	}
}

void RendTextPal(const BYTE *buf, DWORD *out, BOOL *flag, const DWORD *pal)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i] != FALSE) {
			flag[i] = FALSE; // Limpia el flag

			const DWORD* in_dw = (const DWORD*)(buf + i * 16);
			DWORD* out_dw = out + i * 32;

			for (int chunk = 0; chunk < 4; chunk++) {
				DWORD eax = in_dw[chunk];
				for (int p = 0; p < 8; p++) {
					eax = (eax << 4) | (eax >> 28); // equivalente a `rol eax, 4`
					DWORD idx = eax & 15;
					out_dw[chunk * 8 + p] = pal[idx];
				}
			}
		}
	}
}

void RendTextAll(const BYTE *buf, DWORD *out, const DWORD *pal)
{
	for (int i = 0; i < 32; i++) {
		const DWORD* in_dw = (const DWORD*)(buf + i * 16);
		DWORD* out_dw = out + i * 32;

		for (int chunk = 0; chunk < 4; chunk++) {
			DWORD eax = in_dw[chunk];
			for (int p = 0; p < 8; p++) {
				eax = (eax << 4) | (eax >> 28); // equivalente a `rol eax, 4`
				DWORD idx = eax & 15;
				out_dw[chunk * 8 + p] = pal[idx];
			}
		}
	}
}

void RendTextCopy(const BYTE *src, const BYTE *dst_c, DWORD plane, BOOL *textmem, BOOL *textflag)
{
	// Cast away const ya que el ASM modifica dst
	BYTE* dst = (BYTE*)dst_c;
	
	for (int p = 0; p < 4; p++) {
		if (plane & (1 << p)) {
			const BYTE* s = src + p * 0x20000;
			BYTE* d = dst + p * 0x20000;

			BOOL* tmem = textmem;
			BOOL* tflag = textflag;

			for (int line = 0; line < 4; line++) {
				DWORD changed_in_line = 0;
				for (int blk = 0; blk < 4; blk++) {
					for (int dw = 0; dw < 8; dw++) {
						DWORD s_val = ((DWORD*)s)[dw];
						DWORD d_val = ((DWORD*)d)[dw];
						if (s_val != d_val) {
							((DWORD*)d)[dw] = s_val;
							changed_in_line = 1;
							tmem[dw] = TRUE;
						}
					}
					s += 32;
					d += 32;
					tmem += 8;
				}
				if (changed_in_line) {
					*tflag = TRUE;
				}
				tflag++;
			}
		}
	}
}

static void Rend1024Flag(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal, int shift)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD *src = (const DWORD *)(gvram + i * 32);
			DWORD *dst = buf + i * 16;
			for (int j = 0; j < 8; j += 2) {
				DWORD v = src[j + 0] >> shift;
				DWORD p = pal[v & 15];
				dst[j * 2 + 0] = p;
				dst[j * 2 + 0 + 1024] = p;
				p = pal[(v >> 16) & 15];
				dst[j * 2 + 1] = p;
				dst[j * 2 + 1 + 1024] = p;

				v = src[j + 1] >> shift;
				p = pal[v & 15];
				dst[j * 2 + 2] = p;
				dst[j * 2 + 2 + 1024] = p;
				p = pal[(v >> 16) & 15];
				dst[j * 2 + 3] = p;
				dst[j * 2 + 3 + 1024] = p;
			}
		}
	}
}

static int Rend1024All(const BYTE *gvram, DWORD *buf, const DWORD *pal, int shift)
{
	int changed = 0;
	const DWORD *src = (const DWORD *)gvram;
	for (int i = 0; i < 128; i++) {
		DWORD *dst = buf + (i * 4);
		DWORD v = src[i * 2 + 0] >> shift;
		DWORD p = pal[v & 15];
		if (dst[0] != p) {
			changed = 1;
		}
		dst[0] = p;
		dst[0 + 1024] = p;
		p = pal[(v >> 16) & 15];
		if (dst[1] != p) {
			changed = 1;
		}
		dst[1] = p;
		dst[1 + 1024] = p;

		v = src[i * 2 + 1] >> shift;
		p = pal[v & 15];
		if (dst[2] != p) {
			changed = 1;
		}
		dst[2] = p;
		dst[2 + 1024] = p;
		p = pal[(v >> 16) & 15];
		if (dst[3] != p) {
			changed = 1;
		}
		dst[3] = p;
		dst[3 + 1024] = p;
	}
	return changed;
}

int Rend1024A(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend1024All(gvram, buf, pal, 0); }
void Rend1024B(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend1024Flag(gvram, buf, flag, pal, 0); }
int Rend1024C(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend1024All(gvram, buf, pal, 4); }
void Rend1024D(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend1024Flag(gvram, buf, flag, pal, 4); }
int Rend1024E(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend1024All(gvram, buf, pal, 8); }
void Rend1024F(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend1024Flag(gvram, buf, flag, pal, 8); }
int Rend1024G(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend1024All(gvram, buf, pal, 12); }
void Rend1024H(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend1024Flag(gvram, buf, flag, pal, 12); }

// --- 512x512 16-Color Mode (Rend16A - Rend16H) ---

static int Rend16All(const BYTE *gvram, DWORD *buf, const DWORD *pal, int shift)
{
	int changed = 0;
	const DWORD* src_w = (const DWORD*)gvram;
	DWORD* dst_w = buf;
	for (int i = 0; i < 64; i++) {
		for (int j = 0; j < 4; j++) {
			DWORD eax = src_w[i * 4 + j];
			DWORD p0 = pal[(eax >> shift) & 15];
			if (dst_w[i * 8 + j * 2 + 0] != p0) {
				dst_w[i * 8 + j * 2 + 0] = p0;
				dst_w[i * 8 + j * 2 + 0 + 512] = p0;
				changed = 1;
			}
			DWORD p1 = pal[(eax >> (shift + 16)) & 15];
			if (dst_w[i * 8 + j * 2 + 1] != p1) {
				dst_w[i * 8 + j * 2 + 1] = p1;
				dst_w[i * 8 + j * 2 + 1 + 512] = p1;
				changed = 1;
			}
		}
	}
	return changed;
}

static void Rend16Flag(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal, int shift)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = buf + i * 16;
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p0 = pal[(eax >> shift) & 15];
				dst_w[j * 2 + 0] = p0;
				dst_w[j * 2 + 0 + 512] = p0;
				
				DWORD p1 = pal[(eax >> (shift + 16)) & 15];
				dst_w[j * 2 + 1] = p1;
				dst_w[j * 2 + 1 + 512] = p1;
			}
		}
	}
}

int Rend16A(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 0); }
void Rend16B(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 0); }
int Rend16C(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 4); }
void Rend16D(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 4); }
int Rend16E(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 8); }
void Rend16F(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 8); }
int Rend16G(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 12); }
void Rend16H(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 12); }

// --- 512x512 256-Color Mode (Rend256A - Rend256D) ---

static void Rend256Flag(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal, int shift)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = buf + i * 16;
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p0 = pal[(eax >> shift) & 255];
				dst_w[j * 2 + 0] = p0;
				dst_w[j * 2 + 0 + 512] = p0;
				
				DWORD p1 = pal[(eax >> (shift + 16)) & 255];
				dst_w[j * 2 + 1] = p1;
				dst_w[j * 2 + 1 + 512] = p1;
			}
		}
	}
}

static int Rend256All(const BYTE *gvram, DWORD *buf, const DWORD *pal, int shift)
{
	int changed = 0;
	const DWORD* src_w = (const DWORD*)gvram;
	DWORD* dst_w = buf;
	for (int i = 0; i < 32; i++) {
		for (int j = 0; j < 8; j++) {
			DWORD eax = src_w[i * 8 + j];
			DWORD p0 = pal[(eax >> shift) & 255];
			if (dst_w[i * 16 + j * 2 + 0] != p0) {
				dst_w[i * 16 + j * 2 + 0] = p0;
				dst_w[i * 16 + j * 2 + 0 + 512] = p0;
				changed = 1;
			}
			DWORD p1 = pal[(eax >> (shift + 16)) & 255];
			if (dst_w[i * 16 + j * 2 + 1] != p1) {
				dst_w[i * 16 + j * 2 + 1] = p1;
				dst_w[i * 16 + j * 2 + 1 + 512] = p1;
				changed = 1;
			}
		}
	}
	return changed;
}

int Rend256A(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend256All(gvram, buf, pal, 0); }
void Rend256B(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend256Flag(gvram, buf, flag, pal, 0); }
int Rend256C(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend256All(gvram, buf, pal, 8); }
void Rend256D(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend256Flag(gvram, buf, flag, pal, 8); }

// --- 512x512 65536-Color Mode (Rend64KA - Rend64KB) ---

void Rend64KA(const BYTE *gvram, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const WORD* src_w = (const WORD*)(gvram + i * 32);
			DWORD* dst_w = buf + i * 16;
			for (int j = 0; j < 16; j++) {
				WORD ax = src_w[j];
				BYTE ah = ax >> 8;
				BYTE al = ax & 255;
				WORD ebx = (plt[ah + 256] << 8) | plt[al];
				DWORD color = pal[ebx];
				if (ax == 0) color |= 0x80000000;
				
				dst_w[j] = color;
				dst_w[j + 512] = color;
			}
		}
	}
}

int Rend64KB(const BYTE *gvram, DWORD *buf, BYTE *plt, DWORD *pal)
{
	int changed = 0;
	const WORD* src_w = (const WORD*)gvram;
	DWORD* dst_w = buf;
	for (int i = 0; i < 512; i++) {
		WORD ax = src_w[i];
		BYTE ah = ax >> 8;
		BYTE al = ax & 255;
		WORD ebx = (plt[ah + 256] << 8) | plt[al];
		DWORD color = pal[ebx];
		if (ax == 0) color |= 0x80000000;

		if (dst_w[i] != color) {
			dst_w[i] = color;
			dst_w[i + 512] = color;
			changed = 1;
		}
	}
	return changed;
}

// --- Sprites / PCG ---

void RendClrSprite(DWORD *buf, DWORD color, int len)
{
	int count = (len >> 4) + ((len & 15) ? 1 : 0);
	for (int i = 0; i < count; i++) {
		for (int j = 0; j < 16; j++) {
			buf[i * 16 + j] = color;
		}
	}
}

void RendSpriteCore(const DWORD *line, DWORD *buf, DWORD x, DWORD flag)
{
	if (!flag) {
		if (x < 16) {
			int skip = 16 - x;
			for (int i = 0; i < (int)x; i++) {
				DWORD p = line[skip + i];
				if (!(p & 0x80000000)) buf[i] = p;
			}
		} else {
			DWORD* dst = buf + (x - 16);
			for (int i = 0; i < 16; i++) {
				DWORD p = line[i];
				if (!(p & 0x80000000)) dst[i] = p;
			}
		}
	} else {
		if (x < 16) {
			for (int i = 0; i < (int)x; i++) {
				DWORD p = line[x - 1 - i];
				if (!(p & 0x80000000)) buf[i] = p;
			}
		} else {
			DWORD* dst = buf + (x - 16);
			for (int i = 0; i < 16; i++) {
				DWORD p = line[i];
				if (!(p & 0x80000000)) dst[15 - i] = p; // horizontal reverse
			}
		}
	}
}

void RendSprite(const DWORD *line, DWORD *buf, DWORD x, DWORD flag, int spno, BYTE *pri)
{
	(void)spno; (void)pri;
	RendSpriteCore(line, buf, x, flag);
}

void RendSpriteC(const DWORD *line, DWORD *buf, DWORD x, DWORD flag)
{
	RendSpriteCore(line, buf, x, flag);
}

void RendPCGNew(DWORD index, const BYTE *mem, DWORD *buf, DWORD *pal)
{
	const BYTE* src = mem + ((index & 0xff) << 7) + 0x8000;
	DWORD* dst = buf + (index * 256);
	const DWORD* p_pal = pal + 256 + (((index >> 8) & 15) << 4);

	for (int y = 0; y < 16; y++) {
		DWORD eax = *(const DWORD*)src;
		eax = (eax << 16) | (eax >> 16); // Match rol eax, 16 in ASM
		
		for (int x = 0; x < 8; x++) {
			eax = (eax << 4) | (eax >> 28); // rol 4
			dst[y * 16 + x] = p_pal[eax & 15];
		}
		
		eax = *(const DWORD*)(src + 64);
		eax = (eax << 16) | (eax >> 16); // Match rol eax, 16 in ASM
		for (int x = 8; x < 16; x++) {
			eax = (eax << 4) | (eax >> 28); // rol 4
			dst[y * 16 + x] = p_pal[eax & 15];
		}
		src += 4;
	}
}

static inline BOOL IsBGPixelVisible(DWORD pixel, BOOL legacy_bg_transparency)
{
	if (legacy_bg_transparency) {
		return (pixel & 0x00FFFFFF) != 0;
	}
	return (pixel & 0x80000000) == 0;
}

static inline void RendBGWritePixel(DWORD *dst, DWORD src)
{
	if ((src & 0x80000000) == 0) {
		*dst = src;
	}
}

static inline void RendBGForceWritePixel(DWORD *dst, DWORD src)
{
	if ((src & 0x80000000) == 0 || (*dst & 0x80000000) != 0) {
		*dst = src;
	}
}

void RendBG8(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 3) + ((len & 7) ? 1 : 0);
	bgdata_t *current_ptr = ptr + x;
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = current_ptr->bg;
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}
		
		DWORD* src_pixels = current_ptr->pcg;
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 8; p++) {
				RendBGWritePixel(&dst[p], src_pixels[p]);
			}
		} else {
			for (int p = 0; p < 8; p++) {
				RendBGWritePixel(&dst[7 - p], src_pixels[p]);
			}
		}
		
		dst += 8;
		current_ptr++;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG8F(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 3) + ((len & 7) ? 1 : 0);
	bgdata_t *current_ptr = ptr + x;
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = current_ptr->bg;
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}

		DWORD* src_pixels = current_ptr->pcg;
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 8; p++) {
				RendBGForceWritePixel(&dst[p], src_pixels[p]);
			}
		} else {
			for (int p = 0; p < 8; p++) {
				RendBGForceWritePixel(&dst[7 - p], src_pixels[p]);
			}
		}

		dst += 8;
		current_ptr++;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG8P(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = ptr->bg;
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}
	
	DWORD* src_pixels = ptr->pcg;
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			RendBGWritePixel(&buf[p], src_pixels[offset + p]);
		}
	} else {
		for (int p = 0; p < length; p++) {
			RendBGWritePixel(&buf[p], src_pixels[7 - (offset + p)]);
		}
	}
}

void RendBG16(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 4) + ((len & 15) ? 1 : 0);
	bgdata_t *current_ptr = ptr + x;
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = current_ptr->bg;
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}
		
		DWORD* src = current_ptr->pcg;
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 16; p++) {
				RendBGWritePixel(&dst[p], src[p]);
			}
		} else {
			for (int p = 0; p < 16; p++) {
				RendBGWritePixel(&dst[15 - p], src[p]);
			}
		}
		
		dst += 16;
		current_ptr++;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG16F(bgdata_t *ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 4) + ((len & 15) ? 1 : 0);
	bgdata_t *current_ptr = ptr + x;
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = current_ptr->bg;
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}

		DWORD* src = current_ptr->pcg;
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 16; p++) {
				RendBGForceWritePixel(&dst[p], src[p]);
			}
		} else {
			for (int p = 0; p < 16; p++) {
				RendBGForceWritePixel(&dst[15 - p], src[p]);
			}
		}

		dst += 16;
		current_ptr++;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG16P(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem,
	DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = ptr->bg;
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}
	
	DWORD* src = ptr->pcg;
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			RendBGWritePixel(&buf[p], src[offset + p]);
		}
	} else {
		for (int p = 0; p < length; p++) {
			RendBGWritePixel(&buf[p], src[15 - (offset + p)]);
		}
	}
}

// --- Mix (Compositing width dirty-flag update) ---

void RendMix00(DWORD *buf, BOOL *flag, int len)
{
	int blocks = len >> 4;
	for (int i = 0; i < blocks; i++) {
		int active = 0;
		for (int j = 0; j < 16; j++) {
			if (buf[j] != 0) active = 1;
			buf[j] = 0;
		}
		if (active) flag[0] = TRUE;
		buf += 16;
		flag++;
	}
	int rest = len & 15;
	if (rest) {
		int active = 0;
		for (int j = 0; j < rest; j++) {
			if (buf[j] != 0) active = 1;
			buf[j] = 0;
		}
		if (active) flag[0] = TRUE;
	}
}

void RendMix01(DWORD *buf, DWORD *src, BOOL *flag, int len)
{
	int blocks = len >> 4;
	for (int i = 0; i < blocks; i++) {
		int active = 0;
		for (int j = 0; j < 16; j++) {
			if (buf[j] != src[j]) active = 1;
			buf[j] = src[j];
		}
		if (active) flag[0] = TRUE;
		buf += 16; src += 16;
		flag++;
	}
	int rest = len & 15;
	if (rest) {
		int active = 0;
		for (int j = 0; j < rest; j++) {
			if (buf[j] != src[j]) active = 1;
			buf[j] = src[j];
		}
		if (active) flag[0] = TRUE;
	}
}

void RendMix02(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len)
{
	int blocks = len >> 4;
	for (int i = 0; i < blocks; i++) {
		int active = 0;
		for (int j = 0; j < 16; j++) {
			DWORD p = f[j];
			if (p & 0x80000000) p = s[j]; 
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
		buf += 16; f += 16; s += 16;
		flag++;
	}
	int rest = len & 15;
	if (rest) {
		int active = 0;
		for (int j = 0; j < rest; j++) {
			DWORD p = f[j];
			if (p & 0x80000000) p = s[j];
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
	}
}

void RendMix02C(DWORD *buf, DWORD *f, DWORD *s, BOOL *flag, int len) { RendMix02(buf, f, s, flag, len); }

void RendMix03Base(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len)
{
	int blocks = len >> 4;
	for (int i = 0; i < blocks; i++) {
		int active = 0;
		for (int j = 0; j < 16; j++) {
			DWORD p = f[j];
			if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) p = s[j]; 
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
		buf += 16; f += 16; s += 16;
		flag++;
	}
	int rest = len & 15;
	if (rest) {
		int active = 0;
		for (int j = 0; j < rest; j++) {
			DWORD p = f[j];
			if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) p = s[j];
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
	}
}



void RendMix04(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len)
{
	int blocks = len >> 4;
	for (int i = 0; i < blocks; i++) {
		int active = 0;
		for (int j = 0; j < 16; j++) {
			DWORD p = f[j];
			if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) {
				p = s[j];
				if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) {
					p = t[j];
				}
			}
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
		buf += 16; f += 16; s += 16; t += 16;
		flag++;
	}
	int rest = len & 15;
	if (rest) {
		int active = 0;
		for (int j = 0; j < rest; j++) {
			DWORD p = f[j];
			if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) {
				p = s[j];
				if ((p & 0x80000000) || ((p & 0x00FFFFFF) == 0)) {
					p = t[j];
				}
			}
			if (buf[j] != p) active = 1;
			buf[j] = p;
		}
		if (active) flag[0] = TRUE;
	}
}

void RendMix04C(DWORD *buf, const DWORD *f, const DWORD *s, DWORD *t, BOOL *flag, int len) { RendMix04(buf, f, s, t, flag, len); }

// --- Graphic Layer Compositing (NO dirty-flag update) ---

void RendGrp02(DWORD *buf, const DWORD *f, const DWORD *s, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD p = f[i];
		if (p & 0x80000000) p = s[i];
		buf[i] = p;
	}
}

void RendGrp02C(DWORD *buf, const DWORD *f, const DWORD *s, int len) { RendGrp02(buf, f, s, len); }

void RendGrp03(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD p = f[i];
		if (p & 0x80000000) {
			p = s[i];
			if (p & 0x80000000) {
				p = t[i];
			}
		}
		buf[i] = p;
	}
}

void RendGrp03C(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, int len) { RendGrp03(buf, f, s, t, len); }

void RendGrp04(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, const DWORD *e, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD p = f[i];
		if (p & 0x80000000) {
			p = s[i];
			if (p & 0x80000000) {
				p = t[i];
				if (p & 0x80000000) {
					p = e[i];
				}
			}
		}
		buf[i] = p;
	}
}

void RendGrp04C(DWORD *buf, const DWORD *f, const DWORD *s, const DWORD *t, const DWORD *e, int len) { RendGrp04(buf, f, s, t, e, len); }

static inline DWORD RendHalfBlendRaw(DWORD front, DWORD back)
{
	DWORD keep = front & back;
	DWORD diff = (front ^ back) & 0x00fefefe;
	return (diff >> 1) + keep;
}

static inline DWORD RendHalfBlendPixel(DWORD front, DWORD back)
{
	return RendHalfBlendRaw(front, back) | 0x40000000;
}

static inline BOOL RendTransparent(DWORD p)
{
	return (p & 0x80000000) != 0;
}

static inline BOOL RendHalf(DWORD p)
{
	return (p & 0x40000000) != 0;
}

static inline BOOL RendColor(DWORD p)
{
	return (p & 0x00ffffff) != 0;
}

static inline DWORD Rend64KPixelH(WORD raw, BYTE *plt, DWORD *pal)
{
	BYTE hi = (BYTE)(raw >> 8);
	BYTE lo = (BYTE)(raw & 0xff);
	WORD idx = (WORD)(((WORD)plt[hi + 256] << 8) | plt[lo & 0xfe]);
	DWORD out = pal[idx];
	if (lo & 1) {
		out |= 0x40000000;
	}
	return out;
}

static inline DWORD Rend64KPixelHP(WORD raw, BYTE *plt, DWORD *pal)
{
	BYTE hi = (BYTE)(raw >> 8);
	BYTE lo = (BYTE)(raw & 0xff);
	WORD idx = (WORD)(((WORD)plt[hi + 256] << 8) | plt[lo]);
	DWORD out = pal[idx];
	if (plt[lo | 1] & 1) {
		out |= 0x40000000;
	}
	return out;
}

void Rend64KAH(const BYTE *gvram, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal)
{
	const WORD *src = (const WORD *)gvram;
	for (int block = 0; block < 32; block++) {
		if (!flag[block]) {
			src += 16;
			buf += 16;
			continue;
		}
		flag[block] = FALSE;
		for (int i = 0; i < 16; i++) {
			DWORD out = Rend64KPixelH(src[i], plt, pal);
			buf[i] = out;
			buf[i + 512] = out;
		}
		src += 16;
		buf += 16;
	}
}

int Rend64KBH(const BYTE *gvram, DWORD *buf, BYTE *plt, DWORD *pal)
{
	int changed = 0;
	const WORD *src = (const WORD *)gvram;
	for (int i = 0; i < 512; i++) {
		DWORD out = Rend64KPixelH(src[i], plt, pal);
		if (buf[i] != out) {
			changed = 1;
		}
		buf[i] = out;
		buf[i + 512] = out;
	}
	return changed;
}

void Rend64KAHP(const BYTE *gvram, DWORD *buf, BOOL *flag, BYTE *plt, DWORD *pal)
{
	const WORD *src = (const WORD *)gvram;
	for (int block = 0; block < 32; block++) {
		if (!flag[block]) {
			src += 16;
			buf += 16;
			continue;
		}
		flag[block] = FALSE;
		for (int i = 0; i < 16; i++) {
			DWORD out = Rend64KPixelHP(src[i], plt, pal);
			buf[i] = out;
			buf[i + 512] = out;
		}
		src += 16;
		buf += 16;
	}
}

int Rend64KBHP(const BYTE *gvram, DWORD *buf, BYTE *plt, DWORD *pal)
{
	int changed = 0;
	const WORD *src = (const WORD *)gvram;
	for (int i = 0; i < 512; i++) {
		DWORD out = Rend64KPixelHP(src[i], plt, pal);
		if (buf[i] != out) {
			changed = 1;
		}
		buf[i] = out;
		buf[i + 512] = out;
	}
	return changed;
}

void RendSpriteP(const DWORD *line, DWORD *buf, DWORD x, BOOL hflag, int spno, BYTE *pri, DWORD len)
{
	BYTE p = (BYTE)spno;
	if (len > 16) {
		len = 16;
	}
	if (x < 16) {
		DWORD count = x;
		if (count > len) {
			count = len;
		}
		for (DWORD i = 0; i < count; i++) {
			DWORD src_index = hflag ? (x - 1 - i) : (16 - x + i);
			DWORD pix = line[src_index];
			if (!RendTransparent(pix) && pri[i] >= p) {
				buf[i] = pix;
				pri[i] = p;
			}
		}
		return;
	}
	DWORD start = x - 16;
	buf += start;
	pri += start;
	for (DWORD i = 0; i < len; i++) {
		DWORD pix = line[hflag ? (15 - i) : i];
		if (!RendTransparent(pix) && pri[i] >= p) {
			buf[i] = pix;
			pri[i] = p;
		}
	}
}

void RendBG8FP(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = ptr->bg;
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}

	DWORD* src = ptr->pcg;
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			RendBGForceWritePixel(&buf[p], src[offset + p]);
		}
	} else {
		for (int p = 0; p < length; p++) {
			RendBGForceWritePixel(&buf[p], src[7 - (offset + p)]);
		}
	}
}

void RendBG16FP(bgdata_t *ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = ptr->bg;
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}

	DWORD* src = ptr->pcg;
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			RendBGForceWritePixel(&buf[p], src[offset + p]);
		}
	} else {
		for (int p = 0; p < length; p++) {
			RendBGForceWritePixel(&buf[p], src[15 - (offset + p)]);
		}
	}
}

void RendGrp02HBS(DWORD *buf, const DWORD *f, const DWORD *g, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD fp = f[i];
		DWORD gp = g[i];
		if (RendTransparent(fp)) {
			buf[i] = gp;
		}
		else if (RendHalf(fp)) {
			buf[i] = RendHalfBlendPixel(fp, gp);
		}
		else {
			buf[i] = fp;
		}
	}
}

void RendGrp02HB(DWORD *buf, const DWORD *f, const DWORD *g, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD fp = f[i];
		if (!RendTransparent(fp) && RendHalf(fp)) {
			fp = RendHalfBlendPixel(fp, g[i]);
		}
		buf[i] = fp;
	}
}

void RendMix01H(DWORD *buf, DWORD *f, DWORD c, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = RendHalf(f[i]) ? RendHalfBlendRaw(f[i], c) : f[i];
		if (buf[i] != out) {
			flag[i >> 4] = TRUE;
		}
		buf[i] = out;
	}
}

static inline void RendStoreMix(DWORD *buf, BOOL *flag, int i, DWORD out)
{
	if (buf[i] != out) {
		flag[i >> 4] = TRUE;
	}
	buf[i] = out;
}

void RendMix02A(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (RendTransparent(out) && !RendHalf(g[i])) {
			out = g[i];
		}
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix02B(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = RendColor(f[i]) ? f[i] : g[i];
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix02BH(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (RendHalf(out)) {
			out = RendHalfBlendRaw(out, g[i]);
		}
		else if (!RendColor(out)) {
			out = g[i];
		}
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix02CH(DWORD *buf, DWORD *f, DWORD *g, DWORD c, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		BOOL may_half = FALSE;
		if (RendTransparent(out)) {
			DWORD gp = g[i];
			if (RendColor(gp)) {
				out = gp;
				may_half = TRUE;
			}
		}
		else if (!RendColor(out)) {
			out = g[i];
			may_half = TRUE;
		}
		if (may_half && RendHalf(out)) {
			out = RendHalfBlendRaw(out, c);
		}
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix02CS(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = g[i];
		if (RendTransparent(out) || !RendHalf(out)) {
			DWORD alt;
			out = f[i];
			alt = RendTransparent(out) ? out : g[i];
			if (!RendColor(out)) {
				out = alt;
			}
		}
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix02D(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len) { RendMix02B(buf, f, g, flag, len); }
void RendMix02DH(DWORD *buf, DWORD *f, DWORD *g, DWORD c, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (!RendColor(out)) {
			out = g[i];
			if (RendHalf(out)) {
				out = RendHalfBlendRaw(out, c);
			}
		}
		RendStoreMix(buf, flag, i, out);
	}
}
void RendMix02DS(DWORD *buf, DWORD *f, DWORD *g, BOOL *flag, int len) { RendMix02CS(buf, f, g, flag, len); }
void RendMixP0H(DWORD *buf, DWORD *f, DWORD col, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		RendStoreMix(buf, flag, i, RendHalfBlendRaw(f[i], col));
	}
}

static inline DWORD RendPick3(DWORD f, DWORD g, DWORD h)
{
	if (!RendTransparent(f)) {
		return f;
	}
	if (!RendTransparent(g)) {
		return g;
	}
	return h;
}

void RendMix03A(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len) { for (int i = 0; i < len; i++) RendStoreMix(buf, flag, i, RendPick3(f[i], g[i], h[i])); }
void RendMix03AH(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = g[i];
		if (RendTransparent(out) && !RendHalf(h[i])) {
			out = h[i];
		}
		if (RendHalf(f[i])) {
			out = RendHalfBlendRaw(out, f[i]);
		}
		else if (RendColor(f[i])) {
			out = f[i];
		}
		RendStoreMix(buf, flag, i, out);
	}
}
void RendMix03B(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len) { RendMix03A(buf, f, g, h, flag, len); }
void RendMix03BH(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (!RendTransparent(out)) {
			if (!RendColor(out)) {
				out = RendHalf(g[i]) ? RendHalfBlendRaw(g[i], h[i]) : g[i];
			}
		}
		else {
			out = g[i];
			if (RendHalf(out)) {
				out = RendHalfBlendRaw(out, h[i]);
			}
			else if (!RendColor(out)) {
				out = RendHalf(h[i]) ? f[i] : h[i];
			}
		}
		RendStoreMix(buf, flag, i, out);
	}
}
void RendMix03BS(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len) { RendMix03A(buf, f, g, h, flag, len); }
void RendMix03C(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (RendTransparent(out)) {
			out = g[i];
			if (RendHalf(out)) {
				out = f[i];
			}
		}
		if (!RendColor(out)) {
			out = h[i];
		}
		RendStoreMix(buf, flag, i, out);
	}
}

void RendMix03CH(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, DWORD c, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = f[i];
		if (RendTransparent(out)) {
			DWORD gp = g[i];
			out = RendHalf(gp) ? f[i] : gp;
		}
		if (!RendColor(out)) {
			out = h[i];
			if (RendHalf(out)) {
				out = RendHalfBlendRaw(out, c);
			}
		}
		RendStoreMix(buf, flag, i, out);
	}
}
void RendMix03CS(DWORD *buf, DWORD *f, DWORD *g, DWORD *h, BOOL *flag, int len)
{
	for (int i = 0; i < len; i++) {
		DWORD out = h[i];
		if (RendTransparent(out) || !RendHalf(out)) {
			out = f[i];
			if (RendTransparent(out)) {
				out = g[i];
				if (RendHalf(out)) {
					out = f[i];
				}
			}
			if (!RendColor(out)) {
				out = h[i];
			}
		}
		RendStoreMix(buf, flag, i, out);
	}
}

} // extern "C"
