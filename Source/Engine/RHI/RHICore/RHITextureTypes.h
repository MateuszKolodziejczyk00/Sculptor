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

	// Special layout that can be used as prev texture layout when performing layout transitions
	Auto
};


enum class EFragmentFormat : Uint32
{
	None,

	R8_UN_Float,
	R8_U_Int,
	R16_UN_Float,
	R16_S_Float,
	R32_S_Float,
	R32_U_Int,

	RG8_UN_Float,
	RG16_UN_Float,
	RG16_SN_Float,
	RG16_S_Float,
	RG32_S_Float,

	RGB8_UN_Float,
	BGR8_UN_Float,
	RGB16_UN_Float,
	RGB32_S_Float,

	B10G11R11_U_Float,
	A2B10G10R10_UN_Float,

	RGBA8_UN_Float,
	BGRA8_UN_Float,
	RGBA16_UN_Float,
	RGBA16_S_Float,

	RGBA32_S_Float,
	
	D16_UN_Float,
	D32_S_Float
};


enum class ETextureFlags : Flags8
{
	None = 0,

	Default = None
};


struct RHIResolution
{
public:

	RHIResolution()
		: resolution(math::Vector3u::Zero())
	{ }
	
	RHIResolution(math::Vector2u inResolution)
		: resolution(inResolution.x(), inResolution.y(), 1u)
	{ }
	
	RHIResolution(math::Vector3u inResolution)
		: resolution(inResolution)
	{ }

	RHIResolution& operator=(const math::Vector2u& inResolution)
	{
		resolution = math::Vector3u(inResolution.x(), inResolution.y(), 1u);
		return *this;
	}

	RHIResolution& operator=(const math::Vector3u& inResolution)
	{
		resolution = inResolution;
		return *this;
	}

	Bool operator==(const RHIResolution& other) const
	{
		return resolution == other.resolution;
	}

	Bool operator!=(const RHIResolution& other) const
	{
		return !(*this == other);
	}

	const math::Vector3u& AsVector() const
	{
		return resolution;
	}

private:

	math::Vector3u resolution;
};


enum class ETextureType : Uint8
{
	Auto,
	Texture1D,
	Texture2D,
	Texture3D
};


struct TextureDefinition
{
	TextureDefinition()
		: usage(ETextureUsage::None)
		, format(EFragmentFormat::RGBA8_UN_Float)
		, samples(1)
		, mipLevels(1)
		, arrayLayers(1)
		, flags(ETextureFlags::Default)
		, type(ETextureType::Auto)
	{ }
	
	TextureDefinition(const RHIResolution& inResolution, ETextureUsage inUsage, EFragmentFormat inFormat)
		: resolution(inResolution)
		, usage(inUsage)
		, format(inFormat)
		, samples(1)
		, mipLevels(1)
		, arrayLayers(1)
		, flags(ETextureFlags::Default)
		, type(ETextureType::Auto)
	{ }

	RHIResolution		resolution;
	ETextureUsage		usage;
	EFragmentFormat		format;
	Uint8				samples;
	Uint32				mipLevels;
	Uint32				arrayLayers;
	ETextureFlags		flags;
	ETextureType		type;
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
	Auto			= BIT(0),
	Color			= BIT(1),
	Depth			= BIT(2),
	Stencil			= BIT(3)
};


struct TextureSubresourceRange
{
	TextureSubresourceRange(ETextureAspect inAspect = ETextureAspect::Auto, Uint32 inBaseMip = 0, Uint32 inMipLevelsNum = constants::allRemainingMips, Uint32 inBaseArrayLayer = 0, Uint32 inArrayLayersNum = constants::allRemainingArrayLayers)
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
	TextureViewDefinition(ETextureViewType inViewType = ETextureViewType::Default,
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


inline ETextureAspect GetFullAspectForFormat(EFragmentFormat format)
{
	switch (format)
	{
	case EFragmentFormat::D16_UN_Float:
	case EFragmentFormat::D32_S_Float:
		return ETextureAspect::Depth;

	case EFragmentFormat::R8_UN_Float:
	case EFragmentFormat::R8_U_Int:
	case EFragmentFormat::R16_UN_Float:
    case EFragmentFormat::R16_S_Float:
	case EFragmentFormat::R32_S_Float:
	case EFragmentFormat::R32_U_Int:
	case EFragmentFormat::RG8_UN_Float:
	case EFragmentFormat::RG16_UN_Float:
	case EFragmentFormat::RG16_SN_Float:
	case EFragmentFormat::RG16_S_Float:
	case EFragmentFormat::RG32_S_Float:
	case EFragmentFormat::RGBA8_UN_Float:
	case EFragmentFormat::BGRA8_UN_Float:
	case EFragmentFormat::RGB8_UN_Float:
	case EFragmentFormat::BGR8_UN_Float:
	case EFragmentFormat::B10G11R11_U_Float:
    case EFragmentFormat::A2B10G10R10_UN_Float:
	case EFragmentFormat::RGB32_S_Float:
	case EFragmentFormat::RGBA16_UN_Float:
    case EFragmentFormat::RGBA16_S_Float:
	case EFragmentFormat::RGBA32_S_Float:

		return ETextureAspect::Color;

	case EFragmentFormat::None:
	default:

		SPT_CHECK_NO_ENTRY();
		return ETextureAspect::None;
	}
}

inline Bool IsIntegerFormat(EFragmentFormat format)
{
	switch (format)
	{
	case EFragmentFormat::R8_U_Int:
	case EFragmentFormat::R32_U_Int:
		return true;

	default:
		return false;
	}
}

} // spt::rhi