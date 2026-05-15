//---------------------------------------------------------------------------
//
//	X68000 Emulator "XM6"
//
//	Video engine abstraction.
//
//---------------------------------------------------------------------------

#include "os.h"
#include "xm6.h"
#include "sprite.h"
#include "render.h"
#include "graphic_engine.h"

void FASTCALL GraphicEngine::ProcessCommon(Render *owner)
{
	int i;

	if (owner->render.first >= owner->render.last) {
		return;
	}

	if (owner->render.vc) {
		owner->ComposeVideo();
	}

	if (owner->render.contrast) {
		owner->Contrast();
	}

	if (owner->render.palette) {
		owner->Palette();
	}

	if (owner->render.first == 0) {
		if (owner->sprite->IsDisplay()) {
			if (!owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = TRUE;
			}
		}
		else {
			if (owner->render.bgspdisp) {
				for (i = 0; i < 512; i++) {
					owner->render.bgspmod[i] = TRUE;
				}
				owner->render.bgspdisp = FALSE;
			}
		}
	}

	if ((owner->render.v_mul == 2) && !owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			if ((i & 1) == 0) {
				owner->CaptureVideoSnapshotLine(i >> 1);
				owner->Text(i >> 1);
				owner->Grp(0, i >> 1);
				owner->Grp(1, i >> 1);
				owner->Grp(2, i >> 1);
				owner->Grp(3, i >> 1);
				owner->BGSprite(i >> 1);
				owner->Mix(i >> 1);
			}
		}
		owner->render.first = owner->render.last;
		return;
	}

	if ((owner->render.v_mul == 0) && owner->render.lowres) {
		for (i = owner->render.first; i < owner->render.last; i++) {
			owner->CaptureVideoSnapshotLine(i << 1);
			owner->Text((i << 1) + 0);
			owner->CaptureVideoSnapshotLine((i << 1) + 1);
			owner->Text((i << 1) + 1);
			owner->Grp(0, (i << 1) + 0);
			owner->Grp(0, (i << 1) + 1);
			owner->Grp(1, (i << 1) + 0);
			owner->Grp(1, (i << 1) + 1);
			owner->Grp(2, (i << 1) + 0);
			owner->Grp(2, (i << 1) + 1);
			owner->Grp(3, (i << 1) + 0);
			owner->Grp(3, (i << 1) + 1);
			owner->BGSprite((i << 1) + 0);
			owner->BGSprite((i << 1) + 1);
			owner->Mix((i << 1) + 0);
			owner->Mix((i << 1) + 1);
		}
		owner->render.first = owner->render.last;
		return;
	}

	for (i = owner->render.first; i < owner->render.last; i++) {
		owner->CaptureVideoSnapshotLine(i);
		owner->Text(i);
		owner->Grp(0, i);
		owner->Grp(1, i);
		owner->Grp(2, i);
		owner->Grp(3, i);
		owner->BGSprite(i);
		owner->Mix(i);
	}
	owner->render.first = owner->render.last;
}

void FASTCALL OriginalGraphicEngine::StartFrame(Render *owner)
{
	owner->StartFrameOriginal();
}

void FASTCALL OriginalGraphicEngine::EndFrame(Render *owner)
{
	owner->EndFrameOriginal();
}

void FASTCALL OriginalGraphicEngine::HSync(Render *owner, int raster)
{
	owner->HSyncOriginal(raster);
}

void FASTCALL OriginalGraphicEngine::SetCRTC(Render *owner)
{
	owner->SetCRTCOriginal();
}

void FASTCALL OriginalGraphicEngine::SetVC(Render *owner)
{
	owner->SetVCOriginal();
}

void FASTCALL OriginalGraphicEngine::Video(Render *owner)
{
	owner->Video();
}

void FASTCALL OriginalGraphicEngine::Process(Render *owner)
{
	ProcessCommon(owner);
}

void FASTCALL Px68kGraphicEngine::StartFrame(Render *owner)
{
	owner->StartFrameFast();
}

void FASTCALL Px68kGraphicEngine::EndFrame(Render *owner)
{
	owner->EndFrameFast();
}

void FASTCALL Px68kGraphicEngine::HSync(Render *owner, int raster)
{
	owner->HSyncFast(raster);
}

void FASTCALL Px68kGraphicEngine::SetCRTC(Render *owner)
{
	owner->SetCRTCFast();
}

void FASTCALL Px68kGraphicEngine::SetVC(Render *owner)
{
	owner->SetVCFast();
}

void FASTCALL Px68kGraphicEngine::Video(Render *owner)
{
	owner->VideoFastPX68K();
}

void FASTCALL Px68kGraphicEngine::Process(Render *owner)
{
	owner->ProcessFast();
}
