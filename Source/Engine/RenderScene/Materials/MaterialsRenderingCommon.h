#pragma once

#include "Material.h"
#include "RenderSceneRegistry.h"
#include "ShaderStructs.h"
#include "Bindless/BindlessTypes.h"


namespace spt::rsc
{

struct MaterialSlotsComponent
{
	lib::DynamicArray<ecs::EntityHandle> slots;
};
SPT_REGISTER_COMPONENT_TYPE(MaterialSlotsComponent, RenderSceneRegistry);


BEGIN_SHADER_STRUCT(MaterialPBRData)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>, baseColorTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, metallicRoughnessTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector2f>, normalsTexture)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>,         alphaTexture)
	SHADER_STRUCT_FIELD(Real32,                                 metallicFactor)
	SHADER_STRUCT_FIELD(math::Vector3f,                         baseColorFactor)
	SHADER_STRUCT_FIELD(Real32,                                 roughnessFactor)
	SHADER_STRUCT_FIELD(math::Vector3f,                         emissiveFactor)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<math::Vector3f>, emissiveTexture)
END_SHADER_STRUCT();

} // spt::rsc
