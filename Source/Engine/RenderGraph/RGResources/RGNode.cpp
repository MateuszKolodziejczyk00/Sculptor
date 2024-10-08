#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "RenderGraphResourcesPool.h"
#include "GPUDiagnose/Diagnose.h"
#include "ResourcesManager.h"
#include "RGResources.h"
#include "GPUDiagnose/Profiler/GPUStatisticsCollector.h"
#include "RenderGraphBuilder.h"

namespace spt::rg
{

namespace helpers
{

rdr::EQueryFlags GetQueryFlags(ERenderGraphNodeType nodeType)
{
	switch (nodeType)
	{
	case rg::ERenderGraphNodeType::RenderPass:	return rdr::EQueryFlags::Rasterization;
	case rg::ERenderGraphNodeType::Dispatch:	return rdr::EQueryFlags::Compute;

	default:

		return rdr::EQueryFlags::None;
	}
}

} // helpers

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGNode ========================================================================================

RGNode::RGNode(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type)
	: m_owningGraphBuilder(owningGraphBuilder)
	, m_name(name)
	, m_id(id)
	, m_type(type)
	, m_executed(false)
{ }

RenderGraphBuilder& RGNode::GetOwningGraphBuilder() const
{
	return m_owningGraphBuilder;
}

RGNodeID RGNode::GetID() const
{
	return m_id;
}

const RenderGraphDebugName& RGNode::GetName() const
{
	return m_name;
}

ERenderGraphNodeType RGNode::GetType() const
{
	return m_type;
}

void RGNode::SetDebugMetaData(RGNodeDebugMetaData metaData)
{
	m_debugMetaData = std::move(metaData);
}

const RGNodeDebugMetaData& RGNode::GetDebugMetaData() const
{
	return m_debugMetaData;
}

#if RG_ENABLE_DIAGNOSTICS
void RGNode::SetDiagnosticsRecord(RGDiagnosticsRecord record)
{
	m_diagnosticsRecord = std::move(record);
}

const RGDiagnosticsRecord& RGNode::GetDiagnosticsRecord() const
{
	return m_diagnosticsRecord;
}
#endif // RG_ENABLE_DIAGNOSTICS

void RGNode::AddTextureToAcquire(RGTextureHandle texture)
{
	SPT_CHECK(texture.IsValid());
	SPT_CHECK(!texture->GetAcquireNode().IsValid());

	texture->SetAcquireNode(this);
	m_texturesToAcquire.emplace_back(texture);
}

void RGNode::AddTextureToRelease(RGTextureHandle texture)
{
	SPT_CHECK(texture.IsValid());
	SPT_CHECK(!texture->GetReleaseNode().IsValid());

	texture->SetReleaseNode(this);
	m_texturesToRelease.emplace_back(texture);
}

void RGNode::AddTextureViewToAcquire(RGTextureViewHandle textureView)
{
	SPT_CHECK(textureView.IsValid());
	SPT_CHECK(!textureView->GetAcquireNode().IsValid());

	textureView->SetAcquireNode(this);
	m_textureViewsToAcquire.emplace_back(textureView);
}

void RGNode::AddBufferToAcquire(RGBufferHandle buffer)
{
	SPT_CHECK(buffer.IsValid());
	SPT_CHECK(!buffer->GetAcquireNode().IsValid());

	buffer->SetAcquireNode(this);
	m_buffersToAcquire.emplace_back(buffer);
}

void RGNode::AddBufferToRelease(RGBufferHandle buffer)
{
	SPT_CHECK(buffer.IsValid());
	SPT_CHECK(!buffer->GetReleaseNode().IsValid());

	buffer->SetReleaseNode(this);
	m_buffersToRelease.emplace_back(buffer);
}

void RGNode::AddTextureTransition(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	m_preExecuteTextureTransitions.emplace_back(TextureTransitionDef(texture, textureSubresourceRange, &transitionSource, &transitionTarget));
}

void RGNode::AddBufferSynchronization(RGBufferHandle buffer, Uint64 offset, Uint64 size, rhi::EPipelineStage sourceStage, rhi::EAccessType sourceAccess, rhi::EPipelineStage destStage, rhi::EAccessType destAccess)
{
	SPT_CHECK(buffer->GetLastAccessNode() != this);

	m_preExecuteBufferTransitions.emplace_back(BufferTransitionDef(buffer, offset, size, sourceStage, sourceAccess, destStage, destAccess));
}

void RGNode::TryAppendBufferSynchronizationDest(RGBufferHandle buffer, Uint64 offset, Uint64 size, rhi::EPipelineStage destStage, rhi::EAccessType destAccess)
{
	const auto foundBufferTranstion = std::find_if(std::begin(m_preExecuteBufferTransitions), std::end(m_preExecuteBufferTransitions),
												   [=](const BufferTransitionDef& bufferTransition)
												   {
													   return bufferTransition.buffer == buffer
														   && bufferTransition.offset == offset
														   && bufferTransition.size == size;
												   });

	if (foundBufferTranstion != std::end(m_preExecuteBufferTransitions))
	{
		foundBufferTranstion->destStage		= lib::Union(foundBufferTranstion->destStage, destStage);
		foundBufferTranstion->destAccess	= lib::Union(foundBufferTranstion->destAccess, destAccess);
	}
}

void RGNode::AddDescriptorSetState(lib::MTHandle<rdr::DescriptorSetState> dsState)
{
	m_dsStates.emplace_back(std::move(dsState));
}

void RGNode::Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_GPU_DEBUG_REGION(recorder, GetName().Get().GetData(), lib::Color(static_cast<Uint32>(GetName().Get().GetKey())));
	SPT_GPU_STATISTICS_SCOPE_FLAGS(recorder, context.statisticsCollector, GetName().Get().GetData(), helpers::GetQueryFlags(GetType()));

#if SPT_ENABLE_GPU_CRASH_DUMPS
	recorder.SetDebugCheckpoint(GetName().Get());
#endif // SPT_ENABLE_GPU_CRASH_DUMPS

	SPT_CHECK(!m_executed);

	AcquireResources();

	PreExecuteBarrier(recorder);

	BindDescriptorSetStates(recorder);
	OnExecute(renderContext, recorder, context);
	UnbindDescriptorSetStates(recorder);

	ReleaseResources();

	m_executed = true;
}

void RGNode::AcquireResources()
{
	AcquireTextures();
	AcquireTextureViews();
	AcquireBuffers();
}

void RGNode::PreExecuteBarrier(rdr::CommandRecorder& recorder)
{
	rhi::RHIDependency barrierDependency;

	for (const TextureTransitionDef& textureTransition : m_preExecuteTextureTransitions)
	{
		SPT_CHECK(textureTransition.texture->IsAcquired());
		const lib::SharedPtr<rdr::Texture>& textureInstance = textureTransition.texture->GetResource();
		const SizeType barrierIdx = barrierDependency.AddTextureDependency(textureInstance->GetRHI(), textureTransition.textureSubresourceRange);
		barrierDependency.SetLayoutTransition(barrierIdx, *textureTransition.transitionSource, *textureTransition.transitionTarget);
	}

	for (const BufferTransitionDef& transition : m_preExecuteBufferTransitions)
	{
		SPT_CHECK(transition.buffer->IsAcquired());
		const lib::SharedPtr<rdr::Buffer>& bufferInstance = transition.buffer->GetResource();
		const SizeType barrierIdx = barrierDependency.AddBufferDependency(bufferInstance->GetRHI(), transition.offset, transition.size);
		barrierDependency.SetBufferDependencyStages(barrierIdx, transition.sourceStage, transition.sourceAccess, transition.destStage, transition.destAccess);
	}

	recorder.ExecuteBarrier(barrierDependency);
}

void RGNode::ReleaseResources()
{
	ReleaseTextures();
	ReleaseBuffers();
}

void RGNode::BindDescriptorSetStates(rdr::CommandRecorder& recorder)
{
	recorder.BindDescriptorSetStates(m_dsStates);
}

void RGNode::UnbindDescriptorSetStates(rdr::CommandRecorder& recorder)
{
	recorder.UnbindDescriptorSetStates(m_dsStates);
}

void RGNode::AcquireTextures()
{
	const RenderGraphBuilder& graphBuilder = GetOwningGraphBuilder();
	RenderGraphResourcesPool& resourcesPool = graphBuilder.GetResourcesPool();

	for (RGTextureHandle textureToAcquire : m_texturesToAcquire)
	{
		lib::SharedPtr<rdr::Texture> acquiredTexture = resourcesPool.AcquireTexture(RG_DEBUG_NAME(textureToAcquire->GetName()), textureToAcquire->GetTextureRHIDefinition(), textureToAcquire->GetAllocationInfo());
		SPT_CHECK(!!acquiredTexture);

		textureToAcquire->AcquireResource(std::move(acquiredTexture));
	}
}

void RGNode::ReleaseTextures()
{
	const RenderGraphBuilder& graphBuilder = GetOwningGraphBuilder();
	RenderGraphResourcesPool& resourcesPool = graphBuilder.GetResourcesPool();

	for (RGTextureHandle textureToRelease : m_texturesToRelease)
	{
		resourcesPool.ReleaseTexture(textureToRelease->ReleaseResource());
	}
}

void RGNode::AcquireTextureViews()
{
	for (RGTextureViewHandle textureViewToAcquire : m_textureViewsToAcquire)
	{
		textureViewToAcquire->AcquireResource();
	}
}

void RGNode::AcquireBuffers()
{
	for (RGBufferHandle buffer : m_buffersToAcquire)
	{
		lib::SharedPtr<rdr::Buffer> bufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(buffer->GetName()), buffer->GetBufferDefinition(), buffer->GetAllocationInfo());
		buffer->AcquireResource(std::move(bufferInstance));
	}
}

void RGNode::ReleaseBuffers()
{
	for (RGBufferHandle buffer : m_buffersToRelease)
	{
		buffer->ReleaseResource();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// RGSubpass =====================================================================================

RGSubpass::RGSubpass(const RenderGraphDebugName& name)
	: m_name(name)
{ }

const RenderGraphDebugName& RGSubpass::GetName() const
{
	return m_name;
}

void RGSubpass::BindDSState(lib::MTHandle<rdr::DescriptorSetState> ds)
{
	m_dsStatesToBind.emplace_back(std::move(ds));
}

void RGSubpass::Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context)
{
	SPT_PROFILER_FUNCTION();

	SPT_GPU_DEBUG_REGION(recorder, GetName().Get().GetData(), lib::Color::Blue);
	SPT_GPU_STATISTICS_SCOPE(recorder, context.statisticsCollector, GetName().Get().GetData());

	for(const lib::MTHandle<rdr::DescriptorSetState>& ds : m_dsStatesToBind)
	{
		recorder.BindDescriptorSetState(ds);
	}

	DoExecute(renderContext, recorder);

	for(const lib::MTHandle<rdr::DescriptorSetState>& ds : m_dsStatesToBind)
	{
		recorder.UnbindDescriptorSetState(ds);
	}
}

RGRenderPassNodeBase::RGRenderPassNodeBase(RenderGraphBuilder& owningGraphBuilder, const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type, const RGRenderPassDefinition& renderPassDef)
	: Super(owningGraphBuilder, name, id, type)
	, m_renderPassDef(renderPassDef)
{ }

void RGRenderPassNodeBase::AppendSubpass(RGSubpassHandle subpass)
{
	m_subpasses.emplace_back(subpass);
}

void RGRenderPassNodeBase::OnExecute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder, const RGExecutionContext& context)
{
	const rdr::RenderingDefinition renderingDefinition = m_renderPassDef.CreateRenderingDefinition();

	recorder.BeginRendering(renderingDefinition);

	ExecuteRenderPass(renderContext, recorder);

	for (RGSubpassHandle subpass : m_subpasses)
	{
		subpass->Execute(renderContext, recorder, context);
	}

	recorder.EndRendering();
}

} // spt::rg
