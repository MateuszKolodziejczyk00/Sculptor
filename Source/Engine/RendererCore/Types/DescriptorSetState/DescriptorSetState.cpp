#include "DescriptorSetState.h"
#include "Renderer.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetLayout.h"
#include "DescriptorSetStackAllocator.h"
#include "Types/DescriptorSetWriter.h"

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
	SwapDescriptorSet(rhi::RHIDescriptorSet{});
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
;
}

void DescriptorSetState::SetDirty()
{
	m_dirty = true;
}

const rhi::RHIDescriptorSet& DescriptorSetState::Flush()
{
	if (m_dirty)
	{
		const rhi::RHIDescriptorSet newDS = AllocateDescriptorSet();
		DescriptorSetWriter writer;
		DescriptorSetUpdateContext context(newDS, writer, *m_layout);
		UpdateDescriptors(context);
		writer.Flush();

		SwapDescriptorSet(newDS);

		m_dirty = false;
	}

	return GetDescriptorSet();
}

const rhi::RHIDescriptorSet& DescriptorSetState::GetDescriptorSet() const
{
	return m_descriptorSet;
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

rhi::RHIDescriptorSet DescriptorSetState::AllocateDescriptorSet() const
{
	SPT_PROFILER_FUNCTION();

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
			CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
				[ds = m_descriptorSet]
				{
					rhi::RHI::FreeDescriptorSet(ds);
				});
		}
	}

	m_descriptorSet = newDescriptorSet;
}

} // spt::rdr
