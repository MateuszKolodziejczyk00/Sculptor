#include "PipelinePendingState.h"
#include "Renderer.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/Pipeline/RayTracingPipeline.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/CommandBuffer.h"
#include "Types/RenderContext.h"
#include "ShaderMetaData.h"

namespace spt::rdr
{

PipelinePendingState::PipelinePendingState()
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

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundGfxPipeline), m_dirtyGfxDescriptorSets);
}

void PipelinePendingState::UnbindGraphicsPipeline()
{
	m_boundGfxPipeline.reset();
	m_dirtyGfxDescriptorSets.clear();
}

const lib::SharedPtr<GraphicsPipeline>& PipelinePendingState::GetBoundGraphicsPipeline() const
{
	return m_boundGfxPipeline;
}

void PipelinePendingState::FlushDirtyDSForGraphicsPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundGfxPipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundGfxPipeline), m_dirtyGfxDescriptorSets);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindGfxDescriptorSet(m_boundGfxPipeline->GetRHI(), bindCommand.ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
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

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundComputePipeline), m_dirtyComputeDescriptorSets);
}

void PipelinePendingState::UnbindComputePipeline()
{
	m_boundComputePipeline.reset();
	m_dirtyComputeDescriptorSets.clear();
}

const lib::SharedPtr<ComputePipeline>& PipelinePendingState::GetBoundComputePipeline() const
{
	return m_boundComputePipeline;
}

void PipelinePendingState::FlushDirtyDSForComputePipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundComputePipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundComputePipeline), m_dirtyComputeDescriptorSets);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindComputeDescriptorSet(m_boundComputePipeline->GetRHI(), bindCommand.ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
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

	UpdateDescriptorSetsOnPipelineChange(prevPipeline, lib::Ref(m_boundRayTracingPipeline), m_dirtyRayTracingDescriptorSets);
}

void PipelinePendingState::UnbindRayTracingPipeline()
{
	m_boundRayTracingPipeline.reset();
	m_dirtyRayTracingDescriptorSets.clear();
}

const lib::SharedPtr<rdr::RayTracingPipeline>& PipelinePendingState::GetBoundRayTracingPipeline() const
{
	return m_boundRayTracingPipeline;
}

void PipelinePendingState::FlushDirtyDSForRayTracingPipeline(rhi::RHICommandBuffer& cmdBuffer)
{
	SPT_CHECK(!!m_boundRayTracingPipeline);

	const DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundRayTracingPipeline), m_dirtyRayTracingDescriptorSets);

	for (const DSBindCommand& bindCommand : descriptorSetsToBind)
	{
		cmdBuffer.BindRayTracingDescriptorSet(m_boundRayTracingPipeline->GetRHI(), bindCommand.ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
	}
}

void PipelinePendingState::BindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	m_boundDescriptorSetStates.emplace_back(BoundDescriptorSetState{ state->GetTypeID(), state->Flush(), DynamicOffsetsArray(state->GetDynamicOffsets()) });

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
	TryMarkAsDirtyImpl(state, m_boundGfxPipeline, m_dirtyGfxDescriptorSets);
	TryMarkAsDirtyImpl(state, m_boundComputePipeline, m_dirtyComputeDescriptorSets);
	TryMarkAsDirtyImpl(state, m_boundRayTracingPipeline, m_dirtyRayTracingDescriptorSets);
}

void PipelinePendingState::TryMarkAsDirtyImpl(const lib::MTHandle<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	if (pipeline)
	{
		const Uint32 boundIdx = pipeline->GetMetaData().FindDescriptorSetOfType(state->GetTypeID());
		if (boundIdx != idxNone<Uint32>)
		{
			dirtyDescriptorSets[static_cast<SizeType>(boundIdx)] = true;
		}
	}
}

void PipelinePendingState::UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	const smd::ShaderMetaData& newMetaData = newPipeline->GetMetaData();
	dirtyDescriptorSets.resize(static_cast<SizeType>(newMetaData.GetDescriptorSetsNum()), true);

	if (prevPipeline)
	{
		const smd::ShaderMetaData& prevMetaData = prevPipeline->GetMetaData();

		const Uint32 commonDescriptorSetsNum = std::min(prevMetaData.GetDescriptorSetsNum(), newMetaData.GetDescriptorSetsNum());
		for (Uint32 dsIdx = 0; dsIdx < commonDescriptorSetsNum; ++dsIdx)
		{
			if (prevMetaData.GetDescriptorSetStateTypeID(dsIdx) != newMetaData.GetDescriptorSetStateTypeID(dsIdx))
			{
				dirtyDescriptorSets[static_cast<SizeType>(dsIdx)] = true;
			}
		}
	}
}

PipelinePendingState::DSBindCommands PipelinePendingState::FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

	DSBindCommands descriptorSetsToBind;

	for (SizeType dsIdx = 0; dsIdx < dirtyDescriptorSets.size(); ++dsIdx)
	{
		if (dirtyDescriptorSets[dsIdx])
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
			bindCommand.idx            = static_cast<Uint32>(dsIdx);
			bindCommand.ds             = foundState->ds;
			bindCommand.dynamicOffsets = foundState->dynamicOffsets;

			descriptorSetsToBind.emplace_back(std::move(bindCommand));
		}
	}

	// Clear all dirty flags
	std::fill(std::begin(dirtyDescriptorSets), std::end(dirtyDescriptorSets), false);

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
