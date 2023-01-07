#include "SceneRenderer.h"
#include "RenderScene.h"
#include "RenderSystem.h"

namespace spt::rsc
{

SceneRenderer::SceneRenderer()
{ }

void SceneRenderer::Render(rg::RenderGraphBuilder& graphBuilder, const RenderScene& scene, RenderView& view)
{
	SPT_PROFILER_FUNCTION();

	lib::DynamicArray<RenderViewEntryPoints> renderViews = CollectRenderViews(scene, view);

	const lib::DynamicArray<lib::UniquePtr<RenderSystem>>& renderSystems = scene.GetRenderSystems();
	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->RenderPerFrame(graphBuilder, scene);
	}
	
	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
	{
		for (RenderViewEntryPoints& renderView : renderViews)
		{
			if (lib::HasAnyFlag(renderView.GetSupportedStages(), renderSystem->GetSupportedStages()))
			{
				renderSystem->RenderPerView(graphBuilder, scene, renderView);
			}
		}
	}

	// TODO: Process render stages here
	SPT_CHECK_NO_ENTRY();

	for (const lib::UniquePtr<RenderSystem>& renderSystem : renderSystems)
	{
		renderSystem->FinishRenderingFrame(graphBuilder, scene);
	}
}

lib::DynamicArray<RenderViewEntryPoints> SceneRenderer::CollectRenderViews(const RenderScene& scene, RenderView& view) const
{
	SPT_PROFILER_FUNCTION();

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

	lib::DynamicArray<RenderViewEntryPoints> viewsEntryPoints(views.size());
	for (SizeType viewIdx = 0; viewIdx < views.size(); ++viewIdx)
	{
		viewsEntryPoints[viewIdx].Initialize(*views[viewIdx]);
	}

	return viewsEntryPoints;
}

} // spt::rsc
