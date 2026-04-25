#include "SceneRenderSystems/SceneRenderSystem.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRenderSystem =============================================================================

SceneRenderSystem::SceneRenderSystem(RenderScene& owningScene)
	: m_supportedStages(ERenderStage::None)
	, m_owningScene(owningScene)
{ }

ERenderStage SceneRenderSystem::GetSupportedStages() const
{
	return m_supportedStages;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRenderSystemsAPI =========================================================================

SceneRenderSystemsAPI& SceneRenderSystemsAPI::Get()
{
	static SceneRenderSystemsAPI api;
	return api;
}

SceneRenderSystem* SceneRenderSystemsAPI::CallConstructor(ESceneRenderSystem systemType, lib::MemoryArena& arena, RenderScene& scene) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.constructor.IsValid());
	return systemEntry.constructor(arena, scene);
}

void SceneRenderSystemsAPI::CallDestructor(ESceneRenderSystem systemType, SceneRenderSystem& system) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.destructor.IsValid());
	systemEntry.destructor(system);
}

void SceneRenderSystemsAPI::CallInitialize(ESceneRenderSystem systemType, SceneRenderSystem& system, lib::MemoryArena& arena, RenderScene& scene) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.updateFunc.IsValid());
	systemEntry.initializeFunc(system, arena, scene);
}

void SceneRenderSystemsAPI::CallDeinitialize(ESceneRenderSystem systemType, SceneRenderSystem& system, RenderScene& scene) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.updateFunc.IsValid());
	systemEntry.deinitializeFunc(system, scene);
}

void SceneRenderSystemsAPI::CallUpdateFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, const SceneUpdateContext& context) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.updateFunc.IsValid());
	systemEntry.updateFunc(system, context);
}

void SceneRenderSystemsAPI::CallUpdateGPUSceneDataFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, RenderSceneConstants& sceneData) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.updateGPUSceneDataFunc.IsValid());
	systemEntry.updateGPUSceneDataFunc(system, sceneData);
}

void SceneRenderSystemsAPI::CallCollectRenderViewsFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	if (systemEntry.collectRenderViewsFunc.IsValid())
	{
		systemEntry.collectRenderViewsFunc(system, rendererInterface, renderScene, mainRenderView, INOUT viewsCollector);
	}
}

void SceneRenderSystemsAPI::CallRenderPerFrameFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.renderPerFrameFunc.IsValid());
	systemEntry.renderPerFrameFunc(system, graphBuilder, rendererInterface, renderScene, viewSpecs, settings);
}

void SceneRenderSystemsAPI::CallFinishRenderingFrameFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene) const
{
	const SystemEntry& systemEntry = GetSystemEntry(systemType);

	SPT_CHECK(systemEntry.finishRenderingFrameFunc.IsValid());
	systemEntry.finishRenderingFrameFunc(system, graphBuilder, rendererInterface, renderScene);
}

} // spt::rsc
