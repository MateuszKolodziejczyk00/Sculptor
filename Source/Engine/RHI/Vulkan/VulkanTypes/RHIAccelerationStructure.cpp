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
	, m_buildScratchSize(0)
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
	
	VkAccelerationStructureGeometryKHR geometry;
	const VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = CreateBuildGeometryInfo(OUT geometry);

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
    asInfo.buffer	= accelerationStructureBuffer.GetHandle();
    asInfo.offset	= accelerationStructureBufferOffset;
    asInfo.size		= accelerationStructureSize;
    asInfo.type		= VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

	SPT_VK_CHECK(vkCreateAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), &asInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));
	
	m_buildScratchSize = buildSizeInfo.buildScratchSize;
}

void RHIBottomLevelAS::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	if (IsValid())
	{
		vkDestroyAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());
		m_handle = VK_NULL_HANDLE;
		m_name.Reset();

		m_locationsDeviceAddress	= 0;
		m_maxVerticesNum			= 0;
		m_indicesNum				= 0;
		m_indicesDeviceAddress		= 0;
		m_geometryFlags				= 0;
		m_buildScratchSize			= 0;
	}
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

void RHITopLevelAS::InitializeRHI(const rhi::TLASDefinition& definition, INOUT RHIBuffer& accelerationStructureBuffer, INOUT Uint64& accelerationStructureBufferOffset)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());
	SPT_CHECK(!m_instancesBuildBuffer.IsValid());

	const Uint32 instancesNum = static_cast<Uint32>(definition.instances.size());
	const Uint64 instancesBufferSize = instancesNum * sizeof(VkAccelerationStructureInstanceKHR);

	const rhi::BufferDefinition instancesBufferDef(instancesBufferSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::ASBuildInputReadOnly));
	m_instancesBuildBuffer.InitializeRHI(instancesBufferDef, rhi::EMemoryUsage::CPUToGPU);
	
	// Init buffer of VkAccelerationStructureInstanceKHR from TLASInstanceDefinitions
	{
		const RHIMappedBuffer<VkAccelerationStructureInstanceKHR> instancesMappedBuffer(m_instancesBuildBuffer);

		for(SizeType instanceIdx = 0; instanceIdx < definition.instances.size(); ++instanceIdx)
		{
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
	}

	VkAccelerationStructureGeometryKHR geometry;
	const VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = CreateBuildGeometryInfo(OUT geometry);

	const Uint32 maxPrimitivesCount = static_cast<Uint32>(definition.instances.size());

	VkAccelerationStructureBuildSizesInfoKHR buildSizeInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
	vkGetAccelerationStructureBuildSizesKHR(VulkanRHI::GetDeviceHandle(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeometryInfo, &maxPrimitivesCount, &buildSizeInfo);

	const Uint64 accelerationStructureSize = buildSizeInfo.accelerationStructureSize;

	if (!accelerationStructureBuffer.IsValid())
	{
		const rhi::BufferDefinition tlasBufferDef(accelerationStructureSize, lib::Flags(rhi::EBufferUsage::DeviceAddress, rhi::EBufferUsage::AccelerationStructureStorage));
		accelerationStructureBuffer.InitializeRHI(tlasBufferDef, rhi::EMemoryUsage::GPUOnly);
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
	SPT_PROFILER_FUNCTION();

	if (IsValid())
	{
		vkDestroyAccelerationStructureKHR(VulkanRHI::GetDeviceHandle(), m_handle, VulkanRHI::GetAllocationCallbacks());
		m_handle = VK_NULL_HANDLE;
		m_name.Reset();
	}

	m_buildScratchSize = 0;

	ClearInstancesBuildBuffer();
}

VkAccelerationStructureBuildGeometryInfoKHR RHITopLevelAS::CreateBuildGeometryInfo(OUT VkAccelerationStructureGeometryKHR& geometry) const
{
	geometry = CreateGeometryData();

	VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
	buildGeometryInfo.flags			= VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	buildGeometryInfo.geometryCount = 1;
	buildGeometryInfo.pGeometries	= &geometry;
	buildGeometryInfo.mode			= VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	buildGeometryInfo.type			= VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

	return buildGeometryInfo;
}

void RHITopLevelAS::ClearInstancesBuildBuffer()
{
	if (m_instancesBuildBuffer.IsValid())
	{
		m_instancesBuildBuffer.ReleaseRHI();
	}
}

VkAccelerationStructureGeometryKHR RHITopLevelAS::CreateGeometryData() const
{
	SPT_CHECK(m_instancesBuildBuffer.IsValid());

	VkAccelerationStructureGeometryInstancesDataKHR instancesData{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR };
	instancesData.data.deviceAddress = m_instancesBuildBuffer.GetDeviceAddress();

	VkAccelerationStructureGeometryKHR geometry{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
	geometry.geometryType		= VK_GEOMETRY_TYPE_INSTANCES_KHR;
	geometry.geometry.instances	= instancesData;

	return geometry;
}

} // spt::vulkan
