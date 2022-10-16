#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIContextTypes.h"
#include "RHICore/RHIDescriptorTypes.h"
#include "Vulkan/Debug/DebugUtils.h"
#include "RHIDescriptorSet.h"


namespace spt::vulkan
{

class DescriptorPoolSet;


class RHI_API RHIContext
{
public:

	RHIContext();
	~RHIContext();

	RHIContext(RHIContext&& other);
	RHIContext& operator=(RHIContext&& rhs);

	void						InitializeRHI(const rhi::ContextDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	rhi::ContextID				GetID() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

	// Descriptor sets ======================================================

	/** Allocates dynamic descriptor sets bound to this context. These descriptors are automatically destroyed with this context */
	SPT_NODISCARD lib::DynamicArray<RHIDescriptorSet> AllocateDescriptorSets(const rhi::DescriptorSetLayoutID* layoutIDs, Uint32 descriptorSetsNum);

private:

	rhi::ContextID m_id;

	lib::UniquePtr<DescriptorPoolSet> m_dynamicDescriptorsPool;

	DebugName m_name;
};

} // spt::vulkan