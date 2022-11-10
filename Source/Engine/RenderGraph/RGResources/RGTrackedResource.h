#pragma once

#include "RenderGraphMacros.h"
#include "SculptorCoreTypes.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"


namespace spt::rg
{

class RENDER_GRAPH_API RGTrackedResource
{
public:

	RGTrackedResource() = default;
	virtual ~RGTrackedResource() = default;
};


template<typename TResourceType>
class RGResourceHandle
{
public:

	RGResourceHandle()
		: m_resource(nullptr)
	{ }

	RGResourceHandle(TResourceType* resource)
		: m_resource(resource)
	{ }

	Bool IsValid() const
	{
		return !!m_resource;
	}

	TResourceType* Get() const
	{
		return m_resource;
	}

	TResourceType* operator->() const
	{
		return m_resource;
	}

	void Reset()
	{
		m_resource = nullptr;
	}

private:

	TResourceType* m_resource;
};

} // spt::rg
