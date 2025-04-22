#include "ViewRenderingSpec.h"
#include "RenderView.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderStageEntries ============================================================================

RenderStageEntries::RenderStageEntries()
{ }

RenderStageEntries::PreRenderStageDelegate& RenderStageEntries::GetPreRenderStage()
{
	return m_preRenderStage;
}

RenderStageEntries::OnRenderStageDelegate& RenderStageEntries::GetOnRenderStage()
{
	return m_onRenderStage;
}

RenderStageEntries::PostRenderStageDelegate& RenderStageEntries::GetPostRenderStage()
{
	return m_postRenderStage;
}

void RenderStageEntries::BroadcastPreRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	GetPreRenderStage().Broadcast(graphBuilder, scene, viewSpec, context);
}

void RenderStageEntries::BroadcastOnRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context, RenderStageContextMetaDataHandle metaData /*= RenderStageContextMetaDataHandle()*/)
{
	GetOnRenderStage().Broadcast(graphBuilder, scene, viewSpec, context, metaData);
}

void RenderStageEntries::BroadcastPostRenderStage(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, ViewRenderingSpec& viewSpec, const RenderStageExecutionContext& context)
{
	GetPostRenderStage().Broadcast(graphBuilder, scene, viewSpec, context);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// ViewRenderingSpec =============================================================================

ViewRenderingSpec::ViewRenderingSpec()
	: m_renderView(nullptr)
{ }

void ViewRenderingSpec::Initialize(RenderView& renderView)
{
	SPT_PROFILER_FUNCTION();

	SPT_CHECK(!m_renderView);
	
	m_renderView = &renderView;

	const SizeType supportedStages = static_cast<SizeType>(renderView.GetSupportedStages());
	for (SizeType stage = 1; stage <= supportedStages; stage <<= 1)
	{
		if (supportedStages & stage)
		{
			m_stagesEntries.emplace(stage, RenderStageEntries());
		}
	}

	const Bool isShadingView = lib::HasAnyFlag(renderView.GetSupportedStages(), ERenderStage::DeferredLightingStages);
	if (isShadingView)
	{
		m_blackboard.Create<ShadingViewContext>();
		m_blackboard.Create<ShadingViewResourcesUsageInfo>();
		m_blackboard.Create<ShadingViewRenderingSystemsInfo>();
	}
}

RenderView& ViewRenderingSpec::GetRenderView() const
{
	SPT_CHECK(!!m_renderView);
	return *m_renderView;
}

ERenderStage ViewRenderingSpec::GetSupportedStages() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->GetSupportedStages();
}

Bool ViewRenderingSpec::SupportsStage(ERenderStage stage) const
{
	SPT_CHECK(!!m_renderView);
	return lib::HasAnyFlag(m_renderView->GetSupportedStages(), stage);
}

math::Vector2u ViewRenderingSpec::GetRenderingRes() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->GetRenderingRes();
}

math::Vector2u ViewRenderingSpec::GetRenderingHalfRes() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->GetRenderingHalfRes();
}

math::Vector2u ViewRenderingSpec::GetOutputRes() const
{
	SPT_CHECK(!!m_renderView);
	return m_renderView->GetOutputRes();
}

ShadingViewContext& ViewRenderingSpec::GetShadingViewContext()
{
	return m_blackboard.Get<ShadingViewContext>();
}

const ShadingViewContext& ViewRenderingSpec::GetShadingViewContext() const
{
	return m_blackboard.Get<ShadingViewContext>();
}

const lib::Blackboard& ViewRenderingSpec::GetBlackboard() const
{
	return m_blackboard;
}

lib::Blackboard& ViewRenderingSpec::GetBlackboard()
{
	return m_blackboard;
}

const RenderStageEntries& ViewRenderingSpec::GetRenderStageEntries(ERenderStage stage) const
{
	SPT_CHECK(!!m_renderView);
	return m_stagesEntries.at(static_cast<SizeType>(stage));
}

RenderStageEntries& ViewRenderingSpec::GetRenderStageEntries(ERenderStage stage)
{
	SPT_CHECK(!!m_renderView);
	return m_stagesEntries.at(static_cast<SizeType>(stage));
}

RenderViewEntryDelegate& ViewRenderingSpec::GetRenderViewEntry(const lib::HashedString& name)
{
	return m_viewEntries[name];
}

} // spt::rsc