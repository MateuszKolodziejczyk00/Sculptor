#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include "RHISemaphoreTypes.h"
#include "Debug/DebugUtils.h"


namespace spt::vulkan
{

class VULKAN_RHI_API RHISemaphore
{
public:

	RHISemaphore();

	void						InitializeRHI(const rhi::SemaphoreDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	Uint64						GetValue() const;
	void						Signal(Uint64 value);

	Bool						Wait(Uint64 value, Uint64 timeout = maxValue<Uint64>) const;

	VkSemaphore					GetHandle() const;
	rhi::ESemaphoreType			GetType() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

private:

	VkSemaphore					m_semaphore;

	rhi::ESemaphoreType			m_type;

	DebugName					m_name;
};


class VULKAN_RHI_API RHISemaphoresArray
{
public:

	RHISemaphoresArray();

	void									AddBinarySemaphore(const RHISemaphore& semaphore);
	void									AddTimelineSemaphore(const RHISemaphore& semaphore, Uint64 value);

	const lib::DynamicArray<VkSemaphore>&	GetSemaphores() const;

	const lib::DynamicArray<Uint64>&		GetValues() const;

private:

	lib::DynamicArray<VkSemaphore>			m_semaphores;

	// wait values for timeline semaphores
	lib::DynamicArray<Uint64>				m_values;

};

}