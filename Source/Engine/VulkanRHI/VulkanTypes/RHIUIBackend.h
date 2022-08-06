#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "UITypes.h"
#include "SculptorCoreTypes.h"
#include "DescriptorSetsManager/DescriptorPool.h"


namespace spt::vulkan
{

class RHIWindow;


class VULKAN_RHI_API RHIUIBackend
{
public:

	RHIUIBackend();

	void				InitializeRHI(ui::UIContext context, const RHIWindow& window);
	void				ReleaseRHI();

	Bool				IsValid() const;

private:

	void				InitializeDescriptorPool();

	ui::UIContext		m_context;

	DescriptorPool		m_uiDescriptorPool;
};

}