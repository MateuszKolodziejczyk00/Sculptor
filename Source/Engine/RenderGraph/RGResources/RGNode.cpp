#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "RenderGraphPersistentState.h"
#include "GPUDiagnose/Diagnose.h"
#include "ResourcesManager.h"

namespace spt::rg
{

RGNode::RGNode(const RenderGraphDebugName& name, RGNodeID id, ERenderGraphNodeType type)
	: m_name(name)
	, m_id(id)
	, m_type(type)
	, m_executed(false)
{ }

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

void RGNode::AddTextureToAcquire(RGTextureHandle texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(texture.IsValid());

	SPT_CHECK(!texture->GetAcquireNode().IsValid());
	texture->SetAcquireNode(this);
	m_texturesToAcquire.emplace_back(texture);
}

void RGNode::AddTextureToRelease(RGTextureHandle texture)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(texture.IsValid());

	SPT_CHECK(!texture->GetReleaseNode().IsValid());
	texture->SetReleaseNode(this);
	m_texturesToRelease.emplace_back(texture);
}

void RGNode::AddBufferToAcquire(RGBufferHandle buffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(buffer.IsValid());

	SPT_CHECK(!buffer->GetAcquireNode().IsValid());
	buffer->SetAcquireNode(this);
	m_buffersToAcquire.emplace_back(buffer);
}

void RGNode::AddBufferToRelease(RGBufferHandle buffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(buffer.IsValid());

	SPT_CHECK(!buffer->GetReleaseNode().IsValid());
	buffer->SetReleaseNode(this);
	m_buffersToRelease.emplace_back(buffer);
}

void RGNode::AddTextureTransition(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionDefinition& transitionSource, const rhi::BarrierTextureTransitionDefinition& transitionTarget)
{
	SPT_PROFILER_FUNCTION();

	m_preExecuteTransitions.emplace_back(TextureTransitionDef(texture, textureSubresourceRange, &transitionSource, &transitionTarget));
}

void RGNode::AddBufferSynchronization(RGBufferHandle buffer)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK_NO_ENTRY_MSG("TODO: Buffer Synchronization not supported in RHI!");
}

void RGNode::Execute(const lib::SharedRef<rdr::RenderContext>& renderContext, rdr::CommandRecorder& recorder)
{
	SPT_PROFILER_FUNCTION();

	SPT_GPU_PROFILER_EVENT(GetName().Get().GetData());
	SPT_GPU_DEBUG_REGION(recorder, GetName().Get().GetData(), lib::Color::Blue);

	SPT_CHECK(!m_executed);

	CreateResources();

	PreExecuteBarrier(recorder);

	OnExecute(renderContext, recorder);

	ReleaseResources();

	m_executed = true;
}

void RGNode::CreateResources()
{
	SPT_PROFILER_FUNCTION();
	
	CreateTextures();
	CreateBuffers();
}

void RGNode::PreExecuteBarrier(rdr::CommandRecorder& recorder)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIDependency barrierDependency;

	for (const TextureTransitionDef& textureTransition : m_preExecuteTransitions)
	{
		const lib::SharedPtr<rdr::Texture>& textureInstance = textureTransition.texture->GetResource();
		const SizeType barrierIdx = barrierDependency.AddTextureDependency(textureInstance->GetRHI(), textureTransition.textureSubresourceRange);
		barrierDependency.SetLayoutTransition(barrierIdx, *textureTransition.transitionSource, *textureTransition.transitionTarget);
	}

	recorder.ExecuteBarrier(std::move(barrierDependency));
}

void RGNode::ReleaseResources()
{
	SPT_PROFILER_FUNCTION();

	ReleaseTextures();
	ReleaseBuffers();
}

void RGNode::CreateTextures()
{
	RenderGraphResourcesPool& resourcesPool = RenderGraphPersistentState::GetResourcesPool();

	for (RGTextureHandle textureToAcquire : m_texturesToAcquire)
	{
		lib::SharedPtr<rdr::Texture> acquiredTexture = resourcesPool.AcquireTexture(RG_DEBUG_NAME(textureToAcquire->GetName()), textureToAcquire->GetTextureDefinition(), textureToAcquire->GetAllocationInfo());
		SPT_CHECK(!!acquiredTexture);

		textureToAcquire->AcquireResource(std::move(acquiredTexture));
	}
}

void RGNode::ReleaseTextures()
{
	RenderGraphResourcesPool& resourcesPool = RenderGraphPersistentState::GetResourcesPool();

	for (RGTextureHandle textureToRelease : m_texturesToRelease)
	{
		resourcesPool.ReleaseTexture(textureToRelease->ReleaseResource());
	}
}

void RGNode::CreateBuffers()
{
	for (RGBufferHandle buffer : m_buffersToAcquire)
	{
		const lib::SharedPtr<rdr::Buffer> bufferInstance = rdr::ResourcesManager::CreateBuffer(RENDERER_RESOURCE_NAME(buffer->GetName()), buffer->GetBufferDefinition(), buffer->GetAllocationInfo());
		buffer->AcquireResource(bufferInstance);
	}
}

void RGNode::ReleaseBuffers()
{
	for (RGBufferHandle buffer : m_buffersToRelease)
	{
		buffer->ReleaseResource();
		
	}
}

} // spt::rg
