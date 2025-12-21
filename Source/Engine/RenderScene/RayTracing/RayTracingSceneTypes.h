#pragma once

#include "SculptorCoreTypes.h"
#include "RenderSceneRegistry.h"
#include "Bindless/BindlessTypes.h"
#include "RenderSceneTypes.h"
#include "Material.h"

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
SPT_REGISTER_COMPONENT_TYPE(RayTracingGeometryProviderComponent, RenderSceneRegistry)


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
};


struct RayTracingGeometryComponent
{
	lib::DynamicArray<RayTracingGeometryDefinition> geometries;
};
SPT_REGISTER_COMPONENT_TYPE(RayTracingGeometryComponent, ecs::Registry);


enum class EMaterialRTFlags : Uint16
{
	None        = 0,
	DoubleSided = BIT(0),
};


BEGIN_SHADER_STRUCT(RTInstanceData)
	SHADER_STRUCT_FIELD(RenderEntityGPUPtr,      entity)
	SHADER_STRUCT_FIELD(mat::MaterialDataHandle, materialDataHandle)
	SHADER_STRUCT_FIELD(Uint16,                  metarialRTFlags) // EMaterialRTFlags
	SHADER_STRUCT_FIELD(Uint32,                  indicesDataUGBOffset)
	SHADER_STRUCT_FIELD(Uint32,                  locationsDataUGBOffset)
	SHADER_STRUCT_FIELD(Uint32,                  uvsDataUGBOffset)
	SHADER_STRUCT_FIELD(Uint32,                  normalsDataUGBOffset)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RTSceneData)
	SHADER_STRUCT_FIELD(gfx::TLASRef,                        tlas)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<RTInstanceData>, rtInstances)
END_SHADER_STRUCT();

} // spt::rsc