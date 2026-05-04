#include "PipelinePendingState.h"
#include "GPUApi.h"
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

SPT_DEFINE_LOG_CATEGORY(PipelinePendingState, false);

PipelinePendingState::PipelinePendingState()
	: m_bindlessDescriptorsHeapOffset(GPUApi::GetDescriptorManager().GetHeapOffset())
{
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
	SPT_CHECK(state.IsValid());

	state->Flush();
	m_boundDescriptorSetStates.EmplaceBack(BoundDescriptorSetState{ state->GetTypeID(), state->GetDescriptorsHeapOffset() });

	SPT_LOG_INFO(PipelinePendingState, "Bound descriptor set state: {} with heap offset: {} for pipeline pending state", state->GetName().GetData(), state->GetDescriptorsHeapOffset());

	TryMarkAsDirty(state);
}

void PipelinePendingState::UnbindDescriptorSetState(const lib::MTHandle<DescriptorSetState>& state)
{
	TryMarkAsDirty(state);

	Bool found = false;

	for (SizeType i = m_boundDescriptorSetStates.size(); i > 0; --i)
	{
		if (m_boundDescriptorSetStates[i - 1].typeID == state->GetTypeID() && m_boundDescriptorSetStates[i - 1].heapOffset == state->GetDescriptorsHeapOffset())
		{
			m_boundDescriptorSetStates.RemoveAt(i - 1);
			found = true;

			SPT_LOG_INFO(PipelinePendingState, "Unbound descriptor set state: {} with heap offset: {} from pipeline pending state", state->GetName().GetData(), state->GetDescriptorsHeapOffset());

			break;
		}
	}

	SPT_CHECK(found);
}

void PipelinePendingState::BindShaderParams(Uint32 heapOffset)
{
	m_pendingShaderParamsOffset = heapOffset;
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
	descriptorsState.dirtyDescriptorSets.Resize(static_cast<SizeType>(newMetaData.GetDescriptorSetsNum()), true);

	if (prevPipeline)
	{
		const smd::ShaderMetaData& prevMetaData = prevPipeline->GetMetaData();

		const Uint32 commonDescriptorSetsNum = std::min(prevMetaData.GetDescriptorSetsNum(), newMetaData.GetDescriptorSetsNum());
		for (Uint32 dsIdx = 0; dsIdx < commonDescriptorSetsNum; ++dsIdx)
		{
			if (prevMetaData.GetDescriptorSetStateTypeID(dsIdx) != newMetaData.GetDescriptorSetStateTypeID(dsIdx))
			{
				descriptorsState.dirtyDescriptorSets[static_cast<SizeType>(dsIdx)] = true;
			}
		}
	}
}

PipelinePendingState::DSBindCommands PipelinePendingState::FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, PipelineDescriptorsState& descriptorsState)
{
	const smd::ShaderMetaData& metaData = pipeline->GetMetaData();

	DSBindCommands descriptorSetsToBind;

	SPT_LOG_INFO(PipelinePendingState, "Flushing pending descriptor sets for pipeline: {}. Total sets to check: {}.", pipeline->GetRHI().GetName().GetData(), metaData.GetDescriptorSetsNum());

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

			descriptorSetsToBind.EmplaceBack(std::move(bindCommand));

			SPT_LOG_INFO(PipelinePendingState, "Marked descriptor set idx: {} with heap offset: {} as dirty for pipeline: {}.", dsIdx, foundState->heapOffset, pipeline->GetRHI().GetName().GetData());
		}
	}

	if (!descriptorsState.isBindlessBound)
	{
		DSBindCommand bindlessDescriptorsCommand;
		bindlessDescriptorsCommand.idx        = 0u;
		bindlessDescriptorsCommand.heapOffset = m_bindlessDescriptorsHeapOffset;

		descriptorSetsToBind.EmplaceBack(bindlessDescriptorsCommand);
	}

	if (metaData.GetShaderParamsType().IsValid())
	{
		SPT_CHECK_MSG(m_pendingShaderParamsOffset != idxNone<Uint32>, "Shader params not bound for pipeline {}! Expected: {}.", pipeline->GetRHI().GetName().GetData(), metaData.GetShaderParamsType().GetData());

		DSBindCommand shaderParamsDescriptorCommand;
		shaderParamsDescriptorCommand.idx        = metaData.GetDescriptorSetsNum();
		shaderParamsDescriptorCommand.heapOffset = m_pendingShaderParamsOffset;

		descriptorSetsToBind.EmplaceBack(shaderParamsDescriptorCommand);
	}

	// Clear all dirty flags
	std::fill(std::begin(descriptorsState.dirtyDescriptorSets), std::end(descriptorsState.dirtyDescriptorSets), false);
	descriptorsState.isBindlessBound = true;
	m_pendingShaderParamsOffset = idxNone<Uint32>;

	return descriptorSetsToBind;
}

const PipelinePendingState::BoundDescriptorSetState* PipelinePendingState::GetBoundDescriptorSetState(DSStateTypeID dsTypeID) const
{
	// Iterate backwards to get stack-like behavior in case of multiple bound states of the same type (e.g. when a descriptor set state is bound, then another one of the same type is bound, then the first one is unbound - we want to get the second one as the currently bound state)
	for (SizeType i = m_boundDescriptorSetStates.size(); i > 0; --i)
	{
		if (m_boundDescriptorSetStates[i - 1].typeID == dsTypeID)
		{
			return &m_boundDescriptorSetStates[i - 1];
		}
	}

	return nullptr;
}

} // spt::rdr
