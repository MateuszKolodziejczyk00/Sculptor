#include "RenderGraphBuilder.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "Vulkan/VulkanTypes/RHIBuffer.h"

namespace spt::rg
{

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

RGBufferViewHandle RenderGraphBuilder::CreateBufferView(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	SPT_PROFILER_FUNCTION();

	const RGBufferHandle buffer = CreateBuffer(name, bufferDefinition, allocationInfo, flags);

	return CreateBufferView(name, buffer, 0, bufferDefinition.size, flags);
}

void RenderGraphBuilder::ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination)
{
	SPT_PROFILER_FUNCTION();
	
	SPT_CHECK(buffer.IsValid());

	buffer->SetExtractionDest(&extractDestination);
	m_extractedBuffers.emplace_back(buffer);
}

void RenderGraphBuilder::AddFillBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range, Uint32 data)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(bufferView.IsValid());
	SPT_CHECK(offset + range <= bufferView->GetSize());

	const Bool isHostAccessible = bufferView->AllowsHostWrites();
	const Bool canFillOnHost = isHostAccessible && !bufferView->GetLastAccessNode().IsValid();

	const Uint64 writeOffset = bufferView->GetOffset() + offset;
	const auto executeLambda = [canFillOnHost, bufferView, writeOffset, range, data](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const lib::SharedPtr<rdr::Buffer>& bufferInstance = bufferView->GetBuffer()->GetResource();
		SPT_CHECK(!!bufferInstance);
		if (canFillOnHost)
		{
			rhi::RHIBuffer& rhiBuffer = bufferInstance->GetRHI();

			SPT_CHECK(rhiBuffer.CanMapMemory());
			Byte* bufferData = rhiBuffer.MapBufferMemory();
			SPT_CHECK(!bufferData);
			memset(bufferData + writeOffset, static_cast<int>(data), range);
			rhiBuffer.UnmapBufferMemory();
		}
		else
		{
			recorder.FillBuffer(lib::Ref(bufferInstance), writeOffset, range, data);
		}
	};

	using LambdaType = std::remove_cv_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(commandName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	if (!canFillOnHost)
	{
		RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
		dependenciesBuilder.AddBufferAccess(bufferView, ERGBufferAccess::ShaderWrite, rhi::EPipelineStage::Transfer);
	}

	AddNodeInternal(node, dependencies);
}

void RenderGraphBuilder::BindDescriptorSetState(const lib::SharedRef<rdr::DescriptorSetState>& dsState)
{
	SPT_PROFILER_FUNCTION();

	m_boundDSStates.emplace_back(dsState);
}

void RenderGraphBuilder::UnbindDescriptorSetState(const lib::SharedRef<rdr::DescriptorSetState>& dsState)
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

		const ERGBufferAccess prevAccess			= accessedBufferView->GetLastAccessType();
		const rhi::EPipelineStage prevAccessStages	= accessedBufferView->GetLastAccessPipelineStages();

		const ERGBufferAccess nextAccess			= bufferAccess.access;
		const rhi::EPipelineStage nextAccessStages	= bufferAccess.pipelineStages;

		if (RequiresSynchronization(accessedBuffer, prevAccess, nextAccess))
		{
			const Uint64 offset	= accessedBufferView->GetOffset();
			const Uint64 size	= accessedBufferView->GetSize();
			
			rhi::EAccessType sourceAccessType = rhi::EAccessType::None;
			GetSynchronizationParamsForBuffer(prevAccess, sourceAccessType);
			
			rhi::EAccessType destAccessType = rhi::EAccessType::None;
			GetSynchronizationParamsForBuffer(nextAccess, destAccessType);

			node.AddBufferSynchronization(accessedBuffer, offset, size, prevAccessStages, sourceAccessType, nextAccessStages, destAccessType);
		}
		
		accessedBuffer->SetLastAccessNode(&node);

		accessedBufferView->SetLastAccessNode(&node);
		accessedBufferView->SetLastAccessType(nextAccess);
		accessedBufferView->SetLastAccessPipelineStages(nextAccessStages);
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

void RenderGraphBuilder::GetSynchronizationParamsForBuffer(ERGBufferAccess lastAccess, rhi::EAccessType& outAccessType) const
{
	switch (lastAccess)
	{
	case ERGBufferAccess::ShaderRead:
		outAccessType = rhi::EAccessType::Write;
		break;

	case ERGBufferAccess::ShaderWrite:
		outAccessType = rhi::EAccessType::Write;
		break;

	case ERGBufferAccess::ShaderReadWrite:
		outAccessType = lib::Flags(rhi::EAccessType::Read, rhi::EAccessType::Write);
		break;

	default:
		SPT_CHECK_NO_ENTRY();
		outAccessType = rhi::EAccessType::None;
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

Bool RenderGraphBuilder::RequiresSynchronization(RGBufferHandle buffer, ERGBufferAccess prevAccess, ERGBufferAccess nextAccess) const
{
	// Assume that we don't need synchronization for buffers that were not used before in this render graph and host cannot write to them
	// This type of buffers should be already properly synchronized using semaphores if wrote on GPU
	if (!buffer->AllowsHostWrites() && prevAccess == ERGBufferAccess::Unknown)
	{
		return false;
	}

	const auto isWriteAccess = [](ERGBufferAccess access)
	{
		return access == ERGBufferAccess::ShaderReadWrite
			|| access == ERGBufferAccess::ShaderWrite;
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

} // spt::rg
