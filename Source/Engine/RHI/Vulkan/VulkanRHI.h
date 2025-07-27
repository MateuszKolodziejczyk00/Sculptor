#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICore/RHITypes.h"
#include "RHICore/RHISettings.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "VulkanTypes/RHIDescriptorSet.h"
#include "VulkanTypes/RHIDeviceQueue.h"


namespace spt::rhi
{
struct RHIInitializationInfo;
struct RHIWindowInitializationInfo;
struct SubmitBatchData;
}


namespace spt::vulkan
{

class VulkanMemoryManager;
class LogicalDevice;
class CommandPoolsManager;
class LayoutsManager;
class PipelineLayoutsManager;


class RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void				Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void				Uninitialize();

	static void				FlushCaches();

	static rhi::ERHIType	GetRHIType();

	static void				WaitIdle();

	static const rhi::RHISettings&	GetSettings();
	static Bool						IsRayTracingEnabled();

	static rhi::DescriptorProps		GetDescriptorProps();

	// Device Queues ==================================================================

	static RHIDeviceQueue	GetDeviceQueue(rhi::EDeviceCommandQueueType queueType);	

	// Descriptor Sets =================================================================

	SPT_NODISCARD static RHIDescriptorSet AllocateDescriptorSet(const RHIDescriptorSetLayout& layout);
	static void	FreeDescriptorSet(const RHIDescriptorSet& set);

	// Debug ===========================================================================

#if SPT_RHI_DEBUG

	static void EnableValidationWarnings(Bool enable);

	struct DisableValidationWarningsScope
	{
		DisableValidationWarningsScope()
		{
			VulkanRHI::EnableValidationWarnings(false);
		}

		~DisableValidationWarningsScope()
		{
			VulkanRHI::EnableValidationWarnings(true);
		}
	};

#endif // SPT_RHI_DEBUG

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static CommandPoolsManager&				GetCommandPoolsManager();

	static LayoutsManager&					GetLayoutsManager();

	static PipelineLayoutsManager&			GetPipelineLayoutsManager();

	static const LogicalDevice&				GetLogicalDevice();

	static VulkanMemoryManager&				GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};


#if SPT_RHI_DEBUG

#define RHI_DISABLE_VALIDATION_WARNINGS_SCOPE const vulkan::VulkanRHI::DisableValidationWarningsScope SPT_SCOPE_NAME(_rhi_disable_val_warnings_);

#else

#define RHI_DISABLE_VALIDATION_WARNINGS_SCOPE

#endif //SPT_RHI_DEBUG

} // spt::vulkan