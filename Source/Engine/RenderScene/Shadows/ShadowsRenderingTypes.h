#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

struct ShadowMapViewComponent
{
	lib::SharedPtr<rdr::TextureView>	shadowMapView;
	RenderSceneEntity					owningLight;
};

} // spt::rsc