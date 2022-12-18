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

	m_textures.emplace_back(textureHandle);

	return textureHandle;
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	RGTextureHandle textureHandle = allocator.Allocate<RGTexture>(definition, textureDefinition, allocationInfo);

	m_textures.emplace_back(textureHandle);

	return textureHandle;
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination)
{
	SPT_PROFILER_FUNCTION()

	textureHandle->SetExtractionDestination(extractDestination);

	m_extractedTextures.emplace_back(textureHandle);
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination, const rhi::TextureSubresourceRange& transitionRange, const rhi::BarrierTextureTransitionDefinition& preExtractionTransitionTarget)
{
	SPT_PROFILER_FUNCTION()

	textureHandle->SetExtractionDestination(extractDestination, transitionRange, preExtractionTransitionTarget);

	m_extractedTextures.emplace_back(textureHandle);
}

void RenderGraphBuilder::Execute()
{
	PostBuild();

	Compile();

	ExecuteGraph();
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
		const RGTextureViewHandle accessedTextureView					= textureAccessDef.textureView;
		const RGTextureHandle accessedTexture							= accessedTextureView->GetTexture();
		const rhi::TextureSubresourceRange& accessedSubresourceRange	= accessedTextureView->GetViewDefinition().subresourceRange;

		if (!accessedTexture->IsExternal())
		{
			if (accessedTexture->HasAcquireNode())
			{
				accessedTexture->SetAcquireNode(&node);
				node.AddTextureToAcquire(accessedTexture);
			}
		}

		const rhi::BarrierTextureTransitionDefinition& transitionTarget = GetTransitionDefForAccess(textureAccessDef.access);

		AppendTextureTransitionToNode(node, accessedTexture, accessedSubresourceRange, transitionTarget);

		RGTextureAccessState& textureAccessState = accessedTexture->GetAccessState();
		textureAccessState.SetSubresourcesAccess(RGTextureSubresourceAccessState(textureAccessDef.access, &node), accessedSubresourceRange);
	}
}

void RenderGraphBuilder::AppendTextureTransitionToNode(RGNode& node, RGTextureHandle accessedTexture, const rhi::TextureSubresourceRange& accessedSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	RGTextureAccessState& textureAccessState = accessedTexture->GetAccessState();

	if (textureAccessState.IsFullResource())
	{
		const rhi::BarrierTextureTransitionDefinition& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForFullResource().lastAccessType);
		node.AddTextureState(accessedTexture, accessedSubresourceRange, transitionSource, transitionTarget);
	}
	else
	{
		textureAccessState.ForEachSubresource(accessedSubresourceRange,
											  [&, this](RGTextureSubresource subresource)
											  {
												  const rhi::BarrierTextureTransitionDefinition& transitionSource = GetTransitionDefForAccess(textureAccessState.GetForSubresource(subresource).lastAccessType);

												  rhi::TextureSubresourceRange subresourceRange;
												  subresourceRange.aspect			= accessedSubresourceRange.aspect;
												  subresourceRange.baseArrayLayer	= subresource.arrayLayerIdx;
												  subresourceRange.arrayLayersNum	= 1;
												  subresourceRange.baseMipLevel		= subresource.mipMapIdx;
												  subresourceRange.mipLevelsNum		= 1;

												  node.AddTextureState(accessedTexture, subresourceRange, transitionSource, transitionTarget);
											  });
	}
}

void RenderGraphBuilder::ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();


}

const rhi::BarrierTextureTransitionDefinition& RenderGraphBuilder::GetTransitionDefForAccess(ERGAccess access) const
{
	SPT_CHECK_NO_ENTRY();
	return rhi::TextureTransition::Undefined;
}

void RenderGraphBuilder::PostBuild()
{
	SPT_PROFILER_FUNCTION();

	AddPrepareTexturesForExtractionNode();

	ResolveTextureReleases();
}

void RenderGraphBuilder::Compile()
{
	SPT_PROFILER_FUNCTION();

	
}

void RenderGraphBuilder::ExecuteGraph()
{
	SPT_PROFILER_FUNCTION();


}

void RenderGraphBuilder::AddPrepareTexturesForExtractionNode()
{
	SPT_PROFILER_FUNCTION();

	RGEmptyNode& barrierNode = AllocateNode<RGEmptyNode>(RG_DEBUG_NAME("ExtractionBarrierNode"));
	AddNodeInternal(barrierNode, RGDependeciesContainer{});

	for (RGTextureHandle extractedTexture : m_extractedTextures)
	{
		const rhi::BarrierTextureTransitionDefinition* transitionTarget = extractedTexture->GetPreExtractionTransitionTarget();
		if (transitionTarget)
		{
			const rhi::TextureSubresourceRange& transitionRange = extractedTexture->GetPreExtractionTransitionRange();
			AppendTextureTransitionToNode(barrierNode, extractedTexture, transitionRange, *transitionTarget);
		}
	}
}

void RenderGraphBuilder::ResolveResourceReleases()
{
	ResolveTextureReleases();
}

void RenderGraphBuilder::ResolveTextureReleases()
{
	SPT_PROFILER_FUNCTION();

	for (RGTextureHandle texture : m_textures)
	{
		if (!texture->IsExternal() && !texture->IsExtracted())
		{
			RGTextureAccessState& textureAccesses = texture->GetAccessState();

			RGNodeHandle lastAccessNode;

			if (textureAccesses.IsFullResource())
			{
				const RGTextureSubresourceAccessState& accessState = textureAccesses.GetForFullResource();
				lastAccessNode = accessState.lastAccessNode;
			}
			else
			{
				// find last accessed subresource
				textureAccesses.ForEachSubresource([&](RGTextureSubresource subresource)
												   {
													   const RGTextureSubresourceAccessState& accessState = textureAccesses.GetForSubresource(subresource);
													   if (!lastAccessNode.IsValid() || lastAccessNode->GetID() < accessState.lastAccessNode->GetID())
													   {
														   lastAccessNode = accessState.lastAccessNode;
													   }
												   });
			}

			SPT_CHECK(lastAccessNode.IsValid());
			lastAccessNode->AddTextureToRelease(texture);
		}
	}
}

} // spt::rg
