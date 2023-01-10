#include "RenderInstancesList.h"
#include "RendererUtils.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderInstancesListBase =======================================================================

RenderInstancesListBase::RenderInstancesListBase(const rdr::RendererResourceName& listName, Uint64 dataSize, Uint64 instanceDataAlignment)
	: m_instances(CreateInstancesBuffer(listName, dataSize))
	, m_instanceDataAlignment(instanceDataAlignment)
{ }

const lib::SharedRef<rdr::Buffer>& RenderInstancesListBase::GetInstancesBuffer() const
{
	return m_instances;
}

rhi::RHISuballocation RenderInstancesListBase::AddInstanceImpl(const Byte* instanceData, Uint64 instanceDataSize)
{
	SPT_PROFILER_FUNCTION();
	
	const rhi::SuballocationDefinition suballocationDef(instanceDataSize, m_instanceDataAlignment, rhi::EBufferSuballocationFlags::PreferFasterAllocation);

	rhi::RHISuballocation instanceSuballocation;
	instanceSuballocation = m_instances->GetRHI().CreateSuballocation(suballocationDef);

	SPT_CHECK(instanceSuballocation.IsValid());
	gfx::UploadDataToBuffer(m_instances, instanceSuballocation.GetOffset(), instanceData, instanceDataSize);

	return instanceSuballocation;
}

void RenderInstancesListBase::RemoveInstanceImpl(const rhi::RHISuballocation& instanceSuballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(instanceSuballocation.IsValid());

	m_pendingRemoveSuballocations.emplace_back(instanceSuballocation);
}

void RenderInstancesListBase::FlushRemovedInstancesImpl(Uint64 instanceDataSize)
{
	SPT_PROFILER_FUNCTION();

	for (const rhi::RHISuballocation& removedSuballocation : m_pendingRemoveSuballocations)
	{
		m_instances->GetRHI().DestroySuballocation(removedSuballocation);
		gfx::FillBuffer(m_instances, removedSuballocation.GetOffset(), instanceDataSize, 0);
	}
}

lib::SharedRef<rdr::Buffer> RenderInstancesListBase::CreateInstancesBuffer(const rdr::RendererResourceName& listName, Uint64 dataSize) const
{
	rhi::RHIAllocationInfo bufferAllocationInfo(rhi::EMemoryUsage::CPUToGPU);

	rhi::BufferDefinition instancesBufferDef;
	instancesBufferDef.size		= dataSize;
	instancesBufferDef.usage	= rhi::EBufferUsage::Storage;
	instancesBufferDef.flags	= rhi::EBufferFlags::WithVirtualSuballocations;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(listName + "_InstancesBuffer"), instancesBufferDef, bufferAllocationInfo);
}

} // spt::rs
