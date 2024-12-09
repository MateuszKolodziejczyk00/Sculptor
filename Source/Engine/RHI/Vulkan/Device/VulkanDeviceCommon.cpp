#include "VulkanDeviceCommon.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/VulkanRHI.h"

#include "RHI/RHICore/RHIPlugin.h"

namespace spt::vulkan
{

namespace priv
{

class VulkanDefaultRHIPlugin : public rhi::RHIPlugin
{
protected:

	using Super = rhi::RHIPlugin;

public:

	// Begin rhi::RHIExtensionsProvider overrides
	virtual void CollectExtensions(rhi::ExtensionsCollector& collector) const override;
	// End rhi::RHIExtensionsProvider overrides
};

void VulkanDefaultRHIPlugin::CollectExtensions(rhi::ExtensionsCollector& collector) const
{
	Super::CollectExtensions(collector);

	const lib::DynamicArray<const char*> requiredDeviceExtensions = VulkanDeviceCommon::GetRequiredDeviceExtensions();
	for (const char* extension : requiredDeviceExtensions)
	{
		collector.AddDeviceExtension(lib::StringView(extension));
	}
}

VulkanDefaultRHIPlugin g_vulkanDefaultRHIPlugin;

} // priv

lib::DynamicArray<const char*> VulkanDeviceCommon::GetRequiredDeviceExtensions()
{
	lib::DynamicArray<const char*> requiredExtensions;

	requiredExtensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

	requiredExtensions.emplace_back(VK_EXT_CONSERVATIVE_RASTERIZATION_EXTENSION_NAME);

#if SPT_ENABLE_NSIGHT_AFTERMATH

	if (VulkanRHI::GetSettings().AreGPUCrashDumpsEnabled())
	{
		requiredExtensions.emplace_back(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
		requiredExtensions.emplace_back(VK_NV_DEVICE_DIAGNOSTICS_CONFIG_EXTENSION_NAME);
	}

#endif // SPT_ENABLE_NSIGHT_AFTERMATH

	requiredExtensions.emplace_back(VK_KHR_PIPELINE_EXECUTABLE_PROPERTIES_EXTENSION_NAME);

	// looks like DXC can generate spir-v using any of these extensions?
	requiredExtensions.emplace_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
	requiredExtensions.emplace_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);

	if (VulkanRHI::GetSettings().IsRayTracingEnabled())
	{
		requiredExtensions.emplace_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
		requiredExtensions.emplace_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
		requiredExtensions.emplace_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
		requiredExtensions.emplace_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
	}

	return requiredExtensions;
}

} // spt::vulkan
