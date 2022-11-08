#pragma once

#include "SculptorCoreTypes.h"
#include "RGResources/RenderGraphResource.h"
#include "Types/Texture.h"

namespace spt::rg
{

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

	RenderGraphPersistentState() = default;

	RenderGraphResourcesPool m_resourcesPool;
};

} // spt::rg