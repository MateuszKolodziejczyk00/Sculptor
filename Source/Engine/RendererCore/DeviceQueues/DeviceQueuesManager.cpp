#include "DeviceQueuesManager.h"
#include "ResourcesManager.h"
#include "Types/Semaphore.h"
#include "Renderer.h"
#include "EngineFrame.h"
#include "GPUWorkload.h"
#include "Engine.h"
#include "JobSystem.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceQueue ===================================================================================

DeviceQueue::DeviceQueue()
	: m_lastSignaledSemaphoreValue(0)
{
}

void DeviceQueue::Initialize(rhi::RHIDeviceQueue queue)
{
	m_queue = queue;

	m_semaphore = rdr::ResourcesManager::CreateRenderSemaphore(rdr::RendererResourceName("DeviceQueueSemaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Timeline));
	m_lastSignaledSemaphoreValue = 0;
}

void DeviceQueue::Release()
{
	m_semaphore.reset();
	m_queue.ReleaseRHI();
	m_lastSignaledSemaphoreValue = 0;
}

void DeviceQueue::SubmitGPUWorkload(WorkloadSubmitDefinition&& definition)
{
	SPT_PROFILER_FUNCTION();

	{
		const lib::LockGuard lock(m_lock);

		rhi::RHISemaphoresArray waitSemaphores = std::move(definition.waitSemaphores.GetRHISemaphores());
		if (m_lastSignaledSemaphoreValue)
		{
			waitSemaphores.AddTimelineSemaphore(m_semaphore->GetRHI(), m_lastSignaledSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);
		}

		rhi::RHISemaphoresArray signalSemaphores = std::move(definition.signalSemaphores.GetRHISemaphores());
		signalSemaphores.AddTimelineSemaphore(m_semaphore->GetRHI(), ++m_lastSignaledSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);

		rhi::SubmitBatchData submitBatch;
		submitBatch.commandBuffers.emplace_back(&definition.workload->GetRecordedBuffer()->GetRHI());
		submitBatch.waitSemaphores   = &waitSemaphores;
		submitBatch.signalSemaphores = &signalSemaphores;

		m_queue.SubmitCommands(submitBatch);
	}

	definition.workload->OnSubmitted(m_semaphore, m_lastSignaledSemaphoreValue, this);
}

const rhi::RHICommandBuffer& DeviceQueue::GetCommandBuffer(const GPUWorkload& workload) const
{
	return workload.GetRecordedBuffer()->GetRHI();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SubmittedWorkloadsQueue =======================================================================

SubmittedWorkloadsQueue::SubmittedWorkloadsQueue()
	: m_flushThreadWakeSemaphore(0u)
{ }

void SubmittedWorkloadsQueue::Initialize()
{
	m_flushThreadRunning.store(true);
	m_flushThread = lib::Thread(&SubmittedWorkloadsQueue::FlushThreadMain, this);
}

void SubmittedWorkloadsQueue::Uninitialize()
{
	m_flushThreadRunning.store(false);

	m_flushThreadWakeSemaphore.release();

	m_flushThread.Join();

	SPT_CHECK(m_submittedWorkloadsQueue.empty());
	SPT_CHECK(GetSubmittedSection() == GetExecutedSection());
}

void SubmittedWorkloadsQueue::Push(lib::SharedRef<GPUWorkload> workload)
{
	SPT_PROFILER_FUNCTION();

	{
		const lib::LockGuard lock(m_submittedWorkloadsLock);

		m_submittedWorkloadsQueue.emplace_back(std::move(workload));
	}

	m_flushThreadWakeSemaphore.release();
}

void SubmittedWorkloadsQueue::FullFlush()
{
	const lib::LockGuard lock(m_submittedWorkloadsLock);

	while (!m_submittedWorkloadsQueue.empty())
	{
		lib::SharedRef<GPUWorkload> workload = std::move(m_submittedWorkloadsQueue.front());
		m_submittedWorkloadsQueue.pop_front();

		workload->Wait();

		js::Launch(SPT_GENERIC_JOB_NAME,
				   [finishedWorkload = std::move(workload)]()
				   {
					   finishedWorkload->OnExecutionFinished();
				   });
	}
}

void SubmittedWorkloadsQueue::SignalAfterFlushedPendingWork(const js::Event& event)
{
	const lib::LockGuard lock(m_submittedWorkloadsLock);

	SignalAfterFlushedPendingWork_Locked(event);
}

GPUTimelineSection SubmittedWorkloadsQueue::GetSubmittedSection() const
{
	return m_submissionSection;
}

GPUTimelineSection SubmittedWorkloadsQueue::GetExecutedSection() const
{
	return m_executionSection;
}

GPUTimelineSection SubmittedWorkloadsQueue::GetRecordedSection() const
{
	return GPUTimelineSection{ m_submissionSection.value + 1u };
}

void SubmittedWorkloadsQueue::AdvanceGPUTimelineSection()
{
	++m_submissionSection.value;

	const lib::LockGuard lock(m_submittedWorkloadsLock);

	if (m_submittedWorkloadsQueue.empty())
	{
		++m_executionSection.value;

		SPT_CHECK(m_executionSection <= m_submissionSection);
	}
	else
	{
		js::Event advanceExecutedSectionEvent = js::CreateEvent("AdvanceExecutedSectionEvent",
																[this]
																{
																	++m_executionSection.value;

																	SPT_CHECK(m_executionSection <= m_submissionSection);
																});

		SignalAfterFlushedPendingWork_Locked(advanceExecutedSectionEvent);
	}
}

void SubmittedWorkloadsQueue::SignalAfterFlushedPendingWork_Locked(const js::Event& event)
{
	if (m_submittedWorkloadsQueue.empty())
	{
		event.Signal();
	}
	else
	{
		m_submittedWorkloadsQueue[m_submittedWorkloadsQueue.size() - 1u]->BindEvent(std::move(event));
	}
}

void SubmittedWorkloadsQueue::FlushThreadMain()
{
	SPT_PROFILER_THREAD("GPU Workloads Tracker");

	while (m_flushThreadRunning.load())
	{
		m_flushThreadWakeSemaphore.acquire();

		lib::SharedPtr<GPUWorkload> workload;

		{
			const lib::LockGuard lock(m_submittedWorkloadsLock);

			if (m_submittedWorkloadsQueue.empty())
			{
				continue;
			}

			workload = *m_submittedWorkloadsQueue.begin();
		}

		SPT_CHECK(!!workload);

		workload->Wait();

		{
			const lib::LockGuard lock(m_submittedWorkloadsLock);

			if (m_submittedWorkloadsQueue.empty() || m_submittedWorkloadsQueue.front().Get() != workload.get())
			{
				// FullFlush on another thread already handled this workload
				continue;
			}

			m_submittedWorkloadsQueue.pop_front();
		}

		js::Launch(SPT_GENERIC_JOB_NAME,
				   [finishedWorkload = std::move(workload)]()
				   {
					   finishedWorkload->OnExecutionFinished();
				   });
	}

	FullFlush();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// DeviceQueuesManager ===========================================================================

DeviceQueuesManager::DeviceQueuesManager()
	: m_corePipeSemaphoreValue(0)
	, m_memoryTransfersSemaphoreValue(0)
{ }

void DeviceQueuesManager::Initialize()
{
	m_graphicsQueue.Initialize(rhi::VulkanRHI::GetDeviceQueue(rhi::EDeviceCommandQueueType::Graphics));
	m_computeQueue.Initialize(rhi::VulkanRHI::GetDeviceQueue(rhi::EDeviceCommandQueueType::AsyncCompute));
	m_transferQueue.Initialize(rhi::VulkanRHI::GetDeviceQueue(rhi::EDeviceCommandQueueType::Transfer));

	m_corePipeSemaphore      = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("Core Pipe Semaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Timeline));
	m_corePipeSemaphoreValue = 0u;

	m_memoryTransfersSemaphore      = rdr::ResourcesManager::CreateRenderSemaphore(RENDERER_RESOURCE_NAME("Memory Transfers Semaphore"), rhi::SemaphoreDefinition(rhi::ESemaphoreType::Timeline));
	m_memoryTransfersSemaphoreValue = 0u;

	m_submittedWorkloadsQueue.Initialize();
}

void DeviceQueuesManager::Uninitialize()
{
	m_submittedWorkloadsQueue.Uninitialize();

	m_graphicsQueue.Release();
	m_computeQueue.Release();
	m_transferQueue.Release();

	m_corePipeSemaphore.reset();
	m_memoryTransfersSemaphore.reset();
}

void DeviceQueuesManager::Submit(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags /*= EGPUWorkloadSubmitFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lock(m_lock);

	SubmitWorkloadInternal(workload, flags);
}

void DeviceQueuesManager::FlushSubmittedWorkloads()
{
	m_submittedWorkloadsQueue.FullFlush();
}

void DeviceQueuesManager::SignalAfterFlushingPendingWork(const js::Event& event)
{
	m_submittedWorkloadsQueue.SignalAfterFlushedPendingWork(event);
}

GPUTimelineSection DeviceQueuesManager::GetLastSubmittedSection() const
{
	return m_submittedWorkloadsQueue.GetSubmittedSection();
}

GPUTimelineSection DeviceQueuesManager::GetLastExecutedSection() const
{
	return m_submittedWorkloadsQueue.GetExecutedSection();
}

GPUTimelineSection DeviceQueuesManager::GetRecordedSection() const
{
	return m_submittedWorkloadsQueue.GetRecordedSection();
}

Bool DeviceQueuesManager::IsSubmitted(GPUTimelineSection section) const
{
	return !section.IsValid() || m_submittedWorkloadsQueue.GetSubmittedSection() >= section;
}

Bool DeviceQueuesManager::IsExecuted(GPUTimelineSection section) const
{
	return !section.IsValid() || m_submittedWorkloadsQueue.GetExecutedSection() >= section;
}

void DeviceQueuesManager::AdvanceGPUTimelineSection()
{
	m_submittedWorkloadsQueue.AdvanceGPUTimelineSection();
}

void DeviceQueuesManager::SubmitWorkloadInternal(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags /*= EGPUWorkloadSubmitFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(workload->IsReadyToSubmit());

	const Bool isValidCorePipe = !lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::CorePipeSignal) || lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::CorePipeWait);
	SPT_CHECK_MSG(isValidCorePipe, "CorePipeSignal flag cannot be used without CorePipeWait!");

	const Bool isValidMemoryTransfers = !lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::MemoryTransfersSignal) || lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::MemoryTransfersWait);
	SPT_CHECK_MSG(isValidMemoryTransfers, "MemoryTransfersSignal flag cannot be used without MemoryTransfersWait!");

	WorkloadSubmitDefinition definition;
	definition.workload = workload.ToSharedPtr();

	DeviceQueue& submissionQueue = GetQueue(workload->GetQueueType());

	definition.waitSemaphores   = std::move(workload->GetWaitSemaphores());
	definition.signalSemaphores = std::move(workload->GetSignalSemaphores());

	CollectWaitSemaphores(workload, submissionQueue, flags, INOUT definition.waitSemaphores);
	CollectSignalSemaphores(workload, submissionQueue, flags, INOUT definition.signalSemaphores);

	submissionQueue.SubmitGPUWorkload(std::move(definition));

	m_submittedWorkloadsQueue.Push(workload);
}

void DeviceQueuesManager::CollectWaitSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& waitSemaphores)
{
	SPT_PROFILER_FUNCTION();

	const lib::DynamicArray<lib::SharedPtr<GPUWorkload>>& prerequisites = workload->GetPrerequisites_Unsafe();

	for (const lib::SharedPtr<GPUWorkload>& prerequisite : prerequisites)
	{
		SPT_CHECK(prerequisite->IsSubmitted());

		if (prerequisite->GetSubmissionQueue() != &submissionQueue)
		{
			waitSemaphores.AddTimelineSemaphore(prerequisite->GetSignaledSemaphore(), prerequisite->GetSignaledSemaphoreValue(), rhi::EPipelineStage::ALL_COMMANDS);
		}
	}

	if(lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::CorePipeWait) && m_corePipeSemaphoreValue > 0)
	{
		waitSemaphores.AddTimelineSemaphore(m_corePipeSemaphore, m_corePipeSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);
	}

	if(lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::MemoryTransfersWait) && m_memoryTransfersSemaphoreValue > 0)
	{
		waitSemaphores.AddTimelineSemaphore(m_memoryTransfersSemaphore, m_memoryTransfersSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);
	}
}

void DeviceQueuesManager::CollectSignalSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& signalSemaphores)
{
	SPT_PROFILER_FUNCTION();

	if(lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::CorePipeSignal))
	{
		signalSemaphores.AddTimelineSemaphore(m_corePipeSemaphore, ++m_corePipeSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);
	}

	if(lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::MemoryTransfersSignal))
	{
		signalSemaphores.AddTimelineSemaphore(m_memoryTransfersSemaphore, ++m_memoryTransfersSemaphoreValue, rhi::EPipelineStage::ALL_COMMANDS);
	}
}

DeviceQueue& DeviceQueuesManager::GetQueue(rhi::EDeviceCommandQueueType queueType)
{
	switch (queueType)
	{
	case rhi::EDeviceCommandQueueType::Graphics:     return m_graphicsQueue;
	case rhi::EDeviceCommandQueueType::AsyncCompute: return m_computeQueue;
	case rhi::EDeviceCommandQueueType::Transfer:     return m_transferQueue;

	default:

		SPT_CHECK_NO_ENTRY_MSG("Invalid queue type {}!", static_cast<Uint32>(queueType));
	}

	return m_graphicsQueue;
}

} // spt::rdr
