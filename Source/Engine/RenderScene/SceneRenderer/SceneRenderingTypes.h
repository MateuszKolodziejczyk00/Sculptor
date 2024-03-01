#pragma once

#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructsMacros.h"
#include "RGResources/RGResourceHandles.h"


namespace spt::rsc
{

class RenderView;


enum class ERenderStage : Flags32
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
	SpecularReflections				= BIT(9),
	ApplyAtmosphere					= BIT(10),
	VolumetricFog					= BIT(11),
	PostProcessPreAA				= BIT(12),
	AntiAliasing					= BIT(13),
	HDRResolve						= BIT(14),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	DepthPrepassStages				= DepthPrepass | MotionAndDepth,
	ForwardLightingStages			= GlobalIllumination | GenerateGeometryNormals | DownsampleGeometryTextures | AmbientOcclusion | ForwardOpaque | SpecularReflections | ApplyAtmosphere | VolumetricFog,
	PostProcessStages				= PostProcessPreAA | AntiAliasing | HDRResolve,

	// Presets
	ForwardRendererStages			= DepthPrepassStages | ForwardLightingStages | PostProcessStages,
	ShadowMapRendererStages			= ShadowMap,
};


inline Uint32 RenderStageToIndex(ERenderStage stage)
{
	 const Uint32 idx = math::Utils::LowestSetBitIdx(static_cast<Flags32>(stage));
	 SPT_CHECK_MSG(1u << idx == static_cast<Uint32>(stage), "Multiple stage bits are set! stage = {}", static_cast<Flags32>(stage));
	 return idx;
}


struct SceneRendererSettings
{
	SceneRendererSettings()
		: outputFormat(rhi::EFragmentFormat::None)
	{ }

	rhi::EFragmentFormat outputFormat;
};


class RenderViewsCollector
{
public:

	RenderViewsCollector()
	{
		m_addedRenderViews.reserve(4u);
		m_newRenderViews.reserve(4u);
	}

	void AddRenderView(RenderView& renderView)
	{
		const auto emplaceResult = m_addedRenderViews.emplace(&renderView);
		if (emplaceResult.second)
		{
			m_newRenderViews.emplace_back(&renderView);
		}
	}

	SizeType GetViewsNum() const
	{
		return m_addedRenderViews.size();
	}

	lib::DynamicArray<RenderView*> ExtractNewRenderViews()
	{
		lib::DynamicArray<RenderView*> extractedViews = std::move(m_newRenderViews);
		return extractedViews;
	}

	Bool HasAnyViewsToExtract() const
	{
		return !m_newRenderViews.empty();
	}

private:

	lib::DynamicArray<RenderView*>	m_newRenderViews;
	lib::HashSet<RenderView*>		m_addedRenderViews;
};


struct SceneRendererStatics
{
	static constexpr rhi::EFragmentFormat hdrFormat = rhi::EFragmentFormat::RGBA16_S_Float;
};


BEGIN_SHADER_STRUCT(DepthCullingParams)
	SHADER_STRUCT_FIELD(math::Vector2f, hiZResolution)
END_SHADER_STRUCT();


DS_BEGIN(DepthCullingDS, rg::RGDescriptorSetState<DepthCullingDS>)
	DS_BINDING(BINDING_TYPE(gfx::SRVTexture2DBinding<Real32>),                                      u_hiZTexture)
	DS_BINDING(BINDING_TYPE(gfx::ImmutableSamplerBinding<rhi::SamplerState::LinearMinClampToEdge>), u_hiZSampler)
	DS_BINDING(BINDING_TYPE(gfx::ConstantBufferBinding<DepthCullingParams>),                        u_depthCullingParams)
DS_END();

} // spt::rsc