#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHIBufferImpl.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RendererResource.h"
#include "DescriptorSetState/DescriptorTypes.h"
#include "Utility/Templates/TypeStorage.h"


namespace spt::rhi
{
struct RHIAllocationInfo;
}


namespace spt::rdr
{

class Buffer;
class GPUMemoryPool;
struct RendererResourceName;



struct BufferViewDescriptorsAllocation
{
	ResourceDescriptorHandle uavDescriptor;
};


class RENDERER_CORE_API BufferView
{
public:

	BufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size);

	Bool operator==(const BufferView& rhs) const;

	const lib::SharedRef<Buffer>& GetBuffer() const;

	Uint64 GetOffset() const;
	Uint64 GetSize() const;

private:

	lib::SharedRef<Buffer>	m_buffer;

	Uint64					m_offset;
	Uint64					m_size;
};


class RENDERER_CORE_API BindableBufferView : public BufferView, public lib::SharedFromThis<BindableBufferView>
{
public:

	BindableBufferView(const lib::SharedRef<Buffer>& buffer, Uint64 offset, Uint64 size, BufferViewDescriptorsAllocation externalDescriptorsAllocation = BufferViewDescriptorsAllocation{});
	~BindableBufferView();

	ResourceDescriptorIdx GetUAVDescriptor() const { return m_uavDescriptor.Get(); }

	lib::SharedPtr<BindableBufferView> AsSharedPtr();

private:

	void CreateDescriptors(BufferViewDescriptorsAllocation externalDescriptorsAllocation);

	ResourceDescriptorHandle m_uavDescriptor;
};


class RENDERER_CORE_API Buffer : public RendererResource<rhi::RHIBuffer>, public lib::SharedFromThis<Buffer>, public rhi::RHIBufferMemoryOwner
{
protected:

	using ResourceType = RendererResource<rhi::RHIBuffer>;

public:

	Buffer(const RendererResourceName& name, const rhi::BufferDefinition& definition, const AllocationDefinition& allocationDefinition, BufferViewDescriptorsAllocation externalDescriptorsAllocation = BufferViewDescriptorsAllocation{});
	Buffer(const RendererResourceName& name, const rhi::RHIBuffer& rhiBufferInstance);
	~Buffer();

	Bool HasBoundMemory() const;
	void BindMemory(const AllocationDefinition& definition);
	void ReleasePlacedAllocation();

	BufferView CreateFullView() const;
	lib::SharedPtr<BindableBufferView> GetFullView() const;
	const BindableBufferView&          GetFullViewRef() const;

	lib::SharedPtr<BindableBufferView> CreateView(Uint64 offset, Uint64 size, BufferViewDescriptorsAllocation externalDescriptorsAllocation = BufferViewDescriptorsAllocation()) const;

	Uint64 GetSize() const;

	template<typename TType>
	rhi::RHIMappedBuffer<TType> Map()
	{
		return rhi::RHIMappedBuffer<TType>(GetRHI());
	}

private:

	void InitFullView(BufferViewDescriptorsAllocation externalDescriptorsAllocation);

	lib::TypeStorage<BindableBufferView> m_fullView;
	lib::SharedPtr<GPUMemoryPool> m_owningMemoryPool;
};

} // spt::rdr
