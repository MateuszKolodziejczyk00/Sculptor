#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHISynchronizationTypes.h"
#include "Vulkan/Debug/DebugUtils.h"

namespace spt::vulkan
{

struct RHI_API RHIEventReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkEvent> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIEvent
{
public:

	RHIEvent();

	void						InitializeRHI(const rhi::EventDefinition& definition);
	void						ReleaseRHI();

	RHIEventReleaseTicket		DeferredReleaseRHI();

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
