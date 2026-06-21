#pragma once

#include "RenderSceneMacros.h"
#include "SculptorCoreTypes.h"
#include "Bindless/BindlessTypes.h"
#include "Material.h"


namespace spt::rdr
{
class TextureView;
} // spt::rdr


namespace spt::rsc
{

namespace terrain_material_props
{
static constexpr Uint32 maxMaterialEntries = 64u;
} // terrain_material_props

namespace terrain_consts
{
static constexpr Real32 tileSizeMeters = 32.f;
} // terrain_consts

BEGIN_SHADER_STRUCT(TerrainMaterialEntry)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, dataHandle)
	SHADER_STRUCT_FIELD(Real32,                  uvScale)
	SHADER_STRUCT_FIELD(Uint16,                  grassType)
	SHADER_STRUCT_FIELD(Uint16,                  displacementMatFeatureID)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainMaterialData)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<TerrainMaterialEntry>, matEntries)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(TerrainMaterialsMap)
	SHADER_STRUCT_FIELD(math::Vector2f,            minBounds)
	SHADER_STRUCT_FIELD(math::Vector2f,            rcpBoundsSize)
	SHADER_STRUCT_FIELD(math::Vector2f,            resolution)
	SHADER_STRUCT_FIELD(math::Vector2f,            rcpResolution)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Uint32>, materialIDs)
END_SHADER_STRUCT();


struct TerrainDefinition
{
	lib::SharedPtr<rdr::TextureView> heightMap;
	lib::SharedPtr<rdr::TextureView> tileHeightMinMaxMap;
	lib::SharedPtr<rdr::TextureView> farLODBaseColor;
	lib::SharedPtr<rdr::TextureView> farLODProps;
	math::Vector2f farLODMinBounds;
	math::Vector2f farLODMaxBounds;
	TerrainMaterialsMap materialsMap;
	TerrainMaterialData material;
};

} // spt::rsc
