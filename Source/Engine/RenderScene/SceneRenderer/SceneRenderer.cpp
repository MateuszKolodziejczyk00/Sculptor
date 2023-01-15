#include "SceneRenderer.h"
#include "RenderScene.h"
#include "RenderSystem.h"
#include "BufferUtilities.h"
#include "RenderStages/GBufferGenerationStage.h"
#include "RenderGraphBuilder.h"
#include "SceneRendererTypes.h"

namespace spt::rsc
{

namespace renderer_utils
{

template<typename TRenderStage>
void ProcessRenderStage(ERenderStage stage, rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, lib::DynamicArray<ViewRenderingSpec*>& renderViewsSpecs)
{
	SPT_PROFILER_FUNCTION();

	TRenderStage stageInstance;

	for (ViewRenderingSpec* viewSpec : renderViewsSpecs)
	{
		if (viewSpec->SupportsStage(stage))
		{
			stageInstance.Render(graphBuilder, scene, *viewSpec);
		}
	}
}

} // renderer_utils

SceneRenderer::SceneRenderer()
{ }

rg::RGTextureViewHandle SceneRenderer::Render(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view)
{
	SPT_PROFILER_FUNCTION();

	scene.Update();

	SizeType mainViewIdx = idxNone<SizeType>;
	lib::DynamicArray<ViewRenderingSpec*> renderViewsSpecs = CollectRenderViews(graphBuilder, scene, view, OUT mainViewIdx);

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->RenderPerFrame(graphBuilder, scene);
	}
	
	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
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

	renderer_utils::ProcessRenderStage<GBufferGenerationStage>(ERenderStage::GBufferGenerationStage, graphBuilder, scene, renderViewsSpecs);

	ViewRenderingSpec& mainViewSpec = *renderViewsSpecs[mainViewIdx];

	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->FinishRenderingFrame(graphBuilder, scene);
	}

	const GBufferData& mainViewGBuffer = mainViewSpec.GetData().Get<GBufferData>();
	return mainViewGBuffer.testColor;
}

lib::DynamicArray<ViewRenderingSpec*> SceneRenderer::CollectRenderViews(rg::RenderGraphBuilder& graphBuilder, RenderScene& scene, RenderView& view, SizeType& OUT mainViewIdx) const
{
	SPT_PROFILER_FUNCTION();

	mainViewIdx = idxNone<SizeType>;

	lib::DynamicArray<RenderView*> views;
	views.reserve(4);
	views.emplace_back(&view);

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
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
