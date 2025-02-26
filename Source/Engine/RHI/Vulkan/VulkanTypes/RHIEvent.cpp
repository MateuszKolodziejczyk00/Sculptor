#include "Vulkan/VulkanTypes/RHIEvent.h"
#include "Vulkan/VulkanRHIUtils.h"
#include "Vulkan/VulkanRHI.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIEventReleaseTicket =========================================================================

void RHIEventReleaseTicket::ExecuteReleaseRHI()
{
	if (handle.IsValid())
	{
		vkDestroyEvent(VulkanRHI::GetDeviceHandle(), handle.GetValue(), VulkanRHI::GetAllocationCallbacks());
		handle.Reset();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHIEvent ======================================================================================

RHIEvent::RHIEvent()
	: m_handle(VK_NULL_HANDLE)
{ }

void RHIEvent::InitializeRHI(const rhi::EventDefinition& definition)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!IsValid());

	VkEventCreateInfo eventInfo{ VK_STRUCTURE_TYPE_EVENT_CREATE_INFO };
	eventInfo.flags = RHIToVulkan::GetEventFlags(definition.flags);

	SPT_VK_CHECK(vkCreateEvent(VulkanRHI::GetDeviceHandle(), &eventInfo, VulkanRHI::GetAllocationCallbacks(), &m_handle));

	SPT_CHECK(IsValid());
}

void RHIEvent::ReleaseRHI()
{
	RHIEventReleaseTicket releaseTicket = DeferredReleaseRHI();
	releaseTicket.ExecuteReleaseRHI();
}

RHIEventReleaseTicket RHIEvent::DeferredReleaseRHI()
{
	SPT_CHECK(!IsValid());

	RHIEventReleaseTicket releaseTicket;
	releaseTicket.handle = m_handle;

#if SPT_RHI_DEBUG
	releaseTicket.name = GetName();
#endif // SPT_RHI_DEBUG

	m_name.Reset(reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_EVENT);
	m_handle = VK_NULL_HANDLE;

	SPT_CHECK(IsValid());

	return releaseTicket;
}

Bool RHIEvent::IsValid() const
{
	return m_handle != VK_NULL_HANDLE;
}

void RHIEvent::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_handle), VK_OBJECT_TYPE_EVENT);
}

const lib::HashedString& RHIEvent::GetName() const
{
	return m_name.Get();
}

void RHIEvent::SetEvent()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkSetEvent(VulkanRHI::GetDeviceHandle(), m_handle);
}

void RHIEvent::ResetEvent()
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	vkResetEvent(VulkanRHI::GetDeviceHandle(), m_handle);
}

Bool RHIEvent::IsSignaled() const
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(IsValid());

	const VkResult status = vkGetEventStatus(VulkanRHI::GetDeviceHandle(), m_handle);

	return status == VK_EVENT_SET;
}

VkEvent RHIEvent::GetHandle() const
{
	return m_handle;
}

} // spt::vulkan
