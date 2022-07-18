#pragma once


#if VULKAN_RHI

namespace spt::vulkan
{

class VulkanRHI;

}

namespace spt::rhi
{

using RHI = vulkan::VulkanRHI;

}

#else

#error Only Vulkan RHI is supported

#endif// VULKAN_RHI