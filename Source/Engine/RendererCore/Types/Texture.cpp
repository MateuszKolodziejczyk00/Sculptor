#include "Texture.h"
#include "GPUMemoryPool.h"


namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

Texture::Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
	SPT_PROFILER_FUNCTION();

	GetRHI().InitializeRHI(textureDefinition, allocationDefinition.GetRHIAllocationDef());
	GetRHI().SetName(name.Get());

	if (allocationDefinition.IsPlaced())
	{
		m_owningMemoryPool = allocationDefinition.GetPlacedAllocationDef().memoryPool;
	}
}

Texture::Texture(const RendererResourceName& name, const rhi::RHITexture& rhiTexture)
{
	SPT_CHECK(rhiTexture.IsValid());

	GetRHI() = rhiTexture;
	GetRHI().SetName(name.Get());
}

Bool Texture::HasBoundMemory() const
{
	return GetRHI().HasBoundMemory();
}

void Texture::BindMemory(const AllocationDefinition& definition)
{
	SPT_CHECK(!HasBoundMemory());

	if (definition.IsPlaced())
	{
		m_owningMemoryPool = definition.GetPlacedAllocationDef().memoryPool;
	}
	RHITextureMemoryOwner::BindMemory(GetRHI(), definition.GetRHIAllocationDef());
}

void Texture::ReleasePlacedAllocation()
{
	SPT_CHECK(HasBoundMemory());
	SPT_CHECK(!!m_owningMemoryPool);

	rhi::RHIGPUMemoryPool& memoryPool = m_owningMemoryPool->GetRHI();

	const rhi::RHIResourceAllocationHandle allocation = RHITextureMemoryOwner::ReleasePlacedAllocation(GetRHI());

	SPT_CHECK(std::holds_alternative<rhi::RHIPlacedAllocation>(allocation) || std::holds_alternative<rhi::RHINullAllocation>(allocation))
	if (std::holds_alternative<rhi::RHIPlacedAllocation>(allocation))
	{
		memoryPool.Free(std::get<rhi::RHIPlacedAllocation>(allocation).GetSuballocation());
	}

	m_owningMemoryPool.reset();
}

void Texture::Rename(const RendererResourceName& name)
{
	GetRHI().SetName(name.Get());
}

const rhi::TextureDefinition& Texture::GetDefinition() const
{
	return GetRHI().GetDefinition();
}

const math::Vector3u& Texture::GetResolution() const
{
	return GetRHI().GetResolution();
}

math::Vector2u Texture::GetResolution2D() const
{
	return GetResolution().head<2>();
}

lib::SharedRef<TextureView> Texture::CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition /*= rhi::TextureViewDefinition()*/) const
{
	SPT_PROFILER_FUNCTION();

	return lib::Ref(std::make_shared<TextureView>(name, lib::Ref(const_cast<Texture*>(this)->shared_from_this()), viewDefinition));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

TextureView::TextureView(const RendererResourceName& name, const lib::SharedRef<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition)
	: m_texture(texture)
{
	SPT_PROFILER_FUNCTION();

	const rhi::RHITexture& rhiTexture = texture->GetRHI();

	GetRHI().InitializeRHI(rhiTexture, viewDefinition);
	GetRHI().SetName(name.Get());
}

const lib::SharedRef<Texture>& TextureView::GetTexture() const
{
	return m_texture;
}

math::Vector3u TextureView::GetResolution() const
{
	return GetRHI().GetResolution();
}

math::Vector2u TextureView::GetResolution2D() const
{
	return GetResolution().head<2>();
}

} // spt::rdr
