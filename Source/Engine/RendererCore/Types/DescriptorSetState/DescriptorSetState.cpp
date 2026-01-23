#include "DescriptorSetState.h"
#include "Renderer.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetLayout.h"
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
	, m_descriptorsAllocator(params.descriptorsAllocator)
	, m_constantsAllocator(params.constantsAllocator)
	, m_name(name)
{ }

DescriptorSetState::~DescriptorSetState()
{
	SwapDescriptorRange(rhi::RHIDescriptorRange{});
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

void DescriptorSetState::Flush()
{
	if (m_dirty)
	{
		rhi::RHIDescriptorRange descriptorRange = AllocateDescriptorRange();

		DescriptorSetIndexer indexer(descriptorRange.data, *m_layout);

		UpdateDescriptors(indexer);

		SwapDescriptorRange(descriptorRange);

		m_dirty = false;
	}
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

const lib::HashedString& DescriptorSetState::GetName() const
{
	return m_name.Get();
}

void DescriptorSetState::SetTypeID(DSStateTypeID id, const DescriptorSetStateParams& params)
{
	m_typeID = id;

	m_layout = DescriptorSetStateLayoutsRegistry::Get().GetLayoutChecked(m_typeID);
}

rhi::RHIDescriptorRange DescriptorSetState::AllocateDescriptorRange() const
{
	const Uint64 size = m_layout->GetRHI().GetDescriptorsDataSize();

	if (m_descriptorsAllocator)
	{
		return m_descriptorsAllocator->AllocateRange(static_cast<Uint32>(size));
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
		if (!m_descriptorsAllocator)
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

} // spt::rdr
