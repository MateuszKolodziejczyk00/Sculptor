#pragma once

#include "GraphicsMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::rdr
{
class Buffer;
} // spt::rdr


namespace spt::gfx
{

void GRAPHICS_API FillBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, Uint64 range, Uint32 data);
void GRAPHICS_API UploadDataToBuffer(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);
void GRAPHICS_API UploadDataToBufferOnCPU(const lib::SharedRef<rdr::Buffer>& destBuffer, Uint64 bufferOffset, const Byte* sourceData, Uint64 dataSize);

void GRAPHICS_API FlushPendingUploads();

} // spt::gfx