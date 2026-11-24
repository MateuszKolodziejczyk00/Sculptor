#pragma once

#include "ShaderStructs/ShaderStructs.h"
#include "Lights/LightTypes.h"
#include "ViewRenderSystems/ParticipatingMedia/ParticipatingMediaTypes.h"


namespace spt::rsc
{

namespace lights_grid_constants
{
static constexpr Uint32 cascadesNum     = 5u;
static constexpr Uint32 cascadeResXY    = 16u;
static constexpr Uint32 cascadeResZ     = 8u;
static constexpr Uint32 cascadeCellsNum = cascadeResXY * cascadeResXY * cascadeResZ;
static constexpr Real32 baseCellSize    = 2.f;
// 0xFFFF is reserved as list terminator
static constexpr Uint32 nodesCapacity   = 0xFFFFu;
} // lights_grid_constants


BEGIN_SHADER_STRUCT(LightsGridCascade)
	SHADER_STRUCT_FIELD(math::Vector3f, origin)
	SHADER_STRUCT_FIELD(Real32,         cellSize)
END_SHADER_STRUCT();


using LightsGridCascades = lib::StaticArray<LightsGridCascade, lights_grid_constants::cascadesNum>;


BEGIN_SHADER_STRUCT(LightsGridData)
	SHADER_STRUCT_FIELD(LightsGridCascades,          cascades)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint16>, cells)
	SHADER_STRUCT_FIELD(gfx::TypedBufferRef<Uint16>, nodes)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(SceneLightsData)
	SHADER_STRUCT_FIELD(HeightFogParams,                                   heightFog)
	SHADER_STRUCT_FIELD(Uint32,                                            localLightsNum)
	SHADER_STRUCT_FIELD(Uint32,                                            directionalLightsNum)
	SHADER_STRUCT_FIELD(Bool,                                              hasValidCloudsTransmittanceMap)
	SHADER_STRUCT_FIELD(math::Matrix4f,                                    cloudsTransmittanceViewProj)
	SHADER_STRUCT_FIELD(gfx::ConstTypedBufferRef<LocalLightGPUData>,       localLights)
	SHADER_STRUCT_FIELD(gfx::ConstTypedBufferRef<DirectionalLightGPUData>, directionalLights)
	SHADER_STRUCT_FIELD(gfx::ConstSRVTexture2D<Real32>,                    cloudsTransmittanceMap) // only for dir light 0
	SHADER_STRUCT_FIELD(LightsGridData,                                    lightsGrid)
END_SHADER_STRUCT();

} // spt::rsc
