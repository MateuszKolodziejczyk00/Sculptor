#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "Vulkan/Debug/DebugUtils.h"

namespace spt::vulkan
{

class RHI_API RHIEvent
{
public:

	RHIEvent();

	void						InitializeRHI(const rhi::EventDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	void						SetEvent();
	void						ResetEvent();

	Bool						IsSignaled() const;

	VkEvent						GetHandle() const;

private:

	VkEvent m_handle;

	DebugName m_name;
};

} // spt::vulkan
