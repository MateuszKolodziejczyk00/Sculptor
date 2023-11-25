#include "Buffer.h"
#include "RendererUtils.h"
#include "GPUMemoryPool.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

Buffer::Buffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition)
{
	GetRHI().InitializeRHI(definition, allocationDefinition.GetRHIAllocationDef());
	GetRHI().SetName(name.Get());

	if(allocationDefinition.IsPlaced())
	{
		m_owningMemoryPool = allocationDefinition.GetPlacedAllocationDef().memoryPool;
	}
}

Buffer::Buffer(const RendererResourceName& name, const rhi::RHIBuffer& rhiBufferInstance)
{
	SPT_CHECK_MSG(rhiBufferInstance.IsValid(), "Invalid RHI buffer instance");

	GetRHI() = rhiBufferInstance;
	GetRHI().SetName(name.Get());
}

Bool Buffer::HasBoundMemory() const
{
	return GetRHI().HasBoundMemory();
}

void Buffer::BindMemory(const AllocationDefinition& definition)
{
	SPT_CHECK(!HasBoundMemory());

	if (definition.IsPlaced())
	{
		m_owningMemoryPool = definition.GetPlacedAllocationDef().memoryPool;
	}
	RHIBufferMemoryOwner::BindMemory(GetRHI(), definition.GetRHIAllocationDef());
}

void Buffer::ReleasePlacedAllocation()
{
	SPT_CHECK(HasBoundMemory());
	SPT_CHECK(!!m_owningMemoryPool);

	rhi::RHIGPUMemoryPool& memoryPool = m_owningMemoryPool->GetRHI();

	const rhi::RHIResourceAllocationHandle allocation = RHIBufferMemoryOwner::ReleasePlacedAllocation(GetRHI());

	SPT_CHECK(std::holds_alternative<rhi::RHIPlacedAllocation>(allocation) || std::holds_alternative<rhi::RHINullAllocation>(allocation))
	if (std::holds_alternative<rhi::RHIPlacedAllocation>(allocation))
	{
		memoryPool.Free(std::get<rhi::RHIPlacedAllocation>(allocation).GetSuballocation());
	}

	m_owningMemoryPool.reset();
}

BufferView Buffer::CreateFullView() const
{
	return BufferView(lib::Ref(const_cast<Buffer*>(this)->shared_from_this()), 0, GetRHI().GetSize());
}

Uint64 Buffer::GetSize() const
{
	return GetRHI().GetSize();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer.ToSharedPtr())
	, m_offset(offset)
	, m_size(size)
{ }

void BufferView::Initialize(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
{
	m_buffer = buffer;
	m_offset = offset;
	m_size = size;
}

Bool BufferView::operator==(const BufferView& rhs) const
{
	return m_buffer == rhs.m_buffer
		&& m_offset == rhs.m_offset
		&& m_size == rhs.m_size;
}

const lib::SharedRef<Buffer>& BufferView::GetBuffer() const
{
	return m_buffer;
}

Uint64 BufferView::GetOffset() const
{
	return m_offset;
}

Uint64 BufferView::GetSize() const
{
	return m_size;
}

}
