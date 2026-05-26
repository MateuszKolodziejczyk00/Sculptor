#pragma once

#include "SculptorCoreTypes.h"
#include "TerrainTypes.h"
#include "TerrainRenderSystem.h"
#include "TerrainEditorRenderingTypes.h"


namespace spt::rsc::editor
{

void RenderInfluenceGizmo(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const RenderViewEntryDelegates::DebugRenderAndEditorData& params, const EditorRendering& edParams);

void ExecuteMaterialPaintCommand(rg::RenderGraphBuilder& graphBuilder, const SceneRendererInterface& rendererInterface, const RenderScene& renderScene, const ViewRenderingSpec& view, const EditorRendering& edParams);

} // spt::rsc::editor
