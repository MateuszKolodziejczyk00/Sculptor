#pragma once

#include "SculptorCoreTypes.h"
#include "ECSRegistry.h"
#include "RenderSceneRegistry.h"

namespace spt::rdr
{
class BottomLevelAS;
} // spt::rdr


namespace spt::rsc
{

struct RayTracingGeometryProviderComponent
{
	// Entity that contains RayTracingGeometryComponent
	ecs::EntityHandle entity;
};
SPT_REGISTER_COMPONENT_TYPE(RenderSceneRegistry, RenderSceneRegistry)


struct RayTracingGeometryDefinition
{
	lib::SharedPtr<rdr::BottomLevelAS> blas;

	/** 
	 * Identifier which allows get geometry definition. Type of this definition may vary depending on geometry type
	 * For example for static meshes we can use submeshes and store submesh index in this field
	 */
	Uint32 geometryDataID;
};


struct RayTracingGeometryComponent
{
	lib::DynamicArray<RayTracingGeometryDefinition> geometries;
};
SPT_REGISTER_COMPONENT_TYPE(RayTracingGeometryComponent, ecs::Registry);

} // spt::rsc