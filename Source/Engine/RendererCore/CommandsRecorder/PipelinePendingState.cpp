#include "PipelinePendingState.h"
#include "Types/Pipeline/GraphicsPipeline.h"
#include "Types/Pipeline/ComputePipeline.h"
#include "Types/DescriptorSetState.h"
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

	if (prevPipeline == pipeline.ToSharedPtr())
	{
		return;
	}

	const lib::SharedPtr<GraphicsPipeline> prevPipeline = pipeline;
		
	m_boundGraphicsPipeline = pipeline.ToSharedPtr();
		
	const lib::SharedRef<smd::ShaderMetaData> newMetaData = pipeline->GetMetaData();
	m_dirtyDescriptorSets.resize(static_cast<SizeType>(newMetaData->GetDescriptorSetsNum()), true);

	if (prevPipeline)
	{
		const lib::SharedRef<smd::ShaderMetaData> prevMetaData = prevPipeline->GetMetaData();

		const Uint32 commonDescriptorSetsNum = std::min(prevMetaData->GetDescriptorSetsNum(), newMetaData->GetDescriptorSetsNum());
		for (Uint32 dsIdx = 0; dsIdx < commonDescriptorSetsNum; ++dsIdx)
		{
			if (prevMetaData->GetDescriptorSetHash(dsIdx) != newMetaData->GetDescriptorSetHash(dsIdx))
			{
				m_dirtyDescriptorSets[static_cast<SizeType>(dsIdx)] = true;
			}
		}
	}
}

void PipelinePendingState::UnbindGraphicsPipeline()
{
	m_boundGraphicsPipeline.reset();
}

const lib::SharedPtr<GraphicsPipeline>& PipelinePendingState::GetBoundGraphicsPipeline() const
{
	return m_boundGraphicsPipeline;
}

void PipelinePendingState::FlushDirtyDSForGraphicsPipeline(CommandQueue& cmdQueue)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!m_boundGraphicsPipeline);

	/*
	DescriptorSetsManager& dsManager = Renderer::GetDescriptorSetsManager();
	const lib::SharedRef<smd::ShaderMetaData> metaData = m_boundGraphicsPipeline->GetMetaData();

	for (SizeType dsIdx = 0; dsIdx < m_dirtyDescriptorSets.size(); ++dsIdx)
	{
		if (m_boundDescriptorSetStates[dsIdx])
		{
			const SizeType dsHash = metaData->GetDescriptorSetHash(static_cast<Uint32>(dsIdx));
			const lib::SharedPtr<DescriptorSetState> state = GetBoundDescriptorSetState(dsHash);
			SPT_CHECK(!!state);

			const rhi::DescriptorSet descriptorSet = dsManager.GetDescriptorSet(m_boundGraphicsPipeline, State, static_cast<Uint32>(dsIdx));
		}
	}
	*/
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

	if (m_boundGraphicsPipeline)
	{
		const Uint32 boundIdx = m_boundGraphicsPipeline->GetMetaData()->FindDescriptorSetWithHash(state->GetDescriptorSetHash());
		if (boundIdx != idxNone<Uint32>)
		{
			m_dirtyDescriptorSets[static_cast<SizeType>(boundIdx)] = true;
		}
	}
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
