#include "GPUDebug.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/Buffer.h"
#include "ResourcesManager.h"


SPT_DEFINE_LOG_CATEGORY(GPUDebug, true);

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DebugRegion ===================================================================================

DebugRegion::DebugRegion(CommandRecorder& recorder, const lib::HashedString& regionName, const lib::Color& color)
	: m_cachedRecorder(recorder)
{
	m_cachedRecorder.BeginDebugRegion(regionName, color);
}

DebugRegion::~DebugRegion()
{
	m_cachedRecorder.EndDebugRegion();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUCheckpointValidator ========================================================================

GPUCheckpointValidator::GPUCheckpointValidator()
	: m_checkpointsBuffer(rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("GPUCheckpointsBuffer"), rhi::BufferDefinition(sizeof(Uint32), lib::Flags(rhi::EBufferUsage::TransferSrc, rhi::EBufferUsage::TransferDst, rhi::EBufferUsage::Storage)), rhi::EMemoryUsage::GPUToCpu))
{
	const rhi::RHIMappedBuffer<Uint32> mappedBuffer(m_checkpointsBuffer->GetRHI());
	mappedBuffer[0] = 0u;
}

void GPUCheckpointValidator::AddCheckpoint(CommandRecorder& recorder, const lib::HashedString& marker)
{
	const auto pipelineFlush = [this, &recorder]()
	{
		rhi::RHIDependency dependency;
		const SizeType depIdx = dependency.AddBufferDependency(m_checkpointsBuffer->GetRHI(), 0, 4);
		dependency.SetBufferDependencyStages(depIdx, rhi::EPipelineStage::ALL_COMMANDS, rhi::EAccessType::Write, rhi::EPipelineStage::ALL_COMMANDS, rhi::EAccessType::Write);
		recorder.ExecuteBarrier(dependency);
	};

	pipelineFlush();

	m_checkpointNames.emplace_back(marker);
	recorder.FillBuffer(m_checkpointsBuffer, 0u, sizeof(Uint32), static_cast<Uint32>(m_checkpointNames.size()));

	pipelineFlush();
}

void GPUCheckpointValidator::ValidateExecution()
{
	const rhi::RHIMappedBuffer<Uint32> mappedBuffer(m_checkpointsBuffer->GetRHI());
	const Uint32 reachedCheckpointsNum = mappedBuffer[0];

	if (reachedCheckpointsNum != m_checkpointNames.size())
	{
		SPT_CHECK(reachedCheckpointsNum < m_checkpointNames.size());

		SPT_LOG_ERROR(GPUDebug, "======================================================================");
		SPT_LOG_ERROR(GPUDebug, "======================================================================");
		SPT_LOG_ERROR(GPUDebug, "GPU did not reach all checkpoints! Reached {0} out of {1} checkpoints.", reachedCheckpointsNum, m_checkpointNames.size());
		SPT_LOG_ERROR(GPUDebug, "Last reached checkpoint: {0}", reachedCheckpointsNum != 0u ? m_checkpointNames[reachedCheckpointsNum - 1].ToString() : "None");
		SPT_LOG_ERROR(GPUDebug, "First not reached checkpoint: {0}", m_checkpointNames[reachedCheckpointsNum].ToString());
		SPT_LOG_ERROR(GPUDebug, "======================================================================");
		SPT_LOG_ERROR(GPUDebug, "======================================================================");
	}
}

} // spt::rdr
