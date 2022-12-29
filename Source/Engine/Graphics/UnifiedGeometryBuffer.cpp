#include "UnifiedGeometryBuffer.h"
#include "ResourcesManager.h"

namespace spt::gfx
{

UnifiedGeometryBuffer& UnifiedGeometryBuffer::Get()
{
	static UnifiedGeometryBuffer ugb;
	return ugb;
}

rhi::RHISuballocation UnifiedGeometryBuffer::Allocate(Uint64 size)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_bufferInstance);
	SPT_CHECK(size > 0);

	rhi::RHISuballocation suballocation = m_bufferInstance->GetRHI().CreateSuballocation(rhi::SuballocationDefinition(size, 4, rhi::EBufferSuballocationFlags::PreferMinMemory));
	SPT_CHECK(suballocation.IsValid());

	return suballocation;
}

void UnifiedGeometryBuffer::Deallocate(rhi::RHISuballocation allocation)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_bufferInstance);
	SPT_CHECK(allocation.IsValid());

	m_bufferInstance->GetRHI().DestroySuballocation(allocation);
}

const lib::SharedPtr<rdr::Buffer>& UnifiedGeometryBuffer::GetBufferInstance() const
{
	return m_bufferInstance;
}

const lib::SharedPtr<rdr::BufferView>& UnifiedGeometryBuffer::GetBufferView() const
{
	return m_bufferView;
}

UnifiedGeometryBuffer::UnifiedGeometryBuffer()
{
	rhi::BufferDefinition bufferDefinition;
	bufferDefinition.size = 512 * 1024 * 1024;
	bufferDefinition.usage = lib::Flags(rhi::EBufferUsage::Storage, rhi::EBufferUsage::TransferDst);
	bufferDefinition.flags = rhi::WithVirtualSuballocations;

	rhi::RHIAllocationInfo allocationInfo;
	allocationInfo.memoryUsage = rhi::EMemoryUsage::GPUOnly;
	allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateDedicatedAllocation;

	m_bufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("UnifiedGeometryBuffer"), bufferDefinition, allocationInfo);
	m_bufferView = m_bufferInstance->CreateView(0, bufferDefinition.size);
}

} // spt::gfx
