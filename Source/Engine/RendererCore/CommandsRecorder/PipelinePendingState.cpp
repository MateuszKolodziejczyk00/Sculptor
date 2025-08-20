#include "PipelinePendingState.h"
#include "Renderer.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Pipeline/RayTracingPipeline.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/CommandBuffer.h"
#include "Types/RenderContext.h"
#include "ShaderMetaData.h"
#include "Types/DescriptorSetState/DescriptorManager.h"

namespace spt::rdr
{

PipelinePendingState::PipelinePendingState()
	: m_bindlessDescriptorsHeapOffset(Renderer::GetDescriptorManager().GetHeapOffset())
{
	m_boundDescriptorSetStates.reserve(16);
}

PipelinePendingState::~PipelinePendingState() = default;

void PipelinePendingState::BindGraphicsPipeline(const lib::SharedRef<GraphicsPipeline>& pipeline)
{
	if (m_boundGfxPipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<GraphicsPipeline> prevPipeline = std::move(m_boundGfxPipeline);
		
	m_boundGfxPipeline = pipeline.ToSharedPtr();

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundGfxPipeline), m_gfxPipelineDescriptorsState);
}

void PipelinePendingState::UnbindGraphicsPipeline()
{
	m_boundGfxPipeline.reset();
	m_gfxPipelineDescriptorsState = PipelineDescriptorsState{};
}

const lib::SharedPtr<GraphicsPipeline>& PipelinePendingState::GetBoundGraphicsPipeline() const
{
	return m_boundGfxPipeline;
}

void PipelinePendingState::FlushDirtyDSForGraphicsPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundGfxPipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundGfxPipeline), m_gfxPipelineDescriptorsState);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindGfxDescriptors(m_boundGfxPipeline->GetRHI(), bindCommand.idx, bindCommand.heapOffset);
	}
}

void PipelinePendingState::BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline)
{
	if (m_boundComputePipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<ComputePipeline> prevPipeline = std::move(m_boundComputePipeline);
		
	m_boundComputePipeline = pipeline.ToSharedPtr();

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundComputePipeline), m_computePipelineDescriptorsState);
}

void PipelinePendingState::UnbindComputePipeline()
{
	m_boundComputePipeline.reset();
	m_computePipelineDescriptorsState = PipelineDescriptorsState{};
}

const lib::SharedPtr<ComputePipeline>& PipelinePendingState::GetBoundComputePipeline() const
{
	return m_boundComputePipeline;
}

void PipelinePendingState::FlushDirtyDSForComputePipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundComputePipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundComputePipeline), m_computePipelineDescriptorsState);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindComputeDescriptors(m_boundComputePipeline->GetRHI(), bindCommand.idx, bindCommand.heapOffset);
	}
}

void PipelinePendingState::BindRayTracingPipeline(const lib::SharedRef<RayTracingPipeline>& pipeline)
{
	if (m_boundRayTracingPipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<RayTracingPipeline> prevPipeline = std::move(m_boundRayTracingPipeline);
		
	m_boundRayTracingPipeline = pipeline.ToSharedPtr();

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundRayTracingPipeline), m_rayTracingPipelineDescriptorsState);
}

void PipelinePendingState::UnbindRayTracingPipeline()
{
	m_boundRayTracingPipeline.reset();
	m_rayTracingPipelineDescriptorsState = PipelineDescriptorsState{};
}

const lib::SharedPtr<rdr::RayTracingPipeline>& PipelinePendingState::GetBoundRayTracingPipeline() const
{
	return m_boundRayTracingPipeline;
}

void PipelinePendingState::FlushDirtyDSForRayTracingPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundRayTracingPipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundRayTracingPipeline), m_rayTracingPipelineDescriptorsState);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindRayTracingDescriptors(m_boundRayTracingPipeline->GetRHI(), bindCommand.idx, bindCommand.heapOffset);
	}
}

void PipelinePendingState::BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	state->Flush();
	m_boundDescriptorSetStates.emplace_back(BoundDescriptorSetState{ state->GetTypeID(), state->GetDescriptorsHeapOffset() });

	TryMarkAsDirty(state);
}

void PipelinePendingState::UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	const auto foundDescriptor = std::find_if(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates),
											  [statePtr = state](const BoundDescriptorSetState& boundState)
											  {
												  return boundState.typeID == statePtr->GetTypeID();
											  });

	SPT_CHECK(foundDescriptor != std::cend(m_boundDescriptorSetStates));
	m_boundDescriptorSetStates.erase(foundDescriptor);

	TryMarkAsDirty(state);
}

void PipelinePendingState::TryMarkAsDirty(const lib::MTHandle<DescriptorSetState>& state)
{
	TryMarkAsDirtyImpl(state, m_boundGfxPipeline, m_gfxPipelineDescriptorsState);
	TryMarkAsDirtyImpl(state, m_boundComputePipeline, m_computePipelineDescriptorsState);
	TryMarkAsDirtyImpl(state, m_boundRayTracingPipeline, m_rayTracingPipelineDescriptorsState);
}

void PipelinePendingState::TryMarkAsDirtyImpl(const lib::MTHandle<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, PipelineDescriptorsState& descriptorsState)
{
	if (pipeline)
	{
		const Uint32 boundIdx = pipeline->GetMetaData().FindDescriptorSetOfType(state->GetTypeID());
		if (boundIdx != idxNone<Uint32>)
		{
			descriptorsState.dirtyDescriptorSets[static_cast<SizeType>(boundIdx)] = true;
		}
	}
}

void PipelinePendingState::UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, PipelineDescriptorsState& descriptorsState)
{
	const smd::ShaderMetaData& newMetaData = newPipeline->GetMetaData();
	descriptorsState.dirtyDescriptorSets.resize(static_cast<SizeType>(newMetaData.GetDescriptorSetsNum()), true);

	Bool wasUsingBindless = false;

	if (prevPipeline)
	{
		const smd::ShaderMetaData& prevMetaData = prevPipeline->GetMetaData();

		wasUsingBindless = prevMetaData.IsBindless();

		const Uint32 commonDescriptorSetsNum = std::min(prevMetaData.GetDescriptorSetsNum(), newMetaData.GetDescriptorSetsNum());
		for (Uint32 dsIdx = 0; dsIdx < commonDescriptorSetsNum; ++dsIdx)
		{
			if (prevMetaData.GetDescriptorSetStateTypeID(dsIdx) != newMetaData.GetDescriptorSetStateTypeID(dsIdx))
			{
				descriptorsState.dirtyDescriptorSets[static_cast<SizeType>(dsIdx)] = true;
			}
		}
	}

	if (wasUsingBindless != newMetaData.IsBindless())
	{
		descriptorsState.isBindlessDirty = true;
	}
}

PipelinePendingState::DSBindCommands PipelinePendingState::FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, PipelineDescriptorsState& descriptorsState)
{
	const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

	DSBindCommands descriptorSetsToBind;

	for (SizeType dsIdx = 0; dsIdx < descriptorsState.dirtyDescriptorSets.size(); ++dsIdx)
	{
		if (descriptorsState.dirtyDescriptorSets[dsIdx])
		{
			const DSStateTypeID dsTypeID = DSStateTypeID(metaData.GetDescriptorSetStateTypeID(static_cast<Uint32>(dsIdx)));
			if (dsTypeID == idxNone<SizeType>)
			{
				// This is unused set idx
				continue;
			}

			const BoundDescriptorSetState* foundState = GetBoundDescriptorSetState(dsTypeID);
			SPT_CHECK_MSG(!!foundState, "Cannot find descriptor state for pipeline {0} at descriptor set idx: {1}\nTry removing Saved/Shaders/Cache directory", pipeline->GetRHI().GetName().GetData(), dsIdx);

			DSBindCommand bindCommand;
			bindCommand.idx        = static_cast<Uint32>(dsIdx);
			bindCommand.heapOffset = foundState->heapOffset;

			descriptorSetsToBind.emplace_back(std::move(bindCommand));
		}
	}

	if (descriptorsState.isBindlessDirty && metaData.IsBindless())
	{
		DSBindCommand bindlessDescriptorsCommand;
		bindlessDescriptorsCommand.idx        = 0u;
		bindlessDescriptorsCommand.heapOffset = m_bindlessDescriptorsHeapOffset;

		descriptorSetsToBind.emplace_back(bindlessDescriptorsCommand);
	}

	// Clear all dirty flags
	std::fill(std::begin(descriptorsState.dirtyDescriptorSets), std::end(descriptorsState.dirtyDescriptorSets), false);
	descriptorsState.isBindlessDirty = false;

	return descriptorSetsToBind;
}

const PipelinePendingState::BoundDescriptorSetState* PipelinePendingState::GetBoundDescriptorSetState(DSStateTypeID dsTypeID) const
{
	const auto foundState = std::find_if(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates),
										 [dsTypeID](const BoundDescriptorSetState& state)
										 {
											 return state.typeID == dsTypeID;
										 });

	return foundState != std::cend(m_boundDescriptorSetStates) ? &(*foundState) : nullptr;
}

} // spt::rdr
