#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc::editor
{

struct CloudscapeInfluenceGizmo
{
	Real32          radius = 0.f;
	math::Vector3f  color   = math::Vector3f::Ones();
	Real32          opacity = 1.f;
};


struct WeatherMapPaintCommand
{
	Uint32 channelToPaint = 0u;
	Real32 value          = 0.f;
};

} // spt::rsc::editor
