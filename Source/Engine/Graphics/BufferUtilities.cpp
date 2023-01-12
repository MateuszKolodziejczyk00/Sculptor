#include "BufferUtilities.h"
#include "ResourcesManager.h"
#include "RHIBridge/RHILimitsImpl.h"
#include "MathUtils.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "CurrentFrameContext.h"
#include "Renderer.h"
#include "Types/Semaphore.h"
#include "Types/RenderContext.h"

namespace spt::gfx
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// GPUBuffersUpdateStagingManager ================================================================

GPUBuffersUploadStagingManager& GPUBuffersUploadStagingManager::Get()
{
	static GPUBuffersUploadStagingManager instance;
	return instance;
}

void GPUBuffersUploadStagingManager::EnqueueUpload(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dataSize <= stagingBufferSize);

	CopyCommand* command = nullptr;
	lib::SharedPtr<rdr::Buffer> stagingBufferInstance;
	SizeType stagingBufferIdx = idxNone<SizeType>;
	Uint64 stagingBufferOffset = 0;

	{
		const lib::LockGuard enqueueLockGuard(m_enqueueLock);

		if (m_stagingBuffers.empty() || m_currentStagingBufferOffset + dataSize > stagingBufferSize)
		{
			m_stagingBuffers.emplace_back(GetAvailableStagingBuffer());
			m_currentStagingBufferOffset = 0;
		}

		command = &m_commands.emplace_back();

		SPT_CHECK(!m_stagingBuffers.empty());
		stagingBufferIdx = m_stagingBuffers.size() - 1;
		stagingBufferOffset = m_currentStagingBufferOffset;
		stagingBufferInstance = m_stagingBuffers.back();
	
		m_currentStagingBufferOffset += dataSize;
		math::Utils::RoundUp(m_currentStagingBufferOffset, rhi::RHILimits::GetOptimalBufferCopyOffsetAlignment());
	}

	SPT_CHECK(stagingBufferIdx != idxNone<SizeType>);
	SPT_CHECK(!!stagingBufferInstance);
	
	command->destBuffer = destBuffer;
	command->destBufferOffset = bufferOffset;
	command->stagingBufferIdx = stagingBufferIdx;
	command->stagingBufferOffset = stagingBufferOffset;
	command->dataSize = dataSize;

	UploadDataToBufferOnCPU(lib::Ref(stagingBufferInstance), stagingBufferOffset, sourceData, dataSize);
}

void GPUBuffersUploadStagingManager::EnqueueFill(const lib::SharedRef<rdr::Buffer>& buffer, Uint64 bufferOffset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	CopyCommand* command = nullptr;

	{
		const lib::LockGuard enqueueLockGuard(m_enqueueLock);
		command = &m_commands.emplace_back();
	}

	command->destBuffer = buffer;
	command->destBufferOffset = bufferOffset;
	command->stagingBufferIdx = idxNone<SizeType>;
	command->fillData = data;
	command->dataSize = range;
}

Bool GPUBuffersUploadStagingManager::HasPendingUploads() const
{
	return !m_commands.empty();
}

void GPUBuffersUploadStagingManager::FlushPendingUploads(rdr::CommandRecorder& recorder)
{
	SPT_PROFILER_FUNCTION();

	for (const CopyCommand& command : m_commands)
	{
		if (command.stagingBufferIdx != idxNone<SizeType>)
		{
			const lib::SharedRef<rdr::Buffer> stagingBuffer = lib::Ref(m_stagingBuffers[command.stagingBufferIdx]);
			recorder.CopyBuffer(stagingBuffer, command.stagingBufferOffset, lib::Ref(command.destBuffer), command.destBufferOffset, command.dataSize);
		}
		else
		{
			recorder.FillBuffer(lib::Ref(command.destBuffer), command.destBufferOffset, command.dataSize, command.fillData);
		}
	}

	m_commands.clear();

	m_stagingBuffers.resize(preservedStagingBuffersNum);
	rdr::CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda([this, stagingBuffers = std::move(m_stagingBuffers)]()
																		 {
																			 const lib::LockGuard preserveBuffersLockGuard(m_preservedBuffersLock);
																			 if (m_preservedStagingBuffers.empty())
																			 {
																				 m_preservedStagingBuffers = std::move(stagingBuffers);
																			 }
																			 else
																			 {
																				 m_preservedStagingBuffers.insert(std::end(m_preservedStagingBuffers), std::cbegin(stagingBuffers), std::cend(stagingBuffers));
																			 }
																		 });
}

lib::SharedPtr<rdr::Buffer> GPUBuffersUploadStagingManager::GetAvailableStagingBuffer()
{
	SPT_PROFILER_FUNCTION();

	{
		const lib::LockGuard preserveBuffersLockGuard(m_preservedBuffersLock);
		if (!m_preservedStagingBuffers.empty())
		{
			lib::SharedPtr<rdr::Buffer> buffer = std::move(m_preservedStagingBuffers.back());
			m_preservedStagingBuffers.pop_back();
			return buffer;
		}
	}

	rhi::BufferDefinition bufferDef;
	bufferDef.size = stagingBufferSize;
	bufferDef.usage = rhi::EBufferUsage::TransferSrc;

	rhi::RHIAllocationInfo allocationInfo;
	allocationInfo.memoryUsage = rhi::EMemoryUsage::CPUToGPU;

	return rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StagingBuffer"), bufferDef, allocationInfo).ToSharedPtr();
}

GPUBuffersUploadStagingManager::GPUBuffersUploadStagingManager()
	: m_currentStagingBufferOffset(0)
{
	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_preservedStagingBuffers.clear();
																m_stagingBuffers.clear();
															});
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferUtilities ===============================================================================

void FillBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIBuffer& rhiBuffer = destBuffer->GetRHI();
	SPT_CHECK(bufferOffset + range <= rhiBuffer.GetSize());

	const Bool isBufferHostAccessible = rhiBuffer.CanMapMemory();
	if (isBufferHostAccessible)
	{
		Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
		SPT_CHECK(!!bufferMemoryPtr);
		
		memset(bufferMemoryPtr + bufferOffset, static_cast<int>(data), range);

		rhiBuffer.UnmapBufferMemory();
	}
	else
	{
		GPUBuffersUploadStagingManager::Get().EnqueueFill(destBuffer, bufferOffset, range, data);
	}
}

void UploadDataToBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIBuffer& rhiBuffer = destBuffer->GetRHI();
	SPT_CHECK(bufferOffset + dataSize <= rhiBuffer.GetSize());

	const Bool isBufferHostAccessible = rhiBuffer.CanMapMemory();
	if (isBufferHostAccessible)
	{
		Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
		SPT_CHECK(!!bufferMemoryPtr);
		
		std::memcpy(bufferMemoryPtr + bufferOffset, sourceData, dataSize);

		rhiBuffer.UnmapBufferMemory();
	}
	else
	{
		SPT_CHECK_MSG(lib::HasAnyFlag(rhiBuffer.GetUsage(), rhi::EBufferUsage::TransferDst), "Buffer memory cannot be mapped on host and cannot be transfer destination!");

		GPUBuffersUploadStagingManager::Get().EnqueueUpload(destBuffer, bufferOffset, sourceData, dataSize);
	}
}

void UploadDataToBufferOnCPU(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIBuffer& rhiBuffer = destBuffer->GetRHI();
	SPT_CHECK(bufferOffset + dataSize <= rhiBuffer.GetSize());

	Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
	SPT_CHECK(!!bufferMemoryPtr);
	
	std::memcpy(bufferMemoryPtr + bufferOffset, sourceData, dataSize);

	rhiBuffer.UnmapBufferMemory();
}

lib::SharedPtr<rdr::Semaphore> FlushPendingUploads()
{
	SPT_PROFILER_FUNCTION();

	GPUBuffersUploadStagingManager& uploadsManager = GPUBuffersUploadStagingManager::Get();

	lib::SharedPtr<rdr::Semaphore> finishSemaphore;

	if (uploadsManager.HasPendingUploads())
	{
		const rhi::SemaphoreDefinition semaphoreDef;
		finishSemaphore = rdr::ResourcesManager::CreateSemaphore(RENDERER_RESOURCE_NAME("FlushPendingBuffersUploadsSemaphore"), semaphoreDef);

		rhi::ContextDefinition contextDef;
		const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("FlushPendingBuffersUploadsContext"), contextDef);

		lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

		uploadsManager.FlushPendingUploads(*recorder);

		rdr::CommandsRecordingInfo recordingInfo;
		recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("FlushPendingBuffersUploadsCmdBuffer");
		recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
		recorder->RecordCommands(renderContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

		lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
		rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back();
		submitBatch.recordedCommands.emplace_back(std::move(recorder));
		submitBatch.signalSemaphores.AddBinarySemaphore(finishSemaphore, rhi::EPipelineStage::ALL_TRANSFER);

		rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);
	}

	return finishSemaphore;
}

} // spt::gfx
