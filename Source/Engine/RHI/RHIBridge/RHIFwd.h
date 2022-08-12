#pragma once


#if VULKAN_RHI

namespace spt::vulkan
{

class VulkanRHI;
class RHIBuffer;
class RHITexture;
class RHITextureView;
class RHIWindow;
class RHISemaphore;
class RHISemaphoresArray;
class RHICommandBuffer;
class RHIBarrier;
class RHIUIBackend;
class RHIShaderModule;

}

namespace spt::rhi
{

using RHI = vulkan::VulkanRHI;

using namespace spt::vulkan;

}

#else

#error Only Vulkan RHI is supported

#endif// VULKAN_RHI