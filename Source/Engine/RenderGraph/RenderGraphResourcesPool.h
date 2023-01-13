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

	RenderGraphResourcesPool();

	void DestroyResources();

	struct PooledTexture
	{
		lib::SharedPtr<rdr::Texture> texture;
		Uint32 unusedFramesNum;
	};

	lib::DynamicArray<PooledTexture> m_pooledTextures;
};

} // spt::rg