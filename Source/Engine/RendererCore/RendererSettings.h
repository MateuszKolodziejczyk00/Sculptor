#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rdr
{

class RendererSettings
{
public:

	RendererSettings();

	static const RendererSettings& Get();

	Uint32 framesInFlight;
};

} // spt::rdr