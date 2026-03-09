//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Portable replacements for selected routines from mfc_asm.asm
//
//---------------------------------------------------------------------------

#if defined(_WIN32)

#include "os.h"
#include "mfc.h"
#include "xm6.h"
#include "mfc_asm.h"

#include <stdint.h>
#include <string.h>
#include <limits.h>

#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
#include <intrin.h>
#endif

extern "C" {

static __forceinline uint32_t XM6_Rol32(uint32_t v, unsigned n)
{
	return (v << n) | (v >> (32U - n));
}

static __forceinline int16_t XM6_SaturateS16_FromS32(int32_t v)
{
	// Match legacy ASM limits exactly:
	//   max = 0x00007fff
	//   min = 0xffff8000
	if (v > 32767) {
		return SHRT_MAX;
	}
	if (v < (-32768)) {
		return SHRT_MIN;
	}
	return (int16_t)v;
}

#if defined(_DEBUG)
static void XM6_VerifySoundPath_Once()
{
	static bool verified = false;
	if (verified) {
		return;
	}
	verified = true;

	int32_t src[16] = {
		-40000, -32769, -32768, -32767,
		-1, 0, 1, 32766,
		32767, 32768, 40000, 1234,
		-1234, 50000, -50000, 42
	};
	int16_t dst[16];
	memset(dst, 0, sizeof(dst));

	for (int i = 0; i < 16; ++i) {
		dst[i] = XM6_SaturateS16_FromS32(src[i]);
	}

	ASSERT(dst[0]  == SHRT_MIN);
	ASSERT(dst[1]  == SHRT_MIN);
	ASSERT(dst[2]  == SHRT_MIN);
	ASSERT(dst[8]  == SHRT_MAX);
	ASSERT(dst[9]  == SHRT_MAX);
	ASSERT(dst[10] == SHRT_MAX);
}
#endif

BOOL IsMMXSupportPortable(void)
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
	int cpu_info[4] = { 0, 0, 0, 0 };
	__cpuid(cpu_info, 0);
	if (cpu_info[0] < 1) {
		return FALSE;
	}
	__cpuid(cpu_info, 1);
	return (cpu_info[3] & (1 << 23)) ? TRUE : FALSE; // EDX.MMX
#else
	return FALSE;
#endif
}

BOOL IsMMXSupport(void)
{
	return IsMMXSupportPortable();
}

BOOL IsCMOVSupportPortable(void)
{
#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
	int cpu_info[4] = { 0, 0, 0, 0 };
	__cpuid(cpu_info, 0);
	if (cpu_info[0] < 1) {
		return FALSE;
	}
	__cpuid(cpu_info, 1);
	return (cpu_info[3] & (1 << 15)) ? TRUE : FALSE; // EDX.CMOV
#else
	return FALSE;
#endif
}

BOOL IsCMOVSupport(void)
{
	return IsCMOVSupportPortable();
}

void SoundMMXPortable(DWORD *pSrc, WORD *pDst, int nBytes)
{
	#if defined(_DEBUG)
	XM6_VerifySoundPath_Once();
	#endif

	if (!pSrc || !pDst || (nBytes <= 0)) {
		return;
	}

	const int32_t* src = reinterpret_cast<const int32_t*>(pSrc);
	int16_t* dst = reinterpret_cast<int16_t*>(pDst);
	const int samples = (nBytes >> 1);

	for (int i = 0; i < samples; ++i) {
		dst[i] = XM6_SaturateS16_FromS32(src[i]);
	}
}

void SoundMMX(DWORD *pSrc, WORD *pDst, int nBytes)
{
	SoundMMXPortable(pSrc, pDst, nBytes);
}

void SoundEMMSPortable()
{
	// No-op in portable C/C++ implementation.
}

void SoundEMMS()
{
	SoundEMMSPortable();
}

void VideoText(const BYTE *pTVRAM, DWORD *pBits, int nLen, DWORD *pPalette)
{
	if (!pTVRAM || !pBits || !pPalette || (nLen <= 0)) {
		return;
	}

	for (int blk = 0; blk < nLen; ++blk) {
		uint32_t p0 = XM6_Rol32(*(const uint32_t*)(pTVRAM + 0x00000), 16);
		uint32_t p1 = XM6_Rol32(*(const uint32_t*)(pTVRAM + 0x20000), 16);
		uint32_t p2 = XM6_Rol32(*(const uint32_t*)(pTVRAM + 0x40000), 16);
		uint32_t p3 = XM6_Rol32(*(const uint32_t*)(pTVRAM + 0x60000), 16);

		for (int i = 0; i < 32; ++i) {
			unsigned idx = 0;
			idx = (idx << 1) | ((p3 >> 31) & 1); p3 <<= 1;
			idx = (idx << 1) | ((p2 >> 31) & 1); p2 <<= 1;
			idx = (idx << 1) | ((p1 >> 31) & 1); p1 <<= 1;
			idx = (idx << 1) | ((p0 >> 31) & 1); p0 <<= 1;
			pBits[i] = pPalette[idx];
		}

		pTVRAM += 4;
		pBits += 32;
	}
}

static void XM6_VideoG1024(const BYTE *src, DWORD *dst, DWORD *plt, int start_shift)
{
	const uint32_t *s = (const uint32_t*)src;
	DWORD *d = dst;

	for (int block = 0; block < 64; ++block) {
		for (int k = 0; k < 4; ++k) {
			uint32_t v = s[k];
			uint32_t t = (uint32_t)start_shift ? (v >> (unsigned)start_shift) : v;

			d[k * 2 + 0] = plt[(t >> 0) & 0x0f];
			d[k * 2 + 512] = plt[(t >> 4) & 0x0f];
			d[k * 2 + 1] = plt[(t >> 16) & 0x0f];
			d[k * 2 + 513] = plt[(t >> 20) & 0x0f];
		}

		s += 4;
		d += 8;
	}
}

void VideoG1024A(const BYTE *src, DWORD *dst, DWORD *plt)
{
	if (!src || !dst || !plt) return;
	XM6_VideoG1024(src, dst, plt, 0);
}

void VideoG1024B(const BYTE *src, DWORD *dst, DWORD *plt)
{
	if (!src || !dst || !plt) return;
	XM6_VideoG1024(src, dst, plt, 8);
}

static void XM6_VideoG16(const BYTE *src, DWORD *dst, int len, DWORD *plt, int shift)
{
	if (!src || !dst || !plt || (len <= 0)) return;
	for (int i = 0; i < len; ++i) {
		uint16_t w = *(const uint16_t*)(src + (i << 1));
		dst[i] = plt[(w >> shift) & 0x0f];
	}
}

void VideoG16A(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG16(src, dst, len, plt, 0);
}

void VideoG16B(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG16(src, dst, len, plt, 4);
}

void VideoG16C(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG16(src, dst, len, plt, 8);
}

void VideoG16D(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG16(src, dst, len, plt, 12);
}

static void XM6_VideoG256(const BYTE *src, DWORD *dst, int len, DWORD *plt, int hi)
{
	if (!src || !dst || !plt || (len <= 0)) return;
	for (int i = 0; i < len; ++i) {
		uint16_t w = *(const uint16_t*)(src + (i << 1));
		uint8_t idx = (uint8_t)(hi ? (w >> 8) : (w & 0xff));
		dst[i] = plt[idx];
	}
}

void VideoG256A(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG256(src, dst, len, plt, 0);
}

void VideoG256B(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	XM6_VideoG256(src, dst, len, plt, 1);
}

void VideoG64K(const BYTE *src, DWORD *dst, int len, DWORD *plt)
{
	if (!src || !dst || !plt || (len <= 0)) return;
	for (int i = 0; i < len; ++i) {
		uint16_t idx = *(const uint16_t*)(src + (i << 1));
		dst[i] = plt[idx];
	}
}

void VideoPCG(BYTE *src, DWORD *dst, DWORD *plt)
{
	if (!src || !dst || !plt) return;

	for (int row = 0; row < 32; ++row) {
		uint32_t v = XM6_Rol32(*(const uint32_t*)src, 16);
		for (int x = 0; x < 8; ++x) {
			v = XM6_Rol32(v, 4);
			dst[x] = plt[v & 0x0f];
		}
		src += 0x40;
		dst += 8;
	}
}

void VideoBG16(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt)
{
	if (!pcg || !dst || !plt) return;

	DWORD *pal = (DWORD*)((BYTE*)plt + ((bg >> 2) & 0x03c0));
	BYTE *src = pcg + ((bg << 7) & 0x7f80);

	if (bg & 0x8000) {
		y = 15 - y;
	}
	if (y & 8) {
		y &= 7;
		src += 0x20;
	}
	src += (y << 2);

	uint32_t a = XM6_Rol32(*(uint32_t*)src, 16);
	uint32_t b = XM6_Rol32(*(uint32_t*)(src + 64), 16);

	if (!(bg & 0x4000)) {
		for (int i = 0; i < 8; ++i) {
			a = XM6_Rol32(a, 4);
			dst[i] = pal[a & 0x0f];
		}
		for (int i = 0; i < 8; ++i) {
			b = XM6_Rol32(b, 4);
			dst[8 + i] = pal[b & 0x0f];
		}
	}
	else {
		for (int i = 0; i < 8; ++i) {
			a = XM6_Rol32(a, 4);
			dst[15 - i] = pal[a & 0x0f];
		}
		for (int i = 0; i < 8; ++i) {
			b = XM6_Rol32(b, 4);
			dst[7 - i] = pal[b & 0x0f];
		}
	}
}

void VideoBG8(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt)
{
	if (!pcg || !dst || !plt) return;

	DWORD *pal = (DWORD*)((BYTE*)plt + ((bg >> 2) & 0x03c0));
	BYTE *src = pcg + ((bg << 5) & 0x1fe0);

	if (bg & 0x8000) {
		y = 7 - y;
	}
	src += (y << 2);

	uint32_t v = XM6_Rol32(*(uint32_t*)src, 16);
	if (!(bg & 0x4000)) {
		for (int i = 0; i < 8; ++i) {
			v = XM6_Rol32(v, 4);
			dst[i] = pal[v & 0x0f];
		}
	}
	else {
		for (int i = 0; i < 8; ++i) {
			v = XM6_Rol32(v, 4);
			dst[7 - i] = pal[v & 0x0f];
		}
	}
}

} // extern "C"

#endif // _WIN32

