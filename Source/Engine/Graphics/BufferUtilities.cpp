#include "BufferUtilities.h"
#include "ResourcesManager.h"
#include "RHIBridge/RHILimitsImpl.h"
#include "MathUtils.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "CurrentFrameContext.h"

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

	const lib::LockGuard enqueueLockGuard(m_enqueueLock);

	if (m_stagingBuffers.empty() || m_currentStagingBufferOffset + dataSize > stagingBufferSize)
	{
		m_stagingBuffers.emplace_back(GetAvailableStagingBuffer());
		m_currentStagingBufferOffset = 0;
	}

	CopyCommand& command = m_commands.emplace_back();
	command.destBuffer = destBuffer;
	command.destBufferOffset = bufferOffset;
	command.stagingBufferIdx = m_stagingBuffers.size() - 1;
	command.stagingBufferOffset = m_currentStagingBufferOffset;
	command.dataSize = dataSize;

	m_currentStagingBufferOffset += dataSize;

	math::Utils::RoundUp(m_currentStagingBufferOffset, rhi::RHILimits::GetOptimalBufferCopyOffsetAlignment());
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
		const lib::SharedRef<rdr::Buffer> stagingBuffer = lib::Ref(m_stagingBuffers[command.stagingBufferIdx]);
		recorder.CopyBuffer(stagingBuffer, command.stagingBufferOffset, lib::Ref(command.destBuffer), command.destBufferOffset, command.dataSize);
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
{ }

//////////////////////////////////////////////////////////////////////////////////////////////////
// BufferUtilities ===============================================================================

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
		
		memcpy(bufferMemoryPtr, sourceData, dataSize);

		rhiBuffer.UnmapBufferMemory();
	}
	else
	{
		SPT_CHECK_MSG(lib::HasAnyFlag(rhiBuffer.GetUsage(), rhi::EBufferUsage::TransferDst), "Buffer memory cannot be mapped on host and cannot be transfer destination!");

		GPUBuffersUploadStagingManager::Get().EnqueueUpload(destBuffer, bufferOffset, sourceData, dataSize);
	}
}

} // spt::gfx
