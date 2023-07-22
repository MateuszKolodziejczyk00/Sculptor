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
	GenerateGeometryNormals			= BIT(3),
	AmbientOcclusion				= BIT(4),
	DirectionalLightsShadowMasks	= BIT(5),
	ForwardOpaque					= BIT(6),
	ApplyAtmosphere					= BIT(7),
	VolumetricFog					= BIT(8),
	PostProcessPreAA				= BIT(9),
	AntiAliasing					= BIT(10),
	HDRResolve						= BIT(11),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	DepthPrepassStages				= DepthPrepass | MotionAndDepth,
	ForwardLightingStages			= GenerateGeometryNormals | AmbientOcclusion | ForwardOpaque | ApplyAtmosphere | VolumetricFog,
	PostProcessStages				= PostProcessPreAA | AntiAliasing | HDRResolve,

	// Presets
	ForwardRendererStages			= DepthPrepassStages | ForwardLightingStages | PostProcessStages,
	ShadowMapRendererStages			= ShadowMap,
};

} // spt::rsc