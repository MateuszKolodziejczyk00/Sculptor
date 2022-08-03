#pragma once

#include "VulkanRHIMacros.h"
#include "Vulkan.h"
#include "SculptorCoreTypes.h"
#include "RHIPipelineTypes.h"
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


class VULKAN_RHI_API RHISemaphoresArray
{
public:

	RHISemaphoresArray();

	void												AddBinarySemaphore(const RHISemaphore& semaphore, rhi::EPipelineStage::Flags submitStage);
	void												AddTimelineSemaphore(const RHISemaphore& semaphore, rhi::EPipelineStage::Flags submitStage, Uint64 value);

	SizeType											GetSemaphoresNum() const;

	// Vulkan =====================================================================

	const lib::DynamicArray<VkSemaphoreSubmitInfo>&		GetSubmitInfos() const;

private:

	void												AddSemaphoreInfo(VkSemaphore semaphore, rhi::EPipelineStage::Flags submitStage, Uint64 value);

	lib::DynamicArray<VkSemaphoreSubmitInfo>			m_submitInfos;

};

}