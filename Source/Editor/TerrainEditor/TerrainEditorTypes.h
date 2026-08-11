#pragma once

#include "TerrainEditorMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"


namespace spt::ed
{

struct TerrainEditorFrameState
{
	std::optional<rsc::editor::TerrainInfluenceGizmo> activeTerrainGizmo;

	std::optional<rsc::editor::TerrainMaterialMapPaintCommand> materialTerrainPaintCommand;

	lib::SharedPtr<rdr::TextureView> paintedMaterialMap;
};

} // namespace spt::ed
