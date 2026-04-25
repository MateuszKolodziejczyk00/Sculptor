#include "SceneRenderer.h"
#include "Containers/InlineArray.h"
#include "Debug/Stats/SceneRendererStatsView.h"
#include "EngineFrame.h"
#include "Parameters/SceneRendererParams.h"
#include "RenderScene.h"
#include "RenderSceneConstants.h"
#include "RenderStages/RenderStage.h"
#include "SceneRenderSystems/SceneRenderSystem.h"
#include "View/RenderView.h"
#include "Utils/TransfersUtils.h"
#include "Utils/ViewRenderingSpec.h"
#include "RenderGraphBuilder.h"


namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRendererData =============================================================================

struct SceneRendererData
{
	explicit SceneRendererData(const SceneRendererDefinition& definition);
	~SceneRendererData();

	RenderScene& scene;

	lib::MemoryArena rendererArena;
	lib::MemoryArena perFrameArena;

	lib::StaticArray<SceneRenderSystem*, MaxSceneRenderSystemsNum> renderSystems;
};


SceneRendererData::SceneRendererData(const SceneRendererDefinition& definition)
	: scene(definition.scene)
	, rendererArena("SceneRendererData_RendererArena", 1024u * 1024u, 1024u * 1024u * 32u)
	, perFrameArena("SceneRendererData_PerFrameArena", 1024u * 1024u, 1024u * 1024u * 32u)
{
	const SceneRenderSystemsAPI& systemsAPI = SceneRenderSystemsAPI::Get();

	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		if (lib::HasAnyFlag(definition.renderSystems, static_cast<ESceneRenderSystem>(1u << systemIdx)))
		{
			const ESceneRenderSystem systemType = static_cast<ESceneRenderSystem>(1u << systemIdx);
			renderSystems[systemIdx] = systemsAPI.CallConstructor(systemType, rendererArena, definition.scene);
			systemsAPI.CallInitialize(systemType, *renderSystems[systemIdx], rendererArena, definition.scene);
		}
		else
		{
			renderSystems[systemIdx] = nullptr;
		}
	}
}

SceneRendererData::~SceneRendererData()
{
	const SceneRenderSystemsAPI& systemsAPI = SceneRenderSystemsAPI::Get();

	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		if (SceneRenderSystem* renderSystem = renderSystems[systemIdx])
		{
			systemsAPI.CallDeinitialize(static_cast<ESceneRenderSystem>(1u << systemIdx), *renderSystem, scene);
			systemsAPI.CallDestructor(static_cast<ESceneRenderSystem>(1u << systemIdx), *renderSystem);
			renderSystem = nullptr;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRendererRuntime ==========================================================================

struct SceneRendererRuntime : public SceneRendererInterface
{
	explicit SceneRendererRuntime(SceneRendererData& inRendererData, const SceneRendererSettings& inSettings)
		: SceneRendererInterface(inSettings)
		, rendererData(inRendererData)
	{ }

	// Begin SceneRendererInterface
	virtual SceneRenderSystem* GetRenderSystem(ESceneRenderSystem type) const override;
	// End SceneRendererInterface

	SceneRendererData& rendererData;
};

SceneRenderSystem* SceneRendererRuntime::GetRenderSystem(ESceneRenderSystem type) const
{
	const Uint32 systemIdx = GetSceneRenderSystemIdx(type);
	SPT_CHECK(systemIdx < rendererData.renderSystems.size());
	return rendererData.renderSystems[systemIdx];
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace renderer_utils
{

using RenderViewSpecsArray = lib::DynamicPushArray<ViewRenderingSpec>;


RenderViewSpecsArray CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, const SceneRendererRuntime& rendererRuntime, RenderScene& scene, RenderView& mainRenderView, ViewRenderingSpec*& OUT mainViewSpec)
{
	SPT_PROFILER_FUNCTION();

	mainViewSpec = nullptr;

	RenderViewsCollector viewsCollector;
	viewsCollector.AddRenderView(mainRenderView);

	const SceneRenderSystemsAPI& systemsAPI = SceneRenderSystemsAPI::Get();
	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		if (SceneRenderSystem* renderSystem = rendererRuntime.rendererData.renderSystems[systemIdx])
		{
			systemsAPI.CallCollectRenderViewsFunc(GetSceneRenderSystemByIdx(systemIdx), *renderSystem, rendererRuntime, scene, mainRenderView, INOUT viewsCollector);
		}
	}

	RenderViewSpecsArray viewRenderingSpecs(rendererRuntime.rendererData.perFrameArena);

	while (viewsCollector.HasAnyViewsToExtract())
	{
		for (RenderView* renderView : viewsCollector.ExtractNewRenderViews())
		{
			ViewRenderingSpec& viewSpec = viewRenderingSpecs.EmplaceBack();
			viewSpec.Initialize(graphBuilder, *reinterpret_cast<RenderViewPrivate*>(renderView));

			viewSpec.CollectRenderViews(scene, INOUT viewsCollector);

			if (renderView == &mainRenderView)
			{
				mainViewSpec = &viewSpec;
			}
		}
	}

	SPT_CHECK(!!mainViewSpec);

	return viewRenderingSpecs;
}


lib::MTHandle<RenderSceneDS> UpdateRenderSystemsPerFrame(SceneRendererData& rendererData, rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, const SceneUpdateContext& updateContext)
{
	SPT_PROFILER_FUNCTION();

	const SceneRenderSystemsAPI& systemsAPI = SceneRenderSystemsAPI::Get();

	struct ActiveRenderSystem
	{
		SceneRenderSystem* system;
		ESceneRenderSystem systemType;
	};
	lib::InlineArray<ActiveRenderSystem, static_cast<Uint32>(ESceneRenderSystem::NUM)> activeRenderSystems;

	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		activeRenderSystems.EmplaceBack(ActiveRenderSystem{
			.system     = rendererData.renderSystems[systemIdx],
			.systemType = GetSceneRenderSystemByIdx(systemIdx)
		});
	}

	js::InlineParallelForEach("Update Scene Render Systems",
							  activeRenderSystems,
							  [&systemsAPI, &updateContext](const ActiveRenderSystem& activeSystem)
							  {
								  systemsAPI.CallUpdateFunc(activeSystem.systemType, *activeSystem.system, updateContext);
							  });

	const engn::FrameContext& frame = scene.GetCurrentFrameRef();

	GPUSceneData sceneData;
	sceneData.deltaTime                 = frame.GetDeltaTime();
	sceneData.time                      = frame.GetTime();
	sceneData.frameIdx                  = static_cast<Uint32>(frame.GetFrameIdx());
	sceneData.renderEntitiesArray       = scene.GetRenderEntitiesBuffer()->GetFullViewRef();

	SceneGeometryData geometryData;
	geometryData.staticMeshGeometryBuffers = StaticMeshUnifiedData::Get().GetGeometryBuffers();
	geometryData.ugb                       = GeometryManager::Get().GetUnifiedGeometryBuffer();

	GPUMaterialsData gpuMaterials;
	gpuMaterials.data = mat::MaterialsUnifiedData::Get().GetMaterialUnifiedData();

	RenderSceneConstants sceneConstants;
	sceneConstants.gpuScene  = sceneData;
	sceneConstants.geometry  = geometryData;
	sceneConstants.materials = gpuMaterials;

	for (const ActiveRenderSystem& activeSystem : activeRenderSystems)
	{
		systemsAPI.CallUpdateGPUSceneDataFunc(activeSystem.systemType, *activeSystem.system, sceneConstants);
	}

	lib::MTHandle<RenderSceneDS> renderSceneDS = graphBuilder.CreateDescriptorSet<RenderSceneDS>(RENDERER_RESOURCE_NAME("RenderSceneDS"));
	renderSceneDS->u_renderSceneConstants = sceneConstants;

	return renderSceneDS;
}


void ProcessRenderStage(rg::RenderGraphBuilder& graphBuilder, SceneRendererRuntime& rendererRuntime, RenderScene& scene, ERenderStage stage, const RenderViewSpecsArray& renderViewsSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	const RenderStagesAPI& stagesAPI = RenderStagesAPI::Get();

	const Uint32 stageIdx = RenderStageToIdx(stage);

	for (ViewRenderingSpec& viewSpec : renderViewsSpecs)
	{
		if (viewSpec.SupportsStage(stage))
		{
			const rg::BindDescriptorSetsScope viewDSScope(graphBuilder, rg::BindDescriptorSets(viewSpec.GetRenderViewDS()));

			const RenderViewPrivate& view = reinterpret_cast<const RenderViewPrivate&>(viewSpec.GetRenderView());
			RenderStageBase* stageInstance = view.renderStages[stageIdx];
			SPT_CHECK(stageInstance != nullptr);

			stagesAPI.CallRenderFunc(stage, *stageInstance, graphBuilder, rendererRuntime, scene, viewSpec, settings);
		}
	}
}

} // renderer_utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// RenderView ====================================================================================

RenderView* CreateRenderView(const RenderViewDefinition& definition)
{
	return new RenderViewPrivate(definition);
}

void DestroyRenderView(RenderView*& view)
{
	if (view)
	{
		RenderViewPrivate* viewPrivate = reinterpret_cast<RenderViewPrivate*>(view);
		delete viewPrivate;

		view = nullptr;
	}
}

void SetShadingViewSettings(RenderView& view, const ShadingRenderViewSettings& settings)
{
	RenderViewPrivate& viewPrivate = reinterpret_cast<RenderViewPrivate&>(view);
	viewPrivate.shadingSettings = settings;
	viewPrivate.settingsDirty = true;
}

const ShadingRenderViewSettings& GetShadingViewSettings(RenderView& view)
{
	RenderViewPrivate& viewPrivate = reinterpret_cast<RenderViewPrivate&>(view);
	return viewPrivate.shadingSettings;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRenderer =================================================================================

SceneRendererHandle CreateSceneRenderer(const SceneRendererDefinition& definition)
{
	SceneRendererData* data = new SceneRendererData(definition);
	return SceneRendererHandle(reinterpret_cast<Uint64>(data));
}

void DestroySceneRenderer(SceneRendererHandle& handle)
{
	SceneRendererData* data = reinterpret_cast<SceneRendererData*>(handle.Get());
	delete data;

	handle = SceneRendererHandle();
}

rg::RGTextureViewHandle ExecuteSceneRendering(SceneRendererHandle renderer, rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "SceneRenderer");

	const SceneRenderSystemsAPI& systemsAPI = SceneRenderSystemsAPI::Get();

	SceneRendererData* rendererData = reinterpret_cast<SceneRendererData*>(renderer.Get());
	rendererData->perFrameArena.Reset();

	SceneRendererRuntime rendererRuntime(*rendererData, settings);

	SceneUpdateContext updateContext{
			.mainRenderView   = view,
			.rendererSettings = settings
		};

	const lib::MTHandle<RenderSceneDS> renderSceneDS = renderer_utils::UpdateRenderSystemsPerFrame(*rendererData, graphBuilder, scene, updateContext);
	const rg::BindDescriptorSetsScope rendererDSScope(graphBuilder, rg::BindDescriptorSets(std::move(renderSceneDS)));

	ViewRenderingSpec* mainViewSpec = nullptr;
	renderer_utils::RenderViewSpecsArray renderViewsSpecs = renderer_utils::CollectRenderViews(graphBuilder, rendererRuntime, scene, view, OUT mainViewSpec);

	for (ViewRenderingSpec& viewSpec : renderViewsSpecs)
	{
		viewSpec.BeginFrame(graphBuilder, scene);
	}

	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		if (SceneRenderSystem* renderSystem = rendererData->renderSystems[systemIdx])
		{
			lib::DynamicPushArray<ViewRenderingSpec*> viewSpecs(rendererData->perFrameArena);
			for (ViewRenderingSpec& viewSpec : renderViewsSpecs)
			{
				if (lib::HasAnyFlag(viewSpec.GetSupportedStages(), renderSystem->GetSupportedStages()))
				{
					viewSpecs.EmplaceBack(&viewSpec);
				}
			}
			systemsAPI.CallRenderPerFrameFunc(GetSceneRenderSystemByIdx(systemIdx), *renderSystem, graphBuilder, rendererRuntime, scene, viewSpecs, settings);
		}
	}

	for (Uint32 stageIdx = 0; stageIdx < static_cast<Uint32>(ERenderStage::NUM); ++stageIdx)
	{
		const ERenderStage stage = static_cast<ERenderStage>(1 << stageIdx);

		renderer_utils::ProcessRenderStage(graphBuilder, rendererRuntime, scene, stage,renderViewsSpecs, settings);
	}

	for (Uint32 systemIdx = 0; systemIdx < static_cast<Uint32>(ESceneRenderSystem::NUM); ++systemIdx)
	{
		if (SceneRenderSystem* renderSystem = rendererData->renderSystems[systemIdx])
		{
			systemsAPI.CallFinishRenderingFrameFunc(GetSceneRenderSystemByIdx(systemIdx), *renderSystem, graphBuilder, rendererRuntime, scene);
		}
	}

	for (ViewRenderingSpec& viewSpec : renderViewsSpecs)
	{
		viewSpec.EndFrame(graphBuilder, scene);
	}

	// Flush all writes that happened during render graph recording
	rdr::FlushPendingUploads();
	
	const ShadingViewContext& mainViewContext = mainViewSpec->GetShadingViewContext();

	return mainViewContext.output;
}

void TempDrawParamtersUI(void* ctx)
{
	RendererParamsRegistry::DrawParametersUI(ctx);
}

void TempDrawRendererStatsUI(void* ctx)
{
	SceneRendererStatsRegistry::GetInstance().DrawUI(ctx);
}

} // spt::rsc

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRendererAPI ==============================================================================

void InitializeModule(const spt::engn::EngineGlobals& engineGlobals, spt::lib::ModuleDescriptor& outModuleDescriptor)
{
	spt::prf::ProfilerCore::GetInstance().InitializeModule(engineGlobals.profiler);
	spt::lib::HashedStringDB::InitializeModule(*engineGlobals.hashedStringDBData);
	spt::engn::Engine::InitializeModule(*engineGlobals.engineInstance);
	spt::rdr::GPUApi::InitializeModule(*engineGlobals.gpuApiData);

	static SceneRendererDLLModuleAPI api;
	api.CreateRenderView       = &spt::rsc::CreateRenderView;
	api.DestroyRenderView      = &spt::rsc::DestroyRenderView;
	api.SetShadingViewSettings = &spt::rsc::SetShadingViewSettings;
	api.GetShadingViewSettings = &spt::rsc::GetShadingViewSettings;
	api.CreateSceneRenderer    = &spt::rsc::CreateSceneRenderer;
	api.DestroySceneRenderer   = &spt::rsc::DestroySceneRenderer;
	api.ExecuteSceneRendering  = &spt::rsc::ExecuteSceneRendering;
	api.DrawParametersUI       = &spt::rsc::TempDrawParamtersUI;
	api.DrawRendererStatsUI    = &spt::rsc::TempDrawRendererStatsUI;

	outModuleDescriptor.api     = &api;
	outModuleDescriptor.apiType = spt::lib::TypeInfo<SceneRendererDLLModuleAPI>();
}
