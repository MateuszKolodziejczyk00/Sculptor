#include "RenderGraphBuilder.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"

namespace spt::rg
{

RenderGraphBuilder::RenderGraphBuilder()
{ }

RGTextureHandle RenderGraphBuilder::AcquireExternalTexture(lib::SharedPtr<rdr::Texture> texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!texture);

	const RenderGraphDebugName name = RG_DEBUG_NAME(texture->GetRHI().GetName());

	RGResourceDef definition;
	definition.name = name;
	definition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);

	RGTextureHandle textureHandle = allocator.Allocate<RGTexture>(definition, texture);

	m_externalTextures.emplace(std::move(texture), textureHandle);

	return textureHandle;
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	RGTextureHandle textureHandle = allocator.Allocate<RGTexture>(definition, textureDefinition, allocationInfo);

	return textureHandle;
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination)
{
	SPT_PROFILER_FUNCTION()

	textureHandle->SetExtractionDestination(extractDestination);

	m_extractedTextures.emplace_back(textureHandle);
}

const rhi::BarrierTextureTransitionTarget& RenderGraphBuilder::GetTransitionDefForAccess(ERGAccess access) const
{
	SPT_CHECK_NO_ENTRY();
	return rhi::TextureTransition::Undefined;
}

} // spt::rg
