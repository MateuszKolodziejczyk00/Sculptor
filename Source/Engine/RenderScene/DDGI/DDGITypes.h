#pragma once

#include "SculptorCoreTypes.h"
#include "ShaderStructs/ShaderStructsMacros.h"


namespace spt::rsc
{

BEGIN_SHADER_STRUCT(DDGIGPUParams)
	SHADER_STRUCT_FIELD(math::Vector3f, probesOriginWorldLocation) // AABB begin
	SHADER_STRUCT_FIELD(math::Vector3f, probesEndWorldLocation) // AABB end
	
	SHADER_STRUCT_FIELD(math::Vector3f, probesSpacing)
	SHADER_STRUCT_FIELD(math::Vector3f, rcpProbesSpacing)
	SHADER_STRUCT_FIELD(Real32,			maxDistBetweenProbes)
	
	SHADER_STRUCT_FIELD(math::Vector3u, probesVolumeResolution)
	SHADER_STRUCT_FIELD(math::Vector3i, probesWrapCoords)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probesIrradianceTextureRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probesHitDistanceTextureRes)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probeIrradianceDataRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probeIrradianceDataWithBorderRes)
	
	SHADER_STRUCT_FIELD(math::Vector2u, probeHitDistanceDataRes)
	SHADER_STRUCT_FIELD(math::Vector2u, probeHitDistanceDataWithBorderRes)
	
	SHADER_STRUCT_FIELD(math::Vector2f, probesIrradianceTexturePixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, probesIrradianceTextureUVDeltaPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, probesIrradianceTextureUVPerProbeNoBorder)
	
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceTexturePixelSize)
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceUVDeltaPerProbe)
	SHADER_STRUCT_FIELD(math::Vector2f, probesHitDistanceTextureUVPerProbeNoBorder)
	SHADER_STRUCT_FIELD(Real32,			probeIrradianceEncodingGamma)
END_SHADER_STRUCT();


BEGIN_SHADER_STRUCT(DDGIUpdateProbesGPUParams)
	SHADER_STRUCT_FIELD(math::Vector3u,		probesToUpdateCoords)
	SHADER_STRUCT_FIELD(Real32,				probeRaysMaxT)
	SHADER_STRUCT_FIELD(math::Vector3u,		probesToUpdateCount)
	SHADER_STRUCT_FIELD(Real32,				probeRaysMinT)
	SHADER_STRUCT_FIELD(Uint32,				raysNumPerProbe)
	SHADER_STRUCT_FIELD(Uint32,				probesNumToUpdate)
	SHADER_STRUCT_FIELD(Real32,				rcpRaysNumPerProbe)
	SHADER_STRUCT_FIELD(Real32,				rcpProbesNumToUpdate)
	SHADER_STRUCT_FIELD(math::Vector3f,		skyIrradiance)
	SHADER_STRUCT_FIELD(Real32,				blendHysteresis)
	SHADER_STRUCT_FIELD(math::Vector3f,		groundIrradiance)
	SHADER_STRUCT_FIELD(math::Matrix4f,		raysRotation)
END_SHADER_STRUCT();


namespace EDDDGIProbesDebugMode
{
enum Type
{
	None,
	Irradiance,
	HitDistance,

	NUM
};
} // EDDDGIProbesDebugMode

} // spt::rsc
