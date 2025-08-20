#pragma once

#include "RendererCoreMacros.h"
#include "RHIBridge/RHITextureImpl.h"
#include "RHICore/RHIAllocationTypes.h"
#include "RendererResource.h"
#include "RendererUtils.h"
#include "DescriptorSetState/DescriptorTypes.h"


namespace spt::rhi
{
struct TextureViewDefinition;
}


namespace spt::rdr
{

class TextureView;
class GPUMemoryPool;


struct TextureViewDescriptorsAllocation
{
	ResourceDescriptorHandle srvDescriptor;
	ResourceDescriptorHandle uavDescriptor;
};


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

	lib::SharedRef<TextureView>		CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition = rhi::TextureViewDefinition(), TextureViewDescriptorsAllocation externalDescriptorsAllocation = TextureViewDescriptorsAllocation()) const;

private:

	lib::SharedPtr<GPUMemoryPool> m_owningMemoryPool;
};


class RENDERER_CORE_API TextureView : public RendererResource<rhi::RHITextureView>, public lib::SharedFromThis<TextureView>
{
protected:

	using ResourceType = RendererResource<rhi::RHITextureView>;

public:

	TextureView(const RendererResourceName& name, const lib::SharedRef<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition, TextureViewDescriptorsAllocation externalDescriptorsAllocation);
	~TextureView();

	const lib::SharedRef<Texture>& GetTexture() const;

	math::Vector3u GetResolution() const;
	math::Vector2u GetResolution2D() const;

	ResourceDescriptorIdx GetSRVDescriptor() const { return m_srvDescriptor; }
	ResourceDescriptorIdx GetUAVDescriptor() const { return m_uavDescriptor; }

	lib::SharedPtr<TextureView> AsShared() { return shared_from_this(); }

private:

	void CreateDescriptors(TextureViewDescriptorsAllocation externalDescriptorsAllocation);

	lib::SharedRef<Texture> m_texture;

	ResourceDescriptorHandle m_srvDescriptor;
	ResourceDescriptorHandle m_uavDescriptor;
};

} // spt::rdr
