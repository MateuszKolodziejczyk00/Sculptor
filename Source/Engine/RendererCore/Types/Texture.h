#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHITextureImpl.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"


namespace spt::rhi
{
struct TextureViewDefinition;
}


namespace spt::rdr
{

class TextureView;
class GPUMemoryPool;


class RENDERER_CORE_API Texture : public RendererResource<rhi::RHITexture>, public lib::SharedFromThis<Texture>, private rhi::RHITextureMemoryOwner
{
protected:

	using ResourceType = RendererResource<rhi::RHITexture>;

public:

	Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition);
	Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture);

	Bool HasBoundMemory() const;
	void BindMemory(const AllocationDefinition& definition);
	void ReleasePlacedAllocation();

	void Rename(const RendererResourceName& name);

	const rhi::TextureDefinition&	GetDefinition() const;
	const math::Vector3u&			GetResolution() const;
	math::Vector2u					GetResolution2D() const;

	lib::SharedRef<TextureView>		CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition = rhi::TextureViewDefinition()) const;

private:

	lib::SharedPtr<GPUMemoryPool> m_owningMemoryPool;
};


class RENDERER_CORE_API TextureView : public RendererResource<rhi::RHITextureView>
{
protected:

	using ResourceType = RendererResource<rhi::RHITextureView>;

public:

	TextureView(const RendererResourceName& name, const lib::SharedRef<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition);
	~TextureView();

	const lib::SharedRef<Texture>& GetTexture() const;

	math::Vector3u GetResolution() const;
	math::Vector2u GetResolution2D() const;

private:

	lib::SharedRef<Texture> m_texture;
};

} // spt::rdr
