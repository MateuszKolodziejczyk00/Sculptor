#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rsc
{

enum class ERenderStage
{
	None							= 0,
	GlobalIllumination				= BIT(0),
	ShadowMap						= BIT(1),
	DepthPrepass					= BIT(2),
	MotionAndDepth					= BIT(3),
	GenerateGeometryNormals			= BIT(4),
	AmbientOcclusion				= BIT(5),
	DirectionalLightsShadowMasks	= BIT(6),
	ForwardOpaque					= BIT(7),
	ApplyAtmosphere					= BIT(8),
	VolumetricFog					= BIT(9),
	PostProcessPreAA				= BIT(10),
	AntiAliasing					= BIT(10),
	HDRResolve						= BIT(11),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	DepthPrepassStages				= DepthPrepass | MotionAndDepth,
	ForwardLightingStages			= GlobalIllumination | GenerateGeometryNormals | AmbientOcclusion | ForwardOpaque | ApplyAtmosphere | VolumetricFog,
	PostProcessStages				= PostProcessPreAA | AntiAliasing | HDRResolve,

	// Presets
	ForwardRendererStages			= DepthPrepassStages | ForwardLightingStages | PostProcessStages,
	ShadowMapRendererStages			= ShadowMap,
};


struct SceneRendererSettings
{
	SceneRendererSettings()
		: outputFormat(rhi::EFragmentFormat::None)
	{ }

	rhi::EFragmentFormat outputFormat;
};

} // spt::rsc