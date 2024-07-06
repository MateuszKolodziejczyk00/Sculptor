#include "SculptorShader.hlsli"

#include "SpecularReflections/SpecularReflectionsCommon.hlsli"
#include "SpecularReflections/SpecularReflectionsTracingCommon.hlsli"
#include "SpecularReflections/SRReservoir.hlsli"

#include "Lights/Lighting.hlsli"

#include "DDGI/DDGITypes.hlsli"

#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Random.hlsli"


#define USE_BLUE_NOISE_SAMPLES 0

#if USE_BLUE_NOISE_SAMPLES
#include "Utils/BlueNoiseSamples.hlsli"
#endif // USE_BLUE_NOISE_SAMPLES

float3 GenerateReflectedRayDir(in float3 normal, in float roughness, in float3 toView, in uint2 pixel, out float pdf)
{
	float3 h;
	if(roughness < SPECULAR_TRACE_MAX_ROUGHNESS)
	{
		h = normal;
		pdf = 1.f;
		return reflect(-toView, h);
	}
	else
	{
#if USE_BLUE_NOISE_SAMPLES
		const uint sampleIdx = (((pixel.y & 15u) * 16u + (pixel.x & 15u) + u_gpuSceneFrameConstants.frameIdx * 23u)) & 255u;
#endif // USE_BLUE_NOISE_SAMPLES

		uint attempt = 0;
		while(true)
		{
#if USE_BLUE_NOISE_SAMPLES
			const uint currentSampleIdx = (sampleIdx + attempt) & 255u;
			const float2 noise = frac(g_BlueNoiseSamples[currentSampleIdx] + u_traceParams.random);
#else
			RngState rng = RngState::Create(pixel, u_gpuSceneFrameConstants.frameIdx * 3);
			const float2 noise = float2(rng.Next(), rng.Next());
#endif // USE_BLUE_NOISE_SAMPLES

			const float3 h = SampleVNDFIsotropic(noise, toView, RoughnessToAlpha(roughness), normal);

			const float3 reflectedRay = reflect(-toView, h);

			if(dot(reflectedRay, normal) > 0.f || attempt == 16u) 
			{
				pdf = PDFVNDFIsotrpic(toView, reflectedRay, RoughnessToAlpha(roughness), normal);
				return reflectedRay;
			}

			++attempt;
		}
	}
}


struct RayResult
{
	SRReservoir reservoir;
	float hitDistance;
};


RayResult TraceReflectionRay(in float3 surfWorldLocation, in float3 surfNormal, in float surfRoughness, in float3 toView, in uint2 pixel)
{
	float pdf = 1.f;
	const float3 reflectedRayDirection = GenerateReflectedRayDir(surfNormal, surfRoughness, toView, pixel, OUT pdf);
	
	RayResult result;
	result.hitDistance = 0.f;
	result.reservoir = SRReservoir::CreateEmpty();

	float3 luminance = 0.f;
 
	RayDesc rayDesc;
	rayDesc.TMin = 0.004f;
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
	const float maxHitDistance = 400.f;
	const bool isValidHit = !isBackface && payload.distance < maxHitDistance;

	result.hitDistance = isValidHit ? payload.distance : 2000.f;
	const float3 hitLocation = surfWorldLocation + reflectedRayDirection * result.hitDistance;
	const float3 hitNormal = isValidHit ? normalize(payload.normal) : 0.f;

	if (isValidHit)
	{
		if(dot(toView, surfNormal) > 0.f && dot(surfNormal, reflectedRayDirection) > 0.f)
		{
			const float4 baseColorMetallic = UnpackFloat4x8(payload.baseColorMetallic);

			ShadedSurface surface;
			surface.location = hitLocation;
			surface.shadingNormal = hitNormal;
			surface.geometryNormal = hitNormal;
			surface.roughness = payload.roughness;
			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, surface.diffuseColor, surface.specularColor);

			luminance = CalcReflectedLuminance(surface, -reflectedRayDirection, DDGISecondaryBounceSampleContext::Create(surfWorldLocation, toView), 1.f);

			const float3 emissive = payload.emissive;
			luminance += emissive;

			if (any(isnan(luminance)))
			{
				luminance = 0.f;
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

				luminance = inScatteringTransmittance.rgb + luminance * inScatteringTransmittance.a;
			}
		}
		else
		{
			pdf = 0.f;
		}
	}
	else if (!isBackface)
	{
		const float3 locationInAtmoshpere = GetLocationInAtmosphere(u_atmosphereParams, surfWorldLocation);
		luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, locationInAtmoshpere, reflectedRayDirection);
	}

	result.reservoir = SRReservoir::Create(hitLocation, hitNormal, luminance, pdf);

	if(!isValidHit && !isBackface)
	{
		result.reservoir.AddFlag(SR_RESERVOIR_FLAGS_MISS);
	}

	return result;
}


[shader("raygeneration")]
void GenerateSpecularReflectionsRaysRTG()
{
	const uint2 pixel = DispatchRaysIndex().xy;

	const float depth = u_depthTexture.Load(uint3(pixel, 0));

	float hitDistance = 0.f;

	SRReservoir reservoir = SRReservoir::CreateEmpty();

	if (depth > 0.f)
	{
		const float2 uv = (pixel + 0.5f) / float2(DispatchRaysDimensions().xy);
		const float3 ndc = float3(uv * 2.f - 1.f, depth);
		const float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);

		const float3 normal   = u_shadingNormalsTexture.Load(uint3(pixel, 0)).xyz * 2.f - 1.f;
		const float roughness = u_roughnessTexture.Load(uint3(pixel, 0));

		const float3 toView = normalize(u_sceneView.viewLocation - worldLocation);

		const RayResult rayResult = TraceReflectionRay(worldLocation, normal, roughness, toView, pixel);

		hitDistance = rayResult.hitDistance;
		reservoir   = rayResult.reservoir;
	}

	reservoir.luminance = LuminanceToExposedLuminance(reservoir.luminance);

	const uint reservoirIdx = GetScreenReservoirIdx(pixel, u_traceParams.reservoirsResolution);
	u_reservoirsBuffer[reservoirIdx] = PackReservoir(reservoir);
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

