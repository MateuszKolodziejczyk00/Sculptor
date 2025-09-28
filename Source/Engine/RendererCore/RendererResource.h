#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "RHICore/RHIAllocationTypes.h"
#include "GPUReleaseQueue.h"


namespace spt::rdr
{

class GPUMemoryPool;


namespace RHITraits
{

template<typename TType>
constexpr bool ShouldUseDeferredRelease()
{
	return requires(const TType& res)
	{
		{ res.DeferredReleaseRHI() };
	};
}

} // RHITraits


class RendererResourceBase
{
protected:

	static void DeferRelease(GPUReleaseQueue::ReleaseEntry entry);
};


template<typename TRHIType, Bool deferredReleaseRHI = true>
class RendererResource : public RendererResourceBase
{
public:

	using RHIType  = TRHIType;
	using ThisType = RendererResource<RHIType, deferredReleaseRHI>;

	RendererResource()                    = default;
	RendererResource(const ThisType& rhs) = delete;
	RendererResource(ThisType&& rhs)      = delete;

	ThisType& operator=(const ThisType& rhs) = delete;
	ThisType& operator=(ThisType&& rhs)      = delete;
	
	~RendererResource();

	inline RHIType& GetRHI()
	{
		return m_rhiResource;
	}

	inline const RHIType& GetRHI() const
	{
		return m_rhiResource;
	}

protected:

	void ReleaseRHI();

private:

	RHIType m_rhiResource;
};


template<typename TRHIType, Bool deferredReleaseRHI /*= true*/>
inline RendererResource<TRHIType, deferredReleaseRHI>::~RendererResource()
{
	if (m_rhiResource.IsValid())
	{
		ReleaseRHI();
	}

	SPT_CHECK(!m_rhiResource.IsValid());
}

template<typename TRHIType, Bool deferredReleaseRHI /*= true*/>
inline void RendererResource<TRHIType, deferredReleaseRHI>::ReleaseRHI()
{
	if constexpr (RHITraits::ShouldUseDeferredRelease<TRHIType>())
	{
		DeferRelease(GPUReleaseQueue::ReleaseEntry::CreateLambda(
			[releaseTicket = std::move(m_rhiResource.DeferredReleaseRHI())]() mutable
			{
				releaseTicket.ExecuteReleaseRHI();
			}));
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
	Bool IsPlaced()    const;
	Bool IsNull()      const;

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