#include "Texture.h"
#include "GPUMemoryPool.h"
#include "GPUApi.h"
#include "DescriptorSetState/DescriptorManager.h"

namespace spt::rdr
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

Texture::Texture(const RendererResourceName& name, const rhi::TextureDefinition& textureDefinition, const AllocationDefinition& allocationDefinition)
{
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

lib::SharedRef<TextureView> Texture::CreateView(const RendererResourceName& name, const rhi::TextureViewDefinition& viewDefinition /*= rhi::TextureViewDefinition()*/, TextureViewDescriptorsAllocation externalDescriptorsAllocation /*= TextureViewDescriptorsAllocation()*/) const
{
	return lib::Ref(std::make_shared<TextureView>(name, lib::Ref(const_cast<Texture*>(this)->shared_from_this()), viewDefinition, std::move(externalDescriptorsAllocation)));
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// Texture =======================================================================================

TextureView::TextureView(const RendererResourceName& name, const lib::SharedRef<Texture>& texture, const rhi::TextureViewDefinition& viewDefinition, TextureViewDescriptorsAllocation externalDescriptorsAllocation)
	: m_texture(texture)
{
	const rhi::RHITexture& rhiTexture = texture->GetRHI();

	GetRHI().InitializeRHI(rhiTexture, viewDefinition);
	GetRHI().SetName(name.Get());

	CreateDescriptors(std::move(externalDescriptorsAllocation));
}

TextureView::~TextureView()
{
	// It's important to make sure that view will be destroyed before the texture if this view holds last ref
	// because of that view rhi must be destroyed before calling destructor of members

	DescriptorManager& descriptorManager = rdr::GPUApi::GetDescriptorManager();

	if (m_srvDescriptor.IsValid())
	{
		descriptorManager.ClearDescriptorInfo(m_srvDescriptor);
	}

	if (m_uavDescriptor.IsValid())
	{
		descriptorManager.ClearDescriptorInfo(m_uavDescriptor);
	}

	DeferRelease(GPUReleaseQueue::ReleaseEntry::CreateLambda(
		[releaseTicket = std::move(GetRHI().DeferredReleaseRHI()), srvDescriptor = std::move(m_srvDescriptor), uavDescriptor = std::move(m_uavDescriptor)]() mutable
		{
			DescriptorManager& descriptorManager = rdr::GPUApi::GetDescriptorManager();

			if (srvDescriptor.IsValid())
			{
				descriptorManager.FreeResourceDescriptor(std::move(srvDescriptor));
			}
			if (uavDescriptor.IsValid())
			{
				descriptorManager.FreeResourceDescriptor(std::move(uavDescriptor));
			}

			releaseTicket.ExecuteReleaseRHI();
		}));
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

void TextureView::CreateDescriptors(TextureViewDescriptorsAllocation externalDescriptorsAllocation)
{
	DescriptorManager& descriptorManager = rdr::GPUApi::GetDescriptorManager();

	if (GetRHI().GetTexture()->HasUsage(rhi::ETextureUsage::SampledTexture))
	{
		m_srvDescriptor = externalDescriptorsAllocation.srvDescriptor.IsValid() ? std::move(externalDescriptorsAllocation.srvDescriptor) : descriptorManager.AllocateResourceDescriptor();
		descriptorManager.UploadSRVDescriptor(m_srvDescriptor, *this);
	}
	else if (externalDescriptorsAllocation.srvDescriptor.IsValid()) // Free descriptor if it's allocated externally (f.e. by render graph) but can't be used
	{
		descriptorManager.FreeResourceDescriptor(std::move(externalDescriptorsAllocation.srvDescriptor));
	}

	if (GetRHI().GetTexture()->HasUsage(rhi::ETextureUsage::StorageTexture))
	{
		m_uavDescriptor = externalDescriptorsAllocation.uavDescriptor.IsValid() ? std::move(externalDescriptorsAllocation.uavDescriptor) : descriptorManager.AllocateResourceDescriptor();
		descriptorManager.UploadUAVDescriptor(m_uavDescriptor, *this);
	}
	else if (externalDescriptorsAllocation.uavDescriptor.IsValid()) // Free descriptor if it's allocated externally (f.e. by render graph) but can't be used
	{
		descriptorManager.FreeResourceDescriptor(std::move(externalDescriptorsAllocation.uavDescriptor));
	}
}

} // spt::rdr
