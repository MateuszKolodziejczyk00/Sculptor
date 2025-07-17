#include "SculptorShader.hlsli"

#define SPT_LIGHTING_SHADOW_RAY_MISS_SHADER_IDX 1
#define SHARC_UPDATE 1

[[descriptor_set(SharcUpdateDS, 6)]]

[[descriptor_set(GlobalLightsDS, 7)]]

[[descriptor_set(ShadowMapsDS, 8)]]

[[descriptor_set(CloudscapeProbesDS, 9)]]

#include "SpecularReflections/RTGITracingDescriptors.hlsli"
#include "SpecularReflections/RTGITracing.hlsli"

#include "SpecularReflections/SculptorSharc.hlsli"

#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Lights/Lighting.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


float3 QueryLuminanceInPreviousCache(in float3 hitLocation, in float3 hitNormal)
{
	SharcDef sharcDef;
	sharcDef.cameraPosition = u_prevFrameSceneView.viewLocation;
	sharcDef.capacity       = 1u << 22;
	sharcDef.hashEntries    = u_hashEntriesPrev;
	sharcDef.voxelData      = u_voxelDataPrev;
	sharcDef.exposure       = u_viewExposure.exposureLastFrame;

	const SharcParameters sharcParams = CreateSharcParameters(sharcDef);

	float3 luminance = 0.f;

	SharcHitData hitData;
	hitData.positionWorld = hitLocation;
	hitData.normalWorld   = hitNormal;
	const bool success = SharcGetCachedRadiance(sharcParams, hitData, OUT luminance, false);

	return luminance;
}


float3 ShadeMissRay(in float3 rayOrigin, in float3 rayDirection)
{
	const float3 locationInAtmoshpere = GetLocationInAtmosphere(u_atmosphereParams, rayOrigin);
	float3 luminance = GetLuminanceFromSkyViewLUT(u_atmosphereParams, u_skyViewLUT, u_linearSampler, locationInAtmoshpere, rayDirection);

	const CloudscapeSample cloudscapeSample = SampleHighResCloudscape(rayDirection);
	luminance = cloudscapeSample.inScattering + luminance * cloudscapeSample.transmittance;

	const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(u_lightsParams.heightFog, rayOrigin, rayOrigin + rayDirection * 1000.f);
	luminance *= fogTransmittance;

	return luminance;
}


float3 ShadeHitRay(in float3 hitLocation, in float3 rayDirection, in const RayHitResult hitResult, in bool isLastBounce)
{
	if(isLastBounce)
	{
	return QueryLuminanceInPreviousCache(hitLocation, hitResult.normal);
	}

	ShadedSurface surface;
	surface.location       = hitLocation;
	surface.shadingNormal  = hitResult.normal;
	surface.geometryNormal = hitResult.normal;
	surface.roughness      = hitResult.roughness;
	ComputeSurfaceColor(hitResult.baseColor, hitResult.metallic, surface.diffuseColor, surface.specularColor);

	float3 luminance = CalcReflectedLuminance_Direct(surface, -rayDirection);

	luminance += hitResult.emissive;

	return luminance;
}



[shader("raygeneration")]
void SharcUpdateRTG()
{
	const uint2 coords = DispatchRaysIndex().xy * 5u + uint2(u_constants.frameIdx % 5u, (u_constants.frameIdx % 25u) / 5u);

	const float depth = u_depthTexture.Load(uint3(coords, 0u));

	const float2 uv = (coords + 0.5f) * u_constants.invResolution;

	RngState rng = RngState::Create(coords, u_constants.seed);

	const float3 ndc = float3(uv * 2.f - 1.f, depth);

	float3 worldLocation = NDCToWorldSpace(ndc, u_sceneView);
	float3 normal = OctahedronDecodeNormal(u_normalsTexture.Load(uint3(coords, 0u)));
	float  roughness = u_roughnessTexture.Load(uint3(coords, 0u));
	float4 baseColorMetallic = u_baseColorMetallicTexture.Load(uint3(coords, 0u));
	float3 fromDir = normalize(u_sceneView.viewLocation - worldLocation);

	float3 throughput = 1.f;
	float3 luminance = 0.f;

	SharcState sharcState;
	SharcInit(INOUT sharcState);

	SharcDef sharcDef;
	sharcDef.cameraPosition = u_sceneView.viewLocation;
	sharcDef.capacity       = u_constants.sharcCapacity;
	sharcDef.hashEntries    = u_hashEntries;
	sharcDef.voxelData      = u_voxelData;
	sharcDef.voxelDataPrev  = u_voxelDataPrev;
	sharcDef.exposure       = u_viewExposure.exposure;

	SharcParameters sharcParams = CreateSharcParameters(sharcDef);

	{
		const bool isLastBounce = false;

		// Primary ray
		RayHitResult hitRes;
		hitRes.normal = normal;
		hitRes.roughness = roughness;
		hitRes.baseColor = baseColorMetallic.xyz;
		hitRes.metallic = baseColorMetallic.w;
		hitRes.emissive = 0.f;
		hitRes.hitType = RTGBUFFER_HIT_TYPE_VALID_HIT;
		hitRes.hitDistance = distance(u_sceneView.viewLocation, worldLocation);
		const float3 Li = ShadeHitRay(worldLocation, normalize(worldLocation - u_sceneView.viewLocation), hitRes, isLastBounce);

		SharcHitData hitData;
		hitData.positionWorld = worldLocation;
		hitData.normalWorld   = normal;
#if SHARC_SEPARATE_EMISSIVE
		hitData.emissive      = hitRes.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

		if (!SharcUpdateHit(sharcParams, sharcState, hitData, Li, rng.Next()))
		{
			return;
		}
	}

	uint hitsNum = 0u;
	for(uint it = 0u; it < 3u; ++it)
	{

		hitsNum++;
		float3 specularColor;
		float3 diffuseColor;
		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);

		const RayDirectionInfo rayInfo = GenerateReflectionRayDir(diffuseColor, specularColor, normal, roughness, fromDir, rng);

		RayHitResult hitRes = RTGITraceRay(u_sceneTLAS, worldLocation, rayInfo.direction);

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_BACKFACE)
		{
			luminance = 0.f;
			break;
		}

		hitRes.roughness = max(hitRes.roughness, 0.4f);

		const float nDotL = dot(normal, rayInfo.direction);
		float3 brdf;
		if(rayInfo.rayType == SPT_RAY_TYPE_DIFFUSE)
		{
			brdf = Diffuse_Lambert(diffuseColor);
		}
		else
		{
			brdf = SR_GGX_Specular(normal, fromDir, rayInfo.direction, roughness, specularColor);
		}

		const float3 segmentThroughput = brdf * nDotL / rayInfo.pdf;

		SharcSetThroughput(sharcState, segmentThroughput);

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
		{
			const bool isLastBounce = it == 2u;

			const float3 hitLocation = worldLocation + rayInfo.direction * hitRes.hitDistance;

			const float3 Li = ShadeHitRay(hitLocation, rayInfo.direction, hitRes, isLastBounce);

			worldLocation     = worldLocation + rayInfo.direction * hitRes.hitDistance;
			normal            = hitRes.normal;
			roughness         = hitRes.roughness;
			baseColorMetallic = float4(hitRes.baseColor, hitRes.metallic);
			fromDir           = -rayInfo.direction;

			const uint  gridLevel = HashGridGetLevel(hitLocation, sharcParams.gridParameters);
			const float voxelSize = HashGridGetVoxelSize(gridLevel, sharcParams.gridParameters);

			if (hitRes.hitDistance > voxelSize)
			{
				SharcHitData hitData;
				hitData.positionWorld = worldLocation;
				hitData.normalWorld = normal;
#if SHARC_SEPARATE_EMISSIVE
				hitData.emissive      = hitRes.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

				if (!SharcUpdateHit(sharcParams, sharcState, hitData, Li, rng.Next()))
				{
					break;
				}
			}
		}
		else if(hitRes.hitType == RTGBUFFER_HIT_TYPE_NO_HIT)
		{
			const float3 Li = ShadeMissRay(worldLocation, rayInfo.direction);

			SharcUpdateMiss(sharcParams, sharcState, Li);
			break;
		}
		else
		{
			break;
		}
	}
}


[shader("miss")]
void SharcUpdateRTM(inout RTGIRayPayload payload)
{
	payload.distance = 999999.f;
}


[shader("miss")]
void SharcUpdateShadowRaysRTM(inout ShadowRayPayload payload)
{
	payload.isShadowed = false;
}
