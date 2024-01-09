#include "TransfersManager.h"
#include "RendererUtils.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Renderer.h"
#include "ResourcesManager.h"

namespace spt::gfx
{

TransfersManager& TransfersManager::Get()
{
	static TransfersManager instance;
	return instance;
}

void TransfersManager::WaitForTransfersFinished()
{
#if WITH_NSIGHT_CRASH_FIX
	rdr::Renderer::WaitIdle(false);
#else
	const TransfersManager& instance = Get();
	const Bool finished = instance.m_transferFinishedSemaphore->GetRHI().Wait(instance.m_lastSubmitCountValue.load());
	SPT_CHECK(finished);
#endif // !WITH_NSIGHT_CRASH_FIX
}

void TransfersManager::WaitForTransfersFinished(Uint64 transferSignalValue)
{
#if WITH_NSIGHT_CRASH_FIX
	rdr::Renderer::WaitIdle(false);
#else
	const TransfersManager& instance = Get();
	const Bool finished = instance.m_transferFinishedSemaphore->GetRHI().Wait(transferSignalValue);
	SPT_CHECK(finished);
#endif // WITH_NSIGHT_CRASH_FIX
}

void TransfersManager::WaitForTransfersFinished(rdr::SemaphoresArray& waitSemaphoresArray)
{
#if WITH_NSIGHT_CRASH_FIX
	rdr::Renderer::WaitIdle(false);
#else
	const TransfersManager& instance = Get();
	waitSemaphoresArray.AddTimelineSemaphore(lib::Ref(instance.m_transferFinishedSemaphore), instance.m_lastSubmitCountValue.load(), rhi::EPipelineStage::ALL_TRANSFER);
#endif // WITH_NSIGHT_CRASH_FIX
}

Uint64 TransfersManager::RecordAndSubmitTransfers(const lib::SharedRef<rdr::RenderContext>& context, lib::UniquePtr<rdr::CommandRecorder> recorder)
{
	SPT_PROFILER_FUNCTION();

	TransfersManager& instance = Get();

	const Uint64 semaphorePrevValue	= instance.m_lastSubmitCountValue.fetch_add(1);
	const Uint64 semaphoreNewValue	= semaphorePrevValue + 1;

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("TransfersCommandBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Transfer, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	recorder->RecordCommands(context, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back();
	submitBatch.recordedCommands.emplace_back(std::move(recorder));

	const lib::SharedRef<rdr::Semaphore> transfersSemaphoresRef = lib::Ref(instance.m_transferFinishedSemaphore);

	// We must wait for previous transfers
	// Without it signal value might have been smaller than current value when signaling
	submitBatch.waitSemaphores.AddTimelineSemaphore(transfersSemaphoresRef, semaphorePrevValue, rhi::EPipelineStage::ALL_TRANSFER);
	submitBatch.signalSemaphores.AddTimelineSemaphore(transfersSemaphoresRef, semaphoreNewValue, rhi::EPipelineStage::ALL_TRANSFER);

	submitBatch.waitSemaphores.AddTimelineSemaphore(rdr::Renderer::GetReleaseFrameSemaphore(), rdr::Renderer::GetCurrentFrameIdx() - 1, rhi::EPipelineStage::ALL_TRANSFER);

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Transfer, submitBatches);

	return semaphoreNewValue;
}

TransfersManager::TransfersManager()
	: m_transferFinishedSemaphore(rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("TransferFinishedSemaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Timeline)))
	, m_lastSubmitCountValue(0)
{
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_transferFinishedSemaphore.reset();
															});
}

} // spt::gfx
