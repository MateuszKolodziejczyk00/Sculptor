#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISamplerTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

struct RHI_API RHISamplerReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkSampler> handle;
};


class RHI_API RHISampler
{
public:

	RHISampler();

	void InitializeRHI(const rhi::SamplerDefinition& def);
	void ReleaseRHI();

	RHISamplerReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	void CopyDescriptor(Byte* dst) const;

	VkSampler GetHandle() const;

private:

	VkSampler m_handle;
};

} // spt::vulkan
