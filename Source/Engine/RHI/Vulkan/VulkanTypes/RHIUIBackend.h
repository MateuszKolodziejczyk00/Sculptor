#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "UITypes.h"
#include "SculptorCoreTypes.h"
#include "Vulkan/DescriptorSets/DescriptorPool.h"


namespace spt::vulkan
{

class RHIWindow;
class RHICommandBuffer;


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

private:

	void				InitializeDescriptorPool();

	ui::UIContext		m_context;

	DescriptorPool		m_uiDescriptorPool;
};

}