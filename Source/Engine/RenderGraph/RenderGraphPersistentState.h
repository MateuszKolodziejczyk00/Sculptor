#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RenderGraphResource.h"
#include "Types/Texture.h"

namespace spt::rg
{

static_assert(std::is_integral_v<RGResourceID>);

class RenderGraphResourcesPool
{
public:

	RenderGraphResourcesPool();

	lib::SharedPtr<rdr::Texture> AcquireTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseTexture(lib::SharedPtr<rdr::Texture> texture);

private:

	struct PooledTexture
	{
		lib::SharedPtr<rdr::Texture> texture;
	};

	lib::DynamicArray<PooledTexture> m_pooledTextures;
};


class RenderGraphPersistentState
{
public:

	static RGResourceID GetAvailableResourceID()
	{
		const RGResourceID newResourceID = GetInstance().m_idCounter.fetch_add(1);
		SPT_CHECK(newResourceID != idxNone<RGResourceID>);
		return newResourceID;
	}

	static RenderGraphResourcesPool& GetResourcesPool()
	{
		return GetInstance().m_resourcesPool;
	}

private:

	static RenderGraphPersistentState& GetInstance()
	{
		static RenderGraphPersistentState instance;
		return instance;
	}

	RenderGraphPersistentState()
		: m_idCounter(0)
	{ }

	RenderGraphResourcesPool m_resourcesPool;

	std::atomic<RGResourceID> m_idCounter;
};

} // spt::rg