//--------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Adapter renderer for the XM VM timing path.
//
//--------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "x68krender.h"

X68kRender::X68kRender(VM *p) : Render(p)
{
	dev.desc = "X68kRender";
}

X68kRender::~X68kRender()
{
}

BOOL FASTCALL X68kRender::Init()
{
	return Render::Init();
}

void FASTCALL X68kRender::Cleanup()
{
	Render::Cleanup();
}

void FASTCALL X68kRender::Reset()
{
	Render::Reset();
}

void FASTCALL X68kRender::StartFrame()
{
	Render::StartFrame();
}

void FASTCALL X68kRender::EndFrame()
{
	Render::EndFrame();

	IRenderTarget *target = GetRenderTarget();
	if (target) {
		target->DrawFrame();
	}
}

void FASTCALL X68kRender::HSync(int raster)
{
	Render::HSync(raster);

	IRenderTarget *target = GetRenderTarget();
	if (target) {
		target->DrawLine();
	}
}

void FASTCALL X68kRender::SetCRTC()
{
	Render::SetCRTC();
}

void FASTCALL X68kRender::SetVC()
{
	Render::SetVC();
}
