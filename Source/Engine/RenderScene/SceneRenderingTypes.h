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
	PostProcessPreAA				= BIT(6),
	AntiAliasing					= BIT(7),
	HDRResolve						= BIT(8)
};

} // spt::rsc