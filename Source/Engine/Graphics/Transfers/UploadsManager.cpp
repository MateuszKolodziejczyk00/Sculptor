#include "UploadsManager.h"
#include "RHICore/RHIAllocationTypes.h"
#include "Renderer.h"
#include "Types/Buffer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "TransfersManager.h"
#include "ResourcesManager.h"
#include "MathUtils.h"
#include "UploadUtils.h"
#include "RHIBridge/RHILimitsImpl.h"
#include "Types/Texture.h"

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

		while (currentOffset < dataSize)
		{
			const Uint64 copySize = std::min(stagingBufferSize, dataSize - currentOffset);
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

	const Uint64 fragmentSize = texture->GetRHI().GetFragmentSize();
	const Uint64 rowDataSize  = copyExtent.x() * fragmentSize;
	SPT_CHECK(rowDataSize <= stagingBufferSize);

	const Uint32 rowsNum   = copyExtent.y();
	const Uint32 slicesNum = copyExtent.z();

	SPT_CHECK(rowDataSize * rowsNum * slicesNum == dataSize);

	const Uint32 maxRowsInBatch   = static_cast<Uint32>(stagingBufferSize / rowDataSize);
	SPT_CHECK(maxRowsInBatch > 0u);

	Uint64 srcDataOffset = 0u;
	Uint32 currentRow    = 0u;
	Uint32 currentSlice  = 0u;

	while (currentSlice < slicesNum)
	{
		currentRow = 0u;
		while (currentRow < rowsNum)
		{
			const Uint32 rowsInCurrentBatch = std::min(maxRowsInBatch, rowsNum - currentRow);

			const math::Vector3u currentBatchExtent = math::Vector3u(copyExtent.x(), rowsInCurrentBatch, 1u);
			const math::Vector3u currentBatchOffset = math::Vector3u(copyOffset.x(), copyOffset.y() + currentRow, copyOffset.z() + currentSlice);

			const Uint64 dataSizeForCurrentBatch = rowsInCurrentBatch * rowDataSize;
			SPT_CHECK(srcDataOffset + dataSizeForCurrentBatch <= dataSize);

			EnqueueUploadToTextureImpl(data + srcDataOffset, dataSizeForCurrentBatch, texture, aspect, currentBatchExtent, currentBatchOffset, mipLevel, arrayLayer);
			currentRow += rowsInCurrentBatch;
			srcDataOffset += dataSizeForCurrentBatch;
		}

		++currentSlice;
	}

	SPT_CHECK(currentRow    == rowsNum);
	SPT_CHECK(currentSlice  == slicesNum);
	SPT_CHECK(srcDataOffset == dataSize);
}

Bool UploadsManager::HasPendingUploads() const
{
	return !m_bufferCommands.empty() || !m_copyToTextureCommands.empty();
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
	allocationInfo.memoryUsage     = rhi::EMemoryUsage::CPUToGPU;
	allocationInfo.allocationFlags = rhi::EAllocationFlags::CreateMapped;

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
	SPT_CHECK(dataSize > 0u);

	lib::SharedPtr<rdr::Buffer> stagingBufferInstance;
	SizeType stagingBufferIdx = idxNone<SizeType>;
	Uint64 stagingBufferOffset = 0;


	{
		const lib::LockGuard lockGuard(m_lock);

		if (m_currentStagingBufferOffset == idxNone<Uint64> || m_currentStagingBufferOffset + dataSize > stagingBufferSize)
		{
			AcquireAvailableStagingBuffer_AssumesLocked();
		}

		SPT_CHECK(m_lastUsedStagingBufferIdx < m_stagingBuffers.size());

		const StagingBufferInfo& bufferInfo = m_stagingBuffers[m_lastUsedStagingBufferIdx];;

		stagingBufferIdx		= m_lastUsedStagingBufferIdx;
		stagingBufferOffset		= m_currentStagingBufferOffset;
		stagingBufferInstance	= bufferInfo.buffer;
		
		SPT_CHECK(stagingBufferIdx != idxNone<SizeType>);
		SPT_CHECK(!!stagingBufferInstance);

		CopyCommand command;
		command.destBuffer			= destBuffer;
		command.destBufferOffset	= bufferOffset;
		command.stagingBufferIdx	= stagingBufferIdx;
		command.stagingBufferOffset	= stagingBufferOffset;
		command.dataSize			= dataSize;
		
		m_bufferCommands.emplace_back(command);
	
		m_currentStagingBufferOffset += dataSize;
		m_currentStagingBufferOffset = math::Utils::RoundUp(m_currentStagingBufferOffset, std::max<Uint64>(rhi::RHILimits::GetOptimalBufferCopyOffsetAlignment(), 4u));

		++m_copiesInProgressNum;
	}

	// upload data to staging buffer
	UploadDataToBufferOnCPU(lib::Ref(stagingBufferInstance), stagingBufferOffset, sourceData, dataSize);

	--m_copiesInProgressNum;
}

void UploadsManager::EnqueueUploadToTextureImpl(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset, Uint32 mipLevel, Uint32 arrayLayer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(dataSize <= stagingBufferSize);

	lib::SharedPtr<rdr::Buffer> stagingBufferInstance;
	SizeType stagingBufferIdx = idxNone<SizeType>;
	Uint64 stagingBufferOffset = 0;

	{
		const lib::LockGuard lockGuard(m_lock);

		if (m_currentStagingBufferOffset == idxNone<Uint64> || m_currentStagingBufferOffset + dataSize > stagingBufferSize)
		{
			AcquireAvailableStagingBuffer_AssumesLocked();
		}

		SPT_CHECK(m_lastUsedStagingBufferIdx < m_stagingBuffers.size());

		const StagingBufferInfo& bufferInfo = m_stagingBuffers[m_lastUsedStagingBufferIdx];

		stagingBufferIdx		= m_lastUsedStagingBufferIdx;
		stagingBufferOffset		= m_currentStagingBufferOffset;
		stagingBufferInstance	= bufferInfo.buffer;
		
		SPT_CHECK(stagingBufferIdx != idxNone<SizeType>);
		SPT_CHECK(!!stagingBufferInstance);
	
		CopyToTextureCommand command;
		command.destTexture			= texture;
		command.copyExtent			= copyExtent;
		command.copyOffset			= copyOffset;
		command.aspect				= aspect;
		command.mipLevel			= mipLevel;
		command.arrayLayer			= arrayLayer;
		command.stagingBufferIdx	= stagingBufferIdx;
		command.stagingBufferOffset	= stagingBufferOffset;

		m_copyToTextureCommands.emplace_back(command);
	
		m_currentStagingBufferOffset += dataSize;
		m_currentStagingBufferOffset = math::Utils::RoundUp(m_currentStagingBufferOffset, std::max<Uint64>(rhi::RHILimits::GetOptimalBufferCopyOffsetAlignment(), 4u));

		++m_copiesInProgressNum;
	}

	// upload data to staging buffer
	UploadDataToBufferOnCPU(lib::Ref(stagingBufferInstance), stagingBufferOffset, data, dataSize);

	--m_copiesInProgressNum;
}

void UploadsManager::FlushPendingUploads_AssumesLocked()
{
	SPT_PROFILER_FUNCTION();

	lib::SharedRef<rdr::RenderContext> context = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("FlushPendingUploadsContext"), rhi::ContextDefinition());

	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Low);
	lib::UniquePtr<rdr::CommandRecorder> recorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("TransfersCommandBuffer"), context, cmdBufferDef);

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

	for (const CopyToTextureCommand& command : m_copyToTextureCommands)
	{
		rhi::RHIDependency dependency;
		const SizeType barrierIdx = dependency.AddTextureDependency(command.destTexture->GetRHI(), rhi::TextureSubresourceRange(command.aspect));
		dependency.SetLayoutTransition(barrierIdx, rhi::TextureTransition::Undefined, rhi::TextureTransition::TransferDest);

		recorder->ExecuteBarrier(dependency);

		const lib::SharedRef<rdr::Buffer> stagingBuffer = lib::Ref(m_stagingBuffers[command.stagingBufferIdx].buffer);
		recorder->CopyBufferToTexture(stagingBuffer, command.stagingBufferOffset, lib::Ref(command.destTexture), command.aspect, command.copyExtent, command.copyOffset, command.mipLevel, command.arrayLayer);
	}

	FlushAsyncCopiesToStagingBuffer();

	const Uint64 transferFinishedSingalValue = TransfersManager::SubmitTransfers(context, std::move(recorder));

	for (SizeType stagingBufferIdx : m_stagingBuffersPendingFlush)
	{
		m_stagingBuffers[stagingBufferIdx].lastTransferSignalValue = transferFinishedSingalValue;
	}

	m_bufferCommands.clear();
	m_copyToTextureCommands.clear();
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

void UploadsManager::FlushAsyncCopiesToStagingBuffer()
{
	SPT_PROFILER_FUNCTION();

	while (m_copiesInProgressNum > 0u)
	{
		platf::Platform::SwitchToThread();
	}
}

} // spt::gfx
