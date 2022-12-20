#include "RenderGraphBuilder.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"

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

	RGTextureHandle textureHandle = m_allocator.Allocate<RGTexture>(definition, texture);

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

	RGTextureHandle textureHandle = m_allocator.Allocate<RGTexture>(definition, textureDefinition, allocationInfo);

	m_textures.emplace_back(textureHandle);

	return textureHandle;
}

RGTextureViewHandle RenderGraphBuilder::CreateTextureView(const RenderGraphDebugName& name, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	RGTextureViewHandle textureViewHandle = m_allocator.Allocate<RGTextureView>(definition, texture, viewDefinition);

	return textureViewHandle;
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

void RenderGraphBuilder::BindDescriptorSetState(const lib::SharedPtr<rdr::DescriptorSetState>& dsState)
{
	SPT_PROFILER_FUNCTION();

	m_boundDSStates.emplace_back(dsState);
}

void RenderGraphBuilder::UnbindDescriptorSetState(const lib::SharedPtr<rdr::DescriptorSetState>& dsState)
{
	SPT_PROFILER_FUNCTION();

	const auto foundDS = std::find(std::cbegin(m_boundDSStates), std::cend(m_boundDSStates), dsState);
	if (foundDS != std::cend(m_boundDSStates))
	{
		m_boundDSStates.erase(foundDS);
	}
}

void RenderGraphBuilder::Execute(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores)
{
	PostBuild();

	ExecuteGraph(waitSemaphores, signalSemaphores);
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

		const rhi::BarrierTextureTransitionDefinition& transitionTarget = GetTransitionDefForAccess(&node, textureAccessDef.access);

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
		const RGTextureSubresourceAccessState& sourceAccessState = textureAccessState.GetForFullResource();
		const rhi::BarrierTextureTransitionDefinition& transitionSource = GetTransitionDefForAccess(sourceAccessState.lastAccessNode, sourceAccessState.lastAccessType);

		if (RequiresSynchronization(transitionSource, transitionTarget))
		{
			node.AddTextureState(accessedTexture, accessedSubresourceRange, transitionSource, transitionTarget);
		}
	}
	else
	{
		textureAccessState.ForEachSubresource(accessedSubresourceRange,
											  [&, this](RGTextureSubresource subresource)
											  {
												  const RGTextureSubresourceAccessState& subresourceSourceAccessState = textureAccessState.GetForSubresource(subresource);
												  const rhi::BarrierTextureTransitionDefinition& transitionSource = GetTransitionDefForAccess(subresourceSourceAccessState.lastAccessNode, subresourceSourceAccessState.lastAccessType);

												  rhi::TextureSubresourceRange subresourceRange;
												  subresourceRange.aspect			= accessedSubresourceRange.aspect;
												  subresourceRange.baseArrayLayer	= subresource.arrayLayerIdx;
												  subresourceRange.arrayLayersNum	= 1;
												  subresourceRange.baseMipLevel		= subresource.mipMapIdx;
												  subresourceRange.mipLevelsNum		= 1;

		
												  if (RequiresSynchronization(transitionSource, transitionTarget))
												  {
													  node.AddTextureState(accessedTexture, subresourceRange, transitionSource, transitionTarget);
												  }
											  });
	}
}

void RenderGraphBuilder::ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY();
}

const rhi::BarrierTextureTransitionDefinition& RenderGraphBuilder::GetTransitionDefForAccess(RGNodeHandle node, ERGAccess access) const
{
	SPT_CHECK_NO_ENTRY();

	const ERenderGraphNodeType nodeType = node.IsValid() ? node->GetType() : ERenderGraphNodeType::None;

	switch (access)
	{
	case spt::rg::ERGAccess::Unknown:
		return rhi::TextureTransition::Auto;

	case spt::rg::ERGAccess::ColorRenderTarget:
		return rhi::TextureTransition::ColorRenderTarget;

	case spt::rg::ERGAccess::DepthRenderTarget:
		return rhi::TextureTransition::DepthRenderTarget;

	case spt::rg::ERGAccess::StencilRenderTarget:
		return rhi::TextureTransition::DepthStencilRenderTarget;

	case spt::rg::ERGAccess::StorageWriteTexture:
		if (nodeType == ERenderGraphNodeType::RenderPass)
		{
			return rhi::TextureTransition::FragmentGeneral;
		}
		else if (nodeType == ERenderGraphNodeType::Dispatch)
		{
			return rhi::TextureTransition::ComputeGeneral;
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
		}

	case spt::rg::ERGAccess::SampledTexture:
		if (nodeType == ERenderGraphNodeType::RenderPass)
		{
			return rhi::TextureTransition::FragmentReadOnly;
		}
		else if (nodeType == ERenderGraphNodeType::Dispatch)
		{
			return rhi::TextureTransition::ComputeReadOnly;
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
		}

	default:
		SPT_CHECK_NO_ENTRY();
		return rhi::TextureTransition::Undefined;
	}
}

Bool RenderGraphBuilder::RequiresSynchronization(const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget) const
{
	const Bool prevAccessIsWrite = transitionSource.accessType == rhi::EAccessType::Write;
	const Bool newAccessIsWrite = transitionTarget.accessType == rhi::EAccessType::Write;

	return transitionSource.layout != transitionTarget.layout
		|| prevAccessIsWrite != newAccessIsWrite // read -> write, write -> read
		|| prevAccessIsWrite && newAccessIsWrite; // write -> write
}

void RenderGraphBuilder::PostBuild()
{
	SPT_PROFILER_FUNCTION();

	AddPrepareTexturesForExtractionNode();

	ResolveTextureReleases();
}

void RenderGraphBuilder::ExecuteGraph(const rdr::SemaphoresArray& waitSemaphores, const rdr::SemaphoresArray& signalSemaphores)
{
	SPT_PROFILER_FUNCTION();

	rhi::ContextDefinition contextDefinition;
	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("Render Graph Context"), contextDefinition);
	lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::Renderer::StartRecordingCommands();

	for (const RGNodeHandle node : m_nodes)
	{
		node->Execute(renderContext, *commandRecorder);
	}

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(commandRecorder));
	submitBatch.waitSemaphores = waitSemaphores;
	submitBatch.signalSemaphores = signalSemaphores;

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);
}

void RenderGraphBuilder::AddPrepareTexturesForExtractionNode()
{
	SPT_PROFILER_FUNCTION();

	RGEmptyNode& barrierNode = AllocateNode<RGEmptyNode>(RG_DEBUG_NAME("ExtractionBarrierNode"), ERenderGraphNodeType::None);
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
