#include "SculptorShader.hlsli"

[[shader_params(GPURenderView, VIEW)]]
[[shader_params(CompositeLightingConstants, CONSTS)]]


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
	const float3 rayDirection = ComputeViewRayDirectionWS(VIEW->sceneView, uv);

	const float3 viewLocation = GetLocationInAtmosphere(*CONSTS->atmosphereParams->atmosphereParams, VIEW->sceneView.viewLocation);

	const float3 skyLuminance = GetLuminanceFromSkyViewLUT(*CONSTS->atmosphereParams->atmosphereParams, CONSTS->atmosphereParams->skyViewLUT, BindlessSamplers::LinearClampEdge(), viewLocation, rayDirection);

	float3 sunLuminance = float3(0.0f, 0.0f, 0.0f);

	for (int lightIdx = 0; lightIdx < CONSTS->atmosphereParams->atmosphereParams->directionalLightsNum; ++lightIdx)
	{
		const DirectionalLightGPUData directionalLight = CONSTS->atmosphereParams->directionalLights[lightIdx];
		const float3 lightDirection = -directionalLight.direction;	

		const float rayLightDot    = dot(rayDirection, lightDirection);
		const float minRayLightDot = cos(directionalLight.sunDiskMinCosAngle);

		if (rayLightDot > minRayLightDot)
		{
			float3 transmittance = GetTransmittanceFromLUT(*CONSTS->atmosphereParams->atmosphereParams, CONSTS->atmosphereParams->transmittanceLUT, BindlessSamplers::LinearClampEdge(), viewLocation, lightDirection);

			const Sphere groundSphere = Sphere::Create(ZERO_VECTOR, CONSTS->atmosphereParams->atmosphereParams->groundRadiusMM);
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
	const float apDepth = ComputeAPDepth(CONSTS->atmosphereParams->atmosphereParams->aerialPerspectiveParams, linearDepth);
#if AP_TRICUBIC
	const float4 apData = SampleTricubic(CONSTS->atmosphereParams->aerialPerspective, BindlessSamplers::LinearClampEdge(), float3(uv, apDepth), CONSTS->atmosphereParams->atmosphereParams->aerialPerspectiveParams.resolution);
#else
	const float4 apData = CONSTS->atmosphereParams->aerialPerspective.SampleLevel(BindlessSamplers::LinearClampEdge(), float3(uv, apDepth), 0.f);
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
	fogFroxelUVW = ComputeFogFroxelUVW(uv, linearDepth, CONSTS->fogParams->fogNearPlane, CONSTS->fogParams->fogFarPlane);

	// We use froxel that is closer to avoid light leaking
	const float froxelDepth = 1.f / CONSTS->fogParams->fogResolution.z;
	const float zBias = 2 * froxelDepth;
	fogFroxelUVW.z -= zBias;

	const float4 inScatteringTransmittance = SampleTricubic(CONSTS->fogParams->integratedInScatteringTexture, BindlessSamplers::LinearClampEdge(), fogFroxelUVW, CONSTS->fogParams->fogResolution);

	IntegratedVolumetricFog result;
	result.inScattering  = inScatteringTransmittance.rgb;
	result.transmittance = inScatteringTransmittance.a;

	return result;
}
#endif // VOLUMETRIC_FOG_ENABLED

#if VOLUMETRIC_CLOUDS_ENABLED
float3 CompositeCirrusClouds(in uint2 coords, in float3 background)
{
	float4 cirrusClouds = CONSTS->atmosphereParams->cirrusClouds.Load(uint3(coords, 0u));
	cirrusClouds.rgb = ExposedLuminanceToLuminance(cirrusClouds.rgb);

	if(cirrusClouds.a == 1.f || any(isnan(cirrusClouds))) // NaNs here can be caused by upsampling using samples that were rejected due to occlusion. In practice this shoould be fixed in upsampling code
	{
		return background;
	}

	return cirrusClouds.rgb + background * cirrusClouds.a;
}

float3 CompositeVolumetricClouds(in uint2 coords, in float2 uv, in float2 pixelSize, in float3 background, in float backgroundLinearDepth, in float3 backgroundInScattering)
{
	const float cloudsLinearDepth = CONSTS->atmosphereParams->volumetricCloudsDepth.Load(uint3(coords, 0u));

	if(cloudsLinearDepth < 0.f || cloudsLinearDepth > backgroundLinearDepth) // Skip clouds if they are behind opaque geometry
	{
		return background + backgroundInScattering;
	}

	float4 volumetricClouds = CONSTS->atmosphereParams->volumetricClouds.Load(uint3(coords, 0u));

	if(volumetricClouds.a == 1.f || any(isnan(volumetricClouds))) // NaNs here can be caused by upsampling using samples that were rejected due to occlusion. In practice this shoould be fixed in upsampling code
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


struct RTReflections
{
	float3 specular;
	float3 diffuse;
};


float3 GTAOMultiBounce(in float visibility, float3 albedo)
{
	const float3 a =  2.0404f * albedo - 0.3324f;
	const float3 b = -4.7951f * albedo + 0.6417f;
	const float3 c =  2.7552f * albedo + 0.6903f;

	const float x = visibility;

	return max(x, ((x *	a + b) * x + c) * x);
}


#if RT_REFLECTIONS_ENABLED
RTReflections ComputeRTReflectionsLuminance(in uint2 pixel, in float2 uv)
{
	const float depth = CONSTS->depthTexture.Load(uint3(pixel, 0)).x;
	if(depth == 0.f)
	{
		RTReflections result;
		result.specular = 0.f;
		result.diffuse  = 0.f;
		return result;
	}

	float3 specularLo = CONSTS->rtReflectionsParams->specularGI.Load(uint3(pixel, 0)).rgb;
	float3 diffuseLo  = CONSTS->rtReflectionsParams->diffuseGI.Load(uint3(pixel, 0)).rgb;

	if(any(isnan(specularLo)))
	{
		specularLo = 0.f;
	}

	if(any(isnan(diffuseLo)))
	{
		diffuseLo = 0.f;
	}

	const float4 baseColorMetallic = CONSTS->rtReflectionsParams->baseColorMetallicTexture.Load(uint3(pixel, 0));

	float3 diffuseColor;
	float3 specularColor;
	ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

	const float3 ndc            = float3(uv * 2.f - 1.f, depth);
	const float3 sampleLocation = NDCToWorldSpace(ndc, VIEW->sceneView);
	const float3 sampleNormal   = DecodeGBufferNormal(CONSTS->rtReflectionsParams->tangentFrameTexture.Load(uint3(pixel, 0)));

	const float3 toView = normalize(VIEW->sceneView.viewLocation - sampleLocation);

	const float NdotV     = saturate(dot(sampleNormal, toView));
	const float roughness = CONSTS->rtReflectionsParams->roughnessTexture.Load(uint3(pixel, 0)).x;

	const float2 integratedBRDF = CONSTS->rtReflectionsParams->brdfIntegrationLUT.SampleLevel(BindlessSamplers::LinearClampEdge(), float2(NdotV, roughness), 0);

	// reverse demodulate specular
	specularLo *= max((specularColor * integratedBRDF.x + integratedBRDF.y), 0.01f);

	float3 aoMultiplier = 1.f;

	if (CONSTS->rtReflectionsParams->aoEnabled)
	{
		const float ambientOcclusion = CONSTS->rtReflectionsParams->ambientOcclusion.Load(uint3(pixel, 0u));
		if(CONSTS->enableColoredAO)
		{
			aoMultiplier = lerp(diffuseColor, 1.f, ambientOcclusion);
		}
		else
		{
			aoMultiplier = ambientOcclusion;
		}
	}

	const float detailAO = CONSTS->aoTexture.Load(uint3(pixel, 0u)).x;
	const float3 detailGTAO = GTAOMultiBounce(detailAO, diffuseColor);
	aoMultiplier *= detailGTAO;

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
	
	uint2 outputRes = CONSTS->luminanceTexture.GetResolution();

	const bool isHelperLane = any(pixel >= outputRes);
	pixel = min(pixel, outputRes - 1);

	const float2 pixelSize = rcp(float2(outputRes));
	const float2 uv = (float2(pixel) + 0.5f) * pixelSize;

	const float depth = CONSTS->depthTexture.Load(int3(pixel, 0)).x;
	const float linearDepth = depth > 0.f ? ComputeLinearDepth(depth, VIEW->sceneView) : 100000.f;

	float3 luminance = ExposedLuminanceToLuminance(CONSTS->luminanceTexture[pixel]);

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
	   //
#if VOLUMETRIC_CLOUDS_ENABLED
	if (depth == 0.f)
	{
	luminance = CompositeCirrusClouds(pixel, luminance);
	}
#endif // VOLUMETRIC_CLOUDS_ENABLED

	IntegratedAerialPerspective ap;
	ap.inScattering  = float3(0.f, 0.f, 0.f);
	ap.transmittance = 1.f;
#if ATMOSPHERE_ENABLED
	if (depth != 0.f)
	{
		ap = SampleAerialPerspective(uv, linearDepth);
		ap.inScattering = ap.inScattering * integratedFog.transmittance;
	}
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

	if(CONSTS->rtReflectionsParams->halfResInfluence)
	{
		reflectionsInfluence.x = ComputeDownsampledRTReflectionsInfluence(specularReflectionsInfluence, threadIdx);
		reflectionsInfluence.y = ComputeDownsampledRTReflectionsInfluence(diffuseReflectionsInfluence, threadIdx);

		if(!isHelperLane && (threadIdx & 0x3) == 0)
		{
			const uint2 outputPixel = pixel / 2;
			CONSTS->rtReflectionsParams->reflectionsInfluenceTexture[outputPixel] = reflectionsInfluence;
		}
	}
	else
	{
		reflectionsInfluence.x = specularReflectionsInfluence;
		reflectionsInfluence.y = diffuseReflectionsInfluence;
		CONSTS->rtReflectionsParams->reflectionsInfluenceTexture[pixel] = reflectionsInfluence;
	}
#endif // RT_REFLECTIONS_ENABLED

	luminance = LuminanceToExposedLuminance(luminance);

	if(!isHelperLane)
	{
		CONSTS->luminanceTexture[pixel] = luminance;
	}
}
