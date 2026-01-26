#pragma once

#include "PlatformMacros.h"
#include "SculptorAliases.h"


namespace spt::mem
{

SizeType PLATFORM_API GetPageSize();

Byte* PLATFORM_API ReserveVirtualMemory(SizeType size);

void PLATFORM_API ReleaseVirtualMemory(Byte* address, SizeType size);

void PLATFORM_API CommitVirtualMemory(Byte* address, SizeType size);
void PLATFORM_API DecommitVirtualMemory(Byte* address, SizeType size);

} // spt::mem
