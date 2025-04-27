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
	VisibilityBuffer				= BIT(3),
	MotionAndDepth					= BIT(4),
	DownsampleGeometryTextures		= BIT(5),
	AmbientOcclusion				= BIT(6),
	DirectionalLightsShadowMasks	= BIT(7),
	ForwardOpaque					= BIT(8),
	DeferredShading					= BIT(9),
	SpecularReflections				= BIT(10),
	CompositeLighting				= BIT(11),
	PostProcessPreAA				= BIT(12),
	AntiAliasing					= BIT(13),
	HDRResolve						= BIT(14),

	RayTracingRenderStages			= DirectionalLightsShadowMasks,

	VisibilityBufferStages			= VisibilityBuffer | MotionAndDepth,
	DeferredLightingStages			= GlobalIllumination | DownsampleGeometryTextures | AmbientOcclusion | DeferredShading | SpecularReflections | CompositeLighting,
	PostProcessStages				= PostProcessPreAA | HDRResolve,

	// Presets
	DeferredRendererStages			= VisibilityBufferStages | DeferredLightingStages | PostProcessStages,
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


// GBuffer layout:
// 0 - 4 bytes [baseColor - 24bits] [metallic - 8bits]
// 1 - 4 bytes [tangent frame - 32bits]
// 2 - 2 bytes [roughness - 16bits]
// 3 - 4 bytes [emissive - 32bits]
// 4 - 1 byte  [flags - 8bits] - 0: tangent frame handedness, other bits are reserved
class GBuffer
{
public:

	enum class Texture
	{
		BaseColorMetallic,
		TangentFrame,
		Roughness,
		Emissive,
		Flags,
		NUM
	};

	static constexpr Uint32 texturesNum = static_cast<Uint32>(Texture::NUM);

	static constexpr rhi::EFragmentFormat formats[texturesNum] =
	{
		rhi::EFragmentFormat::RGBA8_UN_Float,
		rhi::EFragmentFormat::RGB10A2_UN_Float,
		rhi::EFragmentFormat::R16_UN_Float,
		rhi::EFragmentFormat::B10G11R11_U_Float,
		rhi::EFragmentFormat::R8_U_Int
	};

	static rhi::EFragmentFormat GetFormat(Texture textureType)
	{
		return formats[static_cast<SizeType>(textureType)];
	}

	struct GBufferExternalTextures
	{
		lib::SharedPtr<rdr::TextureView>*& operator[](Texture textureType)
		{
			return textures[static_cast<SizeType>(textureType)];
		}

		lib::StaticArray<lib::SharedPtr<rdr::TextureView>*, texturesNum> textures = {};
	};

	GBuffer();

	void Create(rg::RenderGraphBuilder& graphBuilder, math::Vector2u resolution, const GBufferExternalTextures& externalTextures = GBufferExternalTextures());

	rg::RGTextureViewHandle operator[](Texture textureType) const
	{
		return m_textures[static_cast<SizeType>(textureType)];
	}

	rg::RGTextureViewHandle operator[](SizeType idx) const
	{
		return m_textures[idx];
	}

	const auto cbegin() const { return std::cbegin(m_textures); }
	const auto begin() const  { return std::begin(m_textures); }

	const auto cend() const { return std::cend(m_textures); }
	const auto end() const  { return std::end(m_textures); }

private:

	lib::StaticArray<rg::RGTextureViewHandle, texturesNum> m_textures;
};


class TextureWithHistory
{
public:

	explicit TextureWithHistory(rdr::RendererResourceName name);

	void SetDefinition(const rhi::TextureDefinition& definition);

	void Reset();

	void Update(rhi::RHIResolution resolution);

	const lib::SharedPtr<rdr::TextureView>& GetCurrent() const { return m_current; }
	const lib::SharedPtr<rdr::TextureView>& GetHistory() const { return m_history; }
	
private:

	rdr::RendererResourceName m_name;

	rhi::TextureDefinition m_definition;

	lib::SharedPtr<rdr::TextureView> m_current;
	lib::SharedPtr<rdr::TextureView> m_history;
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