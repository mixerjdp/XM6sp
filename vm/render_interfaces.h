//---------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Reusable render interfaces for the video pipeline.
//
//---------------------------------------------------------------------------

#if !defined(render_interfaces_h)
#define render_interfaces_h

#include "os.h"

class CRTC;
class GVRAM;
class Sprite;
class TVRAM;
class VC;

class IRenderTarget
{
public:
	virtual ~IRenderTarget() {}
	virtual void FASTCALL DrawLine() = 0;
	virtual void FASTCALL DrawFrame() = 0;
};

class IPaletteResolver
{
public:
	virtual ~IPaletteResolver() {}
	virtual const DWORD* FASTCALL GetPalette() const = 0;
};

class IVideoStateView
{
public:
	virtual ~IVideoStateView() {}
	virtual const CRTC* FASTCALL GetCRTCDevice() const = 0;
	virtual const VC* FASTCALL GetVCDevice() const = 0;
	virtual const TVRAM* FASTCALL GetTVRAMDevice() const = 0;
	virtual const GVRAM* FASTCALL GetGVRAMDevice() const = 0;
	virtual const Sprite* FASTCALL GetSpriteDevice() const = 0;
	virtual const DWORD* FASTCALL GetTextBuf() const = 0;
	virtual const DWORD* FASTCALL GetGrpBuf(int index) const = 0;
	virtual const DWORD* FASTCALL GetBGSpBuf() const = 0;
	virtual const DWORD* FASTCALL GetMixBuf() const = 0;
};

#endif	// render_interfaces_h
