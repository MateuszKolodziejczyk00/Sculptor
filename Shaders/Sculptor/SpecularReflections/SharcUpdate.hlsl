#include "SculptorShader.hlsli"

#define SHARC_UPDATE 1
#define RT_MATERIAL_TRACING

[[descriptor_set(RenderSceneDS)]]
[[descriptor_set(RenderViewDS)]]
[[descriptor_set(SharcUpdateDS)]]
[[descriptor_set(GlobalLightsDS)]]
[[descriptor_set(CloudscapeProbesDS)]]

#include "SpecularReflections/RTGITracing.hlsli"

#include "SpecularReflections/SculptorSharc.hlsli"

#include "Atmosphere/Atmosphere.hlsli"
#include "Atmosphere/VolumetricClouds/Cloudscape.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"

#include "Lights/Lighting.hlsli"

#include "Utils/SceneViewUtils.hlsli"
#include "Utils/Packing.hlsli"


float3 QueryLuminanceInPreviousCache(in float3 rayDirection, in float3 hitLocation, in const RayHitResult hitResult)
{
	SharcDef sharcDef;
	sharcDef.cameraPosition = u_prevFrameSceneView.viewLocation;
	sharcDef.capacity       = 1u << 22;
	sharcDef.hashEntries    = u_hashEntriesPrev;
	sharcDef.voxelData      = u_voxelDataPrev;
	sharcDef.exposure       = u_viewExposure.exposureLastFrame;

	const SharcParameters sharcParams = CreateSharcParameters(sharcDef);

	float3 luminance = 0.f;

	float3 diffuseColor;
	float3 specularColor;
	ComputeSurfaceColor(hitResult.baseColor.rgb, hitResult.metallic, OUT diffuseColor, OUT specularColor);

	const float NdotV = dot(-rayDirection, hitResult.normal);
	const float3 materialDemodulation = ComputeMaterialDemodulation(u_brdfIntegrationLUT, u_brdfIntegrationLUTSampler, diffuseColor, specularColor, NdotV, hitResult.roughness);

	SharcHitData hitData;
	hitData.positionWorld        = hitLocation;
	hitData.normalWorld          = hitResult.normal;
	hitData.materialDemodulation = materialDemodulation;
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
		return QueryLuminanceInPreviousCache(rayDirection, hitLocation, hitResult);
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

	float3 specularColor;
	float3 diffuseColor;

	{
		const bool isLastBounce = false;

		ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);
		diffuseColor = min(diffuseColor, 0.9f);
		specularColor = min(specularColor, 0.9f);

		const float NdotV = saturate(dot(normal, fromDir));
		const float3 materialDemodulation = ComputeMaterialDemodulation(u_brdfIntegrationLUT, u_brdfIntegrationLUTSampler, diffuseColor, specularColor, NdotV, roughness);

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
		hitData.positionWorld        = worldLocation;
		hitData.normalWorld          = normal;
		hitData.materialDemodulation = materialDemodulation;
#if SHARC_SEPARATE_EMISSIVE
		hitData.emissive      = hitRes.emissive;
#endif // SHARC_SEPARATE_EMISSIVE

		if (!SharcUpdateHit(sharcParams, sharcState, hitData, Li, rng.Next()))
		{
			return;
		}
	}

	const uint bouncesNum = 4u;

	uint hitsNum = 0u;
	for(uint it = 0u; it < bouncesNum; ++it)
	{
		hitsNum++;
		const RayDirectionInfo rayInfo = GenerateReflectionRayDir(diffuseColor, specularColor, normal, roughness, fromDir, rng);

		RayHitResult hitRes = RTGITraceRay(worldLocation, rayInfo.direction);

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_BACKFACE)
		{
			luminance = 0.f;
			break;
		}

		hitRes.roughness = max(hitRes.roughness, 0.4f);

		const float nDotL = dot(normal, rayInfo.direction);
		const RTBRDF brdf = RT_EvaluateBRDF(normal, fromDir, rayInfo.direction, roughness, specularColor, diffuseColor);

		const float3 evaluatedBrdf = brdf.diffuse + brdf.specular;

		const float3 segmentThroughput = evaluatedBrdf * nDotL / rayInfo.pdf;

		SharcSetThroughput(sharcState, segmentThroughput);

		if (hitRes.hitType == RTGBUFFER_HIT_TYPE_VALID_HIT)
		{
			const bool isLastBounce = (it == (bouncesNum - 1u));

			const float3 hitLocation = worldLocation + rayInfo.direction * hitRes.hitDistance;

			const float3 Li = ShadeHitRay(hitLocation, rayInfo.direction, hitRes, isLastBounce);

			worldLocation     = worldLocation + rayInfo.direction * hitRes.hitDistance;
			normal            = hitRes.normal;
			roughness         = max(hitRes.roughness, 0.4f);
			baseColorMetallic = float4(hitRes.baseColor, hitRes.metallic);
			fromDir           = -rayInfo.direction;

			ComputeSurfaceColor(baseColorMetallic.rgb, baseColorMetallic.w, OUT diffuseColor, OUT specularColor);
			diffuseColor = min(diffuseColor, 0.9f);
			specularColor = min(specularColor, 0.9f);
			const float NdotV = saturate(dot(normal, fromDir));
			const float3 materialDemodulation = ComputeMaterialDemodulation(u_brdfIntegrationLUT, u_brdfIntegrationLUTSampler, diffuseColor, specularColor, NdotV, roughness);

			const uint  gridLevel = HashGridGetLevel(hitLocation, sharcParams.gridParameters);
			const float voxelSize = HashGridGetVoxelSize(gridLevel, sharcParams.gridParameters);

			if (hitRes.hitDistance > voxelSize)
			{
				SharcHitData hitData;
				hitData.positionWorld        = worldLocation;
				hitData.normalWorld          = normal;
				hitData.materialDemodulation = materialDemodulation;
#if SHARC_SEPARATE_EMISSIVE
				hitData.emissive = hitRes.emissive;
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
