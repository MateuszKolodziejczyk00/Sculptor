#include "RHIDescriptorSet.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHIAccelerationStructure.h"

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
	// poolSetIdx may be set to idx none if pool isn't part of descriptor set manager, e.g. pool single pool bound to context
	// such pools doesn't have any id, as they also don't require to deallocate descriptors, because descriptors are destroyed when pools are reseted
	SPT_CHECK(m_handle != VK_NULL_HANDLE);
	SPT_CHECK(m_poolIdx != idxNone<Uint16>);
}

void RHIDescriptorSet::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_DESCRIPTOR_SET);
}

const lib::HashedString& RHIDescriptorSet::GetName() const
{
	return m_name.Get();
}

void RHIDescriptorSet::ResetName()
{
	m_name.Reset(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_DESCRIPTOR_SET);
}

Bool RHIDescriptorSet::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
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
		// we have to add 1 to proper idx, to differentiate valid pointers from nullptrs
		.pBufferInfo		= reinterpret_cast<const VkDescriptorBufferInfo*>(m_buffers.size() + 1),
		.pTexelBufferView	= nullptr
	};

	m_writes.emplace_back(write);

	const VkDescriptorBufferInfo bufferInfo
	{
		.buffer	= buffer.GetHandle(),
		.offset	= static_cast<VkDeviceSize>(offset),
		.range	= static_cast<VkDeviceSize>(range)
	};

	m_buffers.emplace_back(bufferInfo);
}

void RHIDescriptorSetWriter::WriteBuffer(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHIBuffer& buffer, Uint64 offset, Uint64 range, const RHIBuffer& countBuffer, Uint64 countBufferOffset)
{
	SPT_CHECK_MSG(writeDef.descriptorType == rhi::EDescriptorType::StorageBuffer, "Counter buffers are allowed only for storage buffers (UAVs)");

	WriteBuffer(set, writeDef, buffer, offset, range);

	rhi::WriteDescriptorDefinition countBufferWriteDef{};
	countBufferWriteDef.bindingIdx = writeDef.bindingIdx + 1; // count buffer always use next binding idx
	countBufferWriteDef.arrayElement = writeDef.arrayElement;
	countBufferWriteDef.descriptorType = rhi::EDescriptorType::StorageBuffer;
	
	constexpr Uint64 countBufferRange = sizeof(Int32);

	WriteBuffer(set, countBufferWriteDef, countBuffer, countBufferOffset, countBufferRange);
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
		// we have to add 1 to proper idx, to differentiate valid pointers from nullptrs
		.pImageInfo			= reinterpret_cast<const VkDescriptorImageInfo*>(m_images.size() + 1),
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

void RHIDescriptorSetWriter::WriteAccelerationStructure(const RHIDescriptorSet& set, const rhi::WriteDescriptorDefinition& writeDef, const RHITopLevelAS& tlas)
{
	SPT_CHECK(set.IsValid());
	SPT_CHECK(tlas.IsValid());

	const VkWriteDescriptorSet write
	{
		.sType				= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		// store idx, as address may change during array reallocation
		// later this idx will be resolved before descriptor is updated
		// we have to add 1 to proper idx, to differentiate valid pointers from nullptrs
		.pNext				= reinterpret_cast<const VkDescriptorImageInfo*>(m_accelerationStructures.size() + 1),
		.dstSet				= set.GetHandle(),
		.dstBinding			= writeDef.bindingIdx,
		.dstArrayElement	= writeDef.arrayElement,
		.descriptorCount	= 1,
		.descriptorType		= RHIToVulkan::GetDescriptorType(writeDef.descriptorType),
		.pImageInfo			= nullptr,
		.pBufferInfo		= nullptr,
		.pTexelBufferView	= nullptr
	};

	m_writes.emplace_back(write);

	const VkWriteDescriptorSetAccelerationStructureKHR writeAccelerationStructure
	{
		.sType						= VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
		.accelerationStructureCount	= 1,
		.pAccelerationStructures	= VK_NULL_HANDLE // This handle will be resolved later
	};

	m_accelerationStructureHandles.emplace_back(tlas.GetHandle());
	m_accelerationStructures.emplace_back(writeAccelerationStructure);

	SPT_CHECK(m_accelerationStructureHandles.size() == m_accelerationStructures.size());
}

void RHIDescriptorSetWriter::Reserve(SizeType writesNum)
{
	m_writes.reserve(writesNum);
	m_buffers.reserve(writesNum);
	m_images.reserve(writesNum);
}

void RHIDescriptorSetWriter::Flush()
{
	ResolveReferences();
	SubmitWrites();
	Reset();
}

void RHIDescriptorSetWriter::ResolveReferences()
{
	for (VkWriteDescriptorSet& write : m_writes)
	{
		if (write.pBufferInfo)
		{
			const SizeType bufferInfoIdx = reinterpret_cast<SizeType>(write.pBufferInfo) - 1;
			write.pBufferInfo = &m_buffers[bufferInfoIdx];
		}
		if (write.pImageInfo)
		{
			const SizeType imageInfoIdx = reinterpret_cast<SizeType>(write.pImageInfo) - 1;
			write.pImageInfo = &m_images[imageInfoIdx];
		}
		if (write.pNext)
		{
			const SizeType accelerationStructureIdx = reinterpret_cast<SizeType>(write.pNext) - 1;
			write.pNext = &m_accelerationStructures[accelerationStructureIdx];
		}
	}

	for (SizeType asIdx = 0; asIdx < m_accelerationStructures.size(); ++asIdx)
	{
		m_accelerationStructures[asIdx].pAccelerationStructures = &m_accelerationStructureHandles[asIdx];
	}
}

void RHIDescriptorSetWriter::SubmitWrites()
{
	vkUpdateDescriptorSets(VulkanRHI::GetDeviceHandle(), static_cast<Uint32>(m_writes.size()), m_writes.data(), 0, nullptr);
}

void RHIDescriptorSetWriter::Reset()
{
	m_writes.clear();
	m_buffers.clear();
	m_images.clear();
}

} // spt::vulkan
