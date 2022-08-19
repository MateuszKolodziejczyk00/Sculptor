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


inline Flags32 GetStageFlag(EShaderStage stage)
{
	return 1 << (static_cast<Flags32>(stage) - 1);
}


struct ShaderModuleDefinition
{
	ShaderModuleDefinition()
		: m_stage(EShaderStage::Vertex)
	{ }

	ShaderModuleDefinition(lib::DynamicArray<Uint32> binary, EShaderStage stage)
		: m_binary(std::move(binary))
		, m_stage(EShaderStage::Vertex)
	{
		SPT_CHECK(!m_binary.empty());
	}

	lib::DynamicArray<Uint32>		m_binary;
	EShaderStage					m_stage;
};

}