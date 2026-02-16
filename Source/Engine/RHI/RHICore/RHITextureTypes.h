#pragma once

#include "RHIPipelineTypes.h"


namespace spt::rhi
{

namespace constants
{

static constexpr Uint32 allRemainingMips		= idxNone<Uint32>;
static constexpr Uint32 allRemainingArrayLayers	= idxNone<Uint32>;

} // constants


enum class ETextureUsage : Flags32
{
	None               = 0,
	TransferSource     = BIT(0),
	TransferDest       = BIT(1),
	SampledTexture     = BIT(2),
	StorageTexture     = BIT(3),
	ColorRT            = BIT(4),
	DepthSetncilRT     = BIT(5),
	TransientRT        = BIT(6),
	InputRT            = BIT(7),
	ShadingRateTexture = BIT(8),
	FragmentDensityMap = BIT(9),

	GloballyReadable   = TransferSource | SampledTexture
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

	R8_UN_Float,
	R8_U_Int,
	R16_U_Int,
	R16_UN_Float,
	R16_S_Float,
	R32_S_Float,
	R32_U_Int,

	RG8_UN_Float,
	RG16_U_Int,
	RG16_UN_Float,
	RG16_SN_Float,
	RG16_S_Float,
	RG32_S_Float,

	RGB8_UN_Float,
	BGR8_UN_Float,
	RGB16_UN_Float,
	RGB32_S_Float,

	RGB10A2_UN_Float,

	B10G11R11_U_Float,
	A2B10G10R10_UN_Float,

	RGB9E5_Float,

	RGBA8_UN_Float,
	RGBA8_sRGB_Float,
	BGRA8_UN_Float,
	RGBA16_UN_Float,
	RGBA16_S_Float,

	RGBA32_S_Float,

	BC1_UN,
	BC1_sRGB,
	BC4_UN,
	BC5_UN,
	
	D16_UN_Float,
	D32_S_Float
};


struct TextureFragmentInfo
{
	Uint32 blockWidth    = 0u;
	Uint32 blockHeight   = 0u;
	Uint32 bytesPerBlock = 0u;
};


namespace bc_info
{
static constexpr TextureFragmentInfo bc1 = TextureFragmentInfo{ 4u, 4u, 8u };
static constexpr TextureFragmentInfo bc4 = TextureFragmentInfo{ 4u, 4u, 8u };
static constexpr TextureFragmentInfo bc5 = TextureFragmentInfo{ 4u, 4u, 16u };
} // bc_info


inline TextureFragmentInfo GetFragmentInfo(EFragmentFormat format)
{
	switch (format)
	{
	case EFragmentFormat::R8_UN_Float:
	case EFragmentFormat::R8_U_Int:
		return { 1u, 1u, 1u };

	case EFragmentFormat::R16_U_Int:
	case EFragmentFormat::R16_UN_Float:
	case EFragmentFormat::R16_S_Float:
	case EFragmentFormat::D16_UN_Float:
	case EFragmentFormat::RG8_UN_Float:
		return { 1u, 1u, 2u };

	case EFragmentFormat::RGB8_UN_Float:
	case EFragmentFormat::BGR8_UN_Float:
		return { 1u, 1u, 3u };

	case EFragmentFormat::R32_S_Float:
	case EFragmentFormat::R32_U_Int:
	case EFragmentFormat::RG16_U_Int:
	case EFragmentFormat::RG16_UN_Float:
	case EFragmentFormat::RG16_SN_Float:
	case EFragmentFormat::RG16_S_Float:
	case EFragmentFormat::RGBA8_UN_Float:
	case EFragmentFormat::RGBA8_sRGB_Float:
	case EFragmentFormat::BGRA8_UN_Float:
	case EFragmentFormat::RGB10A2_UN_Float:
	case EFragmentFormat::B10G11R11_U_Float:
	case EFragmentFormat::A2B10G10R10_UN_Float:
	case EFragmentFormat::RGB9E5_Float:
	case EFragmentFormat::D32_S_Float:
		return { 1u, 1u, 4u };

	case EFragmentFormat::RG32_S_Float:
	case EFragmentFormat::RGB16_UN_Float:
	case EFragmentFormat::RGBA16_UN_Float:
	case EFragmentFormat::RGBA16_S_Float:
		return { 1u, 1u, 8u };

	case EFragmentFormat::RGB32_S_Float:
		return { 1u, 1u, 12u };

	case EFragmentFormat::BC1_UN:
	case EFragmentFormat::BC1_sRGB:
		return bc_info::bc1;

	case EFragmentFormat::BC4_UN:
		return bc_info::bc4;

	case EFragmentFormat::BC5_UN:
		return bc_info::bc5;

	case EFragmentFormat::RGBA32_S_Float:
		return { 1u, 1u, 16u };
	}

	SPT_CHECK_NO_ENTRY();
	return {};
}


inline lib::String GetFormatName(rhi::EFragmentFormat format)
{
	switch (format)
	{
	case rhi::EFragmentFormat::None:                 return "None";
	case rhi::EFragmentFormat::R8_UN_Float:          return "R8_UN_Float";
	case rhi::EFragmentFormat::R8_U_Int:             return "R8_U_Int";
	case rhi::EFragmentFormat::R16_U_Int:            return "R16_U_Int";
	case rhi::EFragmentFormat::R16_UN_Float:         return "R16_UN_Float";
	case rhi::EFragmentFormat::R16_S_Float:          return "R16_S_Float";
	case rhi::EFragmentFormat::R32_S_Float:          return "R32_S_Float";
	case rhi::EFragmentFormat::R32_U_Int:            return "R32_U_Int";
	case rhi::EFragmentFormat::RG8_UN_Float:         return "RG8_UN_Float";
	case rhi::EFragmentFormat::RG16_U_Int:           return "RG16_U_Int";
	case rhi::EFragmentFormat::RG16_UN_Float:        return "RG16_UN_Float";
	case rhi::EFragmentFormat::RG16_SN_Float:        return "RG16_SN_Float";
	case rhi::EFragmentFormat::RG16_S_Float:         return "RG16_S_Float";
	case rhi::EFragmentFormat::RG32_S_Float:         return "RG32_S_Float";
	case rhi::EFragmentFormat::RGB8_UN_Float:        return "RGB8_UN_Float";
	case rhi::EFragmentFormat::RGBA8_sRGB_Float:	 return "RGBA8_sRGB_Float";
	case rhi::EFragmentFormat::BGR8_UN_Float:        return "BGR8_UN_Float";
	case rhi::EFragmentFormat::RGB16_UN_Float:       return "RGB16_UN_Float";
	case rhi::EFragmentFormat::RGB32_S_Float:        return "RGB32_S_Float";
	case rhi::EFragmentFormat::RGB10A2_UN_Float:     return "RGB10A2_UN_Float";
	case rhi::EFragmentFormat::B10G11R11_U_Float:    return "B10G11R11_U_Float";
	case rhi::EFragmentFormat::A2B10G10R10_UN_Float: return "A2B10G10R10_UN_Float";
	case rhi::EFragmentFormat::RGB9E5_Float:         return "RGB9E5_Float";
	case rhi::EFragmentFormat::RGBA8_UN_Float:       return "RGBA8_UN_Float";
	case rhi::EFragmentFormat::BGRA8_UN_Float:       return "BGRA8_UN_Float";
	case rhi::EFragmentFormat::RGBA16_UN_Float:      return "RGBA16_UN_Float";
	case rhi::EFragmentFormat::RGBA16_S_Float:       return "RGBA16_S_Float";
	case rhi::EFragmentFormat::RGBA32_S_Float:       return "RGBA32_S_Float";
	case rhi::EFragmentFormat::BC1_UN:               return "BC1_UN";
	case rhi::EFragmentFormat::BC1_sRGB:             return "BC1_sRGB";
	case rhi::EFragmentFormat::BC4_UN:               return "BC4_UN";
	case rhi::EFragmentFormat::BC5_UN:               return "BC5_UN";
	case rhi::EFragmentFormat::D16_UN_Float:         return "D16_UN_Float";
	case rhi::EFragmentFormat::D32_S_Float:          return "D32_S_Float";
	default:
		return "Unknown";
	}
}

inline rhi::EFragmentFormat GetFormatByName(lib::StringView name)
{
	if (name == "None")
	{
		return rhi::EFragmentFormat::None;
	}
	else if (name == "R8_UN_Float")
	{
		return rhi::EFragmentFormat::R8_UN_Float;
	}
	else if (name == "R8_U_Int")
	{
		return rhi::EFragmentFormat::R8_U_Int;
	}
	else if (name == "R16_U_Int")
	{
		return rhi::EFragmentFormat::R16_U_Int;
	}
	else if (name == "R16_UN_Float")
	{
		return rhi::EFragmentFormat::R16_UN_Float;
	}
	else if (name == "R16_S_Float")
	{
		return rhi::EFragmentFormat::R16_S_Float;
	}
	else if (name == "R32_S_Float")
	{
		return rhi::EFragmentFormat::R32_S_Float;
	}
	else if (name == "R32_U_Int")
	{
		return rhi::EFragmentFormat::R32_U_Int;
	}
	else if (name == "RG8_UN_Float")
	{
		return rhi::EFragmentFormat::RG8_UN_Float;
	}
	else if (name == "RG16_U_Int")
	{
		return rhi::EFragmentFormat::RG16_U_Int;
	}
	else if (name == "RG16_UN_Float")
	{
		return rhi::EFragmentFormat::RG16_UN_Float;
	}
	else if (name == "RG16_SN_Float")
	{
		return rhi::EFragmentFormat::RG16_SN_Float;
	}
	else if (name == "RG16_S_Float")
	{
		return rhi::EFragmentFormat::RG16_S_Float;
	}
	else if (name == "RG32_S_Float")
	{
		return rhi::EFragmentFormat::RG32_S_Float;
	}
	else if (name == "RGB8_UN_Float")
	{
		return rhi::EFragmentFormat::RGB8_UN_Float;
	}
	else if (name == "BGR8_UN_Float")
	{
		return rhi::EFragmentFormat::BGR8_UN_Float;
	}
	else if (name == "RGB16_UN_Float")
	{
		return rhi::EFragmentFormat::RGB16_UN_Float;
	}
	else if (name == "RGB32_S_Float")
	{
		return rhi::EFragmentFormat::RGB32_S_Float;
	}
	else if (name == "RGB10A2_UN_Float")
	{
		return rhi::EFragmentFormat::RGB10A2_UN_Float;
	}
	else if (name == "B10G11R11_U_Float")
	{
		return rhi::EFragmentFormat::B10G11R11_U_Float;
	}
	else if (name == "A2B10G10R10_UN_Float")
	{
		return rhi::EFragmentFormat::A2B10G10R10_UN_Float;
	}
	else if (name == "RGB9E5_Float")
	{
		return rhi::EFragmentFormat::RGB9E5_Float;
	}
	else if (name == "RGBA8_UN_Float")
	{
		return rhi::EFragmentFormat::RGBA8_UN_Float;
	}
	else if (name == "RGBA8_sRGB_Float")
	{
		return rhi::EFragmentFormat::RGBA8_sRGB_Float;
	}
	else if (name == "BGRA8_UN_Float")
	{
		return rhi::EFragmentFormat::BGRA8_UN_Float;
	}
	else if (name == "RGBA16_UN_Float")
	{
		return rhi::EFragmentFormat::RGBA16_UN_Float;
	}
	else if (name == "RGBA16_S_Float")
	{
		return rhi::EFragmentFormat::RGBA16_S_Float;
	}
	else if (name == "RGBA32_S_Float")
	{
		return rhi::EFragmentFormat::RGBA32_S_Float;
	}
	else if (name == "BC1_UN")
	{
		return rhi::EFragmentFormat::BC1_UN;
	}
	else if (name == "BC1_sRGB")
	{
		return rhi::EFragmentFormat::BC1_sRGB;
	}
	else if (name == "BC4_UN")
	{
		return rhi::EFragmentFormat::BC4_UN;
	}
	else if (name == "BC5_UN")
	{
		return rhi::EFragmentFormat::BC5_UN;
	}
	else if (name == "D16_UN_Float")
	{
		return rhi::EFragmentFormat::D16_UN_Float;
	}
	else if (name == "D32_S_Float")
	{
		return rhi::EFragmentFormat::D32_S_Float;
	}
	else
	{
		return rhi::EFragmentFormat::None;
	}
}


enum class ETextureFlags : Flags8
{
	None             = 0,
	GloballyReadable = BIT(0),
	SkipAutoGPUInit  = BIT(1), // If this flag is set, transition from "Undefined" state will have to be done manually by the user

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


enum class ETextureTiling : Uint8
{
	Optimal,
	Linear
};


struct TextureDefinition
{
	TextureDefinition()
		: usage(ETextureUsage::None)
		, format(EFragmentFormat::RGBA8_UN_Float)
		, samples(1)
		, tiling(ETextureTiling::Optimal)
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
		, tiling(ETextureTiling::Optimal)
		, mipLevels(1)
		, arrayLayers(1)
		, flags(ETextureFlags::Default)
		, type(ETextureType::Auto)
	{ }

	RHIResolution		resolution;
	ETextureUsage		usage;
	EFragmentFormat		format;
	Uint8				samples;
	ETextureTiling		tiling;
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
	case EFragmentFormat::R16_U_Int:
    case EFragmentFormat::R16_S_Float:
	case EFragmentFormat::R32_S_Float:
	case EFragmentFormat::R32_U_Int:
	case EFragmentFormat::RG8_UN_Float:
	case EFragmentFormat::RG16_UN_Float:
	case EFragmentFormat::RG16_U_Int:
	case EFragmentFormat::RG16_SN_Float:
	case EFragmentFormat::RG16_S_Float:
	case EFragmentFormat::RG32_S_Float:
	case EFragmentFormat::RGBA8_UN_Float:
	case EFragmentFormat::BGRA8_UN_Float:
	case EFragmentFormat::RGB8_UN_Float:
	case EFragmentFormat::RGBA8_sRGB_Float:
	case EFragmentFormat::BGR8_UN_Float:
	case EFragmentFormat::B10G11R11_U_Float:
	case EFragmentFormat::RGB10A2_UN_Float:
	case EFragmentFormat::A2B10G10R10_UN_Float:
	case EFragmentFormat::RGB9E5_Float:
	case EFragmentFormat::RGB32_S_Float:
	case EFragmentFormat::RGBA16_UN_Float:
	case EFragmentFormat::RGBA16_S_Float:
	case EFragmentFormat::RGBA32_S_Float:
	case EFragmentFormat::BC1_UN:
	case EFragmentFormat::BC1_sRGB:
	case EFragmentFormat::BC4_UN:
	case EFragmentFormat::BC5_UN:

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
	case EFragmentFormat::R16_U_Int:
	case EFragmentFormat::R32_U_Int:
	case EFragmentFormat::RG16_U_Int:
		return true;

	default:
		return false;
	}
}

inline rhi::ETextureType GetSelectedTextureType(const rhi::TextureDefinition& definition)
{
	switch (definition.type)
	{
	case ETextureType::Texture1D:
	case ETextureType::Texture2D:
	case ETextureType::Texture3D:
		return definition.type;

	case ETextureType::Auto:
		{
			const math::Vector3u resolution = definition.resolution.AsVector();
			if (resolution.z() > 1u)
			{
				return ETextureType::Texture3D;
			}
			else if (resolution.y() > 1u)
			{
				return ETextureType::Texture2D;
			}
			else
			{
				return ETextureType::Texture1D;
			}
		}
		break;
	default:

		SPT_CHECK_NO_ENTRY();
		return ETextureType::Auto;
	}
}


namespace texture_utils
{

inline Uint32 ComputeMipLevelsNumForResolution(math::Vector2u resolution)
{
	return 1u + static_cast<Uint32>(std::log2(std::max(resolution.x(), resolution.y())));
}

inline Bool CanBeBlockCompressed(math::Vector2u resolution)
{
	return (resolution.x() % 4u == 0) && (resolution.y() % 4u == 0);
}

inline Uint32 ComputeBlockCompressedMipsNumForResolution(math::Vector2u resolution)
{
	Uint32 mips = 0;
	Uint32 width = resolution.x();
	Uint32 height = resolution.y();
	while (width > 0 && height > 0 && (width % 4 == 0) && (height % 4 == 0))
	{
		++mips;
		width /= 2;
		height /= 2;
	}
	return mips;
}

} // texture_utils

} // spt::rhi
