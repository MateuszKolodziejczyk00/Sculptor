#include "RenderStage.h"


namespace spt::rsc
{

RenderStagesAPI& RenderStagesAPI::Get()
{
	static RenderStagesAPI api;
	return api;
}

RenderStageBase* RenderStagesAPI::CallConstructor(ERenderStage stageType, lib::MemoryArena& arena) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	return stageEntry.constructor(arena);
}

void RenderStagesAPI::CallDestructor(ERenderStage stageType, RenderStageBase& stage) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.destructor(stage);
}

void RenderStagesAPI::CallInitializeFunc(ERenderStage stageType, RenderStageBase& stage, RenderView& renderView) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.initializeFunc(stage, renderView);
}

void RenderStagesAPI::CallDeinitializeFunc(ERenderStage stageType, RenderStageBase& stage, RenderView& renderView) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.deinitializeFunc(stage, renderView);
}

void RenderStagesAPI::CallBeginFrameFunc(ERenderStage stageType, RenderStageBase& stage, const RenderScene& renderScene, ViewRenderingSpec& viewSpec) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.beginFrameFunc(stage, renderScene, viewSpec);
}

void RenderStagesAPI::CallEndFrameFunc(ERenderStage stageType, RenderStageBase& stage, const RenderScene& renderScene, const RenderView& renderView) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.endFrameFunc(stage, renderScene, renderView);
}

void RenderStagesAPI::CallRenderFunc(ERenderStage stageType, RenderStageBase& stage, rg::RenderGraphBuilder& graphBuilder, SceneRendererInterface& rendererInterface, const RenderScene& renderScene, ViewRenderingSpec& viewSpec, const SceneRendererSettings& settings) const
{
	const RenderStageEntry& stageEntry = GetStageEntry(stageType);
	stageEntry.renderFunc(stage, graphBuilder, rendererInterface, renderScene, viewSpec, settings);
}

const RenderStagesAPI::RenderStageEntry& RenderStagesAPI::GetStageEntry(ERenderStage stage) const
{
	const Uint32 stageIdx = RenderStageToIdx(stage);
	SPT_CHECK(stageIdx < m_stageEntries.size());
	return m_stageEntries[stageIdx];
}

} // spt::rsc
