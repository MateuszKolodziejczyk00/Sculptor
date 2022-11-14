#pragma once


namespace spt::vulkan
{

class GPUCrashTracker
{
public:

	static void EnableGPUCrashDumps();

	static void SaveGPUCrashDump();
};

} // spt::vulkan