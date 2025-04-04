#pragma once

#include "SculptorCoreTypes.h"
#include "RHICommonTypes.h"


namespace spt::rhi
{

enum class ESamplerFilterType
{
	Nearest,
	Linear
};


enum class ESamplerReductionMode
{
	WeightedAverage,
	Min,
	Max
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
		, reductionMode(ESamplerReductionMode::WeightedAverage)
		, mipLodBias(0.f)
		, enableAnisotropy(true)
		, maxAnisotropy(8.f)
		, compareOp(ECompareOp::None)
		, minLod(0.f)
		, maxLod(-1.f)
		, borderColor(EBorderColor::FloatTransparentBlack)
		, unnormalizedCoords(false)
	{ }

	constexpr SamplerDefinition(ESamplerFilterType filterType, EMipMapAddressingMode mipsAddressingMode, EAxisAddressingMode axisAddressingMode, ESamplerReductionMode reduction = ESamplerReductionMode::WeightedAverage)
		: flags(ESamplerFlags::None)
		, minificationFilter(filterType)
		, magnificationFilter(filterType)
		, mipMapAdressingMode(mipsAddressingMode)
		, addressingModeU(axisAddressingMode)
		, addressingModeV(axisAddressingMode)
		, addressingModeW(axisAddressingMode)
		, reductionMode(reduction)
		, mipLodBias(0.f)
		, enableAnisotropy(false)
		, maxAnisotropy(0.f)
		, compareOp(ECompareOp::None)
		, minLod(0.f)
		, maxLod(-1.f)
		, borderColor(EBorderColor::FloatTransparentBlack)
		, unnormalizedCoords(false)
	{ }

	bool operator==(const SamplerDefinition& rhs) const = default;

	ESamplerFlags flags;

	ESamplerFilterType minificationFilter;
	ESamplerFilterType magnificationFilter;

	EMipMapAddressingMode mipMapAdressingMode;

	EAxisAddressingMode addressingModeU;
	EAxisAddressingMode addressingModeV;
	EAxisAddressingMode addressingModeW;

	ESamplerReductionMode reductionMode;

	Real32	mipLodBias;

	Bool	enableAnisotropy;
	Real32	maxAnisotropy;

	ECompareOp compareOp;

	Real32	minLod;
	Real32	maxLod;

	EBorderColor borderColor;

	Bool unnormalizedCoords;
};


struct SamplerState
{
	static constexpr SamplerDefinition LinearRepeat = SamplerDefinition(ESamplerFilterType::Linear, EMipMapAddressingMode::Linear, EAxisAddressingMode::Repeat);
	static constexpr SamplerDefinition LinearClampToEdge = SamplerDefinition(ESamplerFilterType::Linear, EMipMapAddressingMode::Linear, EAxisAddressingMode::ClampToEdge);
	
	static constexpr SamplerDefinition LinearMinClampToEdge = SamplerDefinition(ESamplerFilterType::Linear, EMipMapAddressingMode::Linear, EAxisAddressingMode::ClampToEdge, ESamplerReductionMode::Min);
	static constexpr SamplerDefinition LinearMaxClampToEdge = SamplerDefinition(ESamplerFilterType::Linear, EMipMapAddressingMode::Linear, EAxisAddressingMode::ClampToEdge, ESamplerReductionMode::Max);
	
	static constexpr SamplerDefinition NearestClampToBorder = SamplerDefinition(ESamplerFilterType::Nearest, EMipMapAddressingMode::Nearest, EAxisAddressingMode::ClampToBorder);
	static constexpr SamplerDefinition NearestClampToEdge = SamplerDefinition(ESamplerFilterType::Nearest, EMipMapAddressingMode::Nearest, EAxisAddressingMode::ClampToEdge);
};

} // spt::rhi


namespace spt::lib
{

template<>
struct Hasher<rhi::SamplerDefinition>
{
    size_t operator()(const rhi::SamplerDefinition& samplerDef) const
    {
		return HashCombine(samplerDef.flags,
						   samplerDef.minificationFilter,
						   samplerDef.magnificationFilter,
						   samplerDef.mipMapAdressingMode,
						   samplerDef.addressingModeU,
						   samplerDef.addressingModeV,
						   samplerDef.addressingModeW,
						   samplerDef.reductionMode,
						   samplerDef.mipLodBias,
						   samplerDef.enableAnisotropy,
						   samplerDef.maxAnisotropy,
						   samplerDef.compareOp,
						   samplerDef.minLod,
						   samplerDef.maxLod,
						   samplerDef.borderColor,
						   samplerDef.unnormalizedCoords);
    }
};

} // spt::lib
