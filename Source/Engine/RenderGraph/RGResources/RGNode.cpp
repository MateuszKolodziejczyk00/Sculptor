#include "RGResources/RGNode.h"
#include "CommandsRecorder/CommandRecorder.h"
#include "RHIBridge/RHIDependencyImpl.h"

namespace spt::rg
{

RGNode::RGNode()
	: m_executed(false)
{ }

void RGNode::AddTextureState(const RGTextureView& textureView, const rhi::BarrierTextureTransitionTarget& transitionSource, const rhi::BarrierTextureTransitionTarget& transitionTarget)
{
	SPT_PROFILER_FUNCTION();

	m_preExecuteTransitions.emplace_back(TextureTransitionDef(textureView, &transitionSource, &transitionTarget));
}

void RGNode::PreExecuteBarrier(const lib::SharedPtr<rdr::CommandRecorder>& recorder)
{
	SPT_PROFILER_FUNCTION();

	rhi::RHIDependency barrierDependency;

	for (const TextureTransitionDef& textureTransition : m_preExecuteTransitions)
	{
		const RGTextureHandle texture = textureTransition.textureView.GetTexture();
		const lib::SharedPtr<rdr::Texture>& textureResource = texture->GetResource();

		const SizeType barrierIdx = barrierDependency.AddTextureDependency(textureResource->GetRHI(), textureTransition.textureView.GetViewDefinition().subresourceRange);
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
