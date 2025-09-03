#include "Buffer.h"
#include "RendererUtils.h"
#include "GPUMemoryPool.h"
#include "DescriptorSetState/DescriptorManager.h"
#include "Renderer.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferView ====================================================================================

BufferView::BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size)
	: m_buffer(buffer.ToSharedPtr())
	, m_offset(offset)
	, m_size(size)
{
	SPT_CHECK(offset + size <= buffer->GetSize());
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

//////////////////////////////////////////////////////////////////////////////////////////////////
// BindableBufferView ============================================================================

BindableBufferView::BindableBufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size, BufferViewDescriptorsAllocation externalDescriptorsAllocation /*= BufferViewDescriptorsAllocation{}*/)
	: BufferView(buffer, offset, size)
{
	CreateDescriptors(std::move(externalDescriptorsAllocation));
}

BindableBufferView::~BindableBufferView()
{
	if (m_uavDescriptor.IsValid())
	{
		Renderer::GetDescriptorManager().ClearDescriptorInfo(m_uavDescriptor.Get());

		Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry::CreateLambda(
		[uavDescriptor = std::move(m_uavDescriptor)]() mutable
		{
			Renderer::GetDescriptorManager().FreeResourceDescriptor(std::move(uavDescriptor));
		}));
	}
}

lib::SharedPtr<BindableBufferView> BindableBufferView::AsSharedPtr()
{
	if (this == &GetBuffer()->GetFullViewRef())
	{
		return GetBuffer()->GetFullView();
	}

	return shared_from_this();
}

void BindableBufferView::CreateDescriptors(BufferViewDescriptorsAllocation externalDescriptorsAllocation)
{
	DescriptorManager& descriptorManager = Renderer::GetDescriptorManager();

	const rhi::RHIBuffer& rhiBuffer = GetBuffer()->GetRHI();

	if (lib::HasAnyFlag(rhiBuffer.GetUsage(), rhi::EBufferUsage::Storage))
	{
		m_uavDescriptor = externalDescriptorsAllocation.uavDescriptor.IsValid() ? std::move(externalDescriptorsAllocation.uavDescriptor) : descriptorManager.AllocateResourceDescriptor();
		descriptorManager.UploadUAVDescriptor(m_uavDescriptor.Get(), *this);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Buffer ========================================================================================

Buffer::Buffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation externalDescriptorsAllocation /*= BufferViewDescriptorsAllocation{}*/)
{
	GetRHI().InitializeRHI(definition, allocationDefinition.GetRHIAllocationDef());
	GetRHI().SetName(name.Get());

	if(allocationDefinition.IsPlaced())
	{
		m_owningMemoryPool = allocationDefinition.GetPlacedAllocationDef().memoryPool;
	}

	InitFullView(std::move(externalDescriptorsAllocation));
}

Buffer::Buffer(const RendererResourceName& name, const rhi::RHIBuffer& rhiBufferInstance)
{
	SPT_CHECK_MSG(rhiBufferInstance.IsValid(), "Invalid RHI buffer instance");

	GetRHI() = rhiBufferInstance;
	GetRHI().SetName(name.Get());

	InitFullView(BufferViewDescriptorsAllocation{});
}

Buffer::~Buffer()
{
	m_fullView.Destroy();
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

lib::SharedPtr<BindableBufferView> Buffer::GetFullView() const
{
	// Pointer to view but uses control block of Buffer. That's because full view doesn't have strong ref to buffer to  create circular reference
	return lib::SharedPtr<BindableBufferView>(shared_from_this(), const_cast<BindableBufferView*>(&m_fullView.Get()));
}

const BindableBufferView& Buffer::GetFullViewRef() const
{
	return m_fullView.Get();
}

lib::SharedPtr<BindableBufferView> Buffer::CreateView(Uint64 offset, Uint64 size, BufferViewDescriptorsAllocation externalDescriptorsAllocation /*= BufferViewDescriptorsAllocation()*/) const
{
	return lib::MakeShared<BindableBufferView>(lib::Ref(const_cast<Buffer*>(this)->shared_from_this()), offset, size, std::move(externalDescriptorsAllocation));
}

Uint64 Buffer::GetSize() const
{
	return GetRHI().GetSize();
}

void Buffer::InitFullView(BufferViewDescriptorsAllocation externalDescriptorsAllocation)
{
	lib::SharedPtr<rdr::Buffer> nonOwningThis = lib::SharedPtr<rdr::Buffer>(lib::SharedPtr<rdr::Buffer>(), this);
	m_fullView.Construct(lib::Ref(nonOwningThis), 0, GetRHI().GetSize(), std::move(externalDescriptorsAllocation));
}

} // spt::rdr
