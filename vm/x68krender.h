//--------------------------------------------------------------------------
//
//	X68000 EMULATOR "XM6"
//
//	Adapter renderer for the XM VM timing path.
//
//--------------------------------------------------------------------------

#if !defined(x68krender_h)
#define x68krender_h

#include "render.h"

class X68kRender : public Render
{
public:
	X68kRender(VM *p);
	virtual ~X68kRender();

	BOOL FASTCALL Init() override;
	void FASTCALL Cleanup() override;
	void FASTCALL Reset() override;

	void FASTCALL StartFrame() override;
	void FASTCALL EndFrame() override;
	void FASTCALL HSync(int raster) override;
	void FASTCALL SetCRTC() override;
	void FASTCALL SetVC() override;
};

#endif	// x68krender_h
