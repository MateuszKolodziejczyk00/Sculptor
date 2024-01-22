#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIBridge/RHIDeviceQueueImpl.h"
#include "RendererUtils.h"


namespace spt::rdr
{

class Semaphore;
class GPUWorkload;


enum class EGPUWorkloadSubmitFlags
{
	None                     = 0,

	// This workload won't be executed until workload which flips previous frame is finished
	WaitForPrevFrameEnd      = BIT(0),
	// GPU frame is going to be considered as "finished" after this workload is executed
	FlipFrame                = BIT(1),
	// This workload will wait for all previous workloads that were part of "core pipe" to finish
	CorePipeWait             = BIT(2),
	// All subsequent workloads that wait for "core pipe" will wait for this workload to finish
	// Warning: This flag must be used with CorePipeWait flag to ensure correct ordering
	CorePipeSignal           = BIT(3),
	CorePipe                 = CorePipeWait | CorePipeSignal,
	// This workload will wait for all previous memory transfers to finish
	// E.g. Wait for all already requested streaming operations to finish before rendering
	MemoryTransfersWait      = BIT(4),
	// This workload is part of memory transfers
	// E.g. This workload transfers vertex buffer to GPU memory
	// Warning: This flag must be used with MemoryTransfersWait flag to ensure correct ordering
	MemoryTransfersSignal    = BIT(5),
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

	Bool CanOverlapWithPrevFrame() const;
	void SetCanOverlapWithPrevFrame(Bool canOverlap);

private:

	const rhi::RHICommandBuffer& GetCommandBuffer(const GPUWorkload& workload) const;

	lib::Lock m_lock;

	rhi::RHIDeviceQueue m_queue;

	lib::SharedPtr<rdr::Semaphore> m_semaphore;
	Uint64                         m_lastSignaledSemaphoreValue;

	Bool m_canOverlapWithPrevFrame;
};


class RENDERER_CORE_API DeviceQueuesManager
{
public:

	DeviceQueuesManager();

	void Initialize();
	void Uninitialize();

	void Submit(const lib::SharedRef<GPUWorkload>& workload, EGPUWorkloadSubmitFlags flags = EGPUWorkloadSubmitFlags::Default);

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
};

} // spt::rdr