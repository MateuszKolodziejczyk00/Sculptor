#include "SculptorShader.hlsli"

[[descriptor_set(RenderViewDS, 0)]]
[[descriptor_set(CompositeLightingDS, 1)]]

#if ATMOSPHERE_ENABLED
[[descriptor_set(CompositeAtmosphereDS, 2)]]
#endif // ATMOSPHERE_ENABLED

#if VOLUMETRIC_FOG_ENABLED
[[descriptor_set(CompositeFogDS, 3)]]
#endif // VOLUMETRIC_FOG_ENABLED

#if RT_REFLECTIONS_ENABLED
[[descriptor_set(CompositeRTReflectionsDS, 4)]]
#endif // RT_REFLECTIONS_ENABLED


#include "Atmosphere/Atmosphere.hlsli"
#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Sampling.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "Utils/GBuffer/GBuffer.hlsli"
#include "Utils/MortonCode.hlsli"
#include "Shading/Shading.hlsli"

struct CS_INPUT
{
	uint3 localID  : SV_GroupThreadID;
	uint3 groupID  : SV_GroupID;
};


#if ATMOSPHERE_ENABLED
float3 ComputeAtmosphereLuminance(in float2 uv)
{
	const float3 rayDirection = ComputeViewRayDirectionWS(u_sceneView, uv);

	const float3 viewLocation = GetLocationInAtmosphere(u_atmosphereParams, u_sceneView.viewLocation);

	const float3 skyLuminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, viewLocation, rayDirection);

	float3 sunLuminance = float3(0.0f, 0.0f, 0.0f);

	for (int lightIdx = 0; lightIdx < u_atmosphereParams.directionalLightsNum; ++lightIdx)
	{
		const DirectionalLightGPUData directionalLight = u_directionalLights[lightIdx];
		const float3 lightDirection = -directionalLight.direction;	

		const float rayLightDot    = dot(rayDirection, lightDirection);
		const float minRayLightDot = cos(directionalLight.sunDiskMinCosAngle);

		if (rayLightDot > minRayLightDot)
		{
			const float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, viewLocation, rayDirection);
			const float cosHalfApex = 0.01f;
			const float softEdge = saturate(2.0f * (rayLightDot - cosHalfApex) / (1.0f - cosHalfApex));

			sunLuminance += directionalLight.color * softEdge * transmittance * ComputeLuminanceFromEC(directionalLight.sunDiskEC);
		}
	}

	return skyLuminance + sunLuminance;
}
#endif // ATMOSPHERE_ENABLED


struct IntegratedVolumetricFog
{
	float3 inScattering;
	float transmittance;
};


#if VOLUMETRIC_FOG_ENABLED
IntegratedVolumetricFog SampleInegratedVolumetricFog(in float2 uv, in float depth)
{
	float3 fogFroxelUVW = 0.f;
	const float linearDepth = depth > 0.f ? ComputeLinearDepth(depth, u_sceneView) : 100000.f;
	fogFroxelUVW = ComputeFogFroxelUVW(uv, linearDepth, u_fogParams.fogNearPlane, u_fogParams.fogFarPlane);

	// We use froxel that is closer to avoid light leaking
	const float froxelDepth = 1.f / u_fogParams.fogResolution.z;
	const float zBias = 2 * froxelDepth;
	fogFroxelUVW.z -= zBias;

	const float4 inScatteringTransmittance = SampleTricubic(u_integratedInScatteringTexture, u_linearSampler, fogFroxelUVW, u_fogParams.fogResolution);

	IntegratedVolumetricFog result;
	result.inScattering  = inScatteringTransmittance.rgb;
	result.transmittance = inScatteringTransmittance.a;

	return result;
}
#endif // VOLUMETRIC_FOG_ENABLED


#if RT_REFLECTIONS_ENABLED
float3 ComputeRTReflectionsLuminance(in uint2 pixel, in float2 uv)
{
	const float depth = u_depthTexture.Load(uint3(pixel, 0)).x;
	if(depth == 0.f)
	{
		return 0.f;
	}

	float3 specularReflections = u_reflectionsTexture.Load(uint3(pixel, 0)).rgb;

	if(any(isnan(specularReflections)))
	{
		specularReflections = 0.f;
	}

	const float4 baseColorMetallic = u_baseColorMetallicTexture.Load(uint3(pixel, 0));

	float3 diffuseColor;
	float3 specularColor;
	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

	const float3 ndc            = float3(uv * 2.f - 1.f, depth);
	const float3 sampleLocation = NDCToWorldSpace(ndc, u_sceneView);
	const float3 sampleNormal   = DecodeGBufferNormal(u_tangentFrameTexture.Load(uint3(pixel, 0)));

	const float3 toView = normalize(u_sceneView.viewLocation - sampleLocation);

	const float NdotV     = saturate(dot(sampleNormal, toView));
	const float roughness = u_roughnessTexture.Load(uint3(pixel, 0)).x;

	const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_linearSampler, float2(NdotV, roughness), 0);

	// reverse demodulate specular
	specularReflections *= max((specularColor * integratedBRDF.x + integratedBRDF.y), 0.01f);

	return ExposedLuminanceToLuminance(specularReflections);
}


float ComputeDownsampledRTReflectionsInfluence(in float thisThreadInfluence, in uint threadIdx)
{
	const uint quadFirstThreadIdx = threadIdx & ~0x3u;
	float influence = thisThreadInfluence;
	influence = max(influence, WaveReadLaneAt(influence, quadFirstThreadIdx + 0));
	influence = max(influence, WaveReadLaneAt(influence, quadFirstThreadIdx + 1));
	influence = max(influence, WaveReadLaneAt(influence, quadFirstThreadIdx + 2));
	influence = max(influence, WaveReadLaneAt(influence, quadFirstThreadIdx + 3));
	return influence;
}
#endif // RT_REFLECTIONS_ENABLED


[numthreads(32, 1, 1)]
void CompositeLightingCS(CS_INPUT input)
{
	const uint threadIdx = input.localID.x;

	const uint2 localID = DecodeMorton2D(threadIdx);

	uint2 pixel = input.groupID.xy * uint2(8u, 4u) + localID;
	
	uint2 outputRes;
	u_luminanceTexture.GetDimensions(outputRes.x, outputRes.y);

	const bool isHelperLane = any(pixel >= outputRes);
	pixel = min(pixel, outputRes - 1);

	const float2 pixelSize = rcp(float2(outputRes));
	const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

	const float depth = u_depthTexture.Load(int3(pixel, 0)).x;

	float3 luminance = ExposedLuminanceToLuminance(u_luminanceTexture[pixel]);

#if ATMOSPHERE_ENABLED
	if (depth == 0.f)
	{
		luminance = ComputeAtmosphereLuminance(uv);
	}
#endif // ATMOSPHERE_ENABLED

#if VOLUMETRIC_FOG_ENABLED
	const IntegratedVolumetricFog integratedFog = SampleInegratedVolumetricFog(uv, depth);
#else
	IntegratedVolumetricFog integratedFog;
	integratedFog.inScattering  = float3(0.f, 0.f, 0.f);
	integratedFog.transmittance = 1.f;
#endif // VOLUMETRIC_FOG_ENABLED

	float3 rtReflectionsLuminance = 0.f;
#if RT_REFLECTIONS_ENABLED
	rtReflectionsLuminance = ComputeRTReflectionsLuminance(pixel, uv);
	luminance += rtReflectionsLuminance;
#endif // RT_REFLECTIONS_ENABLED

	luminance = luminance * integratedFog.transmittance;
	luminance += ExposedLuminanceToLuminance(integratedFog.inScattering);

#if RT_REFLECTIONS_ENABLED
	const float rtReflectionsInfluence = AverageComponent((rtReflectionsLuminance * integratedFog.transmittance) / luminance);

	if(u_rtReflectionsConstants.halfResInfluence)
	{
		const float downsampledRTReflectionsInfluence = ComputeDownsampledRTReflectionsInfluence(rtReflectionsInfluence, threadIdx);

		if(!isHelperLane && (threadIdx & 0x3) == 0)
		{
			const uint2 outputPixel = pixel / 2;
			u_reflectionsInfluenceTexture[outputPixel] = downsampledRTReflectionsInfluence;
		}
	}
	else
	{
		u_reflectionsInfluenceTexture[pixel] = rtReflectionsInfluence;
	}
#endif // RT_REFLECTIONS_ENABLED

	luminance = LuminanceToExposedLuminance(luminance);

	if(!isHelperLane)
	{
		u_luminanceTexture[pixel] = luminance;
	}
}
