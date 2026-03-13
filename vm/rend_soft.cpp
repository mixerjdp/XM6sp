// Compatibilidad con Visual Studio y Windows
#include <windows.h>

#include "rend_soft.h"
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

int Rend1024A(const BYTE *gvram, DWORD *buf, const DWORD *pal)
{
	int changed = 0;
	for (int i = 0; i < 128; i++) {
		for (int w = 0; w < 2; w++) {
			DWORD eax = ((const DWORD*)gvram)[w];
			
			DWORD p0_0 = pal[eax & 15];
			if (buf[w*2 + 0] != p0_0) { buf[w*2 + 0] = p0_0; buf[w*2 + 0 + 1024] = p0_0; changed = 1; }
			
			DWORD p1_0 = pal[(eax >> 4) & 15];
			if (buf[w*2 + 0 + 512] != p1_0) { buf[w*2 + 0 + 512] = p1_0; buf[w*2 + 0 + 512 + 1024] = p1_0; changed = 1; }
			
			DWORD p0_1 = pal[(eax >> 16) & 15];
			if (buf[w*2 + 1] != p0_1) { buf[w*2 + 1] = p0_1; buf[w*2 + 1 + 1024] = p0_1; changed = 1; }
			
			DWORD p1_1 = pal[(eax >> 20) & 15];
			if (buf[w*2 + 1 + 512] != p1_1) { buf[w*2 + 1 + 512] = p1_1; buf[w*2 + 1 + 512 + 1024] = p1_1; changed = 1; }
		}
		gvram += 8;
		buf += 4;
	}
	return changed;
}

int Rend1024B(const BYTE *gvram, DWORD *buf, const DWORD *pal)
{
	int changed = 0;
	for (int i = 0; i < 128; i++) {
		for (int w = 0; w < 2; w++) {
			DWORD eax = ((const DWORD*)gvram)[w];
			
			DWORD p2_0 = pal[(eax >> 8) & 15];
			if (buf[w*2 + 0] != p2_0) { buf[w*2 + 0] = p2_0; buf[w*2 + 0 + 1024] = p2_0; changed = 1; }
			
			DWORD p3_0 = pal[(eax >> 12) & 15];
			if (buf[w*2 + 0 + 512] != p3_0) { buf[w*2 + 0 + 512] = p3_0; buf[w*2 + 0 + 512 + 1024] = p3_0; changed = 1; }
			
			DWORD p2_1 = pal[(eax >> 24) & 15];
			if (buf[w*2 + 1] != p2_1) { buf[w*2 + 1] = p2_1; buf[w*2 + 1 + 1024] = p2_1; changed = 1; }
			
			DWORD p3_1 = pal[(eax >> 28) & 15];
			if (buf[w*2 + 1 + 512] != p3_1) { buf[w*2 + 1 + 512] = p3_1; buf[w*2 + 1 + 512 + 1024] = p3_1; changed = 1; }
		}
		gvram += 8;
		buf += 4;
	}
	return changed;
}

void Rend1024C(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src = (const DWORD*)(gvram + i * 32); // Wait, line 1387 is `add esi, 32`!!! NO! `Rend1024C` uses `add esi, 32` per loop iter? Let's check size accurately.
			// Actually let me replace with pointer arithmetic matching the C++ block.
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = buf + i * 16;
			
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p0_0 = pal[eax & 15];
				dst_w[j*2 + 0] = p0_0;
				dst_w[j*2 + 0 + 1024] = p0_0;
				
				DWORD p0_1 = pal[(eax >> 16) & 15];
				dst_w[j*2 + 1] = p0_1;
				dst_w[j*2 + 1 + 1024] = p0_1;
			}
		}
	}
}

void Rend1024D(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal)
{
	DWORD* dst_base = buf + 512;
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = dst_base + i * 16;
			
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p1_0 = pal[(eax >> 4) & 15];
				dst_w[j*2 + 0] = p1_0;
				dst_w[j*2 + 0 + 1024] = p1_0;
				
				DWORD p1_1 = pal[(eax >> 20) & 15];
				dst_w[j*2 + 1] = p1_1;
				dst_w[j*2 + 1 + 1024] = p1_1;
			}
		}
	}
}

void Rend1024E(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal)
{
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = buf + i * 16;
			
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p2_0 = pal[(eax >> 8) & 15];
				dst_w[j*2 + 0] = p2_0;
				dst_w[j*2 + 0 + 1024] = p2_0;
				
				DWORD p2_1 = pal[(eax >> 24) & 15];
				dst_w[j*2 + 1] = p2_1;
				dst_w[j*2 + 1 + 1024] = p2_1;
			}
		}
	}
}

void Rend1024F(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal)
{
	DWORD* dst_base = buf + 512;
	for (int i = 0; i < 32; i++) {
		if (flag[i]) {
			flag[i] = FALSE;
			const DWORD* src_w = (const DWORD*)(gvram + i * 32);
			DWORD* dst_w = dst_base + i * 16;
			
			for (int j = 0; j < 8; j++) {
				DWORD eax = src_w[j];
				DWORD p3_0 = pal[(eax >> 12) & 15];
				dst_w[j*2 + 0] = p3_0;
				dst_w[j*2 + 0 + 1024] = p3_0;
				
				DWORD p3_1 = pal[(eax >> 28) & 15];
				dst_w[j*2 + 1] = p3_1;
				dst_w[j*2 + 1 + 1024] = p3_1;
			}
		}
	}
}

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
int Rend16B(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 0); return 1; }
int Rend16C(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 4); }
int Rend16D(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 4); return 1; }
int Rend16E(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 8); }
int Rend16F(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 8); return 1; }
int Rend16G(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend16All(gvram, buf, pal, 12); }
int Rend16H(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend16Flag(gvram, buf, flag, pal, 12); return 1; }

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

void Rend256A(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend256Flag(gvram, buf, flag, pal, 0); }
void Rend256B(const BYTE *gvram, DWORD *buf, BOOL *flag, const DWORD *pal) { Rend256Flag(gvram, buf, flag, pal, 8); }
int Rend256C(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend256All(gvram, buf, pal, 0); }
int Rend256D(const BYTE *gvram, DWORD *buf, const DWORD *pal) { return Rend256All(gvram, buf, pal, 8); }

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

void RendSprite(const DWORD *line, DWORD *buf, DWORD x, DWORD flag)
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

void RendSpriteC(const DWORD *line, DWORD *buf, DWORD x, DWORD flag)
{
	RendSprite(line, buf, x, flag);
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

void RendBG8(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 3) + ((len & 7) ? 1 : 0);
	DWORD **current_ptr = ptr + (x * 2);
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = (DWORD)(size_t)current_ptr[1];
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}
		
		DWORD* src_pixels = current_ptr[0];
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 8; p++) {
				// Honor the explicit transparent flag (REND_COLOR0), not RGB==0.
				if (!(src_pixels[p] & 0x80000000)) dst[p] = src_pixels[p];
			}
		} else {
			for (int p = 0; p < 8; p++) {
				if (!(src_pixels[p] & 0x80000000)) dst[7 - p] = src_pixels[p];
			}
		}
		
		dst += 8;
		current_ptr += 2;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG8C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	RendBG8(ptr, buf, x, len, ready, mem, pcgbuf, pal);
}

void RendBG8P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = (DWORD)(size_t)ptr[1];
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}
	
	DWORD* src_pixels = ptr[0];
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			if (!(src_pixels[offset + p] & 0x80000000)) buf[p] = src_pixels[offset + p];
		}
	} else {
		for (int p = 0; p < length; p++) {
			if (!(src_pixels[7 - (offset + p)] & 0x80000000)) buf[p] = src_pixels[7 - (offset + p)];
		}
	}
}

void RendBG16(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	int tiles = (len >> 4) + ((len & 15) ? 1 : 0);
	DWORD **current_ptr = ptr + (x * 2);
	int current_x = x;
	DWORD* dst = buf;

	for (int i = 0; i < tiles; i++) {
		DWORD bgdata = (DWORD)(size_t)current_ptr[1];
		DWORD pcgno = bgdata & 0xfff;
		if (!ready[pcgno]) {
			ready[pcgno] = TRUE;
			RendPCGNew(pcgno, mem, pcgbuf, pal);
		}
		
		DWORD* src = current_ptr[0];
		if (!(bgdata & 0x4000)) {
			for (int p = 0; p < 16; p++) {
				// Honor the explicit transparent flag (REND_COLOR0), not RGB==0.
				if (!(src[p] & 0x80000000)) dst[p] = src[p];
			}
		} else {
			for (int p = 0; p < 16; p++) {
				if (!(src[p] & 0x80000000)) dst[15 - p] = src[p];
			}
		}
		
		dst += 16;
		current_ptr += 2;
		current_x++;
		if (current_x == 64) {
			current_x = 0;
			current_ptr = ptr;
		}
	}
}

void RendBG16C(DWORD **ptr, DWORD *buf, int x, int len, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	RendBG16(ptr, buf, x, len, ready, mem, pcgbuf, pal);
}

void RendBG16P(DWORD **ptr, DWORD *buf, int offset, int length, BOOL *ready, const BYTE *mem, DWORD *pcgbuf, DWORD *pal)
{
	DWORD bgdata = (DWORD)(size_t)ptr[1];
	DWORD pcgno = bgdata & 0xfff;
	if (!ready[pcgno]) {
		ready[pcgno] = TRUE;
		RendPCGNew(pcgno, mem, pcgbuf, pal);
	}
	
	DWORD* src = ptr[0];
	if (!(bgdata & 0x4000)) {
		for (int p = 0; p < length; p++) {
			if (!(src[offset + p] & 0x80000000)) buf[p] = src[offset + p];
		}
	} else {
		for (int p = 0; p < length; p++) {
			if (!(src[15 - (offset + p)] & 0x80000000)) buf[p] = src[15 - (offset + p)];
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

void RendMix01(DWORD *buf, const DWORD *src, BOOL *flag, int len)
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

void RendMix02(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len)
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

void RendMix02C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len) { RendMix02(buf, f, s, flag, len); }

void RendMix03(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len)
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

void RendMix03C(DWORD *buf, const DWORD *f, const DWORD *s, BOOL *flag, int len) { RendMix03(buf, f, s, flag, len); }

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

void RendGrp04(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, DWORD *e, int len)
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

void RendGrp04C(DWORD *buf, DWORD *f, DWORD *s, DWORD *t, DWORD *e, int len) { RendGrp04(buf, f, s, t, e, len); }

} // extern "C"
