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

	const Uint32 imagesNum = window.GetSwapchainImagesNum();

	// First descriptor pool will be for im gui internal objects. Next ones will be swapped every frame
	const Uint32 descriptorPools = imagesNum + 1; 
	for (Uint32 i = 0; i < descriptorPools; ++i)
	{
		m_uiDescriptorPools.emplace_back(InitializeDescriptorPool());
	}

	SPT_CHECK(m_uiDescriptorPools.size() >= 2);
	
	const LogicalDevice& device = VulkanRHI::GetLogicalDevice();

	ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = VulkanRHI::GetInstanceHandle();
    initInfo.PhysicalDevice = VulkanRHI::GetPhysicalDeviceHandle();
    initInfo.Device = device.GetHandle();
    initInfo.QueueFamily = device.GetGfxQueueFamilyIdx();
    initInfo.Queue = device.GetGfxQueueHandle();
    initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_uiDescriptorPools[0].GetHandle();
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

	m_lastPoolIdx = 0;
}

void RHIUIBackend::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_Shutdown();

	for (DescriptorPool& pool : m_uiDescriptorPools)
	{
		pool.Release();
	}

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

	SizeType descriptorPoolIdxThisFrame = ++m_lastPoolIdx;
	if (descriptorPoolIdxThisFrame == m_uiDescriptorPools.size())
	{
		descriptorPoolIdxThisFrame = 1; // 0 idx is reserved for im gui. We're looping from 1
	}

	m_uiDescriptorPools[descriptorPoolIdxThisFrame].ResetPool();
	ImGui_ImplVulkan_SwapDescriptorPool(m_uiDescriptorPools[descriptorPoolIdxThisFrame].GetHandle());

	m_lastPoolIdx = descriptorPoolIdxThisFrame;
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

DescriptorPool RHIUIBackend::InitializeDescriptorPool()
{
	SPT_PROFILER_FUNCTION();

	DescriptorPool uiDescriptorPool;

	const VkDescriptorPoolCreateFlags flags = 0;

	const Uint32 poolSizeForDescriptorType = 128;

	const lib::StaticArray<VkDescriptorPoolSize, 1> poolSizes =
	{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSizeForDescriptorType },
	};

	constexpr Uint32 maxSetsNum = static_cast<Uint32>(poolSizes.size()) * poolSizeForDescriptorType;

	uiDescriptorPool.Initialize(flags, maxSetsNum, poolSizes.data(), static_cast<Uint32>(poolSizes.size()));
	return uiDescriptorPool;
}

} // spt::vulkan
