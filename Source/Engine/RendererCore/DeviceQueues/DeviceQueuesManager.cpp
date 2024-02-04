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
	: m_submittedWorkloadsLastFlushTime(-999.f)
{ }

void SubmittedWorkloadsQueue::Push(lib::SharedRef<GPUWorkload> workload)
{
	SPT_PROFILER_FUNCTION();

	const lib::LockGuard lock(m_submittedWorkloadsLock);

	m_submittedWorkloadsQueue.push(std::move(workload));
}

void SubmittedWorkloadsQueue::Flush()
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<lib::SharedRef<GPUWorkload>> finishedWorkloads;

	{
		const lib::LockGuard lock(m_submittedWorkloadsLock);

		while (!m_submittedWorkloadsQueue.empty())
		{
			if (m_submittedWorkloadsQueue.front()->IsFinished())
			{
				finishedWorkloads.emplace_back(std::move(m_submittedWorkloadsQueue.front()));
				m_submittedWorkloadsQueue.pop();
			}
			else
			{
				break;
			}
		}
	}

	m_submittedWorkloadsLastFlushTime = engn::Engine::Get().GetEngineTimer().GetCurrentTimeSeconds();

	for (const lib::SharedRef<GPUWorkload>& workload : finishedWorkloads)
	{
		workload->OnExecutionFinished();
	}
}

Real32 SubmittedWorkloadsQueue::GetTimeSinceLastFlush() const
{
	return engn::Engine::Get().GetEngineTimer().GetCurrentTimeSeconds() - m_submittedWorkloadsLastFlushTime;
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
}

void DeviceQueuesManager::Uninitialize()
{
	m_graphicsQueue.Release();
	m_computeQueue.Release();
	m_transferQueue.Release();

	m_corePipeSemaphore.reset();
	m_memoryTransfersSemaphore.reset();
}

void DeviceQueuesManager::Submit(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags /*= EGPUWorkloadSubmitFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	SubmitWorkloadInternal(workload, flags);
}

void DeviceQueuesManager::FlushSubmittedWorkloads()
{
	SPT_PROFILER_FUNCTION();

	m_submittedWorkloadsQueue.Flush();
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

	constexpr Real32 flushSubmittedWorkloadsInterval = 0.002f;

	if (lib::HasAnyFlag(flags, EGPUWorkloadSubmitFlags::CorePipeSignal)
		|| m_submittedWorkloadsQueue.GetTimeSinceLastFlush() >= flushSubmittedWorkloadsInterval)
	{
		js::Launch(SPT_GENERIC_JOB_NAME,
				   [workloadsQueue = &m_submittedWorkloadsQueue]()
				   {
					   workloadsQueue->Flush();
				   });
	}
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
