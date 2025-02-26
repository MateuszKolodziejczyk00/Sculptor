#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/CommandBuffer.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RendererUtils.h"
#include "Job.h"


namespace spt::rdr
{

class DeviceQueue;


struct GPUWorkloadInfo
{
	Bool canBeReused = true;
};


class RENDERER_CORE_API GPUWorkload
{
public:

	explicit GPUWorkload(lib::SharedRef<CommandBuffer> recordedBuffer, const GPUWorkloadInfo& workloadInfo);

	Bool IsSubmitted() const;

	// Resets workload submitted state and allows to reuse it
	void Reset();

	void Wait();
	Bool IsFinished() const;

	void ReleaseBuffer();

	void OnSubmitted(lib::SharedPtr<Semaphore> signalSemaphore, Uint64 signalValue, DeviceQueue* queue);
	void OnExecutionFinished();

	const lib::SharedPtr<CommandBuffer>& GetRecordedBuffer() const;
	rhi::EDeviceCommandQueueType         GetQueueType() const;

	const lib::SharedPtr<Semaphore>& GetSignaledSemaphore() const;
	Uint64                           GetSignaledSemaphoreValue() const;
	DeviceQueue*                     GetSubmissionQueue() const;

	Bool IsReadyToSubmit() const;

	SemaphoresArray& GetWaitSemaphores();
	SemaphoresArray& GetSignalSemaphores();

	lib::SharedRef<rdr::Semaphore> AddBinarySignalSemaphore(RendererResourceName name, rhi::EPipelineStage stages);

	void AddPrerequisite(lib::SharedPtr<GPUWorkload> workload);

	// This function is not thread-safe. It cannot by called when prerequisites can be added (before submission of this workload)
	const lib::DynamicArray<lib::SharedPtr<GPUWorkload>>& GetPrerequisites_Unsafe() const;

	void BindEvent(js::Event event);

private:

	void AddSubsequent(GPUWorkload* workload);

	void OnPrerequisiteSubmitted();

	lib::SharedPtr<CommandBuffer> m_recordedBuffer;

	GPUWorkloadInfo m_info;

	lib::SharedPtr<Semaphore> m_signalSemaphore;
	Uint64                    m_signalSemaphoreValue;
	DeviceQueue*              m_submissionQueue;

	lib::Lock                                      m_prerequisitesLock;
	lib::DynamicArray<lib::SharedPtr<GPUWorkload>> m_prerequisites;

	std::atomic<Uint32> m_remainingPrerequisitesNum;

	Bool m_isFinalized = false;

	lib::DynamicArray<GPUWorkload*> m_subsequents;

	SemaphoresArray m_waitSemaphores;
	SemaphoresArray m_signalSemaphores;

	lib::DynamicArray<js::Event> m_eventsOnFinished;
};

} // spt::rdr
