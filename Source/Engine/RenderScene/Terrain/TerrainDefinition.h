#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Material.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(TerrainMaterialData)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, terrainMaterial)
END_SHADER_STRUCT();


struct TerrainDefinition
{
	lib::SharedPtr<rdr::TextureView> heightMap;
	lib::SharedPtr<rdr::TextureView> farLODBaseColor;
	lib::SharedPtr<rdr::TextureView> farLODProps;
	TerrainMaterialData material;

	//...
};

} // spt::rsc
