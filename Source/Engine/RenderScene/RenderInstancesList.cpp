#include "RenderInstancesList.h"
#include "RendererUtils.h"
#include "ResourcesManager.h"
#include "BufferUtilities.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderInstancesListBase =======================================================================

RenderInstancesListBase::RenderInstancesListBase(const rdr::RendererResourceName& listName, Uint64 dataSize, Uint64 instanceDataAlignment)
	: m_instanceDataAlignment(instanceDataAlignment)
{
	rhi::RHIAllocationInfo bufferAllocationInfo(rhi::EMemoryUsage::CPUToGPU);

	rhi::BufferDefinition instancesBufferDef;
	instancesBufferDef.size		= dataSize;
	instancesBufferDef.usage	= rhi::EBufferUsage::Storage;
	instancesBufferDef.flags	= rhi::EBufferFlags::WithVirtualSuballocations;

	m_instances = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(listName + "_InstancesBuffer"), instancesBufferDef, bufferAllocationInfo);
}

const lib::SharedPtr<rdr::Buffer>& RenderInstancesListBase::GetInstancesBuffer() const
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
	gfx::UploadDataToBuffer(lib::Ref(m_instances), instanceSuballocation.GetOffset(), instanceData, instanceDataSize);

	return instanceSuballocation;
}

void RenderInstancesListBase::RemoveInstanceImpl(const rhi::RHISuballocation& instanceSuballocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(instanceSuballocation.IsValid());

	SPT_CHECK_NO_ENTRY_MSG("TODO");
	
	/*
	TODO:
	store removed suballocations in buffer
	at the end of the frame free pending suballocations and schedule FillBuffer commands to clear deleted instances on GPU before rendering next frame
	***	We cannot clear suballocations here, as we would also have to schedule FillBuffer here
		if during same frame other instance would be allocated in the same place FillBuffer would clear its data 
	*/
}

void RenderInstancesListBase::FlushRemovedInstancesImpl()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO");
}

} // spt::rsc
