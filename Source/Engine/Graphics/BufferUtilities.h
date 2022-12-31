#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"
#include "Types/Buffer.h"


namespace spt::rdr
{
class CommandRecorder;
} // spt::rdr


namespace spt::gfx
{

class GRAPHICS_API GPUBuffersUploadStagingManager
{
public:

	static GPUBuffersUploadStagingManager& Get();

	void EnqueueUpload(const lib::SharedRef<rdr::Buffer>&destBuffer, Uint64 bufferOffset, const Byte * sourceData, Uint64 dataSize);

	Bool HasPendingUploads() const;
	void FlushPendingUploads(rdr::CommandRecorder & recorder);

private:

	GPUBuffersUploadStagingManager();

	lib::SharedPtr<rdr::Buffer> GetAvailableStagingBuffer();

	static constexpr SizeType stagingBufferSize = 64 * 1024 * 1024;

	// Number of buffers that are preserved for future frames
	// Actual number of preserved buffers will be FramesInFlight * preservedStagingBuffersNum as we need to keep buffers that may be in use on gpu read-only
	static constexpr SizeType preservedStagingBuffersNum = 1;

	struct CopyCommand
	{
		lib::SharedPtr<rdr::Buffer>	destBuffer;
		Uint64						destBufferOffset = 0;

		SizeType					stagingBufferIdx = 0;
		Uint64						stagingBufferOffset = 0;

		Uint64						dataSize = 0;
	};

	lib::Lock m_enqueueLock;

	lib::DynamicArray<CopyCommand> m_commands;

	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> m_stagingBuffers;
	Uint64 m_currentStagingBufferOffset;

	lib::DynamicArray<lib::SharedPtr<rdr::Buffer>> m_preservedStagingBuffers;
	lib::Lock m_preservedBuffersLock;
};

void GRAPHICS_API UploadDataToBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);

lib::SharedPtr<rdr::Semaphore> GRAPHICS_API FlushPendingUploads();

} // spt::gfx