#pragma once

#include "CloudscapeEditorMacros.h"
#include "SceneRenderer/SceneRendererTypes.h"


namespace spt::ed
{

struct CloudscapeEditorFrameState
{
	std::optional<rsc::editor::CloudscapeInfluenceGizmo> activeCloudscapeGizmo;

	std::optional<rsc::editor::WeatherMapPaintCommand> weatherMapPaintCommand;

	lib::SharedPtr<rdr::TextureView> paintedWeatherMap;
};

} // namespace spt::ed
