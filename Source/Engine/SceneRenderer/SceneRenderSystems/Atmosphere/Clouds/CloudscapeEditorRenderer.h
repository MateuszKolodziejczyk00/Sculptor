#pragma once

#include "SculptorCoreTypes.h"
#include "Utils/SceneRenderingTypes.h"
#include "Utils/ViewRenderingSpec.h"
#include "SceneRendererTypes.h"


namespace spt::rg
{
class RenderGraphBuilder;
} // spt::rg


namespace spt::rsc::editor
{

struct CloudscapePaintingData
{
	rg::RGTextureViewHandle weatherMapNoise;
};


void RenderCloudscapeInfluenceGizmo(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const CloudscapePaintingData& paintingData, const RenderViewEntryDelegates::DebugRenderAndEditorData& params, const EditorRendering& edParams);

void ExecuteWeatherMapPaintCommand(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const CloudscapePaintingData& paintingData, const EditorRendering& edParams);

} // spt::rsc::editor
