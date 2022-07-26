#pragma once


#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include <atomic>


namespace spt::vulkan
{

class RHICommandPool
{
public:

	RHICommandPool();

	void						InitializeRHI(Uint32 queueFamilyIdx, VkCommandPoolCreateFlags flags);
	void						ReleaseRHI();

	Bool						IsValid() const;

	VkCommandPool				GetHandle() const;

	Bool						IsLocked() const;

	Bool						TryLock();
	void						Unlock();

private:

	VkCommandPool				m_poolHandle;

	/** Pool must be locked for the time when any thread is recording commands to buffer allocated from this pool */
	std::atomic<Bool>			m_isLocked;
};

}