#include "RenderViewEntryPoints.h"
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
// RenderViewEntryPoints =========================================================================

RenderViewEntryPoints::RenderViewEntryPoints()
	: m_renderView(nullptr)
{ }

void RenderViewEntryPoints::Initialize(const RenderView& renderView)
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

const RenderView& RenderViewEntryPoints::GetRenderView() const
{
	SPT_CHECK(!!m_renderView);
	return *m_renderView;
}

Bool RenderViewEntryPoints::SupportsStage(ERenderStage stage) const
{
	SPT_CHECK(!!m_renderView);
	return lib::HasAnyFlag(m_renderView->GetSupportedStages(), stage);
}

const RenderStageEntries& RenderViewEntryPoints::GetRenderStageEntries(ERenderStage stage) const
{
	SPT_CHECK(!!m_renderView);
	return m_stagesEntries.at(static_cast<SizeType>(stage));
}

RenderStageEntries& RenderViewEntryPoints::GetRenderStageEntries(ERenderStage stage)
{
	SPT_CHECK(!!m_renderView);
	return m_stagesEntries.at(static_cast<SizeType>(stage));
}

} // spt::rsc