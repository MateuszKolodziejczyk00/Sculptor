#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIDescriptorSet.h"
#include "RHIDescriptorSetLayout.h"


namespace spt::vulkan
{

class DescriptorPoolSet;


struct RHI_API RHIDescriptorSetStackAllocatorReleaseTicket
{
	void ExecuteReleaseRHI();

	RHIResourceReleaseTicket<DescriptorPoolSet*> handle;

#if SPT_RHI_DEBUG
	lib::HashedString name;
#endif // SPT_RHI_DEBUG
};


class RHI_API RHIDescriptorSetStackAllocator
{
public:

	RHIDescriptorSetStackAllocator();

	void InitializeRHI();
	void ReleaseRHI();

	RHIDescriptorSetStackAllocatorReleaseTicket DeferredReleaseRHI();

	Bool IsValid() const;

	RHIDescriptorSet AllocateDescriptorSet(const RHIDescriptorSetLayout& layout);

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;

private:

	DescriptorPoolSet* m_poolSet;

	DebugName m_name;
};

} // spt::vulkan
