#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class ESamplerFilterType
{
	Nearest,
	Linear
};


enum class EAxisAddressingMode
{
	Repeat,
	MirroredRepeat,
	ClampToEdge,
	ClampToBorder,
	MorroredClampToEdge
};


enum class ESamplerFlags
{
	None			= 0,

	NotCached		= BIT(1) // Samplers that are not implicitly cached
};


enum class EMipMapAddressingMode
{
	Nearest,
	Linear
};


enum class EBorderColor
{
	FloatTransparentBlack,
	IntTransparentBlack,
	FloatOpaqueBlack,
	IntOpaqueBlack,
	FloatOpaqueWhite,
	IntOpaqueWhite
};


struct SamplerDefinition
{
	static constexpr Real32 infiniteMaxLod = -1.f;

	constexpr SamplerDefinition()
		: flags(ESamplerFlags::None)
		, minificationFilter(ESamplerFilterType::Nearest)
		, magnificationFilter(ESamplerFilterType::Nearest)
		, mipMapAdressingMode(EMipMapAddressingMode::Nearest)
		, addressingModeU(EAxisAddressingMode::Repeat)
		, addressingModeV(EAxisAddressingMode::Repeat)
		, addressingModeW(EAxisAddressingMode::Repeat)
		, mipLodBias(0.f)
		, enableAnisotropy(true)
		, maxAnisotropy(8.f)
		, minLod(0)
		, maxLod(0)
		, borderColor(EBorderColor::FloatTransparentBlack)
		, unnormalizedCoords(false)
	{ }

	constexpr SamplerDefinition(ESamplerFilterType filterType, EMipMapAddressingMode mipsAddressingMode, EAxisAddressingMode axisAddressingMode)
		: flags(ESamplerFlags::None)
		, minificationFilter(filterType)
		, magnificationFilter(filterType)
		, mipMapAdressingMode(mipsAddressingMode)
		, addressingModeU(axisAddressingMode)
		, addressingModeV(axisAddressingMode)
		, addressingModeW(axisAddressingMode)
		, mipLodBias(0.f)
		, enableAnisotropy(true)
		, maxAnisotropy(8.f)
		, minLod(0.f)
		, maxLod(0.f)
		, borderColor(EBorderColor::FloatTransparentBlack)
		, unnormalizedCoords(false)
	{ }

	ESamplerFlags flags;

	ESamplerFilterType minificationFilter;
	ESamplerFilterType magnificationFilter;

	EMipMapAddressingMode mipMapAdressingMode;

	EAxisAddressingMode addressingModeU;
	EAxisAddressingMode addressingModeV;
	EAxisAddressingMode addressingModeW;

	Real32	mipLodBias;

	Bool	enableAnisotropy;
	Real32	maxAnisotropy;

	Real32	minLod;
	Real32	maxLod;

	EBorderColor borderColor;

	Bool unnormalizedCoords;
};

} // spt::rhi


namespace std
{

template<>
struct hash<spt::rhi::SamplerDefinition>
{
    size_t operator()(const spt::rhi::SamplerDefinition& samplerDef) const
    {
		return spt::lib::HashCombine(samplerDef.flags,
									 samplerDef.minificationFilter,
									 samplerDef.magnificationFilter,
									 samplerDef.mipMapAdressingMode,
									 samplerDef.addressingModeU,
									 samplerDef.addressingModeV,
									 samplerDef.addressingModeW,
									 samplerDef.mipLodBias,
									 samplerDef.enableAnisotropy,
									 samplerDef.maxAnisotropy,
									 samplerDef.minLod,
									 samplerDef.maxLod,
									 samplerDef.borderColor,
									 samplerDef.unnormalizedCoords);
    }
};

} // std
