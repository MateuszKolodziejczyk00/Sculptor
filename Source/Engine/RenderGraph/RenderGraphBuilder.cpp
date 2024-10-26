#include "RenderGraphBuilder.h"
#include "Allocators/StackAllocation/StackTrackingAllocator.h"
#include "ResourcesManager.h"
#include "Renderer.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "Types/RenderContext.h"
#include "Vulkan/VulkanTypes/RHIBuffer.h"
#include "RenderGraphDebugDecorator.h"
#include "DeviceQueues/GPUWorkload.h"
#include "RenderGraphResourcesPool.h"
#include "Types/Pipeline/Pipeline.h"

namespace spt::rg
{

RenderGraphBuilder::RenderGraphBuilder(RenderGraphResourcesPool& resourcesPool)
	: m_onGraphExecutionFinished(js::CreateEvent("Render Graph Execution Finished Event"))
	, m_resourcesPool(resourcesPool)
	, m_dsAllocator(rdr::ResourcesManager::CreateDescriptorSetAllocator(RENDERER_RESOURCE_NAME("RG Descriptor Set Allocator")))
{
	m_resourcesPool.Prepare();
}

RenderGraphBuilder::~RenderGraphBuilder()
{
	m_allocator.Deallocate();

#if SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
	for (const auto& ds : m_allocatedDSStates)
	{
		SPT_CHECK_MSG(ds->GetRefCount() == 1, "Descriptor set {0} is still in use!", ds->GetName().GetData());
	}
#endif // SPT_RG_DEBUG_DESCRIPTOR_SETS_LIFETIME
}

void RenderGraphBuilder::BindGPUStatisticsCollector(const lib::SharedRef<rdr::GPUStatisticsCollector>& collector)
{
	m_statisticsCollector = collector;
}

void RenderGraphBuilder::AddDebugDecorator(const lib::SharedRef<RenderGraphDebugDecorator>& debugDecorator)
{
	m_debugDecorators.emplace_back(std::move(debugDecorator));
}

Bool RenderGraphBuilder::IsTextureAcquired(const lib::SharedPtr<rdr::Texture>& texture) const
{
	return m_externalTextures.contains(texture);
}

RGTextureHandle RenderGraphBuilder::AcquireExternalTexture(const lib::SharedPtr<rdr::Texture>& texture)
{
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

RGTextureViewHandle RenderGraphBuilder::TryAcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView)
{
	return textureView ? AcquireExternalTextureView(std::move(textureView)) : RGTextureViewHandle();
}

RGTextureViewHandle RenderGraphBuilder::AcquireExternalTextureView(lib::SharedPtr<rdr::TextureView> textureView)
{
	SPT_CHECK(!!textureView);

	const lib::SharedRef<rdr::Texture>& texture = textureView->GetTexture();

	const RGTextureHandle textureHandle = AcquireExternalTexture(texture);

	const RenderGraphDebugName name = RG_DEBUG_NAME(textureView->GetRHI().GetName());

	RGResourceDef definition;
	definition.name = name;
	definition.flags = lib::Flags(ERGResourceFlags::Default, ERGResourceFlags::External);
	const RGTextureViewHandle rgTextureView = m_allocator.Allocate<RGTextureView>(definition, textureHandle, lib::Ref(std::move(textureView)));

	return rgTextureView;
}

RGTextureHandle RenderGraphBuilder::CreateTexture(const RenderGraphDebugName& name, const TextureDef& textureDefinition, const std::optional<rhi::RHIAllocationInfo>& allocationInfo /*= std::nullopt*/, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	const RGTextureHandle textureHandle = m_allocator.Allocate<RGTexture>(definition, textureDefinition, allocationInfo);

	m_textures.emplace_back(textureHandle);

	return textureHandle;
}

RGTextureViewHandle RenderGraphBuilder::CreateTextureView(const RenderGraphDebugName& name, RGTextureHandle texture, const rhi::TextureViewDefinition& viewDefinition /*= rhi::TextureViewDefinition()*/, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	RGResourceDef definition;
	definition.name = name;
	definition.flags = flags;

	RGTextureViewHandle textureViewHandle = m_allocator.Allocate<RGTextureView>(definition, texture, viewDefinition);

	return textureViewHandle;
}

RGTextureViewHandle RenderGraphBuilder::CreateTextureView(const RenderGraphDebugName& name, const TextureDef& textureDefinition, const std::optional<rhi::RHIAllocationInfo>& allocationInfo /*= std::nullopt*/, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	const RGTextureHandle texture = CreateTexture(name, textureDefinition, allocationInfo, flags);

	rhi::TextureViewDefinition viewDefintion;
	viewDefintion.subresourceRange.aspect = rhi::GetFullAspectForFormat(textureDefinition.format);

	return CreateTextureView(name, texture, viewDefintion, flags);
}

void RenderGraphBuilder::ExtractTexture(RGTextureHandle textureHandle, lib::SharedPtr<rdr::Texture>& extractDestination)
{
	textureHandle->SetExtractionDestination(extractDestination);

	m_extractedTextures.emplace_back(textureHandle);
}

void RenderGraphBuilder::ReleaseTextureWithTransition(RGTextureHandle textureHandle, const rhi::BarrierTextureTransitionDefinition& releaseTransitionTarget)
{
	SPT_CHECK(textureHandle.IsValid());
	SPT_CHECK(textureHandle->IsExternal() || textureHandle->IsExtracted());

	textureHandle->SetReleaseTransitionTarget(&releaseTransitionTarget);
}

RGBufferHandle RenderGraphBuilder::AcquireExternalBuffer(const lib::SharedPtr<rdr::Buffer>& buffer)
{
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

RGBufferViewHandle RenderGraphBuilder::AcquireExternalBufferView(const rdr::BufferView& bufferView)
{
	const lib::SharedPtr<rdr::Buffer> owningBuffer = bufferView.GetBuffer();
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
	RGResourceDef resourceDefinition;
	resourceDefinition.name = name;
	resourceDefinition.flags = flags;

	const RGBufferHandle bufferHandle = m_allocator.Allocate<RGBuffer>(resourceDefinition, bufferDefinition, allocationInfo);
	m_buffers.emplace_back(bufferHandle);
	
	return bufferHandle;
}

RGBufferViewHandle RenderGraphBuilder::CreateBufferView(const RenderGraphDebugName& name, RGBufferHandle buffer, Uint64 offset, Uint64 size, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	RGResourceDef resourceDefinition;
	resourceDefinition.name = name;
	resourceDefinition.flags = flags;
	
	return m_allocator.Allocate<RGBufferView>(resourceDefinition, buffer, offset, size);
}

RGBufferViewHandle RenderGraphBuilder::CreateBufferView(const RenderGraphDebugName& name, const rhi::BufferDefinition& bufferDefinition, const rhi::RHIAllocationInfo& allocationInfo, ERGResourceFlags flags /*= ERGResourceFlags::Default*/)
{
	const RGBufferHandle buffer = CreateBuffer(name, bufferDefinition, allocationInfo, flags);

	return CreateBufferView(name, buffer, 0, bufferDefinition.size, flags);
}

void RenderGraphBuilder::ExtractBuffer(RGBufferHandle buffer, lib::SharedPtr<rdr::Buffer>& extractDestination)
{
	SPT_CHECK(buffer.IsValid());

	buffer->SetExtractionDest(&extractDestination);
	m_extractedBuffers.emplace_back(buffer);
}

RenderGraphResourcesPool& RenderGraphBuilder::GetResourcesPool() const
{
	return m_resourcesPool;
}

#if RG_ENABLE_DIAGNOSTICS
void RenderGraphBuilder::PushProfilerScope(lib::HashedString name)
{
	m_profilerRecorder.BeginScope(name);
}

void RenderGraphBuilder::PopProfilerScope()
{
	m_profilerRecorder.PopScope();
}
#endif // RG_ENABLE_DIAGNOSTICS

void RenderGraphBuilder::FillBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range, Uint32 data)
{
	SPT_CHECK(bufferView.IsValid());
	SPT_CHECK(offset + range <= bufferView->GetSize());

	const Bool isHostAccessible = bufferView->AllowsHostWrites();
	const Bool canFillOnHost = isHostAccessible && !bufferView->GetLastAccessNode().IsValid();

	SPT_CHECK(canFillOnHost || lib::HasAnyFlag(bufferView->GetUsageFlags(), rhi::EBufferUsage::TransferDst));

	const Uint64 writeOffset = bufferView->GetOffset() + offset;
	const auto executeLambda = [canFillOnHost, bufferView, writeOffset, range, data](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const lib::SharedPtr<rdr::Buffer>& bufferInstance = bufferView->GetBuffer()->GetResource();
		SPT_CHECK(!!bufferInstance);
		if (canFillOnHost)
		{
			rhi::RHIBuffer& rhiBuffer = bufferInstance->GetRHI();

			SPT_CHECK(rhiBuffer.CanMapMemory());

			const rhi::RHIMappedByteBuffer mappedBuffer(rhiBuffer);
			std::memset(mappedBuffer.GetPtr() + writeOffset, static_cast<int>(data), range);
		}
		else
		{
			recorder.FillBuffer(lib::Ref(bufferInstance), writeOffset, range, data);
		}
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(commandName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	dependenciesBuilder.AddBufferAccess(bufferView, ERGBufferAccess::Write, canFillOnHost ? rhi::EPipelineStage::Host : rhi::EPipelineStage::Transfer);

	AddNodeInternal(node, dependencies);
}

void RenderGraphBuilder::FillFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint32 data)
{
	SPT_CHECK(bufferView.IsValid());

	FillBuffer(commandName, bufferView, 0, bufferView->GetSize(), data);
}

void RenderGraphBuilder::CopyFullBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, RGBufferViewHandle destBufferView)
{
	SPT_CHECK(sourceBufferView.IsValid());
	SPT_CHECK(destBufferView.IsValid());
	SPT_CHECK(sourceBufferView->GetSize() == destBufferView->GetSize());

	CopyBuffer(commandName, sourceBufferView, 0, destBufferView, 0, sourceBufferView->GetSize());
}

void RenderGraphBuilder::CopyBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle sourceBufferView, Uint64 sourceOffset, RGBufferViewHandle destBufferView, Uint64 destOffset, Uint64 range)
{
	SPT_CHECK(sourceBufferView.IsValid());
	SPT_CHECK(sourceOffset + range <= sourceBufferView->GetSize());
	SPT_CHECK(destBufferView.IsValid());
	SPT_CHECK(destOffset + range <= destBufferView->GetSize());

	SPT_CHECK(sourceBufferView != destBufferView);

	auto executeLambda = [=](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const Uint64 copySourceOffset = sourceBufferView->GetOffset() + sourceOffset;
		const Uint64 copyDestOffset   = destBufferView->GetOffset() + destOffset;

		const lib::SharedPtr<rdr::Buffer>& sourceBuffer = sourceBufferView->GetBuffer()->GetResource();
		const lib::SharedPtr<rdr::Buffer>& destBuffer   = destBufferView->GetBuffer()->GetResource();

		recorder.CopyBuffer(lib::Ref(sourceBuffer), copySourceOffset, lib::Ref(destBuffer), copyDestOffset, range);
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(commandName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	dependenciesBuilder.AddBufferAccess(sourceBufferView, ERGBufferAccess::Read, rhi::EPipelineStage::Transfer);
	dependenciesBuilder.AddBufferAccess(destBufferView, ERGBufferAccess::Write, rhi::EPipelineStage::Transfer);

	AddNodeInternal(node, dependencies);
}

lib::SharedRef<rdr::Buffer> RenderGraphBuilder::DownloadBuffer(const RenderGraphDebugName& commandName, RGBufferViewHandle bufferView, Uint64 offset, Uint64 range)
{
	rhi::BufferDefinition resultBufferDefinition;
	resultBufferDefinition.size = range;
	resultBufferDefinition.usage = rhi::EBufferUsage::TransferDst;
	const lib::SharedRef<rdr::Buffer> result = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME_FORMATTED("{} Download Buffer", commandName.AsString()), resultBufferDefinition, rhi::EMemoryUsage::GPUToCpu);

	CopyBuffer(commandName, bufferView, offset, AcquireExternalBufferView(result->CreateFullView()), 0, range);

	return result;
}

lib::SharedRef<rdr::Buffer> RenderGraphBuilder::DownloadTexture(const RenderGraphDebugName& commandName, RGTextureViewHandle textureView)
{
	const Uint64 mipSize = textureView->GetMipSize();

	rhi::BufferDefinition resultBufferDefinition;
	resultBufferDefinition.size  = mipSize;
	resultBufferDefinition.usage = rhi::EBufferUsage::TransferDst;
	const lib::SharedRef<rdr::Buffer> result = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME_FORMATTED("{} Download Buffer", commandName.AsString()), resultBufferDefinition, rhi::EMemoryUsage::GPUToCpu);

	CopyTextureToBuffer(commandName, textureView, AcquireExternalBufferView(result->CreateFullView()), 0u);

	return result;
}

void RenderGraphBuilder::CopyTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, const math::Vector3i& sourceOffset, RGTextureViewHandle destRGTextureView, const math::Vector3i& destOffset, const math::Vector3u& copyExtent)
{
	SPT_CHECK(sourceRGTextureView.IsValid());
	SPT_CHECK(destRGTextureView.IsValid());
	SPT_CHECK((copyExtent.array() > 0).all());

	const auto executeLambda = [=](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const lib::SharedPtr<rdr::TextureView> sourceView = sourceRGTextureView->GetResource();
		const lib::SharedPtr<rdr::TextureView> destView = destRGTextureView->GetResource();
		SPT_CHECK(!!sourceView);
		SPT_CHECK(!!destView);

		const lib::SharedRef<rdr::Texture>& sourceTextureInstance = sourceView->GetTexture();
		const lib::SharedRef<rdr::Texture>& destTextureInstance = destView->GetTexture();

		const rhi::TextureSubresourceRange& sourceSubresource = sourceRGTextureView->GetSubresourceRange();
		const rhi::TextureSubresourceRange& destSubresource = destRGTextureView->GetSubresourceRange();
		
		const auto createCopyRange = [](const rhi::TextureSubresourceRange& subresourceRange, const rhi::TextureDefinition& textureDef, const math::Vector3i& offset)
		{
			rhi::TextureCopyRange copyRange;
			copyRange.aspect = subresourceRange.aspect;
			copyRange.mipLevel = subresourceRange.baseMipLevel;
			copyRange.baseArrayLayer = subresourceRange.baseArrayLayer;
			copyRange.arrayLayersNum = subresourceRange.arrayLayersNum != rhi::constants::allRemainingArrayLayers ? subresourceRange.arrayLayersNum : textureDef.arrayLayers;
			copyRange.offset = offset;
			return copyRange;
		};

		const rhi::TextureCopyRange sourceCopyRange = createCopyRange(sourceSubresource, sourceTextureInstance->GetRHI().GetDefinition(), sourceOffset);
		const rhi::TextureCopyRange destCopyRange = createCopyRange(destSubresource, destTextureInstance->GetRHI().GetDefinition(), destOffset);

		recorder.CopyTexture(sourceTextureInstance, sourceCopyRange, destTextureInstance, destCopyRange, copyExtent);
	};
	
	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(copyName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	dependenciesBuilder.AddTextureAccess(sourceRGTextureView, ERGTextureAccess::TransferSource);
	dependenciesBuilder.AddTextureAccess(destRGTextureView, ERGTextureAccess::TransferDest);

	AddNodeInternal(node, dependencies);
}

void RenderGraphBuilder::CopyFullTexture(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, RGTextureViewHandle destRGTextureView)
{
	SPT_CHECK(sourceRGTextureView.IsValid());
	SPT_CHECK(destRGTextureView.IsValid());

	SPT_CHECK(sourceRGTextureView->GetResolution() == destRGTextureView->GetResolution());

	CopyTexture(copyName, sourceRGTextureView, math::Vector3i::Zero(), destRGTextureView, math::Vector3i::Zero(), sourceRGTextureView->GetResolution());
}

void RenderGraphBuilder::CopyTextureToBuffer(const RenderGraphDebugName& copyName, RGTextureViewHandle sourceRGTextureView, RGBufferViewHandle destBufferView, Uint64 bufferOffset)
{
	SPT_CHECK(sourceRGTextureView.IsValid());
	SPT_CHECK(destBufferView.IsValid());

	SPT_CHECK(lib::HasAllFlags(destBufferView->GetUsageFlags(), rhi::EBufferUsage::TransferDst));

	SPT_MAYBE_UNUSED
	const rhi::TextureSubresourceRange& subresourceRange = sourceRGTextureView->GetSubresourceRange();

	//SPT_CHECK(subresourceRange.arrayLayersNum == 1u);
	//SPT_CHECK(subresourceRange.mipLevelsNum == 1u);

	const math::Vector3u resolution = sourceRGTextureView->GetResolution();

	SPT_CHECK(resolution.z() == 1u);
	SPT_CHECK(sourceRGTextureView->GetMipSize() + bufferOffset <= destBufferView->GetSize());

	sourceRGTextureView->GetTexture()->AddUsageForAccess(ERGTextureAccess::TransferSource);

	const auto executeLambda = [=](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const lib::SharedPtr<rdr::TextureView> textureViewInstance = sourceRGTextureView->GetResource();
		SPT_CHECK(!!textureViewInstance);

		const lib::SharedRef<rdr::Texture>& textureInstance = textureViewInstance->GetTexture();

		const rhi::TextureSubresourceRange& textureSubresource = sourceRGTextureView->GetSubresourceRange();

		const math::Vector3u copyExtent = sourceRGTextureView->GetResolution();
		const math::Vector3u copyOffset = math::Vector3u::Zero();

		const rg::RGBufferHandle destBuffer = destBufferView->GetBuffer();
		const lib::SharedPtr<rdr::Buffer>& bufferInstance = destBuffer->GetResource();
		SPT_CHECK(!!bufferInstance);

		recorder.CopyTextureToBuffer(textureInstance,
									 rhi::GetFullAspectForFormat(sourceRGTextureView->GetFormat()),
									 copyExtent,
									 copyOffset,
									 lib::Ref(bufferInstance),
									 bufferOffset,
									 textureSubresource.baseMipLevel,
									 textureSubresource.baseArrayLayer);
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(copyName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	dependenciesBuilder.AddTextureAccess(sourceRGTextureView, ERGTextureAccess::TransferSource);
	dependenciesBuilder.AddBufferAccess(destBufferView, ERGBufferAccess::Write, rhi::EPipelineStage::Transfer);

	AddNodeInternal(node, dependencies);
}

void RenderGraphBuilder::ClearTexture(const RenderGraphDebugName& clearName, RGTextureViewHandle textureView, const rhi::ClearColor& clearColor)
{
	SPT_CHECK(textureView.IsValid());

	const auto executeLambda = [ = ](const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
	{
		const lib::SharedPtr<rdr::TextureView> textureViewInstance = textureView->GetResource();
		SPT_CHECK(!!textureViewInstance);

		const lib::SharedRef<rdr::Texture>& textureInstance = textureViewInstance->GetTexture();

		const rhi::TextureSubresourceRange& textureSubresource = textureView->GetSubresourceRange();
		
		recorder.ClearTexture(textureInstance, clearColor, textureSubresource);
	};

	using LambdaType = std::remove_cvref_t<decltype(executeLambda)>;
	using NodeType = RGLambdaNode<LambdaType>;

	NodeType& node = AllocateNode<NodeType>(clearName, ERenderGraphNodeType::Transfer, std::move(executeLambda));

	RGDependeciesContainer dependencies;
	RGDependenciesBuilder dependenciesBuilder(*this, dependencies);
	dependenciesBuilder.AddTextureAccess(textureView, ERGTextureAccess::TransferDest);

	AddNodeInternal(node, dependencies);
}

void RenderGraphBuilder::BindDescriptorSetState(const lib::MTHandle<RGDescriptorSetStateBase>& dsState)
{
	m_boundDSStates.emplace_back(dsState);
}

void RenderGraphBuilder::UnbindDescriptorSetState(const lib::MTHandle<RGDescriptorSetStateBase>& dsState)
{
	const auto foundDS = std::find(std::cbegin(m_boundDSStates), std::cend(m_boundDSStates), dsState);
	if (foundDS != std::cend(m_boundDSStates))
	{
		m_boundDSStates.erase(foundDS);
	}
}

const js::Event& RenderGraphBuilder::GetGPUFinishedEvent() const
{
	return m_onGraphExecutionFinished;
}

void RenderGraphBuilder::Execute()
{
	PostBuild();

	ExecuteGraph();
}

void RenderGraphBuilder::AssignDescriptorSetsToNode(RGNode& node, const lib::SharedPtr<rdr::Pipeline>& pipeline, lib::Span<lib::MTHandle<RGDescriptorSetStateBase> const> dsStatesRange, RGDependenciesBuilder& dependenciesBuilder)
{
	const smd::ShaderMetaData* metaData = pipeline ? &pipeline->GetMetaData() : nullptr;

	const auto processDSStatesRange = [&metaData, &node, &dependenciesBuilder](const auto& range)
		{
			for (const lib::MTHandle<RGDescriptorSetStateBase>& ds : range)
			{
				if (ds.IsValid() && (!metaData || metaData->FindDescriptorSetOfType(ds->GetTypeID()) != idxNone<Uint32>))
				{
					node.AddDescriptorSetState(ds);
					ds->BuildRGDependencies(dependenciesBuilder);
				}
			}
		};

	processDSStatesRange(dsStatesRange);
	processDSStatesRange(m_boundDSStates);
}

void RenderGraphBuilder::AddNodeInternal(RGNode& node, const RGDependeciesContainer& dependencies)
{
	ResolveNodeDependecies(node, dependencies);

	m_nodes.emplace_back(&node);

	PostNodeAdded(node, dependencies);
}

void RenderGraphBuilder::PostNodeAdded(RGNode& node, const RGDependeciesContainer& dependencies)
{
#if RG_ENABLE_DIAGNOSTICS
	node.SetDiagnosticsRecord(m_profilerRecorder.GetRecord());
#endif // RG_ENABLE_DIAGNOSTICS

	if (node.GetType() == ERenderGraphNodeType::RenderPass)
	{
		m_lastRenderPassNode = static_cast<RGRenderPassNodeBase*>(&node);
	}

	for (const lib::SharedPtr<RenderGraphDebugDecorator>& decorator : m_debugDecorators)
	{
		decorator->PostNodeAdded(*this, node, dependencies);
	}
}

void RenderGraphBuilder::ResolveNodeDependecies(RGNode& node, const RGDependeciesContainer& dependencies)
{
	ResolveNodeTextureAccesses(node, dependencies);
	ResolveNodeBufferAccesses(node, dependencies);
}

void RenderGraphBuilder::ResolveNodeTextureAccesses(RGNode& node, const RGDependeciesContainer& dependencies)
{
	for (const RGTextureAccessDef& textureAccessDef : dependencies.textureAccesses)
	{
		const RGTextureViewHandle accessedTextureView					= textureAccessDef.textureView;
		const RGTextureHandle accessedTexture							= accessedTextureView->GetTexture();
		const rhi::TextureSubresourceRange& accessedSubresourceRange	= accessedTextureView->GetSubresourceRange();

		accessedTexture->AddUsageForAccess(textureAccessDef.access);

		if (!accessedTexture->IsExternal() && !accessedTexture->HasAcquireNode())
		{
			node.AddTextureToAcquire(accessedTexture);
		}
		
		if (!accessedTextureView->IsExternal() && !accessedTextureView->HasAcquireNode())
		{
			node.AddTextureViewToAcquire(accessedTextureView);
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

	if (textureAccessState.IsFullResource() && textureAccessState.RangeContainsFullResource(accessedSubresourceRange))
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
	for (const RGBufferAccessDef& bufferAccess : dependencies.bufferAccesses)
	{
		const RGBufferViewHandle accessedBufferView = bufferAccess.resource;
		SPT_CHECK(accessedBufferView.IsValid());

		const RGBufferHandle accessedBuffer = accessedBufferView->GetBuffer();
		SPT_CHECK(accessedBuffer.IsValid());

		const Uint64 offset	= accessedBufferView->GetOffset();
		const Uint64 size	= accessedBufferView->GetSize();

		if (!accessedBuffer->IsExternal() && !accessedBuffer->GetAcquireNode().IsValid())
		{
			node.AddBufferToAcquire(accessedBuffer);
		}
			
		const ERGBufferAccess nextAccess			= bufferAccess.access;
		const rhi::EPipelineStage nextAccessStages	= bufferAccess.pipelineStages;

		// Buffers may be used in multiple ways in the same node, for example as a indirect buffer and storage buffer
		// Because of that, we need to merge transitions of same buffer views
		if (accessedBuffer->GetLastAccessNode() == &node)
		{
			rhi::EAccessType destAccessType = rhi::EAccessType::None;
			GetSynchronizationParamsForBuffer(nextAccess, destAccessType);
			node.TryAppendBufferSynchronizationDest(accessedBuffer, offset, size, nextAccessStages, destAccessType);
			continue;
		}

		const ERGBufferAccess prevAccess           = accessedBufferView->GetLastAccessType();
		const rhi::EPipelineStage prevAccessStages = accessedBufferView->GetLastAccessPipelineStages();

		if (RequiresSynchronization(accessedBuffer, prevAccessStages, prevAccess, nextAccess, nextAccessStages))
		{
			rhi::EAccessType sourceAccessType = rhi::EAccessType::None;
			GetSynchronizationParamsForBuffer(prevAccess, OUT sourceAccessType);
			
			rhi::EAccessType destAccessType = rhi::EAccessType::None;
			GetSynchronizationParamsForBuffer(nextAccess, OUT destAccessType);

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
	case ERGTextureAccess::Unknown:
		return rhi::TextureTransition::Auto;

	case ERGTextureAccess::ColorRenderTarget:
		return rhi::TextureTransition::ColorRenderTarget;

	case ERGTextureAccess::DepthRenderTarget:
		return rhi::TextureTransition::DepthRenderTarget;

	case ERGTextureAccess::StencilRenderTarget:
		return rhi::TextureTransition::DepthStencilRenderTarget;

	case ERGTextureAccess::StorageWriteTexture:
		if (nodeType == ERenderGraphNodeType::RenderPass)
		{
			return rhi::TextureTransition::FragmentGeneral;
		}
		else if (nodeType == ERenderGraphNodeType::Dispatch)
		{
			return rhi::TextureTransition::ComputeGeneral;
		}
		else if (nodeType == ERenderGraphNodeType::TraceRays)
		{
			return rhi::TextureTransition::RayTracingGeneral;
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
			return rhi::TextureTransition::Undefined;
		}

	case ERGTextureAccess::SampledTexture:
		if (nodeType == ERenderGraphNodeType::RenderPass)
		{
			return rhi::TextureTransition::FragmentReadOnly;
		}
		else if (nodeType == ERenderGraphNodeType::Dispatch)
		{
			return rhi::TextureTransition::ComputeReadOnly;
		}
		else if (nodeType == ERenderGraphNodeType::TraceRays)
		{
			return rhi::TextureTransition::RayTracingReadOnly;
		}
		else
		{
			SPT_CHECK_NO_ENTRY();
			return rhi::TextureTransition::Undefined;
		}

	case ERGTextureAccess::TransferSource:
		return rhi::TextureTransition::TransferSource;

	case ERGTextureAccess::TransferDest:
		return rhi::TextureTransition::TransferDest;

	default:
		SPT_CHECK_NO_ENTRY();
		return rhi::TextureTransition::Undefined;
	}
}

void RenderGraphBuilder::GetSynchronizationParamsForBuffer(ERGBufferAccess lastAccess, rhi::EAccessType& outAccessType) const
{
	switch (lastAccess)
	{
	case ERGBufferAccess::Unknown:
		// Assume worst case (write) if we don't know previous access
		outAccessType = rhi::EAccessType::Write;
		break;

	case ERGBufferAccess::Read:
		outAccessType = rhi::EAccessType::Read;
		break;

	case ERGBufferAccess::Write:
		outAccessType = rhi::EAccessType::Write;
		break;

	case ERGBufferAccess::ReadWrite:
		outAccessType = lib::Flags(rhi::EAccessType::Read, rhi::EAccessType::Write);
		break;

	default:
		SPT_CHECK_NO_ENTRY();
		outAccessType = rhi::EAccessType::None;
	}
}

Bool RenderGraphBuilder::RequiresSynchronization(const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget) const
{
	const Bool prevAccessIsWrite = lib::HasAnyFlag(transitionSource.accessType, rhi::EAccessType::Write);
	const Bool newAccessIsWrite = lib::HasAnyFlag(transitionTarget.accessType, rhi::EAccessType::Write);

	return transitionSource.layout == rhi::ETextureLayout::Auto // always do transition from "auto" state
		|| transitionSource.layout != transitionTarget.layout
		|| prevAccessIsWrite || newAccessIsWrite // read -> write, write -> read, write -> write
		|| transitionTarget.stage != transitionSource.stage; // read -> read, but target stage is earlier than source stage (it's not necessary in some cases (f.e. vertex shader -> fragment shader) but in practice we don't have such cases
}

Bool RenderGraphBuilder::RequiresSynchronization(RGBufferHandle buffer, rhi::EPipelineStage prevAccessStage, ERGBufferAccess prevAccess, ERGBufferAccess nextAccess, rhi::EPipelineStage nextAccessStage) const
{
	const auto isWriteAccess = [](ERGBufferAccess access)
	{
		return access == ERGBufferAccess::ReadWrite
			|| access == ERGBufferAccess::Write;
	};

	// Assume that we don't need synchronization for buffers that were not used before in this render graph and host cannot write to them
	// This type of buffers should be already properly synchronized using semaphores if wrote on GPU
	if (!buffer->AllowsHostWrites() && prevAccess == ERGBufferAccess::Unknown && !isWriteAccess(nextAccess))
	{
		return false;
	}

	const Bool prevAccessIsWrite = isWriteAccess(prevAccess);
	const Bool nextAccessIsWrite = isWriteAccess(nextAccess);

	if (prevAccessIsWrite || nextAccessIsWrite)
	{
		return true;
	}

	if (prevAccessStage != nextAccessStage)
	{
		return true;
	}

	return false;
}

void RenderGraphBuilder::PostBuild()
{
	AddReleaseResourcesNode();

	ResolveTextureProperties();
}

void RenderGraphBuilder::ExecuteGraph()
{
	SPT_PROFILER_FUNCTION();

	RGExecutionContext graphExecutionContext;
	graphExecutionContext.statisticsCollector = m_statisticsCollector;

	rhi::ContextDefinition renderContextDefinition;
	const lib::SharedRef<rdr::RenderContext> renderContext = rdr::ResourcesManager::CreateContext(RENDERER_RESOURCE_NAME("Render Graph Context"), renderContextDefinition);

	const rhi::CommandBufferDefinition cmdBufferDef(rhi::EDeviceCommandQueueType::Graphics, rhi::ECommandBufferType::Primary, rhi::ECommandBufferComplexityClass::Default);
	lib::UniquePtr<rdr::CommandRecorder> commandRecorder = rdr::ResourcesManager::CreateCommandRecorder(RENDERER_RESOURCE_NAME("RenderGraphCommandBuffer"),
																										renderContext,
																										cmdBufferDef);

#if RG_ENABLE_DIAGNOSTICS
	RGDiagnosticsPlayback diagnosticsPlayback;
#endif // RG_ENABLE_DIAGNOSTICS

	for (const RGNodeHandle node : m_nodes)
	{
#if RG_ENABLE_DIAGNOSTICS
		diagnosticsPlayback.Playback(*commandRecorder, m_statisticsCollector, node->GetDiagnosticsRecord());
#endif // RG_ENABLE_DIAGNOSTICS

		node->Execute(renderContext, *commandRecorder, graphExecutionContext);
	}

#if RG_ENABLE_DIAGNOSTICS
	diagnosticsPlayback.PopRemainingScopes(*commandRecorder, m_statisticsCollector);
#endif // RG_ENABLE_DIAGNOSTICS

	lib::SharedRef<rdr::GPUWorkload> workload = commandRecorder->FinishRecording();

	workload->BindEvent(m_onGraphExecutionFinished);

	rdr::Renderer::GetDeviceQueuesManager().Submit(workload, lib::Flags(rdr::EGPUWorkloadSubmitFlags::MemoryTransfersWait, rdr::EGPUWorkloadSubmitFlags::CorePipe));
}

void RenderGraphBuilder::AddReleaseResourcesNode()
{
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
		else
		{
			RGTextureAccessState& accessState = texture->GetAccessState();
			if (!accessState.IsFullResource())
			{
				// This transition may result in disabling resource reuse (last access will be at the end of render graph
				// Alternative is to add this barrier immediately after last use, but this might decrease performance on GPU
				const rhi::EFragmentFormat textureFormat = texture->GetTextureDefinition().format;
				const rhi::TextureSubresourceRange transitionRange(rhi::GetFullAspectForFormat(textureFormat));
				AppendTextureTransitionToNode(barrierNode, texture, transitionRange, rhi::TextureTransition::FragmentGeneral);
				accessState.SetSubresourcesAccess(RGTextureSubresourceAccessState(ERGTextureAccess::StorageWriteTexture, &barrierNode), transitionRange);
			}
		}
	}
}

void RenderGraphBuilder::ResolveResourceProperties()
{
	ResolveTextureProperties();
	ResolveBufferReleases();
}

void RenderGraphBuilder::ResolveTextureProperties()
{
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

			if (lastAccessNode)
			{
				lastAccessNode->AddTextureToRelease(texture);

				texture->SelectAllocationStrategy();
			}
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

rdr::PipelineStateID RenderGraphBuilder::GetOrCreateComputePipelineStateID(rdr::ShaderID shader) const
{
	return rdr::ResourcesManager::CreateComputePipeline(RENDERER_RESOURCE_NAME(shader.GetName()), shader);
}

lib::SharedPtr<rdr::Pipeline> RenderGraphBuilder::GetPipelineObject(rdr::PipelineStateID psoID) const
{
	SPT_CHECK(psoID.IsValid());

	return rdr::ResourcesManager::GetPipelineObject(psoID);
}

} // spt::rg
