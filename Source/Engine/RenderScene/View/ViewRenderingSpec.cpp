#include "ViewRenderingSpec.h"
#include "RenderView.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderStageEntries ============================================================================

RenderStageEntries::RenderStageEntries()
{ }

RenderStageEntries::PreRenderStageDelegate& RenderStageEntries::GetPreRenderStageDelegate()
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

const ViewRenderingDataContainer& ViewRenderingSpec::GetData() const
{
	return m_viewRenderingData;
}

ViewRenderingDataContainer& ViewRenderingSpec::GetData()
{
	return m_viewRenderingData;
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

} // spt::rsc