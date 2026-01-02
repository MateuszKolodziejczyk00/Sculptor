#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDeviceQueueImpl.h"
#include "RendererUtils.h"
#include "Containers/Queue.h"
#include "Job.h"
#include "Types/Window.h"
#include "Utils/GPUResourceInitQueue.h"

#include <semaphore>


namespace spt::rdr
{

class Semaphore;
class GPUWorkload;


struct GPUTimelineSection
{
	GPUTimelineSection() = default;

	explicit GPUTimelineSection(Uint64 value)
		: value(value)
	{
	}

	Bool operator==(const GPUTimelineSection& other) const
	{
		return value == other.value;
	}

	Bool operator!=(const GPUTimelineSection& other) const
	{
		return value != other.value;
	}

	Bool operator<(const GPUTimelineSection& other) const
	{
		return value < other.value;
	}

	Bool operator<=(const GPUTimelineSection& other) const
	{
		return value <= other.value;
	}

	Bool operator>(const GPUTimelineSection& other) const
	{
		return value > other.value;
	}

	Bool operator>=(const GPUTimelineSection& other) const
	{
		return value >= other.value;
	}

	Bool IsValid() const
	{
		return value != 0u;
	}

	Uint64 value = 0u;
};


enum class EGPUWorkloadSubmitFlags
{
	None                     = 0,

	// This workload won't be executed until workload which flips previous frame is finished
	WaitForPrevFrameEnd      = BIT(0),
	// This workload will wait for all previous workloads that were part of "core pipe" to finish
	CorePipeWait             = BIT(1),
	// All subsequent workloads that wait for "core pipe" will wait for this workload to finish
	// Warning: This flag must be used with CorePipeWait flag to ensure correct ordering
	CorePipeSignal           = BIT(2),
	CorePipe                 = CorePipeWait | CorePipeSignal,
	// This workload will wait for all previous memory transfers to finish
	// E.g. Wait for all already requested streaming operations to finish before rendering
	MemoryTransfersWait      = BIT(3),
	// This workload is part of memory transfers
	// E.g. This workload transfers vertex buffer to GPU memory
	// Warning: This flag must be used with MemoryTransfersWait flag to ensure correct ordering
	MemoryTransfersSignal    = BIT(4),
	MemoryTransfers          = MemoryTransfersWait | MemoryTransfersSignal,

	// This is workload for gpu resource initialization. All other workloads will wait for this workload to finish before executing
	GPUResourcesInitSignal   = BIT(6),

	Default = None
};


struct WorkloadSubmitDefinition
{
	lib::SharedPtr<GPUWorkload> workload;
	SemaphoresArray             waitSemaphores;
	SemaphoresArray             signalSemaphores;
};


class DeviceQueue
{
public:

	DeviceQueue();

	void Initialize(rhi::RHIDeviceQueue queue);
	void Release();

	void SubmitGPUWorkload(WorkloadSubmitDefinition&& definition);

	void Present(const lib::SharedRef<Window>& window, SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

private:

	const rhi::RHICommandBuffer& GetCommandBuffer(const GPUWorkload& workload) const;

	lib::Lock m_lock;

	rhi::RHIDeviceQueue m_queue;

	lib::SharedPtr<rdr::Semaphore> m_semaphore;
	Uint64                         m_lastSignaledSemaphoreValue;
};


class SubmittedWorkloadsQueue
{
public:

	SubmittedWorkloadsQueue();

	void Initialize();
	void Uninitialize();

	void Push(lib::SharedRef<GPUWorkload> workload);
	void FullFlush();

	void SignalAfterFlushedPendingWork(const js::Event& event);

	GPUTimelineSection GetSubmittedSection() const;
	GPUTimelineSection GetExecutedSection() const;
	GPUTimelineSection GetRecordedSection() const;

	void AdvanceGPUTimelineSection();

private:

	void SignalAfterFlushedPendingWork_Locked(const js::Event& event);

	void FlushThreadMain();

	std::deque<lib::SharedRef<GPUWorkload>> m_submittedWorkloadsQueue;
	Real32                                  m_submittedWorkloadsLastFlushTime;
	lib::Lock                               m_submittedWorkloadsLock;

	lib::Thread                               m_flushThread;
	std::atomic<Bool>                         m_flushThreadRunning;
	std::counting_semaphore<maxValue<IntPtr>> m_flushThreadWakeSemaphore;

	GPUTimelineSection m_submissionSection;
	GPUTimelineSection m_executionSection;
};


class RENDERER_CORE_API DeviceQueuesManager
{
public:

	DeviceQueuesManager();

	void Initialize();
	void Uninitialize();

	void Submit(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags = EGPUWorkloadSubmitFlags::Default);

	void Present(const lib::SharedRef<Window>& window, SwapchainTextureHandle swapchainTexture, const lib::DynamicArray<lib::SharedPtr<Semaphore>>& waitSemaphores);

	void FlushSubmittedWorkloads();

	void SignalAfterFlushingPendingWork(const js::Event& event);

	GPUTimelineSection GetLastSubmittedSection() const;
	GPUTimelineSection GetLastExecutedSection() const;

	GPUTimelineSection GetRecordedSection() const;

	Bool IsSubmitted(GPUTimelineSection section) const;
	Bool IsExecuted(GPUTimelineSection section) const;

	void AdvanceGPUTimelineSection();

	void GPUInitTexture(const lib::SharedPtr<Texture>& texture) { m_gpuResourceInitQueue.EnqueueTextureInit(texture); }

private:

	void SubmitWorkloadInternal_Locked(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags = EGPUWorkloadSubmitFlags::Default);

	void CollectWaitSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& waitSemaphores);
	void CollectSignalSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& signalSemaphores);

	void FlushGPUInits_Locked();

	DeviceQueue& GetQueue(rhi::EDeviceCommandQueueType queueType);

	DeviceQueue m_graphicsQueue;
	DeviceQueue m_computeQueue;
	DeviceQueue m_transferQueue;

	lib::SharedPtr<rdr::Semaphore> m_corePipeSemaphore;
	Uint64                         m_corePipeSemaphoreValue;

	lib::SharedPtr<rdr::Semaphore> m_memoryTransfersSemaphore;
	Uint64                         m_memoryTransfersSemaphoreValue;

	lib::SharedPtr<rdr::Semaphore> m_resourceGPUInitSemaphore;
	Uint64                         m_resourceGPUInitSemaphoreValue;

	GPUResourceInitQueue m_gpuResourceInitQueue;

	SubmittedWorkloadsQueue m_submittedWorkloadsQueue;

	lib::Lock m_lock;
};

} // spt::rdr