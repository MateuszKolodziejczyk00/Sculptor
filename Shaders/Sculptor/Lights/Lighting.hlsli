#include "Lights/LightsTiles.hlsli"
#include "Lights/LightingUtils.hlsli"
#include "Lights/Shadows.hlsli"
#include "Atmosphere/Atmosphere.hlsli"
#include "RenderStages/VolumetricFog/VolumetricFog.hlsli"
#include "RayTracing/RTScene.hlsli"
#include "SceneRendering/WSC.hlsli"

#ifdef DS_DDGISceneDS
#include "DDGI/DDGITypes.hlsli"
#endif // DS_DDGISceneDS

#ifdef DS_ViewShadingInputDS

// Based on https://themaister.net/blog/2020/01/10/clustered-shading-evolution-in-granite/
uint ClusterMaskRange(uint mask, uint2 range, uint startIdx)
{
	range.x = clamp(range.x, startIdx, startIdx + 32u);
	range.y = clamp(range.y + 1u, range.x, startIdx + 32u);

	const uint numBits = range.y - range.x;
	const uint rangeMask = numBits == 32 ? 0xffffffffu : ((1u << numBits) - 1u) << (range.x - startIdx);
	return mask & uint(rangeMask);
}

template<typename TLightingAccumulator>
void CalcReflectedLuminance(in ShadedSurface surface, in float3 viewDir, inout TLightingAccumulator accumulator)
{
	// Directional Lights

	for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
	{
		const DirectionalLightGPUData directionalLight = u_directionalLights[i];

		const float3 illuminance = directionalLight.illuminance;

		if (any(illuminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
		{
			float visibility = 1.f;
			if(directionalLight.shadowMaskIdx != IDX_NONE_32)
			{
				visibility = u_shadowMasks[directionalLight.shadowMaskIdx].SampleLevel(u_nearestSampler, surface.uv, 0).x;
			}

			if(visibility > 0.f)
			{
				const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(u_lightsData.heightFog, surface.location, surface.location - directionalLight.direction * 1500.f);

				const LightingContribution shadingRes = CalcLighting(surface, -directionalLight.direction, viewDir, illuminance) * fogTransmittance * visibility;
				accumulator.Accumulate(shadingRes);
			}
		}
	}

	// Point lights
	const uint2 lightsTileCoords = GetLightsTile(surface.uv, u_lightsData.tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
	
	const uint clusterIdx = surface.linearDepth / u_lightsData.zClusterLength;
	const uint2 clusterRange = clusterIdx < u_lightsData.zClustersNum ? u_clustersRanges[clusterIdx] : uint2(0u, 0u);
	
	for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
	{
		uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];
		lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

		while(lightsMask)
		{
			const uint maskBitIdx = firstbitlow(lightsMask);

			const uint lightIdx = i * 32 + maskBitIdx;
			const LocalLightInterface localLight = u_localLights[lightIdx];
		  
			const float3 toLight = localLight.location - surface.location;

			if (dot(toLight, surface.shadingNormal) > 0.f)
			{
				const float distToLight = length(toLight);

				if (distToLight < localLight.range)
				{
					const float3 lightDir = toLight / distToLight;
					const float3 illuminance = GetLightIlluminanceAtLocation(localLight, surface.location);

					if(any(illuminance > 0.f))
					{
						float visibility = 1.f;
						if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
						{
							visibility = EvaluatePointLightShadows(surface, localLight.location, localLight.range, localLight.shadowMapFirstFaceIdx);
						}
				
						if (visibility > 0.f)
						{
							const LightingContribution shadingRes = CalcLighting(surface, lightDir, viewDir, illuminance) * visibility;
							accumulator.Accumulate(shadingRes);
						}
					}
				}
			}

			lightsMask &= ~(1u << maskBitIdx);
		}
	}
}


#if SPT_META_PARAM_DEBUG_FEATURES
void TiledShadingDebug(in uint2 pixelCoords, in ShadedSurface surface)
{
	const uint2 lightsTileCoords = GetLightsTile(surface.uv, u_lightsData.tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
	
	const uint clusterIdx = surface.linearDepth / u_lightsData.zClusterLength;
	const uint2 clusterRange = clusterIdx < u_lightsData.zClustersNum ? u_clustersRanges[clusterIdx] : uint2(0u, 0u);

	uint lightsNum = 0u;

	for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
	{
		uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];
		lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

		while(lightsMask)
		{
			const uint maskBitIdx = firstbitlow(lightsMask);

			const uint lightIdx = i * 32 + maskBitIdx;
			const LocalLightInterface localLight = u_localLights[lightIdx];

			++lightsNum;

			lightsMask &= ~(1u << maskBitIdx);
		}
	}

	if(lightsNum > 0u)
	{
		debug::WriteDebugPixelOnScreen(pixelCoords, float4(lerp(float3(0.f, 1.f, 1.f), float3(1.f, 0.f, 0.f), saturate(lightsNum / 10u)), 0.5f));
	}
}
#endif // SPT_META_PARAM_DEBUG_FEATURES


struct InScatteringParams
{
	float2 uv;
	float linearDepth;
	
	float3 worldLocation;

	float3 toViewNormal;

	float phaseFunctionAnisotrophy;

	float3 inScatteringColor;

	float froxelDepthRange;

	float directionalLightShadowTerm;
};


float3 ComputeDirectionalLightsInScattering(in InScatteringParams params)
{
	float3 inScattering = 0.f;

	for (uint i = 0; i < u_lightsData.directionalLightsNum; ++i)
	{
		const DirectionalLightGPUData directionalLight = u_directionalLights[i];

		const float3 illuminance = directionalLight.illuminance;

		if(params.directionalLightShadowTerm > 0.f)
		{
			inScattering += params.directionalLightShadowTerm * illuminance * PhaseFunction(params.toViewNormal, directionalLight.direction, params.phaseFunctionAnisotrophy);
		}
	}

	return inScattering * params.inScatteringColor;
}


float3 ComputeLocalLightsInScattering(in InScatteringParams params)
{
	float3 inScattering = 0.f;
	
	const uint2 lightsTileCoords = GetLightsTile(params.uv, u_lightsData.tileSize);
	const uint tileLightsDataOffset = GetLightsTileDataOffset(lightsTileCoords, u_lightsData.tilesNum, u_lightsData.localLights32Num);
	
	const uint clusterIdx = params.linearDepth / u_lightsData.zClusterLength;
	const uint2 clusterRange = clusterIdx < u_lightsData.zClustersNum ? u_clustersRanges[clusterIdx] : uint2(0u, 0u);
	
	for(uint i = 0; i < u_lightsData.localLights32Num; ++i)
	{
		uint lightsMask = u_tilesLightsMask[tileLightsDataOffset + i];
		lightsMask = ClusterMaskRange(lightsMask, clusterRange, i << 5u);

		while(lightsMask)
		{
			const uint maskBitIdx = firstbitlow(lightsMask);

			const uint lightIdx = i * 32 + maskBitIdx;
			const LocalLightInterface localLight = u_localLights[lightIdx];
		  
			const float3 toLight = localLight.location - params.worldLocation;

			const float distToLight = length(toLight);

			if(distToLight < localLight.range)
			{
				const float3 lightDir = toLight / distToLight;
				const float3 illuminance = GetLightIlluminanceAtLocation(localLight, params.worldLocation);

				if (any(illuminance > 0.f))
				{
					float visibility = 1.f;
					if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
					{
						visibility = EvaluatePointLightShadowsAtLocation(params.worldLocation, localLight.location, localLight.range, localLight.shadowMapFirstFaceIdx);
					}
			
					if (visibility > 0.f)
					{
						inScattering += illuminance * visibility * PhaseFunction(params.toViewNormal, -lightDir, params.phaseFunctionAnisotrophy);
					}
				}
			}

			lightsMask &= ~(1u << maskBitIdx);
		}
	}

	inScattering *= params.inScatteringColor;
	
	return inScattering;
}

#endif // DS_ViewShadingInputDS

#ifdef DS_GlobalLightsDS

struct ShadowRayPayload
{
	bool isShadowed;
};


float3 CalcReflectedLuminance_Direct(in ShadedSurface surface, in float3 viewDir)
{
	float3 luminance = 0.f;

	// Directional Lights

	for (uint i = 0; i < u_lightsParams.directionalLightsNum; ++i)
	{
		const DirectionalLightGPUData directionalLight = u_directionalLights[i];

		float3 lightIlluminance = directionalLight.illuminance;

		float cloudsTransmittance = 1.f;
		if(i == 0u && u_lightsParams.hasValidCloudsTransmittanceMap)
		{
			const float4 ctmCS = mul(u_lightsParams.cloudsTransmittanceViewProj, float4(surface.location, 1.f));
			if(all(ctmCS.xy <= ctmCS.w) && all(ctmCS.xy >= -ctmCS.w))
			{
				const float2 ctmUV = (ctmCS.xy / ctmCS.w) *	0.5f + 0.5f;
				cloudsTransmittance = u_cloudsTransmittanceMap.SampleLevel(u_cloudsTransmittanceMapSampler, ctmUV, 0.f);
			}
		}

		lightIlluminance *= cloudsTransmittance;

		const float fogTransmittance = EvaluateHeightBasedTransmittanceForSegment(u_lightsParams.heightFog, surface.location, surface.location - directionalLight.direction * 1500.f);
		lightIlluminance *= fogTransmittance;

		if (any(lightIlluminance > 0.f) && dot(-directionalLight.direction, surface.shadingNormal) > 0.f)
		{
			RayDesc rayDesc;
			rayDesc.TMin        = 0.00f;
			rayDesc.TMax        = 50.f;
			rayDesc.Origin      = surface.location + surface.geometryNormal * 0.03f;
			rayDesc.Direction   = -directionalLight.direction;

			const float visibility = WSC().SampleShadows(surface.location, surface.geometryNormal);
			if (visibility > 0.f)
			{
				luminance += CalcLighting(surface, -directionalLight.direction, viewDir, lightIlluminance).sceneLuminance * visibility;
			}
		}
	}

	// Local Lights

	for(uint lightIdx = 0; lightIdx < u_lightsParams.localLightsNum; ++lightIdx)
	{
		const LocalLightInterface localLight = u_localLights[lightIdx];
		
		const float3 toLight = localLight.location - surface.location;

		if (dot(toLight, surface.shadingNormal) > 0.f)
		{
			const float distToLight = length(toLight);

			if (distToLight < localLight.range)
			{
				const float3 lightDir = toLight / distToLight;
				const float3 illuminance = GetLightIlluminanceAtLocation(localLight, surface.location);

				if(any(illuminance > 0.f))
				{
					float visibility = 1.f;
					if (localLight.shadowMapFirstFaceIdx != IDX_NONE_32)
					{
						const float3 biasedLocation  = surface.location + surface.shadingNormal * 0.02f;
						visibility = EvaluatePointLightShadowsAtLocation(surface.location, biasedLocation, localLight.range, localLight.shadowMapFirstFaceIdx);
					}
			
					if (visibility > 0.f)
					{
						luminance += CalcLighting(surface, lightDir, viewDir, illuminance).sceneLuminance * visibility;
					}
				}
			}
		}
	}

	return luminance;
}

#ifdef DS_DDGISceneDS
template<typename TDDGISampleContext>
#endif // DS_DDGISceneDS
float3 CalcReflectedLuminance_Indirect(in ShadedSurface surface, in float3 viewDir
#ifdef DS_DDGISceneDS
, in TDDGISampleContext ddgiSampleContext, in float indirectMultiplier
#endif // DS_DDGISceneDS
)
{
	float3 luminance = 0.f;

#if defined(DS_DDGISceneDS)
	const float3 specularDominantDirection = GetSpecularDominantDirection(surface.geometryNormal, reflect(-viewDir, surface.geometryNormal), surface.roughness);
	const float specularWeight = Luminance(surface.specularColor) / Luminance(surface.specularColor + surface.diffuseColor);
	const float3 sampleDirection = normalize(lerp(surface.shadingNormal, specularDominantDirection, specularWeight));

	DDGISampleParams diffuseSampleParams = CreateDDGISampleParams(surface.location, surface.geometryNormal, viewDir);
	diffuseSampleParams.sampleDirection = surface.shadingNormal;
	diffuseSampleParams.sampleLocationBiasMultiplier = 1.0f;

	const float3 indirectLuminance = DDGISampleLuminance(diffuseSampleParams, ddgiSampleContext);
	const float3 indirectIlluminance = indirectLuminance * 2.f * PI;
	luminance += Diffuse_Lambert(indirectIlluminance) * surface.diffuseColor * indirectMultiplier;

	const float NdotL = dot(surface.shadingNormal, specularDominantDirection);
	const float NdotV = saturate(dot(surface.shadingNormal, viewDir));
	const float2 integratedBRDF = u_brdfIntegrationLUT.SampleLevel(u_brdfIntegrationLUTSampler, float2(NdotV, surface.roughness), 0);
	luminance += indirectLuminance * (surface.specularColor * integratedBRDF.x + integratedBRDF.y) * indirectMultiplier * NdotL;
#endif // defined(DS_DDGISceneDS)

	return luminance;
}

#ifdef DS_DDGISceneDS
template<typename TDDGISampleContext>
#endif // DS_DDGISceneDS
float3 CalcReflectedLuminance(in ShadedSurface surface, in float3 viewDir
#ifdef DS_DDGISceneDS
, in TDDGISampleContext ddgiSampleContext, in float indirectMultiplier
#endif // DS_DDGISceneDS
)
{
	return CalcReflectedLuminance_Direct(surface, viewDir) +
		   CalcReflectedLuminance_Indirect(surface, viewDir
#ifdef DS_DDGISceneDS
			, ddgiSampleContext, indirectMultiplier
#endif // DS_DDGISceneDS
			);
}

#endif // DS_GlobalLightsDS
