#include "RHISemaphore.h"
#include "VulkanRHI.h"

namespace spt::vulkan
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Helpers =======================================================================================

namespace priv
{

VkSemaphoreType GetVulkanSemaphoreType(rhi::ESemaphoreType type)
{
	switch (type)
	{
	case rhi::ESemaphoreType::Binary:			return VK_SEMAPHORE_TYPE_BINARY;
	case rhi::ESemaphoreType::Timeline:			return VK_SEMAPHORE_TYPE_TIMELINE;
	}

	SPT_CHECK_NO_ENTRY();
	return VK_SEMAPHORE_TYPE_BINARY;
}

}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RHISemaphore ==================================================================================

RHISemaphore::RHISemaphore()
	: m_semaphore(VK_NULL_HANDLE)
{ }

void RHISemaphore::InitializeRHI(const rhi::SemaphoreDefinition& definition)
{
	SPT_PROFILE_FUNCTION();

	VkSemaphoreTypeCreateInfo semaphoreTypeInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO };
    semaphoreTypeInfo.semaphoreType = priv::GetVulkanSemaphoreType(definition.m_type);
    semaphoreTypeInfo.initialValue = definition.m_initialValue;

	VkSemaphoreCreateInfo semaphoreInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	semaphoreInfo.pNext = &semaphoreTypeInfo;

	SPT_VK_CHECK(vkCreateSemaphore(VulkanRHI::GetDeviceHandle(), &semaphoreInfo, VulkanRHI::GetAllocationCallbacks(), &m_semaphore));

	m_type = definition.m_type;
}

void RHISemaphore::ReleaseRHI()
{
	SPT_PROFILE_FUNCTION();

	if (m_semaphore)
	{
		vkDestroySemaphore(VulkanRHI::GetDeviceHandle(), m_semaphore, VulkanRHI::GetAllocationCallbacks());
		m_semaphore = VK_NULL_HANDLE;
	}
}

Bool RHISemaphore::IsValid() const
{
	return m_semaphore != VK_NULL_HANDLE;
}

Uint64 RHISemaphore::GetValue() const
{
	SPT_PROFILE_FUNCTION();
	SPT_CHECK(IsValid());

	SPT_CHECK(m_type == rhi::ESemaphoreType::Timeline);

	Uint64 result = 0;
	vkGetSemaphoreCounterValue(VulkanRHI::GetDeviceHandle(), m_semaphore, &result);
	return result;
}

void RHISemaphore::Signal(Uint64 value)
{
	SPT_PROFILE_FUNCTION();
	SPT_CHECK(IsValid());

	SPT_CHECK(m_type == rhi::ESemaphoreType::Timeline);

	VkSemaphoreSignalInfo signalInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO };
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;

	vkSignalSemaphore(VulkanRHI::GetDeviceHandle(), &signalInfo);
}

Bool RHISemaphore::Wait(Uint64 value, Uint64 timeout /*= maxValue<Uint64>*/) const
{
	SPT_PROFILE_FUNCTION();
	SPT_CHECK(IsValid());

	SPT_CHECK(m_type == rhi::ESemaphoreType::Timeline);

	VkSemaphoreWaitInfo waitInfo{ VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO };
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;

	const VkResult result = vkWaitSemaphores(VulkanRHI::GetDeviceHandle(), &waitInfo, timeout);

	return result == VK_SUCCESS;
}

VkSemaphore RHISemaphore::GetHandle() const
{
	return m_semaphore;
}

rhi::ESemaphoreType RHISemaphore::GetType() const
{
	return m_type;
}

void RHISemaphore::SetName(const lib::HashedString& name)
{
	m_name.Set(name, reinterpret_cast<Uint64>(m_semaphore), VK_OBJECT_TYPE_SEMAPHORE);
}

const lib::HashedString& RHISemaphore::GetName() const
{
	return m_name.Get();
}

}
