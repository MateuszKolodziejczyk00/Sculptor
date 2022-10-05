#include "PipelinePendingState.h"
#include "Renderer.h"
#include "DescriptorSets/DescriptorSetsManager.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/DescriptorSetState.h"
#include "ShaderMetaData.h"
#include "CommandQueue/CommandQueue.h"
#include "Types/CommandBuffer.h"

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

	lib::DynamicArray<DSBindCommandData> descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundGfxPipeline), m_dirtyGfxDescriptorSets);

	if (!descriptorSetsToBind.empty())
	{
		cmdQueue.Enqueue([pendingDescriptors = std::move(descriptorSetsToBind), pipeline = m_boundGfxPipeline]
						 (const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 for (const DSBindCommandData& bindData : pendingDescriptors)
							 {
								 cmdBuffer->GetRHI().BindGfxDescriptorSet(pipeline->GetRHI(), bindData.ds, bindData.idx, bindData.dynamicOffsets);
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

	lib::DynamicArray<DSBindCommandData> descriptorSetsToBind = FlushPendingDescriptorSets(lib::Ref(m_boundComputePipeline), m_dirtyComputeDescriptorSets);

	if (!descriptorSetsToBind.empty())
	{
		cmdQueue.Enqueue([pendingDescriptors = std::move(descriptorSetsToBind), pipeline = m_boundComputePipeline]
						 (const lib::SharedRef<CommandBuffer>& cmdBuffer, const CommandExecuteContext& executionContext)
						 {
							 for (const DSBindCommandData& bindData : pendingDescriptors)
							 {
								 cmdBuffer->GetRHI().BindComputeDescriptorSet(pipeline->GetRHI(), bindData.ds, bindData.idx, bindData.dynamicOffsets);
							 }
						 });
	}
}

void PipelinePendingState::BindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	m_boundDescriptorSetStates.emplace_back(state);

	TryMarkAsDirty(state);
}

void PipelinePendingState::UnbindDescriptorSetState(const lib::SharedRef<DescriptorSetState>& state)
{
	SPT_PROFILER_FUNCTION();

	const auto foundDescriptor = std::find(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates), state.ToSharedPtr());
	SPT_CHECK(foundDescriptor != std::cend(m_boundDescriptorSetStates));
	m_boundDescriptorSetStates.erase(foundDescriptor);

	TryMarkAsDirty(state);
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

lib::DynamicArray<PipelinePendingState::DSBindCommandData> PipelinePendingState::FlushPendingDescriptorSets(const lib::SharedRef<Pipeline>& pipeline, lib::DynamicArray<Bool>& dirtyDescriptorSets)
{
	SPT_PROFILER_FUNCTION();

	DescriptorSetsManager& dsManager = Renderer::GetDescriptorSetsManager();
	const lib::SharedRef<smd::ShaderMetaData> metaData = pipeline->GetMetaData();

	lib::DynamicArray<DSBindCommandData> descriptorSetsToBind;
	descriptorSetsToBind.reserve(dirtyDescriptorSets.size());

	for (SizeType dsIdx = 0; dsIdx < dirtyDescriptorSets.size(); ++dsIdx)
	{
		if (dirtyDescriptorSets[dsIdx])
		{
			const SizeType dsHash = metaData->GetDescriptorSetHash(static_cast<Uint32>(dsIdx));
			const lib::SharedPtr<DescriptorSetState> state = GetBoundDescriptorSetState(dsHash);
			SPT_CHECK(!!state);

			const rhi::RHIDescriptorSet descriptorSet = dsManager.GetDescriptorSet(pipeline, lib::Ref(state), static_cast<Uint32>(dsIdx));
			descriptorSetsToBind.emplace_back(DSBindCommandData(static_cast<Uint32>(dsIdx), descriptorSet, state->GetDynamicOffsets()));
			
			// this check must be after getting descriptor set, as this call may clear dirty flag if that's first descriptor created using given state
			SPT_CHECK(!state->IsDirty());
		}
	}

	// Clear all dirty flags
	std::fill(std::begin(dirtyDescriptorSets), std::end(dirtyDescriptorSets), false);

	return descriptorSetsToBind;
}

lib::SharedPtr<DescriptorSetState> PipelinePendingState::GetBoundDescriptorSetState(SizeType hash) const
{
	SPT_PROFILER_FUNCTION();

	const auto foundState = std::find_if(std::cbegin(m_boundDescriptorSetStates), std::cend(m_boundDescriptorSetStates),
										 [hash](const lib::SharedPtr<DescriptorSetState>& state)
										 {
											 return state->GetDescriptorSetHash() == hash;
										 });

	return foundState != std::cend(m_boundDescriptorSetStates) ? *foundState : lib::SharedPtr<DescriptorSetState>();
}

} // spt::rdr
