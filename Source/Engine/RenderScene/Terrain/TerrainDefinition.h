#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Material.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(TerrainMaterialData)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, terrainMaterial)
END_SHADER_STRUCT();


struct TerrainDefinition
{
	TerrainMaterialData material;

	//...
};

} // spt::rsc
