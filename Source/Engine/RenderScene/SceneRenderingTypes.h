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
	VolumetricFog					= BIT(5),
	AntiAliasing					= BIT(6),
	HDRResolve						= BIT(7)
};

} // spt::rsc