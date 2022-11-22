#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RHIBridge/RHIDependencyImpl.h"
#include "RenderGraphPersistentState.h"
#include "GPUDiagnose/Diagnose.h"

namespace spt::rg
{

RGNode::RGNode(const RenderGraphDebugName& name, RGNodeID id)
	: m_name(name)
	, m_id(id)
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

void RGNode::AddTextureToAcquire(RGTextureHandle texture)
{
	m_texturesToAcquire.emplace_back(texture);
}

void RGNode::AddTextureToRelease(RGTextureHandle texture)
{
	SPT_CHECK(!texture->HasReleaseNode());
	m_texturesToRelease.emplace_back(texture);
}

void RGNode::AddTextureState(RGTextureHandle texture, const rhi::TextureSubresourceRange& textureSubresourceRange, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_PROFILER_FUNCTION();

	m_preExecuteTransitions.emplace_back(TextureTransitionDef(texture, textureSubresourceRange, &transitionSource, &transitionTarget));
}

void RGNode::Execute(const lib::SharedPtr<rdr::CommandRecorder>& recorder)
{
	SPT_PROFILER_FUNCTION();

	SPT_GPU_PROFILER_EVENT(GetName().Get().GetData());
	SPT_GPU_DEBUG_REGION(*recorder, GetName().Get().GetData(), lib::Color::Blue);

	SPT_CHECK(!m_executed);

	CreateResources();

	PreExecuteBarrier(recorder);

	OnExecute(recorder);

	ReleaseResources();

	m_executed = true;
}

void RGNode::CreateResources()
{
	SPT_PROFILER_FUNCTION();
	
	CreateTextures();
}

void RGNode::PreExecuteBarrier(const lib::SharedPtr<rdr::CommandRecorder>& recorder)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIDependency barrierDependency;

	for (const TextureTransitionDef& textureTransition : m_preExecuteTransitions)
	{
		const lib::SharedPtr<rdr::Texture>& textureInstance = textureTransition.texture->GetResource();
		const SizeType barrierIdx = barrierDependency.AddTextureDependency(textureInstance->GetRHI(), textureTransition.textureSubresourceRange);
		barrierDependency.SetLayoutTransition(barrierIdx, *textureTransition.transitionSource, *textureTransition.transitionTarget);
	}

	recorder->ExecuteBarrier(std::move(barrierDependency));
}

void RGNode::ReleaseResources()
{
	SPT_PROFILER_FUNCTION();

	ReleaseTextures();
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

} // spt::rg
