#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHICommandBufferTypes.h"
#include "RHICore/RHITypes.h"
#include "RHICore/RHISettings.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "VulkanTypes/RHIDescriptorSet.h"


namespace spt::rhi
{
struct RHIInitializationInfo;
struct RHIWindowInitializationInfo;
struct SubmitBatchData;
}


namespace spt::vulkan
{

class MemoryManager;
class LogicalDevice;
class CommandPoolsManager;
class LayoutsManager;
class PipelineLayoutsManager;


class RHI_API VulkanRHI
{
public:

	// RHI Interface ===================================================================

	static void				Initialize(const rhi::RHIInitializationInfo& initInfo);

	static void				InitializeGPUForWindow();

	static void				Uninitialize();

	static void				BeginFrame();

	static rhi::ERHIType	GetRHIType();

	static void				SubmitCommands(rhi::ECommandBufferQueueType queueType, const lib::DynamicArray<rhi::SubmitBatchData>& submitBatches);

	static void				WaitIdle();

	static const rhi::RHISettings&	GetSettings();
	static Bool						IsRayTracingEnabled();

	// Descriptor Sets =================================================================

	SPT_NODISCARD static RHIDescriptorSet						AllocateDescriptorSet(const rhi::DescriptorSetLayoutID layoutID);
	SPT_NODISCARD static lib::DynamicArray<RHIDescriptorSet>	AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* layoutIDs, Uint32 descriptorSetsNum);
	static void	FreeDescriptorSet(const RHIDescriptorSet& set);
	static void	FreeDescriptorSets(const lib::DynamicArray<RHIDescriptorSet>& sets);

	// Debug ===========================================================================

#if RHI_DEBUG

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

#endif // RHI_DEBUG

	// Vulkan Getters ==================================================================

	static VkInstance						GetInstanceHandle();
	static VkDevice							GetDeviceHandle();
	static VkPhysicalDevice					GetPhysicalDeviceHandle();

	static CommandPoolsManager&				GetCommandPoolsManager();

	static LayoutsManager&					GetLayoutsManager();

	static PipelineLayoutsManager&			GetPipelineLayoutsManager();

	static VkSurfaceKHR						GetSurfaceHandle();

	static const LogicalDevice&				GetLogicalDevice();

	static MemoryManager&					GetMemoryManager();

	static VmaAllocator						GetAllocatorHandle();

	static void								SetSurfaceHandle(VkSurfaceKHR surface);

	static const VkAllocationCallbacks*		GetAllocationCallbacks();
};


#if RHI_DEBUG

#define RHI_DISABLE_VALIDATION_WARNINGS_SCOPE const vulkan::VulkanRHI::DisableValidationWarningsScope SPT_SCOPE_NAME(_rhi_disable_val_warnings_);

#else

#define RHI_DISABLE_VALIDATION_WARNINGS_SCOPE

#endif //RHI_DEBUG

} // spt::vulkan