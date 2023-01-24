#include "UploadsManager.h"
#include "Renderer.h"
#include "Types/Buffer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "TransfersManager.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "BufferUtilities.h"
#include "RHIBridge/RHILimitsImpl.h"

namespace spt::gfx
{

UploadsManager& UploadsManager::Get()
{
	static UploadsManager instance;
	return instance;
}

void UploadsManager::EnqueueUpload(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	// If data is too large for single data buffer, divide it into smaller chunks
	if (dataSize > stagingBufferSize)
	{
		Uint64 currentOffset = 0;
		while (currentOffset <= dataSize)
		{
			const Uint64 stagingBufferRemainingSize = m_currentStagingBufferOffset != idxNone<Uint64> ? stagingBufferSize - m_currentStagingBufferOffset : stagingBufferSize;
			const Uint64 copySize = std::max(dataSize - currentOffset, stagingBufferRemainingSize);
			EnqueueUploadImpl(destBuffer, bufferOffset + currentOffset, sourceData + currentOffset, copySize);
			currentOffset += copySize;
		}
	}
	else
	{
		EnqueueUploadImpl(destBuffer, bufferOffset, sourceData, dataSize);
	}
}

void UploadsManager::EnqueueFill(const lib::SharedRef<rdr::Buffer>& buffer, Uint64 bufferOffset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	CopyCommand* command = nullptr;

	{
		const lib::LockGuard lockGuard(m_lock);
		command = &m_commands.emplace_back();
	}

	command->destBuffer = buffer;
	command->destBufferOffset = bufferOffset;
	command->stagingBufferIdx = idxNone<SizeType>;
	command->fillData = data;
	command->dataSize = range;
}

Bool UploadsManager::HasPendingUploads() const
{
	return !m_commands.empty();
}

void UploadsManager::FlushPendingUploads()
{
	if (HasPendingUploads())
	{
		const lib::LockGuard lockGuard(m_lock);
		FlushPendingUploads_AssumesLocked();
		m_currentStagingBufferOffset = idxNone<Uint64>;
	}
}

UploadsManager::UploadsManager()
	: m_currentStagingBufferOffset(idxNone<Uint64>)
	, m_lastUsedStagingBufferIdx(0)
{
	rhi::BufferDefinition stagingBuffersDef;
	stagingBuffersDef.size = stagingBufferSize;
	stagingBuffersDef.usage = rhi::EBufferUsage::TransferSrc;

	rhi::RHIAllocationInfo allocationInfo;
	allocationInfo.memoryUsage = rhi::EMemoryUsage::CPUToGPU;

	for (Uint32 idx = 0; idx < 4; ++idx)
	{
		StagingBufferInfo& stagingBufferInfo = m_stagingBuffers.emplace_back();
		stagingBufferInfo.buffer = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME("StagingBuffer"), stagingBuffersDef, allocationInfo);
	}

	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																m_stagingBuffers.clear();
															});
}

void UploadsManager::EnqueueUploadImpl(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dataSize <= stagingBufferSize);

	CopyCommand* command = nullptr;
	lib::SharedPtr<rdr::Buffer> stagingBufferInstance;
	SizeType stagingBufferIdx = idxNone<SizeType>;
	Uint64 stagingBufferOffset = 0;

	{
		const lib::LockGuard lockGuard(m_lock);

		if (m_currentStagingBufferOffset == idxNone<Uint64> || m_currentStagingBufferOffset + dataSize > stagingBufferSize)
		{
			AcquireAvailableStagingBuffer_AssumesLocked();
		}

		SPT_CHECK(m_currentStagingBufferOffset != idxNone<Uint64>);

		command = &m_commands.emplace_back();

		SPT_CHECK(!m_stagingBuffers.empty());
		stagingBufferIdx = m_lastUsedStagingBufferIdx;
		stagingBufferOffset = m_currentStagingBufferOffset;
		stagingBufferInstance = m_stagingBuffers[m_lastUsedStagingBufferIdx].buffer;
	
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

	// upload data to staging buffer
	UploadDataToBufferOnCPU(lib::Ref(stagingBufferInstance), stagingBufferOffset, sourceData, dataSize);
}

void UploadsManager::FlushPendingUploads_AssumesLocked()
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("FlushPendingUploadsContext"), rhi::ContextDefinition());
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	for (const CopyCommand& command : m_commands)
	{
		if (command.stagingBufferIdx != idxNone<SizeType>)
		{
			const lib::SharedRef<rdr::Buffer> stagingBuffer = lib::Ref(m_stagingBuffers[command.stagingBufferIdx].buffer);
			recorder->CopyBuffer(stagingBuffer, command.stagingBufferOffset, lib::Ref(command.destBuffer), command.destBufferOffset, command.dataSize);
		}
		else
		{
			recorder->FillBuffer(lib::Ref(command.destBuffer), command.destBufferOffset, command.dataSize, command.fillData);
		}
	}

	const Uint64 transferFinishedSingalValue = TransfersManager::RecordAndSubmitTransfers(context, std::move(recorder));

	for (SizeType stagingBufferIdx : m_stagingBuffersPendingFlush)
	{
		m_stagingBuffers[stagingBufferIdx].lastTransferSignalValue = transferFinishedSingalValue;
	}

	m_commands.clear();
	m_stagingBuffersPendingFlush.clear();
}

void UploadsManager::AcquireAvailableStagingBuffer_AssumesLocked()
{
	if (m_stagingBuffersPendingFlush.size() == m_stagingBuffers.size())
	{
		FlushPendingUploads_AssumesLocked();
	}

	SPT_CHECK(m_stagingBuffersPendingFlush.size() < m_stagingBuffers.size())

	if (++m_lastUsedStagingBufferIdx == m_stagingBuffers.size())
	{
		m_lastUsedStagingBufferIdx = 0;
	}

	const StagingBufferInfo& bufferInfo = m_stagingBuffers[m_lastUsedStagingBufferIdx];
	if (bufferInfo.lastTransferSignalValue > 0)
	{
		TransfersManager::WaitForTransfersFinished(bufferInfo.lastTransferSignalValue);
	}
	
	m_currentStagingBufferOffset = 0;

	m_stagingBuffersPendingFlush.emplace_back(m_lastUsedStagingBufferIdx);
}

} // spt::gfx
