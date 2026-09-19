#include "ConstantsAllocator.h"
#include "Types/Buffer.h"
#include "ResourcesManager.h"
#include "RHIBridge/RHIImpl.h"
#include "RHIBridge/RHILimitsImpl.h"


namespace spt::rdr
{

	
ConstantsAllocator::ConstantsAllocator(Uint32 stackSize)
{
	SPT_CHECK(stackSize > 0u);

	const rhi::BufferDefinition constantsBufferDef(stackSize, rhi::EBufferUsage::Uniform);
	const rhi::RHIAllocationInfo allocInfo{rhi::EMemoryUsage::CPUToGPU, rhi::EAllocationFlags::CreateMapped };
	m_buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("Constants Memory Arena"), constantsBufferDef, allocInfo);

	m_buffersAlignment = static_cast<Uint32>(rhi::RHILimits::GetMinUniformBufferOffsetAlignment());
}

ConstantBufferAllocation ConstantsAllocator::Allocate(Uint32 size)
{
	const Uint32 alignedSize = (size + m_buffersAlignment - 1u) / m_buffersAlignment * m_buffersAlignment;
	const Uint32 allocationOffset = m_currentOffset.fetch_add(alignedSize);
	SPT_CHECK(allocationOffset + size < static_cast<Uint32>(m_buffer->GetRHI().GetSize()));

	return ConstantBufferAllocation
	{
		.buffer =  m_buffer,
		.offset = allocationOffset,
		.size   = size
	};
}

} // spt::rdr
