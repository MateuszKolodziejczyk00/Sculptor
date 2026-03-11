#include "GlobalResources.h"
#include "Engine.h"


namespace spt::gfx::global
{

static engn::TEngineSingleton<Resources> resourcesInstance;

const Resources& Resources::Get()
{
	return resourcesInstance.Get();
}

Resources::Resources()
	: blueNoise256("Textures/Bluenoise256.png")
{
}

} // spt::gfx::global
