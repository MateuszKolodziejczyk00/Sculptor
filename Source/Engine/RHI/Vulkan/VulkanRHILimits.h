#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "VulkanCore.h"


namespace spt::vulkan
{

class LogicalDevice;


class RHI_API VulkanRHILimits
{
public:

	static void Initialize(const LogicalDevice& logicalDevice, VkPhysicalDevice physicalDevice);

	static Uint64 GetMinUniformBufferOffsetAlignment();

	static Uint64 GetOptimalBufferCopyOffsetAlignment();

private:

	VulkanRHILimits() = default;
};

} // spt::vulkan