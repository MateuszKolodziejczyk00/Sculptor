#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"


namespace spt::mem
{

PLATFORM_API SizeType GetPageSize();

PLATFORM_API Byte* ReserveVirtualMemory(SizeType size);

PLATFORM_API void ReleaseVirtualMemory(Byte* address, SizeType size);

PLATFORM_API void CommitVirtualMemory(Byte* address, SizeType size);
PLATFORM_API void DecommitVirtualMemory(Byte* address, SizeType size);

} // namespace spt::mem