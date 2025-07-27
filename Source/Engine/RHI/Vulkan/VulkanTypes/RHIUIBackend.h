#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "UIContext.h"
#include "UITypes.h"


namespace spt::vulkan
{

class RHIWindow;
class RHICommandBuffer;
class RHITextureView;
class RHISampler;


class RHI_API RHIUIBackend
{
public:

	RHIUIBackend();

	void				InitializeRHI(ui::UIContext context, const RHIWindow& window);
	void				ReleaseRHI();

	Bool				IsValid() const;

	void				InitializeFonts(const RHICommandBuffer& cmdBuffer);

	void				DestroyFontsTemporaryObjects();

	void				BeginFrame();

	void				Render(const RHICommandBuffer& cmdBuffer);

	ui::TextureID		GetUITexture(const RHITextureView& textureView, const RHISampler& sampler);

private:

	VkDescriptorPool	InitializeDescriptorPool();

	ui::UIContext m_context;

	lib::DynamicArray<VkDescriptorPool> m_uiDescriptorPools;
	SizeType m_lastPoolIdx;
};

} // spt::vulkan