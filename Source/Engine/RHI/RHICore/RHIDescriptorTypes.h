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


enum class EDescriptorSetBindingFlags : Flags32
{
	None					= 0,
	UpdateAfterBind			= BIT(0),
	PartiallyBound			= BIT(1)
};


struct ShaderBindingDefinition
{
	ShaderBindingDefinition()
		: m_bindingIdx(idxNone<Uint32>)
		, m_descriptorType(EDescriptorType::UniformBuffer)
		, m_descriptorCount(1)
		, m_shaderStagesMask(EShaderStageFlags::None)
		, m_bindingFlags(EDescriptorSetBindingFlags::None)
	{ }

	Uint32							m_bindingIdx;
	EDescriptorType					m_descriptorType;
	Uint32							m_descriptorCount;
	EShaderStageFlags				m_shaderStagesMask;
	EDescriptorSetBindingFlags		m_bindingFlags;
};


enum class EBindingsLayoutFlags : Flags32
{
	None					= 0,
	UpdateAfterBindPool		= BIT(1)
};


struct ShaderBindingsSetDefinition
{
	ShaderBindingsSetDefinition()
		: m_flags(EBindingsLayoutFlags::None)
	{ }

	lib::DynamicArray<ShaderBindingDefinition>		m_bindingDefinitions;
	EBindingsLayoutFlags							m_flags;
};

} // spt::rhi
