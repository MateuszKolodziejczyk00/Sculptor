#include "SculptorShader.hlsli"

#include "Utils/BlueNoiseSamples.hlsli"

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SpecularReflectionsTracingCommon.hlsli"

#include "Lights/Lighting.hlsli"

#include "DDGI/DDGITypes.hlsli"

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Utils/SceneViewUtils.hlsli"


float3 GenerateReflectedRayDir(in float3 normal, in float roughness, in float3 toView, in uint2 pixel)
{
	float3 h;
	if(roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		h = normal;
		return reflect(-toView, h);
	}
	else
	{
		uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + u_gpuSceneFrameConstants.frameIdx * 23u)) & 255u;

		uint attempt = 0;
		while(true)
		{
			const uint currentSampleIdx = (sampleIdx + attempt) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[currentSampleIdx] + u_traceParams.random);

			const float3 h = SampleVNDFIsotropic(noise, toView, roughness * roughness, normal);

			const float3 reflectedRay = reflect(-toView, h);

			if(dot(reflectedRay, normal) > 0.f || attempt == 16u) 
			{
				return reflectedRay;
			}

			++attempt;
		}
	}
}


struct RayResult
{
	float3 luminance;
	float hitDistance;
};


RayResult TraceReflectionRay(in float3 surfWorldLocation, in float3 surfNormal, in float surfRoughness, in float3 toView, in uint2 pixel)
{
	const float3 reflectedRayDirection = GenerateReflectedRayDir(surfNormal, surfRoughness, toView, pixel);
	
	RayResult result;
	result.luminance = 0.f;
	result.luminance = 0.f;
 
	RayDesc rayDesc;
	rayDesc.TMin = 0.01f;
	rayDesc.TMax = 200.f;
	rayDesc.Origin = surfWorldLocation;
	rayDesc.Direction = reflectedRayDirection;

	SpecularReflectionsRayPayload payload;

	TraceRay(u_sceneTLAS,
			 0,
			 0xFF,
			 0,
			 1,
			 0,
			 rayDesc,
			 payload);


	const bool isBackface = payload.distance < 0.f;
	const float maxHitDistnace = 400.f;
	const bool isValidHit = !isBackface && payload.distance < maxHitDistnace;

	if (isValidHit)
	{
		result.hitDistance = payload.distance;

		const float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

		const float3 hitNormal = normalize(payload.normal);

		ShadedSurface surface;
		surface.location = surfWorldLocation + reflectedRayDirection * payload.distance;
		surface.shadingNormal = hitNormal;
		surface.geometryNormal = hitNormal;
		surface.roughness = payload.roughness;
		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);

		result.luminance = CalcReflectedLuminance(surface, -reflectedRayDirection, surfNormal);

		const float3 emissive = payload.emissive;
		result.luminance += emissive;

		if (any(isnan(result.luminance)))
		{
			result.luminance = 0.f;
		}

		// Try applying volumetric fog using screen space froxels data
		const float3 ndc = WorldSpaceToNDCNoJitter(surface.location, u_sceneView);
		if(all(ndc.xy >= -1.f) && all(ndc.xy < 1.f) && ndc.z > 0.f)
		{
			const float2 uv = (ndc.xy + 1.f) * 0.5f;

			const float hitLinearDepth = ComputeLinearDepth(ndc.z, u_sceneView);
			const float3 fogFroxelUVW  = ComputeFogFroxelUVW(uv, hitLinearDepth, u_traceParams.volumetricFogNear, u_traceParams.volumetricFogFar);
			
			float4 inScatteringTransmittance = u_integratedInScatteringTexture.SampleLevel(u_linearSampler, fogFroxelUVW, 0);

			const float2 edgeScreenAlpha   = 1.f - smoothstep(0.8f, 1.f, abs(ndc.xy));
			const float volumetricFogAlpha = min(edgeScreenAlpha.x, edgeScreenAlpha.y);

			inScatteringTransmittance.rgb *= volumetricFogAlpha;
			inScatteringTransmittance.a    = lerp(1.f, inScatteringTransmittance.a, volumetricFogAlpha);

			const float intensityAmplifier = payload.distance / max(hitLinearDepth, 0.01f);
			inScatteringTransmittance.rgb *= intensityAmplifier;
			inScatteringTransmittance.a    = pow(inScatteringTransmittance.a, intensityAmplifier);

			result.luminance = inScatteringTransmittance.rgb + result.luminance * inScatteringTransmittance.a;
		}
	}
	else if (!isBackface)
	{
		result.hitDistance = 200.f;
		const float3 locationInAtmoshpere = GetLocationInAtmosphere(u_atmosphereParams, surfWorldLocation);
		result.luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, locationInAtmoshpere, reflectedRayDirection);
	}

	return result;
}


[shader("raygeneration")]
void GenerateSpecularReflectionsRaysRTG()
{
	const uint2 pixel = DispatchRaysIndex().xy;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));

	float3 luminance = 0.f;
	float hitDistance = 0.f;

	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal   = u_shadingNormalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

		if(roughness <= GLOSSY_TRACE_MAX_ROUGHNESS)
		{
			const RayResult rayResult = TraceReflectionRay(worldLocation, normal, roughness, toView, pixel);

			luminance   = rayResult.luminance;
			hitDistance = rayResult.hitDistance;
		}
		else
		{
			const float3 reflectedVector = reflect(-toView, normal);
			const float3 specularDominantDir = GetSpecularDominantDirection(normal, reflectedVector, roughness);
			const float3 ddgiQueryWS = worldLocation;
			DDGISampleParams ddgiSampleParams = CreateDDGISampleParams(ddgiQueryWS, normal, toView);
			ddgiSampleParams.sampleDirection = specularDominantDir;
			luminance = DDGISampleLuminance(ddgiSampleParams);
			hitDistance = 0.f;
		}
	}

	luminance = LuminanceToExposedLuminance(luminance);

	u_reflectedLuminanceTexture[pixel] = float4(luminance, hitDistance);
}

[shader("miss")]
void SpecularReflectionsRTM(inout SpecularReflectionsRayPayload payload)
{
	payload.distance = 999999.f;
}


[shader("miss")]
void SpecularReflectionsShadowsRTM(inout ShadowRayPayload payload)
{
	payload.isShadowed = false;
}

