#pragma once

#include "SculptorCoreTypes.h"
#include "SceneRendererTypes.h"

namespace spt::rsc
{

namespace grass_consts
{
static constexpr Real32 tileSize = 4.f;
} // grass_consts

BEGIN_SHADER_STRUCT(GrassBladeDef)
	SHADER_STRUCT_FIELD(math::Vector2f, location)
	SHADER_STRUCT_FIELD(Uint16,         flags)
	SHADER_STRUCT_FIELD(Uint16,         clumpCoords)
END_SHADER_STRUCT();


struct GrassBlades
{
	rg::RGBufferViewHandle bladeDefs;
	rg::RGBufferViewHandle bladesNum;
};


namespace grass_utils
{

inline math::Vector2i GetTileCoord(const math::Vector2f& location)
{
	return math::Vector2i(static_cast<Int32>(std::floor(location.x() / grass_consts::tileSize)), static_cast<Int32>(std::floor(location.y() / grass_consts::tileSize)));
}


inline math::AlignedBox2f GetTileBounds(const math::Vector2i& tileCoord)
{
	const math::Vector2f tileOrigin = tileCoord.cast<Real32>() * grass_consts::tileSize;
	return math::AlignedBox2f(tileOrigin, tileOrigin + math::Vector2f::Constant(grass_consts::tileSize));
}

} // grass_utils


BEGIN_SHADER_STRUCT(GrassFieldDefinition)
	SHADER_STRUCT_FIELD(math::Vector2i, originTile)
	SHADER_STRUCT_FIELD(math::Vector2i, tilesExtent)
	SHADER_STRUCT_FIELD(math::Vector2f, worldSpaceMin)
	SHADER_STRUCT_FIELD(math::Vector2f, worldSpaceMax)
END_SHADER_STRUCT()

} // spt::rsc
