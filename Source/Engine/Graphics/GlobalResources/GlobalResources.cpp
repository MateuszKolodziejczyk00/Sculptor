#include "GlobalResources.h"


namespace spt::gfx::global
{

static Resources resourcesInstance;

const Resources& Resources::Get()
{
	return resourcesInstance;
}

Resources::Resources()
	: blueNoise256("Textures/Bluenoise256.png")
{
}

} // spt::gfx::global