#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIDescriptorSetDefinition.h"
#include "Vulkan/VulkanCore.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHI_API RHIDescriptorSetLayout
{
public:

	RHIDescriptorSetLayout();

	void InitializeRHI(const rhi::DescriptorSetDefinition& def);
	void ReleaseRHI();

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
