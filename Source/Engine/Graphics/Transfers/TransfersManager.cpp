#include "TransfersManager.h"
#include "RendererUtils.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "GPUApi.h"
#include "ResourcesManager.h"
#include "DeviceQueues/GPUWorkload.h"

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

Uint64 TransfersManager::SubmitTransfers(const lib::SharedRef<rdr::RenderContext>& context, lib::UniquePtr<rdr::CommandRecorder> recorder)
{
	SPT_PROFILER_FUNCTION();

	TransfersManager& instance = Get();

	// This lock is currently needed to make sure that workloads are submitted in order
	// otherwise if they end up on same queue, we may end up with deadlock because of transfers semaphore
	const lib::LockGuard lock(instance.m_lock);

	const Uint64 semaphorePrevValue	= instance.m_lastSubmitCountValue.fetch_add(1);
	const Uint64 semaphoreNewValue	= semaphorePrevValue + 1;

	const lib::SharedRef<rdr::GPUWorkload> workload = recorder->FinishRecording();

	const lib::SharedRef<rdr::Semaphore> transfersSemaphoresRef = lib::Ref(instance.m_transferFinishedSemaphore);

	// We must wait for previous transfers
	// Without it signal value might have been smaller than current value when signaling
	workload->GetWaitSemaphores().AddTimelineSemaphore(transfersSemaphoresRef, semaphorePrevValue, rhi::EPipelineStage::ALL_TRANSFER);
	workload->GetSignalSemaphores().AddTimelineSemaphore(transfersSemaphoresRef, semaphoreNewValue, rhi::EPipelineStage::ALL_TRANSFER);
	
	rdr::GPUApi::GetDeviceQueuesManager().Submit(workload, rdr::EGPUWorkloadSubmitFlags::MemoryTransfers);

	return semaphoreNewValue;
}

TransfersManager::TransfersManager()
	: m_transferFinishedSemaphore(rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("TransferFinishedSemaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Timeline)))
	, m_lastSubmitCountValue(0)
{
	rdr::GPUApi::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_transferFinishedSemaphore.reset();
															});
}

} // spt::gfx
