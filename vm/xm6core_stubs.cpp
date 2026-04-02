//---------------------------------------------------------------------------
//
//	XM6 Core DLL - Stubs para símbolos que normalmente residen en el
//	frontend MFC y que la VM referencia.
//
//	Este archivo sólo se compila en el target DLL (XM6Core.vcxproj).
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"

//---------------------------------------------------------------------------
//	cpudebug_fetch — Lectura de memoria para el desensamblador de debug.
//
//	En el frontend MFC se implementa en mfc_cpu.cpp. En la DLL
//	autónoma, el desensamblador no se usa, por lo que retornamos 0.
//---------------------------------------------------------------------------
extern "C" {
unsigned short cpudebug_fetch(unsigned long /*addr*/)
{
	return 0;
}
}

//---------------------------------------------------------------------------
//	IsCMOV — Detección de soporte CMOV en la CPU host.
//
//	En el frontend MFC se implementa en mfc_asm_portable.cpp vía CPUID.
//	En la DLL autónoma, asumimos que CMOV está soportado (cualquier CPU
//	posterior a Pentium Pro lo soporta, ~1996).
//---------------------------------------------------------------------------
BOOL FASTCALL IsCMOV(void)
{
	return TRUE;
}

//---------------------------------------------------------------------------
//	SoundMMX — Conversión int32[] → int16[] con saturación.
//	SoundEMMS — No-op (ya no se usa MMX real).
//
//	Estas funciones están en mfc_asm_portable.cpp pero éste incluye
//	headers MFC. Reimplementamos la versión portátil aquí.
//---------------------------------------------------------------------------
static inline short XM6_SaturateS16(int value)
{
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return (short)value;
}

void SoundMMX(DWORD *pSrc, WORD *pDst, int nBytes)
{
	if (!pSrc || !pDst || nBytes <= 0) {
		return;
	}

	const int *src = reinterpret_cast<const int*>(pSrc);
	short *dst = reinterpret_cast<short*>(pDst);
	const int samples = nBytes >> 1;

	for (int i = 0; i < samples; ++i) {
		dst[i] = XM6_SaturateS16(src[i]);
	}
}

void SoundEMMS(void)
{
	// No-op: ya no se usa MMX real.
}
