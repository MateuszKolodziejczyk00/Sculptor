#pragma once

#include "RHIMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIQueryTypes.h"
#include "Vulkan/VulkanCore.h"


namespace spt::vulkan
{

class RHI_API RHIQueryPool
{
public:

	RHIQueryPool();

	void			InitializeRHI(const rhi::QueryPoolDefinition& definition);
	void			ReleaseRHI();

	Bool			IsValid() const;

	VkQueryPool		GetHandle() const;

	Uint32			GetQueryCount() const;

	void			Reset(Uint32 firstQuery, Uint32 queryCount);

	lib::DynamicArray<Uint64> GetResults(Uint32 queryCount) const;

private:

	VkQueryPool m_handle;

	rhi::QueryPoolDefinition m_definition;
};

} // spt::vulkan