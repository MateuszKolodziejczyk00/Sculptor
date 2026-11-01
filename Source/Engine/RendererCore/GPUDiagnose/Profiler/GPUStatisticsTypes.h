#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rdr
{

using GPUTimestampNs= Uint64;
using GPUDurationNs = Uint64;
using GPUDurationMs = Real64;


struct TransientAllocEventInfo
{
	Uint64 allocationId = 0u;
	lib::HashedString name;
	Uint64 size = 0u;
};

} // spt::rdr
