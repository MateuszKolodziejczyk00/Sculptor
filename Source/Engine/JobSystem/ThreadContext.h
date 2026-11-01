#pragma once

#include "JobSystemMacros.h"
#include "SculptorCoreTypes.h"


namespace spt::tcx
{

JOB_SYSTEM_API void InitThreadContext();
JOB_SYSTEM_API void ShutdownThreadContext();

JOB_SYSTEM_API lib::MemoryArena& Scratch();

JOB_SYSTEM_API lib::MemoryArenaScope ScratchScope();

} // spt::tcx


#define TCX_SCRATCH_SCOPE() auto scratchScope = spt::tcx::ScratchScope()
