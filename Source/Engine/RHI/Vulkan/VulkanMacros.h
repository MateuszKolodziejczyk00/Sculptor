#pragma once

#include "Vulkan/Debug/GPUCrashTracker.h"

#define SPT_VK_CHECK(Function) { const VkResult result = Function; if(result == VK_ERROR_DEVICE_LOST) { spt::vulkan::GPUCrashTracker::SaveGPUCrashDump(); } SPT_CHECK(result == VK_SUCCESS); }
