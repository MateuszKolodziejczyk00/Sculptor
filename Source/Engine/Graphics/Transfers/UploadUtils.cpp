#include "UploadUtils.h"
#include "ResourcesManager.h"
#include "Types/Buffer.h"
#include "Transfers/UploadsManager.h"

namespace spt::gfx
{

void FillBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIBuffer& rhiBuffer = destBuffer->GetRHI();
	SPT_CHECK(bufferOffset + range <= rhiBuffer.GetSize());

	const Bool isBufferHostAccessible = rhiBuffer.CanMapMemory();
	if (isBufferHostAccessible)
	{
		const rhi::RHIMappedByteBuffer mappedBuffer(rhiBuffer);
		
		memset(mappedBuffer.GetPtr() + bufferOffset, static_cast<int>(data), range);
	}
	else
	{
		UploadsManager::Get().EnqueueFill(destBuffer, bufferOffset, range, data);
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
		const rhi::RHIMappedByteBuffer mappedBuffer(rhiBuffer);
		
		std::memcpy(mappedBuffer.GetPtr() + bufferOffset, sourceData, dataSize);
	}
	else
	{
		SPT_CHECK_MSG(lib::HasAnyFlag(rhiBuffer.GetUsage(), rhi::EBufferUsage::TransferDst), "Buffer memory cannot be mapped on host and cannot be transfer destination!");

		UploadsManager::Get().EnqueueUpload(destBuffer, bufferOffset, sourceData, dataSize);
	}
}

void UploadDataToBufferOnCPU(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHIBuffer& rhiBuffer = destBuffer->GetRHI();
	SPT_CHECK(bufferOffset + dataSize <= rhiBuffer.GetSize());

	const rhi::RHIMappedByteBuffer mappedBuffer(rhiBuffer);
	
	std::memcpy(mappedBuffer.GetPtr() + bufferOffset, sourceData, dataSize);
}

void GRAPHICS_API UploadDataToTexture(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset /*= math::Vector3u::Zero()*/, Uint32 mipLevel /*= 0*/, Uint32 arrayLayer /*= 0*/)
{
	SPT_PROFILER_FUNCTION();
	
	UploadsManager::Get().EnqueUploadToTexture(data, dataSize, texture, aspect, copyExtent, copyOffset, mipLevel, arrayLayer);
}

void FlushPendingUploads()
{
	SPT_PROFILER_FUNCTION();

	UploadsManager& uploadsManager = UploadsManager::Get();
	uploadsManager.FlushPendingUploads();
}

} // spt::gfx
