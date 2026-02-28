#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHITextureTypes.h"


namespace spt::rdr
{
class Buffer;
class Texture;
} // spt::rdr


namespace spt::rdr
{

void RENDERER_CORE_API FillBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, Uint64 range, Uint32 data);
void RENDERER_CORE_API UploadDataToBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);
void RENDERER_CORE_API UploadDataToBufferOnCPU(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);


void RENDERER_CORE_API UploadDataToTexture(const Byte* data, Uint64 dataSize, const lib::SharedRef<rdr::Texture>& texture, rhi::ETextureAspect aspect, math::Vector3u copyExtent, math::Vector3u copyOffset = math::Vector3u::Zero(),  Uint32 mipLevel = 0, Uint32 arrayLayer = 0);

void RENDERER_CORE_API FlushPendingUploads();

} // spt::rdr
