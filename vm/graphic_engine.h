//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Video engine abstraction.
//
//---------------------------------------------------------------------------

#if !defined(graphic_engine_h)
#define graphic_engine_h

#include "os.h"

class Render;

class GraphicEngine
{
public:
	virtual ~GraphicEngine() {}

	virtual void FASTCALL StartFrame(Render *owner) = 0;
	virtual void FASTCALL EndFrame(Render *owner) = 0;
	virtual void FASTCALL HSync(Render *owner, int raster) = 0;
	virtual void FASTCALL SetCRTC(Render *owner) = 0;
	virtual void FASTCALL SetVC(Render *owner) = 0;
	virtual void FASTCALL Video(Render *owner) = 0;
	virtual void FASTCALL Process(Render *owner) = 0;

protected:
	static void FASTCALL ProcessCommon(Render *owner);
};

class OriginalGraphicEngine : public GraphicEngine
{
public:
	virtual void FASTCALL StartFrame(Render *owner);
	virtual void FASTCALL EndFrame(Render *owner);
	virtual void FASTCALL HSync(Render *owner, int raster);
	virtual void FASTCALL SetCRTC(Render *owner);
	virtual void FASTCALL SetVC(Render *owner);
	virtual void FASTCALL Video(Render *owner);
	virtual void FASTCALL Process(Render *owner);
};

class Px68kGraphicEngine : public GraphicEngine
{
public:
	virtual void FASTCALL StartFrame(Render *owner);
	virtual void FASTCALL EndFrame(Render *owner);
	virtual void FASTCALL HSync(Render *owner, int raster);
	virtual void FASTCALL SetCRTC(Render *owner);
	virtual void FASTCALL SetVC(Render *owner);
	virtual void FASTCALL Video(Render *owner);
	virtual void FASTCALL Process(Render *owner);
};

#endif	// graphic_engine_h
