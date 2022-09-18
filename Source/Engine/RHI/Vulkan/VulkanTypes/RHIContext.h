#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIContextTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


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

private:

	rhi::ContextID m_id;

	lib::UniquePtr<DescriptorPoolSet> m_dynamicDescriptorsPool;

	DebugName m_name;
};

} // spt::vulkan