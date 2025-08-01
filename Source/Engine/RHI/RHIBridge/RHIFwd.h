#pragma once


#if SPT_VULKAN_RHI

namespace spt::vulkan
{

class VulkanRHI;
class VulkanRHILimits;
class RHIRenderContext;
class RHIBuffer;
class RHIBufferMemoryOwner;
class RHITexture;
class RHITextureMemoryOwner;
class RHITextureView;
class RHIWindow;
class RHISemaphore;
class RHISemaphoresArray;
class RHICommandBuffer;
class RHIDependency;
class RHIEvent;
class RHIUIBackend;
class RHIShaderModule;
class RHIPipeline;
class RHIProfiler;
class RHIDescriptorHeap;
class RHIDescriptorSetLayout;
class RHISampler;
class RHITopLevelAS;
class RHIBottomLevelAS;
class RHIShaderBindingTable;
class RHIQueryPool;
class RHIGPUMemoryPool;
class RHIDeviceQueue;

} // namespace spt::vulkan

namespace spt::rhi
{

using RHI = vulkan::VulkanRHI;
using RHILimits = vulkan::VulkanRHILimits;

using namespace spt::vulkan;

}

#else

#error Only Vulkan RHI is supported

#endif// SPT_VULKAN_RHI