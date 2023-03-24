#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None							= 0,
	ShadowMap						= BIT(0),
	DepthPrepass					= BIT(1),
	MotionAndDepth					= BIT(2),
	DirectionalLightsShadowMasks	= BIT(3),
	ForwardOpaque					= BIT(4),
	AntiAliasing					= BIT(5),
	HDRResolve						= BIT(6)
};

} // spt::rsc