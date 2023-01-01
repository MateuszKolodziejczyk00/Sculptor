#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"

namespace spt::rdr
{

class RENDERER_CORE_API RendererSettings
{
public:
	
	RendererSettings();

	static const RendererSettings& Get();

	Uint32 framesInFlight;
};

} // spt::rdr