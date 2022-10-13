#pragma once

#include "RendererCoreMacros.h"
#include "SculptorCoreTypes.h"
#include "CurrentFrameContext.h"


namespace spt::rdr
{

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
	SPT_PROFILER_FUNCTION();

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

}