#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "SculptorCoreTypes.h"


namespace spt::rhi
{
struct WriteDescriptorDefinition;
} // spt::rhi


namespace spt::vulkan
{

class RHIBuffer;
class RHITextureView;


/**
 * Descriptor sets are not initialized as other RHI objects.
 * Instead they has to be allocated and freed using RHIDescriptorSetManager or using DescriptorSetPool
 * This allows batching all allocations, and also makes making descriptors pools threadsafe easier.
 */
class RHI_API RHIDescriptorSet
{
public:

	RHIDescriptorSet();
	RHIDescriptorSet(VkDescriptorSet handle, Uint16 poolSetIdx, Uint16 poolIdx);

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	Bool IsValid() const;

	// Vulkan ================================================

	VkDescriptorSet		GetHandle() const;
	Uint16				GetPoolSetIdx() const;
	Uint16				GetPoolIdx() const;

private:

	VkDescriptorSet		m_handle;
	Uint16				m_poolSetIdx;
	Uint16				m_poolIdx;

	DebugName			m_name;
};


class RHI_API RHIDescriptorSetWriter
{
public:

	RHIDescriptorSetWriter();

	void WriteBuffer(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHIBuffer& buffer, Uint64 offset, Uint64 range);
	void WriteBuffer(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHIBuffer& buffer, Uint64 offset, Uint64 range, const RHIBuffer& countBuffer, Uint64 countBufferOffset);
	void WriteTexture(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHITextureView& textureView);

	void Reserve(SizeType writesNum);
	void Flush();

private:

	void ResolveReferences();
	void SubmitWrites();
	void Reset();

	lib::DynamicArray<VkWriteDescriptorSet>		m_writes;
	lib::DynamicArray<VkDescriptorBufferInfo>	m_buffers;
	lib::DynamicArray<VkDescriptorImageInfo>	m_images;
};

} // spt::vulkan