#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "UITypes.h"
#include "SculptorCoreTypes.h"
#include "Vulkan/DescriptorSetsManager/DescriptorPool.h"


namespace spt::vulkan
{

class RHIWindow;


class RHI_API RHIUIBackend
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