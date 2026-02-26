;
; EMULADOR X68000 "XM6"
;
; Copyright (C) 2001-2006 ＰＩ．(ytanaka@ipc-tokai.or.jp)
; [ Sub-ensamblador MFC ]
;

;
; Declaraciones externas
;
		section	.text align=16
		bits	32

		global	_IsMMXSupport
		global	_IsCMOVSupport
		global	_SoundMMX
		global	_SoundEMMS
		global	_VideoText
		global	_VideoG1024A
		global	_VideoG1024B
		global	_VideoG16A
		global	_VideoG16B
		global	_VideoG16C
		global	_VideoG16D
		global	_VideoG256A
		global	_VideoG256B
		global	_VideoG64K
		global	_VideoPCG
		global	_VideoBG16
		global	_VideoBG8

;
; Verificacion de soporte MMX
;
; BOOL IsMMXSupport(void)
;
_IsMMXSupport:
		pushad
; Verificar presencia de CPUID
		pushfd
		pop	eax
		xor	eax,00200000h
		push	eax
		popfd
		pushfd
		pop	ebx
		cmp	eax,ebx
		jnz	.error
; Verificar soporte de flags de funciones CPUID
		xor	eax,eax
		cpuid
		cmp	eax,0
		jz	.error
; Verificar soporte de tecnologia MMX
		mov	eax,1
		cpuid
		and	edx,00800000h
		jz	.error
; MMX presente
		popad
		mov	eax,1
		ret
; MMX no presente
.error:
		popad
		xor	eax,eax
		ret

;
; Verificacion de soporte CMOV
;
; BOOL IsCMOVSupport(void)
;
_IsCMOVSupport:
		pushad
; Verificar presencia de CPUID
		pushfd
		pop	eax
		xor	eax,00200000h
		push	eax
		popfd
		pushfd
		pop	ebx
		cmp	eax,ebx
		jnz	.error
; Verificar soporte de flags de funciones CPUID
		xor	eax,eax
		cpuid
		cmp	eax,0
		jz	.error
; Verificar soporte de CMOV
		mov	eax,1
		cpuid
		and	edx,00008000h
		jz	.error
; CMOV presente
		popad
		mov	eax,1
		ret
; CMOV no presente
.error:
		popad
		xor	eax,eax
		ret

;
; Redimensionamiento de muestras de sonido (MMX)
;
; void SoundMMX(DWORD *pSrc, WORD *pDst, int nBytes)
;
_SoundMMX:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Obtener buffers (pSrc, pDst)
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
; Obtener cantidad (nBytes)
		mov	ecx,[ebp+16]
; largo (unidades de 128 bytes)
		cmp	ecx,128
		jnc	.longchk
		jmp	.shortchk
; Establecer cantidad
.longchk:
		mov	eax,ecx
		shr	eax,7
		and	ecx,byte 127
; Bucle
.longlp:
		movq	mm0,[esi]
		movq	mm1,[esi+16]
		movq	mm2,[esi+32]
		movq	mm3,[esi+48]
		packssdw mm0,[esi+8]
		packssdw mm1,[esi+24]
		packssdw mm2,[esi+40]
		packssdw mm3,[esi+56]
		movq	[edi],mm0
		movq	[edi+8],mm1
		movq	[edi+16],mm2
		movq	[edi+24],mm3
;
		movq	mm0,[esi+64]
		movq	mm1,[esi+80]
		movq	mm2,[esi+96]
		movq	mm3,[esi+112]
		packssdw mm0,[esi+72]
		packssdw mm1,[esi+88]
		packssdw mm2,[esi+104]
		packssdw mm3,[esi+120]
		movq	[edi+32],mm0
		movq	[edi+40],mm1
		movq	[edi+48],mm2
		movq	[edi+56],mm3
;
		movq	mm0,[esi+128]
		movq	mm1,[esi+144]
		movq	mm2,[esi+160]
		movq	mm3,[esi+176]
		packssdw mm0,[esi+136]
		packssdw mm1,[esi+152]
		packssdw mm2,[esi+168]
		packssdw mm3,[esi+184]
		movq	[edi+64],mm0
		movq	[edi+72],mm1
		movq	[edi+80],mm2
		movq	[edi+88],mm3
;
		movq	mm0,[esi+192]
		movq	mm1,[esi+208]
		movq	mm2,[esi+224]
		movq	mm3,[esi+240]
		packssdw mm0,[esi+200]
		packssdw mm1,[esi+216]
		packssdw mm2,[esi+232]
		packssdw mm3,[esi+248]
		movq	[edi+96],mm0
		movq	[edi+104],mm1
		movq	[edi+112],mm2
		movq	[edi+120],mm3
; Siguiente
		add	esi,256
		add	edi,128
		dec	eax
		jnz	.longlp
; corto (unidades de 16 bytes)
.shortchk:
		cmp	ecx,byte 16
		jc	.normalchk
		mov	eax,ecx
		shr	eax,4
		and	ecx,byte 15
; Bucle
.shortlp:
		movq	mm0,[esi]
		movq	mm1,[esi+16]
		packssdw mm0,[esi+8]
		packssdw mm1,[esi+24]
		movq	[edi],mm0
		movq	[edi+8],mm1
; Siguiente
		add	esi,byte 32
		add	edi,byte 16
		dec	eax
		jnz	.shortlp
; normal (sin usar MMX)
.normalchk:
		shr	ecx,1
		or	ecx,ecx
		jz	.exit
		mov	ebx,00007fffh
		mov	edx,0ffff8000h
; Bucle(分岐するケースは少ないと判断。CMOVは使わない)
.normallp:
		mov	eax,[esi]
		cmp	eax,ebx
		jg	.over
		cmp	eax,edx
		jl	.under
; セットしてSiguiente
.next:
		mov	[edi],ax
		add	esi,byte 4
		add	edi,byte 2
		dec	ecx
		jnz	.normallp
; Fin
.exit:
; No realizar EMMS (se delega a SoundEMMS)
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret
; Desbordamiento (Overflow)
.over:
		mov	eax,ebx
		jmp	.next
; Subdesbordamiento (Underflow)
.under:
		mov	eax,edx
		jmp	.next

;
; Redimensionamiento de muestras de sonido (EMMS)
;
; void SoundEMMS()
;
_SoundEMMS:
		emms
		ret

;
; VRAM de texto
; Renderizado
;
; void VideoText(const BYTE *pTVRAM, DWORD *pBits, int nLen, DWORD *pPalette)
;
_VideoText:
		push	ebp
		mov	ebp,esp
		pushad
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
; Bucle
.loop:
		push	ecx
		push	esi
		mov	eax,[esi+0]
		rol	eax,16
		mov	ebx,[esi+0x20000]
		rol	ebx,16
		mov	ecx,[esi+0x40000]
		rol	ecx,16
		mov	edx,[esi+0x60000]
		rol	edx,16
; b31
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi],esi
; b30
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+4],esi
; b29
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+8],esi
; b28
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+12],esi
; b27
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+16],esi
; b26
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+20],esi
; b25
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+24],esi
; b24
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+28],esi
; b23
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+32],esi
; b22
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+36],esi
; b21
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+40],esi
; b20
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+44],esi
; b19
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+48],esi
; b18
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+52],esi
; b17
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+56],esi
; b16
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+60],esi
; b15
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+64],esi
; b14
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+68],esi
; b13
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+72],esi
; b12
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+76],esi
; b11
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+80],esi
; b10
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+84],esi
; b9
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+88],esi
; b8
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+92],esi
; b7
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+96],esi
; b6
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+100],esi
; b5
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+104],esi
; b4
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+108],esi
; b3
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+112],esi
; b2
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+116],esi
; b1
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+120],esi
; b0
		xor	esi,esi
		add	edx,edx
		adc	esi,esi
		add	ecx,ecx
		adc	esi,esi
		add	ebx,ebx
		adc	esi,esi
		add	eax,eax
		adc	esi,esi
		mov	esi,[ebp+esi*4]
		mov	[edi+124],esi
; Al siguiente DWORD
		add	edi,128
		pop	esi
		pop	ecx
		add	esi,4
		dec	ecx
		jnz	near .loop
; Fin
		popad
		pop	ebp
		ret

;
; VRAM de graficos
; 1024×1024Renderizado(ページ0,1)
;
; void VideoG1024A(BYTE *src, DWORD *dst, DWORD *plt)
;
_VideoG1024A:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ebp,[ebp+16]
		mov	ecx,64
		mov	edx,15
; ブロックBucle
.block:
		push	ecx
; +0, +1, +512, +513
		mov	ecx,[esi]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+4],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4+2048],eax
; +2, +3, +514, +515
		mov	ecx,[esi+4]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+12],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12+2048],eax
; +4, +5, +516, +517
		mov	ecx,[esi+8]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+20],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20+2048],eax
; +6, +7, +518, +519
		mov	ecx,[esi+12]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+28],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28+2048],eax
; Al siguiente bloque
		pop	ecx
		add	esi,16
		add	edi,32
		dec	ecx
		jnz	near .block
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 1024×1024Renderizado(ページ2,3)
;
; void VideoG1024B(BYTE *src, DWORD *dst, DWORD *plt)
;
_VideoG1024B:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ebp,[ebp+16]
		mov	ecx,64
		mov	edx,15
; ブロックBucle
.block:
		push	ecx
; +0, +1, +512, +513
		mov	ecx,[esi]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+4],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4+2048],eax
; +2, +3, +514, +515
		mov	ecx,[esi+4]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+12],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12+2048],eax
; +4, +5, +516, +517
		mov	ecx,[esi+8]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+20],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20+2048],eax
; +6, +7, +518, +519
		mov	ecx,[esi+12]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24+2048],eax
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+28],eax
		shr	ecx,4
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28+2048],eax
; Al siguiente bloque
		pop	ecx
		add	esi,16
		add	edi,32
		dec	ecx
		jnz	near .block
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 16色Renderizado(ページ0)
;
; void VideoG16A(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG16A:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	.next
		push	ecx
; ブロックBucle
.block:
		push	edx
		mov	edx,15
; +0, +1
		mov	ecx,[esi]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		pop	edx
		add	esi,16
		add	edi,32
		dec	edx
		jnz	.block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		mov	ebx,[esi]
		and	ebx,15
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 16色Renderizado(ページ1)
;
; void VideoG16B(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG16B:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	near .next
		push	ecx
; ブロックBucle
.block:
		push	edx
		mov	edx,15
; +0, +1
		mov	ecx,[esi]
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		shr	ecx,4
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		pop	edx
		add	esi,16
		add	edi,32
		dec	edx
		jnz	near .block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		mov	ebx,[esi]
		shr	ebx,4
		and	ebx,15
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 16色Renderizado(ページ2)
;
; void VideoG16C(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG16C:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	near .next
		push	ecx
; ブロックBucle
.block:
		push	edx
		mov	edx,15
; +0, +1
		mov	ecx,[esi]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		shr	ecx,8
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		pop	edx
		add	esi,16
		add	edi,32
		dec	edx
		jnz	near .block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		mov	ebx,[esi]
		shr	ebx,8
		and	ebx,15
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 16色Renderizado(ページ3)
;
; void VideoG16D(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG16D:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	near .next
		push	ecx
; ブロックBucle
.block:
		push	edx
		mov	edx,15
; +0, +1
		mov	ecx,[esi]
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		shr	ecx,12
		mov	ebx,edx
		and	ebx,ecx
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		pop	edx
		add	esi,16
		add	edi,32
		dec	edx
		jnz	near .block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		mov	ebx,[esi]
		shr	ebx,12
		and	ebx,15
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 256色Renderizado(ページ0)
;
; void VideoG256A(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG256A:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	near .next
		push	ecx
; ブロックBucle
.block:
		push	edx
		mov	edx,255
; +0, +1
		mov	ecx,[esi]
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		and	ecx,edx
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		pop	edx
		add	esi,16
		add	edi,32
		dec	edx
		jnz	.block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		movzx	ebx,byte[esi]
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 256色Renderizado(ページ1)
;
; void VideoG256B(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG256B:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	.next
		push	ecx
; ブロックBucle
.block:
; +0, +1
		mov	ecx,[esi]
		shr	ecx,8
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		shr	ecx,16
		mov	eax,[ebp+ecx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ecx,[esi+4]
		shr	ecx,8
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+8],eax
		shr	ecx,16
		mov	eax,[ebp+ecx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ecx,[esi+8]
		shr	ecx,8
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+16],eax
		shr	ecx,16
		mov	eax,[ebp+ecx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ecx,[esi+12]
		shr	ecx,8
		movzx	ebx,cl
		mov	eax,[ebp+ebx*4]
		mov	[edi+24],eax
		shr	ecx,16
		mov	eax,[ebp+ecx*4]
		mov	[edi+28],eax
; Siguiente
		add	esi,16
		add	edi,32
		dec	edx
		jnz	.block
		pop	ecx
; 余りBucle
.next:
		jecxz	.exit
		movzx	ebx,byte[esi+1]
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; VRAM de graficos
; 65536色Renderizado
;
; void VideoG64K(BYTE *src, DWORD *dst, int len, DWORD *plt)
;
_VideoG64K:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ecx,[ebp+16]
		mov	ebp,[ebp+20]
		mov	edx,ecx
		and	ecx,byte 7
		shr	edx,3
		jz	.next
; ブロックBucle
.block:
; +0, +1
		mov	ebx,[esi]
		movzx	eax,bx
		mov	eax,[ebp+eax*4]
		mov	[edi],eax
		shr	ebx,16
		mov	eax,[ebp+ebx*4]
		mov	[edi+4],eax
; +2, +3
		mov	ebx,[esi+4]
		movzx	eax,bx
		mov	eax,[ebp+eax*4]
		mov	[edi+8],eax
		shr	ebx,16
		mov	eax,[ebp+ebx*4]
		mov	[edi+12],eax
; +4, +5
		mov	ebx,[esi+8]
		movzx	eax,bx
		mov	eax,[ebp+eax*4]
		mov	[edi+16],eax
		shr	ebx,16
		mov	eax,[ebp+ebx*4]
		mov	[edi+20],eax
; +6, +7
		mov	ebx,[esi+12]
		movzx	eax,bx
		mov	eax,[ebp+eax*4]
		mov	[edi+24],eax
		shr	ebx,16
		mov	eax,[ebp+ebx*4]
		mov	[edi+28],eax
; Siguiente
		add	esi,16
		add	edi,32
		dec	edx
		jnz	.block
; 余りBucle
.next:
		jecxz	.exit
		movzx	ebx,word[esi]
		mov	eax,[ebp+ebx*4]
		mov	[edi],eax
		add	esi,2
		add	edi,4
		dec	ecx
		jmp	short .next
; Fin
.exit:
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; PCGRenderizado
;
; void VideoPCG(BYTE *src, DWORD *dst, DWORD *plt)
;
_VideoPCG:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ebp,[ebp+16]
		mov	ecx,32
		mov	ebx,15
; ブロックBucle
.loop:
		mov	eax,[esi]
		rol	eax,16
; +0
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi],edx
; +1
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+4],edx
; +2
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+8],edx
; +3
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+12],edx
; +4
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+16],edx
; +5
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+20],edx
; +6
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+24],edx
; +7
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+28],edx
; Al siguiente bloque(8x8で+2)
		add	esi,40h
		add	edi,32
		dec	ecx
		jnz	near .loop
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; BG(16x16)Renderizado
;
; void VideoBG16(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt)
;
_VideoBG16:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ebx,[ebp+16]
		mov	ecx,[ebp+20]
		mov	ebp,[ebp+24]
; Configuracion de paleta
		mov	eax,ebx
		shr	eax,2
		and	eax,03c0h
		add	ebp,eax
; Configuracion de PCG
		mov	eax,ebx
		shl	eax,7		; x20->5bit
		and	eax,7f80h	; あわせて変更
		add	esi,eax
; Obtener el offset Y correcto
		test	ebx,8000h
		jz	.offset
; Inversion Y. 15 - ECX es correcto
		sub	ecx,15
		neg	ecx
.offset:
		test	ecx,8
		jz	.upper
; Mitad inferior (Bloques 1 y 3)
		and	ecx,7
		add	esi,20h
; Comun para mitad superior e inferior. Sumar Y normalizada
.upper:
		shl	ecx,2
		add	esi,ecx
; Aqui la inversion horizontal es una bifurcacion
		test	ebx,4000h
		jnz	near .reverse
; Inicio normal horizontal
		mov	ebx,15
		mov	eax,[esi]
		rol	eax,16
; +0
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi],edx
; +1
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+4],edx
; +2
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+8],edx
; +3
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+12],edx
; +4
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+16],edx
; +5
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+20],edx
; +6
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+24],edx
; +7
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+28],edx
; Siguiente busqueda (fetch)
		mov	eax,[esi+64]
		rol	eax,16
; +8
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+32],edx
; +9
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+36],edx
; +10
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+40],edx
; +11
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+44],edx
; +12
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+48],edx
; +13
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+52],edx
; +14
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+56],edx
; +15
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+60],edx
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret
; Inicio de inversion horizontal
.reverse:
		mov	ebx,15
		mov	eax,[esi]
		rol	eax,16
; +0
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+60],edx
; +1
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+56],edx
; +2
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+52],edx
; +3
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+48],edx
; +4
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+44],edx
; +5
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+40],edx
; +6
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+36],edx
; +7
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+32],edx
; Siguiente busqueda (fetch)
		mov	eax,[esi+64]
		rol	eax,16
; +8
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+28],edx
; +9
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+24],edx
; +10
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+20],edx
; +11
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+16],edx
; +12
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+12],edx
; +13
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+8],edx
; +14
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+4],edx
; +15
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi],edx
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; BG(8x8)Renderizado
;
; void VideoBG8(BYTE *pcg, DWORD *dst, DWORD bg, int y, DWORD *plt)
;
_VideoBG8:
		push	ebp
		mov	ebp,esp
		push	eax
		push	ebx
		push	ecx
		push	edx
		push	esi
		push	edi
; Configuracion de direcciones
		mov	esi,[ebp+8]
		mov	edi,[ebp+12]
		mov	ebx,[ebp+16]
		mov	ecx,[ebp+20]
		mov	ebp,[ebp+24]
; Configuracion de paleta
		mov	eax,ebx
		shr	eax,2
		and	eax,03c0h
		add	ebp,eax
; Configuracion de PCG
		mov	eax,ebx
		shl	eax,5
		and	eax,1fe0h
		add	esi,eax
; Obtener el offset Y correcto
		test	ebx,8000h
		jz	.offset
; Inversion Y. 7 - ECX es correcto
		sub	ecx,7
		neg	ecx
; Sumar Y normalizada
.offset:
		shl	ecx,2
		add	esi,ecx
; Aqui la inversion horizontal es una bifurcacion
		test	ebx,4000h
		jnz	near .reverse
; Inicio normal horizontal
		mov	ebx,15
		mov	eax,[esi]
		rol	eax,16
; +0
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi],edx
; +1
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+4],edx
; +2
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+8],edx
; +3
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+12],edx
; +4
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+16],edx
; +5
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+20],edx
; +6
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+24],edx
; +7
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+28],edx
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret
; Inversion en direccion X
.reverse:
		mov	ebx,15
		mov	eax,[esi]
		rol	eax,16
; +0
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+28],edx
; +1
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+24],edx
; +2
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+20],edx
; +3
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+16],edx
; +4
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+12],edx
; +5
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+8],edx
; +6
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi+4],edx
; +7
		rol	eax,4
		mov	edx,eax
		and	edx,ebx
		mov	edx,[ebp+edx*4]
		mov	[edi],edx
; Fin
		pop	edi
		pop	esi
		pop	edx
		pop	ecx
		pop	ebx
		pop	eax
		pop	ebp
		ret

;
; プログラムFin
;
		end
