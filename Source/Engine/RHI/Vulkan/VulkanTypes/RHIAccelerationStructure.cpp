#include "RHIAccelerationStructure.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "RHIBuffer.h"
#include "Vulkan/Device/LogicalDevice.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIAccelerationStructureReleaseTicket =========================================================

void RHIAccelerationStructureReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), handle.GetValue(), nullptr);
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIAccelerationStructure ======================================================================

RHIAccelerationStructure::RHIAccelerationStructure()
	: m_handle(VK_NULL_HANDLE)
	, m_buildScratchSize(0)
	, m_primitivesCount(0)
{ }

Bool RHIAccelerationStructure::IsValid() const
{
	return !!m_handle;
}

void RHIAccelerationStructure::SetName(const lib::HashedString& name)
{
	m_name.SetToObject(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);
}

const lib::HashedString& RHIAccelerationStructure::GetName() const
{
	return m_name.Get();
}

VkAccelerationStructureKHR RHIAccelerationStructure::GetHandle() const
{
	return m_handle;
}

Uint64 RHIAccelerationStructure::GetBuildScratchSize() const
{
	return m_buildScratchSize;
}

VkAccelerationStructureBuildRangeInfoKHR RHIAccelerationStructure::CreateBuildRangeInfo() const
{
	VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo{};
	buildRangeInfo.primitiveCount	= m_primitivesCount;
	buildRangeInfo.primitiveOffset	= 0;
	buildRangeInfo.firstVertex		= 0;
	buildRangeInfo.transformOffset	= 0;

	return buildRangeInfo;
}

RHIAccelerationStructureReleaseTicket RHIAccelerationStructure::DeferredReleaseInternal()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	RHIAccelerationStructureReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif // SPT_RHI_DEBUG

	m_name.Reset(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR);

	m_handle = VK_NULL_HANDLE;

	m_buildScratchSize	= 0;
	m_primitivesCount	= 0;

	SPT_CHECK(!IsValid());

	return releaseTicket;
}

DeviceAddress RHIAccelerationStructure::GetDeviceAddress() const
{
	VkAccelerationStructureDeviceAddressInfoKHR asDeviceAddressInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
	asDeviceAddressInfo.accelerationStructure = m_handle;

	return vkGetAccelerationStructureDeviceAddressKHR(VulkanRHI::GetDeviceHandle(), &asDeviceAddressInfo);
}

void RHIAccelerationStructure::CopySRVDescriptor(Byte* dst) const
{
	SPT_CHECK(IsValid());

	VkDescriptorGetInfoEXT info{ VK_STRUCTURE_TYPE_DESCRIPTOR_GET_INFO_EXT };
	info.type                       = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	info.data.accelerationStructure = GetDeviceAddress();

	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	vkGetDescriptorEXT(device.GetHandle(), &info, device.GetDescriptorProps().SizeOf(rhi::EDescriptorType::AccelerationStructure), dst);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIBottomLevelAS ==============================================================================

RHIBottomLevelAS::RHIBottomLevelAS()
	: m_locationsDeviceAddress(0)
	, m_maxVerticesNum(0)
	, m_indicesDeviceAddress(0)
	, m_geometryFlags(0)
{ }

void RHIBottomLevelAS::InitializeRHI(const rhi::BLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());
	SPT_CHECK(definition.geometryType == rhi::EBLASGeometryType::Triangles);

	m_locationsDeviceAddress	= definition.trianglesGeometry.vertexLocationsAddress;
	m_maxVerticesNum			= definition.trianglesGeometry.maxVerticesNum;
	m_indicesDeviceAddress		= definition.trianglesGeometry.indicesAddress;
	m_geometryFlags				= RHIToVulkan::GetGeometryFlags(definition.geometryFlags);
	
	VkAccelerationStructureGeometryKHR geometry;
	const VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = CreateBuildGeometryInfo(OUT geometry);

	const Uint32 indicesNum = definition.trianglesGeometry.indicesNum;
	SPT_CHECK(indicesNum % 3 == 0 && indicesNum > 0);
	m_primitivesCount = indicesNum / 3;

	VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(VulkanRHI::GetDeviceHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &m_primitivesCount, &buildSizeInfo);

	const Uint64 accelerationStructureSize = buildSizeInfo.accelerationStructureSize;

	if (!accelerationStructureBuffer.IsValid())
	{
		const rhi::BufferDefinition asBufferDef(accelerationStructureSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::AccelerationStructureStorage));
		accelerationStructureBuffer.InitializeRHI(asBufferDef, rhi::RHICommittedAllocationDefinition(rhi::EMemoryUsage::GPUOnly));

		accelerationStructureBufferOffset = 0;
	}

	SPT_CHECK(accelerationStructureBuffer.IsValid());
	SPT_CHECK(accelerationStructureBufferOffset + accelerationStructureSize <= accelerationStructureBuffer.GetSize());

	VkAccelerationStructureCreateInfoKHR asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    asInfo.buffer	= accelerationStructureBuffer.GetHandle();
    asInfo.offset	= accelerationStructureBufferOffset;
    asInfo.size		= accelerationStructureSize;
    asInfo.type		= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	SPT_VK_CHECK(vkCreateAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), &asInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
	
	m_buildScratchSize = buildSizeInfo.buildScratchSize;
}

void RHIBottomLevelAS::ReleaseRHI()
{
	RHIAccelerationStructureReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIAccelerationStructureReleaseTicket RHIBottomLevelAS::DeferredReleaseRHI()
{
	RHIAccelerationStructureReleaseTicket releaseTicket = DeferredReleaseInternal();

	m_locationsDeviceAddress	= 0;
	m_maxVerticesNum			= 0;
	m_indicesDeviceAddress		= 0;
	m_geometryFlags				= 0;

	return releaseTicket;
}

VkAccelerationStructureBuildGeometryInfoKHR RHIBottomLevelAS::CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry) const
{
	geometry = CreateGeometryData();

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildGeometryInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildGeometryInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildGeometryInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.geometryCount	= 1;
	buildGeometryInfo.pGeometries	= &geometry;

	return buildGeometryInfo;
}

VkAccelerationStructureGeometryKHR RHIBottomLevelAS::CreateGeometryData() const
{
	VkAccelerationStructureGeometryTrianglesDataKHR triangles{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR };
	triangles.vertexFormat				= VK_FORMAT_R32G32B32_SFLOAT;
	triangles.vertexData.deviceAddress	= m_locationsDeviceAddress;
	triangles.vertexStride				= sizeof(math::Vector3f);
	triangles.maxVertex					= m_maxVerticesNum;
	triangles.indexType					= VK_INDEX_TYPE_UINT32;
	triangles.indexData.deviceAddress	= m_indicesDeviceAddress;

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType		= VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	geometry.geometry.triangles	= triangles;
	geometry.flags				= m_geometryFlags;

	return geometry;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHITopLevelAS =================================================================================

RHITopLevelAS::RHITopLevelAS()
{ }

void RHITopLevelAS::InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset, OUT RHIBuffer& instancesBuildBuffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());
	SPT_CHECK(!instancesBuildBuffer.IsValid());

	const Uint32 instancesNum = static_cast<Uint32>(definition.instances.size());
	const Uint64 instancesBufferSize = instancesNum * sizeof(VkAccelerationStructureInstanceKHR);

	m_primitivesCount = static_cast<Uint32>(instancesNum);

	const rhi::BufferDefinition instancesBufferDef(instancesBufferSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ASBuildInputReadOnly));
	instancesBuildBuffer.InitializeRHI(instancesBufferDef, rhi::RHICommittedAllocationDefinition(rhi::EMemoryUsage::CPUToGPU));
	
	// Init buffer of VkAccelerationStructureInstanceKHR from TLASInstanceDefinitions
	{
		const RHIMappedBuffer<VkAccelerationStructureInstanceKHR> instancesMappedBuffer(instancesBuildBuffer);

		for(SizeType instanceIdx = 0; instanceIdx < definition.instances.size(); ++instanceIdx)
		{
			const rhi::TLASInstanceDefinition& instance = definition.instances[instanceIdx];
			// We need to transpose matrix because VkTransformMatrixKHR stores transform in row-major order
			const math::Matrix<Real32, 4, 3> transposedMatrix = instance.transform.transpose();

			VkAccelerationStructureInstanceKHR& instanceData = instancesMappedBuffer[instanceIdx];
			std::memcpy(instanceData.transform.matrix, transposedMatrix.data(), sizeof(VkTransformMatrixKHR));
			instanceData.instanceCustomIndex												= instance.customIdx;
			instanceData.mask																= instance.mask;
			instanceData.instanceShaderBindingTableRecordOffset								= instance.sbtRecordOffset;
			instanceData.flags																= RHIToVulkan::GetGeometryInstanceFlags(instance.flags);
			instanceData.accelerationStructureReference										= instance.blasAddress;
		}
	}

	VkAccelerationStructureGeometryKHR geometry;
	const VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = CreateBuildGeometryInfo(instancesBuildBuffer, OUT geometry);

	VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(VulkanRHI::GetDeviceHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &m_primitivesCount, &buildSizeInfo);

	const Uint64 accelerationStructureSize = buildSizeInfo.accelerationStructureSize;

	if (!accelerationStructureBuffer.IsValid())
	{
		const rhi::BufferDefinition tlasBufferDef(accelerationStructureSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::AccelerationStructureStorage));
		accelerationStructureBuffer.InitializeRHI(tlasBufferDef, rhi::RHICommittedAllocationDefinition(rhi::EMemoryUsage::GPUOnly));
	}

	SPT_CHECK(accelerationStructureSize + accelerationStructureBufferOffset <= accelerationStructureBuffer.GetSize());

	// create tlas
	VkAccelerationStructureCreateInfoKHR tlasInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
	tlasInfo.type	= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasInfo.size	= accelerationStructureSize;
	tlasInfo.buffer	= accelerationStructureBuffer.GetHandle();

	SPT_VK_CHECK(vkCreateAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), &tlasInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
	
	m_buildScratchSize = buildSizeInfo.buildScratchSize;
}

void RHITopLevelAS::ReleaseRHI()
{
	RHIAccelerationStructureReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIAccelerationStructureReleaseTicket RHITopLevelAS::DeferredReleaseRHI()
{
	return DeferredReleaseInternal();
}

VkAccelerationStructureBuildGeometryInfoKHR RHITopLevelAS::CreateBuildGeometryInfo(const RHIBuffer& instancesBuildBuffer, OUT VkAccelerationStructureGeometryKHR& geometry) const
{
	geometry = CreateGeometryData(instancesBuildBuffer);

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildGeometryInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries	= &geometry;
	buildGeometryInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

	return buildGeometryInfo;
}

VkAccelerationStructureGeometryKHR RHITopLevelAS::CreateGeometryData(const RHIBuffer& instancesBuildBuffer) const
{
	SPT_CHECK(instancesBuildBuffer.IsValid());

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instancesData.data.deviceAddress = instancesBuildBuffer.GetDeviceAddress();

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType		= VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances	= instancesData;

	return geometry;
}

} // spt::vulkan
