#pragma once

#include "SculptorCoreTypes.h"
#include "GlobalResources/GlobalResourcesRegistry.h"


namespace spt::gfx::global
{

class GRAPHICS_API Resources
{
public:

	Resources();

	static const Resources& Get();

	GlobalTexture blueNoise256;
};

} // spt::gfx::global