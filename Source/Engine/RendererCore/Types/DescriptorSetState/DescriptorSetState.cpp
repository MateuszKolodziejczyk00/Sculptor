#include "DescriptorSetState.h"
#include "Renderer.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetLayout.h"
#include "DescriptorSetStackAllocator.h"
#include "Types/DescriptorSetWriter.h"
#include "Types/DescriptorHeap.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace utils
{

static DSStateID GenerateStateID()
{
	static DSStateID current = 1;
	return current++;
}

} // utils

  //////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetState ============================================================================

DescriptorSetBinding::DescriptorSetBinding(const lib::HashedString& name)
	: m_name(name)
	, m_owningState(nullptr)
	, m_baseBindingIdx(idxNone<Uint32>)
{ }

const lib::HashedString& DescriptorSetBinding::GetName() const
{
	return m_name;
}

void DescriptorSetBinding::PreInit(DescriptorSetState& state, Uint32 baseBindingIdx)
{
	SPT_CHECK(m_owningState == nullptr);
	m_owningState    = &state;
	m_baseBindingIdx = baseBindingIdx;
}

void DescriptorSetBinding::MarkAsDirty()
{
	SPT_CHECK(!!m_owningState);
	m_owningState->SetDirty();
}

Uint32 DescriptorSetBinding::GetBaseBindingIdx() const
{
	return m_baseBindingIdx;
}

lib::HashedString DescriptorSetBinding::GetDescriptorSetName() const
{
	return m_owningState ? m_owningState->GetName() : lib::HashedString{};
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DescriptorSetState ============================================================================

DescriptorSetState::DescriptorSetState(const RendererResourceName& name, const DescriptorSetStateParams& params)
	: m_id(utils::GenerateStateID())
	, m_typeID(DSStateTypeID(idxNone<SizeType>))
	, m_dirty(true)
	, m_flags(params.flags)
	, m_stackAllocator(params.stackAllocator)
	, m_name(name)
{ }

DescriptorSetState::~DescriptorSetState()
{
#if SPT_USE_DESCRIPTOR_BUFFERS
	SwapDescriptorRange(rhi::RHIDescriptorRange{});
#else
	SwapDescriptorSet(rhi::RHIDescriptorSet{});
#endif // SPT_USE_DESCRIPTOR_BUFFERS
}

DSStateID DescriptorSetState::GetID() const
{
	return m_id;
}

DSStateTypeID DescriptorSetState::GetTypeID() const
{
	return m_typeID;
}

Bool DescriptorSetState::IsDirty() const
{
	return m_dirty;
}

void DescriptorSetState::SetDirty()
{
	m_dirty = true;
}

const rhi::RHIDescriptorSet& DescriptorSetState::Flush()
{
	if (m_dirty)
	{
#if SPT_USE_DESCRIPTOR_BUFFERS
		rhi::RHIDescriptorRange descriptorRange = AllocateDescriptorRange();

		DescriptorSetIndexer indexer(descriptorRange.data, *m_layout);

		UpdateDescriptors(indexer);

		SwapDescriptorRange(descriptorRange);
#else
		const rhi::RHIDescriptorSet newDS = AllocateDescriptorSet();
		DescriptorSetWriter writer;
		DescriptorSetUpdateContext context(newDS, writer, *m_layout);
		UpdateDescriptors(context);
		writer.Flush();

		SwapDescriptorSet(newDS);
#endif // SPT_USE_DESCRIPTOR_BUFFERS

		m_dirty = false;
	}

	return GetDescriptorSet();
}

const rhi::RHIDescriptorSet& DescriptorSetState::GetDescriptorSet() const
{
	return m_descriptorSet;
}

Uint32 DescriptorSetState::GetDescriptorsHeapOffset() const
{
	SPT_CHECK(m_descriptorRange.IsValid());
	return m_descriptorRange.heapOffset;
}

EDescriptorSetStateFlags DescriptorSetState::GetFlags() const
{
	return m_flags;
}

const lib::SharedPtr<DescriptorSetLayout>& DescriptorSetState::GetLayout() const
{
	return m_layout;
}

Uint32* DescriptorSetState::AddDynamicOffset()
{
	const Uint32* prevDataPtr = m_dynamicOffsets.data();

	Uint32& newOffset = m_dynamicOffsets.emplace_back(0);

	// make sure that array wasn't deallocated as this would result in dangling pointers in bindings to their offsets
	SPT_CHECK(prevDataPtr == m_dynamicOffsets.data());

	return &newOffset;
}

const lib::DynamicArray<Uint32>& DescriptorSetState::GetDynamicOffsets() const
{
	return m_dynamicOffsets;
}

const lib::HashedString& DescriptorSetState::GetName() const
{
	return m_name.Get();
}

void DescriptorSetState::SetTypeID(DSStateTypeID id, const DescriptorSetStateParams& params)
{
	m_typeID = id;

	m_layout = DescriptorSetStateLayoutsRegistry::Get().GetLayoutChecked(m_typeID);
}

void DescriptorSetState::InitDynamicOffsetsArray(SizeType dynamicOffsetsNum)
{

	m_dynamicOffsets.reserve(dynamicOffsetsNum);
}

#if SPT_USE_DESCRIPTOR_BUFFERS
rhi::RHIDescriptorRange DescriptorSetState::AllocateDescriptorRange() const
{
	const Uint64 size = m_layout->GetRHI().GetDescriptorsDataSize();

	if (m_stackAllocator)
	{
		return m_stackAllocator->AllocateRange(static_cast<Uint32>(size));
	}
	else
	{
		DescriptorHeap& descriptorHeap = Renderer::GetDescriptorHeap();
		return descriptorHeap.GetRHI().AllocateRange(m_layout->GetRHI().GetDescriptorsDataSize());
	}
}

void DescriptorSetState::SwapDescriptorRange(rhi::RHIDescriptorRange newDescriptorRange)
{
	if (m_descriptorRange.IsValid())
	{
		if (!m_stackAllocator)
		{
			Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry::CreateLambda(
				[ds = m_descriptorRange]
				{
					rdr::DescriptorHeap& descriptorHeap = Renderer::GetDescriptorHeap();
					descriptorHeap.GetRHI().DeallocateRange(ds);
				}));
		}
	}

	m_descriptorRange = newDescriptorRange;
}
#else
rhi::RHIDescriptorSet DescriptorSetState::AllocateDescriptorSet() const
{
	rhi::RHIDescriptorSet descriptorSet;

	if (m_stackAllocator)
	{
		descriptorSet = m_stackAllocator->GetRHI().AllocateDescriptorSet(m_layout->GetRHI());
	}
	else
	{
		descriptorSet = rhi::RHI::AllocateDescriptorSet(m_layout->GetRHI());
	}

	descriptorSet.SetName(GetName());

	return descriptorSet;
}

void DescriptorSetState::SwapDescriptorSet(rhi::RHIDescriptorSet newDescriptorSet)
{
	if (m_descriptorSet.IsValid())
	{
		m_descriptorSet.ResetName();

		// We need to free descriptor set if it was allocated from global pool
		if (!m_stackAllocator)
		{
			Renderer::ReleaseDeferred(GPUReleaseQueue::ReleaseEntry::CreateLambda(
				[ds = m_descriptorSet]
				{
					rhi::RHI::FreeDescriptorSet(ds);
				}));
		}
	}

	m_descriptorSet = newDescriptorSet;
}
#endif // SPT_USE_DESCRIPTOR_BUFFERS
} // spt::rdr
