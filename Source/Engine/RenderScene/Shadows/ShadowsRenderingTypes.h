#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class Texture;
} // spt::rdr


namespace spt::rsc
{

struct ShadowMapViewComponent
{
	lib::SharedPtr<rdr::Texture>		shadowMap;
	RenderSceneEntity					owningLight;
	Uint32								faceIdx;
};

} // spt::rsc