#pragma once

#include "TerrainEditorMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"


namespace spt::ed
{

struct TerrainEditorFrameState
{
	std::optional<rsc::editor::TerrainInfluenceGizmo> activeGizmo;

	std::optional<rsc::editor::TerrainMaterialMapPaintCommand> materialPaintCommand;

	lib::SharedPtr<rdr::TextureView> paintedMaterialMap;
};

} // namespace spt::ed
