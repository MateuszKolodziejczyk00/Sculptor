#pragma once

#include "Material.h"
#include "RenderSceneRegistry.h"
#include "ShaderStructs.h"


namespace spt::rsc
{

struct MaterialSlotsComponent
{
	lib::DynamicArray<ecs::EntityHandle> slots;
};
SPT_REGISTER_COMPONENT_TYPE(MaterialSlotsComponent, RenderSceneRegistry);


BEGIN_SHADER_STRUCT(MaterialPBRData)
	SHADER_STRUCT_FIELD(Uint32,         baseColorTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         metallicRoughnessTextureIdx)
	SHADER_STRUCT_FIELD(Uint32,         normalsTextureIdx)
	SHADER_STRUCT_FIELD(Real32,         metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, baseColorFactor)
	SHADER_STRUCT_FIELD(Real32,         roughnessFactor)
	SHADER_STRUCT_FIELD(math::Vector3f, emissiveFactor)
	SHADER_STRUCT_FIELD(Uint32,         emissiveTextureIdx)
END_SHADER_STRUCT();

} // spt::rsc
