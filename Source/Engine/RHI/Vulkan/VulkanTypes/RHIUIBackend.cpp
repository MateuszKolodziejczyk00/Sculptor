#include "RHIUIBackend.h"
#include "imgui_impl_vulkan.h"
#include "RHIWindow.h"
#include "RHITexture.h"
#include "Vulkan/VulkanRHI.h"
#include "Vulkan/Device/LogicalDevice.h"
#include "RHICommandBuffer.h"

namespace spt::vulkan
{

RHIUIBackend::RHIUIBackend()
	: m_lastPoolIdx(0)
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
    initInfo.Queue = device.GetGfxQueue().GetHandleChecked();
    initInfo.PipelineCache = VK_NULL_HANDLE;
	initInfo.DescriptorPool = m_uiDescriptorPools[0];
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

//typedef VkResult (VKAPI_PTR *PFN_vkCreateSampler)(VkDevice device, const VkSamplerCreateInfo* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkSampler* pSampler);

	VkSamplerCreateInfo samplerCreateInfo{ VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
	samplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerCreateInfo.mipLodBias              = 0.0f;
	samplerCreateInfo.anisotropyEnable        = VK_FALSE;
	samplerCreateInfo.maxAnisotropy           = 1.0f;
	samplerCreateInfo.compareEnable           = VK_FALSE;
	samplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
	samplerCreateInfo.minLod                  = 0.0f;
	samplerCreateInfo.maxLod                  = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	samplerCreateInfo.flags                   = 0;

	SPT_VK_CHECK(vkCreateSampler(VulkanRHI::GetDeviceHandle(), &samplerCreateInfo, VulkanRHI::GetAllocationCallbacks(), &m_linearSampler));

	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;

	SPT_VK_CHECK(vkCreateSampler(VulkanRHI::GetDeviceHandle(), &samplerCreateInfo, VulkanRHI::GetAllocationCallbacks(), &m_nearestSampler));
}

void RHIUIBackend::ReleaseRHI()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	ImGui_ImplVulkan_Shutdown();

	for (VkDescriptorPool pool : m_uiDescriptorPools)
	{
		vkDestroyDescriptorPool(VulkanRHI::GetDeviceHandle(), pool, VulkanRHI::GetAllocationCallbacks());
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

	vkResetDescriptorPool(VulkanRHI::GetDeviceHandle(), m_uiDescriptorPools[descriptorPoolIdxThisFrame], 0u);

	ImGui_ImplVulkan_SwapDescriptorPool(m_uiDescriptorPools[descriptorPoolIdxThisFrame]);

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

ui::TextureID RHIUIBackend::GetUITexture(const RHITextureView& textureView, rhi::ESamplerFilterType filterType)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(textureView.IsValid());

	const VkSampler samplerHandle = filterType == rhi::ESamplerFilterType::Linear ? m_linearSampler : m_nearestSampler;
	SPT_CHECK(samplerHandle != VK_NULL_HANDLE);

	const ui::TextureID uiTexture = ImGui_ImplVulkan_AddTexture(samplerHandle, textureView.GetHandle(), VK_IMAGE_LAYOUT_GENERAL);

#if SPT_RHI_DEBUG
	const VkDescriptorSet* textureDS = reinterpret_cast<const VkDescriptorSet*>(&uiTexture);

	const lib::HashedString name = lib::String("UI Texture: ") + textureView.GetName().GetData();

	VkDebugUtilsObjectNameInfoEXT objectNameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType   = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    objectNameInfo.objectHandle = reinterpret_cast<uint64_t>(*textureDS);
	objectNameInfo.pObjectName  = name.GetData();

    SPT_VK_CHECK(vkSetDebugUtilsObjectNameEXT(rhi::VulkanRHI::GetDeviceHandle(), &objectNameInfo));
#endif // SPT_RHI_DEBUG

	return uiTexture;
}

VkDescriptorPool RHIUIBackend::InitializeDescriptorPool()
{
	SPT_PROFILER_FUNCTION();

	const VkDescriptorPoolCreateFlags flags = 0;

	const Uint32 poolSizeForDescriptorType = 128;

	const lib::StaticArray<VkDescriptorPoolSize, 1> poolSizes =
	{
		VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, poolSizeForDescriptorType },
	};

	constexpr Uint32 maxSetsNum = static_cast<Uint32>(poolSizes.size()) * poolSizeForDescriptorType;

	VkDescriptorPool poolHandle = VK_NULL_HANDLE;

	VkDescriptorPoolCreateInfo poolCreateInfo{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolCreateInfo.flags         = flags;
	poolCreateInfo.maxSets       = maxSetsNum;
	poolCreateInfo.poolSizeCount = static_cast<Uint32>(poolSizes.size());
	poolCreateInfo.pPoolSizes    = poolSizes.data();
	SPT_VK_CHECK(vkCreateDescriptorPool(VulkanRHI::GetDeviceHandle(), &poolCreateInfo, VulkanRHI::GetAllocationCallbacks(), &poolHandle));

	SPT_CHECK(poolHandle != VK_NULL_HANDLE);

	return poolHandle;
}

} // spt::vulkan
