#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RHIBridge/RHIDependencyImpl.h"

namespace spt::rg
{

RGNode::RGNode()
	: m_executed(false)
{ }

void RGNode::AddTextureState(RGTextureHandle inTexture, const rhi::TextureSubresourceRange& inTextureSubresourceRange, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_PROFILER_FUNCTION();

	m_preExecuteTransitions.emplace_back(TextureTransitionDef(inTexture, inTextureSubresourceRange, &transitionSource, &transitionTarget));
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

void RGNode::Execute(const lib::SharedPtr<rdr::CommandRecorder>& recorder)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_executed);

	PreExecuteBarrier(recorder);

	OnExecute(recorder);

	m_executed = true;
}

} // spt::rg
