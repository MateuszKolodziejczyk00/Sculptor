#include "GPUWorkload.h"
#include "Types/Semaphore.h"
#include "ResourcesManager.h"


namespace spt::rdr
{

GPUWorkload::GPUWorkload(lib::SharedRef<CommandBuffer> recordedBuffer, const rhi::CommandBufferUsageDefinition& commandBufferUsage)
	: m_recordedBuffer(std::move(recordedBuffer))
	, m_signalSemaphoreValue(idxNone<Uint64>)
	, m_submissionQueue(nullptr)
	, m_remainingPrerequisitesNum(0u)
	, m_canBeReused(!lib::HasAnyFlag(commandBufferUsage.beginFlags, rhi::ECommandBufferBeginFlags::OneTimeSubmit))
{ }

Bool GPUWorkload::IsSubmitted() const
{
	return !!m_signalSemaphore;
}

void GPUWorkload::Reset()
{
	m_signalSemaphore.reset();
	m_signalSemaphoreValue = idxNone<Uint64>;
	m_submissionQueue      = nullptr;

	m_remainingPrerequisitesNum = 0u;

	m_prerequisites.clear();
	m_subsequents.clear();
}

void GPUWorkload::Wait()
{
	SPT_CHECK(IsSubmitted());

	m_signalSemaphore->GetRHI().Wait(m_signalSemaphoreValue);
}

void GPUWorkload::ReleaseBuffer()
{
	SPT_CHECK(IsSubmitted());

	m_recordedBuffer.reset();
}

void GPUWorkload::OnSubmitted(lib::SharedPtr<Semaphore> signalSemaphore, Uint64 signalValue, DeviceQueue* queue)
{
	SPT_CHECK(!IsSubmitted());

	const lib::LockGuard lock(m_prerequisitesLock);

	m_signalSemaphore      = std::move(signalSemaphore);
	m_signalSemaphoreValue = signalValue;
	m_submissionQueue      = queue;

	for (GPUWorkload* subsequent : m_subsequents)
	{
		subsequent->OnPrerequisiteSubmitted();
	}

	m_waitSemaphores.Reset();
	m_signalSemaphores.Reset();
	
	m_subsequents.clear();
	m_prerequisites.clear();

	if (!m_canBeReused)
	{
		ReleaseBuffer();
	}
}

const lib::SharedPtr<CommandBuffer>& GPUWorkload::GetRecordedBuffer() const
{
	return m_recordedBuffer;
}

rhi::EDeviceCommandQueueType GPUWorkload::GetQueueType() const
{
	return m_recordedBuffer->GetRHI().GetQueueType();
}

const lib::SharedPtr<Semaphore>& GPUWorkload::GetSignaledSemaphore() const
{
	return m_signalSemaphore;
}

Uint64 GPUWorkload::GetSignaledSemaphoreValue() const
{
	return m_signalSemaphoreValue;
}

DeviceQueue* GPUWorkload::GetSubmissionQueue() const
{
	return m_submissionQueue;
}

Bool GPUWorkload::IsReadyToSubmit() const
{
	return m_remainingPrerequisitesNum == 0u;
}

SemaphoresArray& GPUWorkload::GetWaitSemaphores()
{
	return m_waitSemaphores;
}

SemaphoresArray& GPUWorkload::GetSignalSemaphores()
{
	return m_signalSemaphores;
}

lib::SharedRef<rdr::Semaphore> GPUWorkload::AddBinarySignalSemaphore(RendererResourceName name, rhi::EPipelineStage stages)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsSubmitted());

	const lib::SharedRef<rdr::Semaphore> semaphore = rdr::ResourcesManager::CreateRenderSemaphore(name, rhi::SemaphoreDefinition(rhi::ESemaphoreType::Binary));

	GetSignalSemaphores().AddBinarySemaphore(semaphore, stages);

	return semaphore;
}

void GPUWorkload::AddPrerequisite(lib::SharedPtr<GPUWorkload> workload)
{
	SPT_CHECK(!!workload);

	SPT_CHECK(!IsSubmitted());

	const lib::LockGuard prerequisiteLock(workload->m_prerequisitesLock);
	const lib::LockGuard lock(m_prerequisitesLock);

	if (!workload->IsSubmitted())
	{
		++m_remainingPrerequisitesNum;
	}

	workload->AddSubsequent(this);
	m_prerequisites.emplace_back(std::move(workload));
}

const lib::DynamicArray<lib::SharedPtr<GPUWorkload>>& GPUWorkload::GetPrerequisites_Unsafe() const
{
	return m_prerequisites;
}

void GPUWorkload::AddSubsequent(GPUWorkload* workload)
{
	SPT_CHECK(!!workload);

	m_subsequents.emplace_back(workload);
}

void GPUWorkload::OnPrerequisiteSubmitted()
{
	--m_remainingPrerequisitesNum;
}

} // spt::rdr
