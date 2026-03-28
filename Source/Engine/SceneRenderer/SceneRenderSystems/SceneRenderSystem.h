#pragma once

#include "RenderSceneMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"
#include "Utils/ViewRenderingSpec.h"
#include "SculptorCoreTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg

namespace spt::rsc
{

class RenderScene;
class RenderView;
struct RenderSceneConstants;


struct SceneUpdateContext
{
	const RenderView&            mainRenderView;
	const SceneRendererSettings& rendererSettings;
};


class SceneRenderSystem abstract
{
public:

	SceneRenderSystem(RenderScene& owningScene);

	void Initialize(RenderScene& renderScene) {};
	void Deinitialize(RenderScene& renderScene) {};

	void Update(const SceneUpdateContext& context) {};

	void UpdateGPUSceneData(RenderSceneConstants& sceneData) {}

	void CollectRenderViews(const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) {};

	void RenderPerFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) {};
	
	void FinishRenderingFrame(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene) {};

	ERenderStage GetSupportedStages() const;

protected:

	RenderScene& GetOwningScene() const { return m_owningScene; }

	ERenderStage m_supportedStages;

private:

	RenderScene& m_owningScene;
};


class SceneRenderSystemsAPI
{
public:

	static SceneRenderSystemsAPI& Get();

	SceneRenderSystem* CallConstructor(ESceneRenderSystem systemType, lib::MemoryArena& arena, RenderScene& scene) const;
	void               CallDestructor(ESceneRenderSystem systemType, SceneRenderSystem& system) const;
	void               CallInitialize(ESceneRenderSystem systemType, SceneRenderSystem& system, RenderScene& scene) const;
	void               CallDeinitialize(ESceneRenderSystem systemType, SceneRenderSystem& system, RenderScene& scene) const;
	void               CallUpdateFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, const SceneUpdateContext& context) const;
	void               CallUpdateGPUSceneDataFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, RenderSceneConstants& sceneData) const;
	void               CallCollectRenderViewsFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector) const;
	void               CallRenderPerFrameFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings) const;
	void               CallFinishRenderingFrameFunc(ESceneRenderSystem systemType, SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene) const;

	template<typename TSystemType>
	void RegisterSystem()
	{
		constexpr ESceneRenderSystem systemType = TSystemType::systemType;
		const Uint32 systemIdx = GetSceneRenderSystemIdx(systemType);

		const auto constructor = [](lib::MemoryArena& arena, RenderScene& scene) -> SceneRenderSystem*
		{
			return arena.AllocateType<TSystemType>(scene);
		};

		const auto destructor = [](SceneRenderSystem& system)
		{
			static_cast<TSystemType&>(system).~TSystemType();
		};

		const auto initializeFunc = [](SceneRenderSystem& system, RenderScene& renderScene)
		{
			static_cast<TSystemType&>(system).Initialize(renderScene);
		};

		const auto deinitializeFunc = [](SceneRenderSystem& system, RenderScene& renderScene)
		{
			static_cast<TSystemType&>(system).Deinitialize(renderScene);
		};

		const auto updateFunc = [](SceneRenderSystem& system, const SceneUpdateContext& context)
		{
			static_cast<TSystemType&>(system).Update(context);
		};

		const auto updateGPUSceneDataFunc = [](SceneRenderSystem& system, RenderSceneConstants& sceneData)
		{
			static_cast<TSystemType&>(system).UpdateGPUSceneData(sceneData);
		};

		const auto collectRenderViewsFunc = [](SceneRenderSystem& system, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const RenderView& mainRenderView, INOUT RenderViewsCollector& viewsCollector)
		{
			static_cast<TSystemType&>(system).CollectRenderViews(rendererInterface, renderScene, mainRenderView, INOUT viewsCollector);
		};

		const auto renderPerFrameFunc = [](SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const lib::DynamicPushArray<ViewRenderingSpec*>& viewSpecs, const SceneRendererSettings& settings)
		{
			static_cast<TSystemType&>(system).RenderPerFrame(graphBuilder, rendererInterface, renderScene, viewSpecs, settings);
		};

		 const auto finishRenderingFrameFunc = [](SceneRenderSystem& system, rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene)
		{
			static_cast<TSystemType&>(system).FinishRenderingFrame(graphBuilder, rendererInterface, renderScene);
		};

		m_systemEntries[systemIdx] = SystemEntry{
			.constructor              = constructor,
			.destructor               = destructor,
			.initializeFunc           = initializeFunc,
			.deinitializeFunc         = deinitializeFunc,
			.updateFunc               = updateFunc,
			.updateGPUSceneDataFunc   = updateGPUSceneDataFunc,
			.collectRenderViewsFunc   = collectRenderViewsFunc,
			.renderPerFrameFunc       = renderPerFrameFunc,
			.finishRenderingFrameFunc = finishRenderingFrameFunc
		};
	}

private:

	struct SystemEntry
	{
		lib::RawCallable<SceneRenderSystem*(lib::MemoryArena&, RenderScene&)>                                                                                                                                  constructor;
		lib::RawCallable<void(SceneRenderSystem&)>                                                                                                                                                             destructor;
		lib::RawCallable<void(SceneRenderSystem&, RenderScene&)>                                                                                                                                               initializeFunc;
		lib::RawCallable<void(SceneRenderSystem&, RenderScene&)>                                                                                                                                               deinitializeFunc;
		lib::RawCallable<void(SceneRenderSystem&, const SceneUpdateContext&)>                                                                                                                                  updateFunc;
		lib::RawCallable<void(SceneRenderSystem&, RenderSceneConstants&)>                                                                                                                                      updateGPUSceneDataFunc;
		lib::RawCallable<void(SceneRenderSystem&, const SceneRendererInterface&, const RenderScene&, const RenderView&, INOUT RenderViewsCollector& )>                                                         collectRenderViewsFunc;
		lib::RawCallable<void(SceneRenderSystem&, rg::RenderGraphBuilder&, const SceneRendererInterface&, const RenderScene&, const lib::DynamicPushArray<ViewRenderingSpec*>&, const SceneRendererSettings&)> renderPerFrameFunc;
		lib::RawCallable<void(SceneRenderSystem&, rg::RenderGraphBuilder&, const SceneRendererInterface&, const RenderScene&)>                                                                                 finishRenderingFrameFunc;
	};

	const SystemEntry& GetSystemEntry(ESceneRenderSystem systemType) const
	{
		const Uint32 systemIdx = GetSceneRenderSystemIdx(systemType);
		SPT_CHECK(systemIdx < m_systemEntries.size());

		return m_systemEntries[systemIdx];
	}

	lib::StaticArray<SystemEntry, static_cast<Uint32>(ESceneRenderSystem::NUM)> m_systemEntries;
};


template<typename TSystemType>
struct SceneRenderSystemAPIRegistration
{
	SceneRenderSystemAPIRegistration()
	{
		SceneRenderSystemsAPI::Get().RegisterSystem<TSystemType>();
	}
};

#define SPT_REGISTER_SCENE_RENDER_SYSTEM(TSystemType) \
	SCENE_RENDERER_API spt::rsc::SceneRenderSystemAPIRegistration<TSystemType> reg##TSystemType##Instance;

} // spt::rsc
