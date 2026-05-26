#pragma once

#include "SculptorCoreTypes.h"


namespace spt::rsc::editor
{

struct TerrainInfluenceGizmo
{
	Real32          radius = 0.f;
	math::Vector3f  color   = math::Vector3f::Ones();
	Real32          opacity = 1.f;
};


struct TerrainMaterialMapPaintCommand
{
	Uint32 materialIDToPaint = 0u;
};

} // spt::rsc::editor
