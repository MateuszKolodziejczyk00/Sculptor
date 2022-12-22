#include "RenderGraphBuilder.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"

namespace spt::rg
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGRenderPassDefinition ========================================================================

RGRenderPassDefinition::RGRenderPassDefinition(math::Vector2i renderAreaOffset, math::Vector2u renderAreaExtent, rhi::ERenderingFlags renderingFlags /*= rhi::ERenderingFlags::Default*/)
	: m_renderAreaOffset(renderAreaOffset)
	, m_renderAreaExtent(renderAreaExtent)
	, m_renderingFlags(renderingFlags)
{ }

RGRenderTargetDef& RGRenderPassDefinition::AddColorRenderTarget()
{
	return m_colorRenderTargetDefs.emplace_back();
}

void RGRenderPassDefinition::AddColorRenderTarget(const RGRenderTargetDef& definition)
{
	m_colorRenderTargetDefs.emplace_back(definition);
}

RGRenderTargetDef& RGRenderPassDefinition::GetDepthRenderTargetRef()
{
	return m_depthRenderTargetDef;
}

void RGRenderPassDefinition::SetDepthRenderTarget(const RGRenderTargetDef& definition)
{
	m_depthRenderTargetDef = definition;
}

RGRenderTargetDef& RGRenderPassDefinition::GetStencilRenderTargetRef()
{
	return m_stencilRenderTargetDef;
}

void RGRenderPassDefinition::SetStencilRenderTarget(const RGRenderTargetDef& definition)
{
	m_stencilRenderTargetDef = definition;
}

rdr::RenderingDefinition RGRenderPassDefinition::CreateRenderingDefinition() const
{
	SPT_PROFILER_FUNCTION();

	rdr::RenderingDefinition renderingDefinition(m_renderAreaOffset, m_renderAreaExtent, m_renderingFlags);

	std::for_each(std::cbegin(m_colorRenderTargetDefs), std::cend(m_colorRenderTargetDefs),
				  [this, &renderingDefinition](const RGRenderTargetDef& def)
				  {
					  renderingDefinition.AddColorRenderTarget(CreateRTDefinition(def));
				  });

	if (IsRTDefinitionValid(m_depthRenderTargetDef))
	{
		renderingDefinition.AddDepthRenderTarget(CreateRTDefinition(m_depthRenderTargetDef));
	}

	if (IsRTDefinitionValid(m_stencilRenderTargetDef))
	{
		renderingDefinition.AddStencilRenderTarget(CreateRTDefinition(m_stencilRenderTargetDef));
	}

	return renderingDefinition;
}

void RGRenderPassDefinition::BuildDependencies(RGDependenciesBuilder& dependenciesBuilder) const
{
	SPT_PROFILER_FUNCTION();

	for (const RGRenderTargetDef& colorRenderTarget : m_colorRenderTargetDefs)
	{
		SPT_CHECK(colorRenderTarget.textureView.IsValid());

		dependenciesBuilder.AddTextureAccess(colorRenderTarget.textureView, ERGTextureAccess::ColorRenderTarget);

		if (colorRenderTarget.resolveTextureView.IsValid())
		{
			dependenciesBuilder.AddTextureAccess(colorRenderTarget.resolveTextureView, ERGTextureAccess::ColorRenderTarget);
		}
	}

	if (m_depthRenderTargetDef.textureView.IsValid())
	{
		dependenciesBuilder.AddTextureAccess(m_depthRenderTargetDef.textureView, ERGTextureAccess::DepthRenderTarget);
	}

	if (m_stencilRenderTargetDef.textureView.IsValid())
	{
		dependenciesBuilder.AddTextureAccess(m_stencilRenderTargetDef.textureView, ERGTextureAccess::StencilRenderTarget);
	}
}

rdr::RTDefinition RGRenderPassDefinition::CreateRTDefinition(const RGRenderTargetDef& rgDef) const
{
	SPT_CHECK(rgDef.textureView.IsValid());

	rdr::RTDefinition def;

	def.textureView = rgDef.textureView->GetViewInstance();

	if (rgDef.resolveTextureView.IsValid())
	{
		def.resolveTextureView = rgDef.resolveTextureView->GetViewInstance();
	}

	def.loadOperation	= rgDef.loadOperation;
	def.storeOperation	= rgDef.storeOperation;
	def.resolveMode		= rgDef.resolveMode;
	def.clearColor		= rgDef.clearColor;

	return def;
}

Bool RGRenderPassDefinition::IsRTDefinitionValid(const RGRenderTargetDef& rgDef) const
{
	return rgDef.textureView.IsValid();
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderGraphBuilder ============================================================================

RenderGraphBuilder::RenderGraphBuilder()
{ }

RGTextureHandle RenderGraphBuilder::AcquireExternalTexture(const lib::SharedPtr<rdr::Texture>& texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!texture);

	RGTextureHandle& textureHandle = m_externalTextures[texture];

	if (!textureHandle.IsValid())
	{
		const RenderGraphDebugName name = RG_DEBUG_NAME(texture->GetRHI().GetName());

		RGResourceDef definition;
		definition.name = name;
		definition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);

		textureHandle = m_allocator.Allocate<RGTexture>(definition, texture);

		m_textures.emplace_back(textureHandle);
	}

	return textureHandle;
}

RGTextureViewHandle RenderGraphBuilder::AcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!textureView);

	const lib::SharedPtr<rdr::Texture>& texture = textureView->GetTexture();
	SPT_CHECK(!!texture);

	const RGTextureHandle textureHandle = AcquireExternalTexture(texture);

	const RenderGraphDebugName name = RG_DEBUG_NAME(textureView->GetRHI().GetName());

	RGResourceDef definition;
	definition.name = name;
	definition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);
	const RGTextureViewHandle rgTextureView = m_allocator.Allocate<RGTextureView>(definition, textureHandle, lib::Ref(std::move(textureView)));

	return rgTextureView;
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& name, const rhi::TextureDefinition& textureDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	const RGTextureHandle textureHandle = m_allocator.Allocate<RGTexture>(definition, textureDefinition, allocationInfo);

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

void RenderGraphBuilder::ReleaseTextureWithTransition(RGTextureHandle textureHandle, const rhi::BarrierTextureTransitionDefinition& releaseTransitionTarget)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(textureHandle.IsValid());
	SPT_CHECK(textureHandle->IsExternal() || textureHandle->IsExtracted());

	textureHandle->SetReleaseTransitionTarget(&releaseTransitionTarget);
}

RGBufferHandle RenderGraphBuilder::AcquireExternalBuffer(const lib::SharedPtr<rdr::Buffer>& buffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!buffer);

	RGBufferHandle& bufferHandle = m_externalBuffers[buffer];

	if (!bufferHandle.IsValid())
	{
		const RenderGraphDebugName name = RG_DEBUG_NAME(buffer->GetRHI().GetName());

		RGResourceDef resourceDefinition;
		resourceDefinition.name = name;
		resourceDefinition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);

		bufferHandle = m_allocator.Allocate<RGBuffer>(resourceDefinition, buffer);
		m_buffers.emplace_back(bufferHandle);
	}
	SPT_CHECK(bufferHandle.IsValid());

	return bufferHandle;
}

RGBufferViewHandle RenderGraphBuilder::AcquireExternalBufferView(lib::SharedPtr<rdr::BufferView> bufferView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!!bufferView);

	const lib::SharedPtr<rdr::Buffer> owningBuffer = bufferView->GetBuffer();
	SPT_CHECK(!!owningBuffer);

	const RGBufferHandle owningBufferHandle = AcquireExternalBuffer(owningBuffer);
	SPT_CHECK(owningBufferHandle.IsValid());

	const RenderGraphDebugName name = RG_DEBUG_NAME(owningBuffer->GetRHI().GetName().ToString() + "View");

	RGResourceDef resourceDefinition;
	resourceDefinition.name = name;
	resourceDefinition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);

	const RGBufferViewHandle bufferViewHandle = m_allocator.Allocate<RGBufferView>(resourceDefinition, owningBufferHandle, std::move(bufferView));

	return bufferViewHandle;
}

RGBufferHandle RenderGraphBuilder::CreateBuffer(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();
	
	RGResourceDef resourceDefinition;
	resourceDefinition.name = name;
	resourceDefinition.flags = flags;

	const RGBufferHandle bufferHandle = m_allocator.Allocate<RGBuffer>(resourceDefinition, bufferDefinition, allocationInfo);
	m_buffers.emplace_back(bufferHandle);
	
	return bufferHandle;
}

RGBufferViewHandle RenderGraphBuilder::CreateBufferView(const RenderGraphDebugName& name, RGBufferHandle buffer, Uint64 offset, Uint64 size, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();
	
	RGResourceDef resourceDefinition;
	resourceDefinition.name = name;
	resourceDefinition.flags = flags;
	
	return m_allocator.Allocate<RGBufferView>(resourceDefinition, buffer, offset, size);
}

void RenderGraphBuilder::ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(buffer.IsValid());

	buffer->SetExtractionDest(&extractDestination);
	m_extractedBuffers.emplace_back(buffer);
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
		const rhi::TextureSubresourceRange& accessedSubresourceRange	= accessedTextureView->GetSubresourceRange();

		if (!accessedTexture->IsExternal())
		{
			if (accessedTexture->HasAcquireNode())
			{
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
			node.AddTextureTransition(accessedTexture, accessedSubresourceRange, transitionSource, transitionTarget);
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
													  node.AddTextureTransition(accessedTexture, subresourceRange, transitionSource, transitionTarget);
												  }
											  });
	}
}

void RenderGraphBuilder::ResolveNodeBufferAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	SPT_PROFILER_FUNCTION();

	for (const RGBufferAccessDef& bufferAccess : dependencies.bufferAccesses)
	{
		const RGBufferViewHandle accessedBufferView = bufferAccess.resource;
		SPT_CHECK(accessedBufferView.IsValid());

		const RGBufferHandle accessedBuffer = accessedBufferView->GetBuffer();
		SPT_CHECK(accessedBuffer.IsValid());

		if (!accessedBuffer->IsExternal() && !accessedBuffer->GetAcquireNode().IsValid())
		{
			node.AddBufferToAcquire(accessedBuffer);
		}

		const ERGBufferAccess prevAccess = accessedBuffer->GetLastAccessType();
		const ERGBufferAccess nextAccess = bufferAccess.access;

		if (RequiresSynchronization(prevAccess, nextAccess))
		{
			node.AddBufferSynchronization(accessedBuffer);
		}

		accessedBuffer->SetLastAccessNode(&node);
		accessedBuffer->SetLastAccessType(nextAccess);
	}
}

const rhi::BarrierTextureTransitionDefinition& RenderGraphBuilder::GetTransitionDefForAccess(RGNodeHandle node, ERGTextureAccess access) const
{
	const ERenderGraphNodeType nodeType = node.IsValid() ? node->GetType() : ERenderGraphNodeType::None;

	switch (access)
	{
	case spt::rg::ERGTextureAccess::Unknown:
		return rhi::TextureTransition::Auto;

	case spt::rg::ERGTextureAccess::ColorRenderTarget:
		return rhi::TextureTransition::ColorRenderTarget;

	case spt::rg::ERGTextureAccess::DepthRenderTarget:
		return rhi::TextureTransition::DepthRenderTarget;

	case spt::rg::ERGTextureAccess::StencilRenderTarget:
		return rhi::TextureTransition::DepthStencilRenderTarget;

	case spt::rg::ERGTextureAccess::StorageWriteTexture:
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
			return rhi::TextureTransition::Undefined;
		}

	case spt::rg::ERGTextureAccess::SampledTexture:
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
			return rhi::TextureTransition::Undefined;
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

	return transitionSource.layout == rhi::ETextureLayout::Auto // always do transition from "auto" state
		|| transitionSource.layout != transitionTarget.layout
		|| prevAccessIsWrite || newAccessIsWrite; // read -> write, write -> read, write -> write
}

Bool RenderGraphBuilder::RequiresSynchronization(ERGBufferAccess prevAccess, ERGBufferAccess nextAccess) const
{
	const auto isWriteAccess = [](ERGBufferAccess access)
	{
		return access == ERGBufferAccess::ReadWrite
			|| access == ERGBufferAccess::Write;
	};

	const Bool prevAccessIsWrite = isWriteAccess(prevAccess);
	const Bool nextAccessIsWrite = isWriteAccess(nextAccess);

	return prevAccessIsWrite || nextAccessIsWrite;
}

void RenderGraphBuilder::PostBuild()
{
	SPT_PROFILER_FUNCTION();

	AddReleaseResourcesNode();

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

	rdr::CommandsRecordingInfo recordingInfo;
	recordingInfo.commandsBufferName = RENDERER_RESOURCE_NAME("RenderGraphCommandBuffer");
	recordingInfo.commandBufferDef = rhi::CommandBufferDefinition(rhi::ECommandBufferQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Default);
	commandRecorder->RecordCommands(renderContext, recordingInfo, rhi::CommandBufferUsageDefinition(rhi::ECommandBufferBeginFlags::OneTimeSubmit));

	lib::DynamicArray<rdr::CommandsSubmitBatch> submitBatches;
	rdr::CommandsSubmitBatch& submitBatch = submitBatches.emplace_back(rdr::CommandsSubmitBatch());
	submitBatch.recordedCommands.emplace_back(std::move(commandRecorder));
	submitBatch.waitSemaphores = waitSemaphores;
	submitBatch.signalSemaphores = signalSemaphores;

	rdr::Renderer::SubmitCommands(rhi::ECommandBufferQueueType::Graphics, submitBatches);
}

void RenderGraphBuilder::AddReleaseResourcesNode()
{
	SPT_PROFILER_FUNCTION();

	RGEmptyNode& barrierNode = AllocateNode<RGEmptyNode>(RG_DEBUG_NAME("ExtractionBarrierNode"), ERenderGraphNodeType::None);
	AddNodeInternal(barrierNode, RGDependeciesContainer{});

	for (RGTextureHandle texture : m_textures)
	{
		const rhi::BarrierTextureTransitionDefinition* transitionTarget = texture->GetReleaseTransitionTarget();
		if (transitionTarget)
		{
			const rhi::EFragmentFormat textureFormat = texture->GetTextureDefinition().format;
			const rhi::TextureSubresourceRange transitionRange(rhi::GetFullAspectForFormat(textureFormat));

			AppendTextureTransitionToNode(barrierNode, texture, transitionRange, *transitionTarget);
		}
	}
}

void RenderGraphBuilder::ResolveResourceReleases()
{
	ResolveTextureReleases();
	ResolveBufferReleases();
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

void RenderGraphBuilder::ResolveBufferReleases()
{
	SPT_PROFILER_FUNCTION();

	for (RGBufferHandle buffer : m_buffers)
	{
		if (!buffer->IsExternal() && !buffer->IsExtracted())
		{
			const RGNodeHandle lastAccessNode = buffer->GetLastAccessNode();
			SPT_CHECK(lastAccessNode.IsValid());

			lastAccessNode->AddBufferToRelease(buffer);
		}
	}
}

lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> RenderGraphBuilder::GetExternalDSStates() const
{
	lib::DynamicArray<lib::SharedRef<rdr::DescriptorSetState>> states;
	states.reserve(m_boundDSStates.size());

	std::transform(std::cbegin(m_boundDSStates), std::cend(m_boundDSStates), std::back_inserter(states),
				   [](const lib::SharedPtr<rdr::DescriptorSetState>& state)
				   {
					   return lib::ToSharedRef(state);
				   });

	return states;
}

} // spt::rg
