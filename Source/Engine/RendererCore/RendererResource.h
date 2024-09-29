#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "CurrentFrameContext.h"
#include "RHICore/RHIAllocationTypes.h"


namespace spt::rdr
{

class GPUMemoryPool;


template<typename TRHIType, Bool deferredReleaseRHI = true>
class RendererResource
{
public:

	using RHIType	= TRHIType;
	using ThisType	= RendererResource<RHIType, deferredReleaseRHI>;

	RendererResource()							= default;
	RendererResource(const ThisType& rhs)		= delete;
	RendererResource(ThisType&& rhs)			= delete;

	ThisType& operator=(const ThisType& rhs)	= delete;
	ThisType& operator=(ThisType&& rhs)			= delete;
	
	~RendererResource();

	inline RHIType&			GetRHI()
	{
		return m_rhiResource;
	}

	inline const RHIType&	GetRHI() const
	{
		return m_rhiResource;
	}

private:

	RHIType		m_rhiResource;
};


template<typename TRHIType, Bool deferredReleaseRHI /*= true*/>
RendererResource<TRHIType, deferredReleaseRHI>::~RendererResource()
{
	SPT_CHECK(m_rhiResource.IsValid());

	if constexpr (deferredReleaseRHI)
	{
		CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = std::move(m_rhiResource)]() mutable
		{
			resource.ReleaseRHI();
		});
	}
	else
	{
		m_rhiResource.ReleaseRHI();
	}
}


struct NullAllocationDef { };


struct CommittedAllocationDef
{
	rhi::RHIAllocationInfo allocationInfo;
	Uint64                 alignment = 0;
};


struct PlacedAllocationDef
{
	explicit PlacedAllocationDef(lib::SharedRef<GPUMemoryPool> memoryPool, rhi::EVirtualAllocationFlags allocationFlags = rhi::EVirtualAllocationFlags::None)
		: memoryPool(memoryPool)
		, allocationFlags(allocationFlags)
	{ }

	lib::SharedRef<GPUMemoryPool> memoryPool;
	rhi::EVirtualAllocationFlags  allocationFlags;
};


class RENDERER_CORE_API AllocationDefinition
{
public:

	using AllocationDefinitionVariant = std::variant<NullAllocationDef, CommittedAllocationDef, PlacedAllocationDef>;

	AllocationDefinition();
	AllocationDefinition(const CommittedAllocationDef& allocationDef);
	AllocationDefinition(rhi::EMemoryUsage memoryUsage);
	AllocationDefinition(const rhi::RHIAllocationInfo& allocationInfo);
	AllocationDefinition(const PlacedAllocationDef& allocationDef);

	Bool IsCommitted() const;
	Bool IsPlaced()   const;
	Bool IsNull()     const;

	template<typename TAllocationDef>
	void SetAllocationDef(TAllocationDef allocationDef)
	{
		m_allocationDef = std::move(allocationDef);
	}

	const CommittedAllocationDef& GetCommittedAllocationDef() const;
	const PlacedAllocationDef&    GetPlacedAllocationDef()   const;

	const AllocationDefinitionVariant& GetAllocationDef() const;

	rhi::RHIResourceAllocationDefinition GetRHIAllocationDef() const;
	
private:

	AllocationDefinitionVariant m_allocationDef;
};

} // spt::rdr