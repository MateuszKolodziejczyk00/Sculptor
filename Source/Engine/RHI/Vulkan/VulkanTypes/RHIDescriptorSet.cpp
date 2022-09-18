#include "RHIDescriptorSet.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "RHIBuffer.h"
#include "RHITexture.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace utils
{

static VkImageLayout SelectImageLayout(rhi::EDescriptorType type)
{
	switch (type)
	{
	case spt::rhi::EDescriptorType::CombinedTextureSampler:	return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case spt::rhi::EDescriptorType::SampledTexture:			return VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
	case spt::rhi::EDescriptorType::StorageTexture:			return VK_IMAGE_LAYOUT_GENERAL;

	default:

		SPT_CHECK_NO_ENTRY();
		return VK_IMAGE_LAYOUT_UNDEFINED;
	}
}

} // utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSet ==============================================================================

RHIDescriptorSet::RHIDescriptorSet()
	: m_handle(VK_NULL_HANDLE)
	, m_poolSetIdx(idxNone<Uint16>)
	, m_poolIdx(idxNone<Uint16>)
{ }

RHIDescriptorSet::RHIDescriptorSet(VkDescriptorSet handle, Uint16 poolSetIdx, Uint16 poolIdx)
	: m_handle(handle)
	, m_poolSetIdx(poolSetIdx)
	, m_poolIdx(poolIdx)
{
	SPT_CHECK(m_handle != VK_NULL_HANDLE);
	SPT_CHECK(m_poolSetIdx != idxNone<Uint16>);
	SPT_CHECK(m_poolIdx != idxNone<Uint16>);
}

Bool RHIDescriptorSet::IsValid() const
{
	return m_handle != VK_NULL_HANDLE && m_poolSetIdx != idxNone<Uint16> && m_poolIdx != idxNone<Uint16>;
}

VkDescriptorSet RHIDescriptorSet::GetHandle() const
{
	return m_handle;
}

Uint16 RHIDescriptorSet::GetPoolSetIdx() const
{
	return m_poolSetIdx;
}

Uint16 RHIDescriptorSet::GetPoolIdx() const
{
	return m_poolIdx;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIDescriptorSetWriter ========================================================================

RHIDescriptorSetWriter::RHIDescriptorSetWriter()
{
	Reserve(64);
}

void RHIDescriptorSetWriter::WriteBuffer(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHIBuffer& buffer, Uint64 offset, Uint64 range)
{
	SPT_CHECK(set.IsValid());
	SPT_CHECK(buffer.IsValid());

	const VkWriteDescriptorSet write
	{
		.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet				= set.GetHandle(),
		.dstBinding			= writeDef.bindingIdx,
		.dstArrayElement	= writeDef.arrayElement,
		.descriptorCount	= 1,
		.descriptorType		= RHIToVulkan::GetDescriptorType(writeDef.descriptorType),
		.pImageInfo			= nullptr,
		// store idx, as address may change during array reallocation
		// later this idx will be resolved before descriptor is updated
		.pBufferInfo		= reinterpret_cast<const VkDescriptorBufferInfo*>(m_buffers.size()),
		.pTexelBufferView	= nullptr
	};

	m_writes.emplace_back(write);

	const VkDescriptorBufferInfo bufferInfo
	{
		.buffer	= buffer.GetBufferHandle(),
		.offset	= static_cast<VkDeviceSize>(offset),
		.range	= static_cast<VkDeviceSize>(range)
	};

	m_buffers.emplace_back(bufferInfo);
}

void RHIDescriptorSetWriter::WriteTexture(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHITextureView& textureView)
{
	SPT_CHECK(set.IsValid());
	SPT_CHECK(textureView.IsValid());

	const VkWriteDescriptorSet write
	{
		.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet				= set.GetHandle(),
		.dstBinding			= writeDef.bindingIdx,
		.dstArrayElement	= writeDef.arrayElement,
		.descriptorCount	= 1,
		.descriptorType		= RHIToVulkan::GetDescriptorType(writeDef.descriptorType),
		// store idx, as address may change during array reallocation
		// later this idx will be resolved before descriptor is updated
		.pImageInfo			= reinterpret_cast<const VkDescriptorImageInfo*>(m_images.size()),
		.pBufferInfo		= nullptr,
		.pTexelBufferView	= nullptr
	};

	m_writes.emplace_back(write);

	const VkDescriptorImageInfo imageInfo
	{
		.sampler		= nullptr,
		.imageView		= textureView.GetHandle(),
		.imageLayout	= utils::SelectImageLayout(writeDef.descriptorType)
	};

	m_images.emplace_back(imageInfo);
}

void RHIDescriptorSetWriter::Reserve(SizeType writesNum)
{
	SPT_PROFILER_FUNCTION();

	m_writes.reserve(writesNum);
	m_buffers.reserve(writesNum);
	m_images.reserve(writesNum);
}

void RHIDescriptorSetWriter::Flush()
{
	SPT_PROFILER_FUNCTION();

	ResolveReferences();
	SubmitWrites();
	Reset();
}

void RHIDescriptorSetWriter::ResolveReferences()
{
	SPT_PROFILER_FUNCTION();
	
	for (VkWriteDescriptorSet& write : m_writes)
	{
		if (write.pBufferInfo)
		{
			const SizeType bufferInfoIdx = reinterpret_cast<SizeType>(write.pBufferInfo);
			write.pBufferInfo = &m_buffers[bufferInfoIdx];
		}
		if (write.pImageInfo)
		{
			const SizeType imageInfoIdx = reinterpret_cast<SizeType>(write.pImageInfo);
			write.pImageInfo = &m_images[imageInfoIdx];
		}
	}
}

void RHIDescriptorSetWriter::SubmitWrites()
{
	SPT_PROFILER_FUNCTION();

	vkUpdateDescriptorSets(VulkanRHI::GetDeviceHandle(), static_cast<Uint32>(m_writes.size()), m_writes.data(), 0, nullptr);
}

void RHIDescriptorSetWriter::Reset()
{
	m_writes.clear();
	m_buffers.clear();
	m_images.clear();
}

} // spt::vulkan
