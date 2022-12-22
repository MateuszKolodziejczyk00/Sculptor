#pragma once

#include "SculptorCoreTypes.h"

namespace spt::rg
{

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

	TResourceType& operator*() const
	{
		return *m_resource;
	}

	void Reset()
	{
		m_resource = nullptr;
	}

private:

	TResourceType* m_resource;
};

class RGNode;
using RGNodeHandle = RGResourceHandle<RGNode>;

class RGTexture;
using RGTextureHandle = RGResourceHandle<RGTexture>;

class RGTextureView;
using RGTextureViewHandle = RGResourceHandle<RGTextureView>;

class RGBuffer;
using RGBufferHandle = RGResourceHandle<RGBuffer>;

class RGBufferView;
using RGBufferViewHandle = RGResourceHandle<RGBufferView>;

} // spt::rg


namespace std
{

template<typename TType>
struct hash<spt::rg::RGResourceHandle<TType>>
{
	size_t operator()(spt::rg::RGResourceHandle<TType> resource) const
	{
		SPT_STATIC_CHECK(sizeof(resource.Get()) == sizeof(size_t));
		return reinterpret_cast<size_t>(resource.Get());
	}
};

} // std