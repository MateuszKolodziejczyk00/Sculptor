#pragma once

#include "RHIPipelineTypes.h"


namespace spt::rhi
{

namespace constants
{

static constexpr Uint32 allRemainingMips			= idxNone<Uint32>;
static constexpr Uint32 allRemainingArrayLayers	= idxNone<Uint32>;

} // constants


enum class ETextureUsage : Flags32
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
	None,
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
		: resolution{}
		, usage(ETextureUsage::None)
		, format(EFragmentFormat::RGBA8_UN_Float)
		, samples(1)
		, mipLevels(1)
		, arrayLayers(1)
	{ }

	math::Vector3u		resolution;
	ETextureUsage		usage;
	EFragmentFormat		format;
	Uint8				samples;
	Uint8				mipLevels;
	Uint8				arrayLayers;
};


enum class ETextureViewType : Uint8
{
	Default,
	Cube
};


enum class ETextureComponentMapping : Uint8
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


enum class ETextureAspect : Flags8
{
	None			= 0,
	Color			= BIT(0),
	Depth			= BIT(1),
	Stencil			= BIT(2)
};


struct TextureSubresourceRange
{
	TextureSubresourceRange(ETextureAspect inAspect = ETextureAspect::None, Uint32 inBaseMip = 0, Uint32 inMipLevelsNum = constants::allRemainingMips, Uint32 inBaseArrayLayer = 0, Uint32 inArrayLayersNum = constants::allRemainingArrayLayers)
		: aspect(inAspect)
		, baseMipLevel(inBaseMip)
		, mipLevelsNum(inMipLevelsNum)
		, baseArrayLayer(inBaseArrayLayer)
		, arrayLayersNum(inArrayLayersNum)
	{ }

	ETextureAspect		aspect;
	Uint32				baseMipLevel;
	Uint32				mipLevelsNum;
	Uint32				baseArrayLayer;
	Uint32				arrayLayersNum;
};


struct TextureViewDefinition
{
	TextureViewDefinition(	ETextureViewType inViewType = ETextureViewType::Default,
							const TextureSubresourceRange& inSubresourceRange = TextureSubresourceRange(),
							const TextureComponentMappings& inComponentMappings = TextureComponentMappings())
		: viewType(inViewType)
		, subresourceRange(inSubresourceRange)
		, componentMappings(inComponentMappings)
	{ }

	ETextureViewType			viewType;
	TextureSubresourceRange		subresourceRange;
	TextureComponentMappings	componentMappings;
};


enum class EAccessType : Flags32
{
	None		= 0,
	Read		= BIT(0),
	Write		= BIT(1)
};


struct BarrierTextureTransitionTarget
{
	constexpr BarrierTextureTransitionTarget()
		: accessType(EAccessType::None)
		, layout(ETextureLayout::Undefined)
		, stage(EPipelineStage::None)
	{ }

	constexpr BarrierTextureTransitionTarget(EAccessType inAccessType, ETextureLayout inLayout, EPipelineStage inStage)
		: accessType(inAccessType)
		, layout(inLayout)
		, stage(inStage)
	{ }

	EAccessType				accessType;
	ETextureLayout			layout;
	EPipelineStage			stage;
};


namespace TextureTransition
{

	static constexpr BarrierTextureTransitionTarget Undefined			= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::Undefined, EPipelineStage::TopOfPipe);

	static constexpr BarrierTextureTransitionTarget ComputeGeneral		= BarrierTextureTransitionTarget(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::ComputeShader);
	static constexpr BarrierTextureTransitionTarget FragmentGeneral		= BarrierTextureTransitionTarget(lib::Flags(EAccessType::Read, EAccessType::Write), ETextureLayout::General, EPipelineStage::FragmentShader);

	static constexpr BarrierTextureTransitionTarget FragmentReadOnly	= BarrierTextureTransitionTarget(EAccessType::Read, ETextureLayout::ColorReadOnlyOptimal, EPipelineStage::FragmentShader);
	
	static constexpr BarrierTextureTransitionTarget PresentSource		= BarrierTextureTransitionTarget(EAccessType::None, ETextureLayout::PresentSrc, EPipelineStage::TopOfPipe);
	
	static constexpr BarrierTextureTransitionTarget ColorRenderTarget	= BarrierTextureTransitionTarget(EAccessType::Write, ETextureLayout::ColorRTOptimal, EPipelineStage::ColorRTOutput);

}

}