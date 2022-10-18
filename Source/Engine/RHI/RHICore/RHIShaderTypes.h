#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

// Must be in the same order as EShaderStage
enum class EShaderStageFlags : Flags32
{
	None			= 0,
	Vertex			= BIT(0),
	Fragment		= BIT(1),
	Compute			= BIT(2)
};


// Must be in the same order as EShaderStageFlags
enum class EShaderStage : Flags32
{
	None,
	Vertex,
	Fragment,
	Compute
};


constexpr EShaderStageFlags GetStageFlag(EShaderStage stage)
{
	return static_cast<EShaderStageFlags>(1 << (static_cast<Flags32>(stage) - 1));
}


enum class EPipelineType : Flags32
{
	None,
	Graphics,
	Compute
};


inline EPipelineType GetPipelineTypeForShaderStage(EShaderStage stage)
{
	switch (stage)
	{
	case EShaderStage::None:		return EPipelineType::None;
	case EShaderStage::Vertex:		return EPipelineType::Graphics;
	case EShaderStage::Fragment:	return EPipelineType::Graphics;
	case EShaderStage::Compute:		return EPipelineType::Compute;

	default:

		SPT_CHECK_NO_ENTRY();
		return EPipelineType::None;
	}
}


struct ShaderModuleDefinition
{
	ShaderModuleDefinition()
		: stage(EShaderStage::Vertex)
	{ }

	ShaderModuleDefinition(lib::DynamicArray<Uint32> inBinary, EShaderStage inStage)
		: binary(std::move(inBinary))
		, stage(inStage)
	{
		SPT_CHECK(!binary.empty());
	}

	lib::DynamicArray<Uint32>		binary;
	EShaderStage					stage;
};

} // spt::rhi