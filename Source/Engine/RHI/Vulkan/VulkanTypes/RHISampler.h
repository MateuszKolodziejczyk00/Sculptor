#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISamplerTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class RHI_API RHISampler
{
public:

	RHISampler();

	void			InitializeRHI(const rhi::SamplerDefinition& def);
	void			ReleaseRHI();

	Bool			IsValid() const;

	VkSampler		GetHandle() const;

private:

	VkSampler m_handle;
};

} // spt::vulkan
