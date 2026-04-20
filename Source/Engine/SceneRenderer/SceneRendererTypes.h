#pragma once

#include "SceneRendererMacros.h"
#include "SculptorCoreTypes.h"
#include "MathUtils.h"
#include "Types/Texture.h"


namespace spt::gfx
{
class DebugRenderer;
} // spt::gfx


namespace spt::rsc
{

class RenderScene;


struct SceneRendererSettings
{
	rhi::EFragmentFormat outputFormat = rhi::EFragmentFormat::None;
	Bool resetAccumulation            = false;

	lib::SharedPtr<rdr::TextureView> debugUAVTexture;
	gfx::DebugRenderer* dynamicDebugRenderer = nullptr;
	gfx::DebugRenderer* persistentDebugRenderer = nullptr;
};


enum class ERenderStage : Flags32
{
	None							= 0,
	PreRendering					= BIT(0),
	GlobalIllumination				= BIT(1),
	ShadowMap						= BIT(2),
	DepthPrepass					= BIT(3),
	VisibilityBuffer				= BIT(4),
	MotionAndDepth					= BIT(5),
	DownsampleGeometryTextures		= BIT(6),
	AmbientOcclusion				= BIT(7), // not used
	DirectionalLightsShadowMasks	= BIT(8),
	ForwardOpaque					= BIT(9),
	DeferredShading					= BIT(10),
	SpecularReflections				= BIT(11),
	CompositeLighting				= BIT(12),
	Transparency					= BIT(13),
	PostProcessPreAA				= BIT(14),
	AntiAliasing					= BIT(15),
	HDRResolve						= BIT(16),

	NUM								= 17,

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	VisibilityBufferStages			= VisibilityBuffer | MotionAndDepth,
	DeferredLightingStages			= GlobalIllumination | DownsampleGeometryTextures | DeferredShading | SpecularReflections | CompositeLighting | Transparency,
	PostProcessStages				= PostProcessPreAA | AntiAliasing | HDRResolve,

	// Presets
	DeferredRendererStages			= PreRendering | VisibilityBufferStages | DeferredLightingStages | PostProcessStages,
	ShadowMapRendererStages			= ShadowMap,
};


inline Uint32 RenderStageToIdx(ERenderStage stage)
{
	const Uint32 idx = math::Utils::LowestSetBitIdx(static_cast<Flags32>(stage));
	SPT_CHECK_MSG(1u << idx == static_cast<Uint32>(stage), "Multiple stage bits are set! stage = {}", static_cast<Flags32>(stage));
	return idx;
}


inline ERenderStage RenderStageByIdx(Uint32 idx)
{
	SPT_CHECK(idx < static_cast<Uint32>(ERenderStage::NUM));
	return static_cast<ERenderStage>(1u << idx);
}


enum class EViewRenderSystem : Flags32
{
	None                     = 0,
	ParticipatingMediaSystem = BIT(0),
	TemporalAASystem         = BIT(1),
	NUM                      = 2,

	ALL = (1u << NUM) - 1
};


inline Uint32 GetViewRenderSystemIdx(EViewRenderSystem renderSystem)
{
	 const Uint32 idx = math::Utils::LowestSetBitIdx(static_cast<Flags32>(renderSystem));
	 SPT_CHECK_MSG(1u << idx == static_cast<Uint32>(renderSystem), "Multiple system bits are set! renderSystem = {}", static_cast<Flags32>(renderSystem));
	 return idx;
}

inline EViewRenderSystem GetViewRenderSystemByIdx(Uint32 idx)
{
	SPT_CHECK(idx < static_cast<Uint32>(EViewRenderSystem::NUM));
	return static_cast<EViewRenderSystem>(1u << idx);
}


enum class ESceneRenderSystem : Flags32
{
	None               = 0,
	ShadowMapsSystem   = BIT(0),
	WorldShadowCache   = BIT(1),
	StaticMeshesSystem = BIT(2),
	LightsSystem       = BIT(3),
	DDGISystem         = BIT(4),
	RayTracingSystem   = BIT(5),
	AtmosphereSystem   = BIT(6),
	TerrainSystem      = BIT(7),
	NUM                = 8,

	ALL = (1u << NUM) - 1
};


inline Uint32 GetSceneRenderSystemIdx(ESceneRenderSystem renderSystem)
{
	 const Uint32 idx = math::Utils::LowestSetBitIdx(static_cast<Flags32>(renderSystem));
	 SPT_CHECK_MSG(1u << idx == static_cast<Uint32>(renderSystem), "Multiple system bits are set! renderSystem = {}", static_cast<Flags32>(renderSystem));
	 return idx;
}


inline ESceneRenderSystem GetSceneRenderSystemByIdx(Uint32 idx)
{
	SPT_CHECK(idx < static_cast<Uint32>(ESceneRenderSystem::NUM));
	return static_cast<ESceneRenderSystem>(1u << idx);
}


static constexpr SizeType MaxViewRenderSystemsNum  = static_cast<SizeType>(EViewRenderSystem::NUM);
static constexpr SizeType MaxRenderStagesNum       = static_cast<SizeType>(ERenderStage::NUM);
static constexpr SizeType MaxSceneRenderSystemsNum = static_cast<SizeType>(ESceneRenderSystem::NUM);


struct RenderViewDefinition
{
	ERenderStage      renderStages = ERenderStage::None;
	EViewRenderSystem renderSystems = EViewRenderSystem::None;
};


struct SceneRendererDefinition
{
	RenderScene&       scene;
	ESceneRenderSystem renderSystems = ESceneRenderSystem::None;
};


enum class EUpscalingMethod
{
	None,
	DLSS,
	TAA,
	Accumulation
};


struct ShadingRenderViewSettings
{
	EUpscalingMethod upscalingMethod = EUpscalingMethod::None;
};

} // spt::rsc
