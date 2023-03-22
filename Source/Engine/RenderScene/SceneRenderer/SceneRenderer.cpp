#include "SceneRenderer.h"
#include "RenderScene.h"
#include "RenderSystem.h"
#include "View/RenderView.h"
#include "Transfers/UploadUtils.h"
#include "RenderStages/ForwardOpaqueRenderStage.h"
#include "RenderStages/DepthPrepassRenderStage.h"
#include "RenderStages/HDRResolveRenderStage.h"
#include "RenderStages/ShadowMapRenderStage.h"
#include "RenderStages/DirectionalLightShadowMasksRenderStage.h"
#include "RenderStages/MotionAndDepthRenderStage.h"
#include "RenderStages/AntiAliasingRenderStage.h"
#include "RenderGraphBuilder.h"
#include "SceneRendererTypes.h"
#include "Parameters/SceneRendererParams.h"

namespace spt::rsc
{

//////////////////////////////////////////////////////////////////////////////////////////////////
// Utils =========================================================================================

namespace renderer_utils
{

template<typename TRenderStage>
void ProcessRenderStage(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, lib::DynamicArray<ViewRenderingSpec*>& renderViewsSpecs)
{
	SPT_PROFILER_FUNCTION();

	TRenderStage stageInstance;

	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		if (viewSpec->SupportsStage(TRenderStage::GetStageEnum()))
		{
			stageInstance.Render(graphBuilder, scene, *viewSpec);
		}
	}
}

} // renderer_utils

//////////////////////////////////////////////////////////////////////////////////////////////////
// SceneRenderer =================================================================================

SceneRenderer::SceneRenderer()
{ }

rg::RGTextureViewHandle SceneRenderer::Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view)
{
	SPT_PROFILER_FUNCTION();

	scene.Update();
	
	const rg::BindDescriptorSetsScope rendererDSScope(graphBuilder, rg::BindDescriptorSets(scene.GetRenderSceneDS()));

	SizeType mainViewIdx = idxNone<SizeType>;
	lib::DynamicArray<ViewRenderingSpec*> renderViewsSpecs = CollectRenderViews(graphBuilder, scene, view, OUT mainViewIdx);

	// Update all relevant views data
	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		viewSpec->GetRenderView().OnBeginRendering();
	}

	const lib::DynamicArray<lib::SharedRef<RenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::SharedRef<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->RenderPerFrame(graphBuilder, scene);
	}
	
	for (const lib::SharedRef<RenderSystem>& renderSystem : renderSystems)
	{
		for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
		{
			if (lib::HasAnyFlag(viewSpec->GetSupportedStages(), renderSystem->GetSupportedStages()))
			{
				renderSystem->RenderPerView(graphBuilder, scene, *viewSpec);
			}
		}
	}

	// Flush all writes that happened during prepare phrase
	gfx::FlushPendingUploads();

	renderer_utils::ProcessRenderStage<ShadowMapRenderStage>(graphBuilder, scene, renderViewsSpecs);
	
	renderer_utils::ProcessRenderStage<DepthPrepassRenderStage>(graphBuilder, scene, renderViewsSpecs);
	
	renderer_utils::ProcessRenderStage<MotionAndDepthRenderStage>(graphBuilder, scene, renderViewsSpecs);
	
	renderer_utils::ProcessRenderStage<DirectionalLightShadowMasksRenderStage>(graphBuilder, scene, renderViewsSpecs);
	
	renderer_utils::ProcessRenderStage<ForwardOpaqueRenderStage>(graphBuilder, scene, renderViewsSpecs);

	renderer_utils::ProcessRenderStage<AntiAliasingRenderStage>(graphBuilder, scene, renderViewsSpecs);

	renderer_utils::ProcessRenderStage<HDRResolveRenderStage>(graphBuilder, scene, renderViewsSpecs);

	for (const lib::SharedPtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->FinishRenderingFrame(graphBuilder, scene);
	}

	ViewRenderingSpec& mainViewSpec = *renderViewsSpecs[mainViewIdx];

	const HDRResolvePassData& mainViewRenderingResult = mainViewSpec.GetData().Get<HDRResolvePassData>();

	rg::RGTextureViewHandle output = mainViewRenderingResult.tonemappedTexture;

#if RENDERER_DEBUG
	if (view.IsAnyDebugFeatureEnabled())
	{
		output = mainViewRenderingResult.debug;
	}
#endif // RENDERER_DEBUG

	return output;
}

lib::DynamicArray<ViewRenderingSpec*> SceneRenderer::CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, SizeType& OUT mainViewIdx) const
{
	SPT_PROFILER_FUNCTION();

	mainViewIdx = idxNone<SizeType>;

	lib::DynamicArray<RenderView*> views;
	views.reserve(4);
	views.emplace_back(&view);

	const lib::DynamicArray<lib::SharedRef<RenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::SharedPtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->CollectRenderViews(scene, view, INOUT views);
	}

	// Remove duplicated views
	std::sort(std::begin(views), std::end(views));
	views.erase(std::unique(std::begin(views), std::end(views)), std::end(views));

	lib::DynamicArray<ViewRenderingSpec*> viewsEntryPoints(views.size(), nullptr);
	for (SizeType viewIdx = 0; viewIdx < views.size(); ++viewIdx)
	{
		viewsEntryPoints[viewIdx] = graphBuilder.Allocate<ViewRenderingSpec>();
		viewsEntryPoints[viewIdx]->Initialize(*views[viewIdx]);

		if (views[viewIdx] == &view)
		{
			mainViewIdx = viewIdx;
		}
	}

	SPT_CHECK(mainViewIdx != idxNone<SizeType>);

	return viewsEntryPoints;
}

} // spt::rsc
