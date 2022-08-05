#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rhi
{

enum class EDescriptorType
{
	Sampler,
	CombinedImageSampler,
	SampledImage,
	StorageImage,
	UniformTexelBuffer,
	StorageTexelBuffer,
	UniformBuffer,
	StorageBuffer,
	UniformBufferDynamic,
	StorageBufferDynamic,
	AccelerationStructure,
};


namespace EDescriptorSetBindingFlags
{

enum Flags : Flags32
{
	None					= 0,
	UpdateAfterBind			= BIT(0),
	PartiallyBound			= BIT(1)
};

}


struct ShaderBindingDefinition
{
	ShaderBindingDefinition()
		: m_bindingIdx(idxNone<Uint32>)
		, m_descriptorType(EDescriptorType::UniformBuffer)
		, m_descriptorCount(1)
		, m_shaderStagesMask(0)
		, m_bindingFlags(0)
	{ }

	Uint32				m_bindingIdx;
	EDescriptorType		m_descriptorType;
	Uint32				m_descriptorCount;
	Flags32				m_shaderStagesMask;
	Flags32				m_bindingFlags;
};


namespace EBindingsLayoutFlags
{

enum Flags : Flags32
{
	None					= 0,
	UpdateAfterBindPool		= BIT(1)
};

}


struct ShaderBindingsSetDefinition
{
	ShaderBindingsSetDefinition()
		: m_flags(0)
	{ }

	lib::DynamicArray<ShaderBindingDefinition>		m_bindingDefinitions;
	Flags32											m_flags;
};

}
