#include "RenderGraphResourcesPool.h"
#include "ResourcesManager.h"
#include "Renderer.h"


namespace spt::rg
{

RenderGraphResourcesPool& RenderGraphResourcesPool::Get()
{
	static RenderGraphResourcesPool instance;
	return instance;
}

void RenderGraphResourcesPool::OnBeginFrame()
{
	SPT_PROFILER_FUNCTION();

	std::for_each(std::begin(m_pooledTextures), std::end(m_pooledTextures),
				  [](PooledTexture& texture)
				  {
					  ++texture.unusedFramesNum;
				  });

	lib::RemoveAllBy(m_pooledTextures,
					 [](PooledTexture& texture)
					 {
						 constexpr Uint32 maxUnusedFrames = 4;
						 return texture.unusedFramesNum > maxUnusedFrames;
					 });
}

lib::SharedPtr<rdr::Texture> RenderGraphResourcesPool::AcquireTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& definition, const rhi::RHIAllocationInfo& allocationInfo)
{
	SPT_PROFILER_FUNCTION();

	const auto isTextureMatching = [&](const PooledTexture& pooledTexture) -> Bool
	{
		const lib::SharedPtr<rdr::Texture>& textureInstance = pooledTexture.texture;
		const rhi::RHITexture& rhiTexture					= textureInstance->GetRHI();
		const rhi::TextureDefinition& textureDef			= rhiTexture.GetDefinition();
		const rhi::RHIAllocationInfo& textureAllocationInfo	= rhiTexture.GetAllocationInfo();

		return textureDef.resolution.x() >= definition.resolution.x()
			&& textureDef.resolution.y() >= definition.resolution.y()
			&& textureDef.resolution.z() >= definition.resolution.z()
			&& lib::HasAllFlags(textureDef.usage, definition.usage)
			&& textureDef.format == definition.format
			&& textureDef.samples == definition.samples
			&& textureDef.mipLevels >= definition.mipLevels
			&& textureDef.arrayLayers >= definition.arrayLayers
			&& textureAllocationInfo.memoryUsage == allocationInfo.memoryUsage
			&& lib::HasAllFlags(textureAllocationInfo.allocationFlags, allocationInfo.allocationFlags);
	};

	const auto foundTextureIt = std::find_if(std::cbegin(m_pooledTextures), std::cend(m_pooledTextures), isTextureMatching);

	if (foundTextureIt != std::cend(m_pooledTextures))
	{
		lib::SharedPtr<rdr::Texture> texture = foundTextureIt->texture;
		SPT_CHECK(!!texture);

		m_pooledTextures.erase(foundTextureIt);

		return texture;
	}

	return rdr::ResourcesManager::CreateTexture(RENDERER_RESOURCE_NAME(name.Get()), definition, allocationInfo);
}

void RenderGraphResourcesPool::ReleaseTexture(lib::SharedPtr<rdr::Texture> texture)
{
	SPT_PROFILER_FUNCTION();

	m_pooledTextures.emplace_back(PooledTexture{ std::move(texture), 0 });
}

RenderGraphResourcesPool::RenderGraphResourcesPool()
{
	// This is singleton object so we can capture this safely
	rdr::Renderer::GetOnRendererCleanupDelegate().AddLambda([this]
															{
																DestroyResources();
															});
}

void RenderGraphResourcesPool::DestroyResources()
{
	m_pooledTextures.clear();
}

} // spt::rg
