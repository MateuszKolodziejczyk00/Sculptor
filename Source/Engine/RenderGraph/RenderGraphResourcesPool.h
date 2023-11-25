#pragma once

#include "SculptorCoreTypes.h"
#include "RenderGraphTypes.h"
#include "Types/Texture.h"

namespace spt::rg
{

class RenderGraphResourcesPool
{
public:

	static RenderGraphResourcesPool& Get();

	void OnBeginFrame();

	lib::SharedPtr<rdr::Texture> AcquireTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo);
	void ReleaseTexture(lib::SharedPtr<rdr::Texture> texture);

private:

	struct MemoryPoolData
	{
		lib::SharedPtr<rdr::GPUMemoryPool> memoryPool;

		Uint64 currentMemoryUsage = 0u;
		Uint64 maxMemoryUsage     = 0u;
	};

	RenderGraphResourcesPool();

	MemoryPoolData& GetMemoryPoolData(const rhi::RHIAllocationInfo& allocationInfo);

	void DestroyResources();

	lib::HashMap<rhi::EMemoryUsage, MemoryPoolData> m_memoryPools;
};

} // spt::rg