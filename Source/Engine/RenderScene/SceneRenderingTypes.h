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
	VolumetricFog					= BIT(7),
	PostProcessPreAA				= BIT(8),
	AntiAliasing					= BIT(9),
	HDRResolve						= BIT(10),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	DepthPrepassStages				= DepthPrepass | MotionAndDepth,
	ForwardLightingStages			= GenerateGeometryNormals | AmbientOcclusion | ForwardOpaque | VolumetricFog,
	PostProcessStages				= PostProcessPreAA | AntiAliasing | HDRResolve,

	// Presets
	ForwardRendererStages			= DepthPrepassStages | ForwardLightingStages | PostProcessStages,
	ShadowMapRendererStages			= ShadowMap,
};

} // spt::rsc