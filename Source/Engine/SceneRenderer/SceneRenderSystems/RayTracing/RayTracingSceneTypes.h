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
	SHADER_STRUCT_FIELD(math::Vector2f,          uvsMin)
	SHADER_STRUCT_FIELD(math::Vector2f,          uvsRange)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(RTSceneData)
	SHADER_STRUCT_FIELD(gfx::TLASRef,                        tlas)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<RTInstanceData>, rtInstances)
END_SHADER_STRUCT();

} // spt::rsc
