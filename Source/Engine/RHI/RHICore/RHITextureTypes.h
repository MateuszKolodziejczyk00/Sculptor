#pragma once

#include "RHIPipelineTypes.h"


namespace spt::rhi
{

namespace ETextureUsage
{

enum Flags : Flags32
{
	None						= 0,
	TransferSource				= BIT(0),
	TransferDest				= BIT(1),
	SampledTexture				= BIT(2),
	StorageTexture				= BIT(3),
	ColorRT						= BIT(4),
	DepthSetncilRT				= BIT(5),
	TransientRT					= BIT(6),
	InputRT						= BIT(7),
	ShadingRateTexture			= BIT(8),
	FragmentDensityMap			= BIT(9),
};

}


enum class ETextureLayout : Uint32
{
	Undefined,
	General,

	ColorRTOptimal,
	DepthRTOptimal,
	DepthStencilRTOptimal,
	DepthRTStencilReadOptimal,
	DepthReadOnlyStencilRTOptimal,

	TransferSrcOptimal,
	TransferDstOptimal,

	DepthReadOnlyOptimal,
	DepthStencilReadOnlyOptimal,
	ColorReadOnlyOptimal,

	PresentSrc,

	ReadOnlyOptimal,
	RenderTargetOptimal,
};


enum class EFragmentFormat : Uint32
{
	RGBA8_UN_Float,
	BGRA8_UN_Float,

	RGB8_UN_Float,
	BGR8_UN_Float,

	RGBA32_S_Float,
	D32_S_Float,
};


struct TextureDefinition
{
	TextureDefinition()
		: m_resolution{}
		, m_usage(0)
		, m_format(EFragmentFormat::RGBA8_UN_Float)
		, m_samples(1)
		, m_mipLevels(1)
		, m_arrayLayers(1)
	{ }

	math::Vector3u		m_resolution;
	Flags32				m_usage;
	EFragmentFormat		m_format;
	Uint8				m_samples;
	Uint8				m_mipLevels;
	Uint8				m_arrayLayers;
};


enum class ETextureViewType
{
	Default,
	Cube
};


enum class ETextureComponentMapping
{
	R,
	G,
	B,
	A,
	Zero,
	One
};


struct TextureComponentMappings
{
	TextureComponentMappings(ETextureComponentMapping inR = ETextureComponentMapping::R,
							 ETextureComponentMapping inG = ETextureComponentMapping::G,
							 ETextureComponentMapping inB = ETextureComponentMapping::B,
							 ETextureComponentMapping inA = ETextureComponentMapping::A)
		: r(inR)
		, g(inG)
		, b(inB)
		, a(inA)
	{ }

	ETextureComponentMapping r;
	ETextureComponentMapping g;
	ETextureComponentMapping b;
	ETextureComponentMapping a;

};


namespace ETextureAspect
{

enum Flags : Flags32
{
	Color			= BIT(0),
	Depth			= BIT(1),
	Stencil			= BIT(2)
};

}


struct TextureSubresourceRange
{
	static constexpr Uint32 s_allRemainingMips			= idxNone<Uint32>;
	static constexpr Uint32 s_allRemainingArrayLayers	= idxNone<Uint32>;

	TextureSubresourceRange(Flags32 aspect = 0, Uint32 baseMip = 0, Uint32 mipLevelsNum = s_allRemainingMips, Uint32 baseArrayLayer = 0, Uint32 arrayLayersNum = s_allRemainingArrayLayers)
		: m_aspect(aspect)
		, m_baseMipLevel(baseMip)
		, m_mipLevelsNum(mipLevelsNum)
		, m_baseArrayLayer(baseArrayLayer)
		, m_arrayLayersNum(arrayLayersNum)
	{ }

	Flags32				m_aspect;
	Uint32				m_baseMipLevel;
	Uint32				m_mipLevelsNum;
	Uint32				m_baseArrayLayer;
	Uint32				m_arrayLayersNum;
};


struct TextureViewDefinition
{
	TextureViewDefinition(	ETextureViewType viewType = ETextureViewType::Default,
							const TextureComponentMappings& componentMappings = TextureComponentMappings(),
							const TextureSubresourceRange& subresourceRange = TextureSubresourceRange())
	{ }

	ETextureViewType			m_viewType;
	TextureComponentMappings	m_componentMappings;
	TextureSubresourceRange		m_subresourceRange;
};


namespace EAccessType
{

enum Flags : Flags32
{
	None		= 0,
	Read		= BIT(0),
	Write		= BIT(1)
};

}


struct BarrierTextureTransitionTarget
{
	constexpr BarrierTextureTransitionTarget()
		: m_accessType(EAccessType::None)
		, m_layout(ETextureLayout::Undefined)
		, m_stage(EPipelineStage::None)
	{ }

	constexpr BarrierTextureTransitionTarget(Flags32 accessType, ETextureLayout layout, Flags32 stage)
		: m_accessType(accessType)
		, m_layout(layout)
		, m_stage(stage)
	{ }

	Flags32						m_accessType;
	ETextureLayout				m_layout;
	Flags32						m_stage;
};


namespace TextureTransition
{

	static constexpr BarrierTextureTransitionTarget Undefined			= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::Undefined, EPipelineStage::TopOfPipe);

	static constexpr BarrierTextureTransitionTarget ComputeGeneral		= BarrierTextureTransitionTarget(EAccessType::Read | EAccessType::Write, ETextureLayout::General, EPipelineStage::ComputeShader);
	static constexpr BarrierTextureTransitionTarget FragmentGeneeral	= BarrierTextureTransitionTarget(EAccessType::Read | EAccessType::Write, ETextureLayout::General, EPipelineStage::FragmentShader);
	
	static constexpr BarrierTextureTransitionTarget PresentSource		= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::PresentSrc, EPipelineStage::TopOfPipe);

}

}