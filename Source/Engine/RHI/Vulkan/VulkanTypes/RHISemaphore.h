#pragma once

#include "RHIMacros.h"
#include "Vulkan/VulkanCore.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIPipelineTypes.h"
#include "RHICore/RHISemaphoreTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

struct RHI_API RHISemaphoreReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<VkSemaphore> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHISemaphore
{
public:

	RHISemaphore();

	void						InitializeRHI(const rhi::SemaphoreDefinition& definition);
	void						ReleaseRHI();

	RHISemaphoreReleaseTicket	DeferredReleaseRHI();

	Bool						IsValid() const;

	Uint64						GetValue() const;
	Bool						Wait(Uint64 value, Uint64 timeout = maxValue<Uint64>) const;

	void						Signal(Uint64 value);

	rhi::ESemaphoreType			GetType() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Vulkan =====================================================================

	VkSemaphore					GetHandle() const;

private:

	VkSemaphore					m_semaphore;

	rhi::ESemaphoreType			m_type;

	DebugName					m_name;
};


class RHI_API RHISemaphoresArray
{
public:

	RHISemaphoresArray();

	void												AddBinarySemaphore(const RHISemaphore& semaphore, rhi::EPipelineStage submitStage);
	void												AddTimelineSemaphore(const RHISemaphore& semaphore, Uint64 value, rhi::EPipelineStage submitStage);

	SizeType											GetSemaphoresNum() const;

	void												Reset();

	void												Append(const RHISemaphoresArray& other);

	// Vulkan =====================================================================

	const lib::DynamicArray<VkSemaphoreSubmitInfo>&		GetSubmitInfos() const;

private:

	void												AddSemaphoreInfo(VkSemaphore semaphore, Uint64 value, rhi::EPipelineStage submitStage);

	lib::DynamicArray<VkSemaphoreSubmitInfo>			m_submitInfos;

};

} // spt::vulkan