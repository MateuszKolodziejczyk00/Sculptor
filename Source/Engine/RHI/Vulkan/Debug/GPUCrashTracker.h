#pragma once

#include "SculptorCoreTypes.h"


namespace spt::vulkan
{

class GPUCrashTracker
{
public:

	static void EnableGPUCrashDumps();

	static void SaveGPUCrashDump();
};

} // spt::vulkan