#include "SceneRenderer.h"
#include "RenderScene.h"
#include "SceneRenderSystem.h"
#include "View/RenderView.h"
#include "Transfers/UploadUtils.h"
#include "RenderStages/PreRenderingRenderStage.h"
#include "RenderStages/GlobalIlluminationRenderStage.h"
#include "RenderStages/ForwardOpaqueRenderStage.h"
#include "RenderStages/DeferredShadingRenderStage.h"
#include "RenderStages/VisibilityBufferRenderStage.h"
#include "RenderStages/SpecularReflectionsRenderStage.h"
#include "RenderStages/DepthPrepassRenderStage.h"
#include "RenderStages/HDRResolveRenderStage.h"
#include "RenderStages/ShadowMapRenderStage.h"
#include "RenderStages/DownsampleGeometryTexturesRenderStage.h"
#include "RenderStages/AmbientOcclusionRenderStage.h"
#include "RenderStages/DirectionalLightShadowMasksRenderStage.h"
#include "RenderStages/MotionAndDepthRenderStage.h"
#include "RenderStages/AntiAliasingRenderStage.h"
#include "RenderStages/CompositeLightingRenderStage.h"
#include "RenderStages/TransparencyRenderStage.h"
#include "RenderStages/PostProcessPreAARenderStage.h"
#include "RenderGraphBuilder.h"
#include "Parameters/SceneRendererParams.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace renderer_utils
{

template<typename TRenderStage>
void ProcessRenderStage(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, lib::DynamicArray<ViewRenderingSpec*>& renderViewsSpecs, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		if (viewSpec->SupportsStage(TRenderStage::GetStageEnum()))
		{
			const rg::BindDescriptorSetsScope viewDSScope(graphBuilder, rg::BindDescriptorSets(viewSpec->GetRenderView().GetRenderViewDS()));

			TRenderStage& stageInstance = viewSpec->GetRenderStageChecked<TRenderStage>();

			stageInstance.Render(graphBuilder, scene, *viewSpec, settings);
		}
	}
}

} // renderer_utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRenderer =================================================================================

SceneRenderer::SceneRenderer()
{ }

rg::RGTextureViewHandle SceneRenderer::Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, const SceneRendererSettings& settings)
{
	SPT_PROFILER_FUNCTION();

	SPT_RG_DIAGNOSTICS_SCOPE(graphBuilder, "SceneRenderer");

	scene.Update(SceneUpdateContext{
			.mainRenderView   = view,
			.rendererSettings = settings
		});
	
	const rg::BindDescriptorSetsScope rendererDSScope(graphBuilder, rg::BindDescriptorSets(scene.GetRenderSceneDS()));

	SizeType mainViewIdx = idxNone<SizeType>;
	lib::DynamicArray<ViewRenderingSpec*> renderViewsSpecs = CollectRenderViews(graphBuilder, scene, view, OUT mainViewIdx);

	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		viewSpec->GetRenderView().BeginFrame(graphBuilder, scene, *viewSpec);
	}

	const lib::DynamicArray<lib::SharedRef<SceneRenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::SharedRef<SceneRenderSystem>& renderSystem : renderSystems)
	{
		lib::DynamicArray<ViewRenderingSpec*> viewSpecs;

		for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
		{
			if (lib::HasAnyFlag(viewSpec->GetSupportedStages(), renderSystem->GetSupportedStages()))
			{
				viewSpecs.emplace_back(viewSpec);
			}
		}
		
		renderSystem->RenderPerFrame(graphBuilder, scene, viewSpecs, settings);
	}

	renderer_utils::ProcessRenderStage<PreRenderingRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<GlobalIlluminationRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<ShadowMapRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<DepthPrepassRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<VisibilityBufferRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<MotionAndDepthRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<DownsampleGeometryTexturesRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<AmbientOcclusionRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<DirectionalLightShadowMasksRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<ForwardOpaqueRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<DeferredShadingRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<SpecularReflectionsRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);
	
	renderer_utils::ProcessRenderStage<CompositeLightingRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<TransparencyRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<PostProcessPreAARenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<AntiAliasingRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	renderer_utils::ProcessRenderStage<HDRResolveRenderStage>(graphBuilder, scene, renderViewsSpecs, settings);

	for (const lib::SharedPtr<SceneRenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->FinishRenderingFrame(graphBuilder, scene);
	}

	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		viewSpec->GetRenderView().EndFrame(scene);
	}

	// Flush all writes that happened during render graph recording
	gfx::FlushPendingUploads();
	
	ViewRenderingSpec& mainViewSpec = *renderViewsSpecs[mainViewIdx];

	const ShadingViewContext& mainViewContext = mainViewSpec.GetShadingViewContext();
	return mainViewContext.output;
}

lib::DynamicArray<ViewRenderingSpec*> SceneRenderer::CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& mainRenderView, SizeType& OUT mainViewIdx) const
{
	SPT_PROFILER_FUNCTION();

	mainViewIdx = idxNone<SizeType>;

	RenderViewsCollector viewsCollector;
	viewsCollector.AddRenderView(mainRenderView);

	const lib::DynamicArray<lib::SharedRef<SceneRenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::SharedPtr<SceneRenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->CollectRenderViews(scene, mainRenderView, INOUT viewsCollector);
	}

	lib::DynamicArray<ViewRenderingSpec*> viewRenderingSpecs;
	viewRenderingSpecs.reserve(viewsCollector.GetViewsNum());

	while (viewsCollector.HasAnyViewsToExtract())
	{
		for (RenderView* renderView : viewsCollector.ExtractNewRenderViews())
		{
			ViewRenderingSpec* viewSpec = graphBuilder.Allocate<ViewRenderingSpec>();
			viewSpec->Initialize(*renderView);
			viewRenderingSpecs.emplace_back(viewSpec);

			renderView->CollectRenderViews(scene, INOUT viewsCollector);

			if (renderView == &mainRenderView)
			{
				mainViewIdx = viewRenderingSpecs.size() - 1;
			}
		}
	}

	SPT_CHECK(mainViewIdx != idxNone<SizeType>);

	return viewRenderingSpecs;
}

} // spt::rsc
