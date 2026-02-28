#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rdr
{

class Buffer;
class Texture;


class RENDERER_CORE_API TransfersManager
{
public:

	TransfersManager();

	void EnqueueUpload(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);
	void EnqueueFill(const lib::SharedRef<rdr::Buffer>& buffer, Uint64 bufferOffset, Uint64 range, Uint32 data);

	void EnqueueUploadToTexture(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset = math::Vector3u::Zero(),  Uint32 mipLevel = 0, Uint32 arrayLayer = 0);

	Bool HasPendingUploads() const;

	void FlushPendingUploads();

private:

	void EnqueueUploadImpl(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);
	void EnqueueUploadToTextureImpl(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset, Uint32 mipLevel, Uint32 arrayLayer);

	void FlushPendingUploads_AssumesLocked();
	void AcquireAvailableStagingBuffer_AssumesLocked();

	void FlushAsyncCopiesToStagingBuffer();

	static constexpr SizeType stagingBufferSize = 32u * 1024u * 1024u;

	// Number of buffers that are preserved for future frames
	// Actual number of preserved buffers will be FramesInFlight * preservedStagingBuffersNum as we need to keep buffers that may be in use on gpu read-only
	static constexpr SizeType preservedStagingBuffersNum = 1;

	struct CopyCommand
	{
		lib::SharedPtr<rdr::Buffer>	destBuffer;
		Uint64						destBufferOffset = 0;

		SizeType					stagingBufferIdx = 0;
		union
		{
			// valid if staging buffer idx != idxNone. used for copy commands
			Uint64					stagingBufferOffset = 0;
			// valid if staging buffer idx == idxNone. used for fill commands
			Uint32					fillData;
		};

		Uint64						dataSize = 0;
	};

	struct CopyToTextureCommand
	{
		lib::SharedPtr<rdr::Texture>	destTexture;
		math::Vector3u					copyExtent = math::Vector3u::Zero();
		math::Vector3u					copyOffset = math::Vector3u::Zero();
		rhi::ETextureAspect				aspect = rhi::ETextureAspect::None;
		Uint32							mipLevel = 0;
		Uint32							arrayLayer = 0;

		SizeType						stagingBufferIdx = 0;
		Uint64							stagingBufferOffset = 0;
	};

	lib::Lock m_lock;

	lib::DynamicArray<CopyCommand> m_bufferCommands;

	lib::DynamicArray<CopyToTextureCommand> m_copyToTextureCommands;

	struct StagingBufferInfo
	{
		StagingBufferInfo()
			: lastTransferSignalValue(0)
		{ }

		lib::SharedPtr<rdr::Buffer> buffer;
		Uint64 lastTransferSignalValue;
	};

	lib::DynamicArray<StagingBufferInfo> m_stagingBuffers;
	Uint64 m_currentStagingBufferOffset;

	lib::DynamicArray<SizeType> m_stagingBuffersPendingFlush;

	SizeType m_lastUsedStagingBufferIdx;

	std::atomic<Uint32> m_copiesInProgressNum = 0u;
};

} // spt::rdr
