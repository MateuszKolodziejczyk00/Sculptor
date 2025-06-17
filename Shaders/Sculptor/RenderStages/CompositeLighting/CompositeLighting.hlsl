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
#include "Atmosphere/AerialPerspective.hlsli"
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
// Appendix B from "Physically Based Sky, Atmosphere and Cloud Rendering in Frostbite" course notes
float3 ComputeSunDiskColorFactor(float centerToEdge)
{
	// Model from http://www.physics.hmc.edu/faculty/esin/a101/limbdarkening.pdf
	float3 u = float3(1.0, 1.0, 1.0); // some models have u!=1
	float3 a = float3(0.397, 0.503, 0.652); // coefficient for RGB wavelength (680, 550, 440)
	
	//centerToEdge = 1.0 - centerToEdge;
	float mu = sqrt(1.0 - centerToEdge * centerToEdge);
	
	float3 factor = 1.0 - u * (1.0 - pow(mu, a));

	return factor;
}


float3 ComputeAtmosphereLuminance(in uint2 coords, in float2 uv, in float2 pixelSize)
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
			float3 transmittance = GetTransmittanceFromLUT(u_atmosphereParams, u_transmittanceLUT, u_linearSampler, viewLocation, lightDirection);

			const Sphere groundSphere = Sphere::Create(ZERO_VECTOR, u_atmosphereParams.groundRadiusMM);
			if(Ray::Create(viewLocation, rayDirection).IntersectSphere(groundSphere).IsValid())
			{
				transmittance = 0.f;
			}

			const float rayLightSin = sqrt(1.f - Pow2(rayLightDot));
			const float edgeSin     = sqrt(1.f - Pow2(minRayLightDot));
			const float centerToEdge = rayLightSin / edgeSin;

			sunLuminance += ComputeSunDiskColorFactor(centerToEdge) * transmittance * ComputeLuminanceFromEC(directionalLight.sunDiskEC);
		}
	}

	return (skyLuminance + sunLuminance);
}


struct IntegratedAerialPerspective
{
	float3 inScattering;
	float transmittance;
};

#define AP_TRICUBIC 1

IntegratedAerialPerspective SampleAerialPerspective(in float2 uv, in float linearDepth)
{
	const float apDepth = ComputeAPDepth(u_atmosphereParams.aerialPerspectiveParams, linearDepth);
#if AP_TRICUBIC
	const float4 apData = SampleTricubic(u_aerialPerspective, u_linearSampler, float3(uv, apDepth), u_atmosphereParams.aerialPerspectiveParams.resolution);
#else
	const float4 apData = u_aerialPerspective.SampleLevel(u_linearSampler, float3(uv, apDepth), 0.f);
#endif

	IntegratedAerialPerspective ap;
	ap.inScattering = apData.xyz;
	ap.transmittance = apData.w;

	return ap;
}
#endif // ATMOSPHERE_ENABLED


struct IntegratedVolumetricFog
{
	float3 inScattering;
	float transmittance;
};


#if VOLUMETRIC_FOG_ENABLED
IntegratedVolumetricFog SampleInegratedVolumetricFog(in float2 uv, in float linearDepth)
{
	float3 fogFroxelUVW = 0.f;
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

#if VOLUMETRIC_CLOUDS_ENABLED
float3 CompositeVolumetricClouds(in uint2 coords, in float2 uv, in float2 pixelSize, in float3 background, in float backgroundLinearDepth, in float3 backgroundInScattering)
{
	//const float cloudsDepth = u_volumetricCloudsDepth.Load(uint3(coords, 0u));
	//const float cloudsLinearDepth = ComputeLinearDepth(cloudsDepth, u_sceneView);

	const float cloudsLinearDepth = u_volumetricCloudsDepth.Load(uint3(coords, 0u));

	if(cloudsLinearDepth < 0.f || cloudsLinearDepth > backgroundLinearDepth) // Skip clouds if they are behind opaque geometry
	{
		return background + backgroundInScattering;
	}

	float4 volumetricClouds = u_volumetricClouds.Load(uint3(coords, 0u));

	if(volumetricClouds.a == 1.f)
	{
		return background + backgroundInScattering;
	}

	volumetricClouds.rgb = ExposedLuminanceToLuminance(volumetricClouds.rgb);

	const IntegratedVolumetricFog integratedFog = SampleInegratedVolumetricFog(uv, cloudsLinearDepth);
	const float3 fogInScattering = ExposedLuminanceToLuminance(integratedFog.inScattering);
	const float fogTransmittance = integratedFog.transmittance;
	volumetricClouds.rgb = fogInScattering + volumetricClouds.rgb * fogTransmittance;

	IntegratedAerialPerspective ap = SampleAerialPerspective(uv, cloudsLinearDepth);
	const float3 apInScattering = ExposedLuminanceToLuminance(ap.inScattering) * (1.f - volumetricClouds.a) * fogTransmittance; // multiply by coverage because we assume that sky is behind. Sky already has this inside itself
	const float apTransmittance = ap.transmittance;

	return volumetricClouds.rgb + ((background + backgroundInScattering - fogInScattering - apInScattering) * volumetricClouds.a);
}
#endif // VOLUMETRIC_CLOUDS_ENABLED


#if RT_REFLECTIONS_ENABLED
struct RTReflections
{
	float3 specular;
	float3 diffuse;
};

RTReflections ComputeRTReflectionsLuminance(in uint2 pixel, in float2 uv)
{
	const float depth = u_depthTexture.Load(uint3(pixel, 0)).x;
	if(depth == 0.f)
	{
		RTReflections result;
		result.specular = 0.f;
		result.diffuse  = 0.f;
		return result;
	}

	float3 specularLo = u_specularGI.Load(uint3(pixel, 0)).rgb;
	float3 diffuseLo  = u_diffuseGI.Load(uint3(pixel, 0)).rgb;

	if(any(isnan(specularLo)))
	{
		specularLo = 0.f;
	}

	if(any(isnan(diffuseLo)))
	{
		diffuseLo = 0.f;
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
	specularLo *= max((specularColor * integratedBRDF.x + integratedBRDF.y), 0.01f);

	const float ambientOcclusion = u_ambientOcclusion.Load(uint3(pixel, 0u));

	float3 aoMultiplier;

	if(u_constants.enableColoredAO)
	{
		aoMultiplier = lerp(diffuseColor, 1.f, ambientOcclusion);
	}
	else
	{
		aoMultiplier = ambientOcclusion;
	}

	diffuseLo *= Diffuse_Lambert(diffuseColor) * aoMultiplier;

	RTReflections result;
	result.specular = ExposedLuminanceToLuminance(specularLo);
	result.diffuse  = ExposedLuminanceToLuminance(diffuseLo);

	return result;
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
	const float linearDepth = depth > 0.f ? ComputeLinearDepth(depth, u_sceneView) : 100000.f;

	float3 luminance = ExposedLuminanceToLuminance(u_luminanceTexture[pixel]);

#if ATMOSPHERE_ENABLED
	if (depth == 0.f)
	{
		luminance = ComputeAtmosphereLuminance(pixel, uv, pixelSize);
	}
#endif // ATMOSPHERE_ENABLED

#if VOLUMETRIC_FOG_ENABLED
	const IntegratedVolumetricFog integratedFog = SampleInegratedVolumetricFog(uv, linearDepth);
#else
	IntegratedVolumetricFog integratedFog;
	integratedFog.inScattering  = float3(0.f, 0.f, 0.f);
	integratedFog.transmittance = 1.f;
#endif // VOLUMETRIC_FOG_ENABLED

	RTReflections reflections;
#if RT_REFLECTIONS_ENABLED
	reflections = ComputeRTReflectionsLuminance(pixel, uv);
	luminance += reflections.diffuse;
	luminance += reflections.specular;
#endif // RT_REFLECTIONS_ENABLED

#if ATMOSPHERE_ENABLED
	IntegratedAerialPerspective ap = SampleAerialPerspective(uv, linearDepth);
	ap.inScattering = ap.inScattering * integratedFog.transmittance;
#else
	IntegratedAerialPerspective integratedFog;
	integratedFog.inScattering  = float3(0.f, 0.f, 0.f);
	integratedFog.transmittance = 1.f;
#endif

	luminance = luminance * integratedFog.transmittance * ap.transmittance;

#if VOLUMETRIC_CLOUDS_ENABLED
	luminance = CompositeVolumetricClouds(pixel, uv, pixelSize, luminance, linearDepth, ExposedLuminanceToLuminance(integratedFog.inScattering + ap.inScattering));
#else
	luminance += ExposedLuminanceToLuminance(integratedFog.inScattering);
#endif // VOLUMETRIC_CLOUDS_ENABLED

#if RT_REFLECTIONS_ENABLED
	const float specularReflectionsInfluence = AverageComponent((reflections.specular * integratedFog.transmittance) / luminance);
	const float diffuseReflectionsInfluence  = AverageComponent((reflections.diffuse * integratedFog.transmittance) / luminance);

	float2 reflectionsInfluence = 0.f;

	if(u_rtReflectionsConstants.halfResInfluence)
	{
		reflectionsInfluence.x = ComputeDownsampledRTReflectionsInfluence(specularReflectionsInfluence, threadIdx);
		reflectionsInfluence.y = ComputeDownsampledRTReflectionsInfluence(diffuseReflectionsInfluence, threadIdx);

		if(!isHelperLane && (threadIdx & 0x3) == 0)
		{
			const uint2 outputPixel = pixel / 2;
			u_reflectionsInfluenceTexture[outputPixel] = reflectionsInfluence;
		}
	}
	else
	{
		reflectionsInfluence.x = specularReflectionsInfluence;
		reflectionsInfluence.y = diffuseReflectionsInfluence;
		u_reflectionsInfluenceTexture[pixel] = reflectionsInfluence;
	}
#endif // RT_REFLECTIONS_ENABLED

	luminance = LuminanceToExposedLuminance(luminance);

	if(!isHelperLane)
	{
		u_luminanceTexture[pixel] = luminance;
	}
}