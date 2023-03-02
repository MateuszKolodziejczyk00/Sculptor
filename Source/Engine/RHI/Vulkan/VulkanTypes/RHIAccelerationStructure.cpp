#include "RHIAccelerationStructure.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "RHIBuffer.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIAccelerationStructure ======================================================================

RHIAccelerationStructure::RHIAccelerationStructure()
	: m_handle(VK_NULL_HANDLE)
{ }

void RHIAccelerationStructure::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	if (IsValid())
	{
		vkDestroyAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());
		m_handle = VK_NULL_HANDLE;
		m_name.Reset();
	}
}

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

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIBottomLevelAS ==============================================================================

RHIBottomLevelAS::RHIBottomLevelAS()
	: m_locationsDeviceAddress(0)
	, m_maxVerticesNum(0)
	, m_indicesNum(0)
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
	m_indicesNum				= definition.trianglesGeometry.indicesNum;
	m_indicesDeviceAddress		= definition.trianglesGeometry.indicesAddress;
	m_geometryFlags				= RHIToVulkan::GetGeometryFlags(definition.geometryFlags);
	
	const VkAccelerationStructureGeometryKHR geometry = CreateGeometryData();

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildGeometryInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	buildGeometryInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildGeometryInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.geometryCount	= 1;
	buildGeometryInfo.pGeometries	= &geometry;

	SPT_CHECK(m_indicesNum % 3 == 0 && m_indicesNum > 0);
	const Uint32 primitivesCount = m_indicesNum / 3;

	VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(VulkanRHI::GetDeviceHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &primitivesCount, &buildSizeInfo);

	const Uint64 accelerationStructureSize = buildSizeInfo.accelerationStructureSize;

	if (!accelerationStructureBuffer.IsValid())
	{
		const rhi::BufferDefinition asBufferDef(accelerationStructureSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::AccelerationStructureStorage));
		accelerationStructureBuffer.InitializeRHI(asBufferDef, rhi::EMemoryUsage::GPUOnly);

		accelerationStructureBufferOffset = 0;
	}

	SPT_CHECK(accelerationStructureBuffer.IsValid());
	SPT_CHECK(accelerationStructureBufferOffset + accelerationStructureSize <= accelerationStructureBuffer.GetSize());

	VkAccelerationStructureCreateInfoKHR asInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    asInfo.buffer	= accelerationStructureBuffer.GetBufferHandle();
    asInfo.offset	= accelerationStructureBufferOffset;
    asInfo.size		= accelerationStructureSize;
    asInfo.type		= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	SPT_VK_CHECK(vkCreateAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), &asInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
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

void RHITopLevelAS::InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	RHIBuffer instancesBuffer;
	const Uint32 instancesNum = static_cast<Uint32>(definition.instances.size());
	const Uint64 instancesBufferSize = instancesNum * sizeof(VkAccelerationStructureInstanceKHR);

	const rhi::BufferDefinition instancesBufferDef(instancesBufferSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ASBuildInputReadOnly));
	instancesBuffer.InitializeRHI(instancesBufferDef, rhi::EMemoryUsage::CPUToGPU);
	RHIMappedBuffer<VkAccelerationStructureInstanceKHR> instancesMappedBuffer(instancesBuffer);

	for(SizeType instanceIdx = 0; instanceIdx < definition.instances.size(); ++instanceIdx)
	{
		// convert TLASInstanceDefinition to VkAccelerationStructureInstanceKHR
		using TransformMatrix = rhi::TLASInstanceDefinition::TransformMatrix;

		const rhi::TLASInstanceDefinition& instance = definition.instances[instanceIdx];

		VkAccelerationStructureInstanceKHR& instanceData = instancesMappedBuffer[instanceIdx];
		math::Map<TransformMatrix>(reinterpret_cast<Real32*>(&instanceData.transform))	= instance.transform;
		instanceData.instanceCustomIndex												= instance.customIdx;
		instanceData.mask																= instance.mask;
		instanceData.instanceShaderBindingTableRecordOffset								= instance.sbtRecordOffset;
		instanceData.flags																= RHIToVulkan::GetGeometryInstanceFlags(instance.flags);
		instanceData.accelerationStructureReference										= instance.blasAddress;
	}

	//if (!accelerationStructureBuffer.IsValid())
	//{
	//	const rhi::BufferDefinition tlasBufferDef(instancesDataSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ASBuildInputReadOnly));
	//	accelerationStructureBuffer.InitializeRHI(tlasBufferDef, rhi::EMemoryUsage::CPUToGPU);
	//}

	//SPT_CHECK(accelerationStructureBuffer.IsValid());

}

} // spt::vulkan
