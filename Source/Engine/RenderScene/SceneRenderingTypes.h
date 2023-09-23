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
	DownsampleGeometryTextures		= BIT(5),
	AmbientOcclusion				= BIT(6),
	DirectionalLightsShadowMasks	= BIT(7),
	ForwardOpaque					= BIT(8),
	ApplyAtmosphere					= BIT(9),
	VolumetricFog					= BIT(10),
	PostProcessPreAA				= BIT(11),
	AntiAliasing					= BIT(12),
	HDRResolve						= BIT(13),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	DepthPrepassStages				= DepthPrepass | MotionAndDepth,
	ForwardLightingStages			= GlobalIllumination | GenerateGeometryNormals | DownsampleGeometryTextures | AmbientOcclusion | ForwardOpaque | ApplyAtmosphere | VolumetricFog,
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