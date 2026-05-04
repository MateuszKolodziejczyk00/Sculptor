#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"


namespace spt::rdr
{
class BottomLevelAS;
} // spt::rdr


namespace spt::rsc
{

struct RayTracingGeometryDefinition
{
	lib::SharedPtr<rdr::BottomLevelAS> blas;

	/** 
	 * Identifier which allows get geometry definition. Type of this definition may vary depending on geometry type
	 * For example for static meshes we can use submeshes and store submesh index in this field
	 */
	Uint32 geometryDataID;

	Uint32 indicesDataUGBOffset   = 0u;
	Uint32 locationsDataUGBOffset = 0u;
	Uint32 uvsDataUGBOffset       = 0u;
	Uint32 normalsDataUGBOffset   = 0u;

	math::Vector2f uvsMin   = math::Vector2f::Zero();
	math::Vector2f uvsRange = math::Vector2f::Zero();
};


class RayTracingGeometryProvider
{
public:

	virtual ~RayTracingGeometryProvider() = default;

	virtual lib::Span<const RayTracingGeometryDefinition> GetRayTracingGeometries() const = 0;
};

} // spt::rsc
