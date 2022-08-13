#pragma once

#include "RendererTypesMacros.h"
#include "SculptorCoreTypes.h"
#include "CurrentFrameContext.h"


namespace spt::renderer
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
	SPT_PROFILE_FUNCTION();

	if constexpr (deferredReleaseRHI)
	{
		CurrentFrameContext::GetCurrentFrameCleanupDelegate().AddLambda(
		[resource = m_rhiResource]() mutable
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