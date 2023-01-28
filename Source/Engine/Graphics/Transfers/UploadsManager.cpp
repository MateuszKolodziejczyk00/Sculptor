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
		command = &m_bufferCommands.emplace_back();
	}

	command->destBuffer = buffer;
	command->destBufferOffset = bufferOffset;
	command->stagingBufferIdx = idxNone<SizeType>;
	command->fillData = data;
	command->dataSize = range;
}

void UploadsManager::EnqueUploadToTexture(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	SPT_PROFILER_FUNCTION();

	// If data is too large for single data buffer, divide it into smaller chunks
	if (dataSize > stagingBufferSize)
	{
		// For now only handle textures 2D
		SPT_CHECK(copyExtent.z() == 1 && copyExtent.y() > 1);

		const Uint32 halfHeight = copyExtent.y() / 2;

		const Byte* data1 = data;
		const Uint64 size1 = dataSize / copyExtent.y() * halfHeight;
		const math::Vector3u offset1 = copyOffset;
		const math::Vector3u extent1 = math::Vector3u(copyExtent.x(), halfHeight, copyExtent.z());

		EnqueueUploadToTextureImpl(data1, size1, texture, aspect, extent1, offset1, mipLevel, arrayLayer);

		const Byte* data2 = data + size1;
		const Uint64 size2 = dataSize - size1;
		const math::Vector3u offset2 = copyOffset + math::Vector3u::UnitY() * halfHeight;
		const math::Vector3u extent2 = math::Vector3u(copyExtent.x(), copyExtent.y() - halfHeight, copyExtent.z());

		EnqueueUploadToTextureImpl(data2, size2, texture, aspect, extent2, offset2, mipLevel, arrayLayer);
	}
	else
	{
		EnqueueUploadToTextureImpl(data, dataSize, texture, aspect, copyExtent, copyOffset, mipLevel, arrayLayer);
	}
}

Bool UploadsManager::HasPendingUploads() const
{
	return !m_bufferCommands.empty();
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

		command = &m_bufferCommands.emplace_back();

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

void UploadsManager::EnqueueUploadToTextureImpl(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset, Uint32 mipLevel, Uint32 arrayLayer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dataSize <= stagingBufferSize);

	CopyToTextureCommand* command = nullptr;
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

		command = &m_copyToTextureCommands.emplace_back();

		SPT_CHECK(!m_stagingBuffers.empty());
		stagingBufferIdx = m_lastUsedStagingBufferIdx;
		stagingBufferOffset = m_currentStagingBufferOffset;
		stagingBufferInstance = m_stagingBuffers[m_lastUsedStagingBufferIdx].buffer;
	
		m_currentStagingBufferOffset += dataSize;
		math::Utils::RoundUp(m_currentStagingBufferOffset, rhi::RHILimits::GetOptimalBufferCopyOffsetAlignment());
	}

	SPT_CHECK(stagingBufferIdx != idxNone<SizeType>);
	SPT_CHECK(!!stagingBufferInstance);
	
	command->destTexture			= texture;
	command->copyExtent				= copyExtent;
	command->copyOffset				= copyOffset;
	command->aspect					= aspect;
	command->mipLevel				= mipLevel;
	command->arrayLayer				= arrayLayer;
	command->stagingBufferIdx		= stagingBufferIdx;
	command->stagingBufferOffset	= stagingBufferOffset;

	// upload data to staging buffer
	UploadDataToBufferOnCPU(lib::Ref(stagingBufferInstance), stagingBufferOffset, data, dataSize);
}

void UploadsManager::FlushPendingUploads_AssumesLocked()
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("FlushPendingUploadsContext"), rhi::ContextDefinition());
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::Renderer::StartRecordingCommands();

	for (const CopyCommand& command : m_bufferCommands)
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

	m_bufferCommands.clear();
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
