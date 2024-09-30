#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDeviceQueueImpl.h"
#include "RendererUtils.h"
#include "Containers/Queue.h"

#include <semaphore>


namespace spt::rdr
{

class Semaphore;
class GPUWorkload;


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

private:

	void FlushThreadMain();

	lib::Queue<lib::SharedRef<GPUWorkload>> m_submittedWorkloadsQueue;
	Real32                                  m_submittedWorkloadsLastFlushTime;
	lib::Lock                               m_submittedWorkloadsLock;

	lib::Thread                               m_flushThread;
	std::atomic<Bool>                         m_flushThreadRunning;
	std::counting_semaphore<maxValue<IntPtr>> m_flushThreadWakeSemaphore;
};


class RENDERER_CORE_API DeviceQueuesManager
{
public:

	DeviceQueuesManager();

	void Initialize();
	void Uninitialize();

	void Submit(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags = EGPUWorkloadSubmitFlags::Default);

	void FlushSubmittedWorkloads();

private:

	void SubmitWorkloadInternal(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags = EGPUWorkloadSubmitFlags::Default);

	void CollectWaitSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& waitSemaphores);
	void CollectSignalSemaphores(const lib::SharedRef<GPUWorkload>& workload, const DeviceQueue& submissionQueue, EGPUWorkloadSubmitFlags flags, INOUT SemaphoresArray& signalSemaphores);

	DeviceQueue& GetQueue(rhi::EDeviceCommandQueueType queueType);

	DeviceQueue m_graphicsQueue;
	DeviceQueue m_computeQueue;
	DeviceQueue m_transferQueue;

	lib::SharedPtr<rdr::Semaphore> m_corePipeSemaphore;
	Uint64                         m_corePipeSemaphoreValue;

	lib::SharedPtr<rdr::Semaphore> m_memoryTransfersSemaphore;
	Uint64                         m_memoryTransfersSemaphoreValue;

	SubmittedWorkloadsQueue m_submittedWorkloadsQueue;

	lib::Lock m_lock;
};

} // spt::rdr