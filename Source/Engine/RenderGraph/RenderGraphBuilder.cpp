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

void RenderGraphBuilder::AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();

	ResolveNodeDependecies(node, dependencies);

	m_nodes.emplace_back(&node);
}

void RenderGraphBuilder::ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies)
{
	ResolveNodeTextureAccesses(node, dependencies);
	ResolveNodeBufferAccesses(node, dependencies);
}

void RenderGraphBuilder::ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();

	for (const RGTextureAccessDef& textureAccessDef : dependencies.textureAccesses)
	{
		const RGTextureView& accessedTextureView = textureAccessDef.textureView;
		const RGTextureHandle accessedTexture = accessedTextureView.GetTexture();
		const rhi::TextureSubresourceRange& accessedSubresourceRange = accessedTextureView.GetViewDefinition().subresourceRange;

		if (accessedTexture->HasAcquiredNode())
		{
			accessedTexture->SetAcquireNode(&node);
		}

		RGTextureAccessState& textureAccessState = accessedTexture->GetAccessState();

		const rhi::BarrierTextureTransitionTarget& transitionTarget = GetTransitionDefForAccess(textureAccessDef.access);

		if (textureAccessState.IsFullResource())
		{
			const rhi::BarrierTextureTransitionTarget& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForFullResource().lastAccessType);
			node.AddTextureState(accessedTexture, accessedSubresourceRange, transitionSource, transitionTarget);
		}
		else
		{
			textureAccessState.ForEachSubresource(accessedSubresourceRange,
												  [&, this](RGTextureSubresource subresource)
												  {
													  const rhi::BarrierTextureTransitionTarget& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForSubresource(subresource).lastAccessType);

													  rhi::TextureSubresourceRange subresourceRange;
													  subresourceRange.aspect = accessedSubresourceRange.aspect;
													  subresourceRange.baseArrayLayer = subresource.arrayLayerIdx;
													  subresourceRange.arrayLayersNum = 1;
													  subresourceRange.baseMipLevel = subresource.mipMapIdx;
													  subresourceRange.mipLevelsNum = 1;

													  node.AddTextureState(accessedTexture, subresourceRange, transitionSource, transitionTarget);
												  });
		}

		textureAccessState.SetSubresourcesAccess(RGTextureSubresourceAccessState(textureAccessDef.access, &node), accessedSubresourceRange);
	}
}

void RenderGraphBuilder::ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();


}

} // spt::rg
