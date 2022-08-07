#include "RHIUIBackend.h"
#include "imgui_impl_vulkan.h"
#include "VulkanRHI.h"
#include "RHIWindow.h"
#include "Device/LogicalDevice.h"

namespace spt::vulkan
{

RHIUIBackend::RHIUIBackend()
{ }

void RHIUIBackend::InitializeRHI(ui::UIContext context, const RHIWindow& window)
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(context.IsValid());
	SPT_CHECK(!IsValid());

	InitializeDescriptorPool();

	const Uint32 imagesNum = window.GetSwapchainImagesNum();
	
	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = VulkanRHI::GetInstanceHandle();
    initInfo.PhysicalDevice = VulkanRHI::GetPhysicalDeviceHandle();
    initInfo.Device = device.GetHandle();
    initInfo.QueueFamily = device.GetGfxQueueIdx();
    initInfo.Queue = device.GetGfxQueueHandle();
    initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_uiDescriptorPool.GetHandle();
    initInfo.MinImageCount = imagesNum;
    initInfo.ImageCount = imagesNum;
	initInfo.MSAASamples; VK_SAMPLE_COUNT_1_BIT;
	initInfo.ColorAttachmentFormat = window.GetSurfaceFormat();
    initInfo.Allocator = VulkanRHI::GetAllocationCallbacks();
    initInfo.CheckVkResultFn = nullptr;

	const bool success = ImGui_ImplVulkan_Init(&initInfo);
	SPT_CHECK(success);

	m_context = context;
}

void RHIUIBackend::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_Shutdown();
	m_uiDescriptorPool.Release();
	m_context.Reset();
}

Bool RHIUIBackend::IsValid() const
{
	return m_context.IsValid();
}

void RHIUIBackend::InitializeDescriptorPool()
{
	SPT_PROFILE_FUNCTION();

	const VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	const Uint32 poolSizeForDescriptorType = 512;

	const lib::DynamicArray<VkDescriptorPoolSize> poolSizes =
	{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, poolSizeForDescriptorType },
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, poolSizeForDescriptorType }
	};

	const Uint32 maxSetsNum = static_cast<Uint32>(poolSizes.size()) * poolSizeForDescriptorType;

	m_uiDescriptorPool.Initialize(flags, maxSetsNum, poolSizes);
}

}
