#pragma once


#if VULKAN_RHI

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
class RHIDescriptorSetManager;
class RHIDescriptorSet;
class RHISampler;
class RHITopLevelAS;
class RHIBottomLevelAS;
class RHIShaderBindingTable;
class RHIQueryPool;
class RHIGPUMemoryPool;

}

namespace spt::rhi
{

using RHI = vulkan::VulkanRHI;
using RHILimits = vulkan::VulkanRHILimits;

using namespace spt::vulkan;

}

#else

#error Only Vulkan RHI is supported

#endif// VULKAN_RHI