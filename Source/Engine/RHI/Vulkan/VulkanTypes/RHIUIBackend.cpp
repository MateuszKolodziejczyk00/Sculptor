#include "RHIUIBackend.h"
#include "imgui_impl_vulkan.h"
#include "RHIWindow.h"
#include "RHISampler.h"
#include "RHITexture.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "RHICommandBuffer.h"

namespace spt::vulkan
{

RHIUIBackend::RHIUIBackend()
{ }

void RHIUIBackend::InitializeRHI(ui::UIContext context, const RHIWindow& window)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(context.IsValid());
	SPT_CHECK(!IsValid());

	InitializeDescriptorPool();

	const Uint32 imagesNum = window.GetSwapchainImagesNum();
	
	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = VulkanRHI::GetInstanceHandle();
    initInfo.PhysicalDevice = VulkanRHI::GetPhysicalDeviceHandle();
    initInfo.Device = device.GetHandle();
    initInfo.QueueFamily = device.GetGfxQueueFamilyIdx();
    initInfo.Queue = device.GetGfxQueueHandle();
    initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_uiDescriptorPool.GetHandle();
    initInfo.MinImageCount = imagesNum;
    initInfo.ImageCount = imagesNum;
	initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
	initInfo.ColorAttachmentFormat = window.GetSurfaceFormat();
    initInfo.Allocator = VulkanRHI::GetAllocationCallbacks();
    initInfo.CheckVkResultFn = nullptr;

	ImGui::SetCurrentContext(context.GetHandle());

	const bool success = ImGui_ImplVulkan_Init(&initInfo);
	SPT_CHECK(success);

	m_context = context;
}

void RHIUIBackend::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_Shutdown();
	m_uiDescriptorPool.Release();
	m_context.Reset();
}

Bool RHIUIBackend::IsValid() const
{
	return m_context.IsValid();
}

void RHIUIBackend::InitializeFonts(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	{
		// Disable small allocations performance warnings
		RHI_DISABLE_VALIDATION_WARNINGS_SCOPE;
		ImGui_ImplVulkan_CreateFontsTexture(cmdBuffer.GetHandle());
	}
}

void RHIUIBackend::DestroyFontsTemporaryObjects()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_DestroyFontUploadObjects();
}

void RHIUIBackend::BeginFrame()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_NewFrame();
}

void RHIUIBackend::Render(const RHICommandBuffer& cmdBuffer)
{
	SPT_PROFILER_FUNCTION();

	ImGui::SetCurrentContext(m_context.GetHandle());

	ImDrawData* drawData = ImGui::GetDrawData();

	{
		// Disable small allocations performance warnings
		RHI_DISABLE_VALIDATION_WARNINGS_SCOPE;
		ImGui_ImplVulkan_RenderDrawData(drawData, cmdBuffer.GetHandle());
	}
}

ui::TextureID RHIUIBackend::GetUITexture(const RHITextureView& textureView, const RHISampler& sampler)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(textureView.IsValid());
	SPT_CHECK(sampler.IsValid());

	return ImGui_ImplVulkan_AddTexture(sampler.GetHandle(), textureView.GetHandle(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void RHIUIBackend::InitializeDescriptorPool()
{
	SPT_PROFILER_FUNCTION();

	const VkDescriptorPoolCreateFlags flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

	const Uint32 poolSizeForDescriptorType = 512;

	const lib::StaticArray<VkDescriptorPoolSize, 11> poolSizes =
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

	constexpr Uint32 maxSetsNum = static_cast<Uint32>(poolSizes.size()) * poolSizeForDescriptorType;

	m_uiDescriptorPool.Initialize(flags, maxSetsNum, poolSizes.data(), static_cast<Uint32>(poolSizes.size()));
}

} // spt::vulkan
