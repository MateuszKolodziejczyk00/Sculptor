#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIDescriptorSetDefinition.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

struct RHI_API RHIDescriptorSetLayoutReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkDescriptorSetLayout> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIDescriptorSetLayout
{
public:

	RHIDescriptorSetLayout();

	void InitializeRHI(const rhi::DescriptorSetDefinition& def);
	void ReleaseRHI();

	RHIDescriptorSetLayoutReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;

	SizeType GetHash() const;

	VkDescriptorSetLayout GetHandle() const;

private:

	VkDescriptorSetLayout m_handle;

	DebugName m_name;
};

} // spt::vulkan
