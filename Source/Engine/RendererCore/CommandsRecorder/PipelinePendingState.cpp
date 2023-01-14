#include "PipelinePendingState.h"
#include "Renderer.h"
#include "DescriptorSets/DescriptorSetsManager.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/DescriptorSetState/DescriptorSetState.h"
#include "Types/CommandBuffer.h"
#include "Types/RenderContext.h"
#include "CommandQueue/CommandQueue.h"
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
	SPT_PROFILER_FUNCTION();

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

void PipelinePendingState::EnqueueFlushDirtyDSForGraphicsPipeline(CommandQueue& cmdQueue)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_boundGfxPipeline);

	DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundGfxPipeline), m_dirtyGfxDescriptorSets);

	if (!descriptorSetsToBind.persistentDSBinds.empty() || !descriptorSetsToBind.dynamicDSBinds.empty())
	{
		cmdQueue.Enqueue([pendingDescriptors = std::move(descriptorSetsToBind), pipeline = m_boundGfxPipeline]
						 (const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 for (const PersistentDSBindCommand& bindCommand : pendingDescriptors.persistentDSBinds)
							 {
								 cmdBuffer->GetRHI().BindGfxDescriptorSet(pipeline->GetRHI(), bindCommand.ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
							 }
							
							 RenderContext& renderContext = executionContext.GetRenderContext();
							 for (const DynamicDSBindCommand& bindCommand : pendingDescriptors.dynamicDSBinds)
							 {
								 const rhi::RHIDescriptorSet ds = renderContext.GetDescriptorSet(bindCommand.dsStateID);
								 cmdBuffer->GetRHI().BindGfxDescriptorSet(pipeline->GetRHI(), ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
							 }
						 });
	}
}

void PipelinePendingState::BindComputePipeline(const lib::SharedRef<ComputePipeline>& pipeline)
{
	SPT_PROFILER_FUNCTION();

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

void PipelinePendingState::EnqueueFlushDirtyDSForComputePipeline(CommandQueue& cmdQueue)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_boundComputePipeline);

	DSBindCommands descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundComputePipeline), m_dirtyComputeDescriptorSets);

	if (!descriptorSetsToBind.persistentDSBinds.empty() || !descriptorSetsToBind.dynamicDSBinds.empty())
	{
		cmdQueue.Enqueue([pendingDescriptors = std::move(descriptorSetsToBind), pipeline = m_boundComputePipeline]
						 (const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 for (const PersistentDSBindCommand& bindCommand : pendingDescriptors.persistentDSBinds)
							 {
								 cmdBuffer->GetRHI().BindComputeDescriptorSet(pipeline->GetRHI(), bindCommand.ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
							 }
							
							 RenderContext& renderContext = executionContext.GetRenderContext();
							 for (const DynamicDSBindCommand& bindCommand : pendingDescriptors.dynamicDSBinds)
							 {
								 const rhi::RHIDescriptorSet ds = renderContext.GetDescriptorSet(bindCommand.dsStateID);
								 cmdBuffer->GetRHI().BindComputeDescriptorSet(pipeline->GetRHI(), ds, bindCommand.idx, bindCommand.dynamicOffsets.data(), static_cast<Uint32>(bindCommand.dynamicOffsets.size()));
							 }
						 });
	}
}

void PipelinePendingState::BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_boundDescriptorSetStates.emplace_back(BoundDescriptorSetState{ state, DynamicOffsetsArray(state->GetDynamicOffsets()) });

	TryMarkAsDirty(state);
}

void PipelinePendingState::UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	const auto foundDescriptor = std::find_if(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates),
											  [statePtr = state.ToSharedPtr()](const BoundDescriptorSetState& boundState)
											  {
												  return boundState.instance == statePtr;
											  });

	SPT_CHECK(foundDescriptor != std::cend(m_boundDescriptorSetStates));
	m_boundDescriptorSetStates.erase(foundDescriptor);

	TryMarkAsDirty(state);
}

void PipelinePendingState::PrepareForExecution(const lib::SharedRef<RenderContext>& renderContext)
{
	SPT_PROFILER_FUNCTION();
	
	if (!m_dynamicDescriptorSetInfos.empty())
	{
		renderContext->BuildDescriptorSets(m_dynamicDescriptorSetInfos);

		m_dynamicDescriptorSetInfos.clear();
		m_cachedDynamicDescriptorSets.clear();
	}
}

void PipelinePendingState::TryMarkAsDirty(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	TryMarkAsDirtyImpl(state, m_boundGfxPipeline, m_dirtyGfxDescriptorSets);
	TryMarkAsDirtyImpl(state, m_boundComputePipeline, m_dirtyComputeDescriptorSets);
}

void PipelinePendingState::TryMarkAsDirtyImpl(const lib::SharedRef<DescriptorSetState>& state, const lib::SharedPtr<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	if (pipeline)
	{
		const Uint32 boundIdx = pipeline->GetMetaData()->FindDescriptorSetWithHash(state->GetDescriptorSetHash());
		if (boundIdx != idxNone<Uint32>)
		{
			dirtyDescriptorSets[static_cast<SizeType>(boundIdx)] = true;
		}
	}
}

void PipelinePendingState::UpdateDescriptorSetsOnPipelineChange(const lib::SharedPtr<Pipeline>& prevPipeline, const lib::SharedRef<Pipeline>& newPipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	const lib::SharedRef<smd::ShaderMetaData> newMetaData = newPipeline->GetMetaData();
	dirtyDescriptorSets.resize(static_cast<SizeType>(newMetaData->GetDescriptorSetsNum()), true);

	if (prevPipeline)
	{
		const lib::SharedRef<smd::ShaderMetaData> prevMetaData = prevPipeline->GetMetaData();

		const Uint32 commonDescriptorSetsNum = std::min(prevMetaData->GetDescriptorSetsNum(), newMetaData->GetDescriptorSetsNum());
		for (Uint32 dsIdx = 0; dsIdx < commonDescriptorSetsNum; ++dsIdx)
		{
			if (prevMetaData->GetDescriptorSetHash(dsIdx) != newMetaData->GetDescriptorSetHash(dsIdx))
			{
				dirtyDescriptorSets[static_cast<SizeType>(dsIdx)] = true;
			}
		}
	}
}

PipelinePendingState::DSBindCommands PipelinePendingState::FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	DescriptorSetsManager& dsManager = Renderer::GetDescriptorSetsManager();
	const lib::SharedRef<smd::ShaderMetaData> metaData = pipeline->GetMetaData();

	DSBindCommands descriptorSetsToBind;

	for (SizeType dsIdx = 0; dsIdx < dirtyDescriptorSets.size(); ++dsIdx)
	{
		if (dirtyDescriptorSets[dsIdx])
		{
			const SizeType dsHash = metaData->GetDescriptorSetHash(static_cast<Uint32>(dsIdx));
			const BoundDescriptorSetState* foundState = GetBoundDescriptorSetState(dsHash);
			SPT_CHECK_MSG(!!foundState, "Cannot find descriptor state for pipeline {0} at descriptor set idx: {1}", pipeline->GetRHI().GetName().GetData(), dsIdx);
			
			const lib::SharedRef<DescriptorSetState> stateInstance = lib::Ref(foundState->instance);

			if (lib::HasAnyFlag(stateInstance->GetFlags(), EDescriptorSetStateFlags::Persistent))
			{
				const rhi::RHIDescriptorSet descriptorSet = dsManager.GetOrCreateDescriptorSet(pipeline, static_cast<Uint32>(dsIdx), stateInstance);
				descriptorSetsToBind.persistentDSBinds.emplace_back(PersistentDSBindCommand(static_cast<Uint32>(dsIdx), descriptorSet, foundState->dynamicOffsets));

				// this check must be after getting descriptor set, as this call may clear dirty flag if that's first descriptor created using given state
				SPT_CHECK(!stateInstance->IsDirty());
			}
			else
			{
				const DSStateID stateID = stateInstance->GetID();
				if (!m_cachedDynamicDescriptorSets.contains(stateID))
				{
					m_dynamicDescriptorSetInfos.emplace_back(DynamicDescriptorSetInfo(pipeline, static_cast<Uint32>(dsIdx), stateInstance));
					m_cachedDynamicDescriptorSets.emplace(stateID);
				}
				descriptorSetsToBind.dynamicDSBinds.emplace_back(DynamicDSBindCommand(static_cast<Uint32>(dsIdx), stateID, foundState->dynamicOffsets));
			}
		}
	}

	// Clear all dirty flags
	std::fill(std::begin(dirtyDescriptorSets), std::end(dirtyDescriptorSets), false);

	return descriptorSetsToBind;
}

const PipelinePendingState::BoundDescriptorSetState* PipelinePendingState::GetBoundDescriptorSetState(SizeType hash) const
{
	SPT_PROFILER_FUNCTION();

	const auto foundState = std::find_if(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates),
										 [hash](const BoundDescriptorSetState& state)
										 {
											 return state.instance->GetDescriptorSetHash() == hash;
										 });

	return foundState != std::cend(m_boundDescriptorSetStates) ? &(*foundState) : nullptr;
}

} // spt::rdr
