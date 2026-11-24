#pragma once

#include "ShaderStructs/ShaderStructs.h"
#include "Lights/LightTypes.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(LightsRenderingData)
	SHADER_STRUCT_FIELD(Uint32,				localLightsNum)
	SHADER_STRUCT_FIELD(Uint32,				localLights32Num)
	SHADER_STRUCT_FIELD(Real32,				zClusterLength)
	SHADER_STRUCT_FIELD(math::Vector2u,		tilesNum)
	SHADER_STRUCT_FIELD(math::Vector2f,		tileSize)
	SHADER_STRUCT_FIELD(Uint32,				zClustersNum)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(ViewShadingParams)
	SHADER_STRUCT_FIELD(rdr::GPUPtr<LightsRenderingData>,    lightsData)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<math::Vector2u>,    clustersRanges) // min,max light idx for each z cluster
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<LocalLightGPUData>, localLights)
	SHADER_STRUCT_FIELD(gfx::TypedBuffer<Uint32>,            tilesLightsMask)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,           shadowMask)
	SHADER_STRUCT_FIELD(gfx::SRVTexture2D<Real32>,           ambientOcclusionTexture)
END_SHADER_STRUCT();


struct ViewSpecShadingParameters
{
	rdr::GPUPtr<ViewShadingParams> viewShadingParams;
};

} // spt::rsc
