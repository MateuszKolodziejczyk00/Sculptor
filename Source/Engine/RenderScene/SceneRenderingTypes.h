#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None							= 0,
	ShadowMap						= BIT(0),
	DepthPrepass					= BIT(1),
	DirectionalLightsShadowMasks	= BIT(2),
	ForwardOpaque					= BIT(3),
	HDRResolve						= BIT(4)
};

} // spt::rsc