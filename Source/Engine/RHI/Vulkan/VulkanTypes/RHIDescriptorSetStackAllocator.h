#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHIDescriptorSet.h"
#include "RHIDescriptorSetLayout.h"


namespace spt::vulkan
{

class DescriptorPoolSet;


class RHI_API RHIDescriptorSetStackAllocator
{
public:

	RHIDescriptorSetStackAllocator();

	void InitializeRHI();
	void ReleaseRHI();

	Bool IsValid() const;

	RHIDescriptorSet AllocateDescriptorSet(const RHIDescriptorSetLayout& layout);

	void                     SetName(const lib::HashedString& name);
	const lib::HashedString& GetName() const;

private:

	DescriptorPoolSet* m_poolSet;

	DebugName m_name;
};

} // spt::vulkan
