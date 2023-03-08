#include "RHIShaderBindingTable.h"
#include "RHIBuffer.h"
#include "Vulkan/VulkanRHILimits.h"
#include "MathUtils.h"
#include "Vulkan/VulkanRHI.h"
#include "RHIPipeline.h"
#include "RHICore/RHIAllocationTypes.h"

namespace spt::vulkan
{

RHIShaderBindingTable::RHIShaderBindingTable()
{ }

void RHIShaderBindingTable::InitializeRHI(const RHIPipeline& pipeline, const rhi::RayTracingShadersDefinition& shadersDef)
{
	SPT_PROFILER_FUNCTION();

	const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtPipelineProps = VulkanRHILimits::GetRayTracingPipelineProperties();

	const Uint32 groupHandleSize = rtPipelineProps.shaderGroupHandleSize;
	const Uint32 groupHandleAlignment = rtPipelineProps.shaderGroupHandleAlignment;

	const Uint32 groupHandleSizeAligned = math::Utils::RoundUp(groupHandleSize, groupHandleAlignment);

	m_rayGenRegion.stride	= static_cast<VkDeviceSize>(math::Utils::RoundUp(groupHandleSizeAligned, rtPipelineProps.shaderGroupBaseAlignment));
	m_rayGenRegion.size		= m_rayGenRegion.stride; // // The size of ray gen region must be always equal to the stride

	const auto initRegionSize = [ & ](VkStridedDeviceAddressRegionKHR& region, Uint32 groupCount)
	{
		region.stride	= static_cast<VkDeviceSize>(groupHandleSizeAligned);
		region.size		= static_cast<VkDeviceSize>(math::Utils::RoundUp(groupCount * groupHandleSizeAligned, rtPipelineProps.shaderGroupBaseAlignment));
	};

	initRegionSize(m_closestHitRegion, static_cast<Uint32>(shadersDef.closestHitModules.size()));
	initRegionSize(m_missRegion, static_cast<Uint32>(shadersDef.missModules.size()));

	const Uint32 handleCount = 1 + static_cast<Uint32>(shadersDef.closestHitModules.size()) + static_cast<Uint32>(shadersDef.missModules.size());

	const SizeType handlesDataSize = handleCount * groupHandleSize;
	lib::DynamicArray<Byte> handlesData(handlesDataSize);

	SPT_VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(VulkanRHI::GetDeviceHandle(), pipeline.GetHandle(), 0, handleCount, handlesDataSize, handlesData.data()));

	const Uint64 sbtSize = m_rayGenRegion.size + m_closestHitRegion.size + m_missRegion.size;

	const rhi::BufferDefinition sbtBufferDefinition(sbtSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ShaderBindingTable));
	m_sbtBuffer.InitializeRHI(sbtBufferDefinition, rhi::EMemoryUsage::CPUToGPU);

	{
		RHIMappedByteBuffer mappedBuffer(m_sbtBuffer);

		auto copyRegionHandles = [ &, sbtData = mappedBuffer.GetPtr(), firstGroupIdx = Uint32(0), sbtCurrentOffset = VkDeviceSize(0) ](VkStridedDeviceAddressRegionKHR& region, Uint32 groupCount) mutable
		{
			for (Uint32 i = 0; i < groupCount; ++i)
			{
				const Byte* handleData = handlesData.data() + (firstGroupIdx + i) * groupHandleSize;
				Byte* regionData = sbtData + sbtCurrentOffset + region.stride * i;
				std::memcpy(regionData, handleData, groupHandleSize);
			}

			region.deviceAddress = m_sbtBuffer.GetDeviceAddress() + sbtCurrentOffset;

			sbtCurrentOffset += region.size;
			firstGroupIdx += groupCount;
		};

		copyRegionHandles(m_rayGenRegion, 1);
		copyRegionHandles(m_closestHitRegion, static_cast<Uint32>(shadersDef.closestHitModules.size()));
		copyRegionHandles(m_missRegion, static_cast<Uint32>(shadersDef.missModules.size()));
	}
}

void RHIShaderBindingTable::ReleaseRHI()
{
	m_sbtBuffer.ReleaseRHI();
}

const VkStridedDeviceAddressRegionKHR& RHIShaderBindingTable::GetRayGenRegion() const
{
	return m_rayGenRegion;
}

const VkStridedDeviceAddressRegionKHR& RHIShaderBindingTable::GetClosestHitRegion() const
{
	return m_closestHitRegion;
}

const VkStridedDeviceAddressRegionKHR& RHIShaderBindingTable::GetMissRegion() const
{
	return m_missRegion;
}

void RHIShaderBindingTable::SetName(const lib::HashedString& name)
{
	m_sbtBuffer.SetName(name);
}

const lib::HashedString& RHIShaderBindingTable::GetName() const
{
	return m_sbtBuffer.GetName();
}

} // spt::vulkan
