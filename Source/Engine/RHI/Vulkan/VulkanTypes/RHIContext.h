#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIContextTypes.h"
#include "Vulkan/Debug/DebugUtils.h"


namespace spt::vulkan
{

class RHI_API RHIContext
{
public:

	RHIContext();

	void						InitializeRHI(const rhi::ContextDefinition& definition);
	void						ReleaseRHI();

	Bool						IsValid() const;

	rhi::ContextID				GetID() const;

	void						SetName(const lib::HashedString& name);
	const lib::HashedString&	GetName() const;

private:

	rhi::ContextID	m_id;

	DebugName		m_name;
};

} // spt::vulkan