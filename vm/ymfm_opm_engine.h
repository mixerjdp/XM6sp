#ifndef YMFM_OPM_ENGINE_H
#define YMFM_OPM_ENGINE_H

#include "os.h"
#include "opm.h"

#if defined(XM6CORE_ENABLE_YMFM)

BOOL YmfmOpmEngineSupportsRate(uint clock, uint rate);
FM::OPM *CreateYmfmOpmEngine(BOOL direct_mode = FALSE);

#endif

#endif	// YMFM_OPM_ENGINE_H
