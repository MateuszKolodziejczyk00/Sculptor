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
	Compute			= BIT(2),

	RTGeneration	= BIT(3),
	RTAnyHit		= BIT(4),
	RTClosestHit	= BIT(5),
	RTMiss			= BIT(6),
	RTIntersection	= BIT(7),

	AllGraphics		= Vertex | Fragment,
	AllCompute		= Compute,
	AllRayTracing	= RTGeneration | RTAnyHit | RTClosestHit | RTMiss | RTIntersection,
	All				= AllGraphics | AllCompute | AllRayTracing
};


// Must be in the same order as EShaderStageFlags
enum class EShaderStage : Flags32
{
	None,
	Vertex,
	Fragment,
	Compute,
	RTGeneration,
	RTAnyHit,
	RTClosestHit,
	RTMiss,
	RTIntersection,

	NUM
};


constexpr EShaderStageFlags GetStageFlag(EShaderStage stage)
{
	return static_cast<EShaderStageFlags>(1 << (static_cast<Flags32>(stage) - 1));
}


enum class EPipelineType : Flags32
{
	None,
	Graphics,
	Compute,
	RayTracing
};


inline EPipelineType GetPipelineTypeForShaderStage(EShaderStage stage)
{
	switch (stage)
	{
	case EShaderStage::None:			return EPipelineType::None;
	case EShaderStage::Vertex:			return EPipelineType::Graphics;
	case EShaderStage::Fragment:		return EPipelineType::Graphics;

	case EShaderStage::Compute:			return EPipelineType::Compute;

	case EShaderStage::RTGeneration:	return EPipelineType::RayTracing;
	case EShaderStage::RTAnyHit:		return EPipelineType::RayTracing;
	case EShaderStage::RTClosestHit:	return EPipelineType::RayTracing;
	case EShaderStage::RTMiss:			return EPipelineType::RayTracing;
	case EShaderStage::RTIntersection:	return EPipelineType::RayTracing;

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

	ShaderModuleDefinition(lib::DynamicArray<Uint32> inBinary, EShaderStage inStage, const lib::HashedString& inEntryPoint)
		: binary(std::move(inBinary))
		, stage(inStage)
		, entryPoint(inEntryPoint)
	{
		SPT_CHECK(!binary.empty());
	}

	lib::DynamicArray<Uint32>	binary;
	EShaderStage				stage;
	lib::HashedString			entryPoint;
};

} // spt::rhi