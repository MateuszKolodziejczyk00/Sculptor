#include "BufferUtilities.h"
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
		Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
		SPT_CHECK(!!bufferMemoryPtr);
		
		memset(bufferMemoryPtr + bufferOffset, static_cast<int>(data), range);

		rhiBuffer.UnmapBufferMemory();
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
		Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
		SPT_CHECK(!!bufferMemoryPtr);
		
		std::memcpy(bufferMemoryPtr + bufferOffset, sourceData, dataSize);

		rhiBuffer.UnmapBufferMemory();
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

	Byte* bufferMemoryPtr = rhiBuffer.MapBufferMemory();
	SPT_CHECK(!!bufferMemoryPtr);
	
	std::memcpy(bufferMemoryPtr + bufferOffset, sourceData, dataSize);

	rhiBuffer.UnmapBufferMemory();
}

void FlushPendingUploads()
{
	SPT_PROFILER_FUNCTION();

	UploadsManager& uploadsManager = UploadsManager::Get();
	uploadsManager.FlushPendingUploads();
}

} // spt::gfx
