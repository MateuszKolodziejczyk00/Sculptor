#ifndef SCENE_ATMOSPHERE_HLSLI
#define SCENE_ATMOSPHERE_HLSLI

#include "Atmosphere/Atmosphere.hlsli"

[[shader_struct(SceneAtmosphereData)]]

struct SceneAtmosphereInterface : SceneAtmosphereData
{
	float3 ComputeLocationInAtmosphere(in float3 location)
	{
		return GetLocationInAtmosphere(atmosphereParams, location);
	}

	float3 GetSkyLuminance(in float3 locationInAtmosphere, in float3 rayDirection, in SRVTexture2D<float3> skyViewLUT)
	{
		return GetLuminanceFromSkyViewLUT(atmosphereParams, skyViewLUT.GetResource(), BindlessSamplers::LinearClampEdge(), locationInAtmosphere, rayDirection);
	}
};

#ifdef DS_RenderSceneDS

SceneAtmosphereInterface SceneAtmosphere()
{
	return SceneAtmosphereInterface(u_renderSceneConstants.atmosphere);
}

#endif // DS_RenderSceneDS

#endif // SCENE_ATMOSPHERE_HLSLI
