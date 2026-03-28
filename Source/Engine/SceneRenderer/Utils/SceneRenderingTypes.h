#pragma once

#include "SceneRendererTypes.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"
#include "RGDescriptorSetState.h"
#include "DescriptorSetBindings/SamplerBinding.h"
#include "DescriptorSetBindings/SRVTextureBinding.h"
#include "DescriptorSetBindings/ConstantBufferBinding.h"
#include "ShaderStructs/ShaderStructs.h"
#include "RGResources/RGResourceHandles.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc
{

class RenderView;
class SceneRenderSystem;


struct SceneRendererInterface
{
	explicit SceneRendererInterface(const SceneRendererSettings& settings)
		: rendererSettings(settings)
	{ }

	virtual SceneRenderSystem* GetRenderSystem(ESceneRenderSystem type) const = 0;

	template<typename TRenderSystem>
	TRenderSystem* GetRenderSystem() const
	{
		return static_cast<TRenderSystem*>(GetRenderSystem(TRenderSystem::systemType));
	}

	template<typename TRenderSystem>
	TRenderSystem& GetRenderSystemChecked() const
	{
		TRenderSystem* renderSystem = GetRenderSystem<TRenderSystem>();
		SPT_CHECK(renderSystem);
		return *renderSystem;
	}

	const SceneRendererSettings& rendererSettings;
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


BEGIN_SHADER_STRUCT(GPUGBuffer)
	SHADER_STRUCT_FIELD(math::Vector2u,                    resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,                    pixelSize)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         depth)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, gBuffer0)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector4f>, gBuffer1)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,         gBuffer2)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<math::Vector3f>, gBuffer3)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>,         gBuffer4)
END_SHADER_STRUCT();


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
		Occlusion,
		NUM
	};

	static constexpr Uint32 texturesNum = static_cast<Uint32>(Texture::NUM);

	static constexpr rhi::EFragmentFormat formats[texturesNum] =
	{
		rhi::EFragmentFormat::RGBA8_UN_Float,
		rhi::EFragmentFormat::RGB10A2_UN_Float,
		rhi::EFragmentFormat::R16_UN_Float,
		rhi::EFragmentFormat::B10G11R11_U_Float,
		rhi::EFragmentFormat::R8_U_Int,
		rhi::EFragmentFormat::R8_UN_Float
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

	GPUGBuffer GetGPUGBuffer(rg::RGTextureViewHandle depth) const;

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

	Bool HasHistory() const { return m_history != nullptr; }

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
