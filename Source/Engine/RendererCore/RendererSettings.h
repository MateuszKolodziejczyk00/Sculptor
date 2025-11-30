#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Serialization.h"

namespace spt::rdr
{

class RENDERER_CORE_API RendererSettings
{
public:
	
	RendererSettings();

	static const RendererSettings& Get();

	Uint32 framesInFlight;

	void Serialize(srl::Serializer& serializer)
	{
		serializer.Serialize("FramesInFlightNum", framesInFlight);
	}
};

} // spt::rdr